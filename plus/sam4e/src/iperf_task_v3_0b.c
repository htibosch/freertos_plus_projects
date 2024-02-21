/*
	A FreeRTOS+TCP demo program:
	A simple example of a TCP/UDP iperf server, see https://iperf.fr

	It uses a single task which will store some client information in a:
	    List_t xTCPClientList;

	The task will sleep constantly by calling:
	    xResult = FreeRTOS_select( xSocketSet, xBlockingTime );

	Command examples for testing:

	iperf3 -c 172.16.0.100  --port 5001 --bytes 1M
	iperf3 -c 192.168.2.124  --port 5001 --bytes 1M
	iperf3 -c fe80::8d11:cd9b:8b66:4a80  --port 5001 --bytes 1M
	iperf3 -c fe80::8d11:cd9b:8b66:4a81  --port 5001 --bytes 1M
	iperf3 -c fe80::6802:9887:8670:b37c  --port 5001 --bytes 1M
	
	ping fe80::8d11:cd9b:8b66:4a80
	ping fe80::8d11:cd9b:8b66:4a81
	iperf3 -s --port 5001
	iperf3 -c zynq  --port 5001 --bytes 1024
	iperf3 -c laptop  --port 5001 --bytes 1024
	fe80::8d11:cd9b:8b66:4a80
	TCP :  iperf3 -c 192.168.2.116  --port 5001 --bytes 1024
	UDP :  iperf3 -c 192.168.2.116  --port 5001 --udp --bandwidth 50M --mss 1460 --bytes 1024
*/

/* Standard includes. */
#include <stdio.h>
#include <time.h>

/* FreeRTOS includes. */
#include <FreeRTOS.h>
#include "task.h"
#include "timers.h"

/* FreeRTOS+TCP includes. */
#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"
#include "FreeRTOS_TCP_IP.h"

#include "iperf_task.h"

#define ipconfigIPERF_DUAL_TCP	0

/* Put the TCP server at this port number: */
#ifndef ipconfigIPERF_TCP_ECHO_PORT
	/* 5001 seems to be the standard TCP server port number. */
	#define ipconfigIPERF_TCP_ECHO_PORT				5001
#endif

/* Put the TCP server at this port number: */
#ifndef ipconfigIPERF_UDP_ECHO_PORT
	/* 5001 seems to be the standard UDP server port number. */
	#define ipconfigIPERF_UDP_ECHO_PORT				5001
#endif

#ifndef ipconfigIPERF_STACK_SIZE_IPERF_TASK
	/* Stack size needed for vIPerfTask(), a bit of a guess: */
	#define ipconfigIPERF_STACK_SIZE_IPERF_TASK		340
#endif

#ifndef ipconfigIPERF_PRIORITY_IPERF_TASK
	/* The priority of vIPerfTask(). Should be lower than the IP-task
	and the task running in NetworkInterface.c. */
	#define	ipconfigIPERF_PRIORITY_IPERF_TASK		3
#endif

#ifndef ipconfigIPERF_RECV_BUFFER_SIZE
	/* Only used when ipconfigIPERF_USE_ZERO_COPY = 0.
	Buffer size when reading from the sockets. */
	#define ipconfigIPERF_RECV_BUFFER_SIZE			( 4 * 1460 )
#endif

#ifndef ipconfigIPERF_LOOP_BLOCKING_TIME_MS
	/* Let the mainloop wake-up so now and then. */
	#define ipconfigIPERF_LOOP_BLOCKING_TIME_MS		5000UL
#endif

#ifndef ipconfigIPERF_HAS_TCP
	/* A TCP server socket will be created. */
	#define ipconfigIPERF_HAS_TCP					1
#endif

#ifndef ipconfigIPERF_DUAL_TCP
	/* Use a dual connection. */
	#define ipconfigIPERF_DUAL_TCP					1
#endif

#ifndef ipconfigIPERF_HAS_UDP
	/* A UDP server socket will be created. */
	#define ipconfigIPERF_HAS_UDP					1
#endif

#ifndef ipconfigIPERF_DOES_ECHO_TCP
	/* By default, this server will not echo TCP data. */
	#define ipconfigIPERF_DOES_ECHO_TCP				0
#endif

#ifndef ipconfigIPERF_DOES_ECHO_UDP
	/* By default, this server will echo UDP data as required by iperf. */
	#define ipconfigIPERF_DOES_ECHO_UDP				1
#endif

#ifndef ipconfigIPERF_USE_ZERO_COPY
	#define ipconfigIPERF_USE_ZERO_COPY				1
#endif

#ifndef ipconfigIPERF_TX_BUFSIZE
	#define ipconfigIPERF_TX_BUFSIZE				( 65 * 1024 )	/* Units of bytes. */
	#define ipconfigIPERF_TX_WINSIZE				( 4 )			/* Size in units of MSS */
	#define ipconfigIPERF_RX_BUFSIZE				( ( 65 * 1024 ) - 1 )	/* Units of bytes. */
	#define ipconfigIPERF_RX_WINSIZE				( 8 )			/* Size in units of MSS */
