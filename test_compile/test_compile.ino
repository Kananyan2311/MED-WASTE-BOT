#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <ESP32Servo.h>
#include <time.h>
#include <math.h>
#include <Preferences.h>
Preferences prefs;
enum DisposalState { NONE, MOVING, DUMPING_DOWN, DUMPING_HOLD, DUMPING_SHAKE_UP, DUMPING_SHAKE_DOWN, DUMPING_UP, RETURNING, MANUAL_TILT_DOWN, MANUAL_TILT_UP };

#include <sys/time.h>
int tzOffsetSec = 0; // offset (seconds) of local time from UTC, set from the browser

// --- WiFi, Hotspot AP & mDNS Settings (Configurable via Web Portal & Stored in Flash ROM) ---
String wifiSSID = "";
String wifiPass = "";
String apSSID = "MedWaste-Master-AP";
String apPass = "medwaste123"; // Known default password for direct fallback connection
String mdnsHost = "medwaste";  // Resolves to http://medwaste.local
bool isAPMode = false;
DNSServer dnsServer;
const byte DNS_PORT = 53;

void loadWiFiPrefs() {
  prefs.begin("net", true);
  wifiSSID = prefs.getString("ssid", "");
  wifiPass = prefs.getString("pass", "");
  apPass = prefs.getString("ap_pass", "medwaste123");
  mdnsHost = prefs.getString("mdns", "medwaste");
  prefs.end();
}

void saveWiFiPrefs(String s, String p, String apP = "") {
  prefs.begin("net", false);
  prefs.putString("ssid", s);
  prefs.putString("pass", p);
  if (apP.length() >= 8) {
    apPass = apP;
    prefs.putString("ap_pass", apPass);
  }
  prefs.end();
}

void clearWiFiPrefs() {
  prefs.begin("net", false);
  prefs.clear();
  prefs.end();
}
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 0;
const int daylightOffset_sec = 3600;

WebServer server(80);

// --- Pin Definitions for ESP32-C3 ---
// Safe, dedicated general-purpose pins avoiding native USB (GPIO 18/19), SPI flash (GPIO 12-17), and boot strapping (GPIO 8/9):
#define UNO_RX 0    // ESP32-C3 GPIO 0 (RX1) <- Connects to Arduino Uno Pin D12 (ESP_TX_PIN)
#define UNO_TX 1    // ESP32-C3 GPIO 1 (TX1) -> Connects to Arduino Uno Pin D2  (ESP_RX_PIN)

// Ultrasonic Sensor (Trash Level)
#define TRIG_PIN 2  // ESP32-C3 GPIO 2 -> HC-SR04 Trig
#define ECHO_PIN 3  // ESP32-C3 GPIO 3 <- HC-SR04 Echo

// DRV8825 Stepper (Tilting Mechanism: 90 deg -> -20 deg)
#define DIR_PIN 4   // ESP32-C3 GPIO 4 -> DRV8825 DIR
#define STEP_PIN 5  // ESP32-C3 GPIO 5 -> DRV8825 STEP
#define EN_PIN 6    // ESP32-C3 GPIO 6 -> DRV8825 EN (Active LOW - holding torque kept active)

// Servo (Lid Opener)
#define SERVO_PIN 7 // ESP32-C3 GPIO 7 -> Servo PWM
Servo lidServo;

// --- Stepper Motor Tilt Configuration (Upright = 90 deg, Dump Tilt = -20 deg) ---
const float UPRIGHT_REF_DEG = 90.0f;
float DUMP_TARGET_DEG = -20.0f; // calibratable from the web dashboard

const float STEPPER_STEPS_PER_REV = 200.0f;                      // 1.8 deg/step (Full Step)
int TILT_STEPS = (int)round(((UPRIGHT_REF_DEG - DUMP_TARGET_DEG) / 360.0f) * STEPPER_STEPS_PER_REV); // recomputed after calibration
int dumpCycles = 3;                                              // number of tilt shake cycles
int currentDumpCycle = 0;
int shakeStepCount = 0;

// --- State Variables ---
unsigned long lastSensorReadTime = 0;
unsigned long lastLidOpenTime = 0;
bool isLidOpen = false;
bool isManualOverride = false;

// Master Data
String masterName = "Unit Alpha";
String masterUnoStatus = "Waiting...";
String masterTrashLevel = "Low";
int masterTrashCm = 0;

// Swarm Slaves Architecture (Dynamic Multi-Slave Support)
#define MAX_SWARM_SLAVES 8
struct SwarmSlave {
  String id;
  String name;
  String ip;
  String unoStatus;
  String trashLevel;
  int trashCm;
  unsigned long lastSeen;
  String pendingCmd;
};
SwarmSlave swarm[MAX_SWARM_SLAVES];
int swarmCount = 0;
String pendingGlobalSlaveCmd = "";

// Backward compatibility references for primary/first slave
String slaveName = "Unit Beta";
String slaveUnoStatus = "Offline";
String slaveTrashLevel = "Unknown";
int slaveTrashCm = 0;
unsigned long lastSlaveUpdate = 0;
String pendingSlaveCmd = "";

// Dual Serial Monitor Diagnostic Buffers
#define LOG_BUFFER_SIZE 40
String espLogs[LOG_BUFFER_SIZE];
int espLogCount = 0;
String unoLogs[LOG_BUFFER_SIZE];
int unoLogCount = 0;

// Disposal & Stepper State Machine (with Multi-Tilt Shake)
// enum moved to top
DisposalState dispState = NONE;
unsigned long dispTimer = 0;
int stepCount = 0;
bool isDisposing = false;
String disposalHistory = "";
int disposalCount = 0;
int scheduledHour = 23;
int scheduledMinute = 0;
int currentSpeed = 150;
int obsThreshold = 30;
int trashFullThreshold = 10;

