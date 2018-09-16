#include "safe_functionalities.h"
#include <stdio.h>
#include <unistd.h>


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



/*inline int setBit(int variable, int bitPosition){
    return variable | (1 << bitPosition);
}*/

/*inline void setBit(int *variable, int bitPosition){
    variable =| (1 << bitPosition);
}*/

//this function is called after every sensor reading to verify whether or not the operating conditions are met
void alarm_checker(int *arr, struct Result *alarm_results)   /***Create Set inline Bit/Clear Bit Function***/
{

    const int brint_out_setpoint=-8;
    const int warn_brin_out_setpoint=-5;

    const int heat_carrier_in_setpoint=56;
    const int warn_heat_carrier_in_setpoint=54;

    const int hot_gas_setpoint=120;
    const int warn_hot_gas_setpoint=110;

    int outdoor_temp_limit=17;
    int min_outdoor=-20;

    int error_2_1=0,error_2_2=0,error_2_3=0;

    /*Digital alarm - No warning*/
    if (arr[1]) { //depends on the way the I/O module reads and cables connected
        printf("Compressor Tripped\n");
        alarm_results->marker_error = 1;
    }
    if (arr[2]) { //depends on the way the I/O module reads and cables connected
        printf("Brine Pump Tripped\n");
        alarm_results->marker_error = 1;
    }
    if (arr[3]) { //depends on the way the I/O module reads and cables connected
        printf("alarm, High Pressure \n");
        alarm_results->marker_error = 1;
    }
    if (arr[4]) { //depends on the way the I/O module reads and cables connected
        printf("alarm, Low Pressure\n");
        alarm_results->marker_error = 1;
    }

    /*Analogue alarm*/
    if ((arr[6]<=warn_brin_out_setpoint)&&(arr[6]<brint_out_setpoint)) {
        printf("Warning, brin out Temperature diverging\n");
    } else if (arr[6]<=brint_out_setpoint) {
        printf("alarm, brin out Temperature diverged\n");
        alarm_results->marker_error=2;
        error_2_1=1;
    } else {
        error_2_1=0;
    }

    if ((arr[7]>=warn_heat_carrier_in_setpoint)&&(arr[7]<heat_carrier_in_setpoint)) {
        printf("Warning, heat in Temperature diverging\n");
    } else if (arr[7]>=heat_carrier_in_setpoint) {
        printf("alarm, heat in Temperature diverged\n");
        alarm_results->marker_error= 2;
        error_2_2=1;
    } else {

        error_2_2=0;
    }

    if ((arr[10]>=warn_hot_gas_setpoint)&&(arr[10]<hot_gas_setpoint)) {
        printf("Warning, hot gas Temperature diverging\n");
    }
    if (arr[10]>=hot_gas_setpoint) {
        printf("alarm, hot gas Temperature diverged\n");
        alarm_results->marker_error = 2;
        error_2_3=1;
    } else {

        error_2_3=0;
    }
    if ((error_2_1==0)&&(error_2_2==0)&&(error_2_3==0)&&(arr[1]==0)&&(arr[2]==0)&&(arr[3]==0)&&(arr[4]==0)) {
        alarm_results->marker_error = 0;
    }

    /*External Ambient Conditions*/
    if (arr[13]>outdoor_temp_limit) {
        printf("Above Summer functioning limit\n");
        alarm_results->marker_max_outdoor = 1;
    } else {
        alarm_results->marker_max_outdoor = 0;
    }

    if (arr[13]<min_outdoor) {
        printf("Below Winter functioning\n");
        alarm_results->marker_min_outdoor = 1;
    } else {
        alarm_results->marker_min_outdoor = 0;
    }

    /*Temperature Difference*/
    //this are warnings connected to bad flow condition so to a decrease of performance of the heat exchange
    if ((arr[5]-arr[6])>=10) {
        printf("Not efficient flow in the cold circuit\n");
        alarm_results->warning_difference_brin =1;
    } else {
        alarm_results->warning_difference_brin = 0;
    }
    if ((arr[8]-arr[7])>=15) {
        printf("Not efficient flow in the warm circuit\n");
        alarm_results->warning_difference_heat_carrier = 1;
    } else {
        alarm_results->warning_difference_heat_carrier = 0;
    }

}