#endif

static void vIPerfTask( void *pvParameter );

/* As for now, still defined in 'FreeRTOS-Plus-TCP\FreeRTOS_TCP_WIN.c' : */
extern void vListInsertGeneric( List_t * const pxList, ListItem_t * const pxNewListItem, MiniListItem_t * const pxWhere );
static portINLINE void vListInsertFifo( List_t * const pxList, ListItem_t * const pxNewListItem )
{
	vListInsertGeneric( pxList, pxNewListItem, &pxList->xListEnd );
}

#if( ipconfigIPERF_DUAL_TCP != 0 )
typedef enum
{
	eConnectConnect,
	eConnectConnected,
	eConnectError,
} eConnectState_t;
#endif /* ipconfigIPERF_DUAL_TCP */

#if( ipconfigIPERF_VERSION == 3 )
	typedef enum
	{
		eTCP_0_WaitName,	// Expect a text like "osboxes.1460335312.612572.527642c36f"
		eTCP_1_WaitCount,
		eTCP_2_WaitHeader,
		eTCP_3_WaitOneTwo,
		eTCP_4_WaitCount2,
		eTCP_5_WaitHeader2,
		eTCP_6_WaitDone,
		eTCP_7_WaitTransfer
	} eTCP_Server_Status_t;
#endif
typedef struct
{
	Socket_t xServerSocket;
	#if( ipconfigIPERF_DUAL_TCP != 0 )
		Socket_t xClientSocket;
		eConnectState_t eConnectState;
		uint32_t ulSendCount;
	#endif /* ipconfigIPERF_DUAL_TCP */
	#if( ipconfigIPERF_VERSION == 3 )
		struct {
			uint32_t
				bIsControl : 1,
				bReverse : 1;
		} bits;
		eTCP_Server_Status_t eTCP_Status;
		uint32_t ulSkipCount, ulAmount;
	#endif /* ipconfigIPERF_VERSION == 3 */
	struct freertos_sockaddr xRemoteAddr;
	uint32_t ulRecvCount;
	struct xLIST_ITEM xListItem;	/* With this item the client will be bound to a List_t. */
} TcpClient_t;

#if( ipconfigIPERF_USE_ZERO_COPY != 0 )
	static char *pcRecvBuffer;
	static char pcSendBuffer[ ipconfigIPERF_RECV_BUFFER_SIZE ];
#else
	static char pcRecvBuffer[ ipconfigIPERF_RECV_BUFFER_SIZE ];
	#define	pcSendBuffer	pcRecvBuffer
#endif

#if( ipconfigIPERF_VERSION == 3 )
	char pcExpectedClient[ 80 ];
	TcpClient_t *pxControlClient, *pxDataClient;
	//{"cpu_util_total":0,"cpu_util_user":0,"cpu_util_system":0,"sender_has_retransmits":-1,"streams":[{"id":1,"bytes":8760,"retransmits":-1,"jitter":0,"errors":0,"packets":0}]}
#endif

static List_t xTCPClientList;
static uint32_t ulUDPRecvCount = 0, ulUDPRecvCountShown = 0, ulUDPRecvCountSeen = 0;
static SocketSet_t xSocketSet;
static TaskHandle_t pxIperfTask;

void vIPerfInstall( void )
{
	if( pxIperfTask == NULL )
	{
		/* Call this function once to start the test with FreeRTOS_accept(). */
		xTaskCreate( vIPerfTask, "vIPerfTask", ipconfigIPERF_STACK_SIZE_IPERF_TASK, NULL, ipconfigIPERF_PRIORITY_IPERF_TASK, &pxIperfTask );
	}
}