// --- Calibration (persisted in flash via Preferences) ---
// prefs moved to top
int calBlack = 200, calLine = 500, calSpin = 1400, calPerson = 50; // Uno line-sensor / spin / person-range calibration
unsigned long holdMs = 2000;    // time held tilted while waste slides out
unsigned long lidOpenMs = 5000; // how long the lid stays open
int binDepthCm = 40;            // distance from bin sensor to bin floor (for fill %)
float calKp = 45.0f, calKi = 0.0f, calKd = 60.0f; // line-following PID gains (sent to the Uno)
bool unoSynced = false;

// --- Persistent Disposal History in Internal Flash ROM (Preferences / NVS) ---
void loadHistoryPrefs() {
  prefs.begin("history", true);
  disposalCount = prefs.getInt("count", 0);
  disposalHistory = prefs.getString("log", "");
  prefs.end();
}

void saveHistoryPrefs() {
  prefs.begin("history", false);
  prefs.putInt("count", disposalCount);
  if (disposalHistory.length() > 1400) {
    int lastNewline = disposalHistory.lastIndexOf('\n', 1300);
    if (lastNewline > 0) {
      disposalHistory = disposalHistory.substring(0, lastNewline);
    }
  }
  prefs.putString("log", disposalHistory);
  prefs.end();
}

void clearHistoryPrefs() {
  prefs.begin("history", false);
  prefs.clear();
  prefs.end();
  disposalCount = 0;
  disposalHistory = "";
}

String getLogTimestamp() {
  time_t raw = time(nullptr);
  if (raw > 1700000000) {
    struct tm ti;
    raw += tzOffsetSec;
    gmtime_r(&raw, &ti);
    char buf[12];
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d", ti.tm_hour, ti.tm_min, ti.tm_sec);
    return String(buf);
  }
  unsigned long s = millis() / 1000;
  char buf[12];
  snprintf(buf, sizeof(buf), "+%02lu:%02lu", (s / 60) % 60, s % 60);
  return String(buf);
}

void logESP(String msg) {
  String entry = "[" + getLogTimestamp() + "] " + msg;
  Serial.println(entry);
  if (espLogCount < LOG_BUFFER_SIZE) {
    espLogs[espLogCount++] = entry;
  } else {
    for (int i = 0; i < LOG_BUFFER_SIZE - 1; i++) espLogs[i] = espLogs[i + 1];
    espLogs[LOG_BUFFER_SIZE - 1] = entry;
  }
}

void logUno(String msg) {
  String entry = "[" + getLogTimestamp() + "] " + msg;
  if (unoLogCount < LOG_BUFFER_SIZE) {
    unoLogs[unoLogCount++] = entry;
  } else {
    for (int i = 0; i < LOG_BUFFER_SIZE - 1; i++) unoLogs[i] = unoLogs[i + 1];
    unoLogs[LOG_BUFFER_SIZE - 1] = entry;
  }
}

void recomputeTilt() {
  TILT_STEPS = (int)round(((UPRIGHT_REF_DEG - DUMP_TARGET_DEG) / 360.0f) * STEPPER_STEPS_PER_REV);
}

void saveCalibrationPrefs() {
  prefs.begin("cal", false);
  prefs.putInt("black", calBlack); prefs.putInt("line", calLine);
  prefs.putInt("spin", calSpin);   prefs.putInt("person", calPerson);
  prefs.putFloat("deg", DUMP_TARGET_DEG);
  prefs.putULong("hold", holdMs);  prefs.putULong("lid", lidOpenMs);
  prefs.putInt("bin", binDepthCm);
  prefs.putFloat("kp", calKp); prefs.putFloat("ki", calKi); prefs.putFloat("kd", calKd);
  prefs.putInt("spd", currentSpeed); prefs.putInt("obs", obsThreshold); prefs.putInt("trash", trashFullThreshold);
  prefs.putInt("cyc", dumpCycles);
  prefs.end();
}

void loadCalibrationPrefs() {
  prefs.begin("cal", true);
  calBlack = prefs.getInt("black", calBlack); calLine = prefs.getInt("line", calLine);
  calSpin = prefs.getInt("spin", calSpin);    calPerson = prefs.getInt("person", calPerson);
  DUMP_TARGET_DEG = prefs.getFloat("deg", DUMP_TARGET_DEG);
  holdMs = prefs.getULong("hold", holdMs);    lidOpenMs = prefs.getULong("lid", lidOpenMs);
  binDepthCm = prefs.getInt("bin", binDepthCm);
  calKp = prefs.getFloat("kp", calKp); calKi = prefs.getFloat("ki", calKi); calKd = prefs.getFloat("kd", calKd);
  currentSpeed = prefs.getInt("spd", currentSpeed); obsThreshold = prefs.getInt("obs", obsThreshold);
  trashFullThreshold = prefs.getInt("trash", trashFullThreshold);
  dumpCycles = prefs.getInt("cyc", dumpCycles);
  prefs.end();
}

void sendCalibrationToUno() {
  Serial1.println("SET," + String(currentSpeed) + "," + String(obsThreshold));
  Serial1.println("CAL," + String(calBlack) + "," + String(calLine) + "," + String(calSpin) + "," + String(calPerson));
  Serial1.println("PID," + String(calKp, 2) + "," + String(calKi, 2) + "," + String(calKd, 2));
  logESP("[CALIBRATION] Synced parameters with Uno (Kp=" + String(calKp, 1) + " Ki=" + String(calKi, 1) + " Kd=" + String(calKd, 1) + ")");
}

// Forward declarations
void openLid();
void closeLid();
long getDistance();
void startDisposalRoutine();
void startStepperTiltDown(DisposalState nextMode);
void startStepperTiltUp(DisposalState nextMode);
void handleDisposalMachine();

// --- HTML Template ---
// HTML dashboard
#include "index_html.h"


