#include "regulation_functions.h"
#include <unistd.h>
#include <stdio.h>
enum outputName {
    K1=0, //compressor
    K2,   //brine pump
    K3,   //heat carrier
    K4,   //radiator pump
    K5,   //change over to radiators
    K6,   //change over to boiler
    K7,   //auxiliary heating boiler
    K8=7  //summary alarm
};

void mysleep(int seconds){
while(seconds > 0){
seconds=sleep(seconds);
}
}
//function to start the compressor after off state (4) it is only called if it is not a cold - it take brine pump operation that control Heat carrier and brine pump operating mode
//the input marker_boiler and marker_flow indicate which section needs heats, if boiler or radiators
int can_bus_regulation_starting(int brine_pump_operation, int *DO_array,int marker_boiler,int marker_flow)
{
    int status=0;
    canid_t DO_card=204;

    if (marker_boiler)                            //if boiler needs heat
    {
        status|=write_DO(DO_card, DO_array,K5,0);
        status|=write_DO(DO_card, DO_array,K6,1);  //valve redirect to boiler independently from flow as boiler has priority
    } else {
        if (marker_flow)                                //if boiler ok but radiator needs heat
        {
            status|=write_DO(DO_card, DO_array,K6,0);
            status|=write_DO(DO_card, DO_array,K5,1);    //valve redirect to radiators
        }
    }

    if (status)
    {
        return 1;
    }

    if (brine_pump_operation) {                      //if is parallel operation (true) for Brine and Heat Carrier as they are meant to be off
        status=write_DO(DO_card, DO_array,K2,1);     //turn on Brine Pump
        if (status) {
            return 1;
        }
        mysleep(1);                                   //wait one second

        status=write_DO(DO_card, DO_array,K3,1);  //turn on Heat Carrier Pump
        if (status) {
            return 1;
        }
        mysleep(20);                                //wait twenty seconds

        status=write_DO(DO_card, DO_array,K1,1);  //turn on compressor
        if (status) {
            return 1;
        }
    }

    else {

        status=write_DO(DO_card, DO_array,K1,1);  //if is continuos operation just turn on comressor as they pumps are meant to be on

        if (status) {
            return 1;
        }
    }

    return 0;
}

//routine to shut exit the (3) Compressor ON state after have checked that the condition for turn off are met
//takes as input only the operation type of brine pump and heat carrier (brine_pump_operation) to decide wheter or not the two pumps shall go off
int can_bus_regulation_stopping(int brine_pump_operation, int *DO_array)
{

    int status=0;
    canid_t DO_card=204;

    //the reading comes here only if the above are false or the return take place.
    if (brine_pump_operation) {  //if operation type is parallel then


        status=write_DO(DO_card,DO_array,K1,0); //shut down compressor
        if (status) {
            return 1;
        }
        mysleep(60);                             // keep Heat carrier and Brine pumps on for 60 seconds

        status=write_DO(DO_card,DO_array,K2,0);  //turn off Brine pump
        if (status) {
            return 1;
        }

        status=write_DO(DO_card,DO_array,K3,0);  //turn off Heat carrier pump
        if (status) {
            return 1;
        }

    } else {                                    //if operation type is continuos then

        status=write_DO(DO_card,DO_array,K1,0);  //just shut down the compressor and leave the rest going
        if (status) {
            return 1;
        }

    }
    return 0;


} //valve in this routine are not considered as their status will then be defined according to the need when the regulation starts again

//this function only used if it is a cold start and all the pumps and compressor needs to go on
int candbus_start_up(int marker_flow,int marker_boiler,int *DO_array)
{
    int status=0;
    canid_t DO_card=204;

    if ((marker_boiler)&&(marker_flow)) {  //if both radiators and boiler needs heat priority is given to the boiler
        marker_boiler=1;
        marker_flow=0;
    }

    if (marker_flow) {

        status|=write_DO(DO_card, DO_array, K6, 0);
        status|=write_DO(DO_card, DO_array, K5, 1); //valve redirect to radiators

    } else if (marker_boiler) {

        status|=write_DO(DO_card, DO_array, K5, 0);
        status|=write_DO(DO_card, DO_array, K6, 1); //valve redirect to boiler independently from flow as boiler has priority

    }
    if (status) {
        return 1;
    }

    /***start up of the brine pump***/
    status=write_DO(DO_card, DO_array, K2, 1);
    if (status) {
        return 1;
    }

    mysleep(1);

    /***start up of the heat carrier pump***/
    status=write_DO(DO_card, DO_array, K3, 1);
    if (status) {
        return 1;
    }


    /***Radiator pump? The radiator pump works independently from the compressor state and is only function of external temperature***/

    mysleep(30);

    /***start up compressor***/
    status=write_DO(DO_card, DO_array, K1, 1); //  Compressor goes on
    if (status) {
        return 1;
    }

    status=write_DO(DO_card, DO_array, K8, 0);  //shut down the summary alarm as the condition for correct functioning are met - decide if this is a variable or a led
    return 0;
}


//this function creates and overwrite the regulation curve
void regulation_curve_generator(float *set_control_parameter, int *control_array)
{
    int i=0;
    int vertical=0;
    double angular=0;
    int offset=0;
    int array_section=0;

    vertical=set_control_parameter[0];
    angular=set_control_parameter[1];
    offset=set_control_parameter[2];
    array_section=set_control_parameter[3]; /***CHANGE IN MODBUS SERVER AND HMI***/

    for (i=0; i<37; i++) {
        switch(array_section) { //set the section to modify

        case 1:
            if (i<5) {
                control_array[i]=(-angular*(i) + 50)+vertical+offset;
            } else {

                control_array[i]=(-angular*(i) + 50)+vertical;
            }
        case 2:
            if ((i>=5)&&(i<10)) {
                control_array[i]=(-angular*(i) + 50)+vertical+offset;
            } else {

                control_array[i]=(-angular*(i) + 50)+vertical;
            }
        case 3:
            if ((i>=10)&&(i<15)) {
                control_array[i]=(-angular*(i) + 50)+vertical+offset;
            } else {

                control_array[i]=(-angular*(i) + 50)+vertical;
            }
        case 4:
            if ((i>=15)&&(i<20)) {
                control_array[i]=(-angular*(i) + 50)+vertical+offset;
            } else {

                control_array[i]=(-angular*(i) + 50)+vertical;
            }
        case 5:
            if ((i>=20)&&(i<25)) {
                control_array[i]=(-angular*(i) + 50)+vertical+offset;
            } else {

                control_array[i]=(-angular*(i) + 50)+vertical;
            }
        case 6:
            if ((i>=25)&&(i<30)) {
                control_array[i]=(-angular*(i) + 50)+vertical+offset;
            } else {

                control_array[i]=(-angular*(i) + 50)+vertical;
            }
        case 7:
            if ((i>=30)&&(i<37)) {
                control_array[i]=(-angular*(i) + 50)+vertical+offset;
            } else {

                control_array[i]=(-angular*(i) + 50)+vertical;
            }
        default: //if array_section=0 then modify everything
            control_array[i]=(-angular*(i) + 50)+vertical;

        }

    }
}













