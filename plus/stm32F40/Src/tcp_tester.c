/**
 * @file
 * @brief TCP/IP test.
 *
 * Based on FreeRTOS LABS TCP/IP stack.
 *
 * Assumes both machine is little-endian (for testing).
 *
 * Tamas Selmeci <selmeci.tamas@bme-infokom.hu>, 2015
 */
//#include <libshared/shared.h>
//#include <libtcpip/libtcpip.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>

/* FreeRTOS+TCP includes. */
#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"
#include "FreeRTOS_IP_Private.h"
#include "NetworkBufferManagement.h"

//#include "logbuf.h"
//#include "plusIpTask.h"

#include "tcp_tester.h"

extern int verboseLevel;
static Socket_t parent_socket, child_socket;

#define TEST_PORT	2020

#define	STACK_TUNER_TASK	( 640  )
#define	PRIO_TUNER_TASK     2

#define REUSE_ENABLED		1

static __inline void clearWDT( void )
{
}

static void tcpTestTask(void *pvArgs);

void tcp_test_start()
{
	xTaskCreate( tcpTestTask, "tcpTest", STACK_TUNER_TASK, NULL, PRIO_TUNER_TASK, NULL );
}

static int tcp_test_open(void)
{
TickType_t to;
int rc;
struct freertos_sockaddr sa;
WinProperties_t xWinProps;

	parent_socket = FreeRTOS_socket(FREERTOS_AF_INET, FREERTOS_SOCK_STREAM, FREERTOS_IPPROTO_TCP);
	if ((parent_socket == FREERTOS_INVALID_SOCKET) || (parent_socket == NULL))
	{
		FreeRTOS_printf( ("TcpTester: FreeRTOS_socket: return %p\n", parent_socket ) );
		return -1;
	}
	/* set blocking state */
	to = pdMS_TO_TICKS(200);
	rc = FreeRTOS_setsockopt(parent_socket, 0, FREERTOS_SO_RCVTIMEO, &to, 0);
	if (rc != 0)
	{
		FreeRTOS_printf( ("TcpTester: FreeRTOS_setsockopt: return %d\n", rc ) );
		return -2;
	}

	/* Fill in the buffer and window sizes that will be used by the socket. */
	memset (&xWinProps, '\0', sizeof xWinProps);
	xWinProps.lTxBufSize = 3 * 1460;
	xWinProps.lTxWinSize = 2;
	xWinProps.lRxBufSize = 3 * 1460;
	xWinProps.lRxWinSize = 2;

	/* Set the window and buffer sizes. */
	FreeRTOS_setsockopt( parent_socket, 0, FREERTOS_SO_WIN_PROPERTIES, ( void * ) &xWinProps,	sizeof( xWinProps ) );

	#if( REUSE_ENABLED != 0 )
	{
		BaseType_t xTrue = 1;
		FreeRTOS_setsockopt( parent_socket, 0, FREERTOS_SO_REUSE_LISTEN_SOCKET, ( void * ) &xTrue,	sizeof( xTrue ) );
	}
	#endif	/* ( REUSE_ENABLED != 0 ) */

	/* TCP: incoming */
	memset(&sa, 0, sizeof(sa));
	/* setting sin_addr doesn't seem to have any effect, it simply receives all
	packets and the user must filter them out */
	sa.sin_port = FreeRTOS_htons(TEST_PORT);
	rc = FreeRTOS_bind(parent_socket, &sa, sizeof(sa));
	if (rc != 0)
	{
		FreeRTOS_printf( ("TcpTester: FreeRTOS_bind: return %d\n", rc ) );
		return -3;
	}
	
	rc = FreeRTOS_listen(parent_socket, 2);
	if (rc != 0)
	{
		FreeRTOS_printf( ("TcpTester: FreeRTOS_listen: return %d\n", rc ) );
		return -4;
	}

	FreeRTOS_printf( ("TcpTester: Socket on port %d\n", TEST_PORT ) );
	return 1;
}

static char ioBuffer[4 * 1460];

static int expectCount, receiveCount;

BaseType_t xValidSocket( Socket_t xSocket )
{
	return ( ( xSocket != NULL ) && ( xSocket != FREERTOS_INVALID_SOCKET ) ) ? pdTRUE : pdFALSE;
}

