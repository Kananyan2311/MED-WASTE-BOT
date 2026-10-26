# MedWaste Bot — Autonomous Medical Waste Disposal System

An autonomous, touchless medical waste collection and disposal robot platform. Designed for hospitals, clinics, and laboratories, the system provides touch-free waste deposit, analog line tracking, ultrasonic collision avoidance, automated waste-level monitoring, and motorized multi-tilt biohazard dumping with forward-and-reverse autonomous navigation.

---

## Table of Contents
1. [System Architecture](#system-architecture)
2. [Hardware Components & Pinout Connections](#hardware-components--pinout-connections)
   - [Arduino Uno (Drive & Navigation Controller)](#1-arduino-uno-drive--navigation-controller)
   - [Unit 1 Controller (ESP32-C3)](#2a-unit-1-controller-esp32-c3)
   - [Unit 2 Controller (NodeMCU ESP8266)](#2b-unit-2-controller-nodemcu-esp8266)
3. [Step-by-Step Installation & Flashing Tutorial](#step-by-step-installation--flashing-tutorial)
   - [Prerequisites & Libraries](#prerequisites--libraries)
   - [Flashing the Arduino Uno](#flashing-the-arduino-uno)
   - [Flashing Unit 1 (ESP32-C3)](#flashing-unit-1-esp32-c3)
   - [Flashing Unit 2 (NodeMCU ESP8266)](#flashing-unit-2-nodemcu-esp8266)
4. [Initial Power-On & Wi-Fi Setup](#initial-power-on--wi-fi-setup)
5. [Operating Instructions & Web Dashboard Tour](#operating-instructions--web-dashboard-tour)
   - [Touch-Free Hands-Free Proximity Lid Opening](#touch-free-hands-free-proximity-lid-opening)
   - [Manual Control & Hold-to-Drive D-Pad](#manual-control--hold-to-drive-d-pad)
   - [Automated Disposal Cycle (Forward & Reverse)](#automated-disposal-cycle-forward--reverse)
   - [Stepper Tilt Controls & Direction Invert](#stepper-tilt-controls--direction-invert)
   - [Line Sensor & System Calibration](#line-sensor--system-calibration)
   - [Scheduled Disposal & Analytics](#scheduled-disposal--analytics)
6. [Serial Communication Protocol (Uno ⟷ Controller)](#serial-communication-protocol-uno--controller)
7. [Troubleshooting & FAQ](#troubleshooting--faq)

---

## System Architecture

The robot utilizes a **dual-controller distributed architecture**:
- **Arduino Uno (`uno_motor_control.ino`)**: Handles real-time motor control (TB6612FNG H-bridge), high-speed 5-channel analog line sensor reading with PID line tracking, front obstacle/person detection (HC-SR04), and bi-directional forward/reverse station patrol.
- **Wi-Fi WebServer Controller**:
  - **MedWaste Control 1 (`master_esp32s2_webserver.ino`)**: Runs on **ESP32-C3**.
  - **MedWaste Control 2 (`slave_esp32s2_client.ino`)**: Runs on **NodeMCU ESP8266 (ESP-12E)**.
  - The Controller hosts an interactive, mobile-responsive web dashboard, reads internal waste bin depth (HC-SR04), actuates the touchless lid servo, and drives the DRV8825 stepper motor for controlled gradual bin tilting with S-curve acceleration.
  - Both Unit 1 and Unit 2 run **100% standalone, identical web interfaces** with zero dependency on each other and zero master/slave hierarchy.

```
                     ┌───────────────────────────────────────────────┐
                     │          ESP32-C3 / NodeMCU ESP8266           │
                     │         (MedWaste WebServer Controller)       │
                     └───────┬──────────────────────┬─────────────┬──┘
                             │ UART Serial          │ PWM         │ Step/Dir/EN
                             │ (9600 Baud)          │ (GPIO 6/D6) │ (DRV8825)
                             ▼                      ▼             ▼
       ┌───────────────────────────────┐      ┌───────────┐ ┌─────────────┐
       │          Arduino Uno          │      │ Micro     │ │ NEMA 17     │
       │    (Motor & Navigation)       │      │ Servo     │ │ Stepper     │
       └───────┬───────────────┬───────┘      │ (Bin Lid) │ │ (Bin Tilt)  │
               │ PWM / Dir     │ Analog (A0-A4)└───────────┘ └─────────────┘
               ▼               ▼
      ┌────────────────┐ ┌────────────────┐
      │   TB6612FNG    │ │ 5-Channel Line │
      │  Motor Driver  │ │ Tracking Array │
      └────────────────┘ └────────────────┘
```

---

## Hardware Components & Pinout Connections

### 1. Arduino Uno (Drive & Navigation Controller)

Controls the dual DC gear motors, front collision/person ultrasonic sensor, and 5-channel line tracking sensor array.

#### TB6612FNG Dual DC Motor Driver
| Pin on TB6612FNG | Connects to Arduino Uno | Description |
| :--- | :--- | :--- |
| **PWMA** | **Pin D5** | Left Motor PWM Speed Control (0–255) |
| **AIN1** | **Pin D3** | Left Motor Direction 1 |
| **AIN2** | **Pin D2** | Left Motor Direction 2 |
| **PWMB** | **Pin D6** | Right Motor PWM Speed Control (0–255) |
| **BIN1** | **Pin D4** | Right Motor Direction 1 |
| **BIN2** | **Pin D7** | Right Motor Direction 2 |
| **STBY** | **5V Rail** | Hardwired HIGH to permanently enable driver output |
| **VM** | **Battery (+) (7.4V–12V)** | High-current motor supply rail |
| **VCC** | **Arduino 5V** | Logic voltage supply |
| **GND** | **Arduino GND** | Common Ground with battery and controller |

#### Front Ultrasonic Sensor (Person & Obstacle Detection)
| Pin on HC-SR04 | Connects to Arduino Uno | Description |
| :--- | :--- | :--- |
| **VCC** | **Arduino 5V** | Power (5V) |
| **TRIG** | **Pin D8** | Trigger Pulse Output |
| **ECHO** | **Pin D9** | Echo Pulse Input |
| **GND** | **Arduino GND** | Common Ground |

#### 5-Channel Analog Line Tracking Sensor Array
| Sensor Channel | Connects to Arduino Uno | Position on Chassis |
| :--- | :--- | :--- |
| **OUT 1** | **Analog Pin A0** | Far Left |
| **OUT 2** | **Analog Pin A1** | Mid Left |
| **OUT 3** | **Analog Pin A2** | Center (Guide Line Tracker) |
| **OUT 4** | **Analog Pin A3** | Mid Right |
| **OUT 5** | **Analog Pin A4** | Far Right |
| **VCC** | **Arduino 5V** | Power |
| **GND** | **Arduino GND** | Ground |

*Reading characteristics: Analog reading `< LINE_DETECT_THRESHOLD` (default 500) indicates BLACK line; `> LINE_DETECT_THRESHOLD` indicates WHITE floor. All 5 sensors $\le$ `BLACK_ZERO_THRESHOLD` (default 200) represents an All-Black station zone.*

#### Dedicated UART Communication Link to Controller
| Arduino Uno Pin | Connects to Unit 1 (ESP32-C3) | Connects to Unit 2 (NodeMCU) | Description |
| :--- | :--- | :--- | :--- |
| **Pin D11 (ESP_TX_PIN)** | **GPIO 2 (UNO_RX)** | **Pin D5 / GPIO 14 (UNO_RX)** | Uno Transmit $\rightarrow$ Controller Receive |
| **Pin D10 (ESP_RX_PIN)** | **GPIO 3 (UNO_TX)** | **Pin D7 / GPIO 13 (UNO_TX)** | Controller Transmit $\rightarrow$ Uno Receive |
| **GND** | **GND** | **GND** | **CRITICAL: Shared Common Ground** |

*(Hardware Serial Pins D0 and D1 on the Arduino Uno remain 100% free for USB programming and live PC Serial Monitoring).*

---

### 2A. Unit 1 Controller: ESP32-C3

File: `master_esp32s2_webserver.ino`

| Peripheral | Component Pin | ESP32-C3 GPIO | Notes |
| :--- | :--- | :--- | :--- |
| **Uno UART Link** | Arduino D11 (TX) | **GPIO 2 (UNO_RX)** | Hardware `Serial1` Receive |
| **Uno UART Link** | Arduino D10 (RX) | **GPIO 3 (UNO_TX)** | Hardware `Serial1` Transmit |
| **Top Bin Ultrasonic** | HC-SR04 TRIG | **GPIO 4** | Top depth sensor trigger |
| **Top Bin Ultrasonic** | HC-SR04 ECHO | **GPIO 5** | 3.3V logic (use 1k:2k divider if 5V echo) |
| **DRV8825 Stepper** | DIR | **GPIO 19** | Direction control |
| **DRV8825 Stepper** | STEP | **GPIO 18** | Step pulses |
| **DRV8825 Stepper** | EN | **GPIO 7** | Active LOW (kept LOW for holding torque) |
| **DRV8825 Stepper** | RESET & SLEEP | *Tied together* | Connect RESET to SLEEP to keep driver awake |
| **DRV8825 Stepper** | VMOT & GND | Battery + 100µF Cap | Stepper power rail (8.2V–45V) |
| **Lid Servo** | PWM Signal (Yellow/Orange) | **GPIO 6** | Servo control signal |
| **Lid Servo** | VCC & GND | External 5V/6V BEC | **Do not power servo directly from ESP32 3.3V pin** |

---

### 2B. Unit 2 Controller: NodeMCU ESP8266 (ESP-12E)

File: `slave_esp32s2_client.ino`

Mapped to boot-safe pins across D0–D7 to allow glitch-free boots without disconnecting peripherals.

| Peripheral | Component Pin | NodeMCU Pin | GPIO | Notes |
| :--- | :--- | :--- | :--- | :--- |
| **Uno UART Link** | Arduino D11 (TX) | **Pin D5** | GPIO 14 | `SoftwareSerial` UNO_RX |
| **Uno UART Link** | Arduino D10 (RX) | **Pin D7** | GPIO 13 | `SoftwareSerial` UNO_TX |
| **Top Bin Ultrasonic** | HC-SR04 TRIG | **Pin D1** | GPIO 5 | Top depth sensor trigger |
| **Top Bin Ultrasonic** | HC-SR04 ECHO | **Pin D2** | GPIO 4 | Depth sensor echo (1k:2k divider) |
| **DRV8825 Stepper** | DIR | **Pin D3** | GPIO 0 | Direction control |
| **DRV8825 Stepper** | STEP | **Pin D4** | GPIO 2 | Step pulses |
| **DRV8825 Stepper** | EN | **Pin D0** | GPIO 16 | Active LOW (holding torque) |
| **DRV8825 Stepper** | RESET & SLEEP | *Tied together* | — | Connect RESET to SLEEP |
| **DRV8825 Stepper** | VMOT & GND | Battery + 100µF Cap | — | Stepper power rail (8.2V–45V) |
| **Lid Servo** | PWM Signal | **Pin D6** | GPIO 12 | PWM signal for lid servo |
| **Lid Servo** | VCC & GND | External 5V/6V BEC | — | External 5V/6V power supply |

---

## Step-by-Step Installation & Flashing Tutorial

### Prerequisites & Libraries

Install the latest version of the [Arduino IDE](https://www.arduino.cc/en/software) (version 2.x recommended).

1. **Add Board Manager URLs**:
   Open **File $\rightarrow$ Preferences**, and in *Additional boards manager URLs*, add:
   ```text
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json, http://arduino.esp8266.com/stable/package_esp8266com_index.json
   ```
2. **Install Board Packages** (via **Tools $\rightarrow$ Board $\rightarrow$ Boards Manager**):
   - **esp32** by Espressif Systems (install version 2.0.14 or later)
   - **esp8266** by ESP8266 Community (install version 3.1.2 or later)
   - **Arduino AVR Boards** (built-in, ensure updated)
3. **Install Required Libraries** (via **Tools $\rightarrow$ Manage Libraries**):
   - `ESP32Servo` by Kevin Harrington (for ESP32-C3 servo control)

---

### Flashing the Arduino Uno
1. Open `uno_motor_control.ino` in the Arduino IDE.
2. Under **Tools**, configure:
   - **Board**: `"Arduino Uno"`
   - **Port**: Select the COM port corresponding to your Uno.
3. Click **Upload** ($\rightarrow$).
4. Open the Serial Monitor at **9600 baud** to confirm initialization messages:
   ```text
   [SYSTEM] USB Hardware Serial initialized @ 9600 baud
   [SYSTEM] Dedicated ESP/NodeMCU SoftwareSerial @ D10(RX) & D11(TX)
   [SYSTEM] Ready in IDLE mode.
   ```

---

### Flashing Unit 1 (ESP32-C3)
1. Open `master_esp32s2_webserver.ino` in the Arduino IDE.
2. Under **Tools**, configure:
   - **Board**: `"ESP32C3 Dev Module"`
   - **USB CDC On Boot**: `"Enabled"`
   - **CPU Frequency**: `"160MHz (WiFi)"`
   - **Flash Frequency**: `"80MHz"`
   - **Flash Mode**: `"DIO"` or `"QIO"`
   - **Partition Scheme**: `"Default 4MB with spiffs"`
   - **Port**: Select the COM port for your ESP32-C3.
3. Click **Upload**. (If the board does not auto-enter flash mode, hold the **BOOT** button, tap **RESET**, release **BOOT**, then upload).
4. Open Serial Monitor at **115200 baud**.

---

### Flashing Unit 2 (NodeMCU ESP8266)
1. Open `slave_esp32s2_client.ino` in the Arduino IDE.
2. Under **Tools**, configure:
   - **Board**: `"NodeMCU 1.0 (ESP-12E Module)"`
   - **CPU Frequency**: `"80 MHz"` (or `160 MHz`)
   - **Flash Size**: `"4MB (FS:2MB OTA:~1019KB)"`
   - **Upload Speed**: `"115200"` or `"921600"`
   - **Port**: Select the COM port for your NodeMCU.
3. Click **Upload**.
4. Open Serial Monitor at **115200 baud**.

---

## Initial Power-On & Wi-Fi Setup

Both controllers feature an **Automatic Cold Power-On Warm Reset**: when you flip the battery switch, the microcontroller pauses for ~800ms while the power rails and capacitors stabilize, then triggers a clean internal restart so Wi-Fi and UART initialize reliably without ever pressing a physical button.

### Connecting via Direct Hotspot (Access Point Mode)
Each unit broadcasts its own independent, password-protected fallback Wi-Fi network:

| Unit | Hotspot AP SSID | Default Password | Default IP | Local mDNS URL |
| :--- | :--- | :--- | :--- | :--- |
| **MedWaste Control 1** | `MedWaste-1-AP` | `medwaste123` | `192.168.4.1` | `http://medwaste1.local` |
| **MedWaste Control 2** | `MedWaste-2-AP` | `medwaste123` | `192.168.4.1` | `http://medwaste2.local` |

1. On your smartphone, tablet, or laptop, connect to the Wi-Fi network (`MedWaste-1-AP` or `MedWaste-2-AP`).
2. Open any browser and navigate to **`http://192.168.4.1`** (or **`http://medwaste1.local`** / **`http://medwaste2.local`**).

### Connecting to Facility / Local Wi-Fi
1. On the web dashboard, scroll to **Settings & Network Setup**.
2. Click **🔍 Scan Wi-Fi** to discover surrounding 2.4 GHz networks.
3. Select your facility network, enter the Wi-Fi password, and click **Save & Connect**.
4. The unit will restart and connect to the local router while continuing to broadcast its AP as a backup hotspot. You can now access the dashboard on any device on the hospital network using the mDNS URL or assigned router IP.

---

## Operating Instructions & Web Dashboard Tour

### Touch-Free Hands-Free Proximity Lid Opening
Designed for zero-contact biohazard deposition while parked at the base station:
1. When parked in **IDLE** at the initial station, the front ultrasonic sensor continuously scans ahead.
2. When a healthcare worker steps within **50 cm** (`personThreshold`, customizable in dashboard):
   - The Uno verifies the bin has room (`!isBinFull`) and signals `OPEN_LID`.
   - The controller drives the lid servo to **90° (fully open)**.
3. The lid remains open for **5 seconds** (`lidOpenMs`, customizable), refreshing the timer if the person remains in front.
4. Once the worker steps away, the servo automatically returns to **0° (closed)**.
5. **Full Bin Lockout**: If the bin sensor measures waste $\le 10\text{ cm}$ from the top, the controller sends `BIN_FULL,1`. Proximity opening is locked shut to prevent overflow.

---

### Manual Control & Hold-to-Drive D-Pad
Located in the **🎮 Manual Control** section of the dashboard:
- **Momentary Hold-to-Drive**:
  - Touch and **hold** `FWD`, `BWD`, `LEFT`, or `RIGHT` $\rightarrow$ the robot moves continuously for as long as your finger or mouse is held down.
  - **Release** your finger or mouse $\rightarrow$ the web UI immediately fires `MAN_STOP`, stopping the robot instantly.
  - **Quick Tap** $\rightarrow$ sends a brief momentary step (~200ms) for micro-adjustments.
- **Safety Deadman Watchdog**: If communication drops or the browser is closed while moving, the Arduino Uno automatically cuts motor power after **650ms** (`MANUAL_DEADMAN_TIMEOUT_MS`).
- **Emergency Stop**: The large red **STOP** button provides an immediate override.

---

### Automated Disposal Cycle (Forward & Reverse)

Click **🚀 Execute Auto Disposal** on the dashboard (or trigger via daily schedule or serial command `MOVE_TO_DISPOSAL`):

```
[Initial Base Station]
   │
   ├─► Follows guide line FORWARD (runLineFollowing)
   │
[Disposal Area Sensed (All 5 Sensors = BLACK)]
   │
   ├─► STOPS IMMEDIATELY (No 180° rotation before dumping!)
   │   Uno sends ARRIVED_DISPOSAL to Controller
   │
   ├─► 1. OPEN LID FIRST (Servo swings open to 90°, 700ms stabilization pause)
   │
   ├─► 2. CONTROLLED GRADUAL STEPPER TILT (12 o'clock 90° ──► 8 o'clock 210° CCW)
   │      • DRV8825 EN_PIN held LOW continuously (holding torque never turns off)
   │      • S-curve trapezoidal acceleration ramp (starts at ~12 steps/s, ramps to ~28 steps/s)
   │      • Clean 15µs step pulse (ultrasonic distance polling paused during tilt to prevent jitter)
   │      • Holds at 8 o'clock for slide-out, returns to 12 o'clock, cycles 3 full times
   │      • Smoothly returns upright to 12 o'clock (90°)
   │
   ├─► 3. CLOSE LID (Servo swings closed to 0°, 700ms pause)
   │
   ├─► Controller sends RETURN_TO_BASE to Uno
   │
   ├─► Runs BACKWARD along the line in reverse (runLineFollowingBackward)
   │
[Initial Base Station Sensed (All 5 Sensors = BLACK)]
   │
   └─► STOPS IMMEDIATELY in IDLE
       Facing forward, ready for the next disposal cycle!
```

---

### Stepper Tilt Controls & Direction Invert
- **Upright Holding Torque**: The DRV8825 `EN_PIN` is held permanently `LOW`. The motor actively holds the heavy bin upright at 90° (12 o'clock) with zero sag.
- **Smooth S-Curve Ramp**: Stepping uses a trapezoidal acceleration/deceleration ramp from 85ms start interval down to 35ms cruise interval (~2.9 seconds total travel time for 120°). The bin never jerks, stalls, or drops.
- **Direction Invert Toggle**: Under the Stepper Tilt card in Manual Control, check **Invert Stepper Direction (Reverse CCW/CW)** to invert the motor direction electronically via `/invert_dir` without rewiring physical motor leads.
- **Angle Presets & Range**:
  - `90° Upright (12 o'clock)`
  - `150° (10 o'clock)`
  - `180° Flat / Horizontal (9 o'clock)`
  - `210° Dump Target (8 o'clock)`
  - `240° Deep Dump (7 o'clock)`
  - Custom angle slider & number input supported from `0°` to `360°`.
- **Zero Reference Calibration**: Click **Set Current Position as Reference** to calibrate the current physical angle as 90° upright without moving the motor, saved directly to Flash ROM.

---

### Line Sensor & System Calibration
Located in the **🎚️ Calibration** section:
- **Live 5-Channel Visualizer**: Real-time progress bars showing analog values for sensors S1 through S5 (`A0`–`A4`). Markers indicate the calibrated Black Pad threshold and Line Detect threshold.
- **⬛ Calibrate BLACK Pad**: Place all 5 sensors over the all-black station pad and tap this button. The Uno auto-calculates the threshold (`maxVal + 60`).
- **➖ Calibrate Guide LINE**: Place the center sensor (S3 / A2) on the black line with outer sensors on white floor and tap. The threshold is auto-calculated as `(whiteVal + blackVal) / 2`.
- **Fine-Tune Sliders**:
  - `Black-pad threshold` (0–1023)
  - `Line-detect threshold` (0–1023)
  - `Person detect range` (5–400 cm)
  - `Dump tilt angle` (0–360°, default 210°)
  - `Tilt hold time` (200–30000 ms)
  - `Lid open time` (1000–60000 ms)
  - `PID Gains`: `Kp` (steering), `Ki` (integral drift), `Kd` (damping)
  - `Dump tilt cycles`: Number of full tilts per disposal (default 3)
- Click **💾 Save & Apply Calibration** to commit all values to internal Flash ROM (NVS on ESP32-C3, EEPROM on NodeMCU) and sync with Uno.

---

### Scheduled Disposal & Analytics
- **Daily Auto-Disposal**: Under **⚙️ Settings**, specify the auto-disposal time (e.g. `23:00`). When the synced clock reaches the scheduled minute, the robot executes the disposal routine automatically.
- **Analytics & History**: Tracks total completed disposal cycles and timestamped event records saved in non-volatile memory. Supports log export and clear history.

---

## Serial Communication Protocol (Uno ⟷ Controller)

Communication between the Arduino Uno (SoftwareSerial pins D10/D11) and the Controller (ESP32-C3 GPIO 2/3 or NodeMCU D5/D7) operates at **9600 baud, 8N1** using clean newline-terminated ASCII packets:

### Telemetry Packet (Uno $\rightarrow$ Controller, every 500ms)
```text
STATUS,<speed>,<S0>,<S1>,<S2>,<S3>,<S4>,<motor_state>,<obstacle_detected>,<robot_state>,<distance>
```
*Example:* `STATUS,150,120,480,110,490,510,FORWARD,0,MOVING,85`

### Event Signals
| Direction | Message | Description |
| :--- | :--- | :--- |
| **Uno $\rightarrow$ Controller** | `OPEN_LID` | Person detected in front ($<50\text{ cm}$) while idle; open lid servo |
| **Uno $\rightarrow$ Controller** | `ARRIVED_DISPOSAL` | Sensed all-black pad at disposal station; stop and begin dumping |
| **Uno $\rightarrow$ Controller** | `ARRIVED_BASE` | Sensed all-black pad in reverse at base station; stop in IDLE |
| **Controller $\rightarrow$ Uno** | `MOVE_TO_DISPOSAL` | Start forward line tracking to disposal area |
| **Controller $\rightarrow$ Uno** | `RETURN_TO_BASE` | Start reverse line tracking back to base station |
| **Controller $\rightarrow$ Uno** | `BIN_FULL,1` | Waste level full; suppress proximity lid opening |
| **Controller $\rightarrow$ Uno** | `BIN_FULL,0` | Waste level has room; enable proximity lid opening |
| **Controller $\rightarrow$ Uno** | `SET,<spd>,<obs>` | Set base motor speed (PWM) and obstacle stop range (cm) |
| **Controller $\rightarrow$ Uno** | `CAL,<b>,<l>,<spin>,<p>`| Sync calibration thresholds (black, line, spin time, person range) |
| **Controller $\rightarrow$ Uno** | `PID,<kp>,<ki>,<kd>` | Update line-following PID coefficients |
| **Controller $\rightarrow$ Uno** | `MAN_FWD` / `MAN_STOP` | Manual teleoperation command with hold keep-alive |

---

## Troubleshooting & FAQ

#### 1. Stepper Motor "stalls" or the bin falls during movement
- **Cause**: DRV8825 current limit is set too low, or pulses were too fast without ramping.
- **Solution**: The updated firmware features S-curve acceleration (`CRUISE_INTERVAL_US = 35000`) and keeps `EN_PIN = LOW` continuously. If physical holding torque is still weak, adjust the small potentiometer on the DRV8825 driver with a ceramic screwdriver to increase current limit ($V_{\text{ref}} \approx 0.6\text{V}–0.8\text{V}$ for standard NEMA 17 motors). Ensure the motor power supply is at least **9V–12V** with a 100µF capacitor across `VMOT` and `GND`.

#### 2. Stepper tilts clockwise instead of counter-clockwise
- **Solution**: In the web dashboard under Manual Control, check **Invert Stepper Direction (Reverse CCW/CW)** or visit `http://<ip>/invert_dir`. The setting is saved to Flash ROM.

#### 3. Robot does not open lid when a person approaches
- **Check 1**: Is the robot in **IDLE** mode? Proximity opening is disabled while moving, disposing, or in manual mode.
- **Check 2**: Is the bin full? If the top bin sensor measures $\le 10\text{ cm}$, the lid is locked shut to prevent overflow. Check the bin capacity badge on the dashboard.
- **Check 3**: Check the person detection threshold. If standing more than 50 cm away, increase `Person detect range` under Calibration to 80–100 cm.
- **Check 4**: Verify wiring: Front ultrasonic sensor is connected to **Uno pins D8 (Trig) & D9 (Echo)**; Lid servo is connected to **Controller GPIO 6 / D6**.

#### 4. Controller cannot communicate with Arduino Uno
- Verify dedicated serial wiring:
  - Uno **Pin D11 (TX)** $\rightarrow$ Controller **RX** (`GPIO 2` on ESP32-C3 / `Pin D5` on NodeMCU).
  - Uno **Pin D10 (RX)** $\leftarrow$ Controller **TX** (`GPIO 3` on ESP32-C3 / `Pin D7` on NodeMCU).
  - **Crucial**: Ensure an established **Common Ground (GND)** connection connects the Uno GND and Controller GND.
- Open the web dashboard **💻 Serial Terminals** tab to view real-time Uno UART stream messages.

#### 5. Robot overshoots the line when backing up
- Decrease the base speed in the web dashboard (e.g. set Base Speed to `120` or `130`).
- Ensure line contrast calibration has been performed using **⬛ Calibrate BLACK Pad** and **➖ Calibrate Guide LINE**.

