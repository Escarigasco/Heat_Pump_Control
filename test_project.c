#include "functions.h"
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <linux/can.h>
#include <stdbool.h>
#include "pigpio.h"
#include "command.h"
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <string.h>
#include <math.h>
#include<pthread.h>

#define SERVER_FIFO_PATH "/tmp/myfifo"
volatile int sending_array[40]= {0};
pthread_t tid[2];
pthread_mutex_t lock_sensor_reading;
pthread_mutex_t lock_read_remote;
pthread_mutex_t lock_3;
volatile int writing_synchro=0;
volatile float control_parameter[7]= {0,0.81,0,0,0};
volatile int marker_remote_setpoint=false;
volatile int boiler_remote_setpoint=0;
volatile int radiator_remote_setpoint=0;

void init(struct Result *alarm_results)
{
    alarm_results->marker_error = 0;        //set error type
    alarm_results->marker_min_outdoor = 0;  //if outdoor is < -20
    alarm_results->marker_max_outdoor = 0;  //if outdoor > +17
    alarm_results->warning_difference_brin = 0; //if diff in-out higher than
    alarm_results->warning_difference_heat_carrier = 0; //if diff in-out higher than
}

void init_on_off(struct Results_boiler *on_off_results)
{
    on_off_results->marker_boiler=0; //goes high if heat is required to boiler
    on_off_results->marker_flow=0;   //goes high if heat is required to radiators
    on_off_results->aux_boiler=0;
    on_off_results->time_set=1;

}


void* pipe_data_out(void *arg)   /***Check Thread Parking***/
{
    int fd;
    char *myfifo = "/tmp/test";

//creat the FIFO (named pipe)
    unlink(myfifo);
    mkfifo(myfifo, 0666);
    fd = open(myfifo, O_WRONLY);
    if (fd==NULL) {
        //
    }

    int i=0;

    while(1) {
        if(writing_synchro) {
            pthread_mutex_lock(&lock_sensor_reading);
            int bytes=write(fd, &sending_array, sizeof(sending_array)); /***define error handling for signal stop or FD leak - check again bytes value against size(sending_array)***/
            writing_synchro=0;
            pthread_mutex_unlock(&lock_sensor_reading);
            printf("bytes written %d\n",bytes);
        }
    }
    //fflush(stdout);
    close(fd);
    unlink(myfifo);
    //sleep(1);
    //close(fd);
}

void* pipe_data_in(void *arg)  /***Check Thread Parking***/
{
    int fd;
    char *myfifo = "/tmp/test_write";
    //int buf[5]= {0};
    int bytes=0;
    //time_t start;
    //time_t stop;
    float set_control_parameter[7]= {0};
    int i=0;
    sleep(10);  //Make sure that other SW (write) has started before we start reading

    printf("\nwaiting for pipe opening\n");
    fd= open(myfifo, O_RDONLY|O_NONBLOCK);
    if (fd==NULL) {
        //
    }


    printf("\nPipe opened!\n");
    //start=time(NULL);
    while(1) {

        bytes=read(fd, set_control_parameter, sizeof(set_control_parameter)); /***return bytes to be checked against size(set_control_parameter) with offset***/

        //printf("sono qui");
        if (bytes>0) {
            //stop=time(NULL);
            //if(stop-start>=5)
            //{

            pthread_mutex_lock(&lock_read_remote);
            for (i=0; i<5; i++) {
                if (i==1) {
                    control_parameter[i]+=(set_control_parameter[i]/10); //these are in the loop as wanted to create array to be passed in function in main
                } else {
                    control_parameter[i]+=set_control_parameter[i];     //these are in the loop as wanted to create array to be passed in function in main
                }
                printf("\nControl Parameter%d\n", set_control_parameter[i]);
            }
            marker_remote_setpoint=set_control_parameter[5];
            boiler_remote_setpoint=set_control_parameter[6];
            radiator_remote_setpoint=set_control_parameter[7];
            //printf("\nBytes read %d\n", bytes);
            pthread_mutex_unlock(&lock_read_remote);
            //start=time(NULL);
            //}
        }
    }

    close(fd);
}

