# ESP32 Smart Water Irrigation System

An ESP32-based IoT irrigation controller for monitoring **two separate soil areas** and controlling two independent water pumps. It combines Wi-Fi provisioning with WiFiManager, remote monitoring and control through Blynk, and remote device management through mDash.

> **Current setup:** two analog soil-moisture sensors (GPIO 34 and 35), with Pump 1 on GPIO 26 for Area 1 and Pump 2 on GPIO 27 for Area 2. Each sensor independently controls its matching pump in AUTO mode.


## System architecture diagram

The ESP32 reads both soil-moisture sensors and controls a separate relay/pump for each area. Readings and controls are exposed through Blynk; Wi-Fi is provisioned with WiFiManager and device management uses mDash.

```mermaid
flowchart TD
    S1["Area 1 soil sensor<br/>AO → GPIO 34"] --> ESP["ESP32 controller"]
    S2["Area 2 soil sensor<br/>AO → GPIO 35"] --> ESP
    WM["WiFiManager<br/>Wi-Fi provisioning"] <--> ESP
    ESP <--> BK["Blynk cloud/app<br/>Moisture · ADC · Pump · Mode"]
    ESP <--> MD["mDash<br/>Device management / FOTA"]
    ESP --> R1["Relay 1<br/>GPIO 26"]
    R1 --> P1["Pump 1 · Area 1"]
    ESP --> R2["Relay 2<br/>GPIO 27"]
    R2 --> P2["Pump 2 · Area 2"]
    PS["Suitable external pump supplies"] --> P1
    PS --> P2
    G["Common ground for ESP32 and sensors"] --- S1
    G --- S2
```

## Wiring diagram

```mermaid
flowchart LR
    subgraph Sensors
      A["Area 1 sensor"] -- "AO" --> G34["ESP32 GPIO 34"]
      B["Area 2 sensor"] -- "AO" --> G35["ESP32 GPIO 35"]
      V["3.3 V / suitable sensor supply"] --> A
      V --> B
      GND["GND"] --- A
      GND --- B
    end
    subgraph Pump control
      G26["ESP32 GPIO 26"] --> IN1["Relay 1 IN"]
      G27["ESP32 GPIO 27"] --> IN2["Relay 2 IN"]
      RV["Relay supply as required"] --> REL["Two-channel relay or two relay modules"]
      IN1 --> REL
      IN2 --> REL
      REL --> CONTACTS1["Relay 1 COM/NO"]
      REL --> CONTACTS2["Relay 2 COM/NO"]
      EXT["Pump power supply 1"] --> CONTACTS1
      EXT2["Pump power supply 2"] --> CONTACTS2
      CONTACTS1 --> MOTOR1["Pump 1 · Area 1"]
      CONTACTS2 --> MOTOR2["Pump 2 · Area 2"]
    end
    EG["ESP32 GND"] --- GND
```

> **Wiring note:** The diagram is a functional overview, not a pin-by-pin relay terminal drawing. Use the relay module's documented VCC/GND/IN pins and COM/NO/NC markings. Keep the pump on its correctly rated external supply; never power it from an ESP32 GPIO. Ensure sensor output voltage stays within ESP32 input limits. Share ground where required by the sensor/driver design.

## Choose a firmware variant

Two standalone Arduino sketches are available. Open **only one sketch folder at a time** in Arduino IDE.

| Variant | Sketch | Sensor wiring | Blynk moisture pins |
|---|---|---|---|
| One sensor | [ESP32-Irrigation-1-Sensor.ino](ESP32-Irrigation-1-Sensor/ESP32-Irrigation-1-Sensor.ino) | One sensor AO → GPIO 34 | V0 = moisture, V6 = raw ADC |
| Two sensors | [ESP32-Irrigation-2-Sensors.ino](ESP32-Irrigation-2-Sensors/ESP32-Irrigation-2-Sensors.ino) | Area 1 AO → GPIO 34; Area 2 AO → GPIO 35 | V0/V6 = Area 1; V7/V8 = Area 2 |

