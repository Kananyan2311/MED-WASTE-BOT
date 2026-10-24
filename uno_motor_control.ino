#include <Arduino.h>
#include <SoftwareSerial.h>
#include <EEPROM.h>

// --- Pin Definitions ---
// Dedicated Serial Pins to ESP32-C3 (instead of Pin 0 and 1)
#define ESP_RX_PIN 10   // Connect to ESP32-C3 TX (GPIO 1)
#define ESP_TX_PIN 11  // Connect to ESP32-C3 RX (GPIO 0)
SoftwareSerial espSerial(ESP_RX_PIN, ESP_TX_PIN);

// TB6612FNG Motor Driver (STBY is hardwired to 5V)
#define PWMA 5
#define AIN1 3
#define AIN2 2
#define PWMB 6
#define BIN1 4
#define BIN2 7

// Ultrasonic Sensor (Obstacle / Person Detection)
#define TRIG_PIN 8
#define ECHO_PIN 9

// 5-Line Analog Sensor Array
const int lineSensors[5] = {A0, A1, A2, A3, A4};

// --- Calibratable Thresholds & Timing (adjust from web dashboard, saved in EEPROM) ---
int BLACK_ZERO_THRESHOLD = 200;               // All 5 sensors <= 200: All-black start/dump zone
int LINE_DETECT_THRESHOLD = 500;              // Analog reading: < 500 = BLACK line, > 500 = WHITE floor
unsigned long SPIN_Opposite_MS = 1400;        // Time (ms) to spin around to face opposite direction
const unsigned long LEAVE_START_MIN_MS = 800; // Minimum travel time to clear starting all-black box
#define CAL_MAGIC 0xCB

// --- PID line-following (smooth steering) ---
// error is the weighted line position: -2 (far left) .. 0 (centered) .. +2 (far right)
float Kp = 45.0f;   // proportional: how hard to steer for a given offset
float Ki = 0.0f;    // integral: removes steady drift (keep small)
float Kd = 60.0f;   // derivative: damps oscillation / jitter
float pidIntegral = 0.0f;
float lastError = 0.0f;
int wheelL = 0, wheelR = 0;           // actual wheel commands (slew-limited, -255..255)
const int SLEW_STEP = 25;             // max PWM change per control tick = smooth acceleration
const unsigned long PID_LOOP_MS = 20; // control loop period (50 Hz)
unsigned long lastDistanceTime = 0;   // ultrasonic is read slower than the PID loop
int obstacleHits = 0;                 // consecutive close readings (noise filter)

// --- State Variables ---
enum RobotState {
  IDLE,
  MOVING_DISPOSAL,
  WAITING_TILT,
  RETURNING,
  MANUAL_OVERRIDE
};

RobotState currentState = IDLE;
bool isManualMode = false;
bool hasLeftStartZone = false;
bool spinClearedLine = false;

unsigned long lastSerialTime = 0;
unsigned long lastSensorReadTime = 0;
unsigned long lastPersonDetectedTime = 0;
unsigned long stateStartTime = 0;
unsigned long lastManualCmdTime = 0;
const unsigned long MANUAL_DEADMAN_TIMEOUT_MS = 650; // Auto-stop if button released or signal lost

int obstacleThreshold = 30; // cm
int personThreshold = 50; // cm
int baseSpeed = 150;

int currentSpeed = 0;
long currentDistance = 0;
String motorState = "STOPPED";
bool obstacleDetected = false;
bool isBinFull = false; // When true, proximity person detection is disabled
int sensorValues[5] = {0, 0, 0, 0, 0};

// Forward declarations
void processCommand(String cmd);
long getDistance();
bool isAllSensorsBlackZero();
bool isMiddleBlackLineSensed();
void runLineFollowing();
void runLineFollowingBackward();
void moveForward(int speed);
void moveBackward(int speed);
void turnLeft(int speed);
void turnRight(int speed);
void stopMotors();
void sendTelemetry();
void broadcastLine(String msg);
void saveCalibration();
void loadCalibration();
void pidReset();
void driveWheels(int targetL, int targetR);

