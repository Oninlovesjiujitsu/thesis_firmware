#ifndef TELEMETRY_H
#define TELEMETRY_H

#include <stdint.h>

struct TelemetryData {
    float   ph;
    float   turbidity1;
    float   turbidity2;
    float   rain;
    bool    tank1_full;
    const char *state;
    uint8_t dose_attempts;
    uint8_t filter_cycles;
    float   ph_at_sensing;    // pH captured at SENSING state
    float   turb1_at_sensing; // Turb1 captured at SENSING state
    bool    warn_ph_max;      // pH correction gave up after max attempts
};

bool telemetry_publish(const TelemetryData &data);

#endif // TELEMETRY_H