void update_modbus_mapping(int *alarm,int *alarm_modbus,struct Result *alarm_results,int *output_array) //
{

    int i=0;
    const int OFFSET_DO=5;
    const int OFFSET_AI=8;
    const int OFFSET_MODBUS=23;
    //int nAddress=0;

    // pthread_mutex_lock(&lock_sensor_reading);
    for(i=0; i<40; i++) {
        if (i<5) {
            sending_array[i] = alarm[i]; //Digital Input
        } else if ((i>=5)&&(i<13)) {
            sending_array[i] = output_array[i-OFFSET_DO]; //Digital Output
        } else if((i>=13)&&(i<23)) {
            sending_array[i] = alarm[i-OFFSET_AI];
        } else if((i>=23)&&(i<32)) {
            sending_array[i] = alarm_modbus[i-OFFSET_MODBUS]; //Analog Temperature
        } else {
            sending_array[32]=alarm_results->marker_error;
            sending_array[33]=alarm_results->marker_min_outdoor;
            sending_array[34]=alarm_results->marker_max_outdoor;
            sending_array[35]=alarm_results->warning_difference_brin;
            sending_array[36]=alarm_results->warning_difference_heat_carrier;

        }
        //printf("sending array %d\n",sending_array[i]);
    }
    //pthread_mutex_unlock(&lock_sensor_reading);


}


