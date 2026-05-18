#include <Wire.h>
#include <Adafruit_ADS1X15.h>

Adafruit_ADS1115 ads; 

// Your measured calibration range (Sensor outputting 5V signals)
const float sensorMaxV = 3.58; // Voltage at pH 0
const float sensorMinV = 2.08; // Voltage at pH 14

void setup() {
  Serial.begin(115200);

  // Initialize ADS1115
  if (!ads.begin()) {
    Serial.println("Failed to initialize ADS1115!");
    while (1);
  }

  // Setting gain to ONE captures +/- 4.096V
  // This is perfect for your 2.08V - 3.58V range.
  ads.setGain(GAIN_ONE); 
}

void loop() {
  int16_t results;
  float sumV = 0;
  int samples = 20;

  for (int i = 0; i < samples; i++) {
    results = ads.readADC_SingleEnded(0); // Reading from A0
    // Convert raw value to voltage (Multiplier for GAIN_ONE is 0.125mV)
    sumV += ads.computeVolts(results);
    delay(10);
  }

  float avgVoltage = sumV / samples;

  // Linear Mapping Formula using your 3.58V to 2.08V range
  float phValue = 0.0 + (avgVoltage - sensorMaxV) * (14.0 - 0.0) / (sensorMinV - sensorMaxV);

  // Constrain pH to 0-14
  if (phValue < 0) phValue = 0; 

  Serial.print("ADS1115 Voltage: ");
  Serial.print(avgVoltage, 3);
  Serial.print("V | pH Value: ");
  Serial.println(phValue, 2);

  delay(1000);
}