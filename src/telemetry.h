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
    uint8_t filter_cycles;
    bool    relays[8];
};

bool telemetry_publish(const TelemetryData &data);

#endif 
