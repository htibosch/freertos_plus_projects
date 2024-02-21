/*
    FreeRTOS V8.2.2 - Copyright (C) 2015 Real Time Engineers Ltd.
    All rights reserved
*/

/*
 * A set of tasks are created that send TCP echo requests to the standard echo
 * port (port 7) on the IP address set by the configECHO_SERVER_ADDR0 to
 * configECHO_SERVER_ADDR3 constants, then wait for and verify the reply
 * (another demo is avilable that demonstrates the reception being performed in
 * a task other than that from with the request was made).
 *
 * See the following web page for essential demo usage and configuration
 * details:
 * http://www.FreeRTOS.org/FreeRTOS-Plus/FreeRTOS_Plus_TCP/examples_FreeRTOS_simulator.html
 */

/* Standard includes. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

/* FreeRTOS+TCP includes. */
#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"
#if( ipconfigMULTI_INTERFACE != 0 )
	#include "FreeRTOS_Routing.h"
#endif

#include "echo_client.h"

/* Exclude the whole file if FreeRTOSIPConfig.h is configured to use UDP only. */
#if( ipconfigUSE_TCP == 1 )

/* The echo tasks create a socket, send out a number of echo requests, listen
for the echo reply, then close the socket again before starting over.  This
delay is used between each iteration to ensure the network does not get too
congested. */
#define echoLOOP_DELAY	( ( TickType_t ) 150 / portTICK_PERIOD_MS )

/* The echo server is assumed to be on port 7, which is the standard echo
protocol port. */

#define echoECHO_PORT	( 80 )

/* If ipconfigUSE_TCP_WIN is 1 then the Tx socket will use a buffer size set by
ipconfigTCP_TX_BUF_LEN, and the Tx window size will be
configECHO_CLIENT_TX_WINDOW_SIZE times the buffer size.  Note
ipconfigTCP_TX_BUF_LEN is set in FreeRTOSIPConfig.h as it is a standard TCP/IP
stack constant, whereas configECHO_CLIENT_TX_WINDOW_SIZE is set in
FreeRTOSConfig.h as it is a demo application constant. */
#ifndef configECHO_CLIENT_TX_WINDOW_SIZE
	#define configECHO_CLIENT_TX_WINDOW_SIZE	2
#endif

/* If ipconfigUSE_TCP_WIN is 1 then the Rx socket will use a buffer size set by
ipconfigTCP_RX_BUFFER_LENGTH, and the Rx window size will be
configECHO_CLIENT_RX_WINDOW_SIZE times the buffer size.  Note
ipconfigTCP_RX_BUFFER_LENGTH is set in FreeRTOSIPConfig.h as it is a standard TCP/IP
stack constant, whereas configECHO_CLIENT_RX_WINDOW_SIZE is set in
FreeRTOSConfig.h as it is a demo application constant. */
#ifndef configECHO_CLIENT_RX_WINDOW_SIZE
	#define configECHO_CLIENT_RX_WINDOW_SIZE	2
#endif

//2404:6800:4003:c02::5e
//66.96.149.18
/*-----------------------------------------------------------*/

/*
 * Uses a socket to send data to, then receive data from, the standard echo
 * port number 7.
 */
static void prvEchoClientTask( void *pvParameters );

void printBuffer(const char* apBuffer, int aLen, int aLineLen, const char *apPrefix);

/*-----------------------------------------------------------*/

/* Counters for each created task - for inspection only. */
static uint32_t ulTxRxCycles[ echoNUM_ECHO_CLIENTS ]  = { 0 },
				ulConnections[ echoNUM_ECHO_CLIENTS ] = { 0 };

static TaskHandle_t xSocketTaskHandles[ echoNUM_ECHO_CLIENTS ];

/*-----------------------------------------------------------*/

