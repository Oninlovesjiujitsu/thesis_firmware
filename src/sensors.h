#ifndef SENSORS_H
#define SENSORS_H

void sensors_init();
float sensors_read_ph();
float sensors_read_turbidity();
float sensors_read_tank_level();

#endif // SENSORS_H
