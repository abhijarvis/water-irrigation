# ESP32 Smart Water Irrigation System

An IoT-based smart water irrigation controller built around the ESP32 for monitoring irrigation conditions, controlling a water pump, and providing remote device management.

## Features

- Wi-Fi provisioning with WiFiManager
- Real-time monitoring with Blynk
- Soil moisture monitoring
- Temperature monitoring
- Water-level monitoring
- Remote pump ON/OFF control
- mDash device management and FOTA
- TLS-enabled mDash communication
- Serial diagnostics
- Expandable sensor architecture

## System Architecture

```text
Sensors ──> ESP32 ──> Blynk Cloud
             │
             ├──> Pump / Relay
             │
             └──> mDash Cloud
                  Device Management / FOTA
```

### Software Architecture

```text
ESP32
 ├── WiFiManager
 │    └── Wi-Fi provisioning / configuration
 ├── Blynk
 │    ├── Sensor monitoring
 │    └── Pump control
 └── mDash
      ├── Device management
      └── FOTA
```

## Hardware

### Main Controller
- ESP32 Dev Module

### Planned / Supported Peripherals
- Soil moisture sensor
- Temperature sensor
- Water-level sensor
- Relay module or MOSFET pump driver
- DC water pump
- Suitable external pump power supply
- Optional status indicators

> **Important:** Never drive a water pump directly from an ESP32 GPIO. Use an appropriately rated relay, MOSFET driver, or motor driver with suitable protection.

## Pin Configuration

| ESP32 GPIO | Function | Direction |
|---:|---|---|
| GPIO 26 | Pump control | Output |

### Blynk Virtual Pins

| Virtual Pin | Function | Data Type |
|---|---|---|
| V0 | Soil moisture | Double |
| V1 | Temperature | Double |
| V2 | Water level | Integer |
| V3 | Pump status | Integer |
| V4 | Pump control | Integer |

The current firmware uses demo sensor values until the physical sensors are integrated:

- Soil moisture: 65%
- Temperature: 28.5°C
- Water level: 80%

## Blynk

Blynk provides live monitoring and remote pump control.

Pump control flow:

```text
Blynk App
   │ V4
   ▼
 ESP32
   │
   ▼
 GPIO 26
   │
   ▼
 Relay / MOSFET Driver
   │
   ▼
 Water Pump
```

The firmware uses `Blynk.config()` after WiFiManager establishes Wi-Fi, preventing Blynk from taking over Wi-Fi provisioning.

## WiFiManager

WiFiManager handles initial Wi-Fi configuration without hard-coding Wi-Fi credentials.

1. Power on the ESP32.
2. If credentials are unavailable, the ESP32 starts a configuration portal.
3. Connect to the `ESP32-Irrigation` access point.
4. Select the Wi-Fi network and enter its password.
5. ESP32 connects to the network.
6. Blynk and mDash are initialized.

The current configuration portal timeout is 180 seconds.

## mDash and FOTA

mDash is used for cloud-based device management and firmware updates.

The project uses:
- mDash device management
- WebSocket Secure (WSS)
- Built-in Mongoose TLS
- FOTA capability

The repository includes the Mongoose configuration used by the project:

```text
ESP32-Irrigation/
└── mongoose_config.h
```

Configuration:

```cpp
#define MG_ARCH MG_ARCH_ESP32
#define MG_TLS MG_TLS_BUILTIN
#define MG_LOG_LEVEL 0
```

`MG_LOG_LEVEL 0` suppresses verbose Mongoose/mDash diagnostic output while keeping the application's own Serial messages available.

## Software Requirements

- Arduino IDE 2.x
- ESP32 board package by Espressif Systems
- WiFiManager
- Blynk
- ArduinoJson
- mDash

The ESP32 core provides WiFi, WebServer, and Update functionality.

## Project Structure

```text
water-irrigation/
│
├── ESP32-Irrigation/
│   ├── ESP32-Irrigation.ino
│   └── mongoose_config.h
│
└── README.md
```

## Getting Started

### 1. Clone the repository