int main(int argc, char **argv)
{

    bool marker_error=0;
    bool modbus_error=0;

    int network_set_up=0;
    int alarm_modbus[9]= {0};
    int alarm[15]= {0};
    int historical_alarm[15]= {0};
    int alarm_level=0;
    int boiler_state=0;
    int output_array[16]= {0};

    //int marker_flow=0;
    //int marker_boiler=0;
    int aux_boiler=0;
    int cold_start=1;
    int marker_on=0;
    int time_set=0;

    canid_t DI_card=5;
    canid_t AI_card=101;
    canid_t DO_card=204;

    int error_checker=0;

    int marker_radiator_pump=0;
    int brine_pump_operation=1; //if 1 you have parallel operation with the compressor (brine and heat carrier pumps goes off when compressor goes off), if 0 you have continuos (indipendent from compressor)
    int radiator_pump_operation=1; //if 1 you have summer stop (above +17) for the radiator pump (goes only on for 5 mins if off after for week), if 0 you have continous operation (always on)
    int state_of_writing=0;
    int counter_modbus=0;

    float setpoint_md_flow=-30;
    int direction=0;

    int boiler_lower_setpoint=44;

    const int baud=19200;
    const char parity='N';
    const int stop_bit=1;
    const int no_of_bit=8;

    time_t start_counter;
    time_t start_counter_aux_boiler;
    time_t start_counter_modbus;
    time_t stop_counter_modbus;

    struct Result alarm_results;
    init(&alarm_results);

    struct Results_boiler on_off_results;
    init_on_off(&on_off_results);

    int i=0;
    int g=0;

    int control_array[37]= {0};

    int err = pthread_create(&tid[0], NULL, &pipe_data_out, NULL);
    if (err != 0) {
        printf("\n can't create thread Pipe_write :[%s]", strerror(err));
    }
    err = pthread_create(&tid[1], NULL, &pipe_data_in, NULL); // the read process has always need to start after the write()
    if (err != 0) {
        printf("\n can't create thread Pipe_read :[%s]", strerror(err));
    }

    printf("Quit the loop: %s\n", strerror(errno));



    gpioInitialise();
    sleep(1);
    gpioSetMode(17, PI_OUTPUT);
    sleep(1);
    gpioWrite(17,0);
    sleep(1);
    gpioWrite(17,1);
    gpioTerminate();

    //https://github.com/stephane/libmodbus - Readme with modbus functions descritpions
    /***Modbus Master Init***/
    modbus_t* ctx;
    ctx = modbus_new_rtu("/dev/ttyUSB0", baud, parity, stop_bit, no_of_bit);

    if(alarm_results.marker_error=read_DI(DI_card,alarm)) {
        fprintf(stderr,"cannot read DI card");
        exit(-1);
    }

    if(alarm_results.marker_error=read_AI(AI_card,alarm)) {
        fprintf(stderr,"cannot read AI card");
        exit(-1);
    }
    //update_modbus_mapping(alarm,alarm_modbus, &alarm_results,output_array, fd,myfifo);
    pthread_mutex_lock(&lock_read_remote);
    update_modbus_mapping(alarm,alarm_modbus, &alarm_results,output_array);
    pthread_mutex_unlock(&lock_read_remote);


    //regulation_curve_generator(vertical,angular,quadratic_pos,quadratic_neg,array_section, control_array);
    pthread_mutex_lock(&lock_read_remote);
    regulation_curve_generator(control_parameter, control_array);
    pthread_mutex_unlock(&lock_read_remote);


    /***Selectable Operation Mode  this has to go in commissioning, not in the programme***/
    //scanf("Insert 0 for Continuos or 1 for parallel %d\n",&brine_pump_operation);
    //scanf("Insert 0 for Continuos or 1 for SummerStop %d\n",&radiator_pump_operation);

    /***(1) Start Monitoring - check operating conditions***/

    start_counter_modbus=time(NULL);
    while(1) { //(1) Start Monitoring
        
        //read values from carel drive
        modbus_error=modbus_read_sensor(alarm_modbus,ctx); //modbus read sensor has no importance for error checker
        
        //read digital sensors
        if(alarm_results.marker_error=read_DI(DI_card,alarm)) {
            fprintf(stderr,"cannot read DI card");
            exit(-1);
        }
        //read temperature sensors
        if(alarm_results.marker_error=read_AI(AI_card,alarm)) {
            fprintf(stderr,"cannot read AI card");
            exit(-1);
        }

        for (i=0; i<15; i++) {
            printf("reading of sensor %d is %d\n",i,alarm[i]);
        }

        //check sensor readings
        alarm_checker(alarm, &alarm_results);
        

        //check if the sensors reading changed - if yes update the array to be piped out
        if (compare_array(alarm,historical_alarm,15)) {
            // update_modbus_mapping(alarm,alarm_modbus, &alarm_results,output_array, fd,myfifo);
            pthread_mutex_lock(&lock_sensor_reading);
            update_modbus_mapping(alarm,alarm_modbus, &alarm_results,output_array);
            writing_synchro=1;
            pthread_mutex_unlock(&lock_sensor_reading);
        }

        //update regulation curve
        pthread_mutex_lock(&lock_read_remote);
        regulation_curve_generator(control_parameter, control_array);
        pthread_mutex_unlock(&lock_read_remote);


        start_counter=time(NULL); // start the counter for the minutes degree function



        /***(2) Ready to Turn ON***/
        while((!alarm_results.marker_error)&&(!alarm_results.marker_min_outdoor)&&(alarm[0])) { //(2) Ready to Turn ON
            

            modbus_error=modbus_read_sensor(alarm_modbus,ctx); //modbus read sensor has no importance for error checker
            
            //in this case you don't want exit from software as DO may be on so marker error is setted for a tidy shut down
            if(alarm_results.marker_error=read_DI(DI_card,alarm)) {
                fprintf(stderr,"cannot read DI card");
            }
            if(alarm_results.marker_error=read_AI(AI_card,alarm)) {
                fprintf(stderr,"cannot read AI card");
            }/***Put Function that does the marker error check***/

            for (i=0; i<15; i++) {
                printf("reading of sensor %d is %d\n",i,alarm[i]);
            }
            alarm_checker(alarm, &alarm_results);
            if ((alarm_results.marker_error)||(alarm_results.marker_min_outdoor)||(!alarm[0])) {
                break;
            }
            
            //the radiator pump is independent from the compressors status - it operates based on mode and outdoor temperature
            marker_radiator_pump=can_bus_radiator_pump_functioning(radiator_pump_operation, marker_radiator_pump, alarm_results.marker_max_outdoor, output_array, alarm[0]);


            pthread_mutex_lock(&lock_read_remote);
            regulation_curve_generator(control_parameter, control_array);
            pthread_mutex_unlock(&lock_read_remote);


            if (compare_array(alarm,historical_alarm,15)) {
                // update_modbus_mapping(alarm,alarm_modbus, &alarm_results,output_array, fd,myfifo);
                pthread_mutex_lock(&lock_sensor_reading);
                update_modbus_mapping(alarm,alarm_modbus, &alarm_results,output_array);
                writing_synchro=1;
                pthread_mutex_unlock(&lock_sensor_reading);
            }

            //Routine of Cold Start - the cold start means that is the first time the machine is activated by the press of the button

            if 	(cold_start) {

                /*you always need the reading inside this loops or you will be missing the sensors readings*/
                pthread_mutex_lock(&lock_read_remote);
                //the reason is mutex lock is due to the remote setpoint
                //functions that checks whether or not condition for turning on are met - they return a marker_flow/marker_boiler that state if radiators or boiler need heat
                //the boiler is on - off regulation with histeresys
                on_off_results.marker_flow=minutes_degree_regulation_down_flow(setpoint_md_flow,start_counter,alarm,direction,control_array,marker_remote_setpoint,radiator_remote_setpoint); //this are only counters
                on_off_results.marker_boiler = boiler_starter(alarm,marker_remote_setpoint,boiler_remote_setpoint);
                pthread_mutex_unlock(&lock_read_remote);

                //if heat is needed at the radiator or boiler
                if ((on_off_results.marker_boiler)||(on_off_results.marker_flow)) {
                    //Turn On
                    /*For Cold Start turn on everything  - Definition of priorities inside start up function*/
                    alarm_results.marker_error = candbus_start_up(on_off_results.marker_flow,on_off_results.marker_boiler,output_array);
                    //if start up failed due to comms error
                    if (alarm_results.marker_error) {
                        break;
                    }
                    
                    cold_start=0; //no more cold start
                    marker_on=1;  /***COMPRESSOR ON***/

                }

            } //close if cold_start

            //most of the functions are recurrent in each state as they involve measuring, checking errors, comparing against setpoints

            else { //if it is not cold start it is a regulation loop

                /*All the commands goes in the ON/OFF loops - insert alarms sections insertion discrimination*/
               
                start_counter=time(NULL); //need time reset everytime you enter a minutes degree regulated loop as it is real time monitoring function

                /***(4) Compressor OFF***/
                while (!marker_on) { // (4) Compressor OFF
                    /*you always need the reading inside this loops or you will be missing the sensors readings*/
                    modbus_error=modbus_read_sensor(alarm_modbus,ctx); //modbus read sensor has no importance for error checker
                    if(alarm_results.marker_error=read_DI(DI_card,alarm)) {
                        fprintf(stderr,"cannot read DI card");
                        break;
                    }
                    if(alarm_results.marker_error=read_AI(AI_card,alarm)) {
                        fprintf(stderr,"cannot read AI card");
                        break;
                    }

                    for (i=0; i<15; i++) {
                        printf("reading of sensor %d is %d\n",i,alarm[i]);
                    }

                    alarm_checker(alarm, &alarm_results);

                    //regulation_curve_generator(vertical,angular,quadratic_pos,quadratic_neg,array_section, control_array);
                    pthread_mutex_lock(&lock_read_remote);
                    regulation_curve_generator(control_parameter, control_array);
                    pthread_mutex_unlock(&lock_read_remote);



                    if (compare_array(alarm,historical_alarm,15)) {
                        // update_modbus_mapping(alarm,alarm_modbus, &alarm_results,output_array, fd,myfifo);
                        pthread_mutex_lock(&lock_sensor_reading);
                        update_modbus_mapping(alarm,alarm_modbus, &alarm_results,output_array);
                        writing_synchro=1;
                        pthread_mutex_unlock(&lock_sensor_reading);
                    }

                    //Radiator Pump functioning - this function is recurrent as in different states the radiator pump functionning depends only on the outdoor condition
                    marker_radiator_pump=can_bus_radiator_pump_functioning(radiator_pump_operation,marker_radiator_pump, alarm_results.marker_max_outdoor, output_array, alarm[0]);

                    if ((alarm_results.marker_error)||(alarm_results.marker_min_outdoor)||(!alarm[0])) {
                        break;
                    }

                    /*Minutes Degree Regulation*/
                    //again the condition are checked for the turning on - here is discrimitated the the minutes degree to the radiator based on the outdoor summer limit - system doesn't go on fo supply radiators in summer
                    if (!alarm_results.marker_max_outdoor) {
                        pthread_mutex_lock(&lock_read_remote);
                        on_off_results.marker_flow = minutes_degree_regulation_down_flow(setpoint_md_flow,start_counter,alarm,direction,control_array,marker_remote_setpoint,radiator_remote_setpoint);
                        pthread_mutex_unlock(&lock_read_remote);
                    } else {

                        on_off_results.marker_flow=0; 
                    }

                    //for the boiler no discrimination on outdoor temp - heat is always needed
                    pthread_mutex_lock(&lock_read_remote);
                    boiler_state = can_bus_on_off_regulation_boiler(start_counter, alarm, output_array, &on_off_results, alarm_results.marker_max_outdoor,marker_remote_setpoint,boiler_remote_setpoint);
                    pthread_mutex_unlock(&lock_read_remote);

                    if ((on_off_results.marker_boiler==1)||(on_off_results.marker_flow==1)) {
                        //For while start up the turning on depends on the selected mode of operation
                        alarm_results.marker_error = can_bus_regulation_starting(brine_pump_operation,output_array,on_off_results.marker_boiler,on_off_results.marker_flow);
                        if (alarm_results.marker_error) {
                            break;
                        }

                        marker_on=1;
                    }
                } //end of the while marker_on


                //again need to restart the timer before a minutes degree regulation
                start_counter=time(NULL);

                /***(3) Compressor ON***/
                while (marker_on) { // Need to insert here the bounds of functioning

                    /*you always need the reading inside this loops or you will be missing the sensors readings*/
                    modbus_error=modbus_read_sensor(alarm_modbus,ctx); //modbus read sensor has no importance for error checker
                    if(alarm_results.marker_error=read_DI(DI_card,alarm)) {
                        fprintf(stderr,"cannot read DI card");
                        exit(-1);
                    }
                    if(alarm_results.marker_error=read_AI(AI_card,alarm)) {
                        fprintf(stderr,"cannot read AI card");
                        exit(-1);
                    }
                    for (i=0; i<15; i++) {
                        printf("reading of sensor %d is %d\n",i,alarm[i]);
                    }

                    alarm_checker(alarm, &alarm_results);

                    pthread_mutex_lock(&lock_read_remote);
                    regulation_curve_generator(control_parameter, control_array);
                    pthread_mutex_unlock(&lock_read_remote);


                    if (compare_array(alarm,historical_alarm,15)) {
                        // update_modbus_mapping(alarm,alarm_modbus, &alarm_results,output_array, fd,myfifo);
                        pthread_mutex_lock(&lock_sensor_reading);
                        update_modbus_mapping(alarm,alarm_modbus, &alarm_results,output_array);
                        writing_synchro=1;
                        pthread_mutex_unlock(&lock_sensor_reading);
                    }

                    if ((alarm_results.marker_error)||(alarm_results.marker_min_outdoor)||(!alarm[0])) {
                        break;
                    }


                    //Radiator Pump functioning - independent from compressor state
                    marker_radiator_pump=can_bus_radiator_pump_functioning(radiator_pump_operation, marker_radiator_pump, alarm_results.marker_max_outdoor, output_array, alarm[0]);

                    //discrimination for radiators against the outdoor temp - if outdoor threshold is met there is not regulation, the radiators has to go off - cfr else
                    if (!alarm_results.marker_max_outdoor) {
                        pthread_mutex_lock(&lock_read_remote);
                        on_off_results.marker_flow = can_bus_minutes_degree_regulation_up_flow(start_counter,alarm,control_array,marker_remote_setpoint,radiator_remote_setpoint);
                        pthread_mutex_unlock(&lock_read_remote);
                    } else {

                        on_off_results.marker_flow=0; /***valid until you change the control array***/
                    }
                    //removed the conditional as the minutes degrees works on delta T so at the limit you may just not have an increase of the delta T

                    //this routine was supposed to control the auxiliary heating of the boiler - it is out of date as Qvantum communicated they won't use it
                    if ((alarm[12]<boiler_lower_setpoint)&&(!on_off_results.time_set)) {
                        start_counter_aux_boiler=time(NULL);
                        on_off_results.time_set=1;
                    }

                    //on off boiler check
                    pthread_mutex_lock(&lock_read_remote);
                    boiler_state =can_bus_on_off_regulation_boiler(start_counter,alarm, output_array, &on_off_results, alarm_results.marker_max_outdoor,marker_remote_setpoint,boiler_remote_setpoint);
                    pthread_mutex_unlock(&lock_read_remote);

                    /*Condition for Shut down - includes Auxiliary Study*/
                    //if minutes degree and boiler on off set the markers flow and boiler to be off, the system doesn't need more heat and the shut down routing can start
                    if ((!on_off_results.marker_flow)&&(!on_off_results.marker_boiler)) {
                        alarm_results.marker_error = can_bus_regulation_stopping(brine_pump_operation,output_array);
                        if (!alarm_results.marker_error) {
                            marker_on=0;
                        } else {
                            break;
                        }

                    }


                } //exit of state (3) Compressor ON

                //if the system meets the operating condition will enter the (4) COMPRESSOR OFF state 

            } //close else cold_start


        } //Exit of state (2) Ready to Turn ON

        cold_start=1; //outside state (2) it would be a new cold start
       
        //fail safe shut down as an alarm triggered
        /***better to insert a boolean to call this function***/
        state_of_writing=can_bus_fail_safe_shut(output_array,alarm[0],alarm_results.marker_error);
        marker_radiator_pump=0;
        marker_on=0;
        if (state_of_writing) {
            printf("System Not Isolated - Perform Manual Isolation\n");
        }



    }  //close marker error loop


    //the last command before exiting must be cutting out the equipment - redundant as system should never exit the monitoring loop

    state_of_writing=can_bus_fail_safe_shut(output_array,alarm[0],alarm_results.marker_error);
    if (state_of_writing) {
        printf("System Not Isolated - Perform Manual Isolation\n");
    }


    //DeAllocate Pointer
    modbus_free(ctx);
    //gpioTerminate(); //gpio should be initialized at the beginning and terminated here but the reason they are both init and terminated at the beginning is that 
    //the system doesn't hit this termination in test or normal condition of operation 
    //however it should be implemented a I/O card reset if the request of sensors time out - before crash the whole control software it would be good reset the cards
    return 0;
}



