#ifndef WRITE_READ_H_INCLUDED
#define WRITE_READ_H_INCLUDED

#include "can_function.h"
#include <linux/can.h>
#include <modbus/modbus.h>

struct Result {
        int marker_error;
        int marker_min_outdoor;
        int marker_max_outdoor;
        int warning_difference_brin;
        int warning_difference_heat_carrier;
};


int write_DO(canid_t card_id,int *DO_array, int output_number, int write_value);
int read_AI(canid_t card_id, int* reading_array);
int read_DI(canid_t card_id, int* reading_array);
//int writing_analogue(int K, int slave_ID, int analog_value);
//int writing_off(int K, int slave_ID, modbus_t * ctx);
//int writing_on(int K, int slave_ID, modbus_t * ctx);
int modbus_read_sensor(int *alarm, modbus_t * ctx);
void modbus_listener(modbus_t *ctx_TCP,modbus_mapping_t *mb_mapping,int socket);


#endif
