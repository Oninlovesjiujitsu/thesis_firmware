#ifndef ACTUATOR_H
#define ACTUATOR_H

#include <stdint.h>

// Relay index — must match s_relay_on[] order in actuator.cpp
enum RelayIdx {
    RI_PUMP1 = 0,
    RI_PUMP2,
    RI_ACID,
    RI_BASE,
    RI_SOL1,
    RI_SOL2,
    RI_SOL3,
    RI_SOL4,
    RI_COUNT
};

// Result codes from actuator_handle_command()
enum CmdResult {
    CMD_NONE = 0,       // No recognized command — caller should not ACK
    CMD_OK,             // Command executed
    CMD_REJECTED_BUSY,  // Relay command rejected — treatment not IDLE
    CMD_UNKNOWN_RELAY   // ON/OFF parsed but relay name unrecognized
};

// Initialize all relay pins (pre-drive HIGH pattern for active-low safety)
void actuator_init_all();

// Named actuator controls — true = energized/open, false = off/closed
void pump1_set(bool on);
void pump2_set(bool on);
void dose_acid_set(bool on);
void dose_base_set(bool on);
void sol1_set(bool open);
void sol2_set(bool open);
void sol3_set(bool open);
void sol4_set(bool open);

// Safety: all actuators OFF immediately
void actuator_all_off();

// Shadow state — true = currently energized. idx must be < RI_COUNT.
bool actuator_relay_state(uint8_t idx);

// MQTT command handler (PAUSE/RESUME/RESET + manual relay ON/OFF when IDLE).
// relay_out  : caller buffer >= 8 bytes; filled with relay name, or "" for
//              PAUSE/RESUME/RESET commands.
// action_out : caller buffer >= 8 bytes; filled with "PAUSE", "RESUME",
//              "RESET", "ON", or "OFF".
// Returns CmdResult — publish ACK only when result != CMD_NONE.
CmdResult actuator_handle_command(const char *payload, unsigned int length,
                                   char *relay_out, char *action_out);

#endif // ACTUATOR_H