static void vIPerfServerWork( Socket_t xSocket )
{
struct freertos_sockaddr xAddress;
socklen_t xSocketLength;
Socket_t xNexSocket;

	xNexSocket = FreeRTOS_accept( xSocket, &xAddress, &xSocketLength);

	if( ( xNexSocket != NULL ) && ( xNexSocket != FREERTOS_INVALID_SOCKET ) )
	{
	char pucBuffer[ 16 ];
	TcpClient_t *pxClient;
	
		pxClient = ( TcpClient_t * )pvPortMalloc( sizeof *pxClient );
		memset( pxClient, '\0', sizeof *pxClient );

		pxClient->xServerSocket = xNexSocket;

		listSET_LIST_ITEM_OWNER( &( pxClient->xListItem ), ( void* ) pxClient );
		FreeRTOS_GetRemoteAddress( xNexSocket, &pxClient->xRemoteAddr );
		FreeRTOS_inet_ntoa( pxClient->xRemoteAddr.sin_addr, pucBuffer );

		FreeRTOS_printf( ( "vIPerfTask: Received a connection from %s:%u\n",
			pucBuffer,
			FreeRTOS_ntohs( pxClient->xRemoteAddr.sin_port ) ) );

		FreeRTOS_FD_SET( xNexSocket, xSocketSet, eSELECT_READ );

		#if( ipconfigIPERF_DUAL_TCP != 0 )
		{
			pxClient->xClientSocket = FreeRTOS_socket( FREERTOS_AF_INET, FREERTOS_SOCK_STREAM, FREERTOS_IPPROTO_TCP );
			if( ( pxClient->xClientSocket != FREERTOS_INVALID_SOCKET ) && ( pxClient->xClientSocket != NULL ) )
			{
			struct freertos_sockaddr xAddress;
			TickType_t xConnectTime = pdMS_TO_TICKS( 0ul );

				memset( &xAddress, '\0', sizeof xAddress );

				FreeRTOS_setsockopt( pxClient->xClientSocket, 0, FREERTOS_SO_RCVTIMEO, &xConnectTime, sizeof( xConnectTime ) );
				FreeRTOS_setsockopt( pxClient->xClientSocket, 0, FREERTOS_SO_SNDTIMEO, &xConnectTime, sizeof( xConnectTime ) );

				/* Bind to an anonymous port number (0): */
				FreeRTOS_bind( pxClient->xClientSocket, &xAddress, sizeof xAddress );

				FreeRTOS_FD_SET( pxClient->xClientSocket, xSocketSet, eSELECT_READ );

				/* Connect to the remote iperf server. */
				xAddress.sin_addr = pxClient->xRemoteAddr.sin_addr;
				xAddress.sin_port = FreeRTOS_htons( ipconfigIPERF_TCP_ECHO_PORT  );

				pxClient->eConnectState = eConnectConnect;

				FreeRTOS_connect( pxClient->xClientSocket, &xAddress, sizeof( xAddress ) );
				
			}
			else
			{
				/* The value of FREERTOS_INVALID_SOCKET is not interesting. */
				pxClient->xClientSocket = NULL;
			}
		}
		#endif

		vListInsertFifo( &xTCPClientList, &( pxClient->xListItem ) );
	}
}

static void vIPerfTCPClose( TcpClient_t *pxClient )
{
	if( pxControlClient == pxClient )
	{
		pxControlClient = NULL;
	}
	if( pxDataClient == pxClient )
	{
		pxDataClient = NULL;
	}
	/* Remove server socket from the socket set. */
	if( pxClient->xServerSocket != NULL )
	{
	char pucBuffer[ 16 ];

		FreeRTOS_inet_ntoa( pxClient->xRemoteAddr.sin_addr, pucBuffer );
		FreeRTOS_printf( ( "vIPerfTCPClose: Closing server socket %s:%u after %lu bytes\n",
			pucBuffer,
			FreeRTOS_ntohs( pxClient->xRemoteAddr.sin_port ),
			pxClient->ulRecvCount ) );					
		FreeRTOS_FD_CLR( pxClient->xServerSocket, xSocketSet, eSELECT_ALL );
		FreeRTOS_closesocket( pxClient->xServerSocket );
		pxClient->xServerSocket = NULL;
		#if( ipconfigIPERF_DUAL_TCP != 0 )
		{
			if( pxClient->xClientSocket != NULL )
			{
			BaseType_t xRemain = FreeRTOS_outstanding( pxClient->xClientSocket );
				FreeRTOS_printf( ( "vIPerfTCPClose: Shutdown on client socket. Outstanding %d\n", xRemain ) );
				FreeRTOS_shutdown( pxClient->xClientSocket, FREERTOS_SHUT_RDWR );
				if( xRemain > 0 )
				{
					FreeRTOS_FD_SET( pxClient->xClientSocket, xSocketSet, eSELECT_WRITE );
				}
			}
		}
		#endif
	}

	/* Remove client socket from the socket set. */
	#if( ipconfigIPERF_DUAL_TCP != 0 )
	{
		if( pxClient->xClientSocket != NULL )
		{
			if( FreeRTOS_outstanding( pxClient->xClientSocket ) <= 0 )
			{
			char pucBuffer[ 16 ];

				FreeRTOS_inet_ntoa( pxClient->xRemoteAddr.sin_addr, pucBuffer );
				FreeRTOS_printf( ( "vIPerfTCPClose: Closing client socket %s:%u after %lu bytes\n",
					pucBuffer,
					FreeRTOS_ntohs( pxClient->xRemoteAddr.sin_port ),
					pxClient->ulSendCount ) );
				FreeRTOS_FD_CLR( pxClient->xClientSocket, xSocketSet, eSELECT_ALL );
				FreeRTOS_closesocket( pxClient->xClientSocket );
				pxClient->xClientSocket = NULL;
			}
		}
	}
	#endif /* ipconfigIPERF_DUAL_TCP */

	#if( ipconfigIPERF_DUAL_TCP != 0 )
	if( pxClient->xClientSocket == NULL )
	#endif /* ipconfigIPERF_DUAL_TCP */
	{
		if( pxClient->xServerSocket == NULL )
		{
			/* Remove this socket from the list. */
			uxListRemove( &( pxClient->xListItem ) );
			vPortFree( ( void * ) pxClient );
		}
	}
}