void vStartTCPEchoClientTasks_SingleTasks( uint16_t usTaskStackSize, UBaseType_t uxTaskPriority )
{
BaseType_t x;
static char pcNames[ echoNUM_ECHO_CLIENTS ][ 8 ];

	/* Create the echo client tasks. */
	for( x = 0; x < echoNUM_ECHO_CLIENTS; x++ )
	{
		sprintf( pcNames[ x ], "Echo%ld", x );
		xTaskCreate( 	prvEchoClientTask,	/* The function that implements the task. */
						pcNames[ x ],		/* Just a text name for the task to aid debugging. */
						usTaskStackSize,	/* The stack size is defined in FreeRTOSIPConfig.h. */
						( void * ) x,		/* The task parameter, not used in this case. */
						uxTaskPriority,		/* The priority assigned to the task is defined in FreeRTOSConfig.h. */
						NULL );				/* The task handle is not used. */
	}
}
/*-----------------------------------------------------------*/

static int startEchos[ echoNUM_ECHO_CLIENTS ];
static char pcHostNames[ echoNUM_ECHO_CLIENTS ][ 40 ];

void wakeupEchoClient( int iIndex, const char *pcHost )
{
	if( iIndex >= 0 && iIndex < echoNUM_ECHO_CLIENTS && xSocketTaskHandles[ iIndex ] != NULL )
	{
		startEchos[ iIndex ]++;
		snprintf( pcHostNames[ iIndex ], sizeof pcHostNames[ iIndex ], "%s", pcHost );
		xTaskNotifyGive( xSocketTaskHandles[ iIndex ] );
	}
}

const char get_command_1[] =
	"GET /index.html HTTP/1.1\x0d\x0a"
	"Host: htibosch.net\x0d\x0a"
	"User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:68.0) Gecko/20100101 Firefox/68.0\x0d\x0a"
	"Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\x0d\x0a"
	"Accept-Language: en-US,en;q=0.5\x0d\x0a"
	"DNT: 1\x0d\x0a"
	"Connection: keep-alive\x0d\x0a"
	"Upgrade-Insecure-Requests: 1\x0d\x0a"
	"If-Modified-Since: Fri, 16 Aug 2019 05:18:19 GMT\x0d\x0a"
	"\x0d\x0a";

//	HTTP/1.1 304 Not Modified
//	Server: Apache/2
//	X-Cnection: close
//	Date: Fri, 16 Aug 2019 05:20:39 GMT

const char get_command_2[] =
	"GET /favicon.ico HTTP/1.1\x0d\x0a"
	"Host: google.nl\x0d\x0a"
	"User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:68.0) Gecko/20100101 Firefox/68.0\x0d\x0a"
	"Accept: image/webp,*/*\x0d\x0a"
	"Accept-Language: en-US,en;q=0.5\x0d\x0a"
	"Accept-Encoding: gzip, deflate\x0d\x0a"
	"DNT: 1\x0d\x0a"
	"Connection: keep-alive\x0d\x0a"
	"\x0d\x0a";

//	HTTP/1.1 200 OK
//	Date: Fri, 16 Aug 2019 06:09:07 GMT
//	Content-Type: image/x-icon
//	Content-Length: 0
//	Server: Apache/2
//	Cache-Control: max-age=86400
//	Accept-Ranges: bytes
//	Age: 331
//	X-Cnection: close

static char pcBuffer[ 512 ];