The two-sensor variant uses GPIO 26 for Pump 1 and GPIO 27 for Pump 2. Blynk V3/V4 are Pump 1 status/manual control; V9/V10 are Pump 2 status/manual control; V5 selects AUTO/MANUAL mode. The one-sensor variant still uses GPIO 26 and V3/V4 for its single pump. Both variants use WiFiManager, Blynk, and mDash integration.

For either variant, create a private `secrets.h` in that variant's folder using the example in the setup section below. The file is ignored by Git. Calibrate the dry/wet ADC constants for the sensor or sensors you actually use.

## Features

- Wi-Fi setup through WiFiManager; no Wi-Fi credentials hard-coded in the firmware
- Two soil-moisture sensors sampled and reported separately
- 100 ADC samples per measurement, collected without a long blocking delay
- Blynk dashboard readings for moisture, raw ADC values, pump status, and control mode
- Manual pump control and automatic irrigation mode
- Separate start/stop moisture thresholds (hysteresis) to reduce relay chatter
- mDash device management and firmware-update integration
- Built-in Mongoose TLS configuration
- Local secrets file excluded from Git

## Hardware

### Required for the current build

- ESP32 Dev Module
- 2 analog soil-moisture sensors compatible with the ESP32's 3.3 V logic
- A two-channel relay module or two appropriately rated pump drivers
- 2 DC water pumps
- Suitable external power supply or supplies for both pumps
- Tubing and connecting wires
- USB cable/power supply for the ESP32

### Wiring

| Component | ESP32 connection | Notes |
|---|---|---|
| Area 1 moisture sensor AO | GPIO 34 | Analog input |
| Area 2 moisture sensor AO | GPIO 35 | Analog input |
| Both sensor VCC | Suitable sensor supply | Use 3.3 V when required by the sensor; keep output within ESP32 ADC limits |
| Both sensor GND | ESP32 GND | Common ground for sensor readings |
| Pump 1 relay input | GPIO 26 | Area 1; firmware defaults to active-LOW logic |
| Pump 2 relay input | GPIO 27 | Area 2; firmware defaults to active-LOW logic |
| Relay/pump contacts | Pump supply circuit | Follow the relay module ratings and pump supply requirements |

**Safety:** Never power the pump from an ESP32 GPIO. Switch the pump using a correctly rated relay/driver and an appropriate external supply. Check the relay's active level before connecting the pump. Do not apply 5 V to an ESP32 GPIO or ADC input.

## Pin configuration

| GPIO | Function |
|---:|---|
| 34 | Area 1 soil-moisture analog input |
| 35 | Area 2 soil-moisture analog input |
| 26 | Pump 1 relay control (Area 1) |
| 27 | Pump 2 relay control (Area 2) |

The firmware uses 12-bit ADC readings and `ADC_11db` attenuation. ADC values and moisture percentages depend on the sensor and the soil; calibrate each sensor independently. Both pumps start OFF, and automatic control waits until the first complete sensor measurement before making a decision.

## Blynk setup

Create a Blynk template/device and add datastreams matching these virtual pins:

| Virtual pin | Function | Suggested data type |
|---|---|---|
| V0 | Area 1 moisture (%) | Double |
| V3 | Pump 1 status (0 = OFF, 1 = ON) | Integer |
| V4 | Pump 1 manual command (0 = OFF, 1 = ON) | Integer |
| V5 | Mode (0 = MANUAL, 1 = AUTO) | Integer |
| V6 | Area 1 raw ADC average | Integer |
| V7 | Area 2 moisture (%) | Double |
| V8 | Area 2 raw ADC average | Integer |
| V9 | Pump 2 status (0 = OFF, 1 = ON) | Integer |
| V10 | Pump 2 manual command (0 = OFF, 1 = ON) | Integer |

Suggested widgets:
- Gauges or value displays for V0 and V7
- Value displays for V6 and V8 when calibrating
- Switch for V4
- Switch for V5
- LED/value displays for V3 and V9
- Separate Switch widgets for V4 and V10

The firmware synchronizes V5 (AUTO/MANUAL mode) when Blynk reconnects and resets the V4/V10 manual widgets to OFF. It intentionally does not restore old manual ON commands, preventing stale cloud switch states from unexpectedly starting a pump. Manual pump commands are ignored while AUTO mode is active.

