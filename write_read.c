#include "write_read.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>




//#include "can_function.h"

int write_DO(canid_t card_id, int *DO_array, int output_number, int write_value)
{
    int i=0;

    DO_array[output_number]=write_value;
    unsigned char first_byte=2;
    unsigned char second_byte=0; //the first is for aknowledge
    unsigned char third_byte=0;
    int *returned_array;
    int digital_output_message=0;
    int status;


    for (i=0; i<16; i++) {
        if (i<8) {
            //third_byte +=(unsigned char) (DO_array[i]*pow(base,(double) i));
            third_byte|=(DO_array[i]&1)<<i;
            //printf("DO_array[%d] %d\n",i,DO_array[i]);
        } else {
            //second_byte+=(unsigned char) (DO_array[i]*pow(base,((double) i)-8));
            second_byte|=(DO_array[i]&1)<<(i-8);
        }
    }


    status=cansend(card_id,first_byte,second_byte,third_byte);
    if (status) {
        printf("Failure in Candbus Sending");
        return 1;
    }
    returned_array=candump(digital_output_message,card_id);
    if (returned_array[0]==-1) {
        printf("Failure in Candbus Receveing");
        return 1;
    }
    //printf("got it\n");

    first_byte=0xAA; //set AA before sending to stop
    status=cansend(card_id,first_byte,second_byte,third_byte);
    if (status) {
        printf("Failure in Candbus Sending");
        return 1;
    }

    free(returned_array);
    return 0;

}

int read_DI(canid_t card_id, int* reading_array)
{
    unsigned char first_byte=0xFF;
    unsigned char second_byte=0;
    unsigned char third_byte=0;
    int status;
    int digital_message=1;
    int *returned_array;
    int i;

//request a read(FF)
    status=cansend(card_id,first_byte,second_byte,third_byte);
    if (status) {
        printf("Failure in Candbus Sending");
        return 1;
    }
//read answer from IO
    returned_array=candump(digital_message,card_id);
    if (returned_array[0]==-1) {
        printf("Failure in Candbus Receveing");
        return 1;
    }

//stop answer (aknowledge)
    first_byte=0xAA;
    status=cansend(card_id,first_byte,second_byte,third_byte);
    if (status) {
        printf("Failure in Candbus Sending");
        return 1;
    }

    for (i=0; i<5; i++) {

        //printf("reading of sensor %d is %d\n",i,returned_array[i]);
        reading_array[i]=returned_array[i];
        //sleep(1);


    }
    free(returned_array);

    return 0;
}

int read_AI(canid_t card_id,int* reading_array)
{
    unsigned char first_byte=255;
    unsigned char second_byte=0;
    unsigned char third_byte=0;
    int status;
    int analogue_message=10;
    int *returned_array;
    int i=0;
    int offset=5;

    status=cansend(card_id,first_byte,second_byte,third_byte);
    if (status) {
        printf("Failure in Candbus Sending");
        return 1;
    }
    returned_array=candump(analogue_message,card_id);
    if (returned_array[0]==-1) {
        printf("Failure in Candbus Receveing");
        return 1;
    }
    first_byte=0xAA;
    status=cansend(card_id,first_byte,second_byte,third_byte);
    if (status) {
        printf("Failure in Candbus Sending");
        return 1;
    }

    for (i=0; i<10; i++) {


        reading_array[i+offset]=(returned_array[i+1])/100 - 300;
        //printf("reading of sensor %d is %d\n",i,reading_array[i+offset]);

    }

    free(returned_array);


    return 0;
}


//https://github.com/stephane/libmodbus - Readme with modbus functions descritpions
int modbus_read_sensor(int *alarm, modbus_t *ctx)
{
#define NREGISTERS (9)

    uint16_t *reading;
    const int sensors_slave=1;
    const int SlaveID=1;
    int state_of_reading=0;
    int rc=0;
    int i=0;
    //const is better practice than define as is type safe - is immediately interpreted by the compiler
    //const int register_array[9]={0,1,9,16,20,146,147,148,149}; //carel drive register address - to be used when connecting the actual system
    const int register_array[NREGISTERS]= {51,51,51,51,51,51,51,51,51}; //carel IO - used for testing

    /*reading from first ID*/
    rc=modbus_set_slave(ctx,SlaveID);   //set the slave ID
    if (rc == -1) {
        printf("Slave not available\n");
        return 1;
    }
    rc=modbus_connect(ctx);   //Establish connection
    if (rc == -1) {
        printf("Connection Failed\n");
        return 1;
    }

    for (i=0; i<NREGISTERS; i++) {
        state_of_reading = modbus_read_registers(ctx,register_array[i],sensors_slave, reading); //polling modbus server starting from reg number register_array[i] for sensors_slave position
        alarm[i]=&reading;
        if (state_of_reading != sensors_slave) {
            printf("Reading not available");
            modbus_close(ctx);
            return 1;

        }
    }
    modbus_close(ctx);
    return 0;
}