static void tcp_test_work(void)
{
int rc;
int myTurn = pdFALSE;

	if ( xValidSocket( child_socket ) )
	{
		rc = FreeRTOS_recv(child_socket, ioBuffer, sizeof ioBuffer, 0);
		if (rc > 0)
		{
			char isGarbage = pdFALSE;
			char isHeader = memcmp (ioBuffer, "_A_", 3) == 0;

			if (receiveCount > 0 || isHeader) {
				if (isHeader) {
					char extra;
					if (sscanf (ioBuffer + 3, "%d%c", &expectCount, &extra) != 2 ||
						extra != '_') {
						isGarbage = pdTRUE;
						expectCount = 0;
					} else if (verboseLevel) {
						FreeRTOS_printf( ("Recv: '%-8.8s' %d bytes\n", ioBuffer,expectCount ) );
					}
					receiveCount = 0;
				}
				receiveCount += rc;
				if (receiveCount >= expectCount) {
					myTurn = pdTRUE;
					expectCount = 0;
					receiveCount = 0;
				}
			} else {
				isGarbage = pdTRUE;
			}
			if (isGarbage) {
				const unsigned char *ptr = (const unsigned char *)ioBuffer;
				FreeRTOS_printf( ("Received %d bytes garbage: '%-3.3s' (%02x %02x %02x)\n",
					rc, ioBuffer, ptr[0], ptr[1], ptr[2] ) );
			}
		}
		if (myTurn) {
		int min_bytes;
		int length;
		int i;
			min_bytes = (1 * sizeof ioBuffer) / 3;
			length = min_bytes + (ipconfigRAND32() % (sizeof ioBuffer - min_bytes));

			for (i = 0; i < length; i++) {
				ioBuffer[i] = 'a' + (i % 26);
			}
			snprintf(ioBuffer, sizeof ioBuffer, "_A_%d_", length);
			rc = FreeRTOS_send(child_socket, ioBuffer, length, 0);
		}
		if (rc < 0)
		{
			FreeRTOS_printf( ("TcpTester: FreeRTOS_recv/send: returned %d\n", rc ) );
			FreeRTOS_closesocket( child_socket );
			#if( REUSE_ENABLED != 0 )
			{
				parent_socket = NULL;
				if( tcp_test_open() < 0 )
				{
					FreeRTOS_closesocket( parent_socket );
					parent_socket = NULL;
				}
			}
			#endif
			child_socket = NULL;
		}
	}
	else if( parent_socket != NULL )
	{
	TickType_t to;
	socklen_t addr_len;
	struct freertos_sockaddr sa_in;
	BaseType_t isClosed;

		/* wait for a connection */
		addr_len = sizeof(sa_in);
		#if( REUSE_ENABLED != 0 )
		isClosed = ( FreeRTOS_connstatus( parent_socket ) == eCLOSED ) ? pdTRUE : pdFALSE;
		if( isClosed != 0 )
		{
			child_socket = FREERTOS_INVALID_SOCKET;
		}
		else
		#endif
		{
			child_socket = FreeRTOS_accept(parent_socket, &sa_in, &addr_len);
		}
		if( child_socket == FREERTOS_INVALID_SOCKET )
		{
			FreeRTOS_printf( ( "FreeRTOS_accept() returned an error\n" ) );
			FreeRTOS_closesocket( parent_socket );	/* Close the socket to free the port. */
			parent_socket = NULL;
			child_socket = NULL;
			if( tcp_test_open() < 0 )
			{
				FreeRTOS_closesocket( parent_socket );
				parent_socket = NULL;
			}
		}
		else if (child_socket != NULL )
		{
			FreeRTOS_printf( ("TcpTester: Received child connection\n" ) );

			/* set blocking state */
			to = pdMS_TO_TICKS(200);

			rc = FreeRTOS_setsockopt(child_socket, 0, FREERTOS_SO_RCVTIMEO, &to, 0);
			rc = FreeRTOS_setsockopt(child_socket, 0, FREERTOS_SO_SNDTIMEO, &to, 0);
		}
	}
}

static void tcpTestTask(void *pvArgs)
{
BaseType_t xStatus = 0;

	( void ) pvArgs;
	for (;;)
	{
		clearWDT ();
		if (xStatus != 2) {
			vTaskDelay (200);
		}
		switch (xStatus)
		{
			case 0:
				if( FreeRTOS_IsNetworkUp() )
				{
					if (tcp_test_open() >= 0)
					{
						xStatus = 2;
					}
					else
					{
						xStatus = 1;
						FreeRTOS_closesocket( parent_socket );
						parent_socket = NULL;
					}
				}
				break;
			case 1:
				/* creation of socket failed. */
				break;
			case 2:
				{
					tcp_test_work();
				}
				break;
		}
	}
}