void setup() {
  Serial.begin(115200);
  loadCalibrationPrefs(); // restore saved calibration
  loadHistoryPrefs();     // restore persistent disposal history and count from Flash ROM
  recomputeTilt();
  Serial1.begin(9600, SERIAL_8N1, UNO_RX, UNO_TX); // Dedicated Serial to Uno

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  
  pinMode(DIR_PIN, OUTPUT);
  pinMode(STEP_PIN, OUTPUT);
  pinMode(EN_PIN, OUTPUT);
  digitalWrite(EN_PIN, LOW);  // Keep stepper ENABLED (holding torque) so bin stays upright at 90 deg
  
  lidServo.setPeriodHertz(50);
  lidServo.attach(SERVO_PIN, 500, 2400);
  closeLid();

  logESP("==============================================");
  logESP("  MED_WASTE_BOT - MASTER ESP32-C3 CONTROLLER  ");
  logESP("==============================================");
  logESP("[SYSTEM] Running on ESP32-C3 (RISC-V architecture)");
  logESP("[SYSTEM] DRV8825 holding torque ACTIVE (Upright 90 deg)");
  logESP("[SYSTEM] Loaded " + String(disposalCount) + " historical disposals from Flash ROM (NVS)");
  logESP("[SYSTEM] Tilt travel configured: 90 deg to " + String(DUMP_TARGET_DEG, 0) + " deg (" + String(TILT_STEPS) + " steps, " + String(dumpCycles) + " shake cycles)");

  // Load and Configure WiFi + Password-Protected Hotspot + mDNS Responder
  loadWiFiPrefs();
  WiFi.persistent(false);

  // Set concurrent Access Point + Station mode
  WiFi.mode(WIFI_AP_STA);

  // Always broadcast password-protected Hotspot so it is accessible anytime, even when leaving router range
  WiFi.softAP(apSSID.c_str(), apPass.c_str());
  IPAddress apIP = WiFi.softAPIP(); // default 192.168.4.1
  dnsServer.start(DNS_PORT, "*", apIP);
  logESP("[WIFI] Password-Protected Hotspot Active: '" + apSSID + "' (Password: " + apPass + ") @ " + apIP.toString());

  if (wifiSSID.length() > 0) {
    logESP("[WIFI] Connecting to configured station network: '" + wifiSSID + "'...");
    WiFi.begin(wifiSSID.c_str(), wifiPass.c_str());
    unsigned long startAttempt = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 8000) {
      delay(250);
      Serial.print(".");
    }
  }

  if (WiFi.status() == WL_CONNECTED) {
    isAPMode = false;
    logESP("[WIFI] Connected to station network! IP: " + WiFi.localIP().toString());
  } else {
    isAPMode = true;
    logESP("[WIFI] Station network unreachable. Running in Fallback Hotspot mode on '" + apSSID + "' (Pass: " + apPass + ")");
  }

  // Initialize mDNS Responder (enables http://medwaste.local)
  if (MDNS.begin(mdnsHost.c_str())) {
    MDNS.addService("http", "tcp", 80);
    logESP("[mDNS] Responder active! Access dashboard at: http://" + mdnsHost + ".local");
  } else {
    logESP("[mDNS] Error: MDNS responder could not be started.");
  }

  // Init NTP
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  setenv("TZ", "UTC0", 1); tzset();

  // Web Server Routes
  server.on("/", HTTP_GET, []() { server.send(200, "text/html", index_html); });
  server.on("/generate_204", HTTP_GET, []() { server.sendHeader("Location", "/"); server.send(302, "text/plain", ""); });
  server.on("/hotspot-detect.html", HTTP_GET, []() { server.sendHeader("Location", "/"); server.send(302, "text/plain", ""); });
  server.on("/canonical.html", HTTP_GET, []() { server.sendHeader("Location", "/"); server.send(302, "text/plain", ""); });

  server.on("/wifi_scan", HTTP_GET, []() {
    int n = WiFi.scanNetworks();
    String json = "[";
    for (int i = 0; i < n; i++) {
      if (i > 0) json += ",";
      json += "{\"ssid\":\"" + WiFi.SSID(i) + "\",\"rssi\":" + String(WiFi.RSSI(i)) + ",\"open\":" + String(WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "true" : "false") + "}";
    }
    json += "]";
    server.send(200, "application/json", json);
  });

  server.on("/set_wifi", HTTP_GET, []() {
    if (server.hasArg("ssid")) {
      String s = server.arg("ssid");
      String p = server.hasArg("pass") ? server.arg("pass") : "";
      String apP = server.hasArg("ap_pass") ? server.arg("ap_pass") : "";
      saveWiFiPrefs(s, p, apP);
      logESP("[WIFI] Saved credentials for '" + s + "'. AP Password updated: " + (apP.length() >= 8 ? apP : "(unchanged)") + ". Rebooting...");
      server.send(200, "text/plain", "WiFi credentials saved! Rebooting to connect...");
      delay(1000);
      ESP.restart();
    } else {
      server.send(400, "text/plain", "Missing ssid");
    }
  });

  server.on("/set_ap_pass", HTTP_GET, []() {
    if (server.hasArg("pass") && server.arg("pass").length() >= 8) {
      String newApP = server.arg("pass");
      prefs.begin("net", false);
      prefs.putString("ap_pass", newApP);
      prefs.end();
      apPass = newApP;
      WiFi.softAP(apSSID.c_str(), apPass.c_str());
      logESP("[WIFI] Fallback Hotspot password updated: '" + apPass + "'");
      server.send(200, "text/plain", "Hotspot password updated to: " + newApP);
    } else {
      server.send(400, "text/plain", "Password must be at least 8 characters (WPA2)");
    }
  });

  server.on("/reset_wifi", HTTP_GET, []() {
    clearWiFiPrefs();
    logESP("[WIFI] Reset to Hotspot Mode. Rebooting...");
    server.send(200, "text/plain", "WiFi reset. Restarting in hotspot setup mode...");
    delay(1000);
    ESP.restart();
  });
  
  // Real-time clock seed from browser
  server.on("/set_time", HTTP_GET, []() {
    if (server.hasArg("epoch")) {
      struct timeval tv;
      tv.tv_sec = (time_t)server.arg("epoch").toInt();
      tv.tv_usec = 0;
      settimeofday(&tv, NULL);
      int tzMin = server.hasArg("tz") ? server.arg("tz").toInt() : 0;
      tzOffsetSec = tzMin * 60;
      int a = abs(tzMin);
      String tz = "UTC" + String(tzMin >= 0 ? "-" : "") + String(a / 60) + (a % 60 ? ":" + String(a % 60) : "");
      setenv("TZ", tz.c_str(), 1);
      tzset();
      logESP("[TIME SYNC] System clock synchronized with browser: " + getLogTimestamp());
      server.send(200, "text/plain", "Time synced");
    } else {
      server.send(400, "text/plain", "Missing epoch");
    }
  });

  // Dual Serial Monitor Terminal Data Stream
  server.on("/terminal_data", HTTP_GET, []() {
    String json = "{\"esp\":[";
    for (int i = 0; i < espLogCount; i++) {
      if (i > 0) json += ",";
      String esc = espLogs[i];
      esc.replace("\\", "\\\\");
      esc.replace("\"", "\\\"");
      esc.replace("\r", "");
      esc.replace("\n", "\\n");
      json += "\"" + esc + "\"";
    }
    json += "],\"uno\":[";
    for (int i = 0; i < unoLogCount; i++) {
      if (i > 0) json += ",";
      String esc = unoLogs[i];
      esc.replace("\\", "\\\\");
      esc.replace("\"", "\\\"");
      esc.replace("\r", "");
      esc.replace("\n", "\\n");
      json += "\"" + esc + "\"";
    }
    json += "]}";
    server.send(200, "application/json", json);
  });

  // Direct Uno Command Sender
  server.on("/send_uno", HTTP_GET, []() {
    if (server.hasArg("cmd")) {
      String cmd = server.arg("cmd");
      Serial1.println(cmd);
      logESP("[UNO TX DIRECT] Dispatched: '" + cmd + "'");
      server.send(200, "text/plain", "Dispatched to Uno: " + cmd);
    } else {
      server.send(400, "text/plain", "Missing cmd parameter");
    }
  });

  // Clear Stored Disposal History from Flash ROM
  server.on("/clear_history", HTTP_GET, []() {
    clearHistoryPrefs();
    logESP("[HISTORY] Persistent disposal history cleared from Flash ROM");
    server.send(200, "text/plain", "Disposal history cleared from Flash ROM");
  });

  server.on("/data", HTTP_GET, []() {
    String json = "{";
    json += "\"masterName\":\"" + masterName + "\",";
    json += "\"masterUnoStatus\":\"" + masterUnoStatus + "\",";
    json += "\"masterTrashLevel\":\"" + masterTrashLevel + "\",";
    json += "\"masterTrashCm\":" + String(masterTrashCm) + ",";
    json += "\"manualMode\":" + String(isManualOverride ? "true" : "false") + ",";
    bool staConnected = (WiFi.status() == WL_CONNECTED);
    json += "\"wifi\":{\"ap\":" + String(isAPMode ? "true" : "false") + ",\"staConnected\":" + String(staConnected ? "true" : "false") + ",\"ssid\":\"" + (staConnected ? WiFi.SSID() : apSSID) + "\",\"ip\":\"" + (staConnected ? WiFi.localIP().toString() : WiFi.softAPIP().toString()) + "\",\"apSsid\":\"" + apSSID + "\",\"apPass\":\"" + apPass + "\",\"apIp\":\"" + WiFi.softAPIP().toString() + "\",\"mdns\":\"http://" + mdnsHost + ".local\"},";
    json += "\"timeSynced\":" + String(time(nullptr) > 1700000000 ? "true" : "false") + ",";
    json += "\"localEpoch\":" + String((long)time(nullptr) + tzOffsetSec) + ",";
    
    // Legacy single-slave backward compatibility
    if (millis() - lastSlaveUpdate > 10000) slaveUnoStatus = "Offline";
    json += "\"slaveName\":\"" + slaveName + "\",";
    json += "\"slaveUnoStatus\":\"" + slaveUnoStatus + "\",";
    json += "\"slaveTrashLevel\":\"" + slaveTrashLevel + "\",";
    json += "\"slaveTrashCm\":" + String(slaveTrashCm) + ",";

    // Dynamic Multi-Slave Swarm Array with Connected Tracking
    int onlineCount = 0;
    for (int i = 0; i < swarmCount; i++) {
      if (millis() - swarm[i].lastSeen <= 10000) onlineCount++;
    }
    json += "\"slaveConnected\":" + String(onlineCount > 0 ? "true" : "false") + ",";
    json += "\"slaveOnlineCount\":" + String(onlineCount) + ",";
    json += "\"swarmCount\":" + String(swarmCount) + ",";
    json += "\"swarm\":[";
    for (int i = 0; i < swarmCount; i++) {
      if (i > 0) json += ",";
      bool online = (millis() - swarm[i].lastSeen <= 10000);
      int secAgo = (millis() - swarm[i].lastSeen) / 1000;
      json += "{\"id\":\"" + swarm[i].id + "\",\"name\":\"" + swarm[i].name + "\",\"ip\":\"" + swarm[i].ip + "\",\"online\":" + (online ? "true" : "false") + ",\"secAgo\":" + String(secAgo) + ",\"status\":\"" + swarm[i].unoStatus + "\",\"trashLevel\":\"" + swarm[i].trashLevel + "\",\"trashCm\":" + String(swarm[i].trashCm) + "}";
    }
    json += "],";

    String histEsc = disposalHistory; histEsc.replace("\n", "\\n");
    json += "\"history\":\"" + histEsc + "\",";
    json += "\"disposalCount\":" + String(disposalCount) + ",";
    
    char schedBuf[10];
    sprintf(schedBuf, "%02d:%02d", scheduledHour, scheduledMinute);
    json += "\"schedTime\":\"" + String(schedBuf) + "\",";
    json += "\"disposing\":" + String(isDisposing ? "true" : "false") + ",";
    json += "\"dispState\":" + String((int)dispState) + ",";
    json += "\"lidOpen\":" + String(isLidOpen ? "true" : "false") + ",";
    json += "\"cal\":{\"black\":" + String(calBlack) + ",\"line\":" + String(calLine) + ",\"spin\":" + String(calSpin) + ",\"person\":" + String(calPerson) + ",\"dumpDeg\":" + String(DUMP_TARGET_DEG, 1) + ",\"hold\":" + String(holdMs) + ",\"lid\":" + String(lidOpenMs) + ",\"binDepth\":" + String(binDepthCm) + ",\"kp\":" + String(calKp, 2) + ",\"ki\":" + String(calKi, 2) + ",\"kd\":" + String(calKd, 2) + ",\"speed\":" + String(currentSpeed) + ",\"obs\":" + String(obsThreshold) + ",\"trash\":" + String(trashFullThreshold) + ",\"cycles\":" + String(dumpCycles) + "}";
    json += "}";
    server.send(200, "application/json", json);
  });

  server.on("/manual", HTTP_GET, []() {
    if (!server.hasArg("cmd")) {
      server.send(400, "text/plain", "Missing cmd");
      return;
    }
    String cmd = server.arg("cmd");
    logESP("[MANUAL CONTROL] " + cmd);
    if (cmd == "MODE_AUTO") {
      isManualOverride = false;
      isDisposing = false;
      dispState = NONE;
      Serial1.println("MODE_AUTO");
      pendingGlobalSlaveCmd = "MODE_AUTO";
    } else if (cmd == "MODE_MANUAL") {
      isManualOverride = true;
      isDisposing = false;
      dispState = NONE;
      Serial1.println("MODE_MANUAL");
      pendingGlobalSlaveCmd = "MODE_MANUAL";
    } else if (cmd == "MAN_FWD" || cmd == "MAN_BWD" || cmd == "MAN_LEFT" || cmd == "MAN_RIGHT" || cmd == "MAN_STOP") {
      isManualOverride = true;
      isDisposing = false;
      dispState = NONE;
      Serial1.println(cmd);
      pendingGlobalSlaveCmd = cmd;
    } else if (cmd == "LID_OPEN") {
      if (masterTrashLevel == "Full" || (masterTrashCm > 0 && masterTrashCm <= trashFullThreshold)) {
        logESP("[MANUAL CONTROL] Open lid BLOCKED: Trash bin is FULL (" + String(masterTrashCm) + " cm)!");
        server.send(200, "text/plain", "Blocked: Trash bin is FULL!");
        return;
      }
      if (isDisposing || dispState != NONE) {
        logESP("[MANUAL CONTROL] Open lid BLOCKED: Disposal routine active!");
        server.send(200, "text/plain", "Blocked: Disposal routine is active!");
        return;
      }
      openLid();
      pendingGlobalSlaveCmd = "LID_OPEN";
    } else if (cmd == "LID_CLOSE") {
      closeLid();
      pendingGlobalSlaveCmd = "LID_CLOSE";
    } else if (cmd == "TILT_DUMP") {
      isManualOverride = true;
      startStepperTiltDown(MANUAL_TILT_DOWN);
    } else if (cmd == "TILT_UPRIGHT") {
      isManualOverride = true;
      startStepperTiltUp(MANUAL_TILT_UP);
    }
    server.send(200, "text/plain", "Executed: " + cmd);
  });

  server.on("/set_schedule", HTTP_GET, []() {
    if(server.hasArg("time")) {
      String t = server.arg("time");
      scheduledHour = t.substring(0, 2).toInt();
      scheduledMinute = t.substring(3, 5).toInt();
      logESP("[SCHEDULE] Updated automated disposal time to: " + t);
      server.send(200, "text/plain", "Schedule updated to " + t);
    } else {
      server.send(400, "text/plain", "Missing time");
    }
  });

  server.on("/set_names", HTTP_GET, []() {
    if(server.hasArg("m") && server.hasArg("s")) {
      masterName = server.arg("m");
      slaveName = server.arg("s");
      logESP("[CONFIG] Master name: '" + masterName + "' | Primary slave: '" + slaveName + "'");
      server.send(200, "text/plain", "Unit names updated!");
    } else {
      server.send(400, "text/plain", "Missing names");
    }
  });

  // Sensor / actuator calibration (persisted in flash)
  server.on("/set_calibration", HTTP_GET, []() {
    if (server.hasArg("black"))  calBlack  = constrain(server.arg("black").toInt(), 0, 1023);
    if (server.hasArg("line"))   calLine   = constrain(server.arg("line").toInt(), 0, 1023);
    if (server.hasArg("spin"))   calSpin   = constrain(server.arg("spin").toInt(), 200, 10000);
    if (server.hasArg("person")) calPerson = constrain(server.arg("person").toInt(), 5, 400);
    if (server.hasArg("deg"))    DUMP_TARGET_DEG = constrain(server.arg("deg").toFloat(), -60.0f, 90.0f);
    if (server.hasArg("hold"))   holdMs    = constrain(server.arg("hold").toInt(), 200, 30000);
    if (server.hasArg("lid"))    lidOpenMs = constrain(server.arg("lid").toInt(), 1000, 60000);
    if (server.hasArg("bin"))    binDepthCm = constrain(server.arg("bin").toInt(), 5, 400);
    if (server.hasArg("kp"))     calKp = constrain(server.arg("kp").toFloat(), 0.0f, 500.0f);
    if (server.hasArg("ki"))     calKi = constrain(server.arg("ki").toFloat(), 0.0f, 50.0f);
    if (server.hasArg("kd"))     calKd = constrain(server.arg("kd").toFloat(), 0.0f, 500.0f);
    if (server.hasArg("cyc"))    dumpCycles = constrain(server.arg("cyc").toInt(), 1, 10);
    recomputeTilt();
    saveCalibrationPrefs();
    sendCalibrationToUno();
    pendingGlobalSlaveCmd = "CALX;" + String(calBlack) + ";" + String(calLine) + ";" + String(calSpin) + ";" + String(calPerson) + ";" + String((int)DUMP_TARGET_DEG) + ";" + String(holdMs) + ";" + String(lidOpenMs) + ";" + String(calKp, 2) + ";" + String(calKi, 2) + ";" + String(calKd, 2) + ";" + String(dumpCycles);
    logESP("[CALIBRATION] Calibration saved to NVS and queued for swarm!");
    server.send(200, "text/plain", "Calibration saved and queued for swarm!");
  });

  server.on("/calibrate", HTTP_GET, []() {
    String mode = server.arg("mode");
    if (mode == "black") {
      Serial1.println("CAL_BLACK");
      pendingGlobalSlaveCmd = "CAL_BLACK";
      logESP("[CALIBRATION] Triggered auto-calibration on all-black surface");
      server.send(200, "text/plain", "Black-pad calibration sent. Robot must be sitting on the all-black pad.");
    } else if (mode == "line") {
      Serial1.println("CAL_LINE");
      pendingGlobalSlaveCmd = "CAL_LINE";
      logESP("[CALIBRATION] Triggered auto-calibration on guide line");
      server.send(200, "text/plain", "Line calibration sent. Center sensor must be on the guide line.");
    } else {
      server.send(400, "text/plain", "Unknown mode");
    }
  });

  server.on("/set_settings", HTTP_GET, []() {
    if(server.hasArg("s") && server.hasArg("o") && server.hasArg("t")) {
      currentSpeed = server.arg("s").toInt();
      obsThreshold = server.arg("o").toInt();
      trashFullThreshold = server.arg("t").toInt();
      saveCalibrationPrefs();
      
      Serial1.print("SET,");
      Serial1.print(currentSpeed);
      Serial1.print(",");
      Serial1.println(obsThreshold);
      logESP("[SETTINGS] Speed=" + String(currentSpeed) + " | Obstacle=" + String(obsThreshold) + "cm | Full=" + String(trashFullThreshold) + "cm");
      server.send(200, "text/plain", "Settings saved!");
    } else {
      server.send(400, "text/plain", "Missing parameters");
    }
  });

  server.on("/dispose", HTTP_GET, []() {
    isManualOverride = false;
    startDisposalRoutine();
    server.send(200, "text/plain", "Disposal triggered for swarm!");
  });

  // Dynamic Swarm Slave Reporting Endpoint
  server.on("/slave_update", HTTP_POST, []() {
    String id = server.hasArg("id") ? server.arg("id") : "SLAVE_01";
    String name = server.hasArg("name") ? server.arg("name") : "Unit Beta";
    String ip = server.client().remoteIP().toString();
    String unoStatus = server.hasArg("unoStatus") ? server.arg("unoStatus") : "IDLE";
    int trashCm = server.hasArg("trashCm") ? server.arg("trashCm").toInt() : 0;
    String trashLevel = "Low";
    if (trashCm > 0 && trashCm <= trashFullThreshold) trashLevel = "Full";
    else if (trashCm > trashFullThreshold && trashCm <= trashFullThreshold + 20) trashLevel = "Halfway";

    // Locate existing slave or register new swarm unit
    int foundIdx = -1;
    for (int i = 0; i < swarmCount; i++) {
      if (swarm[i].id == id) {
        foundIdx = i;
        break;
      }
    }
    bool isNewSlave = false;
    if (foundIdx < 0 && swarmCount < MAX_SWARM_SLAVES) {
      foundIdx = swarmCount++;
      swarm[foundIdx].id = id;
      isNewSlave = true;
      logESP("[SWARM CONNECTED] New slave registered: " + id + " ('" + name + "') @ IP " + ip);
    }

    String slaveCmdToSend = "";
    if (foundIdx >= 0) {
      bool wasOffline = (millis() - swarm[foundIdx].lastSeen > 10000);
      swarm[foundIdx].name = name;
      swarm[foundIdx].ip = ip;
      swarm[foundIdx].unoStatus = unoStatus;
      swarm[foundIdx].trashCm = trashCm;
      swarm[foundIdx].trashLevel = trashLevel;
      swarm[foundIdx].lastSeen = millis();
      if (wasOffline && !isNewSlave) {
        logESP("[SWARM RECONNECTED] Slave robot " + id + " ('" + name + "') reconnected from IP " + ip);
      }
      if (swarm[foundIdx].pendingCmd.length() > 0) {
        slaveCmdToSend = swarm[foundIdx].pendingCmd;
        swarm[foundIdx].pendingCmd = "";
      }
    }

    // Update primary slave alias for compatibility
    slaveName = name;
    slaveUnoStatus = unoStatus;
    slaveTrashLevel = trashLevel;
    slaveTrashCm = trashCm;
    lastSlaveUpdate = millis();

    String prefix = "OK,";
    if (slaveCmdToSend.length() > 0) {
      prefix = "CMD:" + slaveCmdToSend + ",";
    } else if (pendingGlobalSlaveCmd.length() > 0) {
      prefix = "CMD:" + pendingGlobalSlaveCmd + ",";
      pendingGlobalSlaveCmd = ""; // Consumed
    } else if (isDisposing) {
      prefix = "DISPOSE,";
    }
    String resp = prefix + String(currentSpeed) + "," + String(obsThreshold);
    server.send(200, "text/plain", resp);
  });

  server.begin();
}

