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
#define THRESHOLD 850
int LINE_DETECT_THRESHOLD = 850;              // 1 = BLACK (> THRESHOLD), 0 = WHITE (<= THRESHOLD)
int BLACK_ZERO_THRESHOLD = 850;               // All 5 sensors > THRESHOLD: All-black start/finish/dump zone
unsigned long SPIN_Opposite_MS = 1400;        // Time (ms) to spin around to face opposite direction
const unsigned long LEAVE_START_MIN_MS = 800; // Minimum travel time to clear starting all-black box
#define CAL_MAGIC 0xCD

// Global variable to remember last known direction (-1 for Left, 1 for Right, 0 for None)
int lastDirection = 0; 

// ============================================================
// SPEED CONFIGURATION (Adjust up to 255 if motors feel slow)
// ============================================================
const int BASE_SPEED   = 255; 
const int MILD_FAST    = 255; 
const int MILD_SLOW    = 200; // Gentle turn differential (adjust up to 255 if max power desired)
const int SHARP_FAST   = 200; 
const int SHARP_SLOW   = -40; // Negative values will reverse the wheel for sharp pivot

// --- Motor & Steering State ---
float Kp = 45.0f;
float Ki = 0.0f;
float Kd = 60.0f;
float pidIntegral = 0.0f;
float lastError = 0.0f;
int wheelL = 0, wheelR = 0;
const int SLEW_STEP = 25;
const unsigned long PID_LOOP_MS = 20; // control loop period (50 Hz)
unsigned long lastDistanceTime = 0;
int obstacleHits = 0;