static void prvEchoClientTask( void *pvParameters )
{
Socket_t xSocket;
static struct freertos_sockaddr xEchoServerAddress;

BaseType_t xReceivedBytes, xReturned, xInstance;
BaseType_t lTransmitted;
TickType_t xTimeOnEntering;

#if( ipconfigUSE_TCP_WIN == 1 )

	WinProperties_t xWinProps;

	/* Fill in the buffer and window sizes that will be used by the socket. */
	xWinProps.lTxBufSize = ipconfigTCP_TX_BUFFER_LENGTH;
	xWinProps.lTxWinSize = configECHO_CLIENT_TX_WINDOW_SIZE;
	xWinProps.lRxBufSize = ipconfigTCP_RX_BUFFER_LENGTH;
	xWinProps.lRxWinSize = configECHO_CLIENT_RX_WINDOW_SIZE;

#endif /* ipconfigUSE_TCP_WIN */

#if( ipconfigUSE_IPv6 != 0 )
	struct freertos_sockaddr *pxAddress = ( struct freertos_sockaddr * ) &xEchoServerAddress;
#else
	struct freertos_sockaddr *pxAddress = &xEchoServerAddress;
#endif
	/* This task can be created a number of times.  Each instance is numbered
	to enable each instance to use a different Rx and Tx buffer.  The number is
	passed in as the task's parameter. */
	xInstance = ( BaseType_t ) pvParameters;

	if( xInstance >= 0 && xInstance < echoNUM_ECHO_CLIENTS )
	{
		xSocketTaskHandles[ xInstance ] = xTaskGetCurrentTaskHandle();
	}

	for( ;; )
	{
	int rc;
	struct freertos_sockaddr xAddress;
	/* Rx and Tx time outs are used to ensure the sockets do not wait too long for
	missing data. */
	const TickType_t xReceiveTimeOut = pdMS_TO_TICKS( 4000 );
	const TickType_t xSendTimeOut = pdMS_TO_TICKS( 2000 );
#if( ipconfigUSE_IPv6 != 0 )
	IPv6_Address_t xIPAddress_IPv6;
	struct freertos_sockaddr6 xLocalAddress;
#else
	struct freertos_sockaddr xLocalAddress;
#endif

#if( ipconfigMULTI_INTERFACE != 0 )
	NetworkEndPoint_t *pxEndPoint;
#endif


		while( startEchos[ xInstance ] == 0 )
		{
			ulTaskNotifyTake( pdTRUE, 1000 );
		}
		startEchos[ xInstance ] = 0;

		#if( ipconfigUSE_IPv6 != 0 )
		if( FreeRTOS_inet_pton6( pcHostNames[ xInstance ], xIPAddress_IPv6.ucBytes ) != 0 )
		{


			xEchoServerAddress.sin_len = sizeof( struct freertos_sockaddr6 );
			xEchoServerAddress.sin_family = FREERTOS_AF_INET6;
			xEchoServerAddress.sin_port = FreeRTOS_htons( echoECHO_PORT );
			memcpy( xEchoServerAddress.sin_addrv6.ucBytes, xIPAddress_IPv6.ucBytes, ipSIZE_OF_IPv6_ADDRESS );
		}
		else
		#endif
		{
			pxAddress->sin_len = sizeof( struct freertos_sockaddr );
			pxAddress->sin_family = FREERTOS_AF_INET;
			pxAddress->sin_port = FreeRTOS_htons( echoECHO_PORT );
			pxAddress->sin_addr = FreeRTOS_inet_addr( pcHostNames[ xInstance ] );
		}

		/* Create a TCP socket. */
		xSocket = FreeRTOS_socket( FREERTOS_AF_INET, FREERTOS_SOCK_STREAM, FREERTOS_IPPROTO_TCP );
		configASSERT( xSocket != FREERTOS_INVALID_SOCKET );

		memset( &xAddress, '\0', sizeof xAddress );

#if( ipconfigMULTI_INTERFACE != 0 )
	#if( ipconfigUSE_IPv6 != 0 )
		if( xEchoServerAddress.sin_family == FREERTOS_AF_INET6 )
		{
			pxEndPoint = FreeRTOS_FindEndPointOnNetMask_IPv6( &( xEchoServerAddress.sin_addrv6 ) );
			if( pxEndPoint == NULL )
			{
				pxEndPoint = FreeRTOS_FindGateWay( ipTYPE_IPv6 );
			}
			if( pxEndPoint != NULL )
			{
				//memcpy( xEchoServerAddress.sin_addrv6.ucBytes, pxEndPoint->ipv6.xIPAddress.ucBytes, ipSIZE_OF_IPv6_ADDRESS );
			}
		}
		else
	#endif
		{
			pxEndPoint = FreeRTOS_FindEndPointOnNetMask( pxAddress->sin_addr, 9999 );

			if( pxEndPoint != NULL )
			{
				xAddress.sin_addr = pxEndPoint->ipv4_settings.ulIPAddress;
			}
		}
#endif
		FreeRTOS_bind( xSocket, &xAddress, sizeof xAddress );

		/* Set a time out so a missing reply does not cause the task to block
		indefinitely. */
		FreeRTOS_setsockopt( xSocket, 0, FREERTOS_SO_RCVTIMEO, &xReceiveTimeOut, sizeof( xReceiveTimeOut ) );
		FreeRTOS_setsockopt( xSocket, 0, FREERTOS_SO_SNDTIMEO, &xSendTimeOut, sizeof( xSendTimeOut ) );

		#if( ipconfigUSE_TCP_WIN == 1 )
		{
			/* Set the window and buffer sizes. */
			FreeRTOS_setsockopt( xSocket, 0, FREERTOS_SO_WIN_PROPERTIES, ( void * ) &xWinProps,	sizeof( xWinProps ) );
		}
		#endif /* ipconfigUSE_TCP_WIN */

		FreeRTOS_GetLocalAddress( xSocket, &xLocalAddress );
		/* Connect to the echo server. */
		rc = FreeRTOS_connect( xSocket, ( struct freertos_sockaddr * ) &xEchoServerAddress, sizeof( xEchoServerAddress ) );

#if( ipconfigUSE_IPv6 != 0 )
	struct freertos_sockaddr *pxLocalAddress = ( struct freertos_sockaddr * ) &xLocalAddress;
#else
	struct freertos_sockaddr *pxLocalAddress = &xLocalAddress;
#endif

	#if( ipconfigUSE_IPv6 != 0 )
		if( pxAddress->sin_family == FREERTOS_AF_INET6 )
		{
			FreeRTOS_printf( ( "FreeRTOS_connect to %pip port %u: rc %d\n",
				xEchoServerAddress.sin_addrv6.ucBytes,
				FreeRTOS_ntohs( pxAddress->sin_port ),
				rc ) );
		}
		else
	#endif
		{
			FreeRTOS_printf( ( "FreeRTOS_connect from %lxip port %u to %lxip port %u: rc %d\n",
				FreeRTOS_ntohl( pxLocalAddress->sin_addr ),
				FreeRTOS_ntohs( pxLocalAddress->sin_port ),
				FreeRTOS_ntohl( pxAddress->sin_addr ),
				FreeRTOS_ntohs( pxAddress->sin_port ),
				rc ) );
		}

		if( rc == 0 )
		{
			ulConnections[ xInstance ]++;

			/* Send a HTTP request. */
			{
			BaseType_t xLoop;
				/* Send the string to the socket. */
				lTransmitted = FreeRTOS_send(	xSocket,						/* The socket being sent to. */
												( void * ) get_command_1,		/* The data being sent. */
												sizeof( get_command_1 ) - 1,	/* The length of the data being sent. */
												0 );							/* No flags. */
FreeRTOS_printf( ( "FreeRTOS_send : rc %ld\n", lTransmitted ) );
				if( lTransmitted < 0 )
				{
					/* Error? */
					break;
				}

				/* Clear the buffer into which the echoed string will be
				placed. */
				memset( ( void * ) pcBuffer, 0x00, sizeof( pcBuffer ) );
				xReceivedBytes = 0;

				/* Receive data echoed back to the socket. */
				for( xLoop = 0; xLoop < 10; xLoop++ )
				{
					xReturned = FreeRTOS_recv( xSocket,				/* The socket being received from. */
											   pcBuffer,			/* The buffer into which the received data will be written. */
											   sizeof( pcBuffer ),	/* The size of the buffer provided to receive the data. */
											   0 );					/* No flags. */

					FreeRTOS_printf( ( "FreeRTOS_recv : rc %ld\n", xReturned ) );

					if( xReturned < 0 )
					{
						/* Error occurred.  Latch it so it can be detected
						below. */
						xReceivedBytes = xReturned;
						break;
					}
					else if( xReturned == 0 )
					{
						/* Timed out. */
						break;
					}
					else
					{
						/* Keep a count of the bytes received so far. */
						xReceivedBytes += xReturned;
						printBuffer( pcBuffer, xReturned, 129, "");
					}
				}	/* for( xLoop = 0; xLoop < 10; xLoop++ ) */
			}

			/* Finished using the connected socket, initiate a graceful close:
			FIN, FIN+ACK, ACK. */
			FreeRTOS_shutdown( xSocket, FREERTOS_SHUT_RDWR );

			/* Expect FreeRTOS_recv() to return an error once the shutdown is
			complete. */
			xTimeOnEntering = xTaskGetTickCount();
			do
			{
				xReturned = FreeRTOS_recv( xSocket,	/* The socket being received from. */
					pcBuffer,						/* The buffer into which the received data will be written. */
					sizeof( pcBuffer ),				/* The size of the buffer provided to receive the data. */
					0 );

				if( xReturned < 0 )
				{
					break;
				}

			} while( ( xTaskGetTickCount() - xTimeOnEntering ) < xReceiveTimeOut );
		}

		/* Close this socket before looping back to create another. */
		FreeRTOS_closesocket( xSocket );

		/* Pause for a short while to ensure the network is not too
		congested. */
/*		vTaskDelay( echoLOOP_DELAY ); */
	}
}
/*-----------------------------------------------------------*/

