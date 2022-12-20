/*
 * telnet.c
 */


/* Standard includes. */
#include <stdio.h>
#include <time.h>

/* FreeRTOS includes. */
#include <FreeRTOS.h>
#include "task.h"
#include "timers.h"
#include "queue.h"
#include "semphr.h"

/* FreeRTOS+TCP includes. */
#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"
#include "FreeRTOS_DHCP.h"

#include "telnet.h"

#ifndef TELNET_RVCBUF
	#define TELNET_RVCBUF    ipconfigTCP_RX_BUFFER_LENGTH
#endif

#ifndef TELNET_SNDBUF
	#define TELNET_SNDBUF    ipconfigTCP_TX_BUFFER_LENGTH
#endif

/*#undef TELNET_USES_REUSE_SOCKETS */
#ifndef TELNET_USES_REUSE_SOCKETS
	#define TELNET_USES_REUSE_SOCKETS    0
#endif

/* This semaphore will be connected to all sockets created for telnet. */
#if ( ipconfigSOCKET_HAS_USER_SEMAPHORE == 1 )
	SemaphoreHandle_t xTelnetSemaphore = NULL;
#endif

#warning Remove this uxTelnetAcceptTmout when ready testing
TickType_t uxTelnetAcceptTmout = pdMS_TO_TICKS( 10U );
static TelnetClient_t * pxTelnetAddSocket( Telnet_t * pxTelnet );
static void vTelnetRemove( Telnet_t * pxTelnet,
						   TelnetClient_t * pxClient );
static BaseType_t prvCreateParent( Telnet_t * pxTelnet,
								   BaseType_t xPortNr );

static TelnetClient_t * pxTelnetAddSocket( Telnet_t * pxTelnet )
{
	TelnetClient_t * pxNewClient;

	pxNewClient = pvPortMalloc( sizeof( *pxNewClient ) );

	if( pxNewClient != NULL )
	{
		memset( pxNewClient, '\0', sizeof( *pxNewClient ) );

		if( pxTelnet->xClients == NULL )
		{
			pxTelnet->xClients = pxNewClient;
		}
		else
		{
			TelnetClient_t * pxClient;

			pxClient = pxTelnet->xClients;

			while( pxClient->pxNext != NULL )
			{
				pxClient = pxClient->pxNext;
			}

			pxClient->pxNext = pxNewClient;
		}
	}

	return pxNewClient;
}
/*-----------------------------------------------------------*/

static void vTelnetRemove( Telnet_t * pxTelnet,
						   TelnetClient_t * pxClient )
{
	TelnetClient_t * pxList;

	if( pxTelnet->xClients == pxClient )
	{
		pxTelnet->xClients = pxClient->pxNext;
	}
	else
	{
		pxList = pxTelnet->xClients;

		do
		{
			if( pxList->pxNext == pxClient )
			{
				pxList->pxNext = pxClient->pxNext;
				break;
			}

			pxList = pxList->pxNext;
		} while( pxList != NULL );
	}

	FreeRTOS_closesocket( pxClient->xSocket );
	#if ( TELNET_USES_REUSE_SOCKETS != 0 )
		{
			if( pxTelnet->xParentSocket == NULL )
			{
				( void ) prvCreateParent( pxTelnet, pxTelnet->xPortNr );
			}
		}
	#endif
	vPortFree( pxClient );
}
/*-----------------------------------------------------------*/

BaseType_t xTelnetSend( Telnet_t * pxTelnet,
						struct freertos_sockaddr * pxAddress,
						const char * pcBuffer,
						BaseType_t xLength )
{
	TelnetClient_t * pxClient, * pxNextClient = NULL;
	BaseType_t xResult = 0;

	for( pxClient = pxTelnet->xClients; pxClient != NULL; pxClient = pxNextClient )
	{
		/* Make a copy of pxNextClient, because pxClient might be deleted in case send() failes. */
		pxNextClient = pxClient->pxNext;

		/* Send to all, or send to a specific IP/port address. */
		if( ( pxAddress == NULL ) ||
			( ( pxAddress->sin_addr == pxClient->xAddress.sin_addr ) && ( pxAddress->sin_port == pxClient->xAddress.sin_port ) ) )
		{
			xResult = FreeRTOS_send( pxClient->xSocket, pcBuffer, xLength, 0 );

			if( ( xResult < 0 ) && ( xResult != -pdFREERTOS_ERRNO_EAGAIN ) && ( xResult != -pdFREERTOS_ERRNO_EINTR ) )
			{
				FreeRTOS_printf( ( "xTelnetSend: client %p disconnected (rc %d)\n", pxClient->xSocket, ( int ) xResult ) );
				vTelnetRemove( pxTelnet, pxClient );
			}

			if( pxAddress != NULL )
			{
				break;
			}
		}
	}

	return xResult;
}
/*-----------------------------------------------------------*/