void setup() {
  pinMode(PWMA, OUTPUT);
  pinMode(AIN1, OUTPUT);
  pinMode(AIN2, OUTPUT);
  pinMode(PWMB, OUTPUT);
  pinMode(BIN1, OUTPUT);
  pinMode(BIN2, OUTPUT);
  stopMotors();

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // Cleanly initialize SoftwareSerial pins to prevent power-on floating glitches
  pinMode(ESP_RX_PIN, INPUT_PULLUP);
  pinMode(ESP_TX_PIN, OUTPUT);
  digitalWrite(ESP_TX_PIN, HIGH);

  delay(350); // Allow 5V rail, motor driver, and ESP/NodeMCU power to stabilize on cold power-up

  Serial.begin(9600);    // USB Serial Monitor (Debug / Local Control)
  espSerial.begin(9600); // Dedicated Serial to ESP32 / NodeMCU
  espSerial.listen();
  while (Serial.available() > 0) Serial.read();
  while (espSerial.available() > 0) espSerial.read();

  loadCalibration();     // Restore calibrated thresholds from EEPROM

  // Seed initial analog line sensor values so low-pass filter starts cleanly
  for (int i = 0; i < 5; i++) {
    sensorValues[i] = analogRead(lineSensors[i]);
  }

  Serial.println(F("\n=============================================="));
  Serial.println(F("       MED_WASTE_BOT - UNO MOTOR CONTROLLER   "));
  Serial.println(F("=============================================="));
  Serial.println(F("[SYSTEM] USB Hardware Serial initialized @ 9600 baud"));
  Serial.println(F("[SYSTEM] Dedicated ESP/NodeMCU SoftwareSerial @ D10(RX) & D11(TX)"));
  Serial.print(F("[CONFIG] Black Zero Threshold: ")); Serial.println(BLACK_ZERO_THRESHOLD);
  Serial.print(F("[CONFIG] Line Detect Threshold: ")); Serial.println(LINE_DETECT_THRESHOLD);
  Serial.print(F("[CONFIG] Spin Opposite Time: ")); Serial.print(SPIN_Opposite_MS); Serial.println(F(" ms"));
  Serial.print(F("[CONFIG] Person Detect Range: ")); Serial.print(personThreshold); Serial.println(F(" cm"));
  Serial.print(F("[CONFIG] PID Gains: Kp=")); Serial.print(Kp); Serial.print(F(" Ki=")); Serial.print(Ki); Serial.print(F(" Kd=")); Serial.println(Kd);
  Serial.println(F("[SYSTEM] Ready in IDLE mode. Waiting for ESP32/User commands.\n"));
}

void saveCalibration() {
  EEPROM.update(0, CAL_MAGIC);
  EEPROM.put(1, BLACK_ZERO_THRESHOLD);
  EEPROM.put(3, LINE_DETECT_THRESHOLD);
  EEPROM.put(5, SPIN_Opposite_MS);
  EEPROM.put(9, personThreshold);
  EEPROM.put(11, Kp);
  EEPROM.put(15, Ki);
  EEPROM.put(19, Kd);
}

void loadCalibration() {
  if (EEPROM.read(0) != CAL_MAGIC) return; // Nothing saved yet - keep defaults
  EEPROM.get(1, BLACK_ZERO_THRESHOLD);
  EEPROM.get(3, LINE_DETECT_THRESHOLD);
  EEPROM.get(5, SPIN_Opposite_MS);
  EEPROM.get(9, personThreshold);
  EEPROM.get(11, Kp);
  EEPROM.get(15, Ki);
  EEPROM.get(19, Kd);
  // Sanity limits in case of corrupted data
  BLACK_ZERO_THRESHOLD = constrain(BLACK_ZERO_THRESHOLD, 0, 1023);
  LINE_DETECT_THRESHOLD = constrain(LINE_DETECT_THRESHOLD, 0, 1023);
  SPIN_Opposite_MS = constrain(SPIN_Opposite_MS, 200UL, 10000UL);
  personThreshold = constrain(personThreshold, 5, 400);
  if (isnan(Kp) || Kp < 0 || Kp > 500) Kp = 45.0f;
  if (isnan(Ki) || Ki < 0 || Ki > 50) Ki = 0.0f;
  if (isnan(Kd) || Kd < 0 || Kd > 500) Kd = 60.0f;
}

