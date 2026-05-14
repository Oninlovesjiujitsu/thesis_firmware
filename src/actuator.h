#ifndef ACTUATOR_H
#define ACTUATOR_H

// Initialize all relay pins (pre-drive HIGH pattern for active-low safety)
void actuator_init_all();

// Named actuator controls — true = energized/open, false = off/closed
void pump1_set(bool on);
void pump2_set(bool on);
void dose_acid_set(bool on);
void dose_base_set(bool on);
void sol2_set(bool open);
void sol3_set(bool open);
void sol4_set(bool open);

// Safety: all actuators OFF immediately
void actuator_all_off();

// MQTT command handler (PAUSE/RESUME/RESET + manual relay ON/OFF when IDLE)
void actuator_handle_command(const char *payload, unsigned int length);

#endif // ACTUATOR_H
