/* Standard includes. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"
#include "task.h"

void handle_user_test( uint32_t ulIPAddress, BaseType_t xPortNumber )
{
	Socket_t xSocket = FreeRTOS_socket( FREERTOS_AF_INET, FREERTOS_SOCK_STREAM, FREERTOS_IPPROTO_TCP );
	if( xSocketValid( xSocket ) == pdTRUE )
	{
		struct freertos_sockaddr xAddress;
		FreeRTOS_printf( ( "connect-test: socket() OK, calling bind()\n" ) );
		memset( &( xAddress ), 0, sizeof xAddress );
		BaseType_t rc = FreeRTOS_bind( xSocket, &( xAddress ), (socklen_t) sizeof xAddress );
		if( rc == 0 )
		{
			TickType_t xSmallTimeout = pdMS_TO_TICKS( 20000 );
			struct freertos_sockaddr xPeerAddress;
			memset( &( xPeerAddress ), 0, sizeof xPeerAddress );
			/* Note that for connect(), SNDTIMEO must be set.
			 * For accept(), RCVTIMEO must be configured. */
			FreeRTOS_setsockopt( xSocket, 0, FREERTOS_SO_RCVTIMEO, ( void * ) &xSmallTimeout, sizeof( BaseType_t ) );
			FreeRTOS_setsockopt( xSocket, 0, FREERTOS_SO_SNDTIMEO, ( void * ) &xSmallTimeout, sizeof( BaseType_t ) );

			FreeRTOS_printf( ( "connect-test: bind() OK, calling connect()\n" ) );
			xPeerAddress.sin_len = sizeof xPeerAddress;
			xPeerAddress.sin_family = FREERTOS_AF_INET;
			xPeerAddress.sin_port = xPortNumber;
			xPeerAddress.sin_address.ulIP_IPv4 = ulIPAddress;
			rc = FreeRTOS_connect( xSocket, &( xPeerAddress ), (socklen_t) sizeof xPeerAddress );
			if (rc == 0) {
				char pcBuffer[80];
				BaseType_t xLoopCount = 10;

				/* Now that the connection is established, use a normal timeout like 1000 ms. */
				xSmallTimeout = pdMS_TO_TICKS( 1000 );
				FreeRTOS_setsockopt( xSocket, 0, FREERTOS_SO_RCVTIMEO, ( void * ) &xSmallTimeout, sizeof( BaseType_t ) );
				FreeRTOS_setsockopt( xSocket, 0, FREERTOS_SO_SNDTIMEO, ( void * ) &xSmallTimeout, sizeof( BaseType_t ) );

				FreeRTOS_printf( ( "connect-test: connect() OK, calling send()\n" ) );
				memset (pcBuffer, 'a', sizeof pcBuffer);
				rc = FreeRTOS_send( xSocket, pcBuffer, sizeof pcBuffer, 0 );
				for( ;; )
				{
					rc = FreeRTOS_recv( xSocket, pcBuffer, sizeof pcBuffer, 0 );
					if( rc < 0 && rc != -pdFREERTOS_ERRNO_EAGAIN) {
						break;
					}
					if (rc > 0) {
						FreeRTOS_printf( ( "connect-test: Received %d bytes\n", ( int ) rc ) );
						FreeRTOS_printf( ( "connect-test: calling shutdown()\n" ) );
						FreeRTOS_shutdown( xSocket, 0U );
					}
					if( --xLoopCount == 0 )
					{
						FreeRTOS_printf( ( "connect-test: giving up\n" ) );
						break;
					}
				}
			}
			else
			{
				FreeRTOS_printf( ( "connect-test: connect() failed with rc %d\n", ( int ) rc ) );
			}
		}
		else
		{
			FreeRTOS_printf( ( "connect-test: bind() failed with rc %d\n", ( int ) rc ) );
		}
		FreeRTOS_printf( ( "connect-test: calling closesocket()\n" ) );
		FreeRTOS_closesocket( xSocket );
	}
	else
	{
		FreeRTOS_printf( ( "connect-test: The socket is invalid\n" ) );
	}
}