//this function is only called if the machine is going back to state (1) either for turn off command or alarm - also if the software is quitting
/***at the moment the function handles all error in the same safe way (shutting down everything) double check the discrimination with Qvantum though according to alarm[0] and error type***/
int can_bus_fail_safe_shut(int *DO_array,int start_stop,int error_type)
{

    int status=0;
    canid_t DO_card=204;

    if (start_stop) { //if the heat pump was on but alarm occurred
        if (error_type==1) { //error_type 1 is the case of - Compressor Trips, if Brine pump trips, if pressur alarm (DI alarms)

            status|=write_DO(DO_card,DO_array,K1,0);
            if (status == 1) {
                printf("Compressor still energized\n");
                //return 1;
            }

            mysleep(20);
            status|=write_DO(DO_card,DO_array,K2,0);
            if (status) {
                printf("Brine Pump still energized\n");
                //return 1;
            }

            mysleep(1);
            status|=write_DO(DO_card,DO_array,K3,0);
            if (status) {
                printf("Heat Carrier Pump still energized\n");
                //return 1;
            }

            mysleep(1);
            status|=write_DO(DO_card,DO_array,K4,0);
            if (status) {
                printf("Radiator Pump still energized\n");
                //return 1;
            }

            mysleep(1);
            status|=write_DO(DO_card,DO_array,K5,0);
            if (status == 1) {
                printf("Change Over valve still energized\n");
                //return 1;
            }

            status|=write_DO(DO_card,DO_array,K6,0);
            if (status) {
                printf("Change Over valve still energized\n");
                //return 1;
            }


            status=write_DO(DO_card,DO_array,K8,0);

            if (status) {
                return 1;
            }

        }
        if (error_type==2) { //hot gas, heat carrier in and Brin out sensors alarm

            status|=write_DO(DO_card,DO_array,K1,0);
            if (status == 1) {
                printf("Compressor still energized\n");
                //return 1;
            }

            mysleep(20);
            status|=write_DO(DO_card,DO_array,K2,0);
            if (status) {
                printf("Brine Pump still energized\n");
                //return 1;
            }

            mysleep(1);
            status|=write_DO(DO_card,DO_array,K3,0);
            if (status) {
                printf("Heat Carrier Pump still energized\n");
                //return 1;
            }

            mysleep(1);
            status|=write_DO(DO_card,DO_array,K4,0);
            if (status) {
                printf("Radiator Pump still energized\n");
                //return 1;
            }

            mysleep(1);
            status|=write_DO(DO_card,DO_array,K5,0);
            if (status == 1) {
                printf("Change Over valve still energized\n");
                //return 1;
            }

            status|=write_DO(DO_card,DO_array,K6,0);
            if (status) {
                printf("Change Over valve still energized\n");
                //return 1;
            }


            status=write_DO(DO_card,DO_array,K8,0);

            if (status) {
                return 1;
            }
        }



    } else { //if heat pump is turned off shut everything down

        status|=write_DO(DO_card,DO_array,K1,0);
        if (status == 1) {
            printf("Compressor still energized\n");
            //return 1;
        }

        mysleep(20);
        status|=write_DO(DO_card,DO_array,K2,0);
        if (status) {
            printf("Brine Pump still energized\n");
            //return 1;
        }

        mysleep(1);
        status|=write_DO(DO_card,DO_array,K3,0);
        if (status) {
            printf("Heat Carrier Pump still energized\n");
            //return 1;
        }

        mysleep(1);
        status|=write_DO(DO_card,DO_array,K4,0);
        if (status) {
            printf("Radiator Pump still energized\n");
            //return 1;
        }

        mysleep(1);
        status|=write_DO(DO_card,DO_array,K5,0);
        if (status == 1) {
            printf("Change Over valve still energized\n");
            //return 1;
        }

        status|=write_DO(DO_card,DO_array,K6,0);
        if (status) {
            printf("Change Over valve still energized\n");
            //return 1;
        }

        status=write_DO(DO_card,DO_array,K8,0);

        if (status) {
            return 1;
        }
    }



    return 0;
}


int compare_array(int *alarm,int *historical_alarm,int size)

{

    int i=0;
    int marker=0;

    for(i=0; i<size; i++) {
        if(alarm[i]!=historical_alarm[i]) {
            marker=1;
            historical_alarm[i]=alarm[i];

        }


    }
    return marker;
}

