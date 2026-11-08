# MedWaste Bot — Complete Build, Setup & Flashing Guide

Welcome to the complete, step-by-step assembly, configuration, flashing, and calibration handbook for the **MedWaste Bot** autonomous medical waste disposal platform.

This guide walks you through every phase from scratch: purchasing the right hardware, installing the Arduino IDE, configuring board manager packages and libraries, wiring all components with pinout tables and power distribution schematics, performing precision servo horn zeroing, compiling and uploading the firmware across all three microcontrollers, and conducting end-to-end testing until the robot functions cleanly.

---

## Table of Contents
1. [System Overview & Key Features](#1-system-overview--key-features)
2. [Bill of Materials (BOM) & Tool Checklist](#2-bill-of-materials-bom--tool-checklist)
3. [Software Setup & Arduino IDE Configuration](#3-software-setup--arduino-ide-configuration)
   * [3.1 Installing Arduino IDE 2.x](#31-installing-arduino-ide-2x)
   * [3.2 Installing USB Drivers (CH340 / CP2102 / native CDC)](#32-installing-usb-drivers-ch340--cp2102--native-cdc)
   * [3.3 Adding ESP32 & ESP8266 Board Manager URLs](#33-adding-esp32--esp8266-board-manager-urls)
   * [3.4 Installing ESP32 and ESP8266 Board Packages](#34-installing-esp32-and-esp8266-board-packages)
   * [3.5 Installing Required Libraries (`ESP32Servo`)](#35-installing-required-libraries-esp32servo)
4. [Complete Hardware Pinout & Wiring Schematics](#4-complete-hardware-pinout--wiring-schematics)
   * [4.1 Power Architecture, Batteries & Common Ground](#41-power-architecture-batteries--common-ground)
   * [4.2 Arduino Uno R3 Pinout & Wiring](#42-arduino-uno-r3-pinout--wiring)
   * [4.3 MedWaste Control 1 (ESP32-C3) Pinout & Wiring](#43-medwaste-control-1-esp32-c3-pinout--wiring)
   * [4.4 MedWaste Control 2 (NodeMCU ESP8266) Pinout & Wiring](#44-medwaste-control-2-nodemcu-esp8266-pinout--wiring)
   * [4.5 Ultrasonic 5V to 3.3V Logic Level Dividers](#45-ultrasonic-5v-to-33v-logic-level-dividers)
5. [Mechanical Assembly & Servo Horn Zeroing](#5-mechanical-assembly--servo-horn-zeroing)
   * [5.1 5-Channel IR Line Tracker Mounting Height (8–12 mm)](#51-5-channel-ir-line-tracker-mounting-height-812-mm)
   * [5.2 Front & Top Ultrasonic Placement](#52-front--top-ultrasonic-placement)
   * [5.3 Lid Servo Zeroing & Arm Calibration (0° to 90°)](#53-lid-servo-zeroing--arm-calibration-0-to-90)
   * [5.4 Tilt Servo Zeroing & Arm Calibration (45° to 180°)](#54-tilt-servo-zeroing--arm-calibration-45-to-180)
6. [Firmware Compilation & Flashing Walkthrough](#6-firmware-compilation--flashing-walkthrough)
   * [6.1 Flashing the Arduino Uno (`uno_motor_control.ino`)](#61-flashing-the-arduino-uno-uno_motor_controlino)
   * [6.2 Flashing MedWaste Control 1 (`master_esp32s2_webserver.ino` on ESP32-C3)](#62-flashing-medwaste-control-1-master_esp32s2_webserverino-on-esp32-c3)
   * [6.3 Flashing MedWaste Control 2 (`slave_esp32s2_client.ino` on NodeMCU ESP8266)](#63-flashing-medwaste-control-2-slave_esp32s2_clientino-on-nodemcu-esp8266)
   * [6.4 Synchronizing Sketchbook Copies in Arduino Documents](#64-synchronizing-sketchbook-copies-in-arduino-documents)
7. [Initial Bring-Up & Web Dashboard Provisioning](#7-initial-bring-up--web-dashboard-provisioning)
   * [7.1 Power-On Sequence & Automatic Warm Reset](#71-power-on-sequence--automatic-warm-reset)
   * [7.2 Connecting to Fallback Hotspots (`MedWaste-1-AP` / `MedWaste-2-AP`)](#72-connecting-to-fallback-hotspots-medwaste-1-ap--medwaste-2-ap)
   * [7.3 Provisioning Facility Wi-Fi (Station Mode & mDNS)](#73-provisioning-facility-wi-fi-station-mode--mdns)
8. [End-to-End Calibration & Verification ("Until It Works")](#8-end-to-end-calibration--verification-until-it-works)
   * [Phase 1: Telemetry & UART Communication Link Test](#phase-1-telemetry--uart-communication-link-test)
   * [Phase 2: IR Sensor Array Threshold Calibration (850 Threshold)](#phase-2-ir-sensor-array-threshold-calibration-850-threshold)
   * [Phase 3: Ultrasonic Detection & Hands-Free Lid Opening Test](#phase-3-ultrasonic-detection--hands-free-lid-opening-test)
   * [Phase 4: Servo Dump Motion Validation (Smooth 25 ms/deg)](#phase-4-servo-dump-motion-validation-smooth-25-msdeg)
   * [Phase 5: Manual Drive D-Pad Verification (Tap vs Hold)](#phase-5-manual-drive-d-pad-verification-tap-vs-hold)
   * [Phase 6: Full Autonomous Mission Test (Forward & Reverse Station Return)](#phase-6-full-autonomous-mission-test-forward--reverse-station-return)
9. [Operational Cheat Sheet & Safety Rules](#9-operational-cheat-sheet--safety-rules)

---

## 1. System Overview & Key Features

The **MedWaste Bot** is an autonomous biohazard disposal robot designed for hospital clinics, quarantine wards, and laboratory corridors:

* **Dual-Controller Distributed Architecture**:
  * **Arduino Uno R3**: Dedicated real-time motor controller running at 16 MHz. Handles the TB6612FNG dual DC H-bridge, 5-sensor analog IR tracking array, front collision/person ultrasonic detector, and deadman watchdog timers.
  * **MedWaste WebServer Controller** (ESP32-C3 or NodeMCU ESP8266): Hosts a rich, responsive mobile web dashboard, ultrasonic waste fill monitor, hands-free lid opening servo, and high-torque gradual tilt servo. Both Unit 1 (ESP32-C3) and Unit 2 (NodeMCU) provide identical web dashboards with **zero dependency** on each other.
* **Touchless Waste Deposition**: Hands-free proximity detection triggers the lid servo to swing open for 5 seconds when personnel approach within 50 cm.
* **Discrete 5-Channel Line Following**: High-contrast analog reading ($> 850 = \text{Black}$, $\le 850 = \text{White}$) with automatic recovery memory (`lastDirection`) and all-sensor finish line auto-stop.
* **Controlled Gradual Servo Tilting**: Tilts from $45^\circ$ (upright) to $180^\circ$ (dump) in 1-degree steps at **25 ms/deg** ($\approx 3.4\text{ seconds}$ sweep) to prevent mechanical strain and liquid splashing.
* **Bi-directional Autonomous Run**: Follows the track forward to the disposal station, executes 3 smooth dumping cycles, closes the lid, and follows the track backward in reverse back to base.
* **Robust Fail-Safes**: 650ms serial deadman watchdog, full bin lockout ($\le 10\text{ cm}$), emergency manual stop, and 500ms minimum tap duration to prevent gear train stutter.

---

## 2. Bill of Materials (BOM) & Tool Checklist

### Microcontrollers & Shields
| Qty | Item | Specification | Purpose |
| :---: | :--- | :--- | :--- |
| 1 | **Arduino Uno R3** | ATmega328P, 16 MHz, 5V logic | Motor driving & real-time navigation |
| 1 | **ESP32-C3 SuperMini / Dev Module** | RISC-V 160 MHz, 4MB Flash, Wi-Fi | Unit 1 Web Server & Servo Controller |
| 1 | **NodeMCU ESP8266 (ESP-12E)** | Tensilica L106 80/160 MHz, 4MB Flash | Unit 2 Web Server & Servo Controller |

### Actuators & Motor Drivers
| Qty | Item | Specification | Purpose |
| :---: | :--- | :--- | :--- |
| 1 | **TB6612FNG Dual H-Bridge Driver** | 1.2A continuous / 3.2A peak, MOSFET | Drives Left & Right DC motors efficiently |
| 2 | **TT DC Gearmotors (or N20 Metal Gear)** | 6V–9V, 1:48 gear ratio, with rubber wheels | Robot propulsion |
| 1 | **MG996R or DS3218 Digital Servo** | High-torque metal gear (10–20 kg·cm) | Bin tilting mechanism (45° to 180°) |
| 1 | **SG90 or MG90S Micro Servo** | 9g micro servo | Touchless bin lid opener (0° to 90°) |

### Sensors
| Qty | Item | Specification | Purpose |
| :---: | :--- | :--- | :--- |
| 1 | **5-Channel IR Tracker Array (TCRT5000)** | 5 analog outputs (A0–A4), 5V | Ground line navigation & pad detection |
| 2 | **HC-SR04 Ultrasonic Range Sensors** | 5V supply, 2 cm – 400 cm range | 1x Front obstacle/person + 1x Bin depth |

### Power Supply & Voltage Regulation
| Qty | Item | Specification | Purpose |
| :---: | :--- | :--- | :--- |
| 1 | **2S or 3S Li-ion Battery Pack** | 7.4V (2S) or 11.1V (3S), 2000–3000 mAh | Main system power source |
| 1 | **5V or 6V 3A UBEC / Buck Regulator** | Output 5V/6V @ 3A continuous | Powers servos & logic safely without sag |
| 1 | **SPST Toggle Switch (10A rated)** | Heavy-duty power switch | Main system power switch |
| 1 | **1000 µF 16V Electrolytic Capacitor** | Low ESR | Motor supply rail noise filter |
| 1 | **470 µF 10V Electrolytic Capacitor** | Low ESR | Servo 5V/6V power rail buffer |

### Passives, Resistors & Prototyping
| Qty | Item | Purpose |
| :---: | :--- | :--- |
| 2 | **1 kΩ Resistor (1/4 W)** | Top leg of voltage divider (HC-SR04 Echo to ESP 3.3V) |
| 2 | **2 kΩ (or 2.2 kΩ) Resistor (1/4 W)** | Bottom leg of voltage divider |
| 1 | **Solderless Breadboard or Perfboard** | Common power distribution and logic bus |
| 40 | **Dupont Jumper Wires** (M-M, M-F, F-F) | Inter-board wiring |
| 1 | **Micro-USB and USB-C Cables** | Uploading code to Uno, ESP32-C3, and NodeMCU |

---

## 3. Software Setup & Arduino IDE Configuration

### 3.1 Installing Arduino IDE 2.x
1. Download and install **Arduino IDE 2.3.2** (or latest 2.x) from the official website: [arduino.cc/en/software](https://www.arduino.cc/en/software).
2. Complete the standard installer prompts and allow driver installations when requested.

### 3.2 Installing USB Drivers (CH340 / CP2102 / native CDC)
Depending on your specific microcontroller breakout boards:
* **Arduino Uno clone (CH340G)**: Download and install the WCH CH340 driver.
* **NodeMCU ESP8266 (CP2102 or CH340)**: Ensure Silicon Labs CP210x or WCH CH340 driver is installed.
* **ESP32-C3 (Native USB CDC)**: ESP32-C3 utilizes native USB. No external driver is required on Windows 10/11; when plugged in, it enumerates as `USB Serial Device (COMx)`.

### 3.3 Adding ESP32 & ESP8266 Board Manager URLs
1. Open Arduino IDE.
2. Navigate to **File $\rightarrow$ Preferences** (or press `Ctrl + ,`).
3. Locate the field **Additional boards manager URLs**.
4. Paste the following two URLs separated by a comma:
   ```text
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json, http://arduino.esp8266.com/stable/package_esp8266com_index.json
   ```
5. Click **OK** to save preferences.

### 3.4 Installing ESP32 and ESP8266 Board Packages
1. Open the Boards Manager by clicking the board icon in the left toolbar or choosing **Tools $\rightarrow$ Board $\rightarrow$ Boards Manager...**
2. In the search box, type `esp32`.
   * Find **esp32 by Espressif Systems**.
   * Select version **2.0.14** (or 2.0.17 / 3.0.x) and click **Install**.
3. In the search box, type `esp8266`.
   * Find **esp8266 by ESP8266 Community**.
   * Select version **3.1.2** and click **Install**.
4. Verify that **Arduino AVR Boards** (built-in) is also installed and updated.

### 3.5 Installing Required Libraries (`ESP32Servo`)
1. Open the Library Manager by clicking the books icon on the left toolbar or choosing **Tools $\rightarrow$ Manage Libraries...** (or `Ctrl + Shift + I`).
2. In the search box, type `ESP32Servo`.
3. Locate **ESP32Servo by Kevin Harrington**.
4. Click **Install**.
   > [!NOTE]
   > The standard `Servo.h` library is built-in for the Arduino Uno and NodeMCU ESP8266. Only the ESP32-C3 requires `ESP32Servo.h`.

---

## 4. Complete Hardware Pinout & Wiring Schematics

### 4.1 Power Architecture, Batteries & Common Ground

> [!CAUTION]
> **COMMON GROUND IS MANDATORY**: Every microcontroller (Uno, ESP32, NodeMCU), the TB6612FNG driver, the UBEC regulator, and the battery pack **MUST** connect their GND pins to a single shared Ground Bus. Without a common ground reference, serial communication packets will corrupt and servos will twitch uncontrollably.

```
       [ 7.4V – 11.1V Li-ion Battery Pack ]
                 │                │
                 │ (+)            │ (-)
                 ▼                ▼
        [ Main SPST Switch ]   [ Common Ground Bus (GND) ]
                 │                      │
        ┌────────┴────────┐             │
        │                 │             │
        ▼                 ▼             │
[ UBEC Step-Down ]   [ TB6612FNG VM ]   │
(Outputs 5V/6V 3A)   (Motor Supply)     │
        │                 │             │
   ┌────┴────┐            │             │
   │         │            │             │
   ▼         ▼            ▼             ▼
Servos   Uno / ESP     DC Motors   All GND Pins Tied Together!
```

* **DC Motor High-Current Supply**: Battery positive connects through the main switch directly to **VM** on the TB6612FNG driver.
* **Servo High-Current Supply**: Battery positive connects through the switch into the **UBEC (5V/6V 3A)**. The UBEC output supplies the positive red wires of the **Tilt Servo** and **Lid Servo**.
* **Logic Supply**: Uno 5V or UBEC 5V powers sensor VCC pins.
* **Decoupling Capacitors**: Solder a 1000 µF capacitor across TB6612FNG `VM` and `GND`, and a 470 µF capacitor across the servo `5V` and `GND` rails to absorb inductive current spikes.

---

### 4.2 Arduino Uno R3 Pinout & Wiring

The Arduino Uno runs [`uno_motor_control.ino`](file:///c:/Users/Kananyan/Desktop/MED_WASTE_BOT/uno_motor_control.ino).

| Uno Pin | Peripheral Device | Connected Pin | Signal Description |
| :--- | :--- | :--- | :--- |
| **D2** | TB6612FNG | **AIN2** | Left Motor Direction 2 |
| **D3** | TB6612FNG | **AIN1** | Left Motor Direction 1 |
| **D5** | TB6612FNG | **PWMA** | Left Motor Speed (PWM 0–255) |
| **D4** | TB6612FNG | **BIN1** | Right Motor Direction 1 |
| **D7** | TB6612FNG | **BIN2** | Right Motor Direction 2 |
| **D6** | TB6612FNG | **PWMB** | Right Motor Speed (PWM 0–255) |
| **5V Rail**| TB6612FNG | **STBY** | Driver Standby pin (**Must be tied to 5V**) |
| **D8** | Front Ultrasonic (HC-SR04) | **TRIG** | Obstacle / Person ping trigger |
| **D9** | Front Ultrasonic (HC-SR04) | **ECHO** | Obstacle / Person ping echo |
| **D10** | Controller UART Link | **UNO_TX (ESP_RX)** | SoftwareSerial RX (Receives commands from ESP) |
| **D11** | Controller UART Link | **UNO_RX (ESP_TX)** | SoftwareSerial TX (Sends telemetry to ESP) |
| **A0** | 5-Ch IR Sensor Array | **OUT 1** | Far Left line sensor (analog 0–1023) |
| **A1** | 5-Ch IR Sensor Array | **OUT 2** | Mid Left line sensor (analog 0–1023) |
| **A2** | 5-Ch IR Sensor Array | **OUT 3** | Center guide line tracker (analog 0–1023) |
| **A3** | 5-Ch IR Sensor Array | **OUT 4** | Mid Right line sensor (analog 0–1023) |
| **A4** | 5-Ch IR Sensor Array | **OUT 5** | Far Right line sensor (analog 0–1023) |
| **D16 (A2)\*** | Optional Start Button | Pin 1 | Push-to-ground manual start toggle |
| **D17 (A3)\*** | Optional Run LED | Anode | Active HIGH when robot is tracking |
| **D18 (A4)\*** | Optional Stop LED | Anode | Active HIGH when robot is stopped |
| **5V** | Power Rail | VCC | Powers 5V sensors and TB6612FNG VCC |
| **GND** | Ground Rail | GND | Common Ground Bus |

*\*Note: When using the dedicated web dashboard, manual buttons and indicator LEDs are optional; all states are displayed live in the web interface.*

---

### 4.3 MedWaste Control 1 (ESP32-C3) Pinout & Wiring

MedWaste Control 1 runs [`master_esp32s2_webserver.ino`](file:///c:/Users/Kananyan/Desktop/MED_WASTE_BOT/master_esp32s2_webserver.ino).

| ESP32-C3 GPIO | Peripheral Device | Connected Pin | Signal Description |
| :--- | :--- | :--- | :--- |
| **GPIO 2** | Arduino Uno | **Pin D11 (TX)** | Hardware `Serial1` RX (Receives Uno telemetry) |
| **GPIO 3** | Arduino Uno | **Pin D10 (RX)** | Hardware `Serial1` TX (Sends commands to Uno) |
| **GPIO 4** | Top Depth HC-SR04 | **TRIG** | Bin level depth trigger pulse |
| **GPIO 5** | Top Depth HC-SR04 | **ECHO (via divider)** | Bin level echo input (Must use 3.3V divider) |
| **GPIO 6** | Micro Servo (Lid) | **Signal (Orange/White)**| Lid servo PWM signal (0° Closed, 90° Open) |
| **GPIO 19** | Metal Gear Servo (Tilt) | **Signal (Orange/White)**| **Tilt servo PWM signal** (45° Upright to 180° Dump) |
| **GPIO 8** | Optional Status LED | Anode | Webserver heartbeat / Wi-Fi indicator |
| **5V / VIN** | UBEC 5V Rail | Output (+) | Powers ESP32-C3 onboard 3.3V LDO |
| **GND** | Ground Rail | GND | Common Ground Bus |

---

### 4.4 MedWaste Control 2 (NodeMCU ESP8266) Pinout & Wiring

MedWaste Control 2 runs [`slave_esp32s2_client.ino`](file:///c:/Users/Kananyan/Desktop/MED_WASTE_BOT/slave_esp32s2_client.ino). Pins are mapped to boot-safe pins across D0–D7 to allow glitch-free boots without disconnecting peripherals.

| NodeMCU Pin | GPIO | Peripheral Device | Connected Pin | Signal Description |
| :--- | :---: | :--- | :--- | :--- |
| **Pin D5** | GPIO 14 | Arduino Uno | **Pin D11 (TX)** | `SoftwareSerial` RX (Receives Uno telemetry) |
| **Pin D7** | GPIO 13 | Arduino Uno | **Pin D10 (RX)** | `SoftwareSerial` TX (Sends commands to Uno) |
| **Pin D1** | GPIO 5 | Top Depth HC-SR04 | **TRIG** | Bin depth ultrasonic trigger |
| **Pin D2** | GPIO 4 | Top Depth HC-SR04 | **ECHO (via divider)**| Bin depth ultrasonic echo (3.3V divider) |
| **Pin D6** | GPIO 12 | Micro Servo (Lid) | **Signal (Orange/White)**| Lid servo PWM signal (0° Closed, 90° Open) |
| **Pin D3** | GPIO 0 | Metal Gear Servo (Tilt)| **Signal (Orange/White)**| **Tilt servo PWM signal** (45° Upright to 180° Dump) |
| **VIN** | — | UBEC 5V Rail | Output (+) | Powers NodeMCU onboard 3.3V regulator |
| **GND** | — | Ground Rail | GND | Common Ground Bus |

---

### 4.5 Ultrasonic 5V to 3.3V Logic Level Dividers

The HC-SR04 Echo pin outputs a **5V digital pulse**. Feeding 5V into the 3.3V-tolerant GPIOs of the ESP32-C3 or ESP8266 can cause latch-up, heating, or permanent damage.

Use a simple two-resistor voltage divider on the **ECHO** pin:

```
HC-SR04 ECHO (5V Out) ─────[ 1 kΩ ]─────┬─────► ESP GPIO (3.3V In: GPIO 5 on C3 / D2 on ESP8266)
                                        │
                                     [ 2 kΩ ]
                                        │
                                       GND
```

* Formula: $V_{\text{out}} = 5\text{V} \times \frac{2\text{k}\Omega}{1\text{k}\Omega + 2\text{k}\Omega} = 3.33\text{V}$ (safe for 3.3V inputs).

---

## 5. Mechanical Assembly & Servo Horn Zeroing

### 5.1 5-Channel IR Line Tracker Mounting Height (8–12 mm)
* Mount the 5-channel TCRT5000 IR sensor array on the front underside of the chassis.
* **Optimal Ground Clearance**: **8 mm to 12 mm** above the floor surface.
  * If mounted $> 15\text{ mm}$, the reflected infrared light scatters and threshold contrast drops.
  * If mounted $< 5\text{ mm}$, the sensors may drag on carpet seams or track tape transitions.
* Ensure the center sensor (**S3 / A2**) aligns directly along the robot's geometric centerline.

### 5.2 Front & Top Ultrasonic Placement
* **Front HC-SR04 (Obstacle & Person Detection)**:
  * Mount horizontally on the front bumper, pointing straight forward.
  * Mount at least **6 cm above the floor** so ground reflections do not trigger false obstacles.
* **Top HC-SR04 (Bin Depth Monitor)**:
  * Mount inside the top lid or container rim pointing straight down into the bottom of the waste compartment.
  * Ensure the ultrasonic transducer cone is unobstructed by bin walls.

---

### 5.3 Lid Servo Zeroing & Arm Calibration (0° to 90°)

> [!IMPORTANT]
> **NEVER ATTACH SERVO HORNS BEFORE POWERING AND ZEROING THE SERVO**. Attaching the horn at an unknown position can drive the servo into its mechanical endstop, stripping gears or drawing 2A stall current.

1. Leave the servo horn **detached** from the micro servo spline.
2. Power on the Controller (ESP32-C3 or NodeMCU).
3. The firmware initializes the lid servo to **0° (Closed Position)**.
4. With the servo powered and holding torque at 0°, manually position the bin lid fully shut.
5. Push the servo arm onto the spline so it holds the lid tightly closed at 0°.
6. Secure the servo horn with the center screw.
7. Open the web dashboard and click **Open Lid** (or trigger proximity). The servo will rotate smoothly to **90°**, opening the lid.

---

### 5.4 Tilt Servo Zeroing & Arm Calibration (45° to 180°)

The waste bin dumping mechanism utilizes a **135° total stroke span**:
* **Upright Standby Position**: **45°**
* **Discharging Dump Position**: **180°**

#### Zeroing Procedure:
1. Ensure the servo horn is **disconnected** from the MG996R / DS3218 output shaft.
2. Power on the Controller. In the web dashboard under **🎚️ Calibration**, set the target angle to **45°** and click **Apply Angle** (or power on; the setup default moves to 45°).
3. The servo shaft will rotate and lock firmly at **45°**.
4. With the bin resting in its vertical upright position against the chassis support frame, press the metal horn onto the splined shaft.
5. Fasten the central retaining screw tightly.
6. In the web dashboard, test the preset button **180° Dump**.
7. Watch the bin tilt outward smoothly. Confirm that at 180°, the container tilts far enough for waste to slide out cleanly without the mechanical linkages binding or colliding with the chassis.
8. Click **45° Upright** to return the bin to vertical.

---

## 6. Firmware Compilation & Flashing Walkthrough

The project files are located in your workspace:
* Arduino Uno: [`uno_motor_control.ino`](file:///c:/Users/Kananyan/Desktop/MED_WASTE_BOT/uno_motor_control.ino)
* MedWaste Control 1: [`master_esp32s2_webserver.ino`](file:///c:/Users/Kananyan/Desktop/MED_WASTE_BOT/master_esp32s2_webserver.ino)
* MedWaste Control 2: [`slave_esp32s2_client.ino`](file:///c:/Users/Kananyan/Desktop/MED_WASTE_BOT/slave_esp32s2_client.ino)

---

### 6.1 Flashing the Arduino Uno (`uno_motor_control.ino`)

1. Connect the Arduino Uno to your PC using a USB-B cable.
2. Open Arduino IDE and load `uno_motor_control.ino`.
3. In the **Tools** menu, configure:
   * **Board**: `"Arduino Uno"`
   * **Port**: Select your Uno COM port (e.g., `COM3`).
   * **Programmer**: `"AVRISP mkII"` (default).
4. Click **Upload** (the right-arrow button $\rightarrow$).
5. Wait for the status bar to show `Done uploading`.
6. Open the **Serial Monitor** at **9600 baud**.
7. You should observe:
   ```text
   [SYSTEM] USB Hardware Serial initialized @ 9600 baud
   [SYSTEM] Dedicated ESP/NodeMCU SoftwareSerial @ D10(RX) & D11(TX)
   [SYSTEM] Ready in IDLE mode.
   ```

---

### 6.2 Flashing MedWaste Control 1 (`master_esp32s2_webserver.ino` on ESP32-C3)

1. Connect the ESP32-C3 board to your PC using a USB-C data cable.
2. Open `master_esp32s2_webserver.ino` in Arduino IDE.
3. In the **Tools** menu, configure:
   * **Board**: `"ESP32C3 Dev Module"`
   * **USB CDC On Boot**: `"Enabled"` *(Essential: allows Serial prints over native USB)*
   * **CPU Frequency**: `"160MHz (WiFi)"`
   * **Flash Frequency**: `"80MHz"`
   * **Flash Mode**: `"QIO"` (or `"DIO"`)
   * **Flash Size**: `"4MB (32Mb)"`
   * **Partition Scheme**: `"Default 4MB with spiffs (1.2MB APP/1.5MB SPIFFS)"`
   * **Core Debug Level**: `"None"` (or `"Info"`)
   * **Port**: Select the COM port corresponding to your ESP32-C3.
4. Click **Upload**.
   * *Troubleshooting Upload*: If the IDE shows `Connecting......____....`, hold down the physical **BOOT** button on the ESP32-C3, tap the **RESET** button once, release the **BOOT** button, and re-click Upload.
5. Open the **Serial Monitor** at **115200 baud**.
6. You will see:
   ```text
   [SYSTEM] MedWaste Control 1 starting up...
   [SYSTEM] SoftAP created: MedWaste-1-AP (IP: 192.168.4.1)
   [SYSTEM] Tilt travel configured: 45 deg to 180 deg (3 tilt cycles, 25 ms/deg)
   [SYSTEM] System Ready. Web server listening on port 80.
   ```

---

### 6.3 Flashing MedWaste Control 2 (`slave_esp32s2_client.ino` on NodeMCU ESP8266)

1. Connect the NodeMCU ESP8266 board to your PC using a Micro-USB cable.
2. Open `slave_esp32s2_client.ino` in Arduino IDE.
3. In the **Tools** menu, configure:
   * **Board**: `"NodeMCU 1.0 (ESP-12E Module)"`
   * **CPU Frequency**: `"80 MHz"` (or `"160 MHz"`)
   * **Flash Size**: `"4MB (FS:2MB OTA:~1019KB)"`
   * **Upload Speed**: `"115200"` (or `"921600"` for faster flashing)
   * **Port**: Select your NodeMCU COM port.
4. Click **Upload**.
5. Open the **Serial Monitor** at **115200 baud**.
6. You will see:
   ```text
   [SYSTEM] MedWaste Control 2 starting up...
   [SYSTEM] SoftAP created: MedWaste-2-AP (IP: 192.168.4.1)
   [SYSTEM] Tilt travel configured: 45 deg to 180 deg (3 tilt cycles, 25 ms/deg)
   [SYSTEM] System Ready. Web server listening on port 80.
   ```

---

### 6.4 Synchronizing Sketchbook Copies in Arduino Documents

Your Arduino IDE default sketchbook directory points to `C:\Users\Kananyan\Documents\Arduino\`. All firmware source files have been pre-synchronized to the corresponding sketchbook folders:
* `C:\Users\Kananyan\Documents\Arduino\MASTER\MASTER.ino` $\longleftrightarrow$ `master_esp32s2_webserver.ino`
* `C:\Users\Kananyan\Documents\Arduino\Slave\Slave.ino` $\longleftrightarrow$ `slave_esp32s2_client.ino`
* `C:\Users\Kananyan\Documents\Arduino\MOTOR_CTRL\MOTOR_CTRL.ino` $\longleftrightarrow$ `uno_motor_control.ino`
* `C:\Users\Kananyan\Documents\Arduino\MedwatseAr\MedwatseAr.ino` $\longleftrightarrow$ `uno_motor_control.ino`

---

## 7. Initial Bring-Up & Web Dashboard Provisioning

### 7.1 Power-On Sequence & Automatic Warm Reset
1. Turn on the main SPST battery power switch.
2. The controllers execute an **Automatic Cold Power-On Warm Reset**:
   * Microcontroller boots, pauses for ~800ms while power rails and filter capacitors stabilize.
   * Performs an internal soft reset to guarantee that the Wi-Fi modem and UART serial subsystems initialize with 100% stability.

### 7.2 Connecting to Fallback Hotspots (`MedWaste-1-AP` / `MedWaste-2-AP`)
Each unit broadcasts its own standalone Wi-Fi hotspot immediately upon power-up:

| Parameter | MedWaste Control 1 | MedWaste Control 2 |
| :--- | :--- | :--- |
| **SSID** | `MedWaste-1-AP` | `MedWaste-2-AP` |
| **Password** | `medwaste123` | `medwaste123` |
| **Default IP** | `http://192.168.4.1` | `http://192.168.4.1` |
| **mDNS Domain** | `http://medwaste1.local` | `http://medwaste2.local` |

1. On your smartphone, tablet, or laptop, open Wi-Fi settings and connect to `MedWaste-1-AP` (or `MedWaste-2-AP`).
2. Enter password: `medwaste123`.
3. Open any modern web browser (Chrome, Edge, Safari, Firefox).
4. Navigate to **`http://192.168.4.1`** (or **`http://medwaste1.local`**).
5. The full MedWaste Bot dark-mode web dashboard will load immediately.

---

### 7.3 Provisioning Facility Wi-Fi (Station Mode & mDNS)
To control the robot over your existing clinic or home Wi-Fi network without switching hotspots:
1. Scroll down to the **⚙️ Settings & Wi-Fi Setup** card on the web dashboard.
2. Tap the **🔍 Scan Wi-Fi** button. The controller scans all nearby 2.4 GHz access points.
3. Select your network from the dropdown list.
4. Enter your facility Wi-Fi password.
5. Click **💾 Save & Connect**.
6. The controller reboots, joins your local Wi-Fi router, and acquires an IP address (e.g. `192.168.1.125`).
7. You can now access the dashboard from any smartphone or PC connected to that network via:
   * **`http://medwaste1.local`** (Unit 1)
   * **`http://medwaste2.local`** (Unit 2)
   * Or directly via the assigned router IP.
   *(Note: The fallback AP `MedWaste-1-AP` remains active in the background for offline emergency access).*

---

## 8. End-to-End Calibration & Verification ("Until It Works")

Follow this sequence step-by-step to test and calibrate every subsystem before autonomous operation.

---

### Phase 1: Telemetry & UART Communication Link Test
1. On the web dashboard, look at the top status bar.
2. Verify that **Uno Link** shows <span style="color:#22c55e;font-weight:bold;">CONNECTED</span> with a green pulse dot.
3. Scroll to the **💻 Serial Terminals** section:
   * In the **Arduino Uno Telemetry Stream** box, you should see incoming packets streaming every 500ms:
     ```text
     STATUS,150,110,480,105,490,520,STOPPED,0,IDLE,85
     ```
   * If the stream is blank or shows "Uno Disconnected", inspect Uno pins D10/D11 to ESP GPIO 2/3 (or D5/D7 on NodeMCU) and ensure **Common Ground** is securely connected (see [Troubleshooting Issue 38](file:///c:/Users/Kananyan/Desktop/MED_WASTE_BOT/TROUBLESHOOTING.md#issue-38-web-dashboard-displays-uno-disconnected-or---for-all-telemetry)).

---

### Phase 2: IR Sensor Array Threshold Calibration (850 Threshold)
The discrete line-following algorithm uses `#define THRESHOLD 850`:
* Reading **$> 850$**: BLACK line or black finish pad.
* Reading **$\le 850$**: WHITE / reflective clinic floor.

1. In the web dashboard, scroll to the **🎚️ Calibration** card.
2. Observe the live 5-channel visualizer bars for sensors **S1, S2, S3, S4, S5**.
3. **White Floor Test**: Lift the robot and hold it over clean white floor.
   * All 5 bars should read between **80 and 400** (well below 850).
4. **Black Line Test**: Place the center sensor (**S3**) directly over your black electrical tape line (1.5–2.5 cm wide).
   * **S3** should jump to **900–1023** (above 850).
   * **S1, S2, S4, S5** should remain low ($< 500$).
5. **Black Pad Test**: Place the robot over the solid black finish line cross-pad where all 5 sensors sit over black.
   * All 5 bars should simultaneously indicate black ($> 850$).
   * This confirms the auto-stop finish trigger will trip reliably.
6. If ambient lighting causes readings to drift, click **⬛ Calibrate BLACK Pad** and **➖ Calibrate Guide LINE** to save optimized thresholds to flash.

---

### Phase 3: Ultrasonic Detection & Hands-Free Lid Opening Test
1. Place the robot on the floor in **IDLE** mode.
2. **Obstacle / Hands-Free Distance**:
   * Walk in front of the robot at a distance of ~100 cm. The dashboard Front Distance indicator will display `~100 cm`.
   * Step closer to within **50 cm**.
   * The front HC-SR04 triggers the hands-free deposition routine:
     1. Uno transmits `OPEN_LID`.
     2. Lid servo smoothly swings open to **90°**.
     3. The lid stays open for **5 seconds** (`lidOpenMs`).
     4. Step away; after 5 seconds of clear space, the servo automatically swings shut to **0°**.
3. **Bin Fill Depth Test**:
   * Inspect the **Waste Fill Level** gauge on the dashboard.
   * Place an object 5 cm below the top ultrasonic sensor.
   * The gauge will indicate `100% FULL (BIN FULL LOCKOUT)`.
   * Try stepping in front of the robot: the lid **will not open** because the full-bin lockout prevents biohazard overflow.
   * Remove the object: fill level drops back to normal and proximity opening re-enables.

---

### Phase 4: Servo Dump Motion Validation (Smooth 25 ms/deg)
1. On the web dashboard, scroll to the **Tilt Servo (Pin 19 / Pin D3)** control card.
2. Click **180° Dump**.
3. Observe the metal gear tilt servo:
   * The servo should advance **1 degree at a time** with a smooth **25 ms step delay**.
   * The complete 135° stroke (from 45° to 180°) will take approximately **3.4 seconds**.
   * The movement should be quiet, controlled, and free from violent whipping.
4. Click **45° Upright**. The bin returns smoothly to vertical standby.
5. If the motion is too slow or too fast for your bin weight, adjust the **Step Delay** input (between 15 ms and 30 ms) and click **Update Step Delay**.

---

### Phase 5: Manual Drive D-Pad Verification (Tap vs Hold)
1. Lift the robot chassis so wheels can spin freely in the air.
2. In the dashboard, navigate to **🎮 Manual Control**.
3. **Hold-to-Drive Test**:
   * Click and hold down the **▲ Forward** button on screen.
   * Both wheels should spin forward continuously at full base speed.
   * Release your mouse or finger: the wheels stop immediately.
4. **Momentary Tap Test**:
   * Briefly tap the **▲ Forward** button once.
   * The robot executes a crisp **500 ms** pulse step to overcome static friction and then safely halts.
5. Test **◄ Left**, **► Right**, and **▼ Reverse**.
   * If Left spins right or wheels rotate in reverse, refer to [Troubleshooting Issue 9](file:///c:/Users/Kananyan/Desktop/MED_WASTE_BOT/TROUBLESHOOTING.md#issue-9-motors-rotate-in-reverse-when-commanded-forward).

---

### Phase 6: Full Autonomous Mission Test (Forward & Reverse Station Return)
1. Lay out a black track on a light-colored floor:
   * 2 cm wide black electrical tape line connecting the **Base Station** to the **Disposal Station**.
   * At both ends, create an all-black transverse pad (at least 15 cm wide $\times$ 10 cm long) so all 5 IR sensors touch black simultaneously.
2. Place the robot at the **Base Station** facing forward along the line.
3. In the web dashboard, click **🚀 Execute Auto Disposal** (or trigger via serial `MOVE_TO_DISPOSAL`):
   * **Stage 1 (Forward Tracking)**: The robot accelerates forward, dynamically steering along the black line using discrete sensor logic.
   * **Stage 2 (Disposal Pad Arrival)**: When all 5 sensors detect black, the robot stops immediately without turning around!
   * **Stage 3 (Automated Disposal Routine)**:
     1. Lid servo opens to 90° (700ms stabilization pause).
     2. Tilt servo gradually sweeps from 45° to 180° at 25 ms/deg.
     3. Holds at 180° for 1.5 seconds for complete waste slide-out.
     4. Gradually sweeps back to 45°.
     5. Repeats for 3 full dumping cycles.
     6. Lid servo closes to 0° (700ms pause).
   * **Stage 4 (Reverse Tracking Return)**: Robot drives in reverse backward along the line (`runLineFollowingBackward`), steering smoothly back toward the base station.
   * **Stage 5 (Base Station Arrival)**: When all 5 sensors hit the Base Station black pad, the robot halts, shuts off motors, and settles into **IDLE** facing forward, ready for the next call!

---

## 9. Operational Cheat Sheet & Safety Rules

* **Emergency Stop**: Tap the large red **STOP** button on the web dashboard at any time to immediately kill motor power and abort automated cycles.
* **Deadman Watchdog**: If Wi-Fi disconnects or browser closes during manual teleoperation, the robot automatically cuts motor power within **650 ms**.
* **Battery Protection**: Never let 2S Li-ion drop below 6.4V (3.2V/cell) or 3S drop below 9.6V. Charge batteries promptly when motors sound sluggish.
* **Mechanical Binding**: If servos buzz loudly when stationary at 45° or 180°, loosen the linkage slightly; the servo is fighting a physical chassis stop.
* **Cleaning & Biohazard Safety**: Wipe down ultrasonic sensor faces and IR sensors with an alcohol wipe regularly. Do not spray liquids directly into motor driver or microcontroller vents.

---

### Need Further Help?
Consult the **[Troubleshooting Guide (54 Issues)](file:///c:/Users/Kananyan/Desktop/MED_WASTE_BOT/TROUBLESHOOTING.md)** for detailed diagnostic steps and solutions for every conceivable hardware, electrical, code, and network problem.
