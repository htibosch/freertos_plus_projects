/*
 * A simple example of how to use the simple telnet server.
 */
#include <stdio.h>

#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"
#include "telnet.h"

static Telnet_t myTelnet;
static char pcBuffer[ 129 ];
static TaskHandle_t xHandle = NULL;
static void prvTelnetLoop( void * pvParameter );

BaseType_t xSetupTelnet()
{
	BaseType_t xReturn = 0;

	if( xHandle == NULL )
	{
		/*
		 * Make sure that the IP-stack is up and running before calling this function.
		 * Don't call more than once.
		 */
		xReturn = xTelnetCreate( &myTelnet, 23002U ); /* Use port number 23002 */

		if( xReturn >= 0 )
		{
			xTaskCreate( prvTelnetLoop,        /* The function. */
						 "Telnet",             /* Name of the task. */
						 256U,                 /* Stack size ( words ) */
						 NULL,                 /* pvParameter. */
						 1,                    /* Priority. */
						 &( xHandle ) );   /* Pointer to store handle. */
		}
	}

	return xReturn;
}


static void prvTelnetLoop( void * pvParameter )
{
	struct freertos_sockaddr xPeerAddress;

	( void ) pvParameter;

	for( ; ; )
	{
		BaseType_t xResult = xTelnetRecv( &( myTelnet ), &( xPeerAddress ), pcBuffer, sizeof pcBuffer );

		if( xResult > 0 )
		{
			FreeRTOS_printf( ( "Received %d bytes\n", ( int ) xResult ) );
			xResult = snprintf( pcBuffer, sizeof pcBuffer, "Thank you\n" );
			xTelnetSend( &( myTelnet ), &( xPeerAddress ), pcBuffer, xResult );
		}
	}
}