void loop() {
  unsigned long currentMillis = millis();

  // 1. Read Commands from Dedicated ESP32/NodeMCU SoftwareSerial (100% Non-Blocking)
  static String espRxBuf = "";
  while (espSerial.available() > 0) {
    char c = (char)espSerial.read();
    if (c == '\r' || c == '\n') {
      if (espRxBuf.length() > 0) {
        String cmd = espRxBuf;
        espRxBuf = "";
        cmd.trim();
        if (cmd.length() > 0) processCommand(cmd);
      }
    } else if ((uint8_t)c >= 32 && (uint8_t)c < 127 && espRxBuf.length() < 120) {
      espRxBuf += c;
    } else if (espRxBuf.length() >= 120) {
      espRxBuf = "";
    }
  }

  // 2. Read Commands from USB Hardware Serial Monitor (100% Non-Blocking)
  static String usbRxBuf = "";
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == '\r' || c == '\n') {
      if (usbRxBuf.length() > 0) {
        String cmd = usbRxBuf;
        usbRxBuf = "";
        cmd.trim();
        if (cmd.length() > 0) processCommand(cmd);
      }
    } else if ((uint8_t)c >= 32 && (uint8_t)c < 127 && usbRxBuf.length() < 120) {
      usbRxBuf += c;
    } else if (usbRxBuf.length() >= 120) {
      usbRxBuf = "";
    }
  }

  // 3. Read Sensors & Execute State Machine at a fixed 50 Hz (every 20ms using millis)
  if (currentMillis - lastSensorReadTime >= PID_LOOP_MS) {
    lastSensorReadTime = currentMillis;

    // Read 5-Line Sensors with a light low-pass filter (removes jitter from noisy readings)
    for (int i = 0; i < 5; i++) {
      sensorValues[i] = (sensorValues[i] * 2 + analogRead(lineSensors[i])) / 3;
    }

    // Read Ultrasonic Distance every 100ms (its echo wait blocks, so keep it off the fast loop)
    if (currentMillis - lastDistanceTime >= 100) {
      lastDistanceTime = currentMillis;
      currentDistance = getDistance();
      if (currentDistance > 0 && currentDistance < obstacleThreshold) obstacleHits++;
      else obstacleHits = 0;
    }

    // If in MANUAL OVERRIDE mode, do not run automatic state machine
    if (isManualMode || currentState == MANUAL_OVERRIDE) {
      obstacleDetected = (currentDistance > 0 && currentDistance < obstacleThreshold);
      // Hold-to-drive safety watchdog: if button is released or signal lost, stop motors automatically
      if (motorState != "STOPPED" && (currentMillis - lastManualCmdTime >= MANUAL_DEADMAN_TIMEOUT_MS)) {
        stopMotors();
      }
    }
    // AUTO MODE: IDLE State (Not in running mode and not disposing)
    else if (currentState == IDLE) {
      if (currentDistance > 0 && currentDistance < personThreshold) {
        if (!isBinFull) {
          // Not running, not disposing, and bin has space -> open lid!
          if (currentMillis - lastPersonDetectedTime >= 500) {
            Serial.println(F("[EVENT] Person detected near front while IDLE! Signaling OPEN_LID to ESP32."));
            broadcastLine("OPEN_LID");
            lastPersonDetectedTime = currentMillis;
          }
        } else {
          // Bin is FULL: Front person is NOT detected for lid opening (suppressed to prevent overflow)
          static unsigned long lastFullAlertTime = 0;
          if (currentMillis - lastFullAlertTime >= 3000) {
            lastFullAlertTime = currentMillis;
            Serial.println(F("[PROXIMITY IGNORED] Person present in front, but trash bin is FULL! Detection suppressed & lid kept closed."));
          }
        }
      }
    }
    // AUTO MODE: Waiting while Controller opens lid, tilts 3x, and closes lid
    else if (currentState == WAITING_TILT) {
      stopMotors();
    }
    // AUTO MODE: Moving Forward to Disposal Area OR Running Backward to Initial Station
    else if (currentState == MOVING_DISPOSAL || currentState == RETURNING) {
      if (obstacleHits >= 2) {
        if (!obstacleDetected) {
          Serial.print(F("[OBSTACLE ALARM] Obstacle detected at "));
          Serial.print(currentDistance);
          Serial.print(F(" cm <= threshold "));
          Serial.print(obstacleThreshold);
          Serial.println(F(" cm! Emergency stop engaged."));
        }
        obstacleDetected = true;
        stopMotors();
        pidReset();
      } else {
        if (obstacleDetected) {
          Serial.println(F("[OBSTACLE CLEARED] Path is now clear. Resuming line tracking."));
        }
        obstacleDetected = false;

        // Track if robot has left the starting all-black pad (forward from base, or backward from disposal)
        if (!hasLeftStartZone) {
          if (!isAllSensorsBlackZero() && (currentMillis - stateStartTime >= LEAVE_START_MIN_MS)) {
            hasLeftStartZone = true;
            if (currentState == MOVING_DISPOSAL) {
              Serial.println(F("[TRACK] Cleared base pad. Following line forward to disposal area."));
            } else {
              Serial.println(F("[TRACK] Cleared disposal pad in reverse. Following line backward to initial position."));
            }
          }
        }

        // Check if all 5 sensors detect black after leaving the start zone
        if (hasLeftStartZone && isAllSensorsBlackZero()) {
          if (currentState == MOVING_DISPOSAL) {
            // Reached all-black Disposal Station: STOP immediately without turning around, signal Controller to open lid & tilt!
            stopMotors();
            currentState = WAITING_TILT;
            Serial.println(F("[EVENT] All sensors BLACK at Disposal Area! Stopped immediately. Signaling ARRIVED_DISPOSAL to Controller."));
            broadcastLine("ARRIVED_DISPOSAL"); // Controller will open lid first, then tilt smoothly 3x
          } else if (currentState == RETURNING) {
            // Reached all-black Initial Station in reverse: STOP immediately in IDLE, facing forward ready for next run!
            stopMotors();
            currentState = IDLE;
            Serial.println(F("[EVENT] All sensors BLACK at Initial Station! Backed into initial position. Ready in IDLE."));
            broadcastLine("ARRIVED_BASE");
          }
        } else {
          if (currentState == MOVING_DISPOSAL) {
            runLineFollowing(); // Forward line following
          } else {
            runLineFollowingBackward(); // Backward line following
          }
        }
      }
    }
  }

  // 4. Send Status Telemetry to ESP32/NodeMCU & Serial Monitor (every 500ms)
  if (currentMillis - lastSerialTime >= 500) {
    lastSerialTime = currentMillis;
    sendTelemetry();
  }
}

