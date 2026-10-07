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

// ============================================================
// Hardware
// ============================================================

#define SOIL_SENSOR_PIN 34
#define PUMP_PIN        26

// Most common relay modules are active LOW.
// Change to false if your relay works in the opposite way.
#define RELAY_ACTIVE_LOW true


// ============================================================
// Soil Moisture Calibration
// ============================================================
//
// Dry soil : ~1103 ADC
// Wet soil : ~1030 ADC
//
// Higher ADC = drier
// Lower ADC  = wetter
//

#define SOIL_DRY_ADC 1103
#define SOIL_WET_ADC 1030

// Keep 100 samples per measurement.
// Sampling is NON-BLOCKING so mDash/Blynk remain responsive.
#define SOIL_ADC_SAMPLES 100
#define SOIL_SAMPLE_INTERVAL_MS 5


// ============================================================
// Irrigation Settings
// ============================================================

#define PUMP_ON_THRESHOLD  30
#define PUMP_OFF_THRESHOLD 60


// ============================================================
// Runtime Variables
// ============================================================

float soilMoisture = 0.0;
int soilRaw = 0;

bool pumpState = false;
bool autoMode = false;

// Non-blocking ADC sampler
bool samplingSoil = false;
int soilSampleCount = 0;
long soilSampleTotal = 0;
unsigned long lastSoilSampleMs = 0;


// ============================================================
// Pump Control
// ============================================================

void setPump(bool on) {

  pumpState = on;

  if (RELAY_ACTIVE_LOW) {
    digitalWrite(PUMP_PIN, on ? LOW : HIGH);
  } else {
    digitalWrite(PUMP_PIN, on ? HIGH : LOW);
  }

  // Immediately update Blynk pump status.
  Blynk.virtualWrite(V3, pumpState ? 1 : 0);

  Serial.print("Pump: ");
  Serial.println(on ? "ON" : "OFF");
}


// ============================================================
// Soil Moisture
// ============================================================

float calculateMoisture(int raw) {

  if (SOIL_DRY_ADC == SOIL_WET_ADC) {
    return 0.0;
  }

  float percentage =
      ((float)(SOIL_DRY_ADC - raw) * 100.0) /
      (float)(SOIL_DRY_ADC - SOIL_WET_ADC);

  return constrain(percentage, 0.0, 100.0);
}


// Start a new 100-sample measurement.
void startSoilSampling() {

  samplingSoil = true;
  soilSampleCount = 0;
  soilSampleTotal = 0;
  lastSoilSampleMs = millis();

  Serial.println("Starting soil measurement (100 samples)...");
}


// Take one ADC sample when its 5 ms interval has elapsed.
// This function never blocks.
void processSoilSampling() {

  if (!samplingSoil) {
    return;
  }

  unsigned long now = millis();

  if ((unsigned long)(now - lastSoilSampleMs) < SOIL_SAMPLE_INTERVAL_MS) {
    return;
  }

  lastSoilSampleMs = now;

  soilSampleTotal += analogRead(SOIL_SENSOR_PIN);
  soilSampleCount++;

  if (soilSampleCount >= SOIL_ADC_SAMPLES) {

    soilRaw = soilSampleTotal / SOIL_ADC_SAMPLES;
    soilMoisture = calculateMoisture(soilRaw);

    samplingSoil = false;

    // Automatic control after the complete measurement.
    controlAutomaticIrrigation();

    // Blynk virtual pins:
    // V0 = Soil moisture %
    // V3 = Pump status
    // V6 = Raw ADC

    Blynk.virtualWrite(V0, soilMoisture);
    Blynk.virtualWrite(V3, pumpState ? 1 : 0);
    Blynk.virtualWrite(V6, soilRaw);

    Serial.println();
    Serial.println("========== Irrigation Data ==========");

    Serial.print("Soil ADC (100 samples): ");
    Serial.println(soilRaw);

    Serial.print("Soil Moisture: ");
    Serial.print(soilMoisture, 1);
    Serial.println(" %");

    Serial.print("Pump         : ");
    Serial.println(pumpState ? "ON" : "OFF");

    Serial.print("Mode         : ");
    Serial.println(autoMode ? "AUTO" : "MANUAL");

    Serial.println("=====================================");
  }
}


// ============================================================
// Automatic Irrigation
// ============================================================

void controlAutomaticIrrigation() {

  if (!autoMode) {
    return;
  }

  // Hysteresis prevents rapid relay switching.
  if (!pumpState && soilMoisture <= PUMP_ON_THRESHOLD) {

    setPump(true);

    Serial.println(
      "Automatic irrigation: soil is dry."
    );
  }

  else if (pumpState && soilMoisture >= PUMP_OFF_THRESHOLD) {

    setPump(false);

    Serial.println(
      "Automatic irrigation: soil moisture is sufficient."
    );
  }
}


