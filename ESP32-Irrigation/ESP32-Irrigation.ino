/*
 * ESP32 Water Irrigation Controller
 * WiFiManager + Blynk + mDash
 *
 * Copyright (c) 2026 Abhishek Pandit
 */

#define BLYNK_PRINT Serial
#define BLYNK_TEMPLATE_ID   "YOUR_TEMPLATE_ID"
#define BLYNK_TEMPLATE_NAME "ESP32 Irrigation"
#define BLYNK_AUTH_TOKEN    "YOUR_BLYNK_AUTH_TOKEN"

#define MDASH_APP_NAME "ESP32-Irrigation"
#define MDASH_DEVICE_PASSWORD "YOUR_MDASH_DEVICE_PASSWORD"

#include <WiFi.h>
#include <WiFiManager.h>
#include <BlynkSimpleEsp32.h>
#include <mDash.h>

BlynkTimer timer;

float soilMoisture = 0.0;
float temperature = 0.0;
int waterLevel = 0;
bool pumpState = false;

#define PUMP_PIN 26

void sendSensorData() {
  // TODO: Replace demo values with actual sensor readings.
  soilMoisture = 65.0;
  temperature = 28.5;
  waterLevel = 80;

  Blynk.virtualWrite(V0, soilMoisture);
  Blynk.virtualWrite(V1, temperature);
  Blynk.virtualWrite(V2, waterLevel);
  Blynk.virtualWrite(V3, pumpState ? 1 : 0);

  Serial.println("\n----- Irrigation Data -----");
  Serial.printf("Soil Moisture : %.1f %%\n", soilMoisture);
  Serial.printf("Temperature   : %.1f C\n", temperature);
  Serial.printf("Water Level   : %d %%\n", waterLevel);
  Serial.print("Pump          : ");
  Serial.println(pumpState ? "ON" : "OFF");
  Serial.println("---------------------------");
}

BLYNK_WRITE(V4) {
  pumpState = param.asInt();
  digitalWrite(PUMP_PIN, pumpState ? HIGH : LOW);
  Serial.print("Pump changed from Blynk: ");
  Serial.println(pumpState ? "ON" : "OFF");
}

BLYNK_CONNECTED() {
  Serial.println("Blynk connected!");
  Blynk.syncVirtual(V4);
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n==============================");
  Serial.println("ESP32 Water Irrigation");
  Serial.println("==============================");

  pinMode(PUMP_PIN, OUTPUT);
  digitalWrite(PUMP_PIN, LOW);

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

  timer.setInterval(2000L, sendSensorData);
  Serial.println("System ready.");
}

void loop() {
  Blynk.run();
  timer.run();
  delay(10);
}