void processCommand(String rawCmd) {
  unsigned long now = millis();
  rawCmd.trim();
  if (rawCmd.length() == 0) return;

  // Extract valid command even if half-duplex SoftwareSerial had a leading noise character
  String cmd = rawCmd;
  const char* knownTokens[] = {
    "MODE_MANUAL", "MODE_AUTO", "MAN_FWD", "MAN_BWD", "MAN_LEFT", "MAN_RIGHT", "MAN_STOP",
    "MOVE_TO_DISPOSAL", "RETURN_TO_BASE", "CAL_BLACK", "CAL_LINE", "BIN_FULL,", "SET,", "CAL,", "PID,", "IDLE", "STOP", "MOVE"
  };
  for (unsigned int i = 0; i < sizeof(knownTokens) / sizeof(knownTokens[0]); i++) {
    int idx = rawCmd.indexOf(knownTokens[i]);
    if (idx >= 0) {
      cmd = rawCmd.substring(idx);
      break;
    }
  }

  Serial.print(F("[UNO CMD RX] '"));
  Serial.print(cmd);
  Serial.println(F("'"));

  // Mode Switching: AUTO vs MANUAL OVERRIDE
  if (cmd == "MODE_MANUAL" || cmd == "manual") {
    isManualMode = true;
    currentState = MANUAL_OVERRIDE;
    stopMotors();
    broadcastLine("ACK:MODE_MANUAL");
    return;
  } else if (cmd == "MODE_AUTO" || cmd == "auto") {
    isManualMode = false;
    currentState = IDLE;
    stopMotors();
    broadcastLine("ACK:MODE_AUTO");
    return;
  }

  // Manual Override Motor Commands (Hold-to-drive momentary control)
  if (cmd == "MAN_FWD") {
    isManualMode = true;
    currentState = MANUAL_OVERRIDE;
    lastManualCmdTime = millis();
    moveForward(baseSpeed);
    broadcastLine("ACK:MAN_FWD");
    return;
  } else if (cmd == "MAN_BWD") {
    isManualMode = true;
    currentState = MANUAL_OVERRIDE;
    lastManualCmdTime = millis();
    moveBackward(baseSpeed);
    broadcastLine("ACK:MAN_BWD");
    return;
  } else if (cmd == "MAN_LEFT") {
    isManualMode = true;
    currentState = MANUAL_OVERRIDE;
    lastManualCmdTime = millis();
    turnLeft(baseSpeed);
    broadcastLine("ACK:MAN_LEFT");
    return;
  } else if (cmd == "MAN_RIGHT") {
    isManualMode = true;
    currentState = MANUAL_OVERRIDE;
    lastManualCmdTime = millis();
    turnRight(baseSpeed);
    broadcastLine("ACK:MAN_RIGHT");
    return;
  } else if (cmd == "MAN_STOP") {
    isManualMode = true;
    currentState = MANUAL_OVERRIDE;
    stopMotors();
    broadcastLine("ACK:MAN_STOP");
    return;
  }

  // Automated Commands
  if (cmd == "MOVE_TO_DISPOSAL" || cmd == "MOVE" || cmd == "move") {
    isManualMode = false;
    currentState = MOVING_DISPOSAL;
    pidReset();
    hasLeftStartZone = false;
    spinClearedLine = false;
    stateStartTime = now;
    motorState = "FORWARD";
    broadcastLine("ACK:MOVE_TO_DISPOSAL");
  } else if (cmd == "RETURN_TO_BASE") {
    isManualMode = false;
    currentState = RETURNING;
    pidReset();
    hasLeftStartZone = false;
    spinClearedLine = false;
    stateStartTime = now;
    motorState = "FORWARD";
    broadcastLine("ACK:RETURN_TO_BASE");
  } else if (cmd == "IDLE" || cmd == "STOP" || cmd == "stop") {
    currentState = isManualMode ? MANUAL_OVERRIDE : IDLE;
    stopMotors();
    broadcastLine("ACK:STOP");
  } else if (cmd.startsWith("SET,")) {
    int firstComma = cmd.indexOf(',', 4);
    if (firstComma > 0) {
      int newSpd = cmd.substring(4, firstComma).toInt();
      int newObs = cmd.substring(firstComma + 1).toInt();
      if (newSpd > 0) baseSpeed = constrain(newSpd, 0, 255);
      if (newObs > 0) obstacleThreshold = constrain(newObs, 5, 300);
    }
  } else if (cmd.startsWith("BIN_FULL,")) {
    isBinFull = (cmd.substring(9).toInt() == 1);
    if (isBinFull) {
      Serial.println(F("[BIN FULL NOTIFICATION] Received BIN_FULL=1 from Controller. Front proximity detection DISABLED."));
    } else {
      Serial.println(F("[BIN SPACE NOTIFICATION] Received BIN_FULL=0 from Controller. Front proximity detection ENABLED."));
    }
  } else if (cmd.startsWith("CAL,")) {
    // CAL,blackThreshold,lineThreshold,spinMs,personCm
    int v[4] = {BLACK_ZERO_THRESHOLD, LINE_DETECT_THRESHOLD, (int)SPIN_Opposite_MS, personThreshold};
    int start = 4;
    for (int i = 0; i < 4 && start <= (int)cmd.length(); i++) {
      int comma = cmd.indexOf(',', start);
      String part = (comma < 0) ? cmd.substring(start) : cmd.substring(start, comma);
      if (part.length() > 0) v[i] = part.toInt();
      if (comma < 0) break;
      start = comma + 1;
    }
    BLACK_ZERO_THRESHOLD = constrain(v[0], 0, 1023);
    LINE_DETECT_THRESHOLD = constrain(v[1], 0, 1023);
    SPIN_Opposite_MS = (unsigned long)constrain(v[2], 200, 10000);
    personThreshold = constrain(v[3], 5, 400);
    saveCalibration();
  } else if (cmd.startsWith("PID,")) {
    // PID,kp,ki,kd
    int c1 = cmd.indexOf(',', 4);
    int c2 = (c1 > 0) ? cmd.indexOf(',', c1 + 1) : -1;
    if (c1 > 0 && c2 > 0) {
      Kp = constrain(cmd.substring(4, c1).toFloat(), 0.0f, 500.0f);
      Ki = constrain(cmd.substring(c1 + 1, c2).toFloat(), 0.0f, 50.0f);
      Kd = constrain(cmd.substring(c2 + 1).toFloat(), 0.0f, 500.0f);
      pidReset();
      saveCalibration();
    }
  } else if (cmd == "CAL_BLACK") {
    // Place robot on the all-black pad: threshold = brightest sensor reading + safety margin
    int maxVal = 0;
    for (int i = 0; i < 5; i++) if (sensorValues[i] > maxVal) maxVal = sensorValues[i];
    BLACK_ZERO_THRESHOLD = constrain(maxVal + 60, 20, 1023);
    saveCalibration();
    broadcastLine("ACK:CAL_BLACK");
  } else if (cmd == "CAL_LINE") {
    // Place center sensor on the black guide line and outer sensors on the white floor
    int blackVal = sensorValues[2];
    int whiteVal = (sensorValues[0] > sensorValues[4]) ? sensorValues[0] : sensorValues[4];
    if (whiteVal > blackVal) {
      LINE_DETECT_THRESHOLD = constrain((whiteVal + blackVal) / 2, 50, 1000);
      saveCalibration();
      Serial.print(F("[CAL] Auto line threshold set to: "));
      Serial.print(LINE_DETECT_THRESHOLD);
      Serial.print(F(" (Black line: ")); Serial.print(blackVal);
      Serial.print(F(", White floor: ")); Serial.print(whiteVal); Serial.println(F(")"));
      broadcastLine("ACK:CAL_LINE");
    }
  }
}

