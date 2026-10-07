/*
 * ESP32 Soil Moisture Sensor - 100 Sample Test
 *
 * Sensor:
 *   VCC -> 3.3V
 *   GND -> GND
 *   AO  -> GPIO 34
 *
 * Calibration:
 *   Dry soil = 1103 ADC
 *   Wet soil = 1030 ADC
 *
 * Copyright (c) 2026 Abhishek Pandit
 */

#define SOIL_SENSOR_PIN 34

// Calibration
#define SOIL_DRY_ADC 1103
#define SOIL_WET_ADC 1030

// Number of ADC samples per reading
#define ADC_SAMPLES 100


void setup() {

  Serial.begin(115200);

  delay(1000);

  analogReadResolution(12);

  analogSetPinAttenuation(
    SOIL_SENSOR_PIN,
    ADC_11db
  );

  Serial.println();
  Serial.println("==============================");
  Serial.println("ESP32 Soil Moisture Test");
  Serial.println("==============================");

  Serial.println("Sensor Pin : GPIO 34");
  Serial.println("Dry ADC    : 1103");
  Serial.println("Wet ADC    : 1030");
  Serial.println("Samples    : 100");

  Serial.println();
}


// ------------------------------------------------------------
// Read 100 ADC samples
// ------------------------------------------------------------

int readSoilADC() {

  long total = 0;

  for (int i = 0; i < ADC_SAMPLES; i++) {

    total += analogRead(SOIL_SENSOR_PIN);

    delay(5);
  }

  return total / ADC_SAMPLES;
}


// ------------------------------------------------------------
// Convert ADC to moisture percentage
// ------------------------------------------------------------

float calculateMoisture(int adcValue) {

  float moisture =
      ((float)(SOIL_DRY_ADC - adcValue) * 100.0) /
      (float)(SOIL_DRY_ADC - SOIL_WET_ADC);

  return constrain(moisture, 0.0, 100.0);
}


// ------------------------------------------------------------
// Main loop
// ------------------------------------------------------------

void loop() {

  int rawADC = readSoilADC();

  float moisture =
      calculateMoisture(rawADC);


  Serial.print("ADC (100 samples): ");
  Serial.println(rawADC);

  Serial.print("Moisture         : ");
  Serial.print(moisture, 1);
  Serial.println(" %");

  Serial.println("------------------------------");

  delay(1000);
}
