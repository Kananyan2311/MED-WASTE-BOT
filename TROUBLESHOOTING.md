# MedWaste Bot — Comprehensive Troubleshooting Guide

A complete diagnostic and resolution handbook for the MedWaste Bot autonomous medical waste disposal platform. This guide covers **54 common hardware, electrical, firmware, sensor, and communication issues** with step-by-step diagnostic workflows and concrete fixes.

---

## Table of Contents

### 1. Power & Electrical Distribution
* [Issue 1: Robot brownouts or microcontroller resets when DC motors start](#issue-1-robot-brownouts-or-microcontroller-resets-when-dc-motors-start)
* [Issue 2: Servos jitter, buzz, or twitch while stationary in standby](#issue-2-servos-jitter-buzz-or-twitch-while-stationary-in-standby)
* [Issue 3: ESP32-C3 resets when the tilt servo reaches 180° dump position](#issue-3-esp32-c3-resets-when-the-tilt-servo-reaches-180-dump-position)
* [Issue 4: NodeMCU ESP8266 reboots in a continuous loop when connected to 5V rail](#issue-4-nodemcu-esp8266-reboots-in-a-continuous-loop-when-connected-to-5v-rail)
* [Issue 5: TB6612FNG motor driver IC heats up excessively during normal operation](#issue-5-tb6612fng-motor-driver-ic-heats-up-excessively-during-normal-operation)
* [Issue 6: 5V logic supply rail sags below 4.5V under load](#issue-6-5v-logic-supply-rail-sags-below-45v-under-load)
* [Issue 7: Floating ground reference causes intermittent sensor spikes or serial noise](#issue-7-floating-ground-reference-causes-intermittent-sensor-spikes-or-serial-noise)

### 2. Motor Driver (TB6612FNG) & DC Motors
* [Issue 8: DC motors do not move at all when manual drive buttons are tapped](#issue-8-dc-motors-do-not-move-at-all-when-manual-drive-buttons-are-tapped)
* [Issue 9: Motors rotate in reverse when commanded forward](#issue-9-motors-rotate-in-reverse-when-commanded-forward)
* [Issue 10: Robot veers off to one side when driving forward on a flat surface](#issue-10-robot-veers-off-to-one-side-when-driving-forward-on-a-flat-surface)
* [Issue 11: Motors emit high-pitched PWM whine but wheels fail to rotate](#issue-11-motors-emit-high-pitched-pwm-whine-but-wheels-fail-to-rotate)
* [Issue 12: Manual drive moves for 500ms then stops (understanding tap vs hold)](#issue-12-manual-drive-moves-for-500ms-then-stops-understanding-tap-vs-hold)
* [Issue 13: Motors continue driving and ignore the STOP command](#issue-13-motors-continue-driving-and-ignore-the-stop-command)
* [Issue 14: TB6612FNG standby pin (STBY) is pulled LOW or left floating](#issue-14-tb6612fng-standby-pin-stby-is-pulled-low-or-left-floating)
* [Issue 15: Gearmotors stall or struggle on carpet or uneven clinic thresholds](#issue-15-gearmotors-stall-or-struggle-on-carpet-or-uneven-clinic-thresholds)

### 3. Servos (Lid Servo & Tilt Servo Pin 19 / Pin D3)
* [Issue 16: Tilt servo does not reach full 45° upright or 180° dump position](#issue-16-tilt-servo-does-not-reach-full-45-upright-or-180-dump-position)
* [Issue 17: Tilt servo snaps too fast instead of gradual careful motion (25 ms/deg)](#issue-17-tilt-servo-snaps-too-fast-instead-of-gradual-careful-motion-25-msdeg)
* [Issue 18: Lid servo fails to open when hands-free proximity is triggered](#issue-18-lid-servo-fails-to-open-when-hands-free-proximity-is-triggered)
* [Issue 19: Lid servo opens but snaps shut immediately without holding for 5 seconds](#issue-19-lid-servo-opens-but-snaps-shut-immediately-without-holding-for-5-seconds)
* [Issue 20: Tilt servo hums continuously at endpoints (mechanical horn binding)](#issue-20-tilt-servo-hums-continuously-at-endpoints-mechanical-horn-binding)
* [Issue 21: Lid servo pushes against the bin physical hinge stops](#issue-21-lid-servo-pushes-against-the-bin-physical-hinge-stops)
* [Issue 22: Tilt servo moves clockwise instead of counter-clockwise](#issue-22-tilt-servo-moves-clockwise-instead-of-counter-clockwise)
* [Issue 23: ESP32Servo library timer conflicts with Wi-Fi on ESP32-C3](#issue-23-esp32servo-library-timer-conflicts-with-wi-fi-on-esp32-c3)

### 4. Line Following & 5-Channel IR Tracking Array
* [Issue 24: Robot spins in circles continuously instead of tracking the line](#issue-24-robot-spins-in-circles-continuously-instead-of-tracking-the-line)
* [Issue 25: All 5 analog IR sensors read 0 or read 1023 constantly](#issue-25-all-5-analog-ir-sensors-read-0-or-read-1023-constantly)
* [Issue 26: Robot fails to stop at the finish line / disposal crossline pad](#issue-26-robot-fails-to-stop-at-the-finish-line--disposal-crossline-pad)
* [Issue 27: Robot loses the line on sharp turns and stops instead of spinning back](#issue-27-robot-loses-the-line-on-sharp-turns-and-stops-instead-of-spinning-back)
* [Issue 28: Robot overshoots the line during mild steering corrections (hunting)](#issue-28-robot-overshoots-the-line-during-mild-steering-corrections-hunting)
* [Issue 29: Line sensor threshold (850) triggers false detections under ambient light](#issue-29-line-sensor-threshold-850-triggers-false-detections-under-ambient-light)
* [Issue 30: Reverse line tracking (runLineFollowingBackward) steers in the wrong direction](#issue-30-reverse-line-tracking-runlinefollowingbackward-steers-in-the-wrong-direction)
* [Issue 31: Robot stops immediately right after departing from the base station](#issue-31-robot-stops-immediately-right-after-departing-from-the-base-station)

### 5. Ultrasonic Sensors (Front Obstacle & Top Bin Depth)
* [Issue 32: Front HC-SR04 returns -1 cm or erratic out-of-range readings](#issue-32-front-hc-sr04-returns--1-cm-or-erratic-out-of-range-readings)
* [Issue 33: Hands-free lid opening does not trigger when approaching front (< 50 cm)](#issue-33-hands-free-lid-opening-does-not-trigger-when-approaching-front--50-cm)
* [Issue 34: Top bin depth sensor reads 0% full when the container is empty](#issue-34-top-bin-depth-sensor-reads-0-full-when-the-container-is-empty)
* [Issue 35: Full bin lockout triggers prematurely when waste is only half full](#issue-35-full-bin-lockout-triggers-prematurely-when-waste-is-only-half-full)
* [Issue 36: Robot triggers emergency obstacle stops on completely clear floors](#issue-36-robot-triggers-emergency-obstacle-stops-on-completely-clear-floors)
* [Issue 37: Ultrasonic pulseIn measurement blocks motor control loop timing](#issue-37-ultrasonic-pulsein-measurement-blocks-motor-control-loop-timing)

### 6. Serial Communication (Uno ⟷ ESP32 / NodeMCU)
* [Issue 38: Web dashboard displays "Uno Disconnected" or "-" for all telemetry](#issue-38-web-dashboard-displays-uno-disconnected-or---for-all-telemetry)
* [Issue 39: Telemetry stream displays corrupted or garbled ASCII characters](#issue-39-telemetry-stream-displays-corrupted-or-garbled-ascii-characters)
* [Issue 40: D-Pad or buttons clicked in web dashboard do not execute on Uno](#issue-40-d-pad-or-buttons-clicked-in-web-dashboard-do-not-execute-on-uno)
* [Issue 41: Noticeable 1–2 second latency between UI button click and robot movement](#issue-41-noticeable-12-second-latency-between-ui-button-click-and-robot-movement)
* [Issue 42: SoftwareSerial buffer overflow crashes communications under load](#issue-42-softwareserial-buffer-overflow-crashes-communications-under-load)
* [Issue 43: Uno cannot upload new firmware while connected to ESP32 TX/RX](#issue-43-uno-cannot-upload-new-firmware-while-connected-to-esp32-txrx)

### 7. Wi-Fi, AP Hotspot, Web Server & mDNS
* [Issue 44: Fallback Wi-Fi Hotspot (MedWaste-1-AP / MedWaste-2-AP) does not appear](#issue-44-fallback-wi-fi-hotspot-medwaste-1-ap--medwaste-2-ap-does-not-appear)
* [Issue 45: Connected to AP Wi-Fi but http://192.168.4.1 displays a blank page](#issue-45-connected-to-ap-wi-fi-but-http19216841-displays-a-blank-page)
* [Issue 46: Local mDNS URL (http://medwaste1.local) fails to resolve on client device](#issue-46-local-mdns-url-httpmedwaste1local-fails-to-resolve-on-client-device)
* [Issue 47: Controller disconnects from facility Wi-Fi and fails to auto-reconnect](#issue-47-controller-disconnects-from-facility-wi-fi-and-fails-to-auto-reconnect)
* [Issue 48: Web dashboard touch controls feel unresponsive on iOS or Android](#issue-48-web-dashboard-touch-controls-feel-unresponsive-on-ios-or-android)

### 8. Arduino IDE Compilation & Flashing Errors
* [Issue 49: error: 'TILT_STEPS' was not declared in this scope](#issue-49-error-tilt_steps-was-not-declared-in-this-scope)
* [Issue 50: undefined reference to 'getDistance()' / ld returned 1 exit status](#issue-50-undefined-reference-to-getdistance--ld-returned-1-exit-status)
* [Issue 51: A fatal error occurred: Failed to connect to ESP32-C3](#issue-51-a-fatal-error-occurred-failed-to-connect-to-esp32-c3)
* [Issue 52: esp_camera.h: No such file or directory error during compilation](#issue-52-esp_camerah-no-such-file-or-directory-error-during-compilation)
* [Issue 53: NodeMCU ESP8266 throws fatal exception (28) or wdt reset watchdog loop](#issue-53-nodemcu-esp8266-throws-fatal-exception-28-or-wdt-reset-watchdog-loop)
* [Issue 54: avrdude: stk500_recv(): programmer is not responding on Uno upload](#issue-54-avrdude-stk500_recv-programmer-is-not-responding-on-uno-upload)

---

## 1. Power & Electrical Distribution

### Issue 1: Robot brownouts or microcontroller resets when DC motors start
* **Symptom**: Whenever motors ramp up, the ESP32-C3 or Uno resets, Wi-Fi drops, and the robot resets to IDLE.
* **Root Cause**: Geared DC motors draw up to 2A–3A inrush stall current when starting. Powering motors from the microcontroller's 5V pin or a shared weak battery causes voltage sag below the MCU brownout threshold (2.7V for ESP32, 4.3V for Uno).
* **Diagnostic Procedure**:
  1. Connect a digital multimeter across the 5V power rail.
  2. Command a manual forward drive (`MAN_FWD`).
  3. Observe if the voltage drops below 4.5V at the moment of start.
* **Resolution**:
  1. Separate motor power (`VM` on TB6612FNG) from logic power (`VCC`). Connect motors directly to battery pack (+) (7.4V–12V).
  2. Solder a **$470\,\mu\text{F}$ to $1000\,\mu\text{F}$ electrolytic capacitor** directly across the battery power leads at the motor driver board.
  3. Ensure a dedicated 3A+ step-down buck converter (BEC) powers the 5V logic rail.

---

### Issue 2: Servos jitter, buzz, or twitch while stationary in standby
* **Symptom**: Both lid servo and tilt servo vibrate, make buzzing sounds, or randomly twitch while parked at base.
* **Root Cause**: PWM signal noise, electrical interference from motor wiring, or voltage ripple on the servo power rail.
* **Diagnostic Procedure**:
  1. Touch the servo horn. If vibration stops when lightly held, the servo is oscillating around its deadband.
  2. Check whether servo signal wires are bundled tightly alongside motor driver high-current PWM leads.
* **Resolution**:
  1. Separate servo signal wires from DC motor driver wires by at least 3 cm to prevent inductive coupling.
  2. Add a **$100\,\mu\text{F}$ capacitor** between Servo `VCC` and `GND` right next to the servo plug.
  3. Verify that the PWM pulse frequency is set to standard 50 Hz in [`ESP32Servo`](file:///c:/Users/Kananyan/Desktop/MED_WASTE_BOT/master_esp32s2_webserver.ino#L58).

---

### Issue 3: ESP32-C3 resets when the tilt servo reaches 180° dump position
* **Symptom**: Tilt servo starts rotating, but as soon as it reaches $180^\circ$ or lifts the bin, the ESP32 resets with `rst:0x1 (POWERON)`.
* **Root Cause**: The tilt servo is lifting the full bin weight and reaches its peak stall current ($1.5\text{A}–2.5\text{A}$), pulling down the supply rail.
* **Diagnostic Procedure**:
  1. Disconnect the servo linkage from the bin and run the tilt test.
  2. If the servo rotates to $180^\circ$ without rebooting while unloaded, the issue is peak current draw under mechanical load.
* **Resolution**:
  1. Never power the high-torque tilt servo from the ESP32 3.3V or 5V development board pin.
  2. Power the tilt servo from an external **5V/6V 3A UBEC regulator**.
  3. Tie the UBEC ground wire directly to the ESP32 `GND` (common ground).

---

### Issue 4: NodeMCU ESP8266 reboots in a continuous loop when connected to 5V rail
* **Symptom**: NodeMCU blue LED flashes rapidly and serial monitor prints `wdt reset` or gibberish at boot.
* **Root Cause**: Pin `D3` (GPIO 0) or `D4` (GPIO 2) or `D8` (GPIO 15) is pulled to an invalid state at boot. GPIO 0 and GPIO 2 must be HIGH at boot, and GPIO 15 must be LOW for normal Flash boot.
* **Diagnostic Procedure**:
  1. Check what is connected to NodeMCU Pin `D3`. It connects to Tilt Servo signal.
  2. Some servo input circuits pull GPIO 0 to ground at startup, forcing NodeMCU into UART flash download mode.
* **Resolution**:
  1. Add a **$10\text{ k}\Omega$ pull-up resistor** between Pin `D3` (GPIO 0) and 3.3V to ensure it remains HIGH during the bootloader stage.
  2. Ensure the firmware uses boot-safe pins as documented in [PIN_CONNECTIONS.txt](file:///c:/Users/Kananyan/Desktop/MED_WASTE_BOT/PIN_CONNECTIONS.txt#L98-L105).

---

### Issue 5: TB6612FNG motor driver IC heats up excessively during normal operation
* **Symptom**: TB6612FNG surface is too hot to touch ($> 70^\circ\text{C}$), and motors lose power after 30 seconds.
* **Root Cause**: Motor current exceeds the TB6612FNG rated limit ($1.2\text{A}$ continuous, $3.2\text{A}$ peak per channel), or motors are operating at near-stall torque.
* **Diagnostic Procedure**:
  1. Measure motor coil resistance with a multimeter across the motor terminals (should be $> 5\,\Omega$).
  2. Check for mechanical binding: can the wheels turn freely by hand with power off?
* **Resolution**:
  1. Attach a small copper or aluminum stick-on heatsink to the TB6612FNG package.
  2. Verify base speed is not set too low ($< 100$ PWM), which causes high resistive heating in the motor coils without rotation.
  3. Ensure wheel axles are properly lubricated and not rubbing against the chassis.

---

### Issue 6: 5V logic supply rail sags below 4.5V under load
* **Symptom**: Sensors read erratically, distance readings jump, and ADC readings fluctuate.
* **Root Cause**: Using thin jumper wires (28–30 AWG) with high series resistance for the main power feed.
* **Diagnostic Procedure**:
  1. Measure voltage directly at battery terminals, then measure voltage between Arduino Uno 5V and GND pins.
  2. If there is a $> 0.4\text{V}$ drop across the wire, wire gauge is too thin.
* **Resolution**:
  1. Replace main power distribution wires with **20 AWG or 22 AWG silicone stranded wire**.
  2. Use a centralized screw terminal block or distribution PCB for power distribution instead of daisy-chaining breadboard jumpers.

---

### Issue 7: Floating ground reference causes intermittent sensor spikes or serial noise
* **Symptom**: Uno prints random numbers, UART commands drop, or analog sensors jump by $\pm 300$ counts.
* **Root Cause**: Arduino Uno, ESP32/NodeMCU, and battery pack do not share an established common ground.
* **Diagnostic Procedure**:
  1. Set multimeter to continuity / ohms mode with battery disconnected.
  2. Test between Arduino GND, ESP32 GND, TB6612FNG GND, and Battery (-). All must read $< 0.5\,\Omega$.
* **Resolution**:
  1. Connect a dedicated 20 AWG ground wire between Arduino Uno `GND` and ESP32/NodeMCU `GND`.
  2. Ensure all external power supplies (BEC/regulators) share this same common ground return path.

---

## 2. Motor Driver (TB6612FNG) & DC Motors

### Issue 8: DC motors do not move at all when manual drive buttons are tapped
* **Symptom**: Tapping FWD in the web dashboard logs `[MANUAL CONTROL] Cmd=MAN_FWD`, but wheels never turn.
* **Root Cause**: Quick tap was sending `MAN_STOP` 81ms later, killing motor power before gearbox overcame static inertia, or manual speed was below starting torque floor.
* **Diagnostic Procedure**:
  1. Open Serial Monitor on Arduino Uno at 9600 baud.
  2. Tap FWD in dashboard. Confirm whether `ACK:MAN_FWD` is returned.
  3. Measure voltage between `PWMA` (Pin D5) and GND while holding FWD down.
* **Resolution**:
  1. Verify the latest firmware is flashed: [`master_esp32s2_webserver.ino`](file:///c:/Users/Kananyan/Desktop/MED_WASTE_BOT/master_esp32s2_webserver.ino#L1435) guarantees a **500 ms minimum drive step** on quick taps.
  2. Verify [`uno_motor_control.ino`](file:///c:/Users/Kananyan/Desktop/MED_WASTE_BOT/uno_motor_control.ino#L403) uses the high-torque floor: `spd = (baseSpeed > 160) ? baseSpeed : 220;`.
  3. Ensure `STBY` on TB6612FNG is tied directly to 5V (HIGH).

---

### Issue 9: Motors rotate in reverse when commanded forward
* **Symptom**: Pressing `FWD` makes the robot drive backward; pressing `BWD` drives forward.
* **Root Cause**: Motor A or Motor B polarity leads are reversed at the TB6612FNG output screw terminals.
* **Diagnostic Procedure**:
  1. Command `MAN_FWD` from dashboard.
  2. Look at motor terminal wiring: `AIN1` should be HIGH and `AIN2` should be LOW for forward rotation.
* **Resolution**:
  1. Swap the two physical motor wires connected to `AO1` and `AO2` (for Motor A / Left Wheel).
  2. Swap the two physical motor wires connected to `BO1` and `BO2` (for Motor B / Right Wheel).

---

### Issue 10: Robot veers off to one side when driving forward on a flat surface
* **Symptom**: In manual straight forward drive, the robot curves toward the left or right.
* **Root Cause**: Minor manufacturing differences in DC motor internal winding resistance, gearbox friction, or wheel diameter.
* **Diagnostic Procedure**:
  1. Place robot on flat floor without tracking line.
  2. Drive forward for 2 meters. Measure lateral deviation.
* **Resolution**:
  1. In [`uno_motor_control.ino`](file:///c:/Users/Kananyan/Desktop/MED_WASTE_BOT/uno_motor_control.ino#L590), apply a wheel trim multiplier to balance motors:
     ```cpp
     // Example: if left wheel is 5% faster than right wheel
     analogWrite(PWMA, constrain(absLeft * 0.95, 0, 255));
     analogWrite(PWMB, constrain(absRight, 0, 255));
     ```
  2. Check that both rubber tires are seated evenly on their rims and have identical outer diameter.

---

### Issue 11: Motors emit high-pitched PWM whine but wheels fail to rotate
* **Symptom**: High-pitched electrical buzz from motors, but the chassis stays stationary.
* **Root Cause**: PWM duty cycle is too low to overcome the static gearbox friction under chassis load.
* **Diagnostic Procedure**:
  1. Lift robot wheels off the ground. If wheels spin freely in the air but stall on the floor, PWM is below static friction threshold.
* **Resolution**:
  1. In the web dashboard under **Settings**, raise **Base Motor Speed** from 150 to **200–255**.
  2. Confirm battery pack voltage is at least 7.4V (2S LiPo/Li-ion) or 9.6V–12V.

---

### Issue 12: Manual drive moves for 500ms then stops (understanding tap vs hold)
* **Symptom**: Tapping a direction button moves the robot for half a second and then stops. User wonders if this is an error.
* **Root Cause**: **This is the intentional, engineered behavior**.
* **Explanation**:
  * **Quick Tap ($< 500\text{ ms}$)**: Designed for micro-adjustments and docking alignment. It moves for a controlled 500 ms burst.
  * **Hold-to-Drive ($> 500\text{ ms}$)**: Keep your finger or mouse pressed down. The controller streams continuous keep-alive packets and the robot drives indefinitely until you release.
* **Resolution**:
  * For continuous driving, simply press and **hold** the FWD/BWD/LEFT/RIGHT button.

---

### Issue 13: Motors continue driving and ignore the STOP command
* **Symptom**: Releasing the button or tapping STOP does not halt the robot immediately.
* **Root Cause**: Wi-Fi packet dropped while moving, or browser lost connection during hold.
* **Diagnostic Procedure**:
  1. Check Uno watchdog timeout: if communication drops, Uno's internal deadman timer automatically cuts motor power after 650 ms.
* **Resolution**:
  1. Tap the large red **🛑 EMERGENCY STOP** button in the dashboard, which clears all browser intervals and fires immediate `MAN_STOP`.
  2. Ensure the deadman watchdog in [`uno_motor_control.ino`](file:///c:/Users/Kananyan/Desktop/MED_WASTE_BOT/uno_motor_control.ino#L248) is active: `currentMillis - lastManualCmdTime >= MANUAL_DEADMAN_TIMEOUT_MS`.

---

### Issue 14: TB6612FNG standby pin (STBY) is pulled LOW or left floating
* **Symptom**: All logic signals (PWMA, AIN1, PWMB) measure correct voltages, but motor outputs read 0V.
* **Root Cause**: The TB6612FNG driver requires its `STBY` pin to be pulled to logic HIGH (5V) to enable the H-bridge FETs. If left floating or connected to a LOW pin, outputs remain high-impedance.
* **Diagnostic Procedure**:
  1. Measure DC voltage on the `STBY` pin with a multimeter probe.
  2. If it reads 0V or floats, the driver is disabled.
* **Resolution**:
  1. Connect `STBY` directly to the Arduino Uno **5V pin** (permanent enable).
  2. If controlled by a pin, verify `pinMode(STBY_PIN, OUTPUT); digitalWrite(STBY_PIN, HIGH);` runs in `setup()`.

---

### Issue 15: Gearmotors stall or struggle on carpet or uneven clinic thresholds
* **Symptom**: Robot drives normally on smooth tile but stalls or loses line on medical clinic carpet.
* **Root Cause**: Low-gear-ratio motors (e.g. 1:48 plastic yellow gearmotors) produce insufficient torque for rolling resistance on carpet.
* **Diagnostic Procedure**:
  1. Measure motor current on carpet vs smooth tile.
* **Resolution**:
  1. Upgrade to all-metal gear DC motors with a **1:90 or 1:120 gear ratio** (e.g., 25GA-370 or N20 high-torque series).
  2. Increase battery voltage from 7.4V to **11.1V (3S LiPo)** to supply higher torque through the TB6612FNG (rated up to 15V VM).

---

## 3. Servos (Lid Servo & Tilt Servo Pin 19 / Pin D3)

### Issue 16: Tilt servo does not reach full 45° upright or 180° dump position
* **Symptom**: Servo moves toward upright but rests at ~60°, or tilts to ~150° and stops short of dumping.
* **Root Cause**: The servo horn was attached at an offset angle on the splined shaft, or PWM pulse limits are constrained.
* **Diagnostic Procedure**:
  1. In the web dashboard under **📐 Lid & Tilt Servo**, click **45° Upright**.
  2. Check physical orientation of the bin.
* **Resolution**:
  1. Remove the screw securing the servo horn to the servo spline.
  2. Detach the horn, click **45° Upright** on the dashboard, and wait for the shaft to rotate to position.
  3. Re-mount the horn onto the splines so the bin rests in the exact desired upright orientation, then tighten the screw.
  4. Verify PWM range in [`master_esp32s2_webserver.ino`](file:///c:/Users/Kananyan/Desktop/MED_WASTE_BOT/master_esp32s2_webserver.ino#L2659): `tiltServo.attach(TILT_SERVO_PIN, 500, 2400);`.

---

### Issue 17: Tilt servo snaps too fast instead of gradual careful motion (25 ms/deg)
* **Symptom**: When dumping starts, the tilt servo jerks violently to $180^\circ$ instead of smoothly stepping.
* **Root Cause**: Firmware called `tiltServo.write(180)` directly instead of using the non-blocking stepping function [`stepTiltServoTowards()`](file:///c:/Users/Kananyan/Desktop/MED_WASTE_BOT/master_esp32s2_webserver.ino#L3265).
* **Diagnostic Procedure**:
  1. Inspect `handleDisposalMachine()` state `DUMPING_DOWN`.
  2. Verify `stepTiltServoTowards(DUMP_TARGET_DEG)` is called on every loop iteration.
* **Resolution**:
  1. Ensure [`tiltStepDelayMs = 25`](file:///c:/Users/Kananyan/Desktop/MED_WASTE_BOT/master_esp32s2_webserver.ino#L80) is configured.
  2. Verify the 1-degree increment stepping loop:
     ```cpp
     if (now - lastTiltStepMillis >= (unsigned long)tiltStepDelayMs) {
       currentAngleDeg += (targetDeg > currentAngleDeg) ? 1.0f : -1.0f;
       tiltServo.write((int)round(currentAngleDeg));
       lastTiltStepMillis = now;
     }
     ```

---

### Issue 18: Lid servo fails to open when hands-free proximity is triggered
* **Symptom**: Healthcare worker approaches within 50 cm of front ultrasonic sensor, but lid remains closed.
* **Root Cause**: Either the trash bin is full (lockout active), robot is not in IDLE, or Uno-to-ESP serial link is down.
* **Diagnostic Procedure**:
  1. Check web dashboard: is the **Trash Level** badge red ("Full")? If waste $\le 10\text{ cm}$, proximity opening is intentionally locked out.
  2. Check the **Robot State** badge: must be `IDLE`. Proximity opening is disabled while moving or disposing.
  3. Open Serial Monitor on Uno: confirm if `[EVENT] Person detected near front while IDLE! Signaling OPEN_LID` appears.
* **Resolution**:
  1. If bin is empty, verify bin depth calibration under **🎚️ Calibration** (set Bin Depth to 40 cm).
  2. Verify dedicated serial wiring: Uno D11 (TX) $\rightarrow$ ESP32 GPIO 2 (RX).

---

### Issue 19: Lid servo opens but snaps shut immediately without holding for 5 seconds
* **Symptom**: Lid swings open to 90° and immediately slams shut without giving time to deposit waste.
* **Root Cause**: `lidOpenMs` parameter was set too low in Flash ROM, or Uno repeatedly re-triggered `OPEN_LID`.
* **Diagnostic Procedure**:
  1. In the web dashboard under **🎚️ Calibration**, check the value of **Lid open time (ms)**.
* **Resolution**:
  1. Set **Lid open time** to **5000 ms** (5 seconds) and click **💾 Save & Apply Calibration**.
  2. Confirm [`master_esp32s2_webserver.ino`](file:///c:/Users/Kananyan/Desktop/MED_WASTE_BOT/master_esp32s2_webserver.ino#L3169) checks `currentMillis - lastLidOpenTime >= lidOpenMs` before calling `closeLid()`.

---

### Issue 20: Tilt servo hums continuously at endpoints (mechanical horn binding)
* **Symptom**: Servo motor hums loudly, draws excessive current, and heats up when resting at 45° or 180°.
* **Root Cause**: The physical bin or linkage has hit a mechanical stop before the servo reached its commanded angle.
* **Diagnostic Procedure**:
  1. Turn off power. Move the bin by hand throughout the $45^\circ \leftrightarrow 180^\circ$ sweep.
  2. Feel for any point of resistance, rubbing, or binding against the robot frame.
* **Resolution**:
  1. File down or adjust any frame interference points.
  2. In the web dashboard under **🎚️ Calibration**, adjust **Dump tilt angle** from 180° to 175° or **Upright angle** from 45° to 50° to match physical mechanical limits.

---

### Issue 21: Lid servo pushes against the bin physical hinge stops
* **Symptom**: Lid opens past its hinge limits, straining the servo arm and flexing the hinge.
* **Root Cause**: Commanded open angle exceeds physical hinge travel.
* **Diagnostic Procedure**:
  1. Manually open the lid by hand to see its maximum natural opening angle.
* **Resolution**:
  1. In [`master_esp32s2_webserver.ino`](file:///c:/Users/Kananyan/Desktop/MED_WASTE_BOT/master_esp32s2_webserver.ino#L2637), adjust `LID_SERVO_OPEN_DEG` from 90° down to **80° or 75°**.

---

### Issue 22: Tilt servo moves clockwise instead of counter-clockwise
* **Symptom**: Tilt servo rotates downward into the chassis instead of dumping outward.
* **Root Cause**: Physical servo mounting orientation (reversed spline side).
* **Diagnostic Procedure**:
  1. Check servo horn rotation direction during dump test.
* **Resolution**:
  1. In the web dashboard under **📐 Lid & Tilt Servo**, check the box: **Invert Servo Motion Direction**.
  2. Or visit `http://<ip>/invert_dir` to invert electronically without rewiring or disassembling mechanics.

---

### Issue 23: ESP32Servo library timer conflicts with Wi-Fi on ESP32-C3
* **Symptom**: Compiling throws timer allocation errors or Wi-Fi drops whenever `servo.write()` is executed.
* **Root Cause**: Older ESP32Servo library versions allocate Timer 0, which conflicts with ESP32-C3 hardware timers.
* **Resolution**:
  1. Open Arduino IDE **Library Manager** and ensure **`ESP32Servo` by Kevin Harrington** is updated to **version 1.1.0 or later**.
  2. On ESP32-C3 (RISC-V), the updated library uses LEDC PWM channels which have zero Wi-Fi timer conflicts.

---

## 4. Line Following & 5-Channel IR Tracking Array

### Issue 24: Robot spins in circles continuously instead of tracking the line
* **Symptom**: Robot drives in tight continuous circles when line tracking is engaged.
* **Root Cause**: Left/right motor leads are swapped, or IR sensor array left/right channels are inverted.
* **Diagnostic Procedure**:
  1. Open the web dashboard **🎚️ Calibration** page to view the live **5-Channel Line Sensors** visualizer.
  2. Place your finger over Sensor 1 (Far Left). Verify that bar `S1` activates in the dashboard.
  3. If bar `S5` activates instead, sensor channels are physically wired backwards!
* **Resolution**:
  1. Swap sensor plugs: `A0` should be Far Left, `A4` should be Far Right as defined in [`uno_motor_control.ino`](file:///c:/Users/Kananyan/Desktop/MED_WASTE_BOT/uno_motor_control.ino#L24).
  2. Verify steering directions: Sharp Left should slow left motor and speed right motor (`setMotor(-40, 200)`).

---

### Issue 25: All 5 analog IR sensors read 0 or read 1023 constantly
* **Symptom**: Dashboard progress bars are completely empty (0) or completely pegged at 1023, regardless of floor surface.
* **Root Cause**: Sensor array ground disconnected, VCC disconnected, or sensor emitter LEDs have no power.
* **Diagnostic Procedure**:
  1. Point a smartphone camera at the IR sensor bar underneath the chassis in a dim room.
  2. Smartphone cameras can see near-infrared light. You should see 5 faint purple glowing LEDs.
  3. If LEDs are dark, power is not reaching the sensor board.
* **Resolution**:
  1. Check VCC and GND connections to the sensor board (must be connected to Arduino 5V and GND).
  2. Check the onboard potentiometer: some sensor modules have an analog sensitivity trimmer on the board.

---

### Issue 26: Robot fails to stop at the finish line / disposal crossline pad
* **Symptom**: Robot reaches the all-black disposal station but drives right through it without stopping.
* **Root Cause**: The crossline condition requires all 5 sensors to read black simultaneously (`s1 && s2 && s3 && s4 && s5`). If sensor height is uneven or threshold is too high, one outer sensor reads white.
* **Diagnostic Procedure**:
  1. Place the robot over the all-black disposal station pad.
  2. View the Serial Monitor or web dashboard calibration bars.
  3. Check if all 5 values exceed `THRESHOLD 850`.
* **Resolution**:
  1. Ensure the IR sensor bar is mounted strictly parallel to the floor at a height of **5 mm to 10 mm**.
  2. In dashboard under Calibration, lower **Line-detect threshold** from 850 to **800 or 750**.
  3. Use the **⬛ Calibrate BLACK Pad** button on the dashboard while parked on the station to auto-calculate the optimal threshold.

---

### Issue 27: Robot loses the line on sharp turns and stops instead of spinning back
* **Symptom**: On a $90^\circ$ sharp bend, the robot drifts off the track, all sensors see white, and the robot stops dead.
* **Root Cause**: `lastDirection` memory was cleared or lost-line recovery speeds were insufficient.
* **Diagnostic Procedure**:
  1. Watch the Serial Monitor during the turn.
  2. Check if `lastDirection` recorded `-1` (left) or `1` (right) before line was lost.
* **Resolution**:
  1. In [`uno_motor_control.ino`](file:///c:/Users/Kananyan/Desktop/MED_WASTE_BOT/uno_motor_control.ino#L666), verify recovery spin logic:
     ```cpp
     if (lastDirection == 1) {
       setMotor(MILD_FAST, -MILD_SLOW); // Keep spinning right
     } else if (lastDirection == -1) {
       setMotor(-MILD_SLOW, MILD_FAST); // Keep spinning left
     }
     ```
  2. Verify that `SHARP_FAST = 200` and `SHARP_SLOW = -40` allow the robot to pivot sharply before losing the line.

---

### Issue 28: Robot overshoots the line during mild steering corrections (hunting)
* **Symptom**: Robot snakes or fish-tails violently down a straight line, over-correcting left and right.
* **Root Cause**: Mild turn speed differential is too aggressive, or forward speed is too fast for sensor sampling rate.
* **Diagnostic Procedure**:
  1. Observe wheel speeds when only the center sensor `S3` and adjacent sensor `S2` are active.
* **Resolution**:
  1. In [`uno_motor_control.ino`](file:///c:/Users/Kananyan/Desktop/MED_WASTE_BOT/uno_motor_control.ino#L41), soften the mild differential:
     * Change `MILD_SLOW` from 200 to **220** (gentler steering curve).
  2. Lower `BASE_SPEED` from 255 to **200 or 180** in [`uno_motor_control.ino`](file:///c:/Users/Kananyan/Desktop/MED_WASTE_BOT/uno_motor_control.ino#L40).

---

### Issue 29: Line sensor threshold (850) triggers false detections under ambient light
* **Symptom**: Sunlight or halogen ceiling lights cause outer sensors to falsely report black over white floor tiles.
* **Root Cause**: Ambient infrared light from windows or incandescent bulbs blinding non-modulated phototransistors.
* **Diagnostic Procedure**:
  1. Read sensor analog values on white floor under room light vs shaded floor.
* **Resolution**:
  1. Install a 3D-printed or black cardboard **optical skirt/shroud** around the sensor bar to shield it from ambient ceiling lighting.
  2. Click **➖ Calibrate Guide LINE** on the dashboard with center sensor on black line and outer sensors on white floor to automatically set the midpoint threshold.

---

### Issue 30: Reverse line tracking (runLineFollowingBackward) steers in the wrong direction
* **Symptom**: Robot backs up from disposal station but veers off the line and spins out.
* **Root Cause**: Reverse steering directions were not inverted relative to chassis pivot geometry.
* **Diagnostic Procedure**:
  1. In [`uno_motor_control.ino`](file:///c:/Users/Kananyan/Desktop/MED_WASTE_BOT/uno_motor_control.ino#L678), verify:
     * When backing up and left sensors (`S1/S2`) detect line: left motor drives backward fast, right motor drives backward slow (`setMotor(-SHARP_FAST, -SHARP_SLOW)`).
* **Resolution**:
  1. Verify the discrete reverse logic matches [`runLineFollowingBackward()`](file:///c:/Users/Kananyan/Desktop/MED_WASTE_BOT/uno_motor_control.ino#L678-L716).

---

### Issue 31: Robot stops immediately right after departing from the base station
* **Symptom**: When `MOVE_TO_DISPOSAL` is triggered, the robot moves 1 cm and stops, falsely reporting `ARRIVED_DISPOSAL`.
* **Root Cause**: The robot started on the all-black base pad, and the auto-stop condition triggered before the robot cleared the pad.
* **Diagnostic Procedure**:
  1. Check `hasLeftStartZone` variable in [`uno_motor_control.ino`](file:///c:/Users/Kananyan/Desktop/MED_WASTE_BOT/uno_motor_control.ino#L313).
* **Resolution**:
  1. The firmware includes an 800 ms minimum deadband:
     `!isAllSensorsBlackZero() && (currentMillis - stateStartTime >= LEAVE_START_MIN_MS)`
  2. Ensure `LEAVE_START_MIN_MS = 800` in [`uno_motor_control.ino`](file:///c:/Users/Kananyan/Desktop/MED_WASTE_BOT/uno_motor_control.ino#L31).

---

## 5. Ultrasonic Sensors (Front Obstacle & Top Bin Depth)

### Issue 32: Front HC-SR04 returns -1 cm or erratic out-of-range readings
* **Symptom**: Distance display shows `-1 cm` or `no echo` even when an obstacle is 20 cm in front.
* **Root Cause**: Trig/Echo pins swapped, echo wire loose, or sensor running at 3.3V instead of 5V.
* **Diagnostic Procedure**:
  1. Check wiring: `TRIG` must connect to Uno Pin D8; `ECHO` must connect to Uno Pin D9.
  2. Measure `VCC` pin of HC-SR04: standard HC-SR04 requires a full 5V supply and fails to oscillate at 3.3V.
* **Resolution**:
  1. Connect HC-SR04 `VCC` strictly to the 5V rail.
  2. Verify implementation of [`getDistance()`](file:///c:/Users/Kananyan/Desktop/MED_WASTE_BOT/uno_motor_control.ino#L821):
     `long duration = pulseIn(ECHO_PIN, HIGH, 30000UL);`

---

### Issue 33: Hands-free lid opening does not trigger when approaching front (< 50 cm)
* **Symptom**: Stepping in front of the robot while parked does not open the lid.
* **Root Cause**: Uno is in `MANUAL_OVERRIDE` mode instead of `IDLE`, or `personThreshold` is set too short.
* **Diagnostic Procedure**:
  1. Open Serial Monitor on Uno: observe the telemetry line:
     `[UNO TELEMETRY] St:IDLE | Dist:35cm | Obstacle:NO`
  2. If `St:MANUAL`, proximity is disabled. Click **Switch to Auto Mode** in web dashboard.
* **Resolution**:
  1. In the web dashboard under **🎚️ Calibration**, increase **Person detect range** to **60 cm–80 cm**.
  2. Ensure the bin is not full (`unitTrashLevel != "Full"`).

---

### Issue 34: Top bin depth sensor reads 0% full when the container is empty
* **Symptom**: Web dashboard shows `0 cm (100% full)` even when there is zero garbage in the bin.
* **Root Cause**: Top ultrasonic sensor echo bounced off the internal bin sidewalls instead of the floor.
* **Diagnostic Procedure**:
  1. Look down into the bin: is the HC-SR04 mounted centered in the lid, pointing straight down?
  2. If angled, ultrasonic sound waves bounce off the sidewall and return an immediate short echo.
* **Resolution**:
  1. Mount the HC-SR04 squarely perpendicular to the bin floor.
  2. Ensure internal garbage bag liner is pulled smooth against the walls so folds do not block the beam.

---

### Issue 35: Full bin lockout triggers prematurely when waste is only half full
* **Symptom**: Proximity lid opening stops working when the bin is only 50% loaded.
* **Root Cause**: `trashFullThreshold` parameter is set too conservatively (e.g. 20 cm instead of 10 cm).
* **Diagnostic Procedure**:
  1. Check **Trash full threshold** under Calibration on web dashboard.
* **Resolution**:
  1. Adjust `trashFullThreshold` to **10 cm** (bin is only declared full when garbage reaches within 10 cm of the top sensor).
  2. Click **💾 Save & Apply Calibration**.

---

### Issue 36: Robot triggers emergency obstacle stops on completely clear floors
* **Symptom**: While line following, robot suddenly halts and Serial Monitor prints `[OBSTACLE ALARM] Obstacle detected!`.
* **Root Cause**: Front HC-SR04 is tilted slightly downward, detecting the floor surface as an obstacle.
* **Diagnostic Procedure**:
  1. Check front sensor angle: must be strictly horizontal or tilted $2^\circ–3^\circ$ slightly upward.
* **Resolution**:
  1. Shim or adjust the front ultrasonic mounting bracket so it does not point toward the ground.
  2. In dashboard under Calibration, decrease **Obstacle stop distance** from 30 cm to **20 cm**.

---

### Issue 37: Ultrasonic pulseIn measurement blocks motor control loop timing
* **Symptom**: Motors stutter or hesitate every 100 ms during line tracking.
* **Root Cause**: `pulseIn(ECHO_PIN, HIGH)` without a timeout parameter blocks CPU execution for up to 1000 ms on lost echo.
* **Diagnostic Procedure**:
  1. Check pulseIn calls in firmware.
* **Resolution**:
  1. Always provide an explicit 30 ms timeout: `pulseIn(ECHO_PIN, HIGH, 30000UL)`.
  2. In [`uno_motor_control.ino`](file:///c:/Users/Kananyan/Desktop/MED_WASTE_BOT/uno_motor_control.ino#L235), distance is read non-blockingly on a dedicated 100 ms timer (`currentMillis - lastDistanceTime >= 100`) so the 50 Hz motor PID loop is never delayed.

---

## 6. Serial Communication (Uno ⟷ ESP32 / NodeMCU)

### Issue 38: Web dashboard displays "Uno Disconnected" or "-" for all telemetry
* **Symptom**: Dashboard shows Wi-Fi connected, but all motor telemetry, IR sensor bars, and battery badges show `-`.
* **Root Cause**: Dedicated UART TX/RX wires are swapped, disconnected, or lack common ground.
* **Diagnostic Procedure**:
  1. Check physical wiring between Uno and Controller:
     * **Unit 1 (ESP32-C3)**: Uno D11 (TX) $\rightarrow$ ESP32 GPIO 2 (RX); Uno D10 (RX) $\leftarrow$ ESP32 GPIO 3 (TX).
     * **Unit 2 (NodeMCU)**: Uno D11 (TX) $\rightarrow$ NodeMCU Pin D5 (RX); Uno D10 (RX) $\leftarrow$ NodeMCU Pin D7 (TX).
  2. Open the web dashboard **💻 Serial Terminals** tab. If no `STATUS,...` packets appear, UART is silent.
* **Resolution**:
  1. Connect a jumper wire between Arduino Uno `GND` and ESP32/NodeMCU `GND`.
  2. If using a 5V-to-3.3V divider on ESP32 RX, verify resistor values (1k $\Omega$ series, 2k $\Omega$ pull-down to GND).

---

### Issue 39: Telemetry stream displays corrupted or garbled ASCII characters
* **Symptom**: Serial terminal shows `STTUS,15,??...` instead of clean comma-separated values.
* **Root Cause**: Baud rate mismatch between Uno `SoftwareSerial` (9600 baud) and Controller.
* **Diagnostic Procedure**:
  1. Check `Serial1.begin(9600)` in [`master_esp32s2_webserver.ino`](file:///c:/Users/Kananyan/Desktop/MED_WASTE_BOT/master_esp32s2_webserver.ino#L2651).
  2. Check `espSerial.begin(9600)` in [`uno_motor_control.ino`](file:///c:/Users/Kananyan/Desktop/MED_WASTE_BOT/uno_motor_control.ino#L131).
* **Resolution**:
  1. Both microcontrollers must communicate at strictly **9600 baud, 8 data bits, no parity, 1 stop bit (8N1)**.

---

### Issue 40: D-Pad or buttons clicked in web dashboard do not execute on Uno
* **Symptom**: Dashboard logs show command sent (`[MEDWASTE CONTROL 1 -> UNO TX] MAN_FWD`), but Uno does not move.
* **Root Cause**: Uno `espSerial.listen()` was not active, or Uno RX buffer had framing noise.
* **Diagnostic Procedure**:
  1. Open Uno USB Serial Monitor at 9600 baud while sending commands from the web UI.
  2. Does Uno print `[COMMAND RECEIVED] 'MAN_FWD'`? If yes, command arrived; check motor driver. If no, check TX wire from ESP32 GPIO 3 to Uno Pin D10.
* **Resolution**:
  1. Verify Uno pin `D10` is connected to ESP32 `GPIO 3` (or NodeMCU `Pin D7`).
  2. Ensure [`uno_motor_control.ino`](file:///c:/Users/Kananyan/Desktop/MED_WASTE_BOT/uno_motor_control.ino#L803) calls `espSerial.listen()` right after sending telemetry.

---

### Issue 41: Noticeable 1–2 second latency between UI button click and robot movement
* **Symptom**: Pressing FWD on web dashboard takes 1.5 seconds before the robot reacts.
* **Root Cause**: Web server was executing blocking Flash writes (`saveCalibrationPrefs()`) on every command, or Wi-Fi power-save sleep was active.
* **Resolution**:
  1. In the latest firmware, [`saveCalibrationPrefs()`](file:///c:/Users/Kananyan/Desktop/MED_WASTE_BOT/master_esp32s2_webserver.ino#L2900) has been removed from rapid manual commands.
  2. Verify Wi-Fi power-save sleep is permanently disabled in `setup()`:
     * ESP32: `WiFi.setSleep(false);`
     * NodeMCU: `WiFi.setSleepMode(WIFI_NONE_SLEEP);`

---

### Issue 42: SoftwareSerial buffer overflow crashes communications under load
* **Symptom**: Communication works for 2 minutes then freezes completely until Uno reset button is pressed.
* **Root Cause**: `SoftwareSerial` default buffer size is only 64 bytes. If `loop()` stalls, buffer overflows and drops sync.
* **Resolution**:
  1. In [`uno_motor_control.ino`](file:///c:/Users/Kananyan/Desktop/MED_WASTE_BOT/uno_motor_control.ino#L190-L205), incoming serial commands are drained in a fast non-blocking `while (espSerial.available() > 0)` loop on every iteration.
  2. Telemetry is throttled to exactly once every 500 ms to guarantee ample quiet-window time for incoming commands.

---

### Issue 43: Uno cannot upload new firmware while connected to ESP32 TX/RX
* **Symptom**: Flashing Uno from Arduino IDE fails with `avrdude: stk500_recv(): programmer is not responding`.
* **Root Cause**: If ESP32 was accidentally wired to Uno hardware serial pins `D0` and `D1`, the ESP32 conflicts with the USB-to-UART programmer IC (CH340 / ATmega16U2).
* **Resolution**:
  1. The MedWaste Bot specifically uses **dedicated SoftwareSerial pins D10 (RX) and D11 (TX)** on the Arduino Uno!
  2. **Pins D0 and D1 remain 100% unconnected**, allowing you to program the Uno and view the USB Serial Monitor freely without ever unplugging wires.

---

## 7. Wi-Fi, AP Hotspot, Web Server & mDNS

### Issue 44: Fallback Wi-Fi Hotspot (MedWaste-1-AP / MedWaste-2-AP) does not appear
* **Symptom**: Scanning Wi-Fi networks on phone or laptop does not show `MedWaste-1-AP` or `MedWaste-2-AP`.
* **Root Cause**: ESP32/NodeMCU failed during cold boot, or antenna trace is obstructed by metal bin chassis.
* **Diagnostic Procedure**:
  1. Open Serial Monitor on ESP32 at 115200 baud during power-on.
  2. Confirm whether `[WIFI] Hotspot AP Broadcast Active: 'MedWaste-1-AP'` appears.
* **Resolution**:
  1. Verify the board runs in concurrent AP+Station mode: `WiFi.mode(WIFI_AP_STA);`.
  2. Mount the ESP32/NodeMCU antenna clear of metallic surfaces or lead batteries.

---

### Issue 45: Connected to AP Wi-Fi but http://192.168.4.1 displays a blank page
* **Symptom**: Device connects to `MedWaste-1-AP` (IP 192.168.4.2), but browser shows white screen or connection refused.
* **Root Cause**: Modern smartphones enable "Mobile Data / Cellular Assist", routing `192.168.4.1` out to cellular network because the robot hotspot has no internet.
* **Resolution**:
  1. On your smartphone, temporarily **turn OFF Mobile Data (Cellular)**.
  2. When prompted "This network has no internet access, stay connected?", tap **Yes / Stay Connected**.
  3. Navigate to **`http://192.168.4.1`** in Chrome, Firefox, or Safari.

---

### Issue 46: Local mDNS URL (http://medwaste1.local) fails to resolve on client device
* **Symptom**: Browser connects via `http://192.168.4.1`, but `http://medwaste1.local` fails to load.
* **Root Cause**: Some Android versions do not support multicast DNS (mDNS / Zeroconf) natively without Bonjour.
* **Resolution**:
  1. On Android: Use the direct IP address (`http://192.168.4.1` on AP, or the router IP assigned in Station mode).
  2. On Windows / iOS / macOS: Ensure Bonjour Service is active; `http://medwaste1.local` will resolve natively.

---

### Issue 47: Controller disconnects from facility Wi-Fi and fails to auto-reconnect
* **Symptom**: When robot moves to far wing of clinic, it drops hospital router Wi-Fi and never reconnects.
* **Root Cause**: Default Arduino Wi-Fi auto-reconnect can freeze when signal is lost abruptly.
* **Resolution**:
  1. In [`master_esp32s2_webserver.ino`](file:///c:/Users/Kananyan/Desktop/MED_WASTE_BOT/master_esp32s2_webserver.ino#L3095-L3105), a non-blocking background connection monitor attempts reconnection every 15 seconds without blocking robot movement or manual driving.
  2. The fallback AP hotspot remains active 100% of the time, allowing immediate local control even when completely outside facility router range.

---

### Issue 48: Web dashboard touch controls feel unresponsive on iOS or Android
* **Symptom**: Tapping buttons triggers phone page zooming, text selection, or pull-to-refresh instead of driving.
* **Resolution**:
  1. In the latest firmware, CSS rules have been updated:
     ```css
     .dpad button {
       touch-action: none;
       user-select: none;
       -webkit-user-select: none;
     }
     ```
  2. This locks out browser swipe gestures and guarantees instant touch-to-drive responsiveness.

---

## 8. Arduino IDE Compilation & Flashing Errors

### Issue 49: error: 'TILT_STEPS' was not declared in this scope
* **Symptom**: Arduino IDE compilation of `master_esp32s2_webserver.ino` fails with:
  `error: 'TILT_STEPS' was not declared in this scope` at line 2669.
* **Root Cause**: Legacy stepper motor variable was referenced in a setup diagnostic log message after stepper was replaced with the Servo motor on Pin 19.
* **Resolution**:
  1. Replace line 2669 in [`master_esp32s2_webserver.ino`](file:///c:/Users/Kananyan/Desktop/MED_WASTE_BOT/master_esp32s2_webserver.ino#L2669) with:
     ```cpp
     logESP("[SYSTEM] Tilt travel configured: " + String(UPRIGHT_REF_DEG, 0) + " deg to " + String(DUMP_TARGET_DEG, 0) + " deg (" + String(dumpCycles) + " tilt cycles, " + String(tiltStepDelayMs) + " ms/deg)");
     ```
  2. Synchronize the file with `C:\Users\Kananyan\Documents\Arduino\MASTER\MASTER.ino`.

---

### Issue 50: undefined reference to 'getDistance()' / ld returned 1 exit status
* **Symptom**: Arduino Uno compilation fails with:
  `undefined reference to getDistance()`
  `collect2.exe: error: ld returned 1 exit status`
* **Root Cause**: `long getDistance();` was forward-declared at the top of [`uno_motor_control.ino`](file:///c:/Users/Kananyan/Desktop/MED_WASTE_BOT/uno_motor_control.ino#L93) and called in `loop()`, but its function body implementation was missing from the file.
* **Resolution**:
  1. Append the full function body to the bottom of [`uno_motor_control.ino`](file:///c:/Users/Kananyan/Desktop/MED_WASTE_BOT/uno_motor_control.ino#L821):
     ```cpp
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
     ```
  2. Re-upload sketch to Uno.

---

### Issue 51: A fatal error occurred: Failed to connect to ESP32-C3
* **Symptom**: Uploading to ESP32-C3 times out with `No serial data received... Failed to connect to ESP32-C3`.
* **Root Cause**: The ESP32-C3 onboard USB-CDC port did not auto-enter flashing bootloader mode.
* **Resolution**:
  1. Press and **hold** the physical **BOOT** button on the ESP32-C3 board.
  2. Press and release the **RESET (RST)** button once.
  3. Release the **BOOT** button.
  4. Click **Upload** in the Arduino IDE. The board is now locked in ROM download mode and will flash smoothly.

---

### Issue 52: esp_camera.h: No such file or directory error during compilation
* **Symptom**: Arduino IDE fails when compiling a camera test sketch with missing camera header.
* **Root Cause**: Selected board is set to `ESP32 Dev Module` or `Arduino Uno` instead of `AI Thinker ESP32-CAM`.
* **Resolution**:
  1. Under **Tools $\rightarrow$ Board**, select **`AI-Thinker ESP32-CAM`**.
  2. Or if compiling the main MedWaste robot, ensure you have opened [`master_esp32s2_webserver.ino`](file:///c:/Users/Kananyan/Desktop/MED_WASTE_BOT/master_esp32s2_webserver.ino) and selected **`ESP32C3 Dev Module`**.

---

### Issue 53: NodeMCU ESP8266 throws fatal exception (28) or wdt reset watchdog loop
* **Symptom**: Serial Monitor on NodeMCU prints memory dump stack trace and keeps restarting every 2 seconds.
* **Root Cause**: A blocking delay or tight while-loop starved the ESP8266 background Wi-Fi and TCP stack, tripping the hardware watchdog.
* **Resolution**:
  1. In [`slave_esp32s2_client.ino`](file:///c:/Users/Kananyan/Desktop/MED_WASTE_BOT/slave_esp32s2_client.ino), all loops are 100% non-blocking using `millis()`.
  2. If using long loops, insert `yield();` or `delay(0);` to allow the ESP8266 Wi-Fi stack to service network packets.

---

### Issue 54: avrdude: stk500_recv(): programmer is not responding on Uno upload
* **Symptom**: Arduino IDE hangs at 90% upload and fails with `avrdude: stk500_recv()`.
* **Root Cause**: Wrong COM port selected, incorrect board selection, or USB cable is power-only (no data wires).
* **Diagnostic Procedure**:
  1. Open Windows **Device Manager** $\rightarrow$ **Ports (COM & LPT)**. Unplug and replug the Uno USB cable. Confirm which COM port appears and disappears.
* **Resolution**:
  1. Select the exact verified COM port under **Tools $\rightarrow$ Port**.
  2. Verify **Tools $\rightarrow$ Board** is set to **`Arduino Uno`** (Processor: ATmega328P).
  3. Use a high-quality, short USB data cable.