// Returns true when all 5 sensors read <= BLACK_ZERO_THRESHOLD (all black start / dump zone)
bool isAllSensorsBlackZero() {
  for (int i = 0; i < 5; i++) {
    if (sensorValues[i] > BLACK_ZERO_THRESHOLD) {
      return false;
    }
  }
  return true;
}

// Returns true when one middle black line (S2 / A2) is sensed while outer sensors see white floor
bool isMiddleBlackLineSensed() {
  if (isAllSensorsBlackZero()) return false;
  bool centerBlack = (sensorValues[2] < LINE_DETECT_THRESHOLD);
  bool outerWhite = (sensorValues[0] >= LINE_DETECT_THRESHOLD && sensorValues[4] >= LINE_DETECT_THRESHOLD);
  return centerBlack && outerWhite;
}

void pidReset() {
  pidIntegral = 0.0f;
  lastError = 0.0f;
}

// Drive each wheel toward its target with a slew limit so speed changes are gradual (no jerks)
void driveWheels(int targetL, int targetR) {
  targetL = constrain(targetL, -255, 255);
  targetR = constrain(targetR, -255, 255);
  wheelL += constrain(targetL - wheelL, -SLEW_STEP, SLEW_STEP);
  wheelR += constrain(targetR - wheelR, -SLEW_STEP, SLEW_STEP);

  // Motor A = left wheel, Motor B = right wheel
  analogWrite(PWMA, abs(wheelL));
  digitalWrite(AIN1, wheelL >= 0 ? HIGH : LOW);
  digitalWrite(AIN2, wheelL >= 0 ? LOW : HIGH);
  analogWrite(PWMB, abs(wheelR));
  digitalWrite(BIN1, wheelR >= 0 ? HIGH : LOW);
  digitalWrite(BIN2, wheelR >= 0 ? LOW : HIGH);

  currentSpeed = (abs(wheelL) + abs(wheelR)) / 2;
  if (wheelL < wheelR - 25) motorState = "LEFT";
  else if (wheelR < wheelL - 25) motorState = "RIGHT";
  else motorState = "FORWARD";
}

void runLineFollowing() {
  // While still inside the all-black start box, drive straight (with smooth ramp-up) to exit it
  if (!hasLeftStartZone && isAllSensorsBlackZero()) {
    driveWheels(baseSpeed, baseSpeed);
    pidReset();
    return;
  }

  // Weighted line position from analog readings:
  // Black line is < LINE_DETECT_THRESHOLD, White floor is > LINE_DETECT_THRESHOLD
  long sum = 0, weighted = 0;
  bool lineSeen = false;
  for (int i = 0; i < 5; i++) {
    int v = sensorValues[i];
    if (v < LINE_DETECT_THRESHOLD) {
      lineSeen = true;
      int w = LINE_DETECT_THRESHOLD - v; // Darkness intensity (lower ADC = darker black = higher weight)
      sum += w;
      weighted += (long)w * (i - 2) * 100L;
    }
  }

  float error;
  bool lost = !(lineSeen && sum > 0);
  if (!lost) {
    error = (float)weighted / (float)sum / 100.0f;   // -2.0 .. +2.0
  } else if (fabs(lastError) < 0.3f) {
    // Line lost while basically centered: creep straight ahead
    int creep = baseSpeed - 50 > 50 ? baseSpeed - 50 : 50;
    driveWheels(creep, creep);
    return;
  } else {
    // Line lost off to one side: keep steering back toward the side it was last seen
    error = (lastError > 0) ? 2.5f : -2.5f;
  }

  pidIntegral = constrain(pidIntegral + error, -30.0f, 30.0f);   // anti-windup clamp
  float derivative = lost ? 0.0f : (error - lastError);          // no D spike when the line is lost
  float correction = Kp * error + Ki * pidIntegral + Kd * derivative;
  lastError = error;

  // Slow down in sharp corrections so the robot doesn't overshoot
  int speed = baseSpeed - (int)(fabs(error) * 15.0f);
  if (speed < 50) speed = 50;

  // error < 0 => line on the left => left wheel slower, right wheel faster => steer left
  driveWheels(speed + (int)correction, speed - (int)correction);
}

