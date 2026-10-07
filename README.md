# ESP32 Water Irrigation

ESP32-based smart water irrigation controller using:

- WiFiManager for Wi-Fi provisioning
- Blynk for live monitoring and pump control
- mDash for device management / FOTA
- Mongoose TLS for secure mDash connectivity

## Blynk Virtual Pins

| Pin | Function |
|---|---|
| V0 | Soil moisture (%) |
| V1 | Temperature (°C) |
| V2 | Water level (%) |
| V3 | Pump status |
| V4 | Pump control |

## Hardware

- ESP32 Dev Module
- Pump driver/relay
- Soil moisture sensor
- Water-level sensor
- Optional temperature sensor

## Security

Do not commit real Blynk tokens or mDash passwords. Replace the placeholders locally or use a secrets mechanism.

## License

Copyright (c) 2026 Abhishek Pandit
