#ifndef SAFE_FUNCTIONALITIES_H_INCLUDED
#define SAFE_FUNCTIONALITIES_H_INCLUDED

#include "write_read.h"
#include "regulation_functions.h"

void alarm_checker(int *arr, struct Result *alarm_results);
int can_bus_fail_safe_shut(int *DO_array, int start_stop, int error_type);
int compare_array(int *alarm,int *historical_alarm,int size);

#endif // SAFE_FUNCTIONALITIES_H_INCLUDED