// Backward line following: robot reverses along the line from disposal area back to initial base
void runLineFollowingBackward() {
  int revBase = -constrain(baseSpeed, 80, 160); // Controlled reverse speed

  // While still clearing the disposal all-black pad, back straight out
  if (!hasLeftStartZone && isAllSensorsBlackZero()) {
    driveWheels(revBase, revBase);
    pidReset();
    return;
  }

  // Weighted line position from analog readings:
  long sum = 0, weighted = 0;
  bool lineSeen = false;
  for (int i = 0; i < 5; i++) {
    int v = sensorValues[i];
    if (v < LINE_DETECT_THRESHOLD) {
      lineSeen = true;
      int w = LINE_DETECT_THRESHOLD - v;
      sum += w;
      weighted += (long)w * (i - 2) * 100L;
    }
  }

  float error;
  bool lost = !(lineSeen && sum > 0);
  if (!lost) {
    error = (float)weighted / (float)sum / 100.0f; // -2.0 .. +2.0
  } else if (fabs(lastError) < 0.3f) {
    // Line lost while basically centered: creep straight backward
    driveWheels(revBase + 20, revBase + 20);
    return;
  } else {
    error = (lastError > 0) ? 2.5f : -2.5f;
  }

  pidIntegral = constrain(pidIntegral + error, -30.0f, 30.0f);
  float derivative = lost ? 0.0f : (error - lastError);
  float correction = (Kp * 0.85f) * error + Ki * pidIntegral + (Kd * 0.85f) * derivative;
  lastError = error;

  // In reverse: if line is to the right (error > 0), right wheel pulls backward faster
  // so the rear pivots left and the front sensor swings right over the line.
  int targetL = revBase + (int)correction;
  int targetR = revBase - (int)correction;
  driveWheels(targetL, targetR);
}

long getDistance() {
  digitalWrite(TRIG_PIN, LOW);
  unsigned long startMicros = micros();
  while (micros() - startMicros < 2) {} // Non-blocking microsecond wait

  digitalWrite(TRIG_PIN, HIGH);
  startMicros = micros();
  while (micros() - startMicros < 10) {} // Non-blocking microsecond wait

  digitalWrite(TRIG_PIN, LOW);
  long duration = pulseIn(ECHO_PIN, HIGH, 12000); // 12ms timeout (~2m) keeps the control loop responsive
  if (duration == 0) return -1;
  return duration * 0.034 / 2;
}