static void vIPerfTCPWork( TcpClient_t *pxClient )
{
BaseType_t xRecvResult;

	#if( ipconfigIPERF_DUAL_TCP != 0 )
	{
		if( pxClient->xClientSocket != NULL )
		{
			switch( pxClient->eConnectState )
			{
			case eConnectConnect:
				{
				BaseType_t xResult;
				
					xResult = FreeRTOS_connstatus( pxClient->xClientSocket );
					if( xResult == eESTABLISHED )
					{
						FreeRTOS_printf( ( "vIPerfTCPWork: client connected too\n" ) );
						pxClient->eConnectState = eConnectConnected;
					}
					else if ( ( xResult == eCLOSED ) || ( xResult > eESTABLISHED ) )
					{
						FreeRTOS_printf( ( "vIPerfTCPWork: client failed to connect (state %u)\n", xResult ) );
						FreeRTOS_FD_CLR( pxClient->xClientSocket, xSocketSet, eSELECT_ALL );
						FreeRTOS_closesocket( pxClient->xClientSocket );
						pxClient->xClientSocket = NULL;
						pxClient->eConnectState = eConnectError;
					}
				}
				break;
			case eConnectConnected:
				break;
			case eConnectError:
				break;
			}
		}
	}
	#endif /* ipconfigIPERF_DUAL_TCP */

	#if( ipconfigIPERF_DUAL_TCP != 0 )
	if( pxClient->eConnectState == eConnectConnected )
	#endif
	{
		if( pxClient->xServerSocket == NULL )
		{
			vIPerfTCPClose( pxClient );
		}
		else
		{
		#if( ipconfigIPERF_DUAL_TCP != 0 )
		BaseType_t xTXSpace = FreeRTOS_tx_space( pxClient->xClientSocket );
		#endif

			#if( ipconfigIPERF_VERSION == 3 )
			/* Is this a data client with the -R reverse option ? */
			if( pxClient->bits.bIsControl == pdFALSE )
			{
				if( ( pxClient == pxDataClient ) && ( pxClient->bits.bReverse != pdFALSE ) && ( pxClient->ulAmount != 0 ) )
				{
					BaseType_t xMaxSpace = FreeRTOS_tx_space( pxClient->xServerSocket );
					BaseType_t xSize = FreeRTOS_min_int32( xMaxSpace, ( int32_t )pcSendBuffer );
					xSize = FreeRTOS_min_int32( xSize, pxClient->ulAmount );
					if( xSize > 0 )
					{
						FreeRTOS_send( pxClient->xServerSocket, (const void *)pcSendBuffer, xSize, 0 );
					}
					pxClient->ulAmount -= xSize;
					if( pxClient->ulAmount == 0 )
					{
						FreeRTOS_shutdown( pxClient->xServerSocket, FREERTOS_SHUT_RDWR );
					}
				}
			}
			#endif

			#if( ipconfigIPERF_USE_ZERO_COPY != 0 )
			{
				#if( ipconfigIPERF_DUAL_TCP != 0 )
				const BaseType_t xRecvSize = FreeRTOS_min_int32( xTXSpace, 32768 );
				#else
				const BaseType_t xRecvSize = 32768;
				#endif
				xRecvResult = FreeRTOS_recv( pxClient->xServerSocket, /* The socket being received from. */
						&pcRecvBuffer,		/* The buffer into which the received data will be written. */
						xRecvSize,			/* Any size if OK here. */
						FREERTOS_ZERO_COPY );
			}
			#else
			{
				#if( ipconfigIPERF_DUAL_TCP != 0 )
				const BaseType_t xRecvSize = FreeRTOS_min_int32( xTXSpace, sizeof pcRecvBuffer );
				#else
				const BaseType_t xRecvSize = sizeof pcRecvBuffer;
				#endif
				xRecvResult = FreeRTOS_recv( pxClient->xServerSocket, /* The socket being received from. */
						pcRecvBuffer,			/* The buffer into which the received data will be written. */
						sizeof pcRecvBuffer,	/* The size of the buffer provided to receive the data. */
						0 );
			}
			#endif

			if( xRecvResult > 0 )
			{
				#if( ipconfigIPERF_VERSION == 3 )
				char *pcReadBuffer = pcRecvBuffer;
				BaseType_t xRemaining = xRecvResult;
				#endif
				pxClient->ulRecvCount += xRecvResult;
				#if( ipconfigIPERF_DOES_ECHO_TCP != 0 )
				{
					FreeRTOS_send( pxClient->xServerSocket, (const void *)pcRecvBuffer, xRecvResult, 0 );
				}
				#endif /* ipconfigIPERF_DOES_ECHO_TCP */

				#if( ipconfigIPERF_VERSION == 3 )
				if( pxClient->eTCP_Status < eTCP_7_WaitTransfer )
				{
					FreeRTOS_printf( ( "TCP[ port %d ] recv[ %d ] %d\n", FreeRTOS_ntohs( pxClient->xRemoteAddr.sin_port ), ( int ) pxClient->eTCP_Status, xRecvResult ) );
				}
/*
2016-04-11 09:26:55.533 192.168.2.106     16.926.349 [] TCP recv[ 1 ] 4
2016-04-11 09:26:55.539 192.168.2.106     16.926.365 [] TCP skipcount 83 xRecvResult 4
2016-04-11 09:26:55.546 192.168.2.106     16.926.402 [] TCP recv[ 2 ] 4
2016-04-11 09:26:55.552 192.168.2.106     16.927.015 [] ipTCP_FLAG_PSH received
2016-04-11 09:26:55.560 192.168.2.106     16.927.085 [] TCP recv[ 2 ] 83
*/
				switch( pxClient->eTCP_Status  )
				{
				case eTCP_0_WaitName:
					{
					int rc;
						if( pcExpectedClient[ 0 ] != '\0' )
						{
							/* Get a string like 'LAPTOP.1463928623.162445.0fbe105c6eb',
							where 'LAPTOP' is the hostname. */
							rc = strncmp( pcExpectedClient, pcReadBuffer, sizeof( pcExpectedClient ) );
						}
						else
						{
							rc = -1;
						}

						{
						unsigned char tab[ 1 ] = { '\t' };
							FreeRTOS_send( pxClient->xServerSocket, (const void *)tab, 1, 0 );
						}
						if( rc != 0 )
						{
							strncpy( pcExpectedClient, pcReadBuffer, sizeof( pcExpectedClient ) );
							pxControlClient = pxClient;
							pxClient->bits.bIsControl = pdTRUE;
							pxClient->eTCP_Status = ( eTCP_Server_Status_t ) ( ( ( int ) pxClient->eTCP_Status ) + 1 );
							FreeRTOS_printf( ( "Got Control Socket: rc %d: '%s'\n", rc, pcExpectedClient ) );
						}
						else
						{
							FreeRTOS_printf( ( "Got expected client: rc %d: '%s'\n", rc, pcExpectedClient ) );
							pcExpectedClient[ 0 ] = '\0';
							pxDataClient = pxClient;
							if( pxControlClient != NULL )
							{
								pxDataClient->bits.bReverse = pxControlClient->bits.bReverse;
								pxDataClient->ulAmount = pxControlClient->ulAmount;
								if( pxControlClient->bits.bReverse != pdFALSE )
								{
									/* As this socket will only write data, set the WRITE select flag. */
									FreeRTOS_FD_SET( pxClient->xServerSocket, xSocketSet, eSELECT_WRITE );
								}
								pxControlClient->bits.bReverse = 0;
								pxControlClient->ulAmount = 0;
							}
							pxDataClient->eTCP_Status = eTCP_7_WaitTransfer;
						}
					}
					break;
				case eTCP_1_WaitCount:
					if( xRemaining < 4 )
					{
						break;
					}
					{
						pxClient->ulSkipCount =
							( ( ( uint32_t ) pcReadBuffer[ 0 ] ) << 24 ) |
							( ( ( uint32_t ) pcReadBuffer[ 1 ] ) << 16 ) |
							( ( ( uint32_t ) pcReadBuffer[ 2 ] ) <<  8 ) |
							( ( ( uint32_t ) pcReadBuffer[ 3 ] ) <<  0 );
						/* Receive a number: an amount of bytes to be skipped. */
						FreeRTOS_printf( ( "TCP skipcount %d xRecvResult %d\n", pxClient->ulSkipCount, xRemaining ) );
						pcReadBuffer += 4;
						xRemaining -= 4;
						pxClient->eTCP_Status = ( eTCP_Server_Status_t ) ( ( ( int ) pxClient->eTCP_Status ) + 1 );
					}
					if( xRemaining == 0 )
					{
						break;
					}
					// fall through
				case eTCP_2_WaitHeader:
					{
						if (xRemaining > 0) {
							char *pcPtr;
							pcReadBuffer[ xRemaining ] = '\0';
							for( pcPtr = pcReadBuffer; *pcPtr; pcPtr++ )
							{
								if( strncmp( pcPtr, "\"reverse\"", 9 ) == 0 )
								{
									pxClient->bits.bReverse = pcPtr[ 10 ] == 't';
								}
								else if( strncmp( pcPtr, "\"num\"", 5 ) == 0 )
								{
								uint32_t ulAmount;
									if( sscanf( pcPtr + 6, "%lu", &( ulAmount ) ) > 0 )
									{
										pxClient->ulAmount = ulAmount;
									}
								}
							}
							if( pxClient->bits.bReverse != pdFALSE )
							{
								FreeRTOS_printf( ( "Reverse %d send %lu bytes\n", pxClient->bits.bReverse, pxClient->ulAmount ) );
							}
						}
						if( pxClient->ulSkipCount > xRemaining )
						{
							pxClient->ulSkipCount -= xRemaining;
						}
						else
						{
						unsigned char newline[ 1 ] = { '\n' };
						unsigned char onetwo[ 2 ] = { '\1', '\2' };
							FreeRTOS_send( pxClient->xServerSocket, (const void *)newline, sizeof( newline ), 0 );
							FreeRTOS_send( pxClient->xServerSocket, (const void *)onetwo, sizeof( onetwo ), 0 );

							pxClient->ulSkipCount = 0;
							pxClient->eTCP_Status = ( eTCP_Server_Status_t ) ( ( ( int ) pxClient->eTCP_Status ) + 1 );
						}
					}
					break;
				case eTCP_3_WaitOneTwo:
					{
					unsigned char ch = pcReadBuffer[ 0 ];
					unsigned char cr[ 1 ] = { '\r' };

						FreeRTOS_printf( ( "TCP[ port %d ] recv %d bytes: 0x%02X\n",
							FreeRTOS_ntohs( pxClient->xRemoteAddr.sin_port ), xRecvResult, ch ) );

						FreeRTOS_send( pxClient->xServerSocket, (const void *)cr, sizeof( cr ), 0 );
						pxClient->eTCP_Status = ( eTCP_Server_Status_t ) ( ( ( int ) pxClient->eTCP_Status ) + 1 );
					}
					break;
				case eTCP_4_WaitCount2:
					if( xRemaining < 4 )
					{
						break;
					}
					{
						pxClient->ulSkipCount =
							( ( ( uint32_t ) pcReadBuffer[ 0 ] ) << 24 ) |
							( ( ( uint32_t ) pcReadBuffer[ 1 ] ) << 16 ) |
							( ( ( uint32_t ) pcReadBuffer[ 2 ] ) <<  8 ) |
							( ( ( uint32_t ) pcReadBuffer[ 3 ] ) <<  0 );
						FreeRTOS_printf( ( "TCP skipcount %d xRecvResult %d\n", pxClient->ulSkipCount, xRemaining ) );

						pcReadBuffer += 4;
						xRemaining -= 4;
						pxClient->eTCP_Status = ( eTCP_Server_Status_t ) ( ( ( int ) pxClient->eTCP_Status ) + 1 );
					}
					if( xRemaining == 0 )
					{
						break;
					}
					// fall through
				case eTCP_5_WaitHeader2:
					{
						if( pxClient->ulSkipCount > xRemaining )
						{
							pxClient->ulSkipCount -= xRemaining;
						}
						else
						{
						char pcResponse[ 200 ];
						uint32_t ulLength, ulStore, ulCount = 0;
						if( pxDataClient != NULL )
						{
							ulCount = pxDataClient->ulRecvCount;
						}
						ulLength = snprintf( pcResponse + 4, sizeof( pcResponse ) - 4,
							"{"
								"\"cpu_util_total\":0,"
								"\"cpu_util_user\":0,"
								"\"cpu_util_system\":0,"
								"\"sender_has_retransmits\":-1,"
								"\"streams\":["
									"{"
										"\"id\":1,"
										"\"bytes\":%lu,"
										"\"retransmits\":-1,"
										"\"jitter\":0,"
										"\"errors\":0,"
										"\"packets\":0"
									"}"
								"]"
							"}\xe",
							ulCount );
						ulStore = FreeRTOS_htonl( ulLength - 1 );
						memcpy( pcResponse, &( ulStore ), sizeof ( ulStore ) );
							FreeRTOS_send( pxClient->xServerSocket, (const void *)pcResponse, ulLength + 4, 0 );

							pxClient->ulSkipCount = 0;
							pxClient->eTCP_Status = ( eTCP_Server_Status_t ) ( ( ( int ) pxClient->eTCP_Status ) + 1 );
						}
					}
					break;
				case eTCP_6_WaitDone:
					break;
				case eTCP_7_WaitTransfer:
					break;
				}
				#endif /* ipconfigIPERF_VERSION == 3 */

				#if( ipconfigIPERF_DUAL_TCP != 0 )
				{
				BaseType_t xSendCount;

					xSendCount = FreeRTOS_send( pxClient->xClientSocket, (const void *)pcRecvBuffer, xRecvResult, 0 );
					if( xSendCount != xRecvResult )
					{
						FreeRTOS_printf( ( "Send: %d != %d\n", xSendCount, xRecvResult ) );
					}
					pxClient->ulSendCount += xRecvResult;
					xTXSpace -= xRecvResult;
					if( xTXSpace < 1460 )
					{
						FreeRTOS_FD_SET( pxClient->xClientSocket, xSocketSet, eSELECT_WRITE );
					}
					else
					{
						FreeRTOS_FD_CLR( pxClient->xClientSocket, xSocketSet, eSELECT_WRITE );
					}
				}
				#endif
				#if( ipconfigIPERF_USE_ZERO_COPY != 0 )
				{
					FreeRTOS_recv( pxClient->xServerSocket, /* The socket being received from. */
						NULL,			/* The buffer into which the received data will be written. */
						xRecvResult,	/* This is important now. */
						0 );
				}
				#endif
			}
			else if( ( xRecvResult < 0 ) && ( xRecvResult != -pdFREERTOS_ERRNO_EAGAIN ) )
			{
				vIPerfTCPClose( pxClient );
			}
		}
	}
}

