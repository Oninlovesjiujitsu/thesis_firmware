#include <Arduino.h>
#include "config.h"
#include "sensors.h"

// Read averaged sensor voltage (accounts for voltage divider)
static float read_averaged_voltage(int pin, int samples) {
    long sum = 0;
    for (int i = 0; i < samples; i++) {
        sum += analogRead(pin);
        delay(SENSOR_SAMPLE_DELAY_MS);
    }
    float pinVoltage = (sum / (float)samples) * (ADC_VREF / ADC_RESOLUTION);
    return pinVoltage * VOLTAGE_DIVIDER_RATIO;
}

void sensors_init() {
    pinMode(TURBIDITY_PIN, INPUT);
    pinMode(PH_PIN, INPUT);
}

float sensors_read_ph() {
    float voltage = read_averaged_voltage(PH_PIN, SENSOR_SAMPLE_COUNT);
    float ph = PH_SLOPE * voltage + PH_OFFSET;

    if (ph < 0.0f) ph = 0.0f;
    if (ph > 14.0f) ph = 14.0f;
    return ph;
}

float sensors_read_turbidity() {
    float v = read_averaged_voltage(TURBIDITY_PIN, SENSOR_SAMPLE_COUNT);
    float ntu = TURB_COEFF_A * v * v + TURB_COEFF_B * v + TURB_COEFF_C;

    if (ntu < 0.0f) ntu = 0.0f;
    if (ntu > 3000.0f) ntu = 3000.0f;
    return ntu;
}

float sensors_read_tank_level() {
    // TODO: wire ultrasonic sensor
    return TANK_LEVEL_PLACEHOLDER;
}
