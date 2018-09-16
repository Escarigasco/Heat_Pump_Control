#include "radiator_pump_functioning.h"
#include <time.h>
#include <linux/can.h>

//this is a single function file that regulates the radiator pump
int can_bus_radiator_pump_functioning(int functioning, int marker_on_pump, int marker_max_outdoor,int *DO_array,int start_stop)
{
//this pump is independent by the compressor state and needs to be on 5 minutes if it has been off for more than a week
//it includes some counter, some static to check the time in every state of the compressor

    time_t  week_end;
    time_t  counter;
    static time_t week_start;
    static int k=0;
    int K4=3;
    int status;
    canid_t DO_card=204;

    //this routine only works if we setted summer stop operation - which is the standard and this Qvantum machine will only use this - double check shall be change mind
    if (start_stop) { //the system has to be on - if is not but this function is called then the pump should go off - to be check against fail safe shut down, but redundancy in security is ok
        if (functioning) {
            if (marker_max_outdoor) { //if setpoint of external temperature is met 

                if ((marker_on_pump)&&(k==0)) { //if the pump is on, it can go off
                    marker_on_pump=0;    //read return value at the end of the function.
                    status=write_DO(DO_card, DO_array,K4,0); //go off and start counting the time from when goes off
                    week_start=time(NULL); //this won't be touched again before next change in temperature
                }
                if (!marker_on_pump) { //elseif? if the pump is off
                    week_end=time(NULL); //set a stop watch 
                    counter=week_end-week_start; //and check time from when it has been shutted
                    
                    if ((counter>=604800)&&(k==0)) { //is one week hit?
                        status=write_DO(DO_card, DO_array,K4,1); //then go on
                        
                        marker_on_pump=1; //it's on
                        k=1;
                    }
                }
                
                if ((marker_on_pump)&&(k==1)){ // count down at 5 min
                    
                    week_end=time(NULL);
                    counter=week_end-week_start;

                    if ((counter>=604800+300)&&(k==1)) { //the +300 are the 5 minutes
                        write_DO(DO_card, DO_array,K4,0); //if matched can pump can go off
                        k=0;
                        //k=1;
                        marker_on_pump=0;
                        week_start=time(NULL); //so needs to restart the counter
                    }

                }

            } else { //if marker_max_outdoor=0 just turn on again
                if (!marker_on_pump) {
                    status=write_DO(DO_card, DO_array,K4,1);
                    marker_on_pump=1;
                    k=0;
                }
            }
        } else { //if the functioning type is not summer stop - the pumps just need to stay on all the time
            if (!marker_on_pump) { //is it off? turn on
                status=write_DO(DO_card, DO_array,K4,1);
                marker_on_pump=1;
                k=0;
            }
        }
    } else { //if the system is shutted then the pump can just go off
        status=write_DO(DO_card, DO_array,K4,0);
        marker_on_pump=0;
    }
    return marker_on_pump; //this is sort of a self feedback - in the main this value update a variable that is also an input of this function. It is not only a self feedback but also a work around the static variable
    //which is not the best practice possible.
}
