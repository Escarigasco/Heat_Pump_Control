#include <stdio.h>
#include <string.h>
#include <modbus/modbus.h>
#include <errno.h>
#include <inttypes.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>
#include <float.h>
#include <time.h>
#include <pthread.h>
#include <pigpio.h>
#include <stdint.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/types.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <signal.h>
#include <ctype.h>
#include <libgen.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/uio.h>


#include "terminal.h"
#include "lib.h"


#define MAXSOCK 1    /* max. number of CAN interfaces given on the cmdline */
#define MAXIFNAMES 30 /* size of receive name index to omit ioctls */
#define MAXCOL 6      /* number of different colors for colorized output */
#define ANYDEV "any"  /* name of interface to receive from any CAN interface */
#define ANL "\r\n"    /* newline in ASC mode */

#define SILENT_INI 42 /* detect user setting on commandline */
#define SILENT_OFF 0  /* no silent mode */
#define SILENT_ANI 1  /* silent mode with animation */
#define SILENT_ON  2  /* silent mode (completely silent) */

#define BOLD    ATTBOLD
#define RED     ATTBOLD FGRED
#define GREEN   ATTBOLD FGGREEN
#define YELLOW  ATTBOLD FGYELLOW
#define BLUE    ATTBOLD FGBLUE
#define MAGENTA ATTBOLD FGMAGENTA
#define CYAN    ATTBOLD FGCYAN


const char col_on [MAXCOL][19] = {BLUE, RED, GREEN, BOLD, MAGENTA, CYAN};
const char col_off [] = ATTRESET;

static char *cmdlinename[MAXSOCK];
static __u32 dropcnt[MAXSOCK];
static __u32 last_dropcnt[MAXSOCK];
static char devname[MAXIFNAMES][IFNAMSIZ+1];
static int  dindex[MAXIFNAMES];
static int  max_devname_len; /* to prevent frazzled device name output */
const int canfd_on = 1;

#define MAXANI 4
const char anichar[MAXANI] = {'|', '/', '-', '\\'};
const char extra_m_info[4][4] = {"- -", "B -", "- E", "B E"};

extern int optind, opterr, optopt;

static volatile int running = 1;
static int marker_for_board_reset=0;

void sigterm(int signo)
{
    running = 0;
}

int idx2dindex(int ifidx, int socket)
{

    int i;
    struct ifreq ifr;

    for (i=0; i < MAXIFNAMES; i++) {
        if (dindex[i] == ifidx)
            return i;
    }

    /* create new interface index cache entry */

    /* remove index cache zombies first */
    for (i=0; i < MAXIFNAMES; i++) {
        if (dindex[i]) {
            ifr.ifr_ifindex = dindex[i];
            if (ioctl(socket, SIOCGIFNAME, &ifr) < 0)
                dindex[i] = 0;
        }
    }

    for (i=0; i < MAXIFNAMES; i++)
        if (!dindex[i]) /* free entry */
            break;

    if (i == MAXIFNAMES) {
        fprintf(stderr, "Interface index cache only supports %d interfaces.\n",
                MAXIFNAMES);
        exit(1);
    }

    dindex[i] = ifidx;

    ifr.ifr_ifindex = ifidx;
    if (ioctl(socket, SIOCGIFNAME, &ifr) < 0)
        perror("SIOCGIFNAME");

    if (max_devname_len < strlen(ifr.ifr_name))
        max_devname_len = strlen(ifr.ifr_name);

    strcpy(devname[i], ifr.ifr_name);

#ifdef DEBUG
    printf("new index %d (%s)\n", i, devname[i]);
#endif

    return i;
}

