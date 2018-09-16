#include "minutes_degree_regulation.h"
#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <linux/can.h>
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


//function that handles the on off regulation of the boiler
int can_bus_on_off_regulation_boiler(time_t start_counter, int *arr, int *DO_array, struct Results_boiler *on_off_results, int max_outdoor,int remote, int remote_set)
{
    int boiler_upper_setpoint=50;
    int boiler_mid_setpoint=46;
    int boiler_lower_setpoint=boiler_mid_setpoint-2;
    int delay=0;
    time_t stop_counter;
    int status;
    canid_t card_id=204;

    //check if a remote setpoint is given 
    if (remote){                            
        boiler_upper_setpoint=remote_set;
    }else{
        boiler_upper_setpoint=50;
    }

    //if boiler temperature is lower than the setpoint
    if (arr[12]<=boiler_mid_setpoint)
    {
        if (!on_off_results->marker_boiler) //and the boiler is off
        {
            status=write_DO(card_id,DO_array,K5,0); //move the valve to the boiler direction - shut down output to radiator valve 
            status=write_DO(card_id,DO_array,K6,1); //set valve to boiler
            on_off_results->marker_boiler=1; //then boiler is on - the system doesn't control the boiler "fire" but the flow of hot water to it

        }
         //if below the lower setpoint turn on auxiliary heater of the boiler - out of date routine but double check before delete! 
        if (arr[12]<boiler_lower_setpoint)
        {
            stop_counter=time(NULL);
            delay=(stop_counter-start_counter)/60;
            if ((delay>=15)&&(!on_off_results->aux_boiler)) //the routine allow the system to check if the hit pump can self recover before turn on the electric heater for 15 mins - then turn on the electric heater 
            {
                status=write_DO(card_id,DO_array,K7,1); //aux goes on
                on_off_results->aux_boiler=1; //marker for the auxiliary electric heating
            }

        }


    }
    else if ((arr[12]>=boiler_mid_setpoint)&&(on_off_results->aux_boiler)) //if the system reached the middle setpoint aided by the aux then the aux goes off
    {
        status=write_DO(card_id,DO_array,K7,0); // aux off
        on_off_results->aux_boiler=0;
        on_off_results->time_set=0;
    }
    else if    ((arr[12]>=boiler_upper_setpoint)&&(on_off_results->marker_boiler)) //if the boiler is heat supplied and meet the thermal condition it can go off
    {
        if (max_outdoor) //this discrimination is made to avoid that the change over goes to the radiator - if it is summer the valve can stay at boiler supply - the compressor can go off
        {
            on_off_results->marker_boiler=0; 
        }
        else
        {
        status=write_DO(card_id,DO_array,K5,1); //if it is not summer the system start supplying the radiators and when those will also be satisfied it will then turn off the compressor - the driving logic is to avoid 
        status=write_DO(card_id,DO_array,K6,0); //frequent cycles of ON-OFF for the compressor - unuseful usage of electrical contactors and mechanical sollecitation at the compressor 
        on_off_results->marker_boiler=0;
        }

    }


    return 1;
}

//this is only to start the compressor at cold start - it is differentiated by the on off regulation as it is not a regulating function but only a setpoint checker - the can bus start up will turn on devices
int boiler_starter(int *arr,int remote,int remote_set)
{
    int boiler_mid_setpoint=46;
    if(remote) {
          if (arr[12]<=remote_set) {
            return 1;
        } else {
            return 0;
        }
    } else {
        if (arr[12]<=boiler_mid_setpoint) {
            return 1;
        } else {
            return 0;
        } //this is only the curve that should be accessible some where...it will never shut down the
    }

    if (arr[12]<=boiler_mid_setpoint) { //simple check against the setpoint to define wheter or not the boiler needs heat
        return 1;
    } else {
        return 0;
    }

}


//most complex function as involves time counter 
//logic http://www.refrigeration-engineer.com/forums/archive/index.php/t-27902.html comment from user csdome 07-10-2010, 11:23 AM
int minutes_degree_regulation_down_flow(float setpoint_md_flow,time_t start_counter,int *arr,int direction, int *control_array,int remote,int remote_set)
{
    time_t stop_counter1;
    time_t minutes_1;
    static int minutes_degree_1; //this will need to keep the counter 
    static int n1=1; //this will help to keep the count of the minutes 
    int temp_difference;
    
    //the external start counter is imported
    stop_counter1=time(NULL); //set a stop watch
    minutes_1=stop_counter1-start_counter; //check time passed from when start time was set
    //printf("time %ld\n",minutes_1);
    if (minutes_1>=60*n1) //if one minute passed 
    {
        if(remote){ //just check the setpoint type
        temp_difference=arr[11]-remote_set;
        }else{
        temp_difference=arr[11]-control_array[arr[13]+20]; //this is only the curve that should be accessible some where...it will never shut down the
        }
        //printf("Temperature Difference %d\n",temp_difference);
        n1=n1+1; //next time it is necessary to match two minutes (and then three and so on) to check!
        minutes_degree_1=minutes_degree_1+temp_difference;
        /***This logic of the direction is logic and works as the turn off is also dependant of the reach of a high md setpoint for the rad flow***/
        //the reason of the direction is because the minutes degree define both when the compressor should go on and when goes off based on radiators measuring - if too cold what is counted is the difference from the setpoint
        //when is to hot it is also counted the difference from the setpoint but in this case the setpoint will be lower than the measurement! 
       
        if ((direction)&&(minutes_degree_1<0)) //the direction true is for the Compressor ON state check next function of this file - the difference should be positive, if negative is a remain from the previous state as the variable is static!
        {
            minutes_degree_1=0;
        }
        else if ((!direction)&&(minutes_degree_1>0)) //the direction false is for the Compressor OFF state check next function of this file - the difference should be negative, if positive is a remain from the previous state as the variable is static!
        {
            //printf("Minutes Degrees %d\n",minutes_degree_1);
            minutes_degree_1=0;
        }
    }
    if (!direction)
    {
        if (minutes_degree_1<=setpoint_md_flow)   //based on direction return the value when the conditions are met
        {
            n1=1;
            if ((arr[13]<17) && (arr[13]>-20))   //ignore case in which is too warm or too cold - because this is for radiators only!
            {
                minutes_degree_1=0; //reset of the MD for the next turning on / change over
                return 1;
            }
        }
    }
    else
    {
        if (minutes_degree_1>=setpoint_md_flow)   //based on direction return the value when the conditions are met - N.B. this case return to the next function of this file and NOT to the MAIN! That is why still return 1 !
        {
            n1=1;
            //if ((arr[13]<17) && (arr[13]>-20)) { //not to be ignored because is a shut down routine
            minutes_degree_1=0; //reset of the MD for the next turning on / change over
            return 1;
            //}
        }
    }

    return 0;
}


//this function is only used in the Compressor ON state
int can_bus_minutes_degree_regulation_up_flow(time_t start_counter_3, int *arr, int *control_array,int remote, int remote_set)
{

    int setpoint_md_flow=15;
    int marker_flow_off=0;
    int direction=1;
 
   //it works exactly as the function above, it just inverts the direction and the setpoint of the minutes degree - for name and this few settings is created to replace the above function in the main
    marker_flow_off=minutes_degree_regulation_down_flow(setpoint_md_flow,start_counter_3, arr, direction,control_array,remote,remote_set);
    if (marker_flow_off)
    {
        return 0;
    }


    return 1;
}