#if( ipconfigUSE_CALLBACKS == 0 )
	static void vIPerfUDPWork( Socket_t xSocket )
	{
	BaseType_t xRecvResult;
	struct freertos_sockaddr xAddress;
	uint32_t xAddressLength;

		#if( ipconfigIPERF_USE_ZERO_COPY != 0 )
		{
			xRecvResult = FreeRTOS_recvfrom( xSocket, ( void * ) &pcRecvBuffer, sizeof( pcRecvBuffer ),
				FREERTOS_ZERO_COPY, &xAddress, &xAddressLength );
		}
		#else
		{
			xRecvResult = FreeRTOS_recvfrom( xSocket, ( void * ) pcRecvBuffer, sizeof( pcRecvBuffer ),
				0, &xAddress, &xAddressLength );
		}
		#endif /* ipconfigIPERF_USE_ZERO_COPY */
		if( xRecvResult > 0 )
		{
			ulUDPRecvCount += xRecvResult;
			#if( ipconfigIPERF_DOES_ECHO_UDP != 0 )
			{
				FreeRTOS_sendto( xSocket, (const void *)pcRecvBuffer, xRecvResult, 0, &xAddress, sizeof( xAddress ) );
			}
			#endif /* ipconfigIPERF_DOES_ECHO_UDP */
			#if( ipconfigIPERF_USE_ZERO_COPY != 0 )
			{
				FreeRTOS_ReleaseUDPPayloadBuffer( pcRecvBuffer );
			}
			#endif /* ipconfigIPERF_USE_ZERO_COPY */
		}
	}