void loop() {
  dnsServer.processNextRequest();
  server.handleClient();
  unsigned long currentMillis = millis();

  // Background non-blocking Wi-Fi auto-reconnect if disconnected
  static unsigned long lastWifiCheck = 0;
  static wl_status_t lastWifiStatus = WL_IDLE_STATUS;
  if (wifiSSID.length() > 0 && (currentMillis - lastWifiCheck >= 7000)) {
    lastWifiCheck = currentMillis;
    wl_status_t curSt = WiFi.status();
    if (curSt != WL_CONNECTED) {
      isAPMode = true;
      if (lastWifiStatus == WL_CONNECTED) {
        logESP("[WIFI] Lost connection to '" + wifiSSID + "'. Fallback Hotspot '" + apSSID + "' remains active.");
      }
      WiFi.reconnect();
    } else {
      if (lastWifiStatus != WL_CONNECTED && lastWifiStatus != WL_IDLE_STATUS) {
        isAPMode = false;
        logESP("[WIFI] Reconnected to '" + wifiSSID + "'! Local IP: " + WiFi.localIP().toString());
      }
    }
    lastWifiStatus = curSt;
  }

  // Read from Uno over Dedicated Serial1
  if (Serial1.available() > 0) {
    String unoData = Serial1.readStringUntil('\n');
    unoData.trim();
    if (unoData.length() > 0) {
      logUno(unoData);
      if (unoData.startsWith("STATUS,")) {
        masterUnoStatus = unoData.substring(7); // strip "STATUS,"
        if (!unoSynced) { 
          unoSynced = true; 
          sendCalibrationToUno(); 
          Serial1.println((masterTrashLevel == "Full" || (masterTrashCm > 0 && masterTrashCm <= trashFullThreshold)) ? "BIN_FULL,1" : "BIN_FULL,0");
        }
      } else if (unoData == "OPEN_LID") {
        if (isDisposing || dispState != NONE) {
          logESP("[UNO EVENT] OPEN_LID ignored: Robot is in disposal mode.");
        } else if (masterTrashLevel == "Full" || (masterTrashCm > 0 && masterTrashCm <= trashFullThreshold)) {
          logESP("[UNO EVENT] Person proximity detected, but bin is FULL (" + String(masterTrashCm) + " cm). Lid locked closed!");
        } else if (!isManualOverride) {
          logESP("[UNO EVENT] Person proximity detected while idle & bin has space! Opening lid.");
          openLid();
        }
      } else if (unoData == "ARRIVED_DISPOSAL") {
        logESP("[UNO EVENT] Dump pad reached & 180 spin finished! Starting -20 deg tilt (" + String(dumpCycles) + " shake cycles)");
        currentDumpCycle = 0;
        shakeStepCount = 0;
        startStepperTiltDown(DUMPING_DOWN);
      } else if (unoData == "ARRIVED_BASE") {
        logESP("[UNO EVENT] Robot returned to all-black start base.");
        dispState = NONE;
        isDisposing = false;
      }
    }
  }

  // Manage Lid Auto-Close Timer (only in Auto mode)
  if (!isManualOverride && isLidOpen && (currentMillis - lastLidOpenTime >= lidOpenMs)) {
    closeLid();
  }

  // Read Trash Level (every 2 seconds)
  if (currentMillis - lastSensorReadTime >= 2000) {
    lastSensorReadTime = currentMillis;
    masterTrashCm = getDistance();
    if (masterTrashCm > 0 && masterTrashCm <= trashFullThreshold) masterTrashLevel = "Full";
    else if (masterTrashCm > trashFullThreshold && masterTrashCm <= trashFullThreshold + 20) masterTrashLevel = "Halfway";
    else masterTrashLevel = "Low";
    
    // Sync Bin Full status to Uno so front person detection is disabled when full
    bool isFull = (masterTrashLevel == "Full" || (masterTrashCm > 0 && masterTrashCm <= trashFullThreshold));
    static bool lastNotifiedFull = false;
    if (isFull != lastNotifiedFull) {
      lastNotifiedFull = isFull;
      Serial1.println(isFull ? "BIN_FULL,1" : "BIN_FULL,0");
      if (isFull) {
        logESP("[BIN STATUS] Trash bin is FULL (" + String(masterTrashCm) + " cm <= " + String(trashFullThreshold) + " cm). Front person detection DISABLED.");
        if (isLidOpen) closeLid();
      } else {
        logESP("[BIN STATUS] Trash bin has space (" + String(masterTrashCm) + " cm). Front person detection ENABLED.");
      }
    }

    // Auto schedule check (only when not in manual override)
    if (!isManualOverride) {
      struct tm timeinfo;
      if (getLocalTime(&timeinfo, 0)) {
        if (timeinfo.tm_hour == scheduledHour && timeinfo.tm_min == scheduledMinute && timeinfo.tm_sec < 5 && !isDisposing) {
          logESP("[SCHEDULE] Scheduled disposal time triggered!");
          startDisposalRoutine();
        }
      }
    }
  }

  handleDisposalMachine();
}

