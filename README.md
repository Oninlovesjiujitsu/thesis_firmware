# Smart Rainwater Harvesting System — Firmware

> **Emblematic** — *serving as a symbol of a particular quality or concept.*
>
> An intelligent, autonomous water treatment system designed to harvest, treat, and dispense rainwater for sustainable household use.

---

## Technologies

### Hardware & Microcontroller
| Technology | Purpose |
| :--- | :--- |
| **ESP32** | Main microcontroller handling WiFi, MQTT, and GPIO operations |
| **PlatformIO** | Build system and library management |
| **Arduino Framework** | Core execution and hardware abstraction |

### Control & Logic
| Technology | Purpose |
| :--- | :--- |
| **`br3ttb/PID`** | Time-Proportioned PID controller for exact pH dosing |
| **State Machine** | deterministic tracking of treatment cycles (Flush, Collect, Dose, Filter, Dispense) |

### Connectivity & Sensors
| Technology | Purpose |
| :--- | :--- |
| **AWS IoT Core** | MQTT over TLS (port 8883) for secure bidirectional communication |
| **`PubSubClient`** | Lightweight MQTT client for publishing telemetry and receiving commands |
| **`ArduinoJson`** | Parsing dynamic JSON command payloads (e.g., live PID tuning) |
| **Adafruit ADS1X15** | High-precision ADC for analog sensor readings (pH, Turbidity) |

---

## Features

### PID pH Regulation
A precise Time-Proportioned Proportional-Integral-Derivative (PID) controller replaces standard bang-bang logic. The system dynamically adjusts the ON-time of acid/base dosing relays within a 5-second control window to smoothly approach the target pH without overshooting.

### Robust State Machine
The treatment lifecycle is managed autonomously through distinct phases:
- **First Flush:** Diverts initial contaminated roof runoff.
- **Collecting:** Fills the primary treatment tank until the float switch triggers.
- **Dosing (PID):** Measures pH and actuates chemical dosing pumps to reach safe levels.
- **Filtering:** Pumps treated water through physical filters to the clean tank.
- **Turbidity Check:** Validates post-filtration clarity. Re-filters if NTU limits are exceeded.
- **Dispensing:** Makes clean water available for end-user consumption.

### Remote Configuration & Override
Through AWS IoT, the device accepts JSON-formatted MQTT payloads to:
- Dynamically tune the PID loop (`Kp`, `Ki`, `Kd`, `Setpoint`) without flashing new firmware.
- Pause, Resume, or Reset the entire state machine.
- Manually actuate individual pumps and solenoids for maintenance or calibration.

---

## The Process

### Design Phase
The project started with a clear constraint: *the system must be autonomous but entirely observable.* 
This led to the separation of the physical actuation layer (the ESP32) and the user interface layer (the mobile app). 

**Key design decisions:**
1. **MQTT for Telemetry:** Using MQTT over TLS ensures that sensor data can be streamed continuously to the cloud with low overhead, while maintaining high security.
2. **Time-Proportioned PID:** Standard PID outputs a continuous analog signal, but our dosing pumps are driven by simple ON/OFF relays. We introduced a time-proportioned conversion (mapping PID percentage to duty-cycle time) to bridge this gap.
3. **Debounced Safety Checks:** Float switches and rain sensors are heavily debounced to prevent turbulent water from causing rapid state toggling.

### Implementation Phases

| Phase | Focus | Status |
| :--- | :--- | :--- |
| **Phase 1** | Hardware bring-up — ADC calibration, relay switching, WiFi provisioning | Completed |
| **Phase 2** | AWS IoT Integration — TLS certificates, PubSubClient, telemetry publishing | Completed |
| **Phase 3** | State Machine — implementing the autonomous treatment lifecycle | Completed |
| **Phase 4** | PID Dosing Upgrade — migrating from bang-bang to Time-Proportional PID | Completed |

---

## What I Learned

- **TLS on ESP32:** Establishing an MQTT connection over TLS (MQTTS) requires careful memory management, as the SSL handshake consumes significant heap space.
- **Time-Proportional Control:** Translating continuous mathematical models (PID) into discrete mechanical actions (relay clicks) requires mapping outputs to fixed time windows (e.g., a 5000ms loop).
- **Non-blocking Delays:** Relying entirely on `millis()` instead of `delay()` is critical when the device must simultaneously read sensors, compute PID, and maintain an MQTT keep-alive ping.

---

## How It Can Be Improved

### Short-term
- **Over-the-Air (OTA) Updates:** Implementing ArduinoOTA would allow firmware patches to be pushed without physically connecting a USB cable to the waterproof enclosure.
- **Watchdog Timer (WDT):** Adding a hardware watchdog would ensure the ESP32 automatically reboots if the WiFi stack hangs indefinitely.

### Longer-term
- **Variable Speed Dosing:** Upgrading from relay-driven pumps to PWM-controllable peristaltic pumps would allow true continuous PID control, increasing dosing precision.
- **Local Fallback Mode:** If WiFi drops, the system currently continues autonomously, but the user loses visibility. A fallback Bluetooth Low Energy (BLE) server could allow local control without internet.

---

*Emblematic is built on the principle that sustainable water harvesting requires industrial-grade automation brought to the household level.*
