#ifndef TELEMETRY_H
#define TELEMETRY_H

#include <stdint.h>

struct TelemetryData {
    float   ph;
    float   turbidity1;
    float   turbidity2;
    bool    tank1_full;
    const char *state;
    uint8_t dose_attempts;
};

bool telemetry_publish(const TelemetryData &data);

#endif // TELEMETRY_H