## Automatic irrigation behavior

Default settings in `ESP32-Irrigation.ino`:

```cpp
#define PUMP_ON_THRESHOLD  30
#define PUMP_OFF_THRESHOLD 60
```

The firmware calculates moisture as a percentage from each sensor's dry and wet ADC calibration values. With the default calibration, a higher ADC reading means drier soil and a lower reading means wetter soil.

- **Pump 1 starts:** when AUTO mode is enabled, Pump 1 is OFF, and Area 1 reads at or below 30%.
- **Pump 1 stops:** when Pump 1 is ON and Area 1 reaches at least 60%.
- **Pump 2 starts:** when AUTO mode is enabled, Pump 2 is OFF, and Area 2 reads at or below 30%.
- **Pump 2 stops:** when Pump 2 is ON and Area 2 reaches at least 60%.
- **Run-time failsafe:** each pump is stopped and locked out after 10 minutes of continuous operation; adjust `MAX_PUMP_RUNTIME_MS` to suit your setup. The lockout remains until the ESP32 is rebooted, so investigate the cause before restarting.
- **Automatic restart cooldown:** after a pump stops, automatic mode waits at least 30 seconds before restarting it, reducing rapid cycling. Manual operation remains available in MANUAL mode.
- **Sensor rail fault protection:** if a sensor's averaged ADC reading stays at or below 20, or at or above 4075, for three consecutive measurements, its matching pump is stopped and locked out until reboot. These limits catch likely disconnected/shorted sensor readings; validate them for your sensor hardware.
- **MANUAL mode:** V4 controls Pump 1 and V10 controls Pump 2 independently.
- **Startup:** both pumps are initialized OFF.

These thresholds are starting values; tune them for your sensors, soil, and plants. Each area has its own pump, so the two areas can be watered independently.

## Sensor calibration

The firmware currently defines these starting values:

```cpp
#define SOIL1_DRY_ADC 1103
#define SOIL1_WET_ADC 1030
#define SOIL2_DRY_ADC 1103
#define SOIL2_WET_ADC 1030
```

Sensor 1's values are based on initial readings. Sensor 2's values are placeholders and must be measured independently.

1. Upload the firmware and open Serial Monitor at **115200 baud**.
2. Record the raw ADC value for each sensor in dry soil/air as appropriate for your intended calibration method.
3. Record the raw ADC value for each sensor in thoroughly wet soil.
4. Update that sensor's `DRY_ADC` and `WET_ADC` constants.
5. Confirm that the reported percentage rises as the soil gets wetter before enabling automatic mode.

The firmware averages 100 samples per measurement. Calibration readings can vary with sensor type, supply voltage, soil composition, and sensor aging.

## Software requirements

- Arduino IDE 2.x
- ESP32 board support package by Espressif Systems
- WiFiManager
- Blynk library for ESP32
- mDash library and its compatible Mongoose dependency/configuration
- ArduinoJson if required by the installed library versions

The project includes Mongoose configuration headers with ESP32 and built-in TLS settings. Library versions can affect compilation and cloud connectivity.

## Setup

### 1. Clone the repository

```bash
git clone https://github.com/abhijarvis/water-irrigation.git
cd water-irrigation
```

### 2. Create your local secrets file

Create `ESP32-Irrigation/secrets.h` next to `ESP32-Irrigation.ino`. This file is intentionally excluded from Git by `.gitignore`. Add your own Blynk template/device credentials and mDash device password using the macro names expected by your installed Blynk and mDash libraries. For example:

```cpp
#pragma once

#define BLYNK_TEMPLATE_ID   "YOUR_TEMPLATE_ID"
#define BLYNK_TEMPLATE_NAME "ESP32 Irrigation"
#define BLYNK_AUTH_TOKEN    "YOUR_BLYNK_AUTH_TOKEN"
#define MDASH_DEVICE_PASSWORD "YOUR_MDASH_DEVICE_PASSWORD"
```

Keep this file private. Never commit real Wi-Fi passwords, Blynk tokens, mDash passwords, or other credentials.

### 3. Open and configure the sketch

