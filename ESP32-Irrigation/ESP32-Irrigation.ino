/*
 * ESP32 Water Irrigation Controller
 * WiFiManager + Blynk + mDash
 *
 * Soil moisture sensor + relay/pump control
 *
 * Copyright (c) 2026 Abhishek Pandit
 */

#include "secrets.h"

#define BLYNK_PRINT Serial

#include <WiFi.h>
#include <WiFiManager.h>
#include <BlynkSimpleEsp32.h>
#include <mDash.h>

BlynkTimer timer;

// -------------------- Hardware --------------------
#define SOIL_SENSOR_PIN 34
#define PUMP_PIN        26

// Most common relay modules are active LOW.
// If your relay works in the opposite way, change this to false.
#define RELAY_ACTIVE_LOW true

// -------------------- Irrigation settings --------------------
// These are initial values. Calibrate the sensor and update these
// values using the raw ADC readings printed to Serial Monitor.
#define SOIL_DRY_ADC  3000
#define SOIL_WET_ADC  1200

#define PUMP_ON_THRESHOLD  30
#define PUMP_OFF_THRESHOLD 60

// -------------------- Runtime variables --------------------
float soilMoisture = 0.0;
int soilRaw = 0;
bool pumpState = false;
bool autoMode = false;

// Convert logical pump state to the electrical relay signal.
void setPump(bool on) {
  pumpState = on;

  if (RELAY_ACTIVE_LOW) {
    digitalWrite(PUMP_PIN, on ? LOW : HIGH);
  } else {
    digitalWrite(PUMP_PIN, on ? HIGH : LOW);
  }

  Serial.print("Pump: ");
  Serial.println(on ? "ON" : "OFF");
}

// Read and average the soil sensor to reduce ADC noise.
int readSoilRaw() {
  const int samples = 10;
  long total = 0;

  for (int i = 0; i < samples; i++) {
    total += analogRead(SOIL_SENSOR_PIN);
    delay(5);
  }

  return total / samples;
}

// Convert the calibrated raw ADC value to a 0-100% moisture value.
float calculateMoisture(int raw) {
  float percentage =
      ((float)(SOIL_DRY_ADC - raw) * 100.0) /
      (float)(SOIL_DRY_ADC - SOIL_WET_ADC);

  return constrain(percentage, 0.0, 100.0);
}

void controlAutomaticIrrigation() {
  if (!autoMode) {
    return;
  }

  // Hysteresis prevents rapid relay switching.
  if (!pumpState && soilMoisture <= PUMP_ON_THRESHOLD) {
    setPump(true);
    Serial.println("Automatic irrigation: soil is dry.");
  }
  else if (pumpState && soilMoisture >= PUMP_OFF_THRESHOLD) {
    setPump(false);
    Serial.println("Automatic irrigation: soil moisture is sufficient.");
  }
}

void sendSensorData() {
  soilRaw = readSoilRaw();
  soilMoisture = calculateMoisture(soilRaw);

  Blynk.virtualWrite(V0, soilMoisture);
  Blynk.virtualWrite(V3, pumpState ? 1 : 0);
  Blynk.virtualWrite(V6, soilRaw);

  controlAutomaticIrrigation();

  Serial.println("\n----- Irrigation Data -----");
  Serial.printf("Soil ADC     : %d\n", soilRaw);
  Serial.printf("Soil Moisture: %.1f %%\n", soilMoisture);
  Serial.print("Pump         : ");
  Serial.println(pumpState ? "ON" : "OFF");
  Serial.print("Mode         : ");
  Serial.println(autoMode ? "AUTO" : "MANUAL");
  Serial.println("---------------------------");
}

// V4 = Manual pump control
BLYNK_WRITE(V4) {
  if (autoMode) {
    Serial.println("Manual pump command ignored: AUTO mode is active.");
    return;
  }

  setPump(param.asInt() != 0);
}

// V5 = Auto/Manual mode
// 0 = MANUAL
// 1 = AUTO
BLYNK_WRITE(V5) {
  autoMode = param.asInt() != 0;

  Serial.print("Irrigation mode changed to: ");
  Serial.println(autoMode ? "AUTO" : "MANUAL");

  if (autoMode) {
    // Immediately evaluate the current moisture level.
    controlAutomaticIrrigation();
  }
}

// V6 = raw soil ADC value

BLYNK_CONNECTED() {
  Serial.println("Blynk connected!");
  Blynk.syncVirtual(V4, V5);
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n==============================");
  Serial.println("ESP32 Water Irrigation");
  Serial.println("==============================");

  // ESP32 ADC configuration.
  analogReadResolution(12);
  analogSetPinAttenuation(SOIL_SENSOR_PIN, ADC_11db);

  pinMode(PUMP_PIN, OUTPUT);

  // Safe startup: pump OFF.
  if (RELAY_ACTIVE_LOW) {
    digitalWrite(PUMP_PIN, HIGH);
  } else {
    digitalWrite(PUMP_PIN, LOW);
  }

  WiFi.mode(WIFI_STA);
  WiFiManager wm;
  wm.setConfigPortalTimeout(180);

  Serial.println("Starting WiFiManager...");

  if (!wm.autoConnect("ESP32-Irrigation")) {
    Serial.println("WiFiManager failed. Restarting...");
    delay(3000);
    ESP.restart();
  }

  Serial.println("Wi-Fi connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  Serial.print("RSSI: ");
  Serial.print(WiFi.RSSI());
  Serial.println(" dBm");

  Serial.println("Connecting to Blynk...");
  Blynk.config(BLYNK_AUTH_TOKEN);

  if (Blynk.connect(10000)) {
    Serial.println("Blynk connected!");
  } else {
    Serial.println("Blynk connection failed.");
  }

  Serial.println("Starting mDash...");
  mDashBegin(MDASH_DEVICE_PASSWORD);
  Serial.println("mDash initialized.");

  // Read sensors every 2 seconds.
  timer.setInterval(2000L, sendSensorData);

  Serial.println("System ready.");
  Serial.println("Pump starts OFF.");
  Serial.println("Irrigation mode starts in MANUAL.");
}

void loop() {
  Blynk.run();
  timer.run();
  delay(10);
}
