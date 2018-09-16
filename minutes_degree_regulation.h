#ifndef MINUTES_DEGREE_REGULATION_H_INCLUDED
#define MINUTES_DEGREE_REGULATION_H_INCLUDED
#include "write_read.h"

struct Results_boiler{
        int marker_boiler;
        int aux_boiler;
        int time_set;
        int marker_flow;
};//

//int minutes_degree_regulation_down_boiler(time_t counter_start, int *arr);
int *on_off_regulation_boiler(time_t start_counter, int marker_boiler,int aux_boiler, int *arr, modbus_t *ctx);
int can_bus_on_off_regulation_boiler(time_t start_counter, int *arr, int *DO_array, struct Results_boiler *on_off_results, int max_outdoor,int remote, int remote_set);
int boiler_starter(int *arr,int remote,int remote_set);
int minutes_degree_regulation_down_flow(float setpoint_md_flow,time_t start_counter, int *arr,int direction, int *control_array,int remote,int remote_set);
//int minutes_degree_regulation_up_boiler(time_t minutes_degrees,time_t minutes_degrees_change_over,int alarm[17], modbus_t * ctx);
//int minutes_degree_regulation_up_flow(time_t minutes_degrees, int *arr, modbus_t * ctx);
int can_bus_minutes_degree_regulation_up_flow(time_t minutes_degrees, int *arr, int *control_array,int remote,int remote_set);
#endif // MINUTES_DEGREE_REGULATION_H_INCLUDED