Open `ESP32-Irrigation/ESP32-Irrigation.ino` in Arduino IDE. Install the required libraries and select **ESP32 Dev Module**.

### 4. Upload and provision Wi-Fi

Upload the sketch and open Serial Monitor at **115200 baud**. If Wi-Fi has not been configured, the ESP32 starts a WiFiManager access point named `ESP32-Irrigation`. Connect to it and select your Wi-Fi network. The configuration portal timeout is 180 seconds.

### 5. Configure the Blynk dashboard

Create the datastreams listed above and add widgets for both moisture readings, pump status, manual control, and AUTO/MANUAL mode.

### 6. Verify before automatic operation

Check both sensors' raw readings and calibration first. Test relay operation without risking dry running or flooding, and verify that the relay's active level matches `RELAY_ACTIVE_LOW` in the firmware.

## mDash and TLS

The project calls `mDashBegin(MDASH_DEVICE_PASSWORD)` and includes `mongoose_config.h` with:

```cpp
#define MG_ARCH MG_ARCH_ESP32
#define MG_TLS MG_TLS_BUILTIN
#define MG_LOG_LEVEL 0
```

The documented Arduino mDash client maintains its TLS session and reconnects internally after `mDashBegin()`; its documented API does not require a separate `mDashLoop()`/poll call. The firmware keeps `loop()` non-blocking and yields to the ESP32 scheduler. mDash connectivity and firmware-update behavior still depend on compatible mDash/Mongoose versions and the correct device password. Confirm live connectivity in the mDash console; TLS settings alone do not prove that a cloud session is healthy.

## Project structure

```text
water-irrigation/
├── .gitignore
├── README.md
├── ESP32-Irrigation/
│   ├── ESP32-Irrigation.ino   # main two-sensor firmware
│   ├── mongoose_config.h
│   └── secrets.h              # local file; not committed
├── ESP32-Irrigation-1-Sensor/
│   └── ESP32-Irrigation-1-Sensor.ino
└── ESP32-Irrigation-2-Sensors/
    └── ESP32-Irrigation-2-Sensors.ino
```

## Troubleshooting

### Wi-Fi setup fails
- Restart the ESP32 and look for the `ESP32-Irrigation` configuration network.
- Re-enter the Wi-Fi credentials.
- Check Serial Monitor at 115200 baud.

### Blynk does not connect or values do not update
- Check the template ID, template name, auth token, and internet connection.
- Verify the virtual-pin datastreams and widget assignments.
- Confirm the device is using the same Blynk template as the app dashboard.

### Moisture percentage looks incorrect
- Compare raw ADC readings on V6 and V8 or in Serial Monitor.
- Calibrate each sensor separately; do not assume both sensors have identical dry/wet values.
- Confirm sensor output voltage is safe for the ESP32.

### Pump/relay does not switch
- Verify GPIO 26 for Pump 1 and GPIO 27 for Pump 2, relay supply, pump supplies, and relay trigger polarity.
- If a pump timed out or its sensor shows repeated ADC rail readings, that pump is locked out until reboot. Check sensor wiring/calibration, water supply, tubing, and pump before restarting.
- The default is active-LOW (`RELAY_ACTIVE_LOW true`). Change it only after verifying the module's logic.
- Never connect the pump directly to an ESP32 GPIO.

### mDash TLS or connectivity problems
- Confirm that the installed mDash/Mongoose library versions are compatible.
- Verify the device password and network connectivity.
- Review Serial Monitor output and the mDash device status; TLS configuration alone is not a connectivity test.

## Development status

| Component | Status |
|---|---|
| WiFiManager provisioning | Implemented in firmware |
| Blynk integration and controls | Implemented in firmware; dashboard must be configured |
| Two-sensor measurement | Implemented in firmware; hardware calibration required |
| Independent two-pump automatic logic | Implemented in the two-sensor firmware; validate thresholds and wiring before use |
| Relay/pump hardware | Depends on your wiring and testing |
| mDash integration | Present in firmware; verify live connectivity and FOTA on your device |

## Author

**Abhishek Pandit**

ESP32 · Embedded Systems · IoT · Automation

---

If you find this project useful, consider starring the repository.