// --- State Variables ---
enum RobotState {
  IDLE,
  MOVING_DISPOSAL,
  WAITING_TILT,
  RETURNING,
  LINE_FOLLOWING,
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
void setMotor(int left, int right);
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
  if (BLACK_ZERO_THRESHOLD < 600) BLACK_ZERO_THRESHOLD = 850;
  if (LINE_DETECT_THRESHOLD < 600) LINE_DETECT_THRESHOLD = 850;
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
    // STANDALONE LINE FOLLOWER MODE: Stays centered in the middle black line
    else if (currentState == LINE_FOLLOWING) {
      if (obstacleHits >= 2) {
        if (!obstacleDetected) {
          Serial.print(F("[OBSTACLE ALARM] Obstacle detected at "));
          Serial.print(currentDistance);
          Serial.println(F(" cm! Emergency stop engaged."));
        }
        obstacleDetected = true;
        stopMotors();
        pidReset();
      } else {
        if (obstacleDetected) {
          Serial.println(F("[OBSTACLE CLEARED] Path is clear. Resuming middle black line tracking."));
        }
        obstacleDetected = false;
        runLineFollowing(); // Centers on middle sensor (S2)
      }
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
    "LINE_FOLLOW", "FOLLOW_LINE", "LINE_TRACK", "TRACK_LINE", "LINE",
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
    int spd = (baseSpeed > 160) ? baseSpeed : 220;
    moveForward(spd);
    broadcastLine("ACK:MAN_FWD");
    return;
  } else if (cmd == "MAN_BWD") {
    isManualMode = true;
    currentState = MANUAL_OVERRIDE;
    lastManualCmdTime = millis();
    int spd = (baseSpeed > 160) ? baseSpeed : 220;
    moveBackward(spd);
    broadcastLine("ACK:MAN_BWD");
    return;
  } else if (cmd == "MAN_LEFT") {
    isManualMode = true;
    currentState = MANUAL_OVERRIDE;
    lastManualCmdTime = millis();
    int spd = (baseSpeed > 160) ? baseSpeed : 220;
    turnLeft(spd);
    broadcastLine("ACK:MAN_LEFT");
    return;
  } else if (cmd == "MAN_RIGHT") {
    isManualMode = true;
    currentState = MANUAL_OVERRIDE;
    lastManualCmdTime = millis();
    int spd = (baseSpeed > 160) ? baseSpeed : 220;
    turnRight(spd);
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
  if (cmd == "LINE_FOLLOW" || cmd == "FOLLOW_LINE" || cmd == "LINE_TRACK" || cmd == "TRACK_LINE" || cmd == "LINE" || cmd == "line") {
    isManualMode = false;
    currentState = LINE_FOLLOWING;
    pidReset();
    hasLeftStartZone = true;
    motorState = "FORWARD";
    Serial.println(F("[COMMAND] LINE FOLLOWER mode engaged: Centering on middle black line."));
    broadcastLine("ACK:LINE_FOLLOW");
    return;
  }

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
    // Place robot on the all-black pad: threshold = lowest sensor reading on black - safety margin
    int minVal = 1023;
    for (int i = 0; i < 5; i++) if (sensorValues[i] < minVal) minVal = sensorValues[i];
    BLACK_ZERO_THRESHOLD = constrain(minVal - 60, 50, 1023);
    LINE_DETECT_THRESHOLD = BLACK_ZERO_THRESHOLD;
    saveCalibration();
    broadcastLine("ACK:CAL_BLACK");
  } else if (cmd == "CAL_LINE") {
    // Place center sensor on the black guide line (>850) and outer sensors on the white floor (<850)
    int blackVal = sensorValues[2];
    int whiteVal = (sensorValues[0] < sensorValues[4]) ? sensorValues[0] : sensorValues[4];
    if (blackVal > whiteVal) {
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
// Returns true when all 5 sensors detect black (> LINE_DETECT_THRESHOLD)
bool isAllSensorsBlackZero() {
  for (int i = 0; i < 5; i++) {
    if (sensorValues[i] <= LINE_DETECT_THRESHOLD) {
      return false;
    }
  }
  return true;
}

// Returns true when center sensor (S3 / A2) is on black while outer sensors see white
bool isMiddleBlackLineSensed() {
  if (isAllSensorsBlackZero()) return false;
  bool centerBlack = (sensorValues[2] > LINE_DETECT_THRESHOLD);
  bool outerWhite = (sensorValues[0] <= LINE_DETECT_THRESHOLD && sensorValues[4] <= LINE_DETECT_THRESHOLD);
  return centerBlack && outerWhite;
}

void pidReset() {
  pidIntegral = 0.0f;
  lastError = 0.0f;
}

// Drive each wheel toward its target with a slew limit so speed changes are gradual (no jerks)
// ============================================================
// MOTOR DRIVER FUNCTION (TB6612FNG: PWMA 5, AIN1 3, AIN2 2, PWMB 6, BIN1 4, BIN2 7)
// ============================================================
void setMotor(int left, int right) {
  // LEFT MOTOR (Motor A)
  int absLeft = abs(left);
  if (left > 0) {
    digitalWrite(AIN1, HIGH);
    digitalWrite(AIN2, LOW);
  } else if (left < 0) {
    digitalWrite(AIN1, LOW);
    digitalWrite(AIN2, HIGH);
  } else {
    digitalWrite(AIN1, LOW);
    digitalWrite(AIN2, LOW);
  }

  // RIGHT MOTOR (Motor B)
  int absRight = abs(right);
  if (right > 0) {
    digitalWrite(BIN1, HIGH);
    digitalWrite(BIN2, LOW);
  } else if (right < 0) {
    digitalWrite(BIN1, LOW);
    digitalWrite(BIN2, HIGH);
  } else {
    digitalWrite(BIN1, LOW);
    digitalWrite(BIN2, LOW);
  }

  analogWrite(PWMA, constrain(absLeft, 0, 255));
  analogWrite(PWMB, constrain(absRight, 0, 255));

  wheelL = left;
  wheelR = right;
  currentSpeed = (absLeft + absRight) / 2;
  if (left == 0 && right == 0) motorState = "STOPPED";
  else if (left < right - 25) motorState = "LEFT";
  else if (right < left - 25) motorState = "RIGHT";
  else motorState = "FORWARD";
}

void driveWheels(int targetL, int targetR) {
  setMotor(targetL, targetR);
}

// ============================================================
// TRACK FOLLOWING DIRECTIONS (Corrected Steering Directions)
// 5-Sensor Discrete Control Line Follower: Centers on middle black line
// ============================================================
void runLineFollowing() {
  // If moving forward to disposal, drive straight while clearing start box
  if (!hasLeftStartZone && isAllSensorsBlackZero()) {
    setMotor(BASE_SPEED, BASE_SPEED);
    return;
  }

  // 1 = BLACK (above threshold), 0 = WHITE (below threshold)
  int s1 = (sensorValues[0] > LINE_DETECT_THRESHOLD); // Far Left (A0)
  int s2 = (sensorValues[1] > LINE_DETECT_THRESHOLD); // Mid Left (A1)
  int s3 = (sensorValues[2] > LINE_DETECT_THRESHOLD); // Center (A2)
  int s4 = (sensorValues[3] > LINE_DETECT_THRESHOLD); // Mid Right (A3)
  int s5 = (sensorValues[4] > LINE_DETECT_THRESHOLD); // Far Right (A4)

  // -----------------------------------------------------------
  // AUTO-STOP COMBINATION (T-Junction / Crossline Intersection)
  // -----------------------------------------------------------
  if (s1 && s2 && s3 && s4 && s5) {
    setMotor(0, 0);
    lastDirection = 0;
    if (currentState == LINE_FOLLOWING) {
      currentState = IDLE;
      Serial.println(F(">> AUTO-STOP TRIPPED: Finish Line Encountered!"));
      broadcastLine("ACK:LINE_STOPPED");
    }
    return;
  }

  // -----------------------------------------------------------
  // TRACK FOLLOWING DIRECTIONS (Corrected Steering Directions)
  // -----------------------------------------------------------
  
  // Extreme Left Trigger -> Robot has drifted right, needs to turn Sharp Left.
  // Left motor slows/reverses, Right motor drives hard forward.
  if ((s1 && s2) || s1) {
    setMotor(SHARP_SLOW, SHARP_FAST);
    lastDirection = -1;
  }
  // Extreme Right Trigger -> Robot has drifted left, needs to turn Sharp Right.
  // Left motor drives hard forward, Right motor slows/reverses.
  else if ((s4 && s5) || s5) {
    setMotor(SHARP_FAST, SHARP_SLOW);
    lastDirection = 1;
  }
  // Gentle Left Trigger -> Robot slightly drifted right, needs Mild Left correction.
  else if ((s2 && s3) || s2) {
    setMotor(MILD_SLOW, MILD_FAST);
    lastDirection = -1;
  }
  // Gentle Right Trigger -> Robot slightly drifted left, needs Mild Right correction.
  else if ((s3 && s4) || s4) {
    setMotor(MILD_FAST, MILD_SLOW);
    lastDirection = 1;
  }
  // Center Sensor -> Safe on track, travel straight forward.
  else if (s3) {
    setMotor(BASE_SPEED, BASE_SPEED);
  }
  // 00000 -> Line completely lost (Recover based on last known state)
  else {
    if (lastDirection == 1) {
      setMotor(MILD_FAST, -MILD_SLOW); // Keep spinning right to re-acquire line
    } else if (lastDirection == -1) {
      setMotor(-MILD_SLOW, MILD_FAST); // Keep spinning left to re-acquire line
    } else {
      setMotor(0, 0);
    }
  }
}

// Reverse tracking along the line (disposal station -> base station)
void runLineFollowingBackward() {
  int revBase = -constrain(baseSpeed, 80, 200);

  // While still clearing the disposal all-black pad, back straight out
  if (!hasLeftStartZone && isAllSensorsBlackZero()) {
    setMotor(revBase, revBase);
    return;
  }

  int s1 = (sensorValues[0] > LINE_DETECT_THRESHOLD); // Far Left (A0)
  int s2 = (sensorValues[1] > LINE_DETECT_THRESHOLD); // Mid Left (A1)
  int s3 = (sensorValues[2] > LINE_DETECT_THRESHOLD); // Center (A2)
  int s4 = (sensorValues[3] > LINE_DETECT_THRESHOLD); // Mid Right (A3)
  int s5 = (sensorValues[4] > LINE_DETECT_THRESHOLD); // Far Right (A4)

  if ((s1 && s2) || s1) {
    setMotor(-SHARP_FAST, -SHARP_SLOW);
    lastDirection = -1;
  } else if ((s4 && s5) || s5) {
    setMotor(-SHARP_SLOW, -SHARP_FAST);
    lastDirection = 1;
  } else if ((s2 && s3) || s2) {
    setMotor(-MILD_FAST, -MILD_SLOW);
    lastDirection = -1;
  } else if ((s3 && s4) || s4) {
    setMotor(-MILD_SLOW, -MILD_FAST);
    lastDirection = 1;
  } else if (s3) {
    setMotor(revBase, revBase);
  } else {
    if (lastDirection == 1) {
      setMotor(-MILD_SLOW, MILD_FAST);
    } else if (lastDirection == -1) {
      setMotor(MILD_FAST, -MILD_SLOW);
    } else {
      setMotor(0, 0);
    }
  }
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
  else if (currentState == LINE_FOLLOWING) stateStr = "TRACKING";
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

long getDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long duration = pulseIn(ECHO_PIN, HIGH, 30000UL);
  if (duration == 0) return -1;
  return duration * 0.034 / 2;
}