void moveForward(int speed) {
  wheelL = speed; wheelR = speed;
  currentSpeed = speed;
  motorState = "FORWARD";
  analogWrite(PWMA, speed);
  analogWrite(PWMB, speed);
  digitalWrite(AIN1, HIGH);
  digitalWrite(AIN2, LOW);
  digitalWrite(BIN1, HIGH);
  digitalWrite(BIN2, LOW);
}

void moveBackward(int speed) {
  wheelL = -speed; wheelR = -speed;
  currentSpeed = speed;
  motorState = "BACKWARD";
  analogWrite(PWMA, speed);
  analogWrite(PWMB, speed);
  digitalWrite(AIN1, LOW);
  digitalWrite(AIN2, HIGH);
  digitalWrite(BIN1, LOW);
  digitalWrite(BIN2, HIGH);
}

void turnLeft(int speed) {
  wheelL = -speed; wheelR = speed;
  currentSpeed = speed;
  motorState = "LEFT";
  analogWrite(PWMA, speed);
  analogWrite(PWMB, speed);
  digitalWrite(AIN1, LOW);
  digitalWrite(AIN2, HIGH);
  digitalWrite(BIN1, HIGH);
  digitalWrite(BIN2, LOW);
}

void turnRight(int speed) {
  wheelL = speed; wheelR = -speed;
  currentSpeed = speed;
  motorState = "RIGHT";
  analogWrite(PWMA, speed);
  analogWrite(PWMB, speed);
  digitalWrite(AIN1, HIGH);
  digitalWrite(AIN2, LOW);
  digitalWrite(BIN1, LOW);
  digitalWrite(BIN2, HIGH);
}

void stopMotors() {
  wheelL = 0; wheelR = 0;
  currentSpeed = 0;
  motorState = "STOPPED";
  analogWrite(PWMA, 0);
  analogWrite(PWMB, 0);
  digitalWrite(AIN1, LOW);
  digitalWrite(AIN2, LOW);
  digitalWrite(BIN1, LOW);
  digitalWrite(BIN2, LOW);
}

void broadcastLine(String msg) {
  Serial.println(msg);
  espSerial.println(msg);
}

void sendTelemetry() {
  String stateStr = "IDLE";
  if (currentState == MANUAL_OVERRIDE) stateStr = "MANUAL";
  else if (currentState == MOVING_DISPOSAL) stateStr = "MOVING";
  else if (false) stateStr = "SPINNING";
  else if (currentState == WAITING_TILT) stateStr = "DUMPING";
  else if (currentState == RETURNING) stateStr = "RETURNING";

  // Format: STATUS,speed,S0,S1,S2,S3,S4,motor_state,obstacle_status,state,distance
  String packet = "STATUS," + String(currentSpeed) + ",";
  for (int i = 0; i < 5; i++) {
    packet += String(sensorValues[i]) + ",";
  }
  packet += motorState + ",";
  packet += (obstacleDetected ? "1" : "0");
  packet += "," + stateStr + "," + String(currentDistance);
  packet += "," + String(BLACK_ZERO_THRESHOLD) + "," + String(LINE_DETECT_THRESHOLD) + "," + String(SPIN_Opposite_MS) + "," + String(personThreshold);
  packet += "," + String(Kp, 2) + "," + String(Ki, 2) + "," + String(Kd, 2);

  espSerial.println(packet); // Transmit machine packet to ESP32/NodeMCU
  espSerial.listen();        // Immediately resume listening for incoming commands
  
  // Detailed diagnostic output for USB Serial Monitor
  Serial.print(F("[UNO TELEMETRY] St:")); Serial.print(stateStr);
  Serial.print(F(" | Spd:")); Serial.print(currentSpeed);
  Serial.print(F(" (")); Serial.print(motorState); Serial.print(F(")"));
  Serial.print(F(" | Dist:")); Serial.print(currentDistance); Serial.print(F("cm"));
  Serial.print(F(" | Obstacle:")); Serial.print(obstacleDetected ? F("YES") : F("NO"));
  Serial.print(F(" | IR:["));
  for (int i = 0; i < 5; i++) {
    Serial.print(sensorValues[i]);
    if (i < 4) Serial.print(F(","));
  }
  Serial.print(F("] | PID:")); Serial.print(Kp, 1); Serial.print(F("/")); Serial.print(Ki, 1); Serial.print(F("/")); Serial.println(Kd, 1);
}
