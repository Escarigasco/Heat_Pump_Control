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
#include <pigpio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>

#include "terminal.h"
#include "lib.h"

int cansend(canid_t id, unsigned char data_1, unsigned char data_2, unsigned char data_3)
{
	int s; /* can raw socket */
	int required_mtu;
	int mtu;
	int enable_canfd = 1;
	struct sockaddr_can addr;
	struct canfd_frame frame;
	struct ifreq ifr;


	required_mtu=16;
	frame.can_id=id;
    if (data_1<0xAA)
    {
    frame.len=5;
	frame.data[0]=data_1;
	frame.data[1]=data_2;
	frame.data[2]=data_3;
	frame.data[3]=0;
	frame.data[4]=0;
	/*frame.data[5]=0;
	frame.data[6]=0;
	frame.data[7]=0;*/
    }
        else
        {
    frame.len=1;
	frame.data[0]=data_1;
	/*frame.data[1]=data_2;
	frame.data[2]=data_3;
	frame.data[3]=0; //need to be assessed the high side or the push pull mode
	frame.data[4]=0; //need to be assessed the high side or the push pull mode
	frame.data[5]=0;
	frame.data[6]=0;
	frame.data[7]=0;*/
        }




	/* open socket */
	if ((s = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0) {
		perror("socket");
		return 1;
	}

	strncpy(ifr.ifr_name, "can0" , IFNAMSIZ - 1);
	ifr.ifr_name[IFNAMSIZ - 1] = '\0';
	ifr.ifr_ifindex = if_nametoindex(ifr.ifr_name);
	if (!ifr.ifr_ifindex) {
		perror("if_nametoindex");
		return 1;
	}

	addr.can_family = AF_CAN;
	addr.can_ifindex = ifr.ifr_ifindex;

	if (required_mtu > CAN_MTU) {

		/* check if the frame fits into the CAN netdevice */
		if (ioctl(s, SIOCGIFMTU, &ifr) < 0) {
			perror("SIOCGIFMTU");
			return 1;
		}
		mtu = ifr.ifr_mtu;

/*
		if (mtu != CANFD_MTU) {
			printf("CAN interface is not CAN FD capable - sorry.\n");
			return 1;
		}*/

		/* interface is ok - try to switch the socket into CAN FD mode */
		if (setsockopt(s, SOL_CAN_RAW, CAN_RAW_FD_FRAMES,
			       &enable_canfd, sizeof(enable_canfd))){
			printf("error when enabling CAN FD support\n");
			return 1;
		}

		/* ensure discrete CAN FD length values 0..8, 12, 16, 20, 24, 32, 64 */
		frame.len = can_dlc2len(can_len2dlc(frame.len));
	}

	/* disable default receive filter on this RAW socket */
	/* This is obsolete as we do not read from the socket at all, but for */
	/* this reason we can remove the receive list in the Kernel to save a */
	/* little (really a very little!) CPU usage.                          */
	setsockopt(s, SOL_CAN_RAW, CAN_RAW_FILTER, NULL, 0);

	if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		perror("bind");
		return 1;
	}

	/* send frame */
	if (write(s, &frame, required_mtu) != required_mtu) {
		perror("write");
		return 1;
	}

	close(s);

	return 0;
}
