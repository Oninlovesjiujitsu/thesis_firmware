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

enum CmdResult {
    CMD_NONE = 0,       
    CMD_OK,        
    CMD_REJECTED_BUSY,  
    CMD_UNKNOWN_RELAY  
};

void actuator_init_all();

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

bool actuator_relay_state(uint8_t idx);

CmdResult actuator_handle_command(const char *payload, unsigned int length,
                                   char *relay_out, char *action_out);

#endif 
