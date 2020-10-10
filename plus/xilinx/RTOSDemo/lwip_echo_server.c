/*
 * A sample echo server using lwIP
 */

#include <stdio.h>
#include <string.h>
#if __MICROBLAZE__ || __PPC__
#include "xmk.h"
#endif

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

/* lwIP includes. */

#include "lwip/inet.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwipopts.h"

#include "lwip_echo_server.h"


#define	ECHO_PORT		8080

extern int lUDPLoggingPrintf( const char *pcFormatString, ... );

static char pcRetString[29200];

static void echo_recv_task( void *pvParameters );
static void echo_send_task( void *pvParameters );

typedef struct {
	int sd;
	union {
		struct {
			unsigned
				recv_connected : 1,
				send_connected : 1,
				conn_closed : 1;
		};
		unsigned flags;
	};
	unsigned bytes_recv;
	unsigned bytes_send;
	unsigned uSequenceNr;
	TaskHandle_t xRecvTaskHandle;
	TaskHandle_t xSendTaskHandle;
	TickType_t xStartTime;
	SemaphoreHandle_t xSemaphore;
} Connection_t;

#define STACK_ECHO_SERVER_TASK  ( 512 + 256 )
#define	PRIO_ECHO_SERVER_TASK     2

/* lwIP echo server */
void lwip_echo_application_thread( void *parameters )
{
	int sock, new_sd;
	struct sockaddr_in xAddress, xRemote;
	int size = sizeof(xRemote);
	unsigned uSequenceNr = 0;

	/* create a TCP socket */
	sock = lwip_socket(AF_INET, SOCK_STREAM, 0);
	if ( sock < 0 )
	{
		vTaskDelete( NULL );
		return;
	}

	/* bind to port 80 at any interface */
	xAddress.sin_family = AF_INET;
	xAddress.sin_port = htons( ECHO_PORT );
	xAddress.sin_addr.s_addr = INADDR_ANY;
	if (lwip_bind(sock, (struct sockaddr *)&xAddress, sizeof (xAddress)) < 0) {
		vTaskDelete( NULL );
		return;
	}

	/* listen for incoming connections */
	lwip_listen(sock, 0);

	lUDPLoggingPrintf( "Started echo server on port 8080\n" );

	for( ;; )
	{
		new_sd = lwip_accept(sock, (struct sockaddr *)&xRemote, (socklen_t *)&size);
		/* Start 2 tasks for each request: one for RX, one for TX. */
		if( new_sd >= 0 )
		{
		uint32_t ulTimeout = 10;

			lUDPLoggingPrintf( "Echo client %d on port 8080\n", new_sd );

			lwip_setsockopt( new_sd, IPPROTO_TCP, SO_RCVTIMEO, (const void *)&ulTimeout, sizeof ulTimeout );

			Connection_t *t = ( Connection_t *) pvPortMalloc( sizeof *t );
			memset( t, '\0', sizeof *t );
			t->sd = new_sd;
			t->recv_connected = 1;
			t->send_connected = 1;
			t->uSequenceNr = uSequenceNr++;
//			t->xSemaphore = xSemaphoreCreateBinary();
			t->xSemaphore = xQueueCreate (16, 0);

			xTaskCreate( echo_send_task, "lwip_send", STACK_ECHO_SERVER_TASK, (void*)t, PRIO_ECHO_SERVER_TASK+0, &( t->xSendTaskHandle ) );
			xTaskCreate( echo_recv_task, "lwip_recv", STACK_ECHO_SERVER_TASK, (void*)t, PRIO_ECHO_SERVER_TASK+1, &( t->xRecvTaskHandle ) );
		}
	}
}

enum tcp_state {
  CLOSED      = 0,
  LISTEN      = 1,
  SYN_SENT    = 2,
  SYN_RCVD    = 3,
  ESTABLISHED = 4,
  FIN_WAIT_1  = 5,
  FIN_WAIT_2  = 6,
  CLOSE_WAIT  = 7,
  CLOSING     = 8,
  LAST_ACK    = 9,
  TIME_WAIT   = 10
};

const char *tcp_state_name( int iState )
{
	switch( iState )
	{
	case CLOSED       : return "CLOSED";
	case LISTEN       : return "LISTEN";
	case SYN_SENT     : return "SYN_SENT";
	case SYN_RCVD     : return "SYN_RCVD";
	case ESTABLISHED  : return "ESTABLISHED";
	case FIN_WAIT_1   : return "FIN_WAIT_1";
	case FIN_WAIT_2   : return "FIN_WAIT_2";
	case CLOSE_WAIT   : return "CLOSE_WAIT";
	case CLOSING      : return "CLOSING";
	case LAST_ACK     : return "LAST_ACK";
	case TIME_WAIT    : return "TIME_WAIT";
	}
	static char retString[16];
	snprintf( retString, sizeof retString, "STATE_%d", iState );

	return retString;
}

