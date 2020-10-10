#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h> 
#include <sys/time.h>
#include <time.h>
#include <sys/select.h>

static void mssleep(unsigned sleep_ms)
{
	struct timespec tim, tim2;
	tim.tv_sec = 0;
	tim.tv_nsec = sleep_ms * 1000000u;
	nanosleep(&tim , &tim2);
}

int main(int argc, char *argv[])
{
	char recvBuff[1024];
	struct sockaddr_in serv_addr, client_addr; 
	unsigned sleep_ms = 0;
	struct timeval selecy_timeout;

	selecy_timeout.tv_sec = 0;
	selecy_timeout.tv_usec = 100000;

	if(argc < 4)
	{
		printf("\n Usage: %s <ip of server> <port> <count>\n",argv[0]);
		return 1;
	} 
	if(argc >= 5)
		sleep_ms = atoi(argv[4]);

	for (int i = 0; i < atoi(argv[3]); i++)
	{
	int sockfd = {0}, n = 0, rc, rc2;
	struct timeval  tv1, tv2;
	int nfds = 0;

		fd_set fdRead, fdError;
		FD_ZERO(&fdRead);
		FD_ZERO(&fdError);

		memset(recvBuff, '0',sizeof(recvBuff));
		if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		{
			printf("\n Error : Could not create socket \n");
			return 1;
		} 
		client_addr.sin_family = AF_INET;
		client_addr.sin_port = 0; 
		client_addr.sin_addr.s_addr = 0;/*inet_addr("0.0.0.0"); */

		rc = bind(sockfd, (const struct sockaddr*)&client_addr, sizeof client_addr);
		if(rc)
			printf("\n Bind: rc %d \n", rc);

		memset(&serv_addr, '0', sizeof(serv_addr)); 

		serv_addr.sin_family = AF_INET;
		serv_addr.sin_port = htons(atoi(argv[2])); 

		if(inet_pton(AF_INET, argv[1], &serv_addr.sin_addr)<=0)
		{
			printf("\n inet_pton error occured\n");
			return 1;
		} 

		gettimeofday(&tv1, NULL);

		if( connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
		{
			printf("\n Error : Connect Failed \n");
			return 1;
		}
		nfds =  sockfd + 1;
		FD_SET(sockfd, &fdRead);
		FD_SET(sockfd, &fdError);


		if (shutdown(sockfd, SHUT_RDWR) != 0) {
			printf("\n Error : shutdown Failed \n");
			return -1;
		}
		rc = select(nfds, &fdRead, (fd_set *)NULL, &fdError, &selecy_timeout);

		/* Wait for shutdown does not work */
		rc2 = recv( sockfd, recvBuff, sizeof( recvBuff ), 0 );
/*
		printf("select return %d RD %d ERR %d recv %d\n", rc,
			FD_ISSET(sockfd, &fdRead),
			FD_ISSET(sockfd, &fdError),
			rc2);
*/
		if(sleep_ms)
			mssleep(sleep_ms);

		if (close(sockfd) !=0)
			return -1;

		gettimeofday(&tv2, NULL);

		printf ("Total time = %f s\n",
			(double) (tv2.tv_usec - tv1.tv_usec) / 1000000 +
			(double) (tv2.tv_sec - tv1.tv_sec));
	}
	return 0;
}