// ============================================================
// Measurement Scheduler
// ============================================================

// Start a new soil measurement every 2 seconds.
void scheduleSoilMeasurement() {

  if (!samplingSoil) {
    startSoilSampling();
  }
}


// ============================================================
// Blynk Controls
// ============================================================

// V4 = Manual pump control
// Use a Switch widget in Blynk.
BLYNK_WRITE(V4) {

  if (autoMode) {

    Serial.println(
      "Manual pump command ignored: AUTO mode is active."
    );

    // Keep manual control OFF while AUTO is active.
    Blynk.virtualWrite(V4, 0);

    return;
  }

  setPump(param.asInt() != 0);
}


// V5 = Auto / Manual mode
//
// 0 = MANUAL
// 1 = AUTO
//
// Use a Switch widget in Blynk.

BLYNK_WRITE(V5) {

  autoMode = param.asInt() != 0;

  Serial.print("Irrigation mode changed to: ");
  Serial.println(autoMode ? "AUTO" : "MANUAL");

  if (autoMode) {
    controlAutomaticIrrigation();
  }
}


// ============================================================
// Blynk Connected
// ============================================================

BLYNK_CONNECTED() {

  Serial.println("Blynk connected!");

  // Restore the last Blynk switch states.
  Blynk.syncVirtual(V4, V5);
}


// ============================================================
// Setup
// ============================================================

void setup() {

  Serial.begin(115200);

  delay(1000);

  Serial.println();
  Serial.println("==============================");
  Serial.println("ESP32 Water Irrigation");
  Serial.println("==============================");


  // ----------------------------------------------------------
  // ESP32 ADC Configuration
  // ----------------------------------------------------------

  analogReadResolution(12);

  analogSetPinAttenuation(
    SOIL_SENSOR_PIN,
    ADC_11db
  );


  // ----------------------------------------------------------
  // Pump / Relay
  // ----------------------------------------------------------

  pinMode(PUMP_PIN, OUTPUT);

  // Safe startup: pump OFF.

  if (RELAY_ACTIVE_LOW) {
    digitalWrite(PUMP_PIN, HIGH);
  } else {
    digitalWrite(PUMP_PIN, LOW);
  }


  // ----------------------------------------------------------
  // WiFiManager
  // ----------------------------------------------------------

  WiFi.mode(WIFI_STA);

  WiFiManager wm;

  wm.setConfigPortalTimeout(180);

  Serial.println("Starting WiFiManager...");

  if (!wm.autoConnect("ESP32-Irrigation")) {

    Serial.println(
      "WiFiManager failed. Restarting..."
    );

    delay(3000);

    ESP.restart();
  }


  Serial.println("Wi-Fi connected!");

  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  Serial.print("RSSI: ");
  Serial.print(WiFi.RSSI());
  Serial.println(" dBm");


  // ----------------------------------------------------------
  // Blynk
  // ----------------------------------------------------------

  Serial.println("Connecting to Blynk...");

  Blynk.config(BLYNK_AUTH_TOKEN);

  if (Blynk.connect(10000)) {

    Serial.println("Blynk connected!");

  } else {

    Serial.println(
      "Blynk connection failed. Blynk will retry."
    );
  }


  // ----------------------------------------------------------
  // mDash
  // ----------------------------------------------------------

  Serial.println("Starting mDash...");

  mDashBegin(MDASH_DEVICE_PASSWORD);

  Serial.println("mDash initialized.");


  // ----------------------------------------------------------
  // Sensor Measurement Scheduler
  // ----------------------------------------------------------

  // Start one 100-sample measurement every 2 seconds.
  // Sampling itself is non-blocking.
  timer.setInterval(
    2000L,
    scheduleSoilMeasurement
  );


  Serial.println();
  Serial.println("System ready.");
  Serial.println("Soil sensor : GPIO 34");
  Serial.println("Pump relay  : GPIO 26");
  Serial.println("ADC samples : 100");
  Serial.println("Sampling    : non-blocking");
  Serial.println("Pump starts : OFF");
  Serial.println("Mode        : MANUAL");
}


// ============================================================
// Main Loop
// ============================================================

void loop() {

  // Keep cloud services responsive.
  Blynk.run();

  // Run scheduled tasks.
  timer.run();

  // Process one ADC sample at a time.
  // No 500 ms blocking delay.
  processSoilSampling();

  // No deliberate delay here.
  // mDash/Blynk get maximum loop time.
}
