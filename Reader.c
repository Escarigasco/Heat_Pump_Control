#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <modbus/modbus.h>
#include <math.h>
#include<pthread.h>

#define MAX_BUF 1600
#define SERVER_FIFO_PATH "/tmp/myfifo"

#define MAX_BUF 1600
#define SERVER_FIFO_PATH "/tmp/myfifo"


modbus_mapping_t *mb_mapping;
pthread_t tid[3];
pthread_mutex_t lock;

void update_modbus_server(int *buf)
	{
	uint16_t i;
	uint8_t bit=1;
	//int offset_reg=13;


	for(i=0; i<29; i++)
			{
			if (i<13)
					{
					mb_mapping->tab_bits[i] = buf[i]; //Digital Input
					// printf("bit register %d is %d\n",i,mb_mapping->tab_bits[i]);
					// printf("bit register %d is %d\n",13,mb_mapping->tab_bits[13]);


					}
			else if((i>=13)&&(i<32))
					{

					mb_mapping->tab_input_registers[i-13] = buf[i];
					// printf("register %d is %d\n",i,(int)(uint16_t) mb_mapping->tab_input_registers[i-13]);
					}

			//mb_mapping->tab_registers[i] = buf[i];
			//mb_mapping->tab_input_registers[i]=i;
			//printf("*register %d is %d\n",i,mb_mapping->tab_registers[0]);
			//printf("*register %d is %d\n",i,mb_mapping->tab_registers[1]);
			//printf("*register %d is %d\n",i,mb_mapping->tab_registers[2]);
			//printf("*register %d is %d\n",i,mb_mapping->tab_registers[3]);
			//printf("*register %d is %d\n",i,mb_mapping->tab_registers[4]);
			//printf("*register %d is %d\n",i,mb_mapping->tab_registers[5]);

			}


	}

void* pipe_data(void *arg)
	{
	int fd;
	char *myfifo = "/tmp/test";
	int buf[50]= {0};
	int bytes;
	time_t start;
	time_t stop;

	printf("\nwaiting for pipe opening\n");
	fd= open(myfifo, O_RDONLY|O_NONBLOCK);
	printf("\nPipe opened!\n");
	//start=time(NULL);
	while(1)
			{

			bytes=read(fd, buf, sizeof(buf));
			//			printf("\nBytes read %d\n", bytes);
			if (bytes>0)
					{
					//stop=time(NULL);
					//if(stop-start>=5)
					//{
					pthread_mutex_lock(&lock);
					update_modbus_server(buf);
					pthread_mutex_unlock(&lock);
					//start=time(NULL);
					//}
					}
			}
	close(fd);
	}

void* pipe_data_write(void *arg)
	{

	int   sending_array[8]={0};
	int   sending_array_historical[8]={0};
	int   fd;
	char *myfifo = "/tmp/test_write";
	int bytes;
	time_t start;
	time_t stop;
	int i=0;
	int marker=0;


	unlink(myfifo);
	mkfifo(myfifo, 0666);

	printf("\nwaiting for pipe opening\n");
	fd = open(myfifo, O_WRONLY);
	printf("\nPipe opened!\n");
	start=time(NULL);
	while(1)
			{
			stop=time(NULL);
			if(stop-start>5)
					{
					pthread_mutex_lock(&lock);
					for(i=0; i<8; i++)
							{
							sending_array[i]=mb_mapping->tab_registers[i];
							//      printf("sending %d is %d\n",i,sending_array[i]);
							//	printf("historical %d is %d\n",i,sending_array_historical[i]);

							if(sending_array[i]!=sending_array_historical[i])
									{
									marker=1;
									sending_array_historical[i]=sending_array[i];
									printf("sending %d is %d\n",i,sending_array[i]);
									printf("historical %d is %d\n",i,sending_array_historical[i]);

									// printf("we are sending this %d\n",sending_array[i]);
									}
							}
					start=time(NULL);
					pthread_mutex_unlock(&lock);
					}
			if(marker)
					{
					bytes= write(fd, &sending_array, sizeof(sending_array));
					marker=0;
					printf("written bytes %d\n",bytes);
					}
			}

	close(fd);



	return NULL;
	}

void* modbus_comms(void *arg)
	{

	int socket;
	modbus_t *ctx;
	int rc;
	int nPort = 502;
	uint8_t query[MODBUS_TCP_MAX_ADU_LENGTH];


	printf("Waiting for TCP connection on Port %i \n",nPort);
	ctx = modbus_new_tcp("192.168.1.4", nPort);
	printf("Context Created");
	socket = modbus_tcp_listen(ctx, 1);
	modbus_tcp_accept(ctx, &socket);
	printf("TCP connection started!\n");

	while(1)
			{

			rc = modbus_receive(ctx, query);
			if (rc >= 0)
					{
					pthread_mutex_lock(&lock);
					modbus_reply(ctx, query, rc, mb_mapping);
					pthread_mutex_unlock(&lock);
					}
			else
					{
					// Connection closed by the client or server
					printf("Con Closed.\n");
					modbus_close(ctx); // close
					// immediately start waiting for another request again
					modbus_tcp_accept(ctx, &socket);
					}

			}
	printf("Quit the loop: %s\n", modbus_strerror(errno));
	modbus_mapping_free(mb_mapping);
	close(socket);
	modbus_free(ctx);

	}



int main(void)
	{


	int err;

	//keep this
	mb_mapping = modbus_mapping_new(100,100,100,100);

	if (mb_mapping == NULL)
			{
			fprintf(stderr, "Failed to allocate the mapping: %s\n", modbus_strerror(errno));
			return -1;
			}


	if (pthread_mutex_init(&lock, NULL) != 0)
			{
			printf("\n mutex init failed\n");
			return 1;
			}


	err = pthread_create(&tid[0], NULL, &pipe_data, NULL);
	if (err != 0)
			{
			printf("\n can't create thread Pipe :[%s]", strerror(err));
			}
	err = pthread_create(&(tid[1]), NULL, &modbus_comms, NULL);
	if (err != 0)
			{
			printf("\n can't create thread Modbus :[%s]", strerror(err));
			}
	err = pthread_create(&(tid[2]), NULL, &pipe_data_write, NULL);
	if (err != 0)
			{
			printf("\n can't create thread Modbus :[%s]", strerror(err));
			}




	pthread_join(tid[0], NULL);
	pthread_join(tid[1], NULL);
	pthread_join(tid[2], NULL);

	pthread_mutex_destroy(&lock);
	modbus_mapping_free(mb_mapping);

	return 0;
	}

