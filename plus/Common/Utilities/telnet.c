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

static TelnetClient_t *pxTelnetAddSocket( Telnet_t *pxTelnet );
static void vTelnetRemove( Telnet_t * pxTelnet, TelnetClient_t *pxClient );

static TelnetClient_t *pxTelnetAddSocket( Telnet_t *pxTelnet )
{
TelnetClient_t *pxNewClient;

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
		TelnetClient_t *pxClient;

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

static void vTelnetRemove( Telnet_t * pxTelnet, TelnetClient_t *pxClient )
{
TelnetClient_t *pxList;

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
	vPortFree( pxClient );
}
/*-----------------------------------------------------------*/

BaseType_t xTelnetSend( Telnet_t * pxTelnet, struct freertos_sockaddr *pxAddress, const char *pcBuffer, BaseType_t xLength )
{
TelnetClient_t *pxClient, *pxNext;
BaseType_t xResult = 0;

	pxClient = pxTelnet->xClients;
	while( pxClient != NULL )
	{
		/* Make a copy of pxNext, because pxClient might be deleted in case send() failes. */
		pxNext = pxClient->pxNext;

		/* Send to all, or send to a specific IP/port address. */
		if( ( pxAddress == NULL ) ||
			( ( pxAddress->sin_addr == pxClient->xAddress.sin_addr ) && ( pxAddress->sin_port == pxClient->xAddress.sin_port ) ) )
		{
			xResult = FreeRTOS_send( pxClient->xSocket, pcBuffer, xLength, 0 );
			if( ( xResult < 0 ) && ( xResult != -pdFREERTOS_ERRNO_EAGAIN ) && ( xResult != -pdFREERTOS_ERRNO_EINTR ) )
			{
				FreeRTOS_printf( ( "xTelnetSend: client %p disconnected (rc %d)\n", pxClient->xSocket, ( int )xResult ) );
				vTelnetRemove( pxTelnet, pxClient );
			}
			if( pxAddress != NULL )
			{
				break;
			}
		}
		pxClient = pxNext;
	}

	return xResult;
}
/*-----------------------------------------------------------*/

BaseType_t xTelnetRecv( Telnet_t * pxTelnet, struct freertos_sockaddr *pxAddress, char *pcBuffer, BaseType_t xMaxLength )
{
Socket_t xSocket;
#if( ipconfigUSE_IPv6 != 0 )
struct freertos_sockaddr6 xAddress;
struct freertos_sockaddr *xAddress4 = ( struct freertos_sockaddr * ) &( xAddress );
#else
struct freertos_sockaddr xAddress;
struct freertos_sockaddr *xAddress4 = ( struct freertos_sockaddr * ) &( xAddress );
#endif
socklen_t xSize = sizeof( xAddress );
TelnetClient_t *pxClient, *pxNextClient;
BaseType_t xResult = 0;

	if( pxTelnet->xParentSocket != NULL )
	{
		xAddress.sin_len = sizeof( xAddress );
		xAddress.sin_family = FREERTOS_AF_INET;

		xSocket = FreeRTOS_accept( pxTelnet->xParentSocket, ( struct freertos_sockaddr * ) &xAddress, &xSize );
		if( ( xSocket != NULL ) && ( xSocket != FREERTOS_INVALID_SOCKET ) )
		{
			#if( ipconfigUSE_IPv6 != 0 )
			if( xAddress.sin_family == FREERTOS_AF_INET6 )
			{
			struct freertos_sockaddr6 *pxAddress6 = ( struct freertos_sockaddr6 * ) &( xAddress );
				FreeRTOS_printf( ( "xTelnetRead: new client from %pip:%u\n",
					pxAddress6->sin_addrv6.ucBytes,
					( unsigned )FreeRTOS_ntohs( pxAddress6->sin_port ) ) );
			}
			else
			#endif
			{
				FreeRTOS_printf( ( "xTelnetRead: new client from %xip:%u\n",
					( unsigned )FreeRTOS_ntohl( xAddress4->sin_addr ),
					( unsigned )FreeRTOS_ntohs( xAddress4->sin_port ) ) );
			}
			pxClient = pxTelnetAddSocket( pxTelnet );
			if( pxClient != NULL )
			{
				pxClient->xSocket = xSocket;
				memcpy( &pxClient->xAddress, &xAddress, sizeof( pxClient->xAddress ) );
			}
		}
		pxClient = pxTelnet->xClients;
		while( pxClient != NULL )
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
			FreeRTOS_printf( ( "xTelnetRead: client %p disconnected (rc %d)\n", xSocket, ( int )xResult ) );
				vTelnetRemove( pxTelnet, pxClient );
			}
			pxClient = pxNextClient;
		}
	}

	return xResult;
}
/*-----------------------------------------------------------*/

BaseType_t xTelnetCreate( Telnet_t * pxTelnet, BaseType_t xPortNr )
{
#pragma warning Please change back
BaseType_t xSendTimeOut = portMAX_DELAY;
BaseType_t xRecvTimeOut = 10;
struct freertos_sockaddr xBindAddress;
BaseType_t xResult = 0;

	memset( pxTelnet, '\0', sizeof( *pxTelnet ) );

	/* Attempt to open the socket. */
	pxTelnet->xParentSocket = FreeRTOS_socket( FREERTOS_AF_INET, FREERTOS_SOCK_STREAM, FREERTOS_IPPROTO_TCP );
	if( ( pxTelnet->xParentSocket == FREERTOS_INVALID_SOCKET ) || ( pxTelnet->xParentSocket == NULL ) )
	{
		xResult = -pdFREERTOS_ERRNO_ENOMEM;
		/* Don't like the value 'FREERTOS_INVALID_SOCKET'. */
		pxTelnet->xParentSocket = NULL;
	}
	else
	{
		/* Set the time-outs for both sending and receiving data to zero. */
		xResult = FreeRTOS_setsockopt( pxTelnet->xParentSocket, 0, FREERTOS_SO_RCVTIMEO, &xRecvTimeOut, sizeof( xRecvTimeOut ) );
		xResult = FreeRTOS_setsockopt( pxTelnet->xParentSocket, 0, FREERTOS_SO_SNDTIMEO, &xSendTimeOut, sizeof( xSendTimeOut ) );

		if( xResult >= 0 )
		{
			xBindAddress.sin_addr = 0;
			xBindAddress.sin_port = FreeRTOS_htons( xPortNr );
			xResult = FreeRTOS_bind( pxTelnet->xParentSocket, &xBindAddress, sizeof( xBindAddress ) );
			if( xResult >= 0 )
			{
				/* Limit the maximum number of simultaneous clients. */
				xResult = FreeRTOS_listen( pxTelnet->xParentSocket, TELNET_MAX_CLIENT_COUNT );
			}
		}
	}

	FreeRTOS_printf( ( "xTelnetCreate: socket created: rc %ld port %u\n", xResult, ( unsigned )xPortNr ) );

	return xResult;
}
/*-----------------------------------------------------------*/