int *candump(int expected_messages,canid_t id)
{



    fd_set rdfs; //come from library select.h
    int s[MAXSOCK]; //MAXSOCK=16
    int bridge = 0;
    useconds_t bridge_delay = 0;

    unsigned char down_causes_exit = 1;
    unsigned char dropmonitor = 0;

    unsigned char silent = SILENT_INI; //SILENT_INI=42


    unsigned char view = 0;
    unsigned char log = 0;
    int count = 0;
    int rcvbuf_size = 0;
    int ret;
    int currmax;

    char *ptr, *nptr;
    struct sockaddr_can addr;
    char ctrlmsg[CMSG_SPACE(sizeof(struct timeval)) + CMSG_SPACE(sizeof(__u32))];
    struct iovec iov;
    struct msghdr msg;
    struct cmsghdr *cmsg;

    struct canfd_frame frame;
    int nbytes, i, maxdlen;
    struct ifreq ifr;
    struct timeval tv;
    struct timeval timeout_config = { 0, 0 }, *timeout_current = NULL;
    FILE *logfile = NULL;

    int v=0;
    int z=0;

    int product=1;
    int *return_array;
    int receiving_array[4] = {0};
    int running=1;
    time_t start_counter;
    time_t stop_counter;



    char* argv_1[2] = {"candump","can0"};

    signal(SIGTERM, sigterm);
    signal(SIGHUP, sigterm);
    signal(SIGINT, sigterm);

    if (expected_messages==10) {
        return_array=calloc(11,sizeof(int));
    } else if (expected_messages==1) {
        return_array=calloc(16,sizeof(int));
    } else if (expected_messages==0) {
        return_array=calloc(2,sizeof(int));
    } else {
        return_array=NULL;
    }






    silent = SILENT_OFF; // default output - this my be only graphical
    currmax = 1; //argc - optind; // find real number of CAN devices



    for (i=0; i < currmax; i++) {

        ptr = argv_1[optind+i];
        nptr = strchr(ptr, ','); //this function return the pointer to the first occurrence of character ',' of the string pointed by pointer ptr

#ifdef DEBUG
        printf("open %d '%s'.\n", i, ptr);
#endif

        s[i] = socket(PF_CAN, SOCK_RAW, CAN_RAW);
        if (s[i] < 0) {
            perror("socket");
            return_array[1]=-1;
            return return_array;
        }

        cmdlinename[i] = ptr; // save pointer to cmdline name of this socket

        if (nptr)
            nbytes = nptr - ptr;  // interface name is up the first ','
        else
            nbytes = strlen(ptr); // no ',' found => no filter definitions


        if (nbytes > max_devname_len)
            max_devname_len = nbytes; // for nice printing

        addr.can_family = AF_CAN;

        memset(&ifr.ifr_name, 0, sizeof(ifr.ifr_name));
        strncpy(ifr.ifr_name, ptr, nbytes);

#ifdef DEBUG
        printf("using interface name '%s'.\n", ifr.ifr_name);
#endif

        if (strcmp(ANYDEV, ifr.ifr_name)) {
            if (ioctl(s[i], SIOCGIFINDEX, &ifr) < 0) {
                perror("SIOCGIFINDEX");
                exit(1);
            }
            addr.can_ifindex = ifr.ifr_ifindex;
        } else
            addr.can_ifindex = 0; // any can interface



        // try to switch the socket into CAN FD mode
        setsockopt(s[i], SOL_CAN_RAW, CAN_RAW_FD_FRAMES, &canfd_on, sizeof(canfd_on));

        if (rcvbuf_size) {

            int curr_rcvbuf_size;
            socklen_t curr_rcvbuf_size_len = sizeof(curr_rcvbuf_size);

            // try SO_RCVBUFFORCE first, if we run with CAP_NET_ADMIN
            if (setsockopt(s[i], SOL_SOCKET, SO_RCVBUFFORCE,
                           &rcvbuf_size, sizeof(rcvbuf_size)) < 0) {
#ifdef DEBUG
                printf("SO_RCVBUFFORCE failed so try SO_RCVBUF ...\n");
#endif
                if (setsockopt(s[i], SOL_SOCKET, SO_RCVBUF,
                               &rcvbuf_size, sizeof(rcvbuf_size)) < 0) {
                    perror("setsockopt SO_RCVBUF");
                    return_array[1]=-1;
                    return return_array;
                }

                if (getsockopt(s[i], SOL_SOCKET, SO_RCVBUF,
                               &curr_rcvbuf_size, &curr_rcvbuf_size_len) < 0) {
                    perror("getsockopt SO_RCVBUF");
                    return_array[1]=-1;
                    return return_array;
                }

                // Only print a warning the first time we detect the adjustment
                // n.b.: The wanted size is doubled in Linux in net/sore/sock.c
                if (!i && curr_rcvbuf_size < rcvbuf_size*2)
                    fprintf(stderr, "The socket receive buffer size was "
                            "adjusted due to /proc/sys/net/core/rmem_max.\n");
            }
        }


        if (dropmonitor) {

            const int dropmonitor_on = 1;

            if (setsockopt(s[i], SOL_SOCKET, SO_RXQ_OVFL,
                           &dropmonitor_on, sizeof(dropmonitor_on)) < 0) {
                perror("setsockopt SO_RXQ_OVFL not supported by your Linux Kernel");
                return_array[1]=-1;
                return return_array;
            }
        }

        if (bind(s[i], (struct sockaddr *)&addr, sizeof(addr)) < 0) {
            perror("bind");
            return_array[1]=-1;
            return return_array;
        }
    }


    /* these settings are static and can be held out of the hot path */
    iov.iov_base = &frame;

    msg.msg_name = &addr;

    msg.msg_iov = &iov;

    msg.msg_iovlen = 1;

    msg.msg_control = &ctrlmsg;

    start_counter=time(NULL);
    while(running) {


        i=0;
        FD_ZERO(&rdfs);
        //for (i=0; i<currmax; i++)
        FD_SET(s[i], &rdfs);

        if (timeout_current)
            *timeout_current = timeout_config;

        if ((ret = select(s[currmax-1]+1, &rdfs, NULL, NULL, timeout_current)) <= 0) {
            //perror("select");
            running = 0;
            continue;
        }



        if (FD_ISSET(s[i], &rdfs)) {

            int idx;

            /* these settings may be modified by recvmsg() */
            iov.iov_len = sizeof(frame);
            msg.msg_namelen = sizeof(addr);
            msg.msg_controllen = sizeof(ctrlmsg);
            msg.msg_flags = 0;

            nbytes = recvmsg(s[i], &msg, 0); //reads and returns the number of byte contained in the message

            idx = idx2dindex(addr.can_ifindex, s[i]);

            if (nbytes < 0) {
                if ((errno == ENETDOWN) && !down_causes_exit) {
                    fprintf(stderr, "%s: interface down\n", devname[idx]);
                    continue;
                }
                perror("read");
                return_array[1]=-1;
                return return_array;
            }

            if ((size_t)nbytes == CAN_MTU)
                maxdlen = CAN_MAX_DLEN;
            else if ((size_t)nbytes == CANFD_MTU)
                maxdlen = CANFD_MAX_DLEN;
            else {
                fprintf(stderr, "read: incomplete CAN frame\n");
                return_array[1]=-1;
                return return_array;
            }

            if (count && (--count == 0))
                running = 0;

            if (bridge) {
                if (bridge_delay)
                    usleep(bridge_delay);

                nbytes = write(bridge, &frame, nbytes);
                if (nbytes < 0) {
                    perror("bridge write");
                    return_array[1]=-1;
                    return return_array;
                } else if ((size_t)nbytes != CAN_MTU && (size_t)nbytes != CANFD_MTU) {
                    fprintf(stderr,"bridge write: incomplete CAN frame\n");
                    return_array[1]=-1;
                    return return_array;
                }
            }

            for (cmsg = CMSG_FIRSTHDR(&msg);
                 cmsg && (cmsg->cmsg_level == SOL_SOCKET);
                 cmsg = CMSG_NXTHDR(&msg,cmsg)) {
                if (cmsg->cmsg_type == SO_TIMESTAMP)
                    memcpy(&tv, CMSG_DATA(cmsg), sizeof(tv));
                else if (cmsg->cmsg_type == SO_RXQ_OVFL)
                    memcpy(&dropcnt[i], CMSG_DATA(cmsg), sizeof(__u32));
            }

            /* check for (unlikely) dropped frames on this specific socket */
            if (dropcnt[i] != last_dropcnt[i]) {

                __u32 frames = dropcnt[i] - last_dropcnt[i];

                if (silent != SILENT_ON)
                    printf("DROPCOUNT: dropped %d CAN frame%s on '%s' socket (total drops %d)\n",
                           frames, (frames > 1)?"s":"", devname[idx], dropcnt[i]);

                if (log)
                    fprintf(logfile, "DROPCOUNT: dropped %d CAN frame%s on '%s' socket (total drops %d)\n",
                            frames, (frames > 1)?"s":"", devname[idx], dropcnt[i]);

                last_dropcnt[i] = dropcnt[i];
            }

            /* once we detected a EFF frame indent SFF frames accordingly */
            if (frame.can_id & CAN_EFF_FLAG)
                view |= CANLIB_VIEW_INDENT_SFF;






        }
        /***Analogue Routine***/
        if (expected_messages==0xA) {
            if ((frame.data[0]==1) && (frame.can_id==id)) {
                for(z=1; z<4; z++) {
                    //printf("data[%d] - %d\n",z,frame.data[z]);
                    if (frame.can_id==id) {
                        receiving_array[z-1]=frame.data[z];
                        //printf("receiving_array[%d] - %d\n",v,receiving_array[v]);
                    }
                }
                return_array[receiving_array[0]]=(receiving_array[1] << 8) + receiving_array[2];

                product=1;
                for (v=1; v<=expected_messages && product; v++) {
                    product&=return_array[v] !=0;
                    //printf("value %d\n", return_array[v]);
                }

                if (product) {
                    running=0;
                }
            }
        }

        /***Digital Input Routine***/
        else if (expected_messages==1) {
            //return_array=calloc(17,sizeof(int));
            //v=0;
            if ((frame.data[0]==1) && (frame.can_id==id)) {
                for(z=1; z<3; z++) {

                    //printf("data[%d] - %d\n",z,frame.data[z]);
                    receiving_array[z-1]=frame.data[z]; //z-1 because the first frame data is not required
                    //v=v+1;

                }

                for (i=0; i<16; i++) {
                    if (i<8) {
                        return_array[i]=receiving_array[1]&1;
                        receiving_array[1]>>=1;
                    } else {
                        return_array[i]=receiving_array[0]&1;
                        receiving_array[0]>>=1;
                    }
                }
                running=0;
            }
        }

        /***Digital Output***/
        else if ((expected_messages==0)&&(frame.data[0]==0xCC)&&(frame.can_id==id)) {

            /*receiving_array[0]=frame.can_id;
            receiving_array[1]=frame.data[0];
            return_array[0]=receiving_array[0];
            return_array[1]=receiving_array[1];*/
            running=0;
        }

        fflush(stdout);
        stop_counter=time(NULL);
        if (stop_counter-start_counter>5) {
            gpioWrite(17,0);
            gpioWrite(17,1);
            marker_for_board_reset++;

        }




    }


    close(s[i]);

    if (bridge)
        close(bridge);


    printf("The IO have been resetted %d times\n", marker_for_board_reset);
    return return_array;
}