BaseType_t xTelnetRecv( Telnet_t * pxTelnet,
						struct freertos_sockaddr * pxAddress,
						char * pcBuffer,
						BaseType_t xMaxLength )
{
	Socket_t xSocket;

	#if ( ipconfigUSE_IPv6 != 0 )
		struct freertos_sockaddr6 xAddress;
		struct freertos_sockaddr * xAddress4 = ( struct freertos_sockaddr * ) &( xAddress );
	#else
		struct freertos_sockaddr xAddress;
		struct freertos_sockaddr * xAddress4 = ( struct freertos_sockaddr * ) &( xAddress );
	#endif
	socklen_t xSize = sizeof( xAddress );
	BaseType_t xResult = 0;
	TickType_t uxShortTmout = pdMS_TO_TICKS( 10U );

	if( xSocketValid( pxTelnet->xParentSocket ) )
	{
		xAddress.sin_len = sizeof( xAddress );
		xAddress.sin_family = FREERTOS_AF_INET;

		( void ) FreeRTOS_setsockopt( pxTelnet->xParentSocket, 0, FREERTOS_SO_RCVTIMEO, &uxTelnetAcceptTmout, sizeof( uxTelnetAcceptTmout ) );
		xSocket = FreeRTOS_accept( pxTelnet->xParentSocket, ( struct freertos_sockaddr * ) &xAddress, &xSize );

		if( xSocketValid( xSocket ) )
		{
			#if ( TELNET_USES_REUSE_SOCKETS != 0 )
				{
					/* The parent socket has turned into a child socket. */
					pxTelnet->xParentSocket = NULL;
				}
			#endif

			( void ) FreeRTOS_setsockopt( xSocket, 0, FREERTOS_SO_RCVTIMEO, &uxShortTmout, sizeof( uxShortTmout ) );

			#if ( ipconfigUSE_IPv6 != 0 )
				if( xAddress.sin_family == FREERTOS_AF_INET6 )
				{
					struct freertos_sockaddr6 * pxAddress6 = ( struct freertos_sockaddr6 * ) &( xAddress );
					FreeRTOS_printf( ( "xTelnetRead: new client from %pip:%u\n",
									   pxAddress6->sin_addrv6.ucBytes,
									   ( unsigned ) FreeRTOS_ntohs( pxAddress6->sin_port ) ) );
				}
				else
			#endif
			{
				FreeRTOS_printf( ( "xTelnetRead: new client from %xip:%u\n",
								   ( unsigned ) FreeRTOS_ntohl( xAddress4->sin_addr ),
								   ( unsigned ) FreeRTOS_ntohs( xAddress4->sin_port ) ) );
			}

			TelnetClient_t * pxClient = pxTelnetAddSocket( pxTelnet );

			if( pxClient != NULL )
			{
				pxClient->xSocket = xSocket;
				memcpy( &pxClient->xAddress, &xAddress, sizeof( pxClient->xAddress ) );
			}
		} /* if( xSocketValid( xSocket ) ) */
	}     /* if( pxTelnet->xParentSocket != NULL ) */

	{
		TelnetClient_t * pxClient, * pxNextClient = NULL;
		xResult = 0;

		for( pxClient = pxTelnet->xClients; pxClient != NULL; pxClient = pxNextClient )
		{
			/* Make a copy of pxNext, because pxClient might be deleted in case recv() fails. */
			pxNextClient = pxClient->pxNext;
			xSocket = pxClient->xSocket;

			xResult = FreeRTOS_recv( xSocket, pcBuffer, xMaxLength, 0 );

			if( xResult > 0 )
			{
				if( pxAddress != NULL )
				{
					/* Return the address of the remote client. */
					memcpy( pxAddress, &pxClient->xAddress, sizeof( *pxAddress ) );
				}

				break;
			}

			if( ( xResult < 0 ) && ( xResult != -pdFREERTOS_ERRNO_EAGAIN ) && ( xResult != -pdFREERTOS_ERRNO_EINTR ) )
			{
				FreeRTOS_printf( ( "xTelnetRead: client %p disconnected (rc %d)\n", xSocket, ( int ) xResult ) );
				vTelnetRemove( pxTelnet, pxClient );
			}
		}
	}

	return xResult;
}
/*-----------------------------------------------------------*/

