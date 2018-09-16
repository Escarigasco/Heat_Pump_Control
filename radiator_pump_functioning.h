#ifndef RADIATOR_PUMP_functioning_H
#define RADIATOR_PUMP_functioning_H

#include "write_read.h"

int radiator_pump_functioning(int functioning, int marker_on, int marker_max_outdoor, modbus_t * ctx);
int can_bus_radiator_pump_functioning(int functioning, int marker_on, int marker_max_outdoor,int *DO_array, int start_stop);


#endif                          //  RADIATOR_PUMP_functioning_H
