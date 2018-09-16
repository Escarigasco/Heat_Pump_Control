#ifndef REGULATION_FUNCTIONS_H_INCLUDED
#define REGULATION_FUNCTIONS_H_INCLUDED

#include "write_read.h"

int can_bus_regulation_starting(int brine_pump_operation, int *DO_array, int marker_boiler, int marker_flow);
int can_bus_regulation_stopping(int brine_pump_operation, int *DO_array);
int candbus_start_up(int marker_flow,int marker_boiler,int *DO_array);
void regulation_curve_generator(float *set_control_parameter, int *control_array);
void mysleep(int seconds);

#endif // REGULATION_FUNCTIONS_H_INCLUDED