```bash
git clone https://github.com/abhijarvis/water-irrigation.git
cd water-irrigation
```

### 2. Open the firmware

Open `ESP32-Irrigation/ESP32-Irrigation.ino` in Arduino IDE.

### 3. Configure Blynk

Replace the placeholders with your Blynk credentials:

```cpp
#define BLYNK_TEMPLATE_ID   "YOUR_TEMPLATE_ID"
#define BLYNK_TEMPLATE_NAME "ESP32 Irrigation"
#define BLYNK_AUTH_TOKEN    "YOUR_BLYNK_AUTH_TOKEN"
```

### 4. Configure mDash

Set the appropriate device password:

```cpp
#define MDASH_DEVICE_PASSWORD "YOUR_MDASH_DEVICE_PASSWORD"
```

### 5. Select the board

Arduino IDE → Board → ESP32 Dev Module

### 6. Upload

Select the ESP32 USB serial port, compile, and upload the firmware.

### 7. Serial Monitor

Use 115200 baud.

Expected startup messages include:

```text
ESP32 Water Irrigation
Starting WiFiManager...
Wi-Fi connected!
Connecting to Blynk...
Blynk connected!
Starting mDash...
mDash initialized.
System ready.
```

## Security

**Never commit production credentials to GitHub.**

Do not commit:
- Blynk authentication tokens
- mDash passwords
- Wi-Fi passwords
- API keys
- Cloud credentials

The repository intentionally uses placeholders in the firmware.

## Development Status

| Component | Status |
|---|---|
| ESP32 firmware | 🟢 Implemented |
| WiFiManager | 🟢 Implemented |
| Blynk connection | 🟢 Implemented |
| Blynk pump control | 🟢 Implemented |
| Blynk dashboard | 🟢 Implemented with demo data |
| mDash integration | 🟢 Implemented |
| mDash TLS configuration | 🟢 Configured |
| Soil moisture hardware | 🟡 To be integrated |
| Temperature hardware | 🟡 To be integrated |
| Water-level hardware | 🟡 To be integrated |
| Pump hardware | 🟡 To be integrated |
| Automatic irrigation logic | 🟡 Planned |
| Sensor calibration | 🟡 Planned |

## Future Enhancements

- Automatic irrigation based on soil moisture
- Configurable irrigation schedules
- Minimum/maximum water-level protection
- Low-water and dry-soil alerts
- Historical sensor data
- Weather-aware irrigation
- Improved Blynk dashboard
- Remote firmware updates
- Pump dry-run protection
- Pump current monitoring
- Power/failure recovery handling
- Adaptive irrigation logic
- Water and energy consumption monitoring

## Troubleshooting

### ESP32 does not connect to Wi-Fi
1. Restart the ESP32.
2. Connect to the `ESP32-Irrigation` configuration network.
3. Re-enter Wi-Fi credentials.
4. Check Serial Monitor at 115200 baud.

### Blynk does not connect
Check:
- Blynk Template ID
- Blynk Template Name
- Blynk Auth Token
- Internet connectivity
- Blynk datastream configuration

The firmware uses `Blynk.config(BLYNK_AUTH_TOKEN)` and `Blynk.connect(10000)` because WiFiManager handles Wi-Fi.

### mDash shows TLS errors
Verify:

```cpp
#define MG_ARCH MG_ARCH_ESP32
#define MG_TLS MG_TLS_BUILTIN
```

Also verify that the installed mDash/Mongoose version is compatible with this configuration.

### Pump does not switch
Check:
- GPIO 26 wiring
- Relay/MOSFET driver power
- Common ground where required
- Pump power supply
- Relay trigger logic

Never connect the pump directly to GPIO 26.

## License and Copyright

Copyright © 2026 **Abhishek Pandit**.

This project is intended for personal development, experimentation, and IoT prototyping. Add an explicit open-source license before redistributing the project under an open-source license.

## Author

**Abhishek Pandit**

ESP32 • Embedded Systems • IoT • Automation

---

⭐ If you find this project useful, consider starring the repository.