#endif /* ipconfigUSE_CALLBACKS == 0 */

#if( ipconfigUSE_CALLBACKS != 0 )
	static BaseType_t xOnUdpReceive( Socket_t xSocket, void * pvData, size_t xLength,
		const struct freertos_sockaddr *pxFrom, const struct freertos_sockaddr *pxDest )
	{
		ulUDPRecvCount += xLength;
		#if( ipconfigIPERF_DOES_ECHO_UDP != 0 )
		{
			FreeRTOS_sendto( xSocket, (const void *)pcRecvBuffer, xLength, 0, pxFrom, sizeof( *pxFrom ) );
		}
		#endif /* ipconfigIPERF_DOES_ECHO_UDP */
		/* Tell the driver not to store the RX data */
		return 1;
	}
#endif /* ipconfigUSE_CALLBACKS != 0 */

static void vIPerfTask( void *pvParameter )
{
TickType_t xNoTimeOut = 0;
BaseType_t xBindResult, xListenResult;
static struct freertos_sockaddr xEchoServerAddress;
#if( ipconfigIPERF_HAS_TCP != 0 )
	Socket_t xTCPServerSocket;
#endif /* ipconfigIPERF_HAS_TCP */

#if( ipconfigIPERF_HAS_UDP != 0 )
	Socket_t xUDPServerSocket;
#endif /* ipconfigIPERF_HAS_UDP */

	xSocketSet = FreeRTOS_CreateSocketSet( );

	vListInitialise( &xTCPClientList );

	#if( ipconfigIPERF_HAS_TCP != 0 )
	{
		xTCPServerSocket = FreeRTOS_socket( FREERTOS_AF_INET, FREERTOS_SOCK_STREAM, FREERTOS_IPPROTO_TCP );
		configASSERT( ( xTCPServerSocket != FREERTOS_INVALID_SOCKET ) && ( xTCPServerSocket != NULL ) );

		memset( &xEchoServerAddress, '\0', sizeof xEchoServerAddress );
		xEchoServerAddress.sin_port = FreeRTOS_htons( ipconfigIPERF_TCP_ECHO_PORT  );

		/* Set the receive time out to portMAX_DELAY.
		Note that any TCP child connections will inherit this reception time-out. */
		FreeRTOS_setsockopt( xTCPServerSocket, 0, FREERTOS_SO_RCVTIMEO, &xNoTimeOut, sizeof( xNoTimeOut ) );

		{
		WinProperties_t xWinProperties;

			memset(&xWinProperties, '\0', sizeof xWinProperties);

			xWinProperties.lTxBufSize   = ipconfigIPERF_TX_BUFSIZE;	/* Units of bytes. */
			xWinProperties.lTxWinSize   = ipconfigIPERF_TX_WINSIZE;	/* Size in units of MSS */
			xWinProperties.lRxBufSize   = ipconfigIPERF_RX_BUFSIZE;	/* Units of bytes. */
			xWinProperties.lRxWinSize   = ipconfigIPERF_RX_WINSIZE;			/* Size in units of MSS */

			FreeRTOS_setsockopt( xTCPServerSocket, 0, FREERTOS_SO_WIN_PROPERTIES, ( void * ) &xWinProperties, sizeof( xWinProperties ) );
		}

		xBindResult = FreeRTOS_bind( xTCPServerSocket, &xEchoServerAddress, sizeof xEchoServerAddress );
		xListenResult = FreeRTOS_listen( xTCPServerSocket, 4 );

		FreeRTOS_FD_SET( xTCPServerSocket, xSocketSet, eSELECT_READ );

		FreeRTOS_printf( ( "vIPerfTask: created TCP server socket %p bind port %u: %ld listen %ld\n",
			xTCPServerSocket, ipconfigIPERF_TCP_ECHO_PORT, xBindResult, xListenResult ) );
	}
	#endif /* ipconfigIPERF_HAS_TCP */

	#if( ipconfigIPERF_HAS_UDP != 0 )
	{
		xUDPServerSocket = FreeRTOS_socket( FREERTOS_AF_INET, FREERTOS_SOCK_DGRAM, FREERTOS_IPPROTO_UDP );
		configASSERT( ( xUDPServerSocket != FREERTOS_INVALID_SOCKET ) && ( xUDPServerSocket != NULL ) );

		xEchoServerAddress.sin_port = FreeRTOS_htons( ipconfigIPERF_UDP_ECHO_PORT  );

		FreeRTOS_setsockopt( xUDPServerSocket, 0, FREERTOS_SO_RCVTIMEO, &xNoTimeOut, sizeof( xNoTimeOut ) );
		xBindResult = FreeRTOS_bind( xUDPServerSocket, &xEchoServerAddress, sizeof xEchoServerAddress );

		FreeRTOS_printf( ( "vIPerfTask: created UDP server socket %p bind port %u: %ld\n",
			xUDPServerSocket, ipconfigIPERF_UDP_ECHO_PORT, xBindResult ) );
		#if( ipconfigUSE_CALLBACKS == 0 )
		{
			FreeRTOS_FD_SET( xUDPServerSocket, xSocketSet, eSELECT_READ );
		}
		#else
		{
		F_TCP_UDP_Handler_t xHandler;

			memset( &xHandler, '\0', sizeof ( xHandler ) );
			xHandler.pOnUdpReceive = xOnUdpReceive;
			FreeRTOS_setsockopt( xUDPServerSocket, 0, FREERTOS_SO_UDP_RECV_HANDLER, ( void * ) &xHandler, sizeof( xHandler ) );
		}
		#endif /* ipconfigUSE_CALLBACKS */
	}
	#endif /* ipconfigIPERF_HAS_UDP */

#define HUNDREDTH_MB	( ( 1024 * 1024 ) / 100 )
	for( ;; )
	{
	BaseType_t xResult;
	const TickType_t xBlockingTime = pdMS_TO_TICKS( ipconfigIPERF_LOOP_BLOCKING_TIME_MS );

		xResult = FreeRTOS_select( xSocketSet, xBlockingTime );
#if( ipconfigIPERF_HAS_TCP != 0 )
		if( xResult != 0 )
		{
		const MiniListItem_t* pxEnd = ( const MiniListItem_t* ) listGET_END_MARKER( &xTCPClientList );
		const ListItem_t *pxIterator;

			vIPerfServerWork( xTCPServerSocket );

			/* Check all TCP clients: */
			for( pxIterator  = ( const ListItem_t * ) listGET_NEXT( pxEnd );
				 pxIterator != ( const ListItem_t * ) pxEnd;
				 )
			{
			TcpClient_t *pxClient;

				pxClient = ( TcpClient_t * ) listGET_LIST_ITEM_OWNER( pxIterator );

				/* Let the iterator point to the next element before the current element
				removes itself from the list. */
				pxIterator  = ( const ListItem_t * ) listGET_NEXT( pxIterator );

				vIPerfTCPWork( pxClient );
			}
		}
#endif /* ipconfigIPERF_HAS_TCP */

#if( ipconfigIPERF_HAS_UDP != 0 )
		#if( ipconfigUSE_CALLBACKS == 0 )
		if( xResult != 0 )
		{
			vIPerfUDPWork( xUDPServerSocket );
		} else
		#endif /* ipconfigUSE_CALLBACKS */
		{
			if( ulUDPRecvCountSeen != ulUDPRecvCount )
			{
				/* The amount is still changing, do not show it yet. */
				ulUDPRecvCountSeen = ulUDPRecvCount;
			}
			else if( ulUDPRecvCountShown != ulUDPRecvCount )
			{
			uint32_t ulNewBytes = ulUDPRecvCount - ulUDPRecvCountShown;
			uint32_t ulMB = (ulNewBytes + HUNDREDTH_MB/2) / HUNDREDTH_MB;

				FreeRTOS_printf( ( "UDP received %lu + %lu (%u.%02u MB) = %lu\n",
					ulUDPRecvCountShown,
					ulNewBytes,
					ulMB / 100,
					ulMB % 100,
					ulUDPRecvCount ) );
				ulUDPRecvCountShown = ulUDPRecvCount;
			}
		}
#endif /* ipconfigIPERF_HAS_UDP */
	}
}
