#ifndef CAN_FUNCTION_H_INCLUDED
#define CAN_FUNCTION_H_INCLUDED
#include <linux/can.h>

int *candump(int expected_messages, canid_t id);
int cansend(canid_t id,  unsigned char data_1, unsigned char data_2, unsigned char data_3);
#endif                          // CAN_FUNCTION_H_INCLUDED