/* Hack: made a small addition to lwIP library: returns the TCP connection status. */
int socket_status(int s);

extern volatile TickType_t xTickCount;

static void echo_recv_task( void *pvParameters )
{
volatile Connection_t *t = ( volatile Connection_t * ) pvParameters;
int laststatus = 0, iErrnoCpy;

	t->recv_connected = 1;
	lUDPLoggingPrintf( "echo_recv: Starting Session %u\n", t->uSequenceNr );
	for( ;; )
	{
	int lReturnCode;

		errno = 0;
		lReturnCode = lwip_recv( t->sd, pcRetString, sizeof pcRetString, 0 );
		/* Not sure if 'errno' is thread aware, make a copy as quickly
		as possible. */
		iErrnoCpy = errno;
		if( ( lReturnCode < 0 ) || ( iErrnoCpy == ENOTCONN ) )
		{
			t->conn_closed = pdTRUE;
			lUDPLoggingPrintf( "echo_recv: lReturnCode %d errno %d\n", lReturnCode, iErrnoCpy );
			break;
		}
		if( lReturnCode > 0 )
		{
			/* Bytes have been received. */
			if( t->xStartTime == 0ul )
			{
				t->xStartTime = xTickCount;
			}
			t->bytes_recv += lReturnCode;

			if( t->xSemaphore != NULL )
			{
				xSemaphoreGive( t->xSemaphore );
			}
		}
		else
		{
			/* Ask for the lwIP TCP connection status. */
			int status = socket_status( t->sd );
			if( laststatus != status )
			{
				laststatus = status;
				lUDPLoggingPrintf( "echo_recv: status %s (%d)\n",
					tcp_state_name( status ), status );
				if (status == CLOSE_WAIT)
				{
					t->conn_closed = pdTRUE;
					break;
				}
			}
		}
	}

	t->recv_connected = 0;
	/* This task doesn't use the socket any more. Now wait
	for the sending task to confirm that it stopped. */
	while( t->send_connected )
	{
		vTaskDelay( 10 );
		if( t->xSemaphore != NULL )
		{
			xSemaphoreGive( t->xSemaphore );
		}
	}
	/* Close the socket. */
	close( t->sd );
	lUDPLoggingPrintf( "echo_recv: %u bytes\n", t->bytes_recv );

	if( t->xSemaphore != NULL )
	{
		vSemaphoreDelete( t->xSemaphore );
	}

	vPortFree( ( void * ) t );

	vTaskDelete( NULL );
}


static void echo_send_task( void *pvParameters )
{
volatile Connection_t *t = ( volatile Connection_t * ) pvParameters;
unsigned total_sent = 0;

	lUDPLoggingPrintf( "echo_send: Starting (recv %d)\n", t->recv_connected );


	while( t->recv_connected != 0 )
	{
	int diff;
	
		diff = t->bytes_recv - t->bytes_send;
		if( diff > 0 )
		{
			int count = ( diff > 1460 ) ? 1460 : diff;
			lwip_send( t->sd, (const void *)pcRetString, count, 0 );

			total_sent += count;
			t->bytes_send += count;
		}

		if( t->conn_closed != pdFALSE )
		{
			break;
		}
		if( t->bytes_recv == t->bytes_send )
		{
			if( t->xSemaphore != NULL )
			{
				xSemaphoreTake( t->xSemaphore, 20 );
			}
			else
			{
				vTaskDelay( 20 );
			}
		}
	}

	/* Inform the other task that the Send task is ready. */
	t->send_connected = 0;
	{
		TickType_t xDelta = t->xStartTime ? ( xTickCount - t->xStartTime ) : 0;
		uint32_t ulAverage = 0;
		if( xDelta != 0ul )
		{
			ulAverage = ( uint32_t ) ( ( uint64_t ) ( 2000ull * total_sent ) / ( xDelta * 1024ull ) );
		}

		lUDPLoggingPrintf( "echo_send: %u bytes %lu.%03lu MB/sec %lu ms\n", total_sent,
			ulAverage / 1024, ulAverage % 1024, xDelta );
	}
//	for( ;; )
//	{
//		vTaskDelay( 20 );
//	}
	vTaskDelete( NULL );
}