void openLid() {
  if (masterTrashLevel == "Full" || (masterTrashCm > 0 && masterTrashCm <= trashFullThreshold)) {
    logESP("[LID BLOCKED] Cannot open lid: Trash bin is FULL (" + String(masterTrashCm) + " cm). Empty bin or run disposal first!");
    return;
  }
  if (isDisposing || dispState != NONE) {
    logESP("[LID BLOCKED] Cannot open lid while disposal cycle is active.");
    return;
  }
  lidServo.write(90); // Open angle
  isLidOpen = true;
  lastLidOpenTime = millis();
}

void closeLid() {
  lidServo.write(0); // Close angle
  isLidOpen = false;
}

long getDistance() {
  digitalWrite(TRIG_PIN, LOW);
  unsigned long startMicros = micros();
  while (micros() - startMicros < 2) {}
  digitalWrite(TRIG_PIN, HIGH);
  startMicros = micros();
  while (micros() - startMicros < 10) {}
  digitalWrite(TRIG_PIN, LOW);
  long duration = pulseIn(ECHO_PIN, HIGH, 30000);
  if (duration == 0) return -1;
  return duration * 0.034 / 2;
}

void startStepperTiltDown(DisposalState mode) {
  dispState = mode;
  digitalWrite(EN_PIN, LOW);   // Enable Stepper
  digitalWrite(DIR_PIN, HIGH); // Tilt from 90 deg upright to -20 deg (110 deg travel)
  stepCount = 0;
  dispTimer = millis();
}