BaseType_t xAreSingleTaskTCPEchoClientsStillRunning( void )
{
static uint32_t ulLastEchoSocketCount[ echoNUM_ECHO_CLIENTS ] = { 0 }, ulLastConnections[ echoNUM_ECHO_CLIENTS ] = { 0 };
BaseType_t xReturn = pdPASS, x;

	/* Return fail is the number of cycles does not increment between
	consecutive calls. */
	for( x = 0; x < echoNUM_ECHO_CLIENTS; x++ )
	{
		if( ulTxRxCycles[ x ] == ulLastEchoSocketCount[ x ] )
		{
			xReturn = pdFAIL;
		}
		else
		{
			ulLastEchoSocketCount[ x ] = ulTxRxCycles[ x ];
		}

		if( ulConnections[ x ] == ulLastConnections[ x ] )
		{
			xReturn = pdFAIL;
		}
		else
		{
			ulConnections[ x ] = ulLastConnections[ x ];
		}
	}

	return xReturn;
}

void printBuffer(const char* apBuffer, int aLen, int aLineLen, const char *apPrefix)
{
	const char *ptr = apBuffer;
	const char *end = apBuffer + aLen;
	for (;;) {
		const char *next = ptr;
		const char *eot;
		/* Find the first nul, newline of end of text. */
		for (;;) {
			if (next >= end || *next == '\0') {
				eot = next;
				next = NULL;
				break;
			}
			if (*next == '\n' || *next == '\r') {
				char eol = *next == '\n' ? '\r' : '\n';
				eot = next;
				do {
					next++;
				} while (*next == eol);
				break;
			}
			if ((int)(next - ptr) >= aLineLen) {
				eot = next;
				break;
			}
			next++;
		}
		{
			char save = *eot;
			*((char*)eot) = '\0';
			FreeRTOS_printf( ( "%s%s\n", apPrefix, ptr ) );
			*((char*)eot) = save;
		}
		if (next == NULL)
			break;
		ptr = next;
	}
}

#endif /* ipconfigUSE_TCP */