static BaseType_t prvCreateParent( Telnet_t * pxTelnet,
								   BaseType_t xPortNr )
{
	struct freertos_sockaddr xBindAddress;
	BaseType_t xSendTimeOut = pdMS_TO_TICKS( 100U );
	BaseType_t xResult = 0;

	pxTelnet->xPortNr = xPortNr;
	/* Attempt to open the socket. */
	pxTelnet->xParentSocket = FreeRTOS_socket( FREERTOS_AF_INET, FREERTOS_SOCK_STREAM, FREERTOS_IPPROTO_TCP );

	if( xSocketValid( pxTelnet->xParentSocket ) == pdFALSE )
	{
		xResult = -pdFREERTOS_ERRNO_ENOMEM;
		/* Don't like the value 'FREERTOS_INVALID_SOCKET'. */
		pxTelnet->xParentSocket = NULL;
	}
	else
	{
		#if ( TELNET_USES_REUSE_SOCKETS != 0 )
			{
				BaseType_t xReuseSocket = pdTRUE;
				FreeRTOS_setsockopt( pxTelnet->xParentSocket, 0, FREERTOS_SO_REUSE_LISTEN_SOCKET, &xReuseSocket, sizeof( xReuseSocket ) );
			}
		#endif
		{
			/* This semaphore will be connected to all sockets created for telnet. */
			#if ( ipconfigSOCKET_HAS_USER_SEMAPHORE == 1 )
				if( xTelnetSemaphore )
				{
					FreeRTOS_setsockopt( pxTelnet->xParentSocket, 0, FREERTOS_SO_SET_SEMAPHORE, xTelnetSemaphore, sizeof( xTelnetSemaphore ) );
				}
			#endif /* ( ipconfigSOCKET_HAS_USER_SEMAPHORE == 1 ) */

			/* Set the time-outs for both sending and receiving data to zero. */
			xResult = FreeRTOS_setsockopt( pxTelnet->xParentSocket, 0, FREERTOS_SO_RCVTIMEO, &uxTelnetAcceptTmout, sizeof( uxTelnetAcceptTmout ) );
			xResult = FreeRTOS_setsockopt( pxTelnet->xParentSocket, 0, FREERTOS_SO_SNDTIMEO, &xSendTimeOut, sizeof( xSendTimeOut ) );

			size_t uxRCVBUF = TELNET_RVCBUF;
			size_t uxSNDBUF = TELNET_SNDBUF;
			xResult = FreeRTOS_setsockopt( pxTelnet->xParentSocket, 0, FREERTOS_SO_RCVBUF, &uxRCVBUF, sizeof( uxRCVBUF ) );
			xResult = FreeRTOS_setsockopt( pxTelnet->xParentSocket, 0, FREERTOS_SO_SNDBUF, &uxSNDBUF, sizeof( uxSNDBUF ) );

			if( xResult >= 0 )
			{
				xBindAddress.sin_addr = 0;
				xBindAddress.sin_port = FreeRTOS_htons( xPortNr );
				xResult = FreeRTOS_bind( pxTelnet->xParentSocket, &xBindAddress, sizeof( xBindAddress ) );

				if( xResult >= 0 )
				{
					/* Limit the maximum number of simultaneous clients. */
					xResult = FreeRTOS_listen( pxTelnet->xParentSocket, TELNET_MAX_CLIENT_COUNT );
					FreeRTOS_printf( ( "prvCreateParent: FreeRTOS_listen(%d) rc %d\n",
									   ( int ) xPortNr, ( int ) xResult ) );
				}
				else
				{
					FreeRTOS_printf( ( "prvCreateParent: FreeRTOS_bind(%d) failed with rc %d\n",
									   ( int ) xPortNr, ( int ) xResult ) );
				}
			}
		}
	}

	FreeRTOS_printf( ( "prvCreateParent: socket %p created: rc %d port %u\n",
					   ( void * ) pxTelnet->xParentSocket, ( int ) xResult, ( unsigned ) FreeRTOS_htons( xPortNr ) ) );
	return xResult;
}

BaseType_t xTelnetCreate( Telnet_t * pxTelnet,
						  BaseType_t xPortNr )
{
	BaseType_t xResult = 0;

	memset( pxTelnet, '\0', sizeof( *pxTelnet ) );
	xResult = prvCreateParent( pxTelnet, xPortNr );

	return xResult;
}
/*-----------------------------------------------------------*/
