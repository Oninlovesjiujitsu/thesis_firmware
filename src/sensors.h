#ifndef SENSORS_H
#define SENSORS_H

void  sensors_init();
float sensors_read_ph();
float sensors_read_turbidity1();   
float sensors_read_turbidity2();   
float sensors_read_rain();        
bool  sensors_read_float_switch(); 
bool  sensors_adc_ok();            

#endif 