void startStepperTiltUp(DisposalState mode) {
  dispState = mode;
  digitalWrite(EN_PIN, LOW);   // Enable Stepper
  digitalWrite(DIR_PIN, LOW);  // Return from -20 deg back to 90 deg upright
  stepCount = 0;
  dispTimer = millis();
}

void startDisposalRoutine() {
  isDisposing = true;
  dispState = MOVING;
  Serial1.println("MOVE_TO_DISPOSAL"); // Command Master's Uno
  dispTimer = millis();
  
  // Record history
  struct tm timeinfo;
  char timeStringBuff[50];
  if (getLocalTime(&timeinfo, 0)) {
    strftime(timeStringBuff, sizeof(timeStringBuff), "%Y-%m-%d %H:%M:%S", &timeinfo);
  } else {
    strcpy(timeStringBuff, "Unknown Time");
  }
  disposalCount++;
  disposalHistory = String(timeStringBuff) + " - Swarm Disposal Cycle #" + String(disposalCount) + "\n" + disposalHistory;
  
  // Persist immediately into internal Flash ROM
  saveHistoryPrefs();
  logESP("[DISPOSAL ACTION] Disposal routine initiated (#" + String(disposalCount) + ") and saved to Flash ROM.");
}

void handleDisposalMachine() {
  unsigned long currentMillis = millis();
  
  if (dispState == DUMPING_DOWN || dispState == MANUAL_TILT_DOWN) {
    // Step from 90 deg upright to -20 deg (TILT_STEPS steps)
    if (currentMillis - dispTimer >= 2 && stepCount < TILT_STEPS) {
      dispTimer = currentMillis;
      digitalWrite(STEP_PIN, !digitalRead(STEP_PIN));
      if (digitalRead(STEP_PIN) == LOW) stepCount++;
    } else if (stepCount >= TILT_STEPS) {
      if (dispState == MANUAL_TILT_DOWN) {
        dispState = NONE;
        logESP("[STEPPER] Manual tilt to -20 deg complete. Holding.");
      } else {
        dispState = DUMPING_HOLD;
        dispTimer = currentMillis;
        logESP("[STEPPER] Reached -20 deg dump angle. Holding & starting waste ejection agitation.");
      }
    }
  } else if (dispState == DUMPING_HOLD) {
    if (currentMillis - dispTimer >= holdMs) {
      currentDumpCycle++;
      if (currentDumpCycle < dumpCycles) {
        dispState = DUMPING_SHAKE_UP;
        digitalWrite(DIR_PIN, LOW); // Agitation: tilt up partially (~36 deg)
        shakeStepCount = 0;
        dispTimer = currentMillis;
        logESP("[STEPPER] Agitation shake cycle " + String(currentDumpCycle) + "/" + String(dumpCycles));
      } else {
        dispState = DUMPING_UP;
        digitalWrite(DIR_PIN, LOW); // Return full 90 deg upright
        stepCount = 0;
        dispTimer = currentMillis;
        logESP("[STEPPER] All " + String(dumpCycles) + " shake cycles finished. Returning to 90 deg upright.");
      }
    }
  } else if (dispState == DUMPING_SHAKE_UP) {
    // Agitation shake up by 20 steps
    if (currentMillis - dispTimer >= 2 && shakeStepCount < 20) {
      dispTimer = currentMillis;
      digitalWrite(STEP_PIN, !digitalRead(STEP_PIN));
      if (digitalRead(STEP_PIN) == LOW) shakeStepCount++;
    } else if (shakeStepCount >= 20) {
      dispState = DUMPING_SHAKE_DOWN;
      digitalWrite(DIR_PIN, HIGH); // Snap back down to -20 deg
      shakeStepCount = 0;
      dispTimer = currentMillis;
    }
  } else if (dispState == DUMPING_SHAKE_DOWN) {
    // Return downward to -20 deg
    if (currentMillis - dispTimer >= 2 && shakeStepCount < 20) {
      dispTimer = currentMillis;
      digitalWrite(STEP_PIN, !digitalRead(STEP_PIN));
      if (digitalRead(STEP_PIN) == LOW) shakeStepCount++;
    } else if (shakeStepCount >= 20) {
      dispState = DUMPING_HOLD;
      dispTimer = currentMillis;
    }
  } else if (dispState == DUMPING_UP || dispState == MANUAL_TILT_UP) {
    // Return from -20 deg back to 90 deg upright (TILT_STEPS steps)
    if (currentMillis - dispTimer >= 2 && stepCount < TILT_STEPS) {
      dispTimer = currentMillis;
      digitalWrite(STEP_PIN, !digitalRead(STEP_PIN));
      if (digitalRead(STEP_PIN) == LOW) stepCount++;
    } else if (stepCount >= TILT_STEPS) {
      // Back at 90 deg upright: keep driver ENABLED so the stepper holds upright
      currentDumpCycle = 0;
      if (dispState == MANUAL_TILT_UP) {
        dispState = NONE;
        logESP("[STEPPER] Manual return to 90 deg upright complete.");
      } else {
        logESP("[STEPPER] Stepper restored upright at 90 deg. Instructing Uno RETURN_TO_BASE.");
        Serial1.println("RETURN_TO_BASE");
        dispState = RETURNING;
        dispTimer = currentMillis;
      }
    }
  }
}
