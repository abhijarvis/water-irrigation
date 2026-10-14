/*
 * ESP32 Water Irrigation Controller - Two Sensors
 * WiFiManager + Blynk + mDash
 *
 * Two soil moisture sensors + two independently controlled pumps
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

#define SOIL_SENSOR_1_PIN 34
#define SOIL_SENSOR_2_PIN 35
#define PUMP1_PIN         26
#define PUMP2_PIN         27

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

// Calibrate each sensor independently using its own dry/wet readings.
#define SOIL1_DRY_ADC 1103
#define SOIL1_WET_ADC 1030
#define SOIL2_DRY_ADC 1103  // Update after calibrating Sensor 2
#define SOIL2_WET_ADC 1030  // Update after calibrating Sensor 2

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

float soilMoisture1 = 0.0;
float soilMoisture2 = 0.0;
int soilRaw1 = 0;
int soilRaw2 = 0;

bool pump1State = false;
bool pump2State = false;
bool autoMode = false;
bool hasSoilMeasurement = false;
bool pump1TimedOut = false;
bool pump2TimedOut = false;
bool soilSensor1Fault = false;
bool soilSensor2Fault = false;
uint8_t sensor1RailCount = 0;
uint8_t sensor2RailCount = 0;
#define SENSOR_RAIL_LOW_ADC 20
#define SENSOR_RAIL_HIGH_ADC 4075
#define SENSOR_FAULT_CONFIRMATIONS 3
#define MAX_PUMP_RUNTIME_MS (10UL * 60UL * 1000UL)
unsigned long pump1StartedAtMs = 0;
unsigned long pump2StartedAtMs = 0;

// Non-blocking ADC sampler
bool samplingSoil = false;
int soilSampleCount = 0;
long soilSampleTotal1 = 0;
long soilSampleTotal2 = 0;
unsigned long lastSoilSampleMs = 0;


// ============================================================
// Pump Control
// ============================================================

void writePumpOutput(uint8_t pin, bool on) {
  if (RELAY_ACTIVE_LOW) digitalWrite(pin, on ? LOW : HIGH);
  else digitalWrite(pin, on ? HIGH : LOW);
}

void setPump1(bool on) {
  if (on && (pump1TimedOut || soilSensor1Fault)) {
    Serial.println("Pump 1 ON rejected: safety lockout is active.");
    return;
  }
  if (pump1State == on) return;
  pump1State = on;
  writePumpOutput(PUMP1_PIN, on);
  pump1StartedAtMs = on ? millis() : 0;
  Blynk.virtualWrite(V3, pump1State ? 1 : 0);
  Serial.print("Pump 1 (Area 1): ");
  Serial.println(on ? "ON" : "OFF");
}

void setPump2(bool on) {
  if (on && (pump2TimedOut || soilSensor2Fault)) {
    Serial.println("Pump 2 ON rejected: safety lockout is active.");
    return;
  }
  if (pump2State == on) return;
  pump2State = on;
  writePumpOutput(PUMP2_PIN, on);
  pump2StartedAtMs = on ? millis() : 0;
  Blynk.virtualWrite(V9, pump2State ? 1 : 0);
  Serial.print("Pump 2 (Area 2): ");
  Serial.println(on ? "ON" : "OFF");
}


// ============================================================
// Soil Moisture
// ============================================================

float calculateMoisture(int raw, int dryAdc, int wetAdc) {

  if (dryAdc == wetAdc) {
    return 0.0;
  }

  float percentage =
      ((float)(dryAdc - raw) * 100.0) /
      (float)(dryAdc - wetAdc);

  return constrain(percentage, 0.0, 100.0);
}


// Start a new 100-sample measurement.
void startSoilSampling() {

  samplingSoil = true;
  soilSampleCount = 0;
  soilSampleTotal1 = 0;
  soilSampleTotal2 = 0;
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

  // Sample both separate areas during the same non-blocking interval.
  soilSampleTotal1 += analogRead(SOIL_SENSOR_1_PIN);
  soilSampleTotal2 += analogRead(SOIL_SENSOR_2_PIN);
  soilSampleCount++;

  if (soilSampleCount >= SOIL_ADC_SAMPLES) {

    soilRaw1 = soilSampleTotal1 / SOIL_ADC_SAMPLES;
    soilRaw2 = soilSampleTotal2 / SOIL_ADC_SAMPLES;
    soilMoisture1 = calculateMoisture(soilRaw1, SOIL1_DRY_ADC, SOIL1_WET_ADC);
    soilMoisture2 = calculateMoisture(soilRaw2, SOIL2_DRY_ADC, SOIL2_WET_ADC);

    // Repeated ADC rail readings often indicate a disconnected/failed sensor.
    // Latch a fault after three consecutive complete measurement windows.
    if (soilRaw1 <= SENSOR_RAIL_LOW_ADC || soilRaw1 >= SENSOR_RAIL_HIGH_ADC) {
      if (sensor1RailCount < SENSOR_FAULT_CONFIRMATIONS) sensor1RailCount++;
      if (sensor1RailCount >= SENSOR_FAULT_CONFIRMATIONS && !soilSensor1Fault) {
        soilSensor1Fault = true;
        Serial.println("SAFETY FAULT: Area 1 sensor ADC is at a rail. Pump 1 locked out until reboot.");
        setPump1(false);
      }
    } else {
      sensor1RailCount = 0;
    }

    if (soilRaw2 <= SENSOR_RAIL_LOW_ADC || soilRaw2 >= SENSOR_RAIL_HIGH_ADC) {
      if (sensor2RailCount < SENSOR_FAULT_CONFIRMATIONS) sensor2RailCount++;
      if (sensor2RailCount >= SENSOR_FAULT_CONFIRMATIONS && !soilSensor2Fault) {
        soilSensor2Fault = true;
        Serial.println("SAFETY FAULT: Area 2 sensor ADC is at a rail. Pump 2 locked out until reboot.");
        setPump2(false);
      }
    } else {
      sensor2RailCount = 0;
    }

    hasSoilMeasurement = true;

    samplingSoil = false;

    // Automatic control after the complete measurement.
    controlAutomaticIrrigation();

    // Blynk virtual pins:
    // V0 = Area 1 moisture %, V3 = Pump status, V6 = Area 1 raw ADC
    // V7 = Area 2 moisture %, V8 = Area 2 raw ADC
    Blynk.virtualWrite(V0, soilMoisture1);
    Blynk.virtualWrite(V3, pump1State ? 1 : 0);
    Blynk.virtualWrite(V9, pump2State ? 1 : 0);
    Blynk.virtualWrite(V6, soilRaw1);
    Blynk.virtualWrite(V7, soilMoisture2);
    Blynk.virtualWrite(V8, soilRaw2);

    Serial.println();
    Serial.println("========== Irrigation Data ==========");

    Serial.print("Area 1 ADC (100 samples): ");
    Serial.println(soilRaw1);
    Serial.print("Area 1 Moisture: ");
    Serial.print(soilMoisture1, 1);
    Serial.println(" %");

    Serial.print("Area 2 ADC (100 samples): ");
    Serial.println(soilRaw2);
    Serial.print("Area 2 Moisture: ");
    Serial.print(soilMoisture2, 1);
    Serial.println(" %");

    Serial.print("Pump 1       : ");
    Serial.println(pump1State ? "ON" : "OFF");
    Serial.print("Pump 2       : ");
    Serial.println(pump2State ? "ON" : "OFF");

    Serial.print("Mode         : ");
    Serial.println(autoMode ? "AUTO" : "MANUAL");

    Serial.println("=====================================");
  }
}


// ============================================================
// Automatic Irrigation
// ============================================================

void controlAutomaticIrrigation() {

  if (!autoMode || !hasSoilMeasurement) {
    return;
  }

  // Hysteresis prevents rapid relay switching.
  // Each sensor controls only its matching pump.
  if (!pump1State && !pump1TimedOut && !soilSensor1Fault && soilMoisture1 <= PUMP_ON_THRESHOLD) {
    setPump1(true);
    Serial.println("AUTO: Area 1 is dry; Pump 1 started.");
  } else if (pump1State && soilMoisture1 >= PUMP_OFF_THRESHOLD) {
    setPump1(false);
    Serial.println("AUTO: Area 1 moisture target reached; Pump 1 stopped.");
  }

  if (!pump2State && !pump2TimedOut && !soilSensor2Fault && soilMoisture2 <= PUMP_ON_THRESHOLD) {
    setPump2(true);
    Serial.println("AUTO: Area 2 is dry; Pump 2 started.");
  } else if (pump2State && soilMoisture2 >= PUMP_OFF_THRESHOLD) {
    setPump2(false);
    Serial.println("AUTO: Area 2 moisture target reached; Pump 2 stopped.");
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
// V4 controls Pump 1; V10 controls Pump 2. Use a Switch widget for each.
BLYNK_WRITE(V4) {

  if (autoMode) {

    Serial.println(
      "Manual pump command ignored: AUTO mode is active."
    );

    // Keep manual control OFF while AUTO is active.
    Blynk.virtualWrite(V4, 0);

    return;
  }

  if (soilSensor1Fault || pump1TimedOut) {
    Blynk.virtualWrite(V4, 0);
    Serial.println("Pump 1 manual command rejected: safety lockout is active.");
    return;
  }
  setPump1(param.asInt() != 0);
}

BLYNK_WRITE(V10) {
  if (autoMode) {
    Serial.println("Pump 2 manual command ignored: AUTO mode is active.");
    Blynk.virtualWrite(V10, 0);
    return;
  }
  if (soilSensor2Fault || pump2TimedOut) {
    Blynk.virtualWrite(V10, 0);
    Serial.println("Pump 2 manual command rejected: safety lockout is active.");
    return;
  }
  setPump2(param.asInt() != 0);
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
  Blynk.syncVirtual(V4, V5, V10);
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

  analogSetPinAttenuation(SOIL_SENSOR_1_PIN, ADC_11db);
  analogSetPinAttenuation(SOIL_SENSOR_2_PIN, ADC_11db);


  // ----------------------------------------------------------
  // Pump / Relay
  // ----------------------------------------------------------

  pinMode(PUMP1_PIN, OUTPUT);
  pinMode(PUMP2_PIN, OUTPUT);
  // Safe startup: both pumps OFF.
  writePumpOutput(PUMP1_PIN, false);
  writePumpOutput(PUMP2_PIN, false);


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
  Serial.println("Area 1 sensor: GPIO 34 (Blynk V0, raw V6)");
  Serial.println("Area 2 sensor: GPIO 35 (Blynk V7, raw V8)");
  Serial.println("Pump 1       : GPIO 26 (Area 1)");
  Serial.println("Pump 2       : GPIO 27 (Area 2)");
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

  // Independent safety cutoff if moisture thresholds are not reached.
  unsigned long now = millis();
  if (pump1State && (unsigned long)(now - pump1StartedAtMs) >= MAX_PUMP_RUNTIME_MS) {
    setPump1(false);
    pump1TimedOut = true;
    Serial.println("SAFETY: Pump 1 stopped and locked out after maximum run time. Reboot after checking the system.");
  }
  if (pump2State && (unsigned long)(now - pump2StartedAtMs) >= MAX_PUMP_RUNTIME_MS) {
    setPump2(false);
    pump2TimedOut = true;
    Serial.println("SAFETY: Pump 2 stopped and locked out after maximum run time. Reboot after checking the system.");
  }

  // mDash Arduino client maintains its TLS session/reconnect internally after
  // mDashBegin(); its documented API does not require a separate poll call.
  // Keep this loop non-blocking so both mDash and Blynk can run responsively.
  yield();
}
