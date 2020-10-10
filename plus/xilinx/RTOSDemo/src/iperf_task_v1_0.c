/*
	A FreeRTOS+TCP demo program:
	A simple example of a TCP/UDP iperf server, see https://iperf.fr

	It uses a single task which will store some client information in a:
	    List_t xTCPClientList;

	The task will sleep constantly by calling:
	    xResult = FreeRTOS_select( xSocketSet, xBlockingTime );

	Command examples for testing:

	TCP :  iperf -c 192.168.2.116  --port 5001 --num 1024K
	UDP :  iperf -c 192.168.2.116  --port 5001 --udp --bandwidth 50M --mss 1460 --num 1024K
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
	#define ipconfigIPERF_TX_BUFSIZE				( 65 * 1024 );	/* Units of bytes. */
	#define ipconfigIPERF_TX_WINSIZE				( 4 );			/* Size in units of MSS */
	#define ipconfigIPERF_RX_BUFSIZE				( ( 65 * 1024 ) - 1 );	/* Units of bytes. */
	#define ipconfigIPERF_RX_WINSIZE				( 8 );			/* Size in units of MSS */
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

typedef struct
{
	Socket_t xServerSocket;
	#if( ipconfigIPERF_DUAL_TCP != 0 )
		Socket_t xClientSocket;
		eConnectState_t eConnectState;
		uint32_t ulSendCount;
	#endif /* ipconfigIPERF_DUAL_TCP */
	struct freertos_sockaddr xRemoteAddr;
	uint32_t ulRecvCount;
	struct xLIST_ITEM xListItem;	/* With this item the client will be bound to a List_t. */
} TcpClient_t;

#if( ipconfigIPERF_USE_ZERO_COPY != 0 )
	static char *pcRecvBuffer;
#else
	static char pcRecvBuffer[ ipconfigIPERF_RECV_BUFFER_SIZE ];
#endif

static List_t xTCPClientList;
static uint32_t ulUDPRecvCount = 0, ulUDPRecvCountShown = 0;
static SocketSet_t xSocketSet;

void vIPerfInstall( void )
{
	/* Call this function once to start the test with FreeRTOS_accept(). */
	xTaskCreate( vIPerfTask, "vIPerfTask", ipconfigIPERF_STACK_SIZE_IPERF_TASK, NULL, ipconfigIPERF_PRIORITY_IPERF_TASK, NULL);
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
	
		pxClient = pvPortMalloc( sizeof *pxClient );
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
				pxClient->ulRecvCount += xRecvResult;
				#if( ipconfigIPERF_DOES_ECHO_TCP != 0 )
				{
					FreeRTOS_send( pxClient->xServerSocket, (const void *)pcRecvBuffer, xRecvResult, 0 );
				}
				#endif /* ipconfigIPERF_DOES_ECHO_TCP */
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
			else if( xRecvResult < 0 )
			{
				vIPerfTCPClose( pxClient );
			}
		}
	}
}

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

static void vIPerfTask( void *pvParameter )
{
TickType_t xNoTimeOut = 0;
BaseType_t xBindResult, xListenResult;
struct freertos_sockaddr xEchoServerAddress;
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
		FreeRTOS_FD_SET( xUDPServerSocket, xSocketSet, eSELECT_READ );

		FreeRTOS_printf( ( "vIPerfTask: created UDP server socket %p bind port %u: %ld\n",
			xUDPServerSocket, ipconfigIPERF_UDP_ECHO_PORT, xBindResult ) );
	}
	#endif /* ipconfigIPERF_HAS_UDP */


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
		if( xResult != 0 )
		{
			vIPerfUDPWork( xUDPServerSocket );
		}
		else
		{
			if( ulUDPRecvCountShown != ulUDPRecvCount )
			{
			uint32_t ulNewBytes = ulUDPRecvCount - ulUDPRecvCountShown;

				FreeRTOS_printf( ( "UDP received %lu + %lu = %lu\n",
					ulUDPRecvCountShown,
					ulNewBytes,
					ulUDPRecvCount ) );
				ulUDPRecvCountShown = ulUDPRecvCount;
			}
		}
#endif /* ipconfigIPERF_HAS_UDP */
	}
}
