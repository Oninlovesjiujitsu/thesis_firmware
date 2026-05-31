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
    // Relay shadow: [pump1, pump2, acid, base, sol1, sol2, sol3, sol4] — matches RelayIdx order
    bool    relays[8];
};

bool telemetry_publish(const TelemetryData &data);

#endif // TELEMETRY_H
