#ifndef SENSORS_H
#define SENSORS_H

void  sensors_init();
float sensors_read_ph();
float sensors_read_turbidity1();   // Tank 1
float sensors_read_turbidity2();   // Tank 2 (post-filtration)
float sensors_read_rain();         // 0.0 = dry, 1.0 = wet
bool  sensors_read_float_switch(); // true = water present

#endif // SENSORS_H
