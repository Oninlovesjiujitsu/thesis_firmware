#ifndef SENSORS_H
#define SENSORS_H

void  sensors_init();
float sensors_read_ph();
float sensors_read_turbidity1();   // GPIO 34, Tank 1
float sensors_read_turbidity2();   // GPIO 32, Tank 2
bool  sensors_read_float_switch(); // GPIO 18, true = water present

#endif // SENSORS_H
