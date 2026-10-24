#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <ESP32Servo.h>
#include <time.h>
#include <math.h>
#include <Preferences.h>
#include <sys/time.h>
#include <esp_system.h>

RTC_NOINIT_ATTR uint32_t unit1ColdBootMagic;
Preferences prefs;
enum DisposalState { NONE, MOVING, DUMPING_LID_OPEN, DUMPING_DOWN, DUMPING_HOLD, DUMPING_SHAKE_UP, DUMPING_SHAKE_DOWN, DUMPING_UP, DUMPING_LID_CLOSE, RETURNING, MANUAL_TILT_DOWN, MANUAL_TILT_UP, MANUAL_TILT_CUSTOM };

int tzOffsetSec = 0; // offset (seconds) of local time from UTC, set from the browser

// --- WiFi, Hotspot AP & mDNS Settings (Configurable via Web Portal & Stored in Flash ROM) ---
String wifiSSID = "";
String wifiPass = "";
String apSSID = "MedWaste-1-AP";
String apPass = "medwaste123"; // Known default password for direct fallback connection
String mdnsHost = "medwaste1"; // Resolves to http://medwaste1.local
bool isAPMode = false;
DNSServer dnsServer;
const byte DNS_PORT = 53;

void loadWiFiPrefs() {
  prefs.begin("net", true);
  wifiSSID = prefs.getString("ssid", "");
  wifiPass = prefs.getString("pass", "");
  apPass = prefs.getString("ap_pass", "medwaste123");
  mdnsHost = prefs.getString("mdns", "medwaste1");
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
#define UNO_RX 2    // ESP32-C3 GPIO 0 (RX1) <- Connects to Arduino Uno Pin D12 (ESP_TX_PIN)
#define UNO_TX 3    // ESP32-C3 GPIO 1 (TX1) -> Connects to Arduino Uno Pin D2  (ESP_RX_PIN)

// Ultrasonic Sensor (Trash Level)
#define TRIG_PIN 4  // ESP32-C3 GPIO 2 -> HC-SR04 Trig
#define ECHO_PIN 5  // ESP32-C3 GPIO 3 <- HC-SR04 Echo

// DRV8825 Stepper (Tilting Mechanism: 90 deg -> -20 deg)
#define DIR_PIN 19   // ESP32-C3 GPIO 4 -> DRV8825 DIR
#define STEP_PIN 18  // ESP32-C3 GPIO 5 -> DRV8825 STEP
#define EN_PIN 7    // ESP32-C3 GPIO 6 -> DRV8825 EN (Active LOW - holding torque kept active)

// Servo (Lid Opener)
#define SERVO_PIN 6 // ESP32-C3 GPIO 7 -> Servo PWM
Servo lidServo;

// --- Stepper Motor Tilt Configuration (Upright Reference = 90 deg, Dump Tilt = 210 deg CCW) ---
float UPRIGHT_REF_DEG = 90.0f;  // Upright 12 o'clock reference angle (stored in NVS ROM)
float DUMP_TARGET_DEG = 210.0f; // Dump 8 o'clock target angle (stored in NVS ROM, CCW 120 deg)
float currentAngleDeg = 90.0f;  // Current physical angle of stepper (stored in NVS ROM)
float targetCustomDeg = 90.0f;  // Target custom angle for manual tilt
int targetStepsRemaining = 0;   // Steps to execute for custom tilt movement
bool tiltInvertDir = false;     // Invert CCW/CW motor direction (stored in NVS ROM)

const float STEPPER_STEPS_PER_REV = 200.0f;                      // 1.8 deg/step (Full Step)
int TILT_STEPS = (int)round(((210.0f - 90.0f) / 360.0f) * STEPPER_STEPS_PER_REV); // 67 steps for 120 deg
int dumpCycles = 3;                                              // number of full tilt cycles (3x tilt)
int currentDumpCycle = 0;
int shakeStepCount = 0;

// --- State Variables ---
unsigned long lastSensorReadTime = 0;
unsigned long lastLidOpenTime = 0;
bool isLidOpen = false;
bool isManualOverride = false;

// MedWaste Control 1 Data
String unitName = "MedWaste Control 1";
String unitUnoStatus = "Waiting...";
String unitTrashLevel = "Low";
int unitTrashCm = 0;

// Reliable Half-Duplex SoftwareSerial Command Queue to Uno
String pendingUnoCmd = "";
unsigned long pendingUnoCmdTime = 0;
int pendingUnoRetries = 0;

// Dual Serial Monitor Diagnostic Buffers (ESP Controller + Uno Motor Controller)
#define LOG_BUFFER_SIZE 40
String espLogs[LOG_BUFFER_SIZE];
int espLogCount = 0;
String unoLogs[LOG_BUFFER_SIZE];
int unoLogCount = 0;
bool isTestTiltOnly = false;

// Disposal & Stepper State Machine (with 3x Full Tilt)
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

// --- Calibration & Full Settings State (persisted in Flash ROM via Preferences) ---
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
#if ARDUINO_USB_CDC_ON_BOOT
  Serial0.println(entry);
#endif
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

void sendToUno(String cmd) {
  cmd.trim();
  if (cmd.length() == 0) return;
  Serial1.print("\n");
  Serial1.println(cmd);
  logESP("[MEDWASTE CONTROL 1 -> UNO TX] " + cmd);
  if (!cmd.startsWith("SET,") && !cmd.startsWith("CAL,") && !cmd.startsWith("PID,")) {
    pendingUnoCmd = cmd;
    pendingUnoCmdTime = millis();
    pendingUnoRetries = 2;
  }
}

void recomputeTilt() {
  TILT_STEPS = (int)round((fabs(UPRIGHT_REF_DEG - DUMP_TARGET_DEG) / 360.0f) * STEPPER_STEPS_PER_REV);
  if (TILT_STEPS < 1) TILT_STEPS = 1;
}

void saveCalibrationPrefs() {
  prefs.begin("cal", false);
  prefs.putInt("black", calBlack); prefs.putInt("line", calLine);
  prefs.putInt("spin", calSpin);   prefs.putInt("person", calPerson);
  prefs.putFloat("deg", DUMP_TARGET_DEG);
  prefs.putFloat("upDeg", UPRIGHT_REF_DEG);
  prefs.putFloat("curDeg", currentAngleDeg);
  prefs.putBool("invDir", tiltInvertDir);
  prefs.putULong("hold", holdMs);  prefs.putULong("lid", lidOpenMs);
  prefs.putInt("bin", binDepthCm);
  prefs.putFloat("kp", calKp); prefs.putFloat("ki", calKi); prefs.putFloat("kd", calKd);
  prefs.putInt("spd", currentSpeed); prefs.putInt("obs", obsThreshold); prefs.putInt("trash", trashFullThreshold);
  prefs.putInt("cyc", dumpCycles);
  prefs.putString("uName", unitName);
  prefs.putInt("sHr", scheduledHour);
  prefs.putInt("sMin", scheduledMinute);
  prefs.putBool("manMode", isManualOverride);
  prefs.end();
}

void loadCalibrationPrefs() {
  prefs.begin("cal", true);
  calBlack = prefs.getInt("black", calBlack); calLine = prefs.getInt("line", calLine);
  calSpin = prefs.getInt("spin", calSpin);    calPerson = prefs.getInt("person", calPerson);
  DUMP_TARGET_DEG = prefs.getFloat("deg", 210.0f);
  UPRIGHT_REF_DEG = prefs.getFloat("upDeg", 90.0f);
  currentAngleDeg = prefs.getFloat("curDeg", UPRIGHT_REF_DEG);
  tiltInvertDir = prefs.getBool("invDir", false);
  holdMs = prefs.getULong("hold", holdMs);    lidOpenMs = prefs.getULong("lid", lidOpenMs);
  binDepthCm = prefs.getInt("bin", binDepthCm);
  calKp = prefs.getFloat("kp", calKp); calKi = prefs.getFloat("ki", calKi); calKd = prefs.getFloat("kd", calKd);
  currentSpeed = prefs.getInt("spd", currentSpeed); obsThreshold = prefs.getInt("obs", obsThreshold);
  trashFullThreshold = prefs.getInt("trash", trashFullThreshold);
  dumpCycles = prefs.getInt("cyc", dumpCycles);
  unitName = prefs.getString("uName", "MedWaste Control 1");
  if (unitName == "Unit 1") unitName = "MedWaste Control 1";
  scheduledHour = prefs.getInt("sHr", scheduledHour);
  scheduledMinute = prefs.getInt("sMin", scheduledMinute);
  isManualOverride = prefs.getBool("manMode", false);
  prefs.end();
}

void sendCalibrationToUno() {
  Serial1.println("SET," + String(currentSpeed) + "," + String(obsThreshold));
  delay(20);
  Serial1.println("CAL," + String(calBlack) + "," + String(calLine) + "," + String(calSpin) + "," + String(calPerson));
  delay(20);
  Serial1.println("PID," + String(calKp, 2) + "," + String(calKi, 2) + "," + String(calKd, 2));
  logESP("[CALIBRATION] Synced parameters with Uno (Kp=" + String(calKp, 1) + " Ki=" + String(calKi, 1) + " Kd=" + String(calKd, 1) + ")");
}

// Forward declarations
void openLid(bool isDisposal = false);
void closeLid();
long getDistance();
void startDisposalRoutine();
void startDisposalDumping();
void startStepperTiltDown(DisposalState nextMode);
void startStepperTiltUp(DisposalState nextMode);
void moveStepperToAngle(float targetDeg);
void setStepperDirection(bool ccw);
bool executeOneStep(int totalSteps);
void sendMainPage();
void handleDisposalMachine();

// --- HTML Template ---
const char index_html[] PROGMEM = R"rawliteral(
<!-- " -->
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>MedWaste Control 1</title>
<style>
  :root {
    --bg1: #fff7fb;
    --bg2: #ffe8f3;
    --text: #1a1a1a;
    --text-muted: #574454;
    --border: #000000;
    --accent: #ff8fc2;
    --accent-hover: #ff74b4;
    --accent-soft: #ffe0ef;
    --sky: #bfe3ff;
    --sky-text: #1c496e;
    --warn: #ffd166;
    --warn-text: #5c3800;
    --danger: #ff2e63;
    --ok: #8ce9bf;
    --ok-text: #0f5233;
    --panel-bg: #ffffff;
    --card-pink: #fff0f7;
    --shadow: 5px 5px 0 #000;
    --shadow-sm: 3px 3px 0 #000;
    --font-body: 'Nunito', 'Segoe UI Variable', 'Segoe UI', ui-rounded, system-ui, -apple-system, sans-serif;
    --font-head: 'Fredoka', 'Nunito', ui-rounded, 'Segoe UI', system-ui, sans-serif;
    --font-mono: 'JetBrains Mono', ui-monospace, 'Cascadia Mono', Consolas, monospace;
  }
  * { box-sizing: border-box; }
  html, body {
    margin: 0;
    padding: 0;
    width: 100%;
    overflow-x: hidden;
  }
  body {
    padding: clamp(0.6rem, 1.8vw, 1.5rem);
    font-family: var(--font-body);
    font-size: clamp(14.5px, 1vw, 16px);
    font-weight: 600;
    color: var(--text);
    line-height: 1.5;
    -webkit-font-smoothing: antialiased;
    background-color: var(--bg2);
    background-image: radial-gradient(#ffb3d6 1.5px, transparent 1.5px);
    background-size: 24px 24px;
    min-height: 100vh;
  }

  /* Fluid Container Width for PC, Ultrawide, Tablet & Mobile */
  .top-app-bar,
  .top-nav-strip-wrapper,
  .layout {
    max-width: 1640px;
    width: 100%;
    margin-left: auto;
    margin-right: auto;
  }

  /* Layout Grid with Collapsible Sidebar */
  .layout {
    display: grid;
    grid-template-columns: 250px minmax(0, 1fr);
    gap: clamp(1rem, 2vw, 1.75rem);
    align-items: start;
    transition: grid-template-columns 0.25s ease;
  }
  .layout.sidebar-collapsed {
    grid-template-columns: minmax(0, 1fr);
  }
  .layout.sidebar-collapsed .sidebar {
    display: none;
  }
  main {
    min-width: 0;
    width: 100%;
  }

  /* Top App Bar with Always-Visible Clock & Status */
  .top-app-bar {
    margin-bottom: 0.75rem;
    background: var(--panel-bg);
    border: 3px solid #000;
    border-radius: 14px;
    padding: 0.55rem 0.95rem;
    box-shadow: var(--shadow);
    display: flex;
    justify-content: space-between;
    align-items: center;
    flex-wrap: wrap;
    gap: 0.65rem;
    position: sticky;
    top: 0.45rem;
    z-index: 1100;
  }
  .top-app-left {
    display: flex;
    align-items: center;
    gap: 0.65rem;
    min-width: 0;
  }
  .top-logo {
    display: flex;
    align-items: center;
    gap: 0.45rem;
    font-family: var(--font-head);
    font-size: clamp(1.05rem, 1.6vw, 1.25rem);
    font-weight: 800;
    white-space: nowrap;
  }
  .top-logo span {
    color: #000;
    background: #ff8fc2;
    border: 2px solid #000;
    width: 1.8rem;
    height: 1.8rem;
    display: inline-flex;
    align-items: center;
    justify-content: center;
    border-radius: 6px;
    font-size: 1.05rem;
    box-shadow: 2px 2px 0 #000;
    flex-shrink: 0;
  }
  .menu-toggle-btn {
    background: #ff8fc2;
    color: #000;
    border: 3px solid #000;
    border-radius: 10px;
    padding: 0.4rem 0.75rem;
    font-family: var(--font-body);
    font-size: 0.88rem;
    font-weight: 800;
    cursor: pointer;
    box-shadow: var(--shadow-sm);
    display: inline-flex;
    align-items: center;
    gap: 0.35rem;
    transition: all 0.15s ease;
    white-space: nowrap;
  }
  .menu-toggle-btn:hover {
    background: #ff74b4;
    transform: translate(1px, 1px);
    box-shadow: 1px 1px 0 #000;
  }

  .top-app-right {
    display: flex;
    align-items: center;
    gap: 0.55rem;
    flex-wrap: wrap;
    justify-content: flex-end;
  }

  /* Always-Visible Top Bar Live Clock */
  .top-clock-pill {
    background: #1a1a1a;
    color: #ffffff;
    border: 3px solid #000;
    border-radius: 10px;
    padding: 0.32rem 0.75rem;
    box-shadow: var(--shadow-sm);
    display: inline-flex;
    align-items: center;
    gap: 0.55rem;
    cursor: pointer;
    user-select: none;
    transition: transform 0.12s ease;
  }
  .top-clock-pill:hover {
    transform: translate(1px, 1px);
  }
  .top-clock-time {
    font-family: var(--font-mono);
    font-size: clamp(0.95rem, 1.3vw, 1.1rem);
    font-weight: 800;
    color: #ff9ec8;
    letter-spacing: 0.5px;
  }
  .top-clock-date {
    font-size: 0.78rem;
    font-weight: 700;
    color: #e2d9e0;
    border-left: 2px solid #444;
    padding-left: 0.55rem;
    white-space: nowrap;
  }

  /* Compact Top Navigation Pill Strip */
  .top-nav-strip-wrapper {
    margin-bottom: 1rem;
    background: var(--panel-bg);
    border: 3px solid #000;
    border-radius: 12px;
    padding: 0.4rem 0.55rem;
    box-shadow: var(--shadow-sm);
  }
  .top-nav-strip-wrapper.hidden {
    display: none;
  }
  .top-nav-strip {
    display: flex;
    align-items: center;
    gap: 0.5rem;
    overflow-x: auto;
    padding: 0.15rem 0.1rem;
    scrollbar-width: none;
    scroll-behavior: smooth;
    -webkit-overflow-scrolling: touch;
  }
  .top-nav-strip::-webkit-scrollbar {
    display: none;
  }
  .top-pill-btn {
    flex: 1 0 auto;
    justify-content: center;
    background: #fff;
    border: 2.5px solid #000;
    border-radius: 9px;
    padding: 0.48rem 0.85rem;
    font-family: var(--font-body);
    font-size: 0.88rem;
    font-weight: 800;
    color: var(--text);
    cursor: pointer;
    box-shadow: 2px 2px 0 #000;
    transition: all 0.12s ease;
    white-space: nowrap;
    display: inline-flex;
    align-items: center;
    gap: 0.4rem;
  }
  .top-pill-btn:hover {
    background: var(--card-pink);
    transform: translate(1px, 1px);
    box-shadow: 1px 1px 0 #000;
  }
  .top-pill-btn.active {
    background: #ff8fc2;
    color: #000;
    transform: translate(1px, 1px);
    box-shadow: 1px 1px 0 #000;
  }

  /* Sidebar Component */
  .sidebar {
    background: var(--panel-bg);
    border: 3px solid #000;
    border-radius: 14px;
    padding: 1.1rem;
    box-shadow: var(--shadow);
    position: sticky;
    top: 4.8rem;
    transition: transform 0.28s cubic-bezier(0.2, 0.9, 0.3, 1), opacity 0.25s ease;
  }
  .sidebar-header-row {
    display: flex;
    justify-content: space-between;
    align-items: center;
    margin-bottom: 0.85rem;
  }
  .sidebar h1 {
    font-family: var(--font-head);
    font-size: 1.3rem;
    font-weight: 700;
    margin: 0;
    display: flex;
    align-items: center;
    gap: 0.5rem;
  }
  .sidebar h1 span {
    color: #000;
    background: #ff8fc2;
    border: 2.5px solid #000;
    width: 1.9rem;
    height: 1.9rem;
    display: inline-flex;
    align-items: center;
    justify-content: center;
    border-radius: 7px;
    font-size: 1.1rem;
    box-shadow: 2px 2px 0 #000;
  }
  .sidebar-close-btn {
    background: #fff;
    border: 2px solid #000;
    border-radius: 6px;
    width: 28px;
    height: 28px;
    cursor: pointer;
    font-weight: 800;
    display: inline-flex;
    align-items: center;
    justify-content: center;
    box-shadow: 2px 2px 0 #000;
    font-size: 1.1rem;
  }
  .sidebar-close-btn:hover {
    background: var(--danger);
    color: #fff;
  }

  .clock-card {
    background: var(--card-pink);
    border: 3px solid #000;
    border-radius: 11px;
    padding: 0.75rem 0.85rem;
    margin-bottom: 1rem;
    box-shadow: var(--shadow-sm);
    text-align: center;
  }
  .clock-time {
    font-family: var(--font-mono);
    font-size: 1.55rem;
    font-weight: 800;
    color: #d81b60;
    letter-spacing: 1px;
  }
  .clock-date {
    font-size: 0.82rem;
    font-weight: 700;
    color: var(--text-muted);
    margin-top: 0.1rem;
  }
  .clock-sync {
    font-size: 0.72rem;
    font-weight: 700;
    margin-top: 0.35rem;
    color: var(--text-muted);
    display: flex;
    align-items: center;
    justify-content: center;
    gap: 0.4rem;
  }
  .clock-sync button {
    padding: 0.15rem 0.55rem;
    font-size: 0.72rem;
    border-radius: 6px;
    border: 2px solid #000;
    background: #fff;
    color: var(--text);
    cursor: pointer;
    font-family: inherit;
    font-weight: 800;
  }
  .clock-sync button:hover {
    background: var(--accent);
  }

  nav { display: flex; flex-direction: column; gap: 0.5rem; }
  .nav-btn {
    text-align: left;
    background: var(--panel-bg);
    border: 2.5px solid #000;
    padding: 0.6rem 0.85rem;
    font-family: var(--font-body);
    font-size: 0.92rem;
    color: var(--text);
    cursor: pointer;
    border-radius: 9px;
    font-weight: 800;
    box-shadow: var(--shadow-sm);
    transition: all 0.12s;
    display: flex;
    align-items: center;
    gap: 0.5rem;
  }
  .nav-btn:hover { transform: translate(1px, 1px); box-shadow: 1px 1px 0 #000; background: var(--card-pink); }
  .nav-btn.active { background: #ff8fc2; color: #000; transform: translate(1px, 1px); box-shadow: 1px 1px 0 #000; }

  /* Drawer Backdrop Overlay for Mobile/Tablet */
  .drawer-backdrop {
    position: fixed;
    inset: 0;
    background: rgba(30, 12, 24, 0.5);
    backdrop-filter: blur(3px);
    z-index: 2400;
    display: none;
    opacity: 0;
    transition: opacity 0.25s ease;
  }
  .drawer-backdrop.show {
    display: block;
    opacity: 1;
  }

  .section { display: none; }
  .section.active { display: block; animation: fade 0.2s ease; }
  @keyframes fade { from { opacity: 0; transform: translateY(3px); } to { opacity: 1; transform: none; } }

  h2 {
    font-family: var(--font-head);
    font-size: clamp(1.35rem, 2.2vw, 1.75rem);
    font-weight: 700;
    margin: 0 0 1rem 0;
    padding-bottom: 0.4rem;
    border-bottom: 3px solid #000;
  }
  h3 {
    font-family: var(--font-head);
    font-size: 0.95rem;
    font-weight: 700;
    letter-spacing: 0.06em;
    text-transform: uppercase;
    color: #1a1a1a;
    background: #ffd1e6;
    display: inline-block;
    padding: 0.2rem 0.65rem;
    border: 2.5px solid #000;
    box-shadow: 2px 2px 0 #000;
    margin: 1.4rem 0 0.75rem 0;
  }
  h4 { font-family: var(--font-head); font-weight: 700; }

  /* Responsive Multi-Aspect-Ratio Panel Grids */
  .panel-grid {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(min(100%, 330px), 1fr));
    gap: clamp(0.85rem, 1.6vw, 1.25rem);
    align-items: stretch;
  }
  .panel {
    background: var(--panel-bg);
    padding: clamp(0.95rem, 1.8vw, 1.35rem);
    border: 3px solid #000;
    border-radius: 12px;
    box-shadow: var(--shadow);
    min-width: 0;
  }
  .panel-grid .panel:nth-child(even) { background: var(--card-pink); }

  .data-list { list-style: none; padding: 0; margin: 0; }
  .data-list li {
    display: flex;
    justify-content: space-between;
    align-items: center;
    gap: 0.5rem;
    padding: 0.55rem 0;
    border-bottom: 2px dashed #000;
    font-size: 0.9rem;
    font-weight: 700;
    flex-wrap: wrap;
  }
  .data-list li:last-child { border-bottom: none; padding-bottom: 0; }
  .data-label { color: var(--text-muted); }
  .data-value { font-weight: 800; font-family: var(--font-mono); font-size: 0.92rem; text-align: right; }

  .tag {
    box-shadow: 2px 2px 0 #000;
    display: inline-flex;
    align-items: center;
    gap: 0.3rem;
    padding: 0.2rem 0.65rem;
    border-radius: 8px;
    font-size: 0.78rem;
    font-weight: 800;
    border: 2.5px solid #000;
    background: #fff;
    color: var(--text);
    white-space: nowrap;
  }
  .tag.active { background: var(--ok); color: var(--ok-text); }
  .tag.alert { background: var(--danger); color: #fff; }
  .tag.accent { background: var(--sky); color: var(--sky-text); }
  .tag.warn { background: var(--warn); color: var(--warn-text); }

  .control-group { margin-bottom: 0.95rem; }
  .control-group label { display: block; font-size: 0.88rem; font-weight: 800; margin-bottom: 0.35rem; }
  input[type="text"], input[type="number"], input[type="time"] {
    width: 100%;
    padding: 0.6rem 0.8rem;
    border: 2.5px solid #000;
    border-radius: 9px;
    font-size: 0.95rem;
    font-weight: 700;
    outline: none;
    font-family: inherit;
    color: var(--text);
    background: #fff;
  }
  input:focus { border-color: var(--accent-hover); box-shadow: 3px 3px 0 #000; background: var(--bg1); }

  button.primary, button.accent, button.warn, button.danger {
    border: 2.5px solid #000;
    padding: 0.6rem 1rem;
    font-family: var(--font-body);
    font-size: 0.9rem;
    font-weight: 800;
    border-radius: 9px;
    cursor: pointer;
    transition: all 0.12s;
    box-shadow: var(--shadow-sm);
    display: inline-flex;
    align-items: center;
    justify-content: center;
    gap: 0.35rem;
  }
  button.primary { background: #fff; color: var(--text); }
  button.primary:hover { background: var(--card-pink); transform: translate(1px, 1px); box-shadow: 1px 1px 0 #000; }
  button.accent { background: #ff8fc2; color: #000; }
  button.accent:hover { background: #ff74b4; transform: translate(1px, 1px); box-shadow: 1px 1px 0 #000; }
  button.warn { background: var(--warn); color: var(--warn-text); }
  button.warn:hover { transform: translate(1px, 1px); box-shadow: 1px 1px 0 #000; filter: brightness(0.97); }
  button.danger { background: var(--danger); color: #fff; }
  button.danger:hover { transform: translate(1px, 1px); box-shadow: 1px 1px 0 #000; }

  .dpad {
    display: grid;
    grid-template-columns: repeat(3, minmax(68px, 92px));
    grid-template-rows: repeat(3, 58px);
    gap: 0.55rem;
    justify-content: center;
    margin: 0.75rem 0;
  }
  .dpad button {
    font-family: var(--font-body);
    font-weight: 800;
    border: 3px solid #000;
    border-radius: 10px;
    cursor: pointer;
    box-shadow: var(--shadow-sm);
    background: #fff;
    color: var(--text);
    font-size: 0.88rem;
    transition: all 0.1s;
    touch-action: none;
    user-select: none;
    -webkit-user-select: none;
  }
  .dpad button:hover { background: var(--card-pink); }
  .dpad button:active { transform: scale(0.95); background: var(--accent-soft); }
  .dpad button.stop-btn { background: var(--danger); color: #fff; font-size: 0.95rem; }

  canvas { width: 100%; max-height: 230px; margin-top: 0.5rem; }
  .log-viewer {
    font-family: var(--font-mono);
    font-size: 0.84rem;
    font-weight: 600;
    color: var(--text);
    background: var(--card-pink);
    padding: 0.85rem;
    border: 3px solid #000;
    border-radius: 10px;
    height: 160px;
    overflow-y: auto;
  }

  /* ---- HIGH-VISIBILITY STICKY GLOBAL ALERT BANNER ---- */
  .global-alert-banner {
    display: none;
    position: sticky;
    top: 4.5rem;
    z-index: 1050;
    background: #ff1744;
    color: #ffffff;
    padding: 0.85rem 1.1rem;
    border: 3px solid #000;
    border-radius: 12px;
    margin-bottom: 1rem;
    font-weight: 800;
    font-size: clamp(0.92rem, 1.2vw, 1.05rem);
    box-shadow: 6px 6px 0 #000;
    animation: alertPulseBanner 1.5s infinite;
  }
  .global-alert-inner {
    display: flex;
    justify-content: space-between;
    align-items: center;
    flex-wrap: wrap;
    gap: 0.75rem;
  }
  .global-alert-text {
    display: flex;
    align-items: center;
    gap: 0.6rem;
    flex: 1;
    min-width: 200px;
  }
  .global-alert-badge {
    background: #ffea00;
    color: #000;
    border: 2px solid #000;
    border-radius: 6px;
    padding: 0.15rem 0.5rem;
    font-size: 0.78rem;
    font-weight: 900;
    text-transform: uppercase;
    flex-shrink: 0;
  }
  .global-alert-actions {
    display: flex;
    gap: 0.45rem;
    flex-wrap: wrap;
  }
  .global-alert-actions button {
    background: #fff;
    color: #000;
    border: 2px solid #000;
    border-radius: 8px;
    padding: 0.35rem 0.7rem;
    font-size: 0.8rem;
    font-weight: 900;
    cursor: pointer;
    box-shadow: 2px 2px 0 #000;
  }
  .global-alert-actions button:hover {
    background: #ffea00;
  }
  @keyframes alertPulseBanner {
    0%, 100% { box-shadow: 6px 6px 0 #000, 0 0 0 0 rgba(255, 23, 68, 0.7); }
    50% { box-shadow: 6px 6px 0 #000, 0 0 0 10px rgba(255, 23, 68, 0); }
  }

  /* ---- HIGH-VISIBILITY TOASTS & MODAL ALERTS ---- */
  .toast-wrap {
    position: fixed;
    top: 0.85rem;
    right: 0.85rem;
    display: flex;
    flex-direction: column;
    gap: 0.65rem;
    z-index: 3500;
    width: min(380px, calc(100vw - 1.7rem));
    pointer-events: none;
  }
  .toast {
    pointer-events: auto;
    background: #fff;
    border: 3px solid #000;
    border-left-width: 10px;
    border-radius: 12px;
    padding: 0.8rem 1rem;
    box-shadow: 6px 6px 0 #000;
    animation: slidein 0.22s ease;
    font-size: 0.92rem;
    font-weight: 700;
    cursor: pointer;
    position: relative;
  }
  .toast b {
    display: block;
    font-family: var(--font-head);
    font-weight: 800;
    font-size: 1.02rem;
    margin-bottom: 0.15rem;
  }
  .toast.critical {
    border-left-color: #ff1744;
    background: #ffe5ec;
    color: #6b001b;
    animation: slidein 0.22s ease, alertPulseBanner 1.5s infinite;
  }
  .toast.warn {
    border-left-color: #ff9f1c;
    background: #fff3d6;
    color: #5c3800;
  }
  .toast.info {
    border-left-color: #2196f3;
    background: #e8f4fd;
  }
  .toast.ok {
    border-left-color: #00c853;
    background: #e6fced;
    color: #0a4723;
  }
  @keyframes slidein { from { opacity: 0; transform: translateX(30px); } to { opacity: 1; transform: none; } }

  .modal-overlay {
    position: fixed;
    inset: 0;
    background: rgba(30, 12, 24, 0.6);
    backdrop-filter: blur(4px);
    display: none;
    align-items: center;
    justify-content: center;
    z-index: 3200;
    padding: 1rem;
  }
  .modal-overlay.show { display: flex; }
  .modal {
    background: #fff;
    border: 4px solid #000;
    border-radius: 16px;
    padding: 1.5rem;
    max-width: 440px;
    width: 100%;
    text-align: center;
    box-shadow: 10px 10px 0 #000;
    animation: pop 0.2s ease;
  }
  @keyframes pop { from { transform: scale(0.9); opacity: 0; } to { transform: none; opacity: 1; } }
  .modal-icon {
    width: 3.6rem;
    height: 3.6rem;
    border-radius: 50%;
    border: 3px solid #000;
    margin: 0 auto 0.8rem;
    display: flex;
    align-items: center;
    justify-content: center;
    font-family: var(--font-head);
    font-size: 1.9rem;
    font-weight: 800;
    color: #fff;
    background: #2196f3;
  }
  .modal.critical .modal-icon { background: #ff1744; animation: pulse 1s infinite; }
  .modal.warn .modal-icon { background: #ff9f1c; color: #000; }
  .modal.ok .modal-icon { background: #00c853; }
  @keyframes pulse { 0%,100% { box-shadow: 0 0 0 0 rgba(255,23,68,0.65); } 50% { box-shadow: 0 0 0 14px rgba(255,23,68,0); } }
  .modal h3 { background: none; border: none; box-shadow: none; padding: 0; display: block; margin: 0 0 0.4rem 0; font-size: 1.35rem; letter-spacing: 0; text-transform: none; color: var(--text); }
  .modal p { margin: 0 0 1.2rem 0; color: var(--text-muted); font-weight: 700; font-size: 0.98rem; }
  .modal-actions { display: flex; gap: 0.6rem; justify-content: center; flex-wrap: wrap; }

  /* ---- Calibration ---- */
  .sbar-row { display: grid; grid-template-columns: 32px 1fr 48px; gap: 0.5rem; align-items: center; margin: 0.35rem 0; font-size: 0.84rem; font-weight: 800; }
  .sbar-track { position: relative; height: 16px; background: var(--card-pink); border: 2.5px solid #000; border-radius: 8px; overflow: hidden; }
  .sbar-fill { height: 100%; width: 0; background: #ff8fc2; transition: width 0.25s; }
  .sbar-fill.black { background: #1a1a1a; }
  .sbar-fill.line { background: #8cc8f5; }
  .sbar-track i { position: absolute; top: -2px; bottom: -2px; width: 3px; }
  .sbar-track i.mk-b { background: #1a1a1a; }
  .sbar-track i.mk-l { background: #2f8fd8; }
  .legend { font-size: 0.76rem; color: var(--text-muted); margin-top: 0.55rem; }
  .legend .dot { display: inline-block; width: 10px; height: 10px; border-radius: 50%; margin: 0 4px 0 8px; vertical-align: middle; }
  .cal-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(min(100%, 210px), 1fr)); gap: 0.9rem 1.1rem; }
  .cal-grid input { max-width: 100%; }
  .hint { font-size: 0.78rem; font-weight: 700; color: var(--text-muted); margin-top: 0.25rem; }
  .switch-row { display: flex; align-items: center; gap: 0.55rem; margin: 0.55rem 0; font-weight: 800; font-size: 0.9rem; }
  .switch-row input[type="checkbox"] { width: 20px; height: 20px; accent-color: #ff74b4; flex-shrink: 0; }
  .alert-item { display: flex; align-items: center; flex-wrap: wrap; gap: 0.5rem; padding: 0.5rem 0; border-bottom: 2px dashed #000; font-size: 0.88rem; }
  .alert-item:last-child { border-bottom: none; }
  .alert-time { font-family: var(--font-mono); color: var(--text-muted); font-size: 0.78rem; min-width: 62px; font-weight: 800; }
  .terminal-box {
    height: 290px;
    overflow-y: auto;
    font-family: var(--font-mono);
    font-size: 0.82rem;
    line-height: 1.45;
    background: #0f0e13;
    padding: 0.75rem;
    border: 3px solid #000;
    border-radius: 8px;
    white-space: pre-wrap;
    word-break: break-word;
    color: #ff9ec8;
    box-shadow: inset 2px 2px 0 #000;
  }
  #unoConsole { color: #a8f0ff; }

  /* ---- Compact Status Pill ---- */
  .status-pill {
    background: var(--panel-bg);
    border: 2.5px solid #000;
    border-radius: 10px;
    padding: 0.32rem 0.7rem;
    box-shadow: var(--shadow-sm);
    display: inline-flex;
    align-items: center;
    gap: 0.45rem;
    font-size: 0.8rem;
    font-weight: 800;
    white-space: nowrap;
  }
  .status-pill.active { background: var(--ok); color: var(--ok-text); }
  .status-pill.alert { background: var(--danger); color: #fff; }
  .status-pill.warn { background: var(--warn); color: var(--warn-text); }
  .status-dot-pulse {
    display: inline-block;
    width: 9px;
    height: 9px;
    border-radius: 50%;
    background: #008035;
    box-shadow: 0 0 0 0 rgba(0, 200, 83, 0.7);
    animation: pulseDot 1.4s infinite;
    flex-shrink: 0;
  }
  .status-dot-red {
    display: inline-block;
    width: 9px;
    height: 9px;
    border-radius: 50%;
    background: #ffffff;
    border: 2px solid #000;
    flex-shrink: 0;
  }
  @keyframes pulseDot {
    0% { transform: scale(0.95); box-shadow: 0 0 0 0 rgba(0, 200, 83, 0.7); }
    70% { transform: scale(1); box-shadow: 0 0 0 7px rgba(0, 200, 83, 0); }
    100% { transform: scale(0.95); box-shadow: 0 0 0 0 rgba(0, 200, 83, 0); }
  }
  .unit-dropdown {
    padding: 0.48rem 0.75rem;
    border: 2.5px solid #000;
    border-radius: 9px;
    font-family: var(--font-body);
    font-size: 0.88rem;
    font-weight: 800;
    background: #fff;
    color: var(--text);
    box-shadow: var(--shadow-sm);
    cursor: pointer;
    outline: none;
    max-width: 100%;
  }
  .unit-dropdown:focus { background: var(--card-pink); }
  input[type="range"] {
    width: 100%;
    accent-color: #ff74b4;
    cursor: pointer;
    height: 8px;
  }

  /* ---- Responsive Breakpoints for All Aspect Ratios ---- */
  /* Ultrawide & Large PC Monitors (1440px+) */
  @media (min-width: 1440px) {
    .panel-grid {
      grid-template-columns: repeat(auto-fit, minmax(380px, 1fr));
    }
  }
  /* Tablet & Mobile Portrait/Landscape (<= 960px): Drawer Sidebar */
  @media (max-width: 960px) {
    .layout { grid-template-columns: minmax(0, 1fr); gap: 1rem; }
    .sidebar {
      position: fixed;
      top: 0;
      left: 0;
      bottom: 0;
      width: min(290px, 85vw);
      z-index: 2500;
      border-radius: 0 16px 16px 0;
      border-left: none;
      box-shadow: 8px 0 0 #000;
      background: var(--bg1);
      overflow-y: auto;
      transform: translateX(-105%);
    }
    .sidebar.mobile-drawer-open {
      transform: translateX(0);
    }
  }
  /* Mobile Phones (<= 640px) */
  @media (max-width: 640px) {
    body { padding: 0.5rem; }
    .top-app-bar {
      padding: 0.45rem 0.65rem;
      gap: 0.45rem;
      top: 0.25rem;
    }
    .top-app-right {
      width: 100%;
      justify-content: space-between;
    }
    .top-clock-pill {
      flex: 1;
      justify-content: center;
    }
    .top-pill-btn {
      padding: 0.42rem 0.7rem;
      font-size: 0.82rem;
    }
    .panel {
      padding: 0.9rem;
    }
    .dpad {
      grid-template-columns: repeat(3, minmax(64px, 1fr));
      grid-template-rows: repeat(3, 54px);
    }
    .global-alert-banner {
      top: 6.2rem;
      padding: 0.7rem 0.85rem;
    }
    .toast-wrap {
      top: auto;
      bottom: 0.6rem;
      right: 0.5rem;
      left: 0.5rem;
      width: auto;
    }
  }
  /* Extra Compact Screens / Split-View (<= 400px) */
  @media (max-width: 400px) {
    .top-clock-date { display: none; }
    .top-logo { font-size: 0.98rem; }
  }
</style>
<script>
  let chart;
  var initChart = function() {
    const canvas = document.getElementById('disposalChart');
    chart = {
      data: { datasets: [{ data: [0] }] },
      update: function() {
        if (!canvas) return;
        const ctx = canvas.getContext('2d');
        const w = canvas.width = canvas.clientWidth || 400;
        const h = canvas.height = 200;
        const val = Number(this.data.datasets[0].data[0]) || 0;
        const maxVal = Math.max(5, val + 2);
        ctx.clearRect(0, 0, w, h);
        ctx.strokeStyle = '#000';
        ctx.lineWidth = 3;
        ctx.beginPath();
        ctx.moveTo(45, 16);
        ctx.lineTo(45, h - 32);
        ctx.lineTo(w - 20, h - 32);
        ctx.stroke();
        ctx.fillStyle = '#1a1a1a';
        ctx.font = 'bold 13px sans-serif';
        ctx.textAlign = 'right';
        ctx.fillText(String(maxVal), 38, 24);
        ctx.fillText('0', 38, h - 28);
        const barW = Math.min(120, Math.max(50, (w - 80) * 0.35));
        const barH = Math.max(6, ((h - 56) * val) / maxVal);
        const bx = 45 + (w - 65 - barW) / 2;
        const by = (h - 32) - barH;
        ctx.fillStyle = '#000';
        ctx.fillRect(bx + 4, by + 4, barW, barH);
        ctx.fillStyle = '#ff8fc2';
        ctx.fillRect(bx, by, barW, barH);
        ctx.strokeRect(bx, by, barW, barH);
        ctx.fillStyle = '#1a1a1a';
        ctx.textAlign = 'center';
        ctx.font = 'bold 15px sans-serif';
        ctx.fillText(val + ' Disposals', bx + barW / 2, Math.max(15, by - 6));
        ctx.font = 'bold 13px sans-serif';
        ctx.fillText('Total Completed Cycles', bx + barW / 2, h - 10);
      }
    };
    chart.update();
  };

  var scanWifi = function() {
    notify('info', 'Scanning', 'Scanning available Wi-Fi networks...');
    const box = document.getElementById('wifiScanResults');
    const list = document.getElementById('wifiList');
    box.style.display = 'block';
    list.innerHTML = 'Scanning nearby networks...';
    fetch('/wifi_scan').then(r => r.json()).then(nets => {
      if (!nets || !nets.length) {
        list.innerHTML = '<div class="hint">No networks found.</div>';
        return;
      }
      list.innerHTML = '';
      nets.forEach(n => {
        const row = document.createElement('div');
        row.style.cssText = 'cursor:pointer; padding:0.3rem 0.5rem; border-bottom:1px dashed #000; display:flex; justify-content:space-between; align-items:center;';
        row.onclick = function() { selectWifi(n.ssid); };
        row.innerHTML = '<b>' + n.ssid + '</b> <span style="font-size:0.75rem; color:var(--text-muted);">' + (n.open ? 'OPEN' : 'SECURE') + ' (' + n.rssi + ' dBm)</span>';
        list.appendChild(row);
      });
    }).catch(e => {
      list.innerHTML = '<div class="hint">Failed to scan.</div>';
    });
  };

  var selectWifi = function(ssid) {
    document.getElementById('wifiSsidInput').value = ssid;
    document.getElementById('wifiPassInput').focus();
  };

  var saveWifi = function() {
    const s = document.getElementById('wifiSsidInput').value.trim();
    const p = document.getElementById('wifiPassInput').value;
    const apP = document.getElementById('apPassInput') ? document.getElementById('apPassInput').value.trim() : '';
    if (!s) { alert('Please enter an SSID'); return; }
    if (apP && apP.length < 8) { alert('Hotspot password must be at least 8 characters (WPA2)'); return; }
    const fallbackAp = (lastData && lastData.wifi && lastData.wifi.apSsid) ? lastData.wifi.apSsid : 'Hotspot AP';
    showModal({
      level: 'warn',
      title: 'Connect to ' + s + '?',
      msg: 'The robot will save these credentials and reboot to connect. If out of range, the fallback hotspot (' + fallbackAp + ') will remain active with your known password.',
      actions: [{
        label: 'Reboot & Connect',
        cls: 'accent',
        fn: () => {
          let url = '/set_wifi?ssid=' + encodeURIComponent(s) + '&pass=' + encodeURIComponent(p);
          if (apP) url += '&ap_pass=' + encodeURIComponent(apP);
          fetch(url)
            .then(r => r.text()).then(t => {
              notify('ok', 'Rebooting', t, { modal: true });
            });
        }
      }]
    });
  };

  var saveApPassOnly = function() {
    const apP = document.getElementById('apPassInput') ? document.getElementById('apPassInput').value.trim() : '';
    if (!apP || apP.length < 8) { alert('Hotspot password must be at least 8 characters (WPA2 standard)'); return; }
    fetch('/set_ap_pass?pass=' + encodeURIComponent(apP))
      .then(r => r.text()).then(t => {
        notify('ok', 'Hotspot Password', t);
        updateData();
      });
  };

  var resetWifi = function() {
    const fallbackAp = (lastData && lastData.wifi && lastData.wifi.apSsid) ? lastData.wifi.apSsid : 'Hotspot AP';
    showModal({
      level: 'critical',
      title: 'Reset Wi-Fi Settings?',
      msg: 'This will erase saved Wi-Fi credentials and keep the robot running in Access Point Hotspot mode (' + fallbackAp + ').',
      actions: [{
        label: 'Reset Now',
        cls: 'danger',
        fn: () => {
          fetch('/reset_wifi').then(r => r.text()).then(t => {
            notify('warn', 'Hotspot Mode', t, { modal: true });
          });
        }
      }]
    });
  };

  var showSection = function(id) {
    document.querySelectorAll('.section').forEach(el => el.classList.remove('active'));
    document.querySelectorAll('.nav-btn').forEach(el => el.classList.remove('active'));
    document.querySelectorAll('.top-pill-btn').forEach(el => el.classList.remove('active'));
    const targetSection = document.getElementById(id);
    if (targetSection) targetSection.classList.add('active');
    document.querySelectorAll(`[data-target="${id}"]`).forEach(el => el.classList.add('active'));

    const activePill = document.querySelector(`.top-pill-btn[data-target="${id}"]`);
    if (activePill) {
      activePill.scrollIntoView({ behavior: 'smooth', block: 'nearest', inline: 'center' });
    }

    if (id === 'analytics' && chart) {
      setTimeout(() => chart.update(), 50);
    }

    if (window.innerWidth <= 960) {
      closeSidebarDrawer();
    }
  };

  var toggleSidebar = function() {
    const layout = document.querySelector('.layout');
    const sidebar = document.getElementById('sidebarPanel');
    if (window.innerWidth <= 960) {
      const isOpen = sidebar && sidebar.classList.contains('mobile-drawer-open');
      if (isOpen) { closeSidebarDrawer(); } else { openSidebarDrawer(); }
    } else {
      if (layout) {
        layout.classList.toggle('sidebar-collapsed');
        const isCollapsed = layout.classList.contains('sidebar-collapsed');
        localStorage.setItem('medwaste_sidebar_collapsed', isCollapsed ? '1' : '0');
        updateSidebarToggleBtnText();
      }
    }
  };

  var openSidebarDrawer = function() {
    const sidebar = document.getElementById('sidebarPanel');
    const overlay = document.getElementById('drawerBackdrop');
    if (sidebar) sidebar.classList.add('mobile-drawer-open');
    if (overlay) overlay.classList.add('show');
    updateSidebarToggleBtnText();
  };

  var closeSidebarDrawer = function() {
    const sidebar = document.getElementById('sidebarPanel');
    const overlay = document.getElementById('drawerBackdrop');
    if (sidebar) sidebar.classList.remove('mobile-drawer-open');
    if (overlay) overlay.classList.remove('show');
    updateSidebarToggleBtnText();
  };

  var updateSidebarToggleBtnText = function() {
    const btn = document.getElementById('sidebarToggleBtn');
    if (!btn) return;
    const layout = document.querySelector('.layout');
    const sidebar = document.getElementById('sidebarPanel');
    const isMobile = window.innerWidth <= 960;
    if (isMobile) {
      const isOpen = sidebar && sidebar.classList.contains('mobile-drawer-open');
      btn.innerHTML = isOpen ? '<span>✕</span> Close' : '<span>☰</span> Menu';
    } else {
      const isCollapsed = layout && layout.classList.contains('sidebar-collapsed');
      btn.innerHTML = isCollapsed ? '<span>☰</span> Menu' : '<span>⇤</span> Sidebar';
    }
  };

  var initNavPreferences = function() {
    if (localStorage.getItem('medwaste_sidebar_collapsed') === '1' && window.innerWidth > 960) {
      const layout = document.querySelector('.layout');
      if (layout) layout.classList.add('sidebar-collapsed');
    }
    updateSidebarToggleBtnText();
    window.addEventListener('resize', function() {
      updateSidebarToggleBtnText();
      if (chart) chart.update();
    });
  };

  var getStatusTag = function(state, obstacle) {
    if (state === 'MANUAL') return `<span class="tag warn">Manual Override</span>`;
    if (obstacle === 'Yes') return `<span class="tag alert">⚠️ Obstacle</span>`;
    if (state === 'MOVING' || state === 'RETURNING' || state === 'SPINNING' || state === 'DUMPING') return `<span class="tag accent">${state}</span>`;
    if (state === 'IDLE') return `<span class="tag active">Idle</span>`;
    return `<span class="tag">Waiting</span>`;
  };

  var triggerDisposal = function() { fetch('/dispose').then(r => r.text()).then(t => notify('info', 'Disposal', t)); };

  // Hold-to-Drive (Momentary Control): Tap = brief move, Hold = continuous drive until release
  let holdInterval = null;
  let activeHoldCmd = null;

  var startHoldDrive = function(cmd, e) {
    if (e) {
      if (e.cancelable) e.preventDefault();
      e.stopPropagation();
    }
    if (activeHoldCmd === cmd) return;
    activeHoldCmd = cmd;
    clearInterval(holdInterval);
    sendManualCmd(cmd); // Immediate move on tap/touch
    holdInterval = setInterval(() => {
      sendManualCmd(cmd); // Keep-alive stream while button is held
    }, 220);
  };

  var stopHoldDrive = function(e) {
    if (e) {
      if (e.cancelable) e.preventDefault();
      e.stopPropagation();
    }
    if (activeHoldCmd) {
      activeHoldCmd = null;
      clearInterval(holdInterval);
      sendManualCmd('MAN_STOP'); // Immediate stop when finger/mouse is lifted
    }
  };

  var initDpadListeners = function() {
    document.querySelectorAll('.dpad-btn').forEach(btn => {
      const cmd = btn.getAttribute('data-cmd');
      if (!cmd) return;
      btn.addEventListener('pointerdown', (e) => startHoldDrive(cmd, e));
      btn.addEventListener('pointerup', stopHoldDrive);
      btn.addEventListener('pointercancel', stopHoldDrive);
      btn.addEventListener('pointerleave', stopHoldDrive);
      btn.addEventListener('contextmenu', (e) => e.preventDefault());
    });
    // Global safety release if pointer is lifted outside button
    window.addEventListener('pointerup', stopHoldDrive);
    window.addEventListener('pointercancel', stopHoldDrive);
  };

  var sendManualCmd = function(cmd) {
    fetch('/manual?cmd=' + encodeURIComponent(cmd))
      .then(r => r.text()).then(t => {
        notify('info', 'Command Executed', t);
        updateData();
      });
  };

  var syncTiltInput = function(val) {
    const num = document.getElementById('customTiltInput');
    const rng = document.getElementById('customTiltSlider');
    if (num) num.value = val;
    if (rng) rng.value = val;
  };

  var applyPresetAngle = function(deg) {
    if (deg === '' || deg === null || deg === undefined) return;
    syncTiltInput(deg);
    fetch('/tilt_angle?deg=' + encodeURIComponent(deg))
      .then(r => r.text()).then(t => {
        notify('ok', 'Tilt Angle', t);
        updateData();
      });
  };

  var sendCustomTilt = function() {
    const deg = document.getElementById('customTiltInput') ? document.getElementById('customTiltInput').value : 90;
    fetch('/tilt_angle?deg=' + encodeURIComponent(deg))
      .then(r => r.text()).then(t => {
        notify('ok', 'Custom Tilt', t);
        updateData();
      });
  };

  var syncRefInput = function(val) {
    if (val === '' || val === null || val === undefined) return;
    const inp = document.getElementById('refAngleInput');
    if (inp) inp.value = val;
  };

  var toggleInvertDir = function(cb) {
    fetch('/invert_dir?inv=' + (cb.checked ? '1' : '0'))
      .then(r => r.text()).then(t => {
        notify('ok', 'Stepper Direction', t);
        updateData();
      });
  };

  var setReferenceAngle = function() {
    const deg = document.getElementById('refAngleInput') ? document.getElementById('refAngleInput').value : 90;
    fetch('/set_ref_angle?deg=' + encodeURIComponent(deg))
      .then(r => r.text()).then(t => {
        notify('ok', 'Reference Angle Calibrated & Saved', t);
        updateData();
      });
  };

  var saveAllSettingsRom = function() {
    fetch('/save_all_settings').then(r => r.text()).then(t => {
      notify('ok', 'Saved to Internal Flash ROM', t);
      updateData();
    });
  };

  var setSchedule = function() {
    const t = document.getElementById('schedTime').value;
    if(t) fetch('/set_schedule?time=' + t).then(r => r.text()).then(t => notify('ok', 'Saved to Flash ROM', t));
  };
  var updateSettings = function() {
    const s = document.getElementById('speedInp').value;
    const o = document.getElementById('obsInp').value;
    const t = document.getElementById('trashInp').value;
    fetch(`/set_settings?s=${s}&o=${o}&t=${t}`).then(r => r.text()).then(t => notify('ok', 'Saved to Flash ROM', t));
  };
  var updateNames = function() {
    const m = document.getElementById('nameInpM').value;
    fetch(`/set_names?name=${encodeURIComponent(m)}`).then(r => r.text()).then(t => {
      notify('ok', 'Saved to Flash ROM', t);
      updateData();
    });
  };

  let initFieldsLoaded = false;
  let isFetchingData = false;
  let consecutiveFetchFails = 0;

  var updateData = function() {
    if (isFetchingData) return;
    isFetchingData = true;
    const ctrl = window.AbortController ? new AbortController() : null;
    const timer = ctrl ? setTimeout(() => ctrl.abort(), 4000) : null;
    fetch('/data', ctrl ? { signal: ctrl.signal, cache: 'no-store' } : { cache: 'no-store' })
      .then(r => {
        if (timer) clearTimeout(timer);
        if (!r.ok) throw new Error('HTTP ' + r.status);
        return r.json();
      })
      .then(data => {
        isFetchingData = false;
        consecutiveFetchFails = 0;
        connectionOk();
        try {
          lastData = data;
          const uName = data.unitName || 'MedWaste Control 1';
          document.title = uName;
          if (document.getElementById('topBrandTitle')) document.getElementById('topBrandTitle').innerText = uName;
          if (document.getElementById('m_hdr')) document.getElementById('m_hdr').innerText = uName;
          if (document.getElementById('termUnitTitle')) document.getElementById('termUnitTitle').innerText = uName + ' Controller Console';
          if (!initFieldsLoaded) {
            initFieldsLoaded = true;
            if (document.getElementById('nameInpM')) document.getElementById('nameInpM').value = uName;
            if (document.getElementById('schedTime') && data.schedTime) document.getElementById('schedTime').value = data.schedTime;
          }
          if (data.wifi) {
            if (document.getElementById('curWifiSsid')) document.getElementById('curWifiSsid').innerText = data.wifi.ssid || '-';
            if (document.getElementById('curWifiIp')) document.getElementById('curWifiIp').innerText = data.wifi.ip || '-';
            if (document.getElementById('wifiStatusBadge')) {
              document.getElementById('wifiStatusBadge').innerHTML = data.wifi.staConnected 
                ? '<span class="tag active">CONNECTED TO ' + (data.wifi.ssid || 'NETWORK') + '</span>' 
                : '<span class="tag warn">HOTSPOT MODE (AP)</span>';
            }
            if (document.getElementById('pillApSsid')) document.getElementById('pillApSsid').innerText = data.wifi.apSsid || '-';
            if (document.getElementById('pillApPass')) document.getElementById('pillApPass').innerText = 'pass: ' + (data.wifi.apPass || 'medwaste123');
            if (document.getElementById('apPassInput') && !document.getElementById('apPassInput').matches(':focus')) {
              document.getElementById('apPassInput').value = data.wifi.apPass || 'medwaste123';
            }
          }

          // Update Live Stepper Angle & Reference Angle in Manual Override
          if (document.getElementById('unitCurAngleBadge') && data.unitAngle !== undefined) {
            document.getElementById('unitCurAngleBadge').innerText = data.unitAngle + '° (Ref: ' + (data.unitRefAngle !== undefined ? data.unitRefAngle : 90) + '°)';
          }
          if (document.getElementById('invertDirCheck')) {
            document.getElementById('invertDirCheck').checked = !!data.invDir;
          }

          // Unit Status Pill Update (Top Bar)
          const unitPill = document.getElementById('unitStatusPill');
          const unitTxt = document.getElementById('unitStatusTxt');
          if (unitPill && unitTxt) {
            if (data.disposing) {
              unitPill.className = 'status-pill warn';
              unitTxt.innerHTML = '<span class="status-dot-pulse"></span> <span><b>' + uName + ':</b> Disposing</span>';
            } else if (data.manualMode) {
              unitPill.className = 'status-pill warn';
              unitTxt.innerHTML = '<span class="status-dot-pulse"></span> <span><b>' + uName + ':</b> Manual</span>';
            } else {
              unitPill.className = 'status-pill active';
              unitTxt.innerHTML = '<span class="status-dot-pulse"></span> <span><b>' + uName + ':</b> Online (Auto)</span>';
            }
          }

          if (document.getElementById('modeBadge')) {
            document.getElementById('modeBadge').innerHTML = data.manualMode
              ? `<span class="tag warn">${uName}: MANUAL OVERRIDE</span>`
              : `<span class="tag active">${uName}: AUTO MODE</span>`;
          }

          const u_csv = (data.unitUnoStatus && data.unitUnoStatus.indexOf(',') !== -1) ? data.unitUnoStatus.split(',') : null;

          // Render Unit Telemetry Overview
          renderUnitOverview(data, u_csv);

          let alerts = [];
          if (u_csv && u_csv.length > 7 && u_csv[7] === '1') {
            const distTxt = (u_csv.length > 9 && parseInt(u_csv[9]) > 0) ? ` (${u_csv[9]} cm ahead)` : '';
            alerts.push(`🛑 ${uName}: OBSTACLE IN PATH${distTxt}!`);
          }
          if (data.unitTrashLevel === 'Full') {
            alerts.push(`🗑️ ${uName}: TRASH BIN IS FULL (${data.unitTrashCm} cm)!`);
          }

          const abox = document.getElementById('globalAlert');
          const atxt = document.getElementById('globalAlertText');
          if (abox) {
            if (alerts.length > 0) {
              if (atxt) atxt.innerHTML = alerts.join(' &nbsp;&bull;&nbsp; ');
              else abox.innerHTML = alerts.join(' | ');
              abox.style.display = 'block';
            } else {
              abox.style.display = 'none';
            }
          }

          if (document.getElementById('history')) {
            document.getElementById('history').innerHTML = (data.history || '').replace(/\n/g, '<br>') || 'No logs yet.';
          }
          if (document.getElementById('schedInfo')) {
            document.getElementById('schedInfo').innerText = data.schedTime || '--:--';
          }
          if (chart && chart.data && chart.data.datasets) {
            chart.data.datasets[0].data[0] = data.disposalCount || 0;
            chart.update();
          }

          // Re-anchor the live clock to the web server's own time
          srvLocalEpoch = data.localEpoch;
          srvFetchedAt = performance.now();
          srvSynced = data.timeSynced;
          if (!srvSynced && !autoSyncTried) { autoSyncTried = true; syncTime(); }
          renderClock();
          renderCalib(data, u_csv);
          runAlerts(data, u_csv);
        } catch (uiErr) {
          console.error('UI render error:', uiErr);
        }
      })
      .catch(err => {
        if (timer) clearTimeout(timer);
        isFetchingData = false;
        consecutiveFetchFails++;
        if (consecutiveFetchFails >= 4) {
          connectionLost();
        }
      });
  };

  // --- Standalone Unit Telemetry & Disposal Overview Renderer ---
  var renderUnitOverview = function(data, u_csv) {
    const grid = document.getElementById('unitTelemetryGrid');
    if (!grid) return;
    const uName = data.unitName || 'Robot Unit';
    const stageName = DISP_NAMES[data.dispState || 0] || 'Idle';
    const html = `
      <div class="panel">
        <div style="display:flex; justify-content:space-between; align-items:center; margin-bottom:0.75rem; flex-wrap:wrap; gap:0.4rem;">
          <h4 id="m_hdr" style="margin:0; font-size:1.15rem; font-weight:800;">${uName}</h4>
          <span class="tag active"><span style="color:#008035; font-size:1rem;">●</span> ONLINE</span>
        </div>
        <ul class="data-list">
          <li><span class="data-label">Operational State</span> <span class="data-value">${data.manualMode ? '<span class="tag warn">Manual Override</span>' : getStatusTag(u_csv ? u_csv[8] : 'IDLE', u_csv ? (u_csv[7]==='1'?'Yes':'No') : 'No')}</span></li>
          <li><span class="data-label">Motor Speed</span> <span class="data-value">${u_csv ? u_csv[0] : (data.cal ? data.cal.speed : '150')} PWM</span></li>
          <li><span class="data-label">Line Sensors (S1-S5)</span> <span class="data-value">${u_csv ? `[${u_csv[1]}, ${u_csv[2]}, ${u_csv[3]}, ${u_csv[4]}, ${u_csv[5]}]` : 'Waiting for Uno...'}</span></li>
          <li><span class="data-label">Front Ultrasonic</span> <span class="data-value">${(u_csv && u_csv.length > 9) ? (u_csv[9] + ' cm') : '-'}</span></li>
          <li><span class="data-label">Bin Capacity</span> <span class="data-value"><span class="${data.unitTrashLevel==='Full'?'tag alert':data.unitTrashLevel==='Halfway'?'tag warn':'tag active'}">${data.unitTrashLevel} (${data.unitTrashCm}cm)</span></span></li>
          <li><span class="data-label">Stepper Angle</span> <span class="data-value"><span class="tag accent">${data.unitAngle !== undefined ? data.unitAngle : 90}&deg; (Ref: ${data.unitRefAngle !== undefined ? data.unitRefAngle : 90}&deg;)</span></span></li>
          <li><span class="data-label">Lid Status</span> <span class="data-value">${data.unitTrashLevel==='Full' ? '<span class="tag alert">LOCKED (FULL)</span>' : data.disposing ? '<span class="tag warn">DISPOSING</span>' : data.lidOpen ? '<span class="tag active">OPEN</span>' : '<span class="tag">CLOSED</span>'}</span></li>
        </ul>
      </div>
      <div class="panel" style="background:var(--card-pink);">
        <div style="display:flex; justify-content:space-between; align-items:center; margin-bottom:0.75rem; flex-wrap:wrap; gap:0.4rem;">
          <h4 style="margin:0; font-size:1.15rem; font-weight:800;">Disposal &amp; Network Status</h4>
          <span class="tag accent">${data.disposing ? 'CYCLE ACTIVE' : 'STANDBY'}</span>
        </div>
        <ul class="data-list">
          <li><span class="data-label">Disposal Stage</span> <span class="data-value"><span class="tag ${data.disposing ? 'warn' : 'active'}">${stageName}</span></span></li>
          <li><span class="data-label">Completed Disposals</span> <span class="data-value">${data.disposalCount || 0} Cycles</span></li>
          <li><span class="data-label">Daily Auto-Schedule</span> <span class="data-value">${data.schedTime || '--:--'}</span></li>
          <li><span class="data-label">Dump Tilt Cycles</span> <span class="data-value">${data.cal && data.cal.cycles ? data.cal.cycles : 3}x Full Tilts</span></li>
          <li><span class="data-label">Hotspot AP SSID</span> <span class="data-value">${data.wifi && data.wifi.apSsid ? data.wifi.apSsid : '-'}</span></li>
          <li><span class="data-label">Active IP Address</span> <span class="data-value">${data.wifi && data.wifi.ip ? data.wifi.ip : '192.168.4.1'}</span></li>
          <li><span class="data-label">Local mDNS Address</span> <span class="data-value">${data.wifi && data.wifi.mdns ? data.wifi.mdns : '-'}</span></li>
        </ul>
      </div>`;
    grid.innerHTML = html;
  };

  // --- Real-time Dual Serial Monitors Polling (Controller + Uno) ---
  let isFetchingTerms = false;
  var updateTerminals = function() {
    const termSec = document.getElementById('terminals');
    if (!termSec || !termSec.classList.contains('active') || isFetchingTerms) return;
    isFetchingTerms = true;
    fetch('/terminal_data', { cache: 'no-store' }).then(r => r.json()).then(data => {
      isFetchingTerms = false;
      const eBox = document.getElementById('espConsole');
      const uBox = document.getElementById('unoConsole');
      const auto = document.getElementById('termAutoScroll') ? document.getElementById('termAutoScroll').checked : true;
      if (eBox && data.esp) {
        eBox.innerText = data.esp.join('\n') || 'Controller terminal idle.';
        if (auto) eBox.scrollTop = eBox.scrollHeight;
      }
      if (uBox && data.uno) {
        uBox.innerText = data.uno.join('\n') || 'Uno UART stream idle.';
        if (auto) uBox.scrollTop = uBox.scrollHeight;
      }
    }).catch(e => { isFetchingTerms = false; });
  };

  var clearTerminals = function() {
    const e = document.getElementById('espConsole');
    const u = document.getElementById('unoConsole');
    if (e) e.innerText = '';
    if (u) u.innerText = '';
  };

  var sendDirectUnoCmd = function() {
    const inp = document.getElementById('unoCmdInput');
    const cmd = inp.value.trim();
    if (!cmd) return;
    fetch('/send_uno?cmd=' + encodeURIComponent(cmd)).then(r => r.text()).then(t => {
      notify('ok', 'Serial Command Sent', t);
      inp.value = '';
      setTimeout(updateTerminals, 300);
    });
  };

  var sendQuickUno = function(cmd) {
    fetch('/send_uno?cmd=' + encodeURIComponent(cmd)).then(r => r.text()).then(t => {
      notify('info', 'Command Sent to Uno', cmd);
      setTimeout(updateTerminals, 300);
    });
  };

  var clearHistory = function() {
    showModal({
      level: 'warn',
      title: 'Clear Stored History?',
      msg: 'This will erase the disposal log from internal flash memory and reset the disposal count to 0.',
      actions: [{
        label: 'Clear Flash History',
        cls: 'danger',
        fn: () => {
          fetch('/clear_history').then(r => r.text()).then(t => {
            notify('ok', 'History Cleared', t);
            updateData();
          });
        }
      }]
    });
  };

  // --- Real-time clock (no RTC module): Controller keeps time, with instant local fallback ---
  let srvLocalEpoch = 0, srvFetchedAt = 0, srvSynced = false, autoSyncTried = false;
  const DAYS_SHORT = ['Sun','Mon','Tue','Wed','Thu','Fri','Sat'];
  const DAYS = ['Sunday','Monday','Tuesday','Wednesday','Thursday','Friday','Saturday'];
  const MONTHS = ['Jan','Feb','Mar','Apr','May','Jun','Jul','Aug','Sep','Oct','Nov','Dec'];
  const pad = n => String(n).padStart(2, '0');

  var renderClock = function() {
    const tSide = document.getElementById('clockTime');
    const dSide = document.getElementById('clockDate');
    const sSide = document.getElementById('clockSyncTxt');
    const tTop = document.getElementById('topClockTime');
    const dTop = document.getElementById('topClockDate');

    let timeStr = '';
    let fullDateStr = '';
    let shortDateStr = '';

    if (srvSynced && srvLocalEpoch > 0) {
      const secs = srvLocalEpoch + Math.floor((performance.now() - srvFetchedAt) / 1000);
      const d = new Date(secs * 1000);
      timeStr = pad(d.getUTCHours()) + ':' + pad(d.getUTCMinutes()) + ':' + pad(d.getUTCSeconds());
      fullDateStr = DAYS[d.getUTCDay()] + ', ' + MONTHS[d.getUTCMonth()] + ' ' + d.getUTCDate() + ', ' + d.getUTCFullYear();
      shortDateStr = DAYS_SHORT[d.getUTCDay()] + ', ' + MONTHS[d.getUTCMonth()] + ' ' + d.getUTCDate();
      if (sSide) sSide.innerText = 'Synced with Controller';
      checkSchedule();
    } else {
      const now = new Date();
      timeStr = pad(now.getHours()) + ':' + pad(now.getMinutes()) + ':' + pad(now.getSeconds());
      fullDateStr = DAYS[now.getDay()] + ', ' + MONTHS[now.getMonth()] + ' ' + now.getDate() + ', ' + now.getFullYear();
      shortDateStr = DAYS_SHORT[now.getDay()] + ', ' + MONTHS[now.getMonth()] + ' ' + now.getDate();
      if (sSide) sSide.innerText = 'Device clock (syncing...)';
    }

    if (tSide) tSide.innerText = timeStr;
    if (dSide) dSide.innerText = fullDateStr;
    if (tTop) tTop.innerText = timeStr;
    if (dTop) dTop.innerText = shortDateStr;
  };

  var syncTime = function() {
    const now = new Date();
    fetch('/set_time?epoch=' + Math.floor(now.getTime() / 1000) + '&tz=' + (-now.getTimezoneOffset()))
      .then(r => r.text()).then(() => {
        srvLocalEpoch = Math.floor(now.getTime() / 1000) - now.getTimezoneOffset() * 60;
        srvFetchedAt = performance.now();
        srvSynced = true;
        renderClock();
      }).catch(e => {});
  };

  window.onload = function() {
    initChart(); buildBars('cal_m'); loadPrefsUI();
    initNavPreferences();
    initDpadListeners();
    renderClock();
    syncTime();
    updateData();
    setInterval(updateData, 1500);
    setInterval(renderClock, 1000);
    setInterval(syncTime, 30 * 60 * 1000);
    setInterval(updateTerminals, 1500);
  };


  // ================= Alerts & notifications =================
  const alertPrefs = Object.assign({ sound: true, modal: true, browser: false, leadMin: 5 }, JSON.parse(localStorage.getItem('alertPrefs') || '{}'));
  const alertLog = [];
  const modalQueue = [];
  let modalOpen = false, audioCtx = null, lastData = null;
  const DISP_NAMES = ['Idle', 'Heading to disposal station', 'Opening lid first', 'Tilting CCW to 8 o\'clock', 'Holding tilt (dumping)', 'Returning to 12 o\'clock', 'Preparing next tilt', 'Final return to 12 o\'clock', 'Closing lid', 'Returning to base station', 'Manual tilt down', 'Manual tilt up', 'Custom angle tilt'];

  var nowStr = function() {
    if (!srvSynced) return new Date().toLocaleTimeString();
    const d = new Date((srvLocalEpoch + Math.floor((performance.now() - srvFetchedAt) / 1000)) * 1000);
    return pad(d.getUTCHours()) + ':' + pad(d.getUTCMinutes()) + ':' + pad(d.getUTCSeconds());
  };

  var beep = function(level) {
    if (!alertPrefs.sound) return;
    try {
      audioCtx = audioCtx || new (window.AudioContext || window.webkitAudioContext)();
      const o = audioCtx.createOscillator(), g = audioCtx.createGain();
      o.type = 'sine'; o.frequency.value = (level === 'critical') ? 520 : 880;
      g.gain.value = 0.08; o.connect(g); g.connect(audioCtx.destination);
      o.start(); o.stop(audioCtx.currentTime + (level === 'critical' ? 0.5 : 0.18));
    } catch (e) {}
  };

  var toast = function(level, title, msg) {
    const wrap = document.getElementById('toastWrap');
    if (!wrap) return;
    const el = document.createElement('div');
    el.className = 'toast ' + level;
    const icon = level === 'critical' ? '🛑 ' : level === 'warn' ? '⚠️ ' : level === 'ok' ? '✅ ' : 'ℹ️ ';
    el.innerHTML = '<b></b><span></span>';
    el.querySelector('b').innerText = icon + title;
    el.querySelector('span').innerText = msg;
    el.onclick = () => el.remove();
    wrap.appendChild(el);
    setTimeout(() => el.remove(), level === 'critical' ? 12000 : 6000);
    while (wrap.children.length > 4) wrap.firstChild.remove();
  };

  var showModal = function(opts) { modalQueue.push(opts); if (!modalOpen) nextModal(); };
  var nextModal = function() {
    const o = modalQueue.shift();
    if (!o) { modalOpen = false; document.getElementById('modalOverlay').classList.remove('show'); return; }
    modalOpen = true;
    const box = document.getElementById('modalBox');
    box.className = 'modal ' + o.level;
    document.getElementById('modalIcon').innerText = o.level === 'ok' ? '\u2713' : (o.level === 'info' ? 'i' : '!');
    document.getElementById('modalTitle').innerText = o.title;
    document.getElementById('modalMsg').innerText = o.msg;
    const acts = document.getElementById('modalActions');
    acts.innerHTML = '';
    (o.actions || []).forEach(a => {
      const b = document.createElement('button');
      b.className = a.cls || 'primary'; b.innerText = a.label;
      b.onclick = () => { try { a.fn(); } catch (e) {} nextModal(); };
      acts.appendChild(b);
    });
    const d = document.createElement('button');
    d.className = 'accent'; d.innerText = (o.actions && o.actions.length) ? 'Dismiss' : 'OK';
    d.onclick = nextModal;
    acts.appendChild(d);
    document.getElementById('modalOverlay').classList.add('show');
  };

  var renderAlertLog = function() {
    const box = document.getElementById('alertHistory');
    if (!box) return;
    if (!alertLog.length) { box.innerHTML = '<div class="hint">No alerts yet.</div>'; return; }
    box.innerHTML = alertLog.map(a => '<div class="alert-item"><span class="alert-time">' + a.time + '</span><span class="tag ' +
      (a.level === 'critical' ? 'alert' : a.level === 'warn' ? 'warn' : a.level === 'ok' ? 'active' : 'accent') + '">' + a.level + '</span><span><b>' + a.title + '</b> &ndash; ' + a.msg + '</span></div>').join('');
  };

  var notify = function(level, title, msg, opts) {
    opts = opts || {};
    alertLog.unshift({ level: level, title: title, msg: msg, time: nowStr() });
    if (alertLog.length > 50) alertLog.pop();
    renderAlertLog();
    toast(level, title, msg);
    beep(level);
    if (opts.modal && alertPrefs.modal) showModal({ level: level, title: title, msg: msg, actions: opts.actions });
    if (alertPrefs.browser && window.Notification && Notification.permission === 'granted' && (level === 'critical' || level === 'warn' || opts.push)) {
      try { new Notification(title, { body: msg }); } catch (e) {}
    }
  };

  var applyPrefs = function() {
    alertPrefs.modal = document.getElementById('prefModal').checked;
    alertPrefs.sound = document.getElementById('prefSound').checked;
    alertPrefs.leadMin = Math.max(1, parseInt(document.getElementById('prefLead').value) || 5);
    localStorage.setItem('alertPrefs', JSON.stringify(alertPrefs));
  };
  var loadPrefsUI = function() {
    document.getElementById('prefModal').checked = alertPrefs.modal;
    document.getElementById('prefSound').checked = alertPrefs.sound;
    document.getElementById('prefBrowser').checked = alertPrefs.browser && window.Notification && Notification.permission === 'granted';
    document.getElementById('prefLead').value = alertPrefs.leadMin;
    if (!window.Notification) document.getElementById('notifStatus').innerText = 'Browser notifications are not supported here. In-page alerts still work.';
  };
  var toggleBrowserNotif = function(cb) {
    const st = document.getElementById('notifStatus');
    if (!cb.checked) { alertPrefs.browser = false; localStorage.setItem('alertPrefs', JSON.stringify(alertPrefs)); st.innerText = ''; return; }
    if (!window.Notification) { cb.checked = false; st.innerText = 'Not supported by this browser.'; return; }
    Notification.requestPermission().then(p => {
      alertPrefs.browser = (p === 'granted');
      cb.checked = alertPrefs.browser;
      localStorage.setItem('alertPrefs', JSON.stringify(alertPrefs));
      st.innerText = alertPrefs.browser ? 'Enabled.' : 'Blocked. Note: browsers only allow notifications on https or localhost; in-page alerts, sound and pop-ups still work over plain http.';
    });
  };

  // --- connection state ---
  let connLost = false;
  var connectionLost = function() {
    if (connLost) return;
    connLost = true;
    notify('warn', 'Connection delayed', 'Retrying connection to controller...');
  };
  var connectionOk = function() {
    if (!connLost) return;
    connLost = false;
    notify('ok', 'Connected', 'Controller link active.');
  };

  // --- event detection (runs on every telemetry update) ---
  const prev = {};
  var runAlerts = function(data, u_csv) {
    lastData = data;
    const first = !prev.init;
    const obsRange = (data.cal && data.cal.obs) || 30;
    const uName = data.unitName || 'Robot Unit';
    const obs = !!(u_csv && u_csv[7] === '1');
    const dist = u_csv ? parseInt(u_csv[9]) : -1;
    const near = !obs && dist > 0 && dist < obsRange * 1.5;
    if (!first) {
      if (obs && !prev.obs) {
        notify('critical', 'Object in front: ' + uName, 'Obstacle detected ' + (dist > 0 ? dist + ' cm' : '') + ' ahead. The robot has stopped.',
          { modal: true, actions: [{ label: 'Emergency STOP', cls: 'danger', fn: () => sendManualCmd('MAN_STOP') }] });
      } else if (!obs && prev.obs) {
        notify('ok', 'Path clear: ' + uName, 'Obstacle removed.');
      } else if (near && !prev.near) {
        notify('warn', 'Object approaching: ' + uName, 'Something is ' + dist + ' cm ahead.');
      }
      if (data.unitTrashLevel === 'Full' && prev.trash !== 'Full') {
        notify('critical', 'Trash bin FULL: ' + uName, 'Bin is at ' + data.unitTrashCm + ' cm. Empty it soon.',
          { modal: true, actions: [{ label: 'Dispose now', cls: 'warn', fn: triggerDisposal }] });
      } else if (data.unitTrashLevel === 'Halfway' && prev.trash === 'Low') {
        notify('info', 'Bin half full: ' + uName, 'Fill level has reached halfway.');
      }
      if (data.lidOpen && !prev.lid) notify('info', 'Lid opened', 'Person detected - lid opened automatically.');
      if (data.manualMode !== prev.manual) {
        notify(data.manualMode ? 'warn' : 'info', data.manualMode ? 'Manual override ON' : 'Auto mode ON',
          data.manualMode ? 'Automatic routines are paused.' : 'Automatic routines resumed.', { modal: data.manualMode });
      }
      if (data.disposing && !prev.disposing) notify('warn', 'Disposal started', uName + ' is heading to the disposal area.', { push: true });
      if (!data.disposing && prev.disposing) notify('ok', 'Disposal complete', uName + ' is back at the initial station.', { push: true });
      if (data.disposing && data.dispState !== prev.dispState) notify('info', 'Disposal: ' + (DISP_NAMES[data.dispState] || '-'), 'Stage ' + data.dispState + ' of the disposal cycle.');
    }
    prev.obs = obs; prev.near = near; prev.trash = data.unitTrashLevel;
    prev.lid = data.lidOpen; prev.manual = data.manualMode; prev.disposing = data.disposing; prev.dispState = data.dispState;
    prev.init = true;
  };

  // --- scheduled-disposal reminders (uses the server clock) ---
  const schedFired = {};
  var checkSchedule = function() {
    if (!srvSynced || !lastData || !lastData.schedTime) return;
    const d = new Date((srvLocalEpoch + Math.floor((performance.now() - srvFetchedAt) / 1000)) * 1000);
    const hm = lastData.schedTime.split(':').map(Number);
    let diff = (hm[0] * 60 + hm[1]) - (d.getUTCHours() * 60 + d.getUTCMinutes());
    if (diff < 0) diff += 1440;
    const day = d.getUTCFullYear() + '-' + d.getUTCMonth() + '-' + d.getUTCDate() + '|' + lastData.schedTime;
    const lead = Math.max(1, parseInt(alertPrefs.leadMin) || 5);
    const fire = (tag, fn) => { const key = day + '|' + tag; if (!schedFired[key]) { schedFired[key] = true; fn(); } };
    if (diff === 0 && d.getUTCSeconds() < 30) {
      fire('now', () => notify('warn', 'Disposal time!', 'Scheduled disposal (' + lastData.schedTime + ') is starting now.', { modal: true, push: true }));
    } else if (diff === 1) {
      fire('1', () => notify('warn', 'Disposal in 1 minute', 'Keep the path clear. Starts at ' + lastData.schedTime + '.', { modal: true, push: true }));
    } else if (diff === lead && lead > 1) {
      fire('lead', () => notify('info', 'Disposal in ' + lead + ' minutes', 'Scheduled for ' + lastData.schedTime + '.', { push: true }));
    }
  };

  // ================= Calibration tab =================
  let calLoaded = false;
  var buildBars = function(id) {
    const container = document.getElementById(id + '_bars');
    if (!container) return;
    let h = '';
    for (let i = 0; i < 5; i++) {
      h += '<div class="sbar-row"><span>S' + (i + 1) + '</span><div class="sbar-track"><div class="sbar-fill" id="' + id + '_f' + i + '"></div>' +
           '<i class="mk-b" id="' + id + '_b' + i + '"></i><i class="mk-l" id="' + id + '_l' + i + '"></i></div>' +
           '<span class="data-value" id="' + id + '_v' + i + '">-</span></div>';
    }
    container.innerHTML = h;
  };
  var renderBars = function(id, csv) {
    const black = csv ? parseInt(csv[10]) : NaN, line = csv ? parseInt(csv[11]) : NaN;
    for (let i = 0; i < 5; i++) {
      const v = csv ? parseInt(csv[1 + i]) : NaN;
      const f = document.getElementById(id + '_f' + i);
      const vEl = document.getElementById(id + '_v' + i);
      const bEl = document.getElementById(id + '_b' + i);
      const lEl = document.getElementById(id + '_l' + i);
      if (vEl) vEl.innerText = isNaN(v) ? '-' : v;
      if (f) {
        f.style.width = isNaN(v) ? '0%' : (v / 1023 * 100) + '%';
        f.className = 'sbar-fill' + (isNaN(v) ? '' : (v <= black ? ' black' : (v < line ? ' line' : '')));
      }
      if (bEl) bEl.style.left = isNaN(black) ? '-10px' : (black / 1023 * 100) + '%';
      if (lEl) lEl.style.left = isNaN(line) ? '-10px' : (line / 1023 * 100) + '%';
    }
    const actEl = document.getElementById(id + '_active');
    if (actEl) {
      actEl.innerText = csv && csv.length > 13
        ? 'Active on Uno: black \u2264 ' + csv[10] + ', line < ' + csv[11] + ' (black < line, white > line), spin ' + csv[12] + ' ms, person ' + csv[13] + ' cm' + (csv.length > 16 ? ', PID ' + csv[14] + '/' + csv[15] + '/' + csv[16] : '')
        : 'Active on Uno: -';
    }
  };
  var binText = function(cm, depth) {
    if (!(cm > 0)) return '-';
    const pct = Math.max(0, Math.min(100, Math.round((1 - cm / depth) * 100)));
    return cm + ' cm (' + pct + '% full)';
  };
  var renderCalib = function(data, u_csv) {
    const uName = data.unitName || 'Robot Unit';
    if (document.getElementById('cal_m_hdr')) document.getElementById('cal_m_hdr').innerText = uName + ' \u2013 5-Channel Line Sensors';
    renderBars('cal_m', u_csv);
    const depth = (data.cal && data.cal.binDepth) || 40;
    if (document.getElementById('cal_m_dist')) document.getElementById('cal_m_dist').innerText = u_csv ? (u_csv[9] > 0 ? u_csv[9] + ' cm' : 'no echo') : '-';
    if (document.getElementById('cal_m_bin')) document.getElementById('cal_m_bin').innerText = binText(data.unitTrashCm, depth);
    if (!calLoaded && data.cal) { loadCalFields(false); calLoaded = true; }
    if (syncFromUno && u_csv && u_csv.length > 13) {
      document.getElementById('calBlack').value = u_csv[10];
      document.getElementById('calLine').value = u_csv[11];
      syncFromUno = false;
    }
  };
  let syncFromUno = false;
  var loadCalFields = function(force) {
    if (!lastData || !lastData.cal) return;
    const c = lastData.cal;
    const set = (id, v) => { if (document.getElementById(id)) document.getElementById(id).value = v; };
    set('calBlack', c.black); set('calLine', c.line); set('calSpin', c.spin); set('calPerson', c.person);
    set('calDeg', c.dumpDeg); set('calHold', c.hold); set('calLid', c.lid); set('calBin', c.binDepth);
    set('calKp', c.kp); set('calKi', c.ki); set('calKd', c.kd);
    set('calCycles', c.cycles !== undefined ? c.cycles : 3);
    set('speedInp', c.speed); set('obsInp', c.obs); set('trashInp', c.trash);
    if (c.refDeg !== undefined) set('refAngleInput', c.refDeg);
    if (force) notify('info', 'Reloaded', 'Fields now show the values stored in internal Flash ROM.');
  };
  var saveCalibration = function() {
    const g = id => encodeURIComponent(document.getElementById(id).value);
    fetch('/set_calibration?black=' + g('calBlack') + '&line=' + g('calLine') + '&spin=' + g('calSpin') + '&person=' + g('calPerson') +
          '&deg=' + g('calDeg') + '&hold=' + g('calHold') + '&lid=' + g('calLid') + '&bin=' + g('calBin') +
          '&kp=' + g('calKp') + '&ki=' + g('calKi') + '&kd=' + g('calKd') + '&cyc=' + g('calCycles'))
      .then(r => r.text()).then(t => { notify('ok', 'Calibration saved to ROM', t); updateData(); });
  };
  var autoCal = function(mode) {
    const black = mode === 'black';
    showModal({
      level: 'info',
      title: black ? 'Calibrate on BLACK pad' : 'Calibrate on guide LINE',
      msg: black ? 'Make sure the robot is sitting fully on the all-black start/dump pad, then continue.'
                 : 'Make sure the center sensor (S3) is directly over the guide line, then continue.',
      actions: [{ label: 'Calibrate now', cls: 'warn', fn: () => {
        fetch('/calibrate?mode=' + mode).then(r => r.text()).then(t => {
          notify('ok', 'Calibration sent', t);
          setTimeout(() => { syncFromUno = true; updateData(); }, 1500);
        });
      } }]
    });
  };
</script>
</head>
<body>
  <div id="toastWrap" class="toast-wrap"></div>
  <div id="modalOverlay" class="modal-overlay">
    <div class="modal" id="modalBox">
      <div class="modal-icon" id="modalIcon">i</div>
      <h3 id="modalTitle"></h3>
      <p id="modalMsg"></p>
      <div class="modal-actions" id="modalActions"></div>
    </div>
  </div>

  <div id="drawerBackdrop" class="drawer-backdrop" onclick="closeSidebarDrawer()"></div>

  <!-- Top Sticky App Bar: Menu + Brand + Live Clock + Unit Status -->
  <div class="top-app-bar">
    <div class="top-app-left">
      <button id="sidebarToggleBtn" class="menu-toggle-btn" onclick="toggleSidebar()">
        <span>☰</span> Menu
      </button>
      <div class="top-logo">
        <span>+</span> <b id="topBrandTitle">MedWaste Control 1</b>
      </div>
    </div>

    <div class="top-app-right">
      <div id="unitStatusPill" class="status-pill active">
        <span id="unitStatusTxt"><span class="status-dot-pulse"></span> <span><b>Status:</b> Online</span></span>
      </div>
      <div class="top-clock-pill" onclick="syncTime()" title="Click to sync controller clock with this device">
        <span>🕒</span>
        <span id="topClockTime" class="top-clock-time">--:--:--</span>
        <span id="topClockDate" class="top-clock-date">Syncing...</span>
      </div>
    </div>
  </div>

  <!-- Clean Horizontal Tab Bar -->
  <div id="topNavStripWrapper" class="top-nav-strip-wrapper">
    <div class="top-nav-strip" id="topNavStrip">
      <button class="top-pill-btn active" data-target="overview" onclick="showSection('overview')">🏥 Overview</button>
      <button class="top-pill-btn" data-target="manual" onclick="showSection('manual')">🎮 Manual Control</button>
      <button class="top-pill-btn" data-target="calib" onclick="showSection('calib')">🎚️ Calibration</button>
      <button class="top-pill-btn" data-target="terminals" onclick="showSection('terminals')">💻 Terminals</button>
      <button class="top-pill-btn" data-target="analytics" onclick="showSection('analytics')">📊 Analytics</button>
      <button class="top-pill-btn" data-target="config" onclick="showSection('config')">⚙️ Settings</button>
    </div>
  </div>

  <div class="layout">
    <header id="sidebarPanel" class="sidebar">
      <div class="sidebar-header-row">
        <h1><span>+</span> Navigation</h1>
        <button class="sidebar-close-btn" onclick="toggleSidebar()" title="Close / Hide">&times;</button>
      </div>
      <div class="clock-card">
        <div class="clock-time" id="clockTime">--:--:--</div>
        <div class="clock-date" id="clockDate">Loading date...</div>
        <div class="clock-sync">
          <span id="clockSyncTxt">Syncing...</span>
          <button onclick="syncTime()">Sync Now</button>
        </div>
      </div>
      <nav>
        <button class="nav-btn active" data-target="overview" onclick="showSection('overview')">🏥 System Overview</button>
        <button class="nav-btn" data-target="manual" onclick="showSection('manual')">🎮 Manual Control</button>
        <button class="nav-btn" data-target="calib" onclick="showSection('calib')">🎚️ Calibration</button>
        <button class="nav-btn" data-target="terminals" onclick="showSection('terminals')">💻 Serial Terminals</button>
        <button class="nav-btn" data-target="analytics" onclick="showSection('analytics')">📊 Analytics &amp; Logs</button>
        <button class="nav-btn" data-target="config" onclick="showSection('config')">⚙️ Settings &amp; Wi-Fi</button>
      </nav>
    </header>

    <main>
      <!-- High-Visibility Sticky Global Alert Banner -->
      <div id="globalAlert" class="global-alert-banner">
        <div class="global-alert-inner">
          <div class="global-alert-text">
            <span class="global-alert-badge">⚠️ ACTIVE ALERT</span>
            <span id="globalAlertText">System Alert</span>
          </div>
          <div class="global-alert-actions">
            <button onclick="sendManualCmd('MAN_STOP')">🛑 EMERGENCY STOP</button>
            <button onclick="triggerDisposal()">🗑️ DISPOSE NOW</button>
          </div>
        </div>
      </div>

      <!-- 1. SYSTEM OVERVIEW -->
      <div id="overview" class="section active">
        <div style="display:flex; justify-content:space-between; align-items:center; flex-wrap:wrap; gap:0.75rem; margin-bottom:1rem; padding-bottom:0.5rem; border-bottom:3px solid #000;">
          <h2 style="margin:0; padding:0; border:none;">System Overview</h2>
          <div style="display:flex; gap:0.5rem; flex-wrap:wrap; align-items:center;">
            <button class="accent" onclick="triggerDisposal()">🚀 Execute Auto Disposal</button>
            <button class="warn" onclick="sendManualCmd('TEST_TILT_3X')">🔄 Tilt 3x Now</button>
            <button class="danger" onclick="sendManualCmd('MAN_STOP')">🛑 Emergency Stop</button>
          </div>
        </div>

        <div class="panel-grid" id="unitTelemetryGrid">
          <div class="panel">
            <h4 id="m_hdr" style="margin:0 0 0.8rem 0; font-size:1.15rem; font-weight:800;">MedWaste Control 1</h4>
            <ul class="data-list">
              <li><span class="data-label">Operational State</span> <span id="m_state" class="data-value">-</span></li>
              <li><span class="data-label">Motor Speed</span> <span id="m_speed" class="data-value">-</span></li>
              <li><span class="data-label">Line Sensors</span> <span id="m_sensors" class="data-value">-</span></li>
              <li><span class="data-label">Front Ultrasonic</span> <span id="m_obs_dist" class="data-value">-</span></li>
              <li><span class="data-label">Bin Capacity</span> <span id="m_trash" class="data-value">-</span></li>
            </ul>
          </div>
        </div>
      </div>

      <!-- 2. LIVE SERIAL TERMINALS -->
      <div id="terminals" class="section">
        <div style="display:flex; justify-content:space-between; align-items:center; flex-wrap:wrap; gap:0.75rem; margin-bottom:1rem; padding-bottom:0.5rem; border-bottom:3px solid #000;">
          <h2 style="margin:0; padding:0; border:none;">Live Serial Terminals</h2>
          <div style="display:flex; gap:0.6rem; align-items:center;">
            <label class="switch-row" style="margin:0;"><input type="checkbox" id="termAutoScroll" checked> <span>Auto-scroll</span></label>
            <button class="primary" onclick="clearTerminals()">Clear Logs</button>
          </div>
        </div>

        <div class="panel-grid">
          <div class="panel" style="background:#14131a;">
            <div style="display:flex; justify-content:space-between; align-items:center; margin-bottom:0.5rem;">
              <span id="termUnitTitle" style="font-family:var(--font-head); font-weight:700; color:#ff8fc2; font-size:1rem;">Wi-Fi Controller Console</span>
              <span class="tag active" style="font-size:0.7rem;">CONTROLLER</span>
            </div>
            <div id="espConsole" class="terminal-box">Waiting for controller logs...</div>
          </div>

          <div class="panel" style="background:#14131a;">
            <div style="display:flex; justify-content:space-between; align-items:center; margin-bottom:0.5rem;">
              <span style="font-family:var(--font-head); font-weight:700; color:#a8f0ff; font-size:1rem;">Uno Motor Controller UART (9600)</span>
              <span class="tag accent" style="font-size:0.7rem;">UNO SERIAL</span>
            </div>
            <div id="unoConsole" class="terminal-box">Waiting for Uno telemetry packets...</div>
          </div>
        </div>

        <div class="panel" style="margin-top:1rem; background:var(--panel-bg);">
          <div style="display:flex; gap:0.6rem; flex-wrap:wrap; align-items:center; margin-bottom:0.7rem;">
            <input type="text" id="unoCmdInput" placeholder="Send serial command to Uno (e.g. MAN_FWD, MAN_STOP, MOVE_TO_DISPOSAL, CAL_BLACK)" style="flex:1; min-width:200px;">
            <button class="accent" onclick="sendDirectUnoCmd()">Send Command</button>
          </div>
          <div style="display:flex; gap:0.45rem; flex-wrap:wrap;">
            <button class="danger" style="font-size:0.8rem; padding:0.35rem 0.7rem;" onclick="sendQuickUno('MAN_STOP')">STOP</button>
            <button class="primary" style="font-size:0.8rem; padding:0.35rem 0.7rem;" onclick="sendQuickUno('MAN_FWD')">FWD</button>
            <button class="primary" style="font-size:0.8rem; padding:0.35rem 0.7rem;" onclick="sendQuickUno('MAN_BWD')">BWD</button>
            <button class="primary" style="font-size:0.8rem; padding:0.35rem 0.7rem;" onclick="sendQuickUno('MAN_LEFT')">LEFT</button>
            <button class="primary" style="font-size:0.8rem; padding:0.35rem 0.7rem;" onclick="sendQuickUno('MAN_RIGHT')">RIGHT</button>
            <button class="accent" style="font-size:0.8rem; padding:0.35rem 0.7rem;" onclick="sendQuickUno('MODE_AUTO')">AUTO MODE</button>
            <button class="warn" style="font-size:0.8rem; padding:0.35rem 0.7rem;" onclick="sendQuickUno('MOVE_TO_DISPOSAL')">DISPOSE ROUTE</button>
            <button class="primary" style="font-size:0.8rem; padding:0.35rem 0.7rem;" onclick="sendQuickUno('CAL_BLACK')">CAL BLACK</button>
            <button class="primary" style="font-size:0.8rem; padding:0.35rem 0.7rem;" onclick="sendQuickUno('CAL_LINE')">CAL LINE</button>
          </div>
        </div>
      </div>

      <!-- 3. MANUAL OVERRIDE & TELEOPERATION -->
      <div id="manual" class="section">
        <h2>Manual Control &amp; Teleoperation</h2>

        <!-- Mode Control Bar -->
        <div class="panel" style="margin-bottom: 1.1rem; background: var(--card-pink);">
          <div style="display:flex; justify-content:space-between; align-items:center; flex-wrap:wrap; gap:0.8rem;">
            <div style="display:flex; align-items:center; gap:0.5rem; flex-wrap:wrap;">
              <span id="modeBadge"><span class="tag active">AUTO MODE</span></span>
            </div>

            <div style="display:flex; gap:0.5rem; flex-wrap:wrap;">
              <button class="accent" onclick="sendManualCmd('MODE_AUTO')">🤖 AUTO Mode</button>
              <button class="warn" onclick="sendManualCmd('MODE_MANUAL')">🎮 MANUAL Mode</button>
              <button class="primary" onclick="saveAllSettingsRom()">💾 Save ROM</button>
            </div>
          </div>
        </div>

        <div class="panel-grid">
          <!-- Drive Override Card -->
          <div class="panel">
            <div style="display:flex; justify-content:space-between; align-items:center; flex-wrap:wrap; gap:0.5rem; margin-bottom:0.5rem;">
              <h4 style="margin:0; font-size:1.05rem; font-weight:800;">🚗 Drive D-Pad</h4>
              <span class="tag accent">Direct Motor Control</span>
            </div>
            <div class="dpad">
              <div></div>
              <button class="dpad-btn" data-cmd="MAN_FWD">&uarr;<br>FWD</button>
              <div></div>
              <button class="dpad-btn" data-cmd="MAN_LEFT">&larr;<br>LEFT</button>
              <button class="stop-btn" onclick="sendManualCmd('MAN_STOP')">STOP</button>
              <button class="dpad-btn" data-cmd="MAN_RIGHT">&rarr;<br>RIGHT</button>
              <div></div>
              <button class="dpad-btn" data-cmd="MAN_BWD">&darr;<br>BWD</button>
              <div></div>
            </div>
            <div class="hint" style="text-align:center; margin-top:0.35rem; font-weight:700;">Tap to step momentarily &bull; Hold to drive continuously</div>
          </div>

          <!-- Lid & Stepper Tilt Card -->
          <div class="panel">
            <div style="display:flex; justify-content:space-between; align-items:center; flex-wrap:wrap; gap:0.5rem; margin-bottom:0.6rem;">
              <h4 style="margin:0; font-size:1.05rem; font-weight:800;">📐 Lid &amp; Stepper Tilt</h4>
              <span id="unitCurAngleBadge" class="tag accent" style="font-family:var(--font-mono);">90&deg; (Ref: 90&deg;)</span>
            </div>

            <div style="display:flex; flex-direction:column; gap:0.65rem;">
              <div style="display:flex; gap:0.5rem; flex-wrap:wrap;">
                <button class="primary" style="flex:1;" onclick="sendManualCmd('LID_OPEN')">🔓 Open Lid</button>
                <button class="primary" style="flex:1;" onclick="sendManualCmd('LID_CLOSE')">🔒 Close Lid</button>
                <button class="warn" style="flex:1;" onclick="sendManualCmd('TEST_TILT_3X')">🔄 Tilt 3x</button>
              </div>

              <div>
                <div style="display:flex; gap:0.4rem; flex-wrap:wrap; margin-bottom:0.45rem;">
                  <select id="presetTiltSelect" class="unit-dropdown" style="flex:1;" onchange="applyPresetAngle(this.value)">
                    <option value="">-- Quick Predefined Tilt --</option>
                    <option value="90">90&deg; Upright (12 o'clock)</option>
                    <option value="120">120&deg; (11 o'clock)</option>
                    <option value="150">150&deg; (10 o'clock)</option>
                    <option value="180">180&deg; Horizontal (9 o'clock)</option>
                    <option value="210">210&deg; Dump Target (8 o'clock)</option>
                    <option value="240">240&deg; Deep Dump (7 o'clock)</option>
                  </select>
                </div>
                <div style="display:flex; gap:0.4rem; flex-wrap:wrap;">
                  <button class="accent" style="padding:0.4rem 0.65rem; font-size:0.8rem;" onclick="applyPresetAngle(90)">90&deg; (12 o'clock)</button>
                  <button class="primary" style="padding:0.4rem 0.65rem; font-size:0.8rem;" onclick="applyPresetAngle(150)">150&deg; (10 o'clock)</button>
                  <button class="primary" style="padding:0.4rem 0.65rem; font-size:0.8rem;" onclick="applyPresetAngle(180)">180&deg; (9 o'clock)</button>
                  <button class="warn" style="padding:0.4rem 0.65rem; font-size:0.8rem;" onclick="applyPresetAngle(210)">210&deg; Dump (8 o'clock)</button>
                  <button class="danger" style="padding:0.4rem 0.65rem; font-size:0.8rem;" onclick="applyPresetAngle(240)">240&deg; Deep (7 o'clock)</button>
                </div>
              </div>

              <div>
                <label style="display:block; font-size:0.82rem; font-weight:800; margin-bottom:0.25rem;">Custom Angle (0&deg; to 360&deg;):</label>
                <div style="display:flex; gap:0.5rem; align-items:center; flex-wrap:wrap;">
                  <input type="number" id="customTiltInput" value="90" min="0" max="360" step="1" style="max-width:95px;" oninput="document.getElementById('customTiltSlider').value = this.value">
                  <input type="range" id="customTiltSlider" min="0" max="360" step="1" value="90" style="flex:1; min-width:100px;" oninput="document.getElementById('customTiltInput').value = this.value">
                  <button class="accent" onclick="sendCustomTilt()">Apply</button>
                </div>
                <div class="switch-row" style="margin-top:0.6rem;"><input type="checkbox" id="invertDirCheck" onchange="toggleInvertDir(this)"><label for="invertDirCheck">Invert Stepper Direction (Reverse CCW/CW)</label></div>
              </div>
            </div>
          </div>
        </div>

        <!-- Reference Angle Zeroing Card -->
        <div class="panel" style="margin-top: 1.1rem; background: var(--panel-bg);">
          <div style="display:flex; justify-content:space-between; align-items:center; flex-wrap:wrap; gap:0.6rem; margin-bottom:0.5rem;">
            <h4 style="margin:0; font-size:1.05rem; font-weight:800;">🎯 Zero / Calibrate Stepper Reference Angle</h4>
          </div>
          <p class="hint" style="margin:0 0 0.75rem 0;">
            Set the robot's current physical bin position as the reference angle (e.g. <b>90&deg; Upright</b>) without moving the motor, and save to Flash ROM.
          </p>
          <div style="display:flex; gap:0.65rem; align-items:flex-end; flex-wrap:wrap;">
            <div>
              <label style="display:block; font-size:0.8rem; font-weight:800; margin-bottom:0.2rem;">Preset:</label>
              <select id="refAnglePreset" class="unit-dropdown" onchange="syncRefInput(this.value)">
                <option value="90">90&deg; (Upright)</option>
                <option value="0">0&deg; (Flat)</option>
                <option value="-20">-20&deg; (Dump)</option>
                <option value="45">45&deg; (Angled)</option>
              </select>
            </div>
            <div>
              <label style="display:block; font-size:0.8rem; font-weight:800; margin-bottom:0.2rem;">Angle (&deg;):</label>
              <input type="number" id="refAngleInput" value="90" min="-90" max="180" step="1" style="max-width:100px; padding:0.48rem 0.65rem;">
            </div>
            <button class="warn" onclick="setReferenceAngle()">Set Current Position as Reference</button>
          </div>
        </div>
      </div>

      <!-- 4. SENSOR CALIBRATION -->
      <div id="calib" class="section">
        <h2>Sensor Calibration</h2>

        <div class="panel-grid">
          <div class="panel">
            <h4 id="cal_m_hdr" style="margin:0 0 0.5rem 0;">5-Channel Line Sensors</h4>
            <div id="cal_m_bars"></div>
            <div class="legend"><span class="dot" style="background:#1a1a1a"></span>Black Pad (&le; black)<span class="dot" style="background:#2f8fd8"></span>Line (&lt; line)</div>
            <div class="hint" id="cal_m_active">Active on Uno: -</div>
          </div>

          <div class="panel">
            <h4 style="margin:0 0 0.5rem 0;">Ultrasonic Distance, Bin Fill &amp; Auto-Calibrate</h4>
            <ul class="data-list" style="margin-bottom:0.8rem;">
              <li><span class="data-label">Front Obstacle Sensor</span> <span id="cal_m_dist" class="data-value">-</span></li>
              <li><span class="data-label">Waste Bin Fill Level</span> <span id="cal_m_bin" class="data-value">-</span></li>
            </ul>
            <p class="hint" style="margin-top:0;">Place the robot on the surface first, then tap below to sample live IR thresholds.</p>
            <div style="display:flex; gap:0.6rem; flex-wrap:wrap; margin-top:0.65rem;">
              <button class="primary" style="flex:1;" onclick="autoCal('black')">⬛ Calibrate BLACK Pad</button>
              <button class="primary" style="flex:1;" onclick="autoCal('line')">➖ Calibrate Guide LINE</button>
            </div>
          </div>
        </div>

        <h3>Fine-Tune Parameters</h3>
        <div class="panel">
          <div class="cal-grid">
            <div class="control-group"><label>Black-pad threshold (0&ndash;1023)</label><input type="number" id="calBlack" min="0" max="1023"><div class="hint">All 5 sensors &le; this = start/dump pad</div></div>
            <div class="control-group"><label>Line-detect threshold (0&ndash;1023)</label><input type="number" id="calLine" min="0" max="1023"><div class="hint">Below = Line, Above = Floor</div></div>
            <div class="control-group"><label>Spin-around time (ms)</label><input type="number" id="calSpin" min="200" max="10000"><div class="hint">180&deg; turn minimum duration</div></div>
            <div class="control-group"><label>Person detect range (cm)</label><input type="number" id="calPerson" min="5" max="400"><div class="hint">Auto-open lid proximity</div></div>
            <div class="control-group"><label>Dump tilt angle (&deg;)</label><input type="number" id="calDeg" min="-60" max="90"><div class="hint">90&deg; upright, default &minus;20&deg;</div></div>
            <div class="control-group"><label>Tilt hold time (ms)</label><input type="number" id="calHold" min="200" max="30000"><div class="hint">Hold duration when dumping</div></div>
            <div class="control-group"><label>Lid open time (ms)</label><input type="number" id="calLid" min="1000" max="60000"><div class="hint">Auto-close delay</div></div>
            <div class="control-group"><label>Bin depth (cm)</label><input type="number" id="calBin" min="5" max="400"><div class="hint">Empty bin depth for fill %</div></div>
            <div class="control-group"><label>PID &ndash; Kp (steering)</label><input type="number" id="calKp" min="0" max="500" step="0.5"><div class="hint">Turn responsiveness</div></div>
            <div class="control-group"><label>PID &ndash; Ki (integral)</label><input type="number" id="calKi" min="0" max="50" step="0.01"><div class="hint">Drift correction</div></div>
            <div class="control-group"><label>PID &ndash; Kd (damping)</label><input type="number" id="calKd" min="0" max="500" step="0.5"><div class="hint">Smooths oscillation</div></div>
            <div class="control-group"><label>Dump tilt cycles</label><input type="number" id="calCycles" min="1" max="10" value="3"><div class="hint">Full tilts per disposal (default 3)</div></div>
          </div>
          <div style="display:flex; gap:0.6rem; flex-wrap:wrap; margin-top:0.5rem;">
            <button class="accent" onclick="saveCalibration()">💾 Save &amp; Apply Calibration</button>
            <button class="primary" onclick="loadCalFields(true)">🔄 Reload from ROM</button>
          </div>
        </div>
      </div>

      <!-- 5. ANALYTICS & LOGS -->
      <div id="analytics" class="section">
        <h2>Analytics &amp; Logs</h2>

        <div class="panel-grid">
          <div class="panel">
            <h4 style="margin:0 0 0.5rem 0;">Completed Disposal Cycles</h4>
            <canvas id="disposalChart"></canvas>
          </div>

          <div class="panel">
            <div style="display:flex; justify-content:space-between; align-items:center; flex-wrap:wrap; gap:0.5rem; margin-bottom:0.6rem;">
              <h4 style="margin:0;">Disposal Log (Flash ROM)</h4>
              <button class="primary" style="font-size:0.78rem; padding:0.28rem 0.65rem;" onclick="clearHistory()">Clear Log</button>
            </div>
            <div class="log-viewer" id="history">Waiting for data stream...</div>
          </div>
        </div>

        <h3>Recent System Alerts</h3>
        <div class="panel" id="alertHistory" style="max-height:260px; overflow-y:auto;">
          <div class="hint">No alerts triggered in this session yet.</div>
        </div>
      </div>

      <!-- 6. SETTINGS & WI-FI (Responsive Multi-Column Grid) -->
      <div id="config" class="section">
        <h2>Settings &amp; Network Setup</h2>

        <div class="panel-grid">
          <!-- Unit Identification & Schedule -->
          <div class="panel">
            <h4 style="margin:0 0 0.8rem 0; font-size:1.1rem;">🤖 Unit Name &amp; Schedule</h4>
            <div class="control-group">
              <label>Robot Unit Display Name</label>
              <input type="text" id="nameInpM" value="MedWaste Control 1">
            </div>
            <button class="primary" style="width:100%; margin-bottom:1.1rem;" onclick="updateNames()">Save Unit Name</button>

            <hr style="border:2px dashed #000; margin:0.9rem 0;">

            <div class="control-group" style="margin-bottom:0;">
              <label>Auto-Disposal Time: <span id="schedInfo" style="font-weight:800; font-family:var(--font-mono); background:#fff; padding:0.15rem 0.45rem; border:2px solid #000; border-radius:6px;">--:--</span></label>
              <div style="display:flex; gap:0.5rem; margin-top:0.5rem;">
                <input type="time" id="schedTime" style="flex:1;">
                <button class="accent" onclick="setSchedule()">Set Time</button>
              </div>
            </div>
          </div>

          <!-- Hardware Thresholds & ROM -->
          <div class="panel">
            <h4 style="margin:0 0 0.8rem 0; font-size:1.1rem;">⚙️ Hardware Thresholds</h4>
            <div class="control-group">
              <label>Base Motor Speed (PWM 0&ndash;255)</label>
              <input type="number" id="speedInp" value="150" min="0" max="255">
            </div>
            <div class="control-group">
              <label>Obstacle Stop Range (cm)</label>
              <input type="number" id="obsInp" value="30">
            </div>
            <div class="control-group">
              <label>Bin Full Alert Threshold (cm)</label>
              <input type="number" id="trashInp" value="10">
            </div>
            <div style="display:flex; flex-direction:column; gap:0.55rem;">
              <button class="primary" onclick="updateSettings()">Commit Thresholds to ROM</button>
              <button class="accent" onclick="saveAllSettingsRom()">💾 Snapshot All State to ROM</button>
            </div>
          </div>

          <!-- Alert & Notification Settings -->
          <div class="panel">
            <h4 style="margin:0 0 0.8rem 0; font-size:1.1rem;">🔔 Alert Visibility &amp; Sound</h4>
            <div class="switch-row"><input type="checkbox" id="prefModal" onchange="applyPrefs()"><label for="prefModal">Pop-up modal for critical alerts</label></div>
            <div class="switch-row"><input type="checkbox" id="prefSound" onchange="applyPrefs()"><label for="prefSound">Audible alarm beep</label></div>
            <div class="switch-row"><input type="checkbox" id="prefBrowser" onchange="toggleBrowserNotif(this)"><label for="prefBrowser">Browser push notifications</label></div>
            <div class="hint" id="notifStatus"></div>
            <div class="control-group" style="margin-top:0.85rem;">
              <label>Schedule Pre-Warning (minutes)</label>
              <input type="number" id="prefLead" min="1" max="60" onchange="applyPrefs()">
            </div>
            <button class="danger" style="width:100%;" onclick="notify('critical','Test Critical Alert','Obstacle & bin-full alerts will appear prominently like this.',{modal:true})">🚨 Trigger Test Alert</button>
          </div>

          <!-- Wi-Fi & Hotspot Configuration -->
          <div class="panel">
            <h4 style="margin:0 0 0.8rem 0; font-size:1.1rem;">📶 Wi-Fi &amp; Hotspot Setup</h4>
            <div style="display:flex; justify-content:space-between; align-items:center; flex-wrap:wrap; gap:0.4rem; margin-bottom:0.65rem;">
              <span id="wifiStatusBadge"><span class="tag active">Checking...</span></span>
              <span style="font-size:0.8rem; font-family:var(--font-mono);">IP: <b id="curWifiIp">-</b></span>
            </div>
            <div style="font-size:0.82rem; margin-bottom:0.85rem; background:#fff; border:2px solid #000; border-radius:8px; padding:0.45rem 0.65rem;">
              <div><b>Wi-Fi SSID:</b> <span id="curWifiSsid" style="font-family:var(--font-mono);">-</span></div>
              <div><b>Fallback AP:</b> <code id="pillApSsid">MedWaste-1-AP</code> (<span id="pillApPass">pass: medwaste123</span>)</div>
            </div>
            <div style="display:flex; gap:0.45rem; margin-bottom:0.85rem; flex-wrap:wrap;">
              <button class="primary" style="flex:1;" onclick="scanWifi()">🔍 Scan Wi-Fi</button>
              <button class="danger" onclick="resetWifi()">Reset AP</button>
            </div>
            <div id="wifiScanResults" style="display:none; margin-bottom:0.85rem;">
              <div id="wifiList" style="max-height:130px; overflow-y:auto; border:2px solid #000; border-radius:8px; padding:0.4rem; background:#fff;"></div>
            </div>
            <div class="control-group">
              <label>Wi-Fi Name (SSID)</label>
              <input type="text" id="wifiSsidInput" placeholder="Select or type Wi-Fi SSID">
            </div>
            <div class="control-group">
              <label>Wi-Fi Password</label>
              <input type="text" id="wifiPassInput" placeholder="Enter Wi-Fi password">
            </div>
            <div class="control-group">
              <label>Hotspot Password (WPA2, min 8 chars)</label>
              <input type="text" id="apPassInput" value="medwaste123">
            </div>
            <div style="display:flex; gap:0.5rem; flex-wrap:wrap;">
              <button class="accent" style="flex:1;" onclick="saveWifi()">Save &amp; Connect</button>
              <button class="primary" onclick="saveApPassOnly()">Update AP Pass</button>
            </div>
          </div>
        </div>
      </div>

    </main>
  </div>

</body>
</html>
)rawliteral";

String jsonEscape(const String& raw) {
  String out = "";
  out.reserve(raw.length() + 8);
  for (size_t i = 0; i < raw.length(); i++) {
    char c = raw[i];
    if (c == '\\') out += "\\\\";
    else if (c == '\"') out += "\\\"";
    else if (c == '\n') out += "\\n";
    else if (c == '\r') { /* skip CR */ }
    else if ((uint8_t)c >= 32 && (uint8_t)c < 127) out += c;
  }
  return out;
}

void sendMainPage() {
  size_t totalLen = strlen_P(index_html);
  server.setContentLength(totalLen);
  server.send(200, "text/html", "");
  const char* ptr = index_html;
  while (totalLen > 0) {
    size_t chunk = (totalLen > 2048) ? 2048 : totalLen;
    server.sendContent_P(ptr, chunk);
    ptr += chunk;
    totalLen -= chunk;
  }
  server.setContentLength(CONTENT_LENGTH_NOT_SET);
}

void setup() {
  pinMode(DIR_PIN, OUTPUT);
  pinMode(STEP_PIN, OUTPUT);
  pinMode(EN_PIN, OUTPUT);
  digitalWrite(EN_PIN, LOW); // Keep stepper ENABLED (holding torque) immediately on power-up

  // --- AUTOMATIC POWER-ON WARM RESET (Hardware Reset Button Equivalent) ---
  // On cold power-on (battery switch flipped ON), wait 800ms for 5V/3.3V rails & peripheral caps
  // to stabilize, then perform a clean software warm restart (ESP.restart) so Wi-Fi RF and UART
  // initialize 100% reliably without ever needing to press the physical RST button!
  esp_reset_reason_t rstReason = esp_reset_reason();
  if (unit1ColdBootMagic != 0x52535439 && (rstReason == ESP_RST_POWERON || rstReason == ESP_RST_BROWNOUT || rstReason == ESP_RST_UNKNOWN)) {
    unit1ColdBootMagic = 0x52535439;
    delay(800);
    WiFi.persistent(false);
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(200);
    ESP.restart(); // Warm restart (identical to pressing hardware Reset button after power is stable)
  }
  unit1ColdBootMagic = 0;

  Serial.begin(115200);
#if ARDUINO_USB_CDC_ON_BOOT
  Serial0.begin(115200);
#endif
  delay(400);
  loadCalibrationPrefs(); // restore saved calibration
  loadHistoryPrefs();     // restore persistent disposal history and count from Flash ROM
  recomputeTilt();
  pinMode(UNO_RX, INPUT_PULLUP);
  Serial1.begin(9600, SERIAL_8N1, UNO_RX, UNO_TX); // Dedicated Serial to Uno
  Serial1.enableRxInternalPull(true);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  
  lidServo.setPeriodHertz(50);
  lidServo.attach(SERVO_PIN, 500, 2400);
  closeLid();

  logESP("==============================================");
  logESP("  MEDWASTE CONTROL 1 - ESP32-C3 CONTROLLER    ");
  logESP("==============================================");
  logESP("[SYSTEM] Power-On Auto-Reset Verified & Active");
  logESP("[SYSTEM] Running on ESP32-C3 (RISC-V architecture)");
  logESP("[SYSTEM] DRV8825 holding torque ACTIVE (Upright 90 deg)");
  logESP("[SYSTEM] Loaded " + String(disposalCount) + " historical disposals from Flash ROM (NVS)");
  logESP("[SYSTEM] Tilt travel configured: 90 deg to " + String(DUMP_TARGET_DEG, 0) + " deg (" + String(TILT_STEPS) + " steps, " + String(dumpCycles) + " tilt cycles)");

  // Load and Configure WiFi + Password-Protected Hotspot + mDNS Responder
  loadWiFiPrefs();
  WiFi.persistent(false);

  // Set concurrent Access Point + Station mode and disable Wi-Fi power-save sleep for zero-latency AP responses
  WiFi.mode(WIFI_AP_STA);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(false);

  // Always broadcast password-protected Hotspot so it is accessible anytime, even when leaving router range
  bool apStarted = WiFi.softAP(apSSID.c_str(), apPass.c_str());
  if (!apStarted || WiFi.softAPIP() == IPAddress(0, 0, 0, 0)) {
    delay(300);
    WiFi.softAP(apSSID.c_str(), apPass.c_str());
  }
  IPAddress apIP = WiFi.softAPIP(); // default 192.168.4.1
  dnsServer.start(DNS_PORT, mdnsHost + ".local", apIP);
  logESP("[WIFI] Password-Protected Hotspot Active: '" + apSSID + "' (Password: " + apPass + ") @ http://" + apIP.toString());

  if (wifiSSID.length() > 0) {
    logESP("[WIFI] Connecting to configured station network: '" + wifiSSID + "'...");
    WiFi.begin(wifiSSID.c_str(), wifiPass.c_str());
    unsigned long startAttempt = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 8000) {
      delay(250);
      Serial.print(".");
#if ARDUINO_USB_CDC_ON_BOOT
      Serial0.print(".");
#endif
    }
    if (WiFi.status() != WL_CONNECTED) {
      WiFi.disconnect(false); // Stop continuous STA channel hopping so SoftAP stays stable
    }
  }

  if (WiFi.status() == WL_CONNECTED) {
    isAPMode = false;
    logESP("[WIFI] Connected to station network! IP: http://" + WiFi.localIP().toString());
  } else {
    isAPMode = true;
    logESP("[WIFI] Station network unreachable. Running in Fallback Hotspot mode on '" + apSSID + "' (Pass: " + apPass + ") -> Open http://192.168.4.1");
  }

  // Initialize mDNS Responder (enables http://medwaste1.local)
  if (MDNS.begin(mdnsHost.c_str())) {
    MDNS.addService("http", "tcp", 80);
    logESP("[mDNS] Responder active! Access dashboard at: http://" + mdnsHost + ".local");
  } else {
    logESP("[mDNS] Error: MDNS responder could not be started.");
  }

  // Init NTP
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  setenv("TZ", "UTC0", 1); tzset();

  // Web Server Routes (Streamed from Flash PROGMEM in 2KB chunks so RAM never exhausts)
  server.enableDelay(false);
  server.on("/", HTTP_GET, sendMainPage);
  server.on("/index.html", HTTP_GET, sendMainPage);
  server.on("/generate_204", HTTP_GET, []() { server.send(204, "text/plain", ""); });
  server.on("/hotspot-detect.html", HTTP_GET, []() { server.send(204, "text/plain", ""); });
  server.on("/canonical.html", HTTP_GET, []() { server.send(204, "text/plain", ""); });
  server.on("/ncsi.txt", HTTP_GET, []() { server.send(204, "text/plain", ""); });
  server.on("/connecttest.txt", HTTP_GET, []() { server.send(204, "text/plain", ""); });
  server.on("/favicon.ico", HTTP_GET, []() { server.send(204, "text/plain", ""); });
  server.onNotFound([]() { server.send(204, "text/plain", ""); });

  server.on("/wifi_scan", HTTP_GET, []() {
    int n = WiFi.scanNetworks();
    String json = "[";
    for (int i = 0; i < n; i++) {
      if (i > 0) json += ",";
      json += "{\"ssid\":\"" + jsonEscape(WiFi.SSID(i)) + "\",\"rssi\":" + String(WiFi.RSSI(i)) + ",\"open\":" + String(WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "true" : "false") + "}";
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

  // Dual Serial Monitor Terminal Data Stream (Controller + Uno)
  server.on("/terminal_data", HTTP_GET, []() {
    String json = "{\"esp\":[";
    for (int i = 0; i < espLogCount; i++) {
      if (i > 0) json += ",";
      json += "\"" + jsonEscape(espLogs[i]) + "\"";
    }
    json += "],\"uno\":[";
    for (int i = 0; i < unoLogCount; i++) {
      if (i > 0) json += ",";
      json += "\"" + jsonEscape(unoLogs[i]) + "\"";
    }
    json += "]}";
    server.send(200, "application/json", json);
  });

  // Direct Uno Command Sender
  server.on("/send_uno", HTTP_GET, []() {
    if (server.hasArg("cmd")) {
      String cmd = server.arg("cmd");
      sendToUno(cmd);
      server.send(200, "text/plain", "Sent '" + cmd + "' to Uno");
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
    json += "\"unitName\":\"" + jsonEscape(unitName) + "\",";
    json += "\"unitUnoStatus\":\"" + jsonEscape(unitUnoStatus) + "\",";
    json += "\"unitTrashLevel\":\"" + jsonEscape(unitTrashLevel) + "\",";
    json += "\"unitTrashCm\":" + String(unitTrashCm) + ",";
    json += "\"unitAngle\":" + String(currentAngleDeg, 1) + ",";
    json += "\"unitRefAngle\":" + String(UPRIGHT_REF_DEG, 1) + ",";
    json += "\"unitDumpAngle\":" + String(DUMP_TARGET_DEG, 1) + ",";
    json += "\"invDir\":" + String(tiltInvertDir ? "true" : "false") + ",";
    json += "\"manualMode\":" + String(isManualOverride ? "true" : "false") + ",";
    bool staConnected = (WiFi.status() == WL_CONNECTED);
    json += "\"wifi\":{\"ap\":" + String(isAPMode ? "true" : "false") + ",\"staConnected\":" + String(staConnected ? "true" : "false") + ",\"ssid\":\"" + jsonEscape(staConnected ? WiFi.SSID() : apSSID) + "\",\"ip\":\"" + (staConnected ? WiFi.localIP().toString() : WiFi.softAPIP().toString()) + "\",\"apSsid\":\"" + jsonEscape(apSSID) + "\",\"apPass\":\"" + jsonEscape(apPass) + "\",\"apIp\":\"" + WiFi.softAPIP().toString() + "\",\"mdns\":\"http://" + jsonEscape(mdnsHost) + ".local\"},";
    json += "\"timeSynced\":" + String(time(nullptr) > 1700000000 ? "true" : "false") + ",";
    json += "\"localEpoch\":" + String((long)time(nullptr) + tzOffsetSec) + ",";
    json += "\"history\":\"" + jsonEscape(disposalHistory) + "\",";
    json += "\"disposalCount\":" + String(disposalCount) + ",";
    
    char schedBuf[10];
    sprintf(schedBuf, "%02d:%02d", scheduledHour, scheduledMinute);
    json += "\"schedTime\":\"" + String(schedBuf) + "\",";
    json += "\"disposing\":" + String(isDisposing ? "true" : "false") + ",";
    json += "\"dispState\":" + String((int)dispState) + ",";
    json += "\"lidOpen\":" + String(isLidOpen ? "true" : "false") + ",";
    json += "\"cal\":{\"black\":" + String(calBlack) + ",\"line\":" + String(calLine) + ",\"spin\":" + String(calSpin) + ",\"person\":" + String(calPerson) + ",\"dumpDeg\":" + String(DUMP_TARGET_DEG, 1) + ",\"refDeg\":" + String(UPRIGHT_REF_DEG, 1) + ",\"curDeg\":" + String(currentAngleDeg, 1) + ",\"hold\":" + String(holdMs) + ",\"lid\":" + String(lidOpenMs) + ",\"binDepth\":" + String(binDepthCm) + ",\"kp\":" + String(calKp, 2) + ",\"ki\":" + String(calKi, 2) + ",\"kd\":" + String(calKd, 2) + ",\"speed\":" + String(currentSpeed) + ",\"obs\":" + String(obsThreshold) + ",\"trash\":" + String(trashFullThreshold) + ",\"cycles\":" + String(dumpCycles) + "}";
    json += "}";
    server.send(200, "application/json", json);
  });

  server.on("/manual", HTTP_GET, []() {
    if (!server.hasArg("cmd")) {
      server.send(400, "text/plain", "Missing cmd");
      return;
    }
    String cmd = server.arg("cmd");
    logESP("[MANUAL CONTROL] Cmd=" + cmd);

    if (cmd == "MODE_AUTO") {
      isManualOverride = false;
      isDisposing = false;
      dispState = NONE;
      sendToUno("MODE_AUTO");
      saveCalibrationPrefs();
    } else if (cmd == "MODE_MANUAL") {
      isManualOverride = true;
      isDisposing = false;
      dispState = NONE;
      sendToUno("MODE_MANUAL");
      saveCalibrationPrefs();
    } else if (cmd == "MAN_FWD" || cmd == "MAN_BWD" || cmd == "MAN_LEFT" || cmd == "MAN_RIGHT" || cmd == "MAN_STOP") {
      isManualOverride = true;
      isDisposing = false;
      dispState = NONE;
      sendToUno(cmd);
      saveCalibrationPrefs();
    } else if (cmd == "LID_OPEN") {
      if (unitTrashLevel == "Full" || (unitTrashCm > 0 && unitTrashCm <= trashFullThreshold)) {
        logESP("[MANUAL CONTROL] Open lid BLOCKED: Trash bin is FULL (" + String(unitTrashCm) + " cm)!");
        server.send(200, "text/plain", "Blocked: Trash bin is FULL!");
        return;
      } else if (isDisposing || dispState != NONE) {
        logESP("[MANUAL CONTROL] Open lid BLOCKED: Disposal routine active!");
        server.send(200, "text/plain", "Blocked: Disposal routine is active!");
        return;
      } else {
        openLid();
      }
    } else if (cmd == "LID_CLOSE") {
      closeLid();
    } else if (cmd == "TILT_DUMP") {
      isManualOverride = true;
      moveStepperToAngle(DUMP_TARGET_DEG);
      saveCalibrationPrefs();
    } else if (cmd == "TILT_UPRIGHT") {
      isManualOverride = true;
      moveStepperToAngle(UPRIGHT_REF_DEG);
      saveCalibrationPrefs();
    } else if (cmd == "TEST_TILT_3X") {
      isTestTiltOnly = true;
      startDisposalDumping();
      logESP("[STEPPER] Starting 3x Tilt test: Opening lid first, then smooth CCW tilt to 8 o'clock");
    }
    server.send(200, "text/plain", "Executed " + cmd);
  });

  // Stepper CCW/CW Direction Invert Endpoint
  server.on("/invert_dir", HTTP_GET, []() {
    if (server.hasArg("inv")) {
      tiltInvertDir = (server.arg("inv") == "1" || server.arg("inv") == "true");
    } else {
      tiltInvertDir = !tiltInvertDir;
    }
    saveCalibrationPrefs();
    logESP("[STEPPER] Direction invert set to: " + String(tiltInvertDir ? "YES (Inverted)" : "NO (Normal)"));
    server.send(200, "text/plain", tiltInvertDir ? "Inverted" : "Normal");
  });

  // Custom & Predefined Angle Tilt Endpoint
  server.on("/tilt_angle", HTTP_GET, []() {
    if (!server.hasArg("deg")) {
      server.send(400, "text/plain", "Missing deg");
      return;
    }
    float deg = constrain(server.arg("deg").toFloat(), -90.0f, 360.0f);
    isManualOverride = true;
    sendToUno("MODE_MANUAL");
    moveStepperToAngle(deg);
    saveCalibrationPrefs();
    logESP("[TILT ANGLE] Tilting stepper to " + String(deg, 1) + " deg (Saved to Flash ROM)");
    server.send(200, "text/plain", "Tilting stepper to " + String(deg, 1) + "°");
  });

  // Reference Angle Calibration Endpoint (Calibrate current physical position as deg without moving motor)
  server.on("/set_ref_angle", HTTP_GET, []() {
    if (!server.hasArg("deg")) {
      server.send(400, "text/plain", "Missing deg");
      return;
    }
    float deg = constrain(server.arg("deg").toFloat(), -90.0f, 360.0f);
    UPRIGHT_REF_DEG = deg;
    currentAngleDeg = deg;
    recomputeTilt();
    saveCalibrationPrefs();
    logESP("[REF ANGLE CALIBRATED] Current physical stepper position set to " + String(deg, 1) + " deg & saved to Flash ROM");
    server.send(200, "text/plain", "Calibrated current position as " + String(deg, 1) + "° reference & saved to ROM!");
  });

  // Explicit Full Save State Endpoint for Internal Flash ROM
  server.on("/save_all_settings", HTTP_GET, []() {
    saveCalibrationPrefs();
    saveHistoryPrefs();
    logESP("[FLASH ROM] Saved all settings & state to internal Flash ROM!");
    server.send(200, "text/plain", "All settings saved to internal Flash ROM!");
  });

  server.on("/set_schedule", HTTP_GET, []() {
    if(server.hasArg("time")) {
      String t = server.arg("time");
      scheduledHour = t.substring(0, 2).toInt();
      scheduledMinute = t.substring(3, 5).toInt();
      saveCalibrationPrefs();
      logESP("[SCHEDULE] Updated automated disposal time to: " + t + " (Saved to Flash ROM)");
      server.send(200, "text/plain", "Schedule updated to " + t + " and saved to ROM!");
    } else {
      server.send(400, "text/plain", "Missing time");
    }
  });

  server.on("/set_names", HTTP_GET, []() {
    String nm = server.hasArg("name") ? server.arg("name") : (server.hasArg("m") ? server.arg("m") : "");
    if (nm.length() > 0) {
      unitName = nm;
      saveCalibrationPrefs();
      logESP("[CONFIG] Unit name updated to: '" + unitName + "' (Saved to Flash ROM)");
      server.send(200, "text/plain", "Unit name updated to '" + unitName + "' & saved to ROM!");
    } else {
      server.send(400, "text/plain", "Missing name");
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
    logESP("[CALIBRATION] Calibration saved to Flash ROM and synced with Uno!");
    server.send(200, "text/plain", "Calibration saved to Flash ROM and synced with Uno!");
  });

  server.on("/calibrate", HTTP_GET, []() {
    String mode = server.arg("mode");
    if (mode == "black") {
      sendToUno("CAL_BLACK");
      logESP("[CALIBRATION] Triggered auto-calibration on all-black surface");
      server.send(200, "text/plain", "Black-pad calibration sent. Robot must be sitting on the all-black pad.");
    } else if (mode == "line") {
      sendToUno("CAL_LINE");
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
      sendToUno("SET," + String(currentSpeed) + "," + String(obsThreshold));
      logESP("[SETTINGS] Speed=" + String(currentSpeed) + " | Obstacle=" + String(obsThreshold) + "cm | Full=" + String(trashFullThreshold) + "cm (Saved to ROM)");
      server.send(200, "text/plain", "Hardware thresholds saved to Flash ROM!");
    } else {
      server.send(400, "text/plain", "Missing parameters");
    }
  });

  server.on("/dispose", HTTP_GET, []() {
    isManualOverride = false;
    startDisposalRoutine();
    server.send(200, "text/plain", "Disposal cycle initiated!");
  });

  server.begin();
}

void loop() {
  dnsServer.processNextRequest();
  server.handleClient();
  unsigned long currentMillis = millis();

  // Background non-blocking Wi-Fi auto-reconnect if disconnected (paused while user is connected to AP hotspot)
  static unsigned long lastWifiCheck = 0;
  static wl_status_t lastWifiStatus = WL_IDLE_STATUS;
  if (wifiSSID.length() > 0 && (currentMillis - lastWifiCheck >= 25000)) {
    lastWifiCheck = currentMillis;
    wl_status_t curSt = WiFi.status();
    if (curSt != WL_CONNECTED) {
      isAPMode = true;
      if (lastWifiStatus == WL_CONNECTED) {
        logESP("[WIFI] Lost connection to '" + wifiSSID + "'. Fallback Hotspot '" + apSSID + "' remains active.");
      }
      if (WiFi.softAPgetStationNum() == 0) {
        WiFi.begin(wifiSSID.c_str(), wifiPass.c_str());
      }
    } else {
      if (lastWifiStatus != WL_CONNECTED && lastWifiStatus != WL_IDLE_STATUS) {
        isAPMode = false;
        logESP("[WIFI] Reconnected to '" + wifiSSID + "'! Local IP: " + WiFi.localIP().toString());
      }
    }
    lastWifiStatus = curSt;
  }

  // Quiet-window SoftwareSerial command resend to Uno if no ACK yet
  if (pendingUnoRetries > 0 && (currentMillis - pendingUnoCmdTime >= 90)) {
    pendingUnoCmdTime = currentMillis;
    pendingUnoRetries--;
    Serial1.println(pendingUnoCmd);
  }

  // Read from Uno over Dedicated Serial1 (100% non-blocking so WebServer never stalls)
  static String unoRxBuf = "";
  while (Serial1.available() > 0) {
    char c = (char)Serial1.read();
    if (c == '\r') continue;
    if (c == '\n') {
      String unoData = unoRxBuf;
      unoRxBuf = "";
      unoData.trim();
      if (unoData.length() > 0) {
        logUno(unoData);
        if (unoData.startsWith("ACK:")) {
          pendingUnoRetries = 0;
        } else if (unoData.startsWith("STATUS,")) {
          unitUnoStatus = unoData.substring(7); // strip "STATUS,"
          if (!unoSynced) { 
            unoSynced = true; 
            sendCalibrationToUno(); 
            Serial1.println((unitTrashLevel == "Full" || (unitTrashCm > 0 && unitTrashCm <= trashFullThreshold)) ? "BIN_FULL,1" : "BIN_FULL,0");
          }
          // Flush any pending command in the quiet window right after Uno finishes STATUS TX
          if (pendingUnoRetries > 0) {
            Serial1.println(pendingUnoCmd);
            pendingUnoCmdTime = currentMillis;
            pendingUnoRetries--;
          }
        } else if (unoData == "OPEN_LID") {
          if (isDisposing || dispState != NONE) {
            logESP("[UNO EVENT] OPEN_LID ignored: Robot is in disposal mode.");
          } else if (unitTrashLevel == "Full" || (unitTrashCm > 0 && unitTrashCm <= trashFullThreshold)) {
            logESP("[UNO EVENT] Person proximity detected, but bin is FULL (" + String(unitTrashCm) + " cm). Lid locked closed!");
          } else if (!isManualOverride) {
            logESP("[UNO EVENT] Person proximity detected while idle & bin has space! Opening lid.");
            openLid();
          }
        } else if (unoData == "ARRIVED_DISPOSAL") {
          logESP("[UNO EVENT] Disposal Station reached (all sensors BLACK)! Opening lid first, then tilting 3x...");
          isTestTiltOnly = false;
          startDisposalDumping();
        } else if (unoData == "ARRIVED_BASE") {
          logESP("[UNO EVENT] Returned backward to Initial Station (all sensors BLACK)! Parked in IDLE.");
          dispState = NONE;
          isDisposing = false;
        }
      }
    } else if ((uint8_t)c >= 32 && (uint8_t)c < 127 && unoRxBuf.length() < 180) {
      unoRxBuf += c;
    } else if (unoRxBuf.length() >= 180) {
      unoRxBuf = ""; // discard noise overflow
    }
  }

  // Manage Lid Auto-Close Timer (only when NOT disposing and only in Auto mode)
  if (!isDisposing && !isManualOverride && isLidOpen && (currentMillis - lastLidOpenTime >= lidOpenMs)) {
    closeLid();
  }

  // Read Trash Level (every 2s) - paused during active stepping so pulseIn never stalls motor pulses
  bool isActivelyTilting = (dispState == MANUAL_TILT_CUSTOM || dispState == DUMPING_DOWN || 
                           dispState == DUMPING_SHAKE_UP || dispState == DUMPING_UP || 
                           dispState == MANUAL_TILT_DOWN || dispState == MANUAL_TILT_UP);
  if (!isActivelyTilting && (currentMillis - lastSensorReadTime >= 2000)) {
    lastSensorReadTime = currentMillis;
    unitTrashCm = getDistance();
    if (unitTrashCm > 0 && unitTrashCm <= trashFullThreshold) unitTrashLevel = "Full";
    else if (unitTrashCm > trashFullThreshold && unitTrashCm <= trashFullThreshold + 20) unitTrashLevel = "Halfway";
    else unitTrashLevel = "Low";
    
    // Sync Bin Full status to Uno so front person detection is disabled when full
    bool isFull = (unitTrashLevel == "Full" || (unitTrashCm > 0 && unitTrashCm <= trashFullThreshold));
    static bool lastNotifiedFull = false;
    if (isFull != lastNotifiedFull) {
      lastNotifiedFull = isFull;
      sendToUno(isFull ? "BIN_FULL,1" : "BIN_FULL,0");
      if (isFull) {
        logESP("[BIN STATUS] Trash bin is FULL (" + String(unitTrashCm) + " cm <= " + String(trashFullThreshold) + " cm). Front person detection DISABLED.");
        if (isLidOpen) closeLid();
      } else {
        logESP("[BIN STATUS] Trash bin has space (" + String(unitTrashCm) + " cm). Front person detection ENABLED.");
      }
    }

    // Periodic Serial Monitor heartbeat so user always sees AP/WiFi IP & robot status
    static unsigned long lastHeartbeatPrint = 0;
    if (currentMillis - lastHeartbeatPrint >= 6000) {
      lastHeartbeatPrint = currentMillis;
      String hb = "[HEARTBEAT] Hotspot: '" + apSSID + "' (Pass: " + apPass + ") -> http://" + WiFi.softAPIP().toString() +
                  (WiFi.status() == WL_CONNECTED ? (" | Station IP: http://" + WiFi.localIP().toString()) : " | Station: Offline (AP Mode)") +
                  " | Uno: " + unitUnoStatus + " | Trash: " + String(unitTrashCm) + "cm (" + unitTrashLevel + ")";
      Serial.println(hb);
#if ARDUINO_USB_CDC_ON_BOOT
      Serial0.println(hb);
#endif
    }

    // Auto schedule check (only when not in manual override)
    if (!isManualOverride) {
      time_t raw = time(nullptr);
      if (raw > 1700000000) {
        raw += tzOffsetSec;
        struct tm timeinfo;
        gmtime_r(&raw, &timeinfo);
        if (timeinfo.tm_hour == scheduledHour && timeinfo.tm_min == scheduledMinute && timeinfo.tm_sec < 5 && !isDisposing) {
          logESP("[SCHEDULE] Scheduled disposal time triggered!");
          startDisposalRoutine();
        }
      }
    }
  }

  handleDisposalMachine();
}

void openLid(bool isDisposal) {
  if (!isDisposal) {
    if (unitTrashLevel == "Full" || (unitTrashCm > 0 && unitTrashCm <= trashFullThreshold)) {
      logESP("[LID BLOCKED] Cannot open lid: Trash bin is FULL (" + String(unitTrashCm) + " cm). Empty bin or run disposal first!");
      return;
    }
    if (isDisposing || (dispState != NONE && dispState != DUMPING_LID_OPEN)) {
      logESP("[LID BLOCKED] Cannot open lid while disposal cycle is active.");
      return;
    }
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

void setStepperDirection(bool ccw) {
  // If ccw is true (moving from 12 o'clock toward 8 o'clock), drive CCW
  // If tiltInvertDir is false, DIR_PIN is HIGH for CCW. If true, DIR_PIN is LOW.
  bool pinState = ccw ? !tiltInvertDir : tiltInvertDir;
  digitalWrite(DIR_PIN, pinState ? HIGH : LOW);
  delayMicroseconds(10); // DRV8825 requires minimum setup time before step pulse
}

unsigned long lastStepMicros = 0;
unsigned long currentStepIntervalUs = 85000;

bool executeOneStep(int totalSteps) {
  if (totalSteps <= 0) return true;
  unsigned long now = micros();
  if (now - lastStepMicros < currentStepIntervalUs) {
    return false;
  }
  lastStepMicros = now;

  // Trapezoidal smooth acceleration / deceleration ramp for smooth, gradual, careful movement
  const unsigned long CRUISE_INTERVAL_US = 35000; // ~28.5 steps/sec (gentle, high-torque cruise)
  const unsigned long START_INTERVAL_US  = 85000; // ~11.7 steps/sec (gentle, zero-jerk start)
  int rampSteps = totalSteps / 4;
  if (rampSteps < 3) rampSteps = 3;
  if (rampSteps > 15) rampSteps = 15;

  if (stepCount < rampSteps) {
    currentStepIntervalUs = START_INTERVAL_US - ((START_INTERVAL_US - CRUISE_INTERVAL_US) * stepCount / rampSteps);
  } else if (stepCount >= totalSteps - rampSteps) {
    int remaining = totalSteps - stepCount;
    if (remaining < 0) remaining = 0;
    currentStepIntervalUs = START_INTERVAL_US - ((START_INTERVAL_US - CRUISE_INTERVAL_US) * remaining / rampSteps);
  } else {
    currentStepIntervalUs = CRUISE_INTERVAL_US;
  }

  // Generate clean, rock-solid step pulse to DRV8825 (minimum 1.9us required)
  digitalWrite(STEP_PIN, HIGH);
  delayMicroseconds(15);
  digitalWrite(STEP_PIN, LOW);
  stepCount++;
  return true;
}

void moveStepperToAngle(float targetDeg) {
  targetDeg = constrain(targetDeg, -90.0f, 360.0f);
  float deltaDeg = targetDeg - currentAngleDeg;
  int steps = (int)round((fabs(deltaDeg) / 360.0f) * STEPPER_STEPS_PER_REV);
  digitalWrite(EN_PIN, LOW); // Keep holding torque active
  if (steps <= 0) {
    currentAngleDeg = targetDeg;
    dispState = NONE;
    saveCalibrationPrefs();
    return;
  }
  // deltaDeg > 0 means increasing angle (e.g. 90 -> 210, which is CCW)
  setStepperDirection(deltaDeg > 0);
  targetStepsRemaining = steps;
  targetCustomDeg = targetDeg;
  stepCount = 0;
  lastStepMicros = micros();
  currentStepIntervalUs = 85000;
  dispState = MANUAL_TILT_CUSTOM;
}

void startStepperTiltDown(DisposalState mode) {
  recomputeTilt();
  dispState = mode;
  digitalWrite(EN_PIN, LOW);   // Enable Stepper holding torque
  setStepperDirection(true);   // CCW: 12 o'clock (90 deg) -> 8 o'clock (210 deg)
  stepCount = 0;
  lastStepMicros = micros();
  currentStepIntervalUs = 85000;
}

void startStepperTiltUp(DisposalState mode) {
  recomputeTilt();
  dispState = mode;
  digitalWrite(EN_PIN, LOW);   // Enable Stepper holding torque
  setStepperDirection(false);  // CW: 8 o'clock (210 deg) -> 12 o'clock (90 deg)
  stepCount = 0;
  lastStepMicros = micros();
  currentStepIntervalUs = 85000;
}

void startDisposalDumping() {
  recomputeTilt();
  currentDumpCycle = 0;
  dispState = DUMPING_LID_OPEN;
  dispTimer = millis();
  openLid(true); // Open the lid FIRST!
  logESP("[DISPOSAL] 180-deg turn complete. Opening lid first before dumping...");
}

void startDisposalRoutine() {
  isDisposing = true;
  dispState = MOVING;
  sendToUno("MOVE_TO_DISPOSAL");
  dispTimer = millis();
  
  // Record history
  char timeStringBuff[50];
  time_t raw = time(nullptr);
  if (raw > 1700000000) {
    raw += tzOffsetSec;
    struct tm timeinfo;
    gmtime_r(&raw, &timeinfo);
    strftime(timeStringBuff, sizeof(timeStringBuff), "%Y-%m-%d %H:%M:%S", &timeinfo);
  } else {
    strcpy(timeStringBuff, "Synced Cycle");
  }
  disposalCount++;
  disposalHistory = String(timeStringBuff) + " - " + unitName + " Disposal Cycle #" + String(disposalCount) + "\n" + disposalHistory;
  
  // Persist immediately into internal Flash ROM
  saveHistoryPrefs();
  logESP("[DISPOSAL ACTION] Disposal routine initiated (#" + String(disposalCount) + ") and saved to Flash ROM.");
}

void handleDisposalMachine() {
  unsigned long currentMillis = millis();
  
  if (dispState == DUMPING_LID_OPEN) {
    if (currentMillis - dispTimer >= 700) { // Allow servo 700ms to fully open lid
      dispState = DUMPING_DOWN;
      setStepperDirection(true); // CCW: 12 o'clock -> 8 o'clock
      stepCount = 0;
      lastStepMicros = micros();
      currentStepIntervalUs = 85000;
      logESP("[STEPPER] Lid fully open. Executing Tilt #1/" + String(dumpCycles) + " (CCW to 8 o'clock)...");
    }
  } else if (dispState == MANUAL_TILT_CUSTOM) {
    if (executeOneStep(targetStepsRemaining)) {
      if (stepCount >= targetStepsRemaining) {
        currentAngleDeg = targetCustomDeg;
        dispState = NONE;
        saveCalibrationPrefs();
        logESP("[STEPPER] Reached " + String(currentAngleDeg, 1) + " deg smoothly. Holding torque active.");
      }
    }
  } else if (dispState == DUMPING_DOWN || dispState == MANUAL_TILT_DOWN) {
    if (executeOneStep(TILT_STEPS)) {
      if (stepCount >= TILT_STEPS) {
        currentAngleDeg = DUMP_TARGET_DEG;
        saveCalibrationPrefs();
        if (dispState == MANUAL_TILT_DOWN) {
          dispState = NONE;
          logESP("[STEPPER] Manual tilt to " + String(DUMP_TARGET_DEG, 1) + " deg (8 o'clock) complete. Holding.");
        } else {
          dispState = DUMPING_HOLD;
          dispTimer = currentMillis;
          logESP("[STEPPER] Tilt #" + String(currentDumpCycle + 1) + "/" + String(dumpCycles) + " reached " + String(DUMP_TARGET_DEG, 1) + " deg (8 o'clock). Holding for " + String(holdMs) + "ms...");
        }
      }
    }
  } else if (dispState == DUMPING_HOLD) {
    if (currentMillis - dispTimer >= holdMs) {
      currentDumpCycle++;
      if (currentDumpCycle < dumpCycles) {
        dispState = DUMPING_SHAKE_UP;
        setStepperDirection(false); // CW return to 12 o'clock (90 deg)
        stepCount = 0;
        lastStepMicros = micros();
        currentStepIntervalUs = 85000;
        logESP("[STEPPER] Returning to 12 o'clock (90 deg) before Tilt #" + String(currentDumpCycle + 1) + "/" + String(dumpCycles));
      } else {
        dispState = DUMPING_UP;
        setStepperDirection(false); // Final CW return to 12 o'clock (90 deg)
        stepCount = 0;
        lastStepMicros = micros();
        currentStepIntervalUs = 85000;
        logESP("[STEPPER] Completed all " + String(dumpCycles) + " tilts! Returning smoothly to 12 o'clock (90 deg).");
      }
    }
  } else if (dispState == DUMPING_SHAKE_UP) {
    if (executeOneStep(TILT_STEPS)) {
      if (stepCount >= TILT_STEPS) {
        dispState = DUMPING_SHAKE_DOWN;
        dispTimer = currentMillis;
        currentAngleDeg = UPRIGHT_REF_DEG;
        logESP("[STEPPER] Upright at 12 o'clock. Brief pause before next tilt down...");
      }
    }
  } else if (dispState == DUMPING_SHAKE_DOWN) {
    if (currentMillis - dispTimer >= 500) {
      dispState = DUMPING_DOWN;
      setStepperDirection(true); // CCW tilt to 8 o'clock (210 deg)
      stepCount = 0;
      lastStepMicros = micros();
      currentStepIntervalUs = 85000;
      logESP("[STEPPER] Executing Tilt #" + String(currentDumpCycle + 1) + "/" + String(dumpCycles) + " to 8 o'clock...");
    }
  } else if (dispState == DUMPING_UP || dispState == MANUAL_TILT_UP) {
    if (executeOneStep(TILT_STEPS)) {
      if (stepCount >= TILT_STEPS) {
        currentDumpCycle = 0;
        currentAngleDeg = UPRIGHT_REF_DEG;
        saveCalibrationPrefs();
        dispState = DUMPING_LID_CLOSE;
        dispTimer = currentMillis;
        closeLid(); // Close the lid!
        logESP("[STEPPER] All " + String(dumpCycles) + " tilts complete & upright at 12 o'clock. Closing lid...");
      }
    }
  } else if (dispState == DUMPING_LID_CLOSE) {
    if (currentMillis - dispTimer >= 700) { // Allow servo 700ms to fully close lid
      if (dispState == MANUAL_TILT_UP || isTestTiltOnly) {
        isTestTiltOnly = false;
        dispState = NONE;
        logESP("[STEPPER] Upright at 12 o'clock (90 deg) with lid closed. Holding torque locked.");
      } else {
        dispState = RETURNING;
        dispTimer = currentMillis;
        logESP("[STEPPER] Lid closed. Instructing Uno RETURN_TO_BASE.");
        sendToUno("RETURN_TO_BASE");
      }
    }
  }
}
