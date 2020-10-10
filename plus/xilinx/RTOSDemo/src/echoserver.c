/* echoserver.c
 *
 * Copyright (C) 2006-2015 wolfSSL Inc.
 *
 * This file is part of wolfSSL. (formerly known as CyaSSL)
 *
 * wolfSSL is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * wolfSSL is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
 */


/* FreeRTOS includes. */
#include <FreeRTOS.h>
#include "task.h"
#include "timers.h"
#include "queue.h"
#include "semphr.h"

/* FreeRTOS+TCP includes. */
#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"

typedef void THREAD_RETURN;
typedef struct freertos_sockaddr SOCKADDR_IN_T;

#ifdef HAVE_CONFIG_H
    #include <config.h>
#endif

#include <cyassl/ssl.h> /* name change portability layer */
#include <cyassl/ctaocrypt/settings.h>
#ifdef HAVE_ECC
    #include <cyassl/ctaocrypt/ecc.h>   /* ecc_fp_free */
#endif

#if defined(WOLFSSL_MDK_ARM)
        #include <stdio.h>
        #include <string.h>

        #if defined(WOLFSSL_MDK5)
            #include "cmsis_os.h"
            #include "rl_fs.h" 
            #include "rl_net.h" 
        #else
            //#include "rtl.h"
        #endif

        #include "wolfssl_MDK_ARM.h"
#endif

#include <cyassl/ssl.h>
//#include <cyassl/test.h>

#ifndef NO_MAIN_DRIVER
    #define ECHO_OUT
#endif

//#include "examples/echoserver/echoserver.h"

#define SVR_COMMAND_SIZE 256

/* so overall tests can pull in test function */
//#ifndef NO_MAIN_DRIVER
//#define esHANDLE_CLIENT_TASK_STACK_SIZE			( 13900 )
#define esHANDLE_CLIENT_TASK_STACK_SIZE			( 2 * 13900 )
#define esHANDLE_CLIENT_TASK_PRIORITY			( tskIDLE_PRIORITY + 2 )

//#define esSERVER_TASK_STACK_SIZE 				( 3600 )
#define esSERVER_TASK_STACK_SIZE 				( 2 * 3600 )
#define esSERVER_TASK_PRIORITY					( tskIDLE_PRIORITY + 1 )

//2015-10-16 16:19:07.624 192.168.2.116    734.687.234 [SvrWork   ] Client_266      1     2755   632206     388  52.6 %
//2015-10-16 16:19:07.657 192.168.2.116    734.687.330 [SvrWork   ] EchoSrv         1     1147      178       3   0.0 %

static int wolfSSLPort = 11111;

#define svrCert    "/certs/server-cert.pem"
#define svrKey     "/certs/server-key.pem"
#define dhParam    "/certs/dh2048.pem"

static INLINE unsigned int my_psk_server_cb(WOLFSSL* ssl, const char* identity,
        unsigned char* key, unsigned int key_max_len)
{
    (void)ssl;
    (void)key_max_len;

    /* identity is OpenSSL testing default for openssl s_client, keep same */
    if (strncmp(identity, "Client_identity", 15) != 0)
	{
        return 0;
	}
    /* test key in hex is 0x1a2b3c4d , in decimal 439,041,101 , we're using
       unsigned binary */
    key[0] = 26;
    key[1] = 43;
    key[2] = 60;
    key[3] = 77;

    return 4;   /* length of key in octets or 0 for error */
}

static void err_sys( const char *pcMessage )
{
	FreeRTOS_printf( ( "err_sys: %s\n", pcMessage ) );
}

static const TickType_t xSendTimeOut    = pdMS_TO_TICKS( 4000 );
static const TickType_t xReceiveTimeOut = pdMS_TO_TICKS( 60000 );

static WOLFSSL_CTX* ctx = NULL;
static int iShutdownRequired = pdFALSE;

static SemaphoreHandle_t xSSL_Semapore;

uint16_t usGetRemotePort( Socket_t xSocket )
{
uint16_t usPort;
	if( xSocket != 0 )
	{
	struct freertos_sockaddr xSockaddr;

		FreeRTOS_GetRemoteAddress( xSocket, &xSockaddr );
		usPort = FreeRTOS_ntohs( xSockaddr.sin_port );
	}
	else
	{
		usPort = 0;
	}

	return usPort;
}

Socket_t xCurrentSocket;

void vHandleButtonClick( BaseType_t xButton, BaseType_t *pxHigherPriorityTaskWoken )
{
	/* This function will be called from a GPIO interrupt. */
	if( xCurrentSocket != NULL )
	{
		#if( ipconfigSUPPORT_SIGNALS != 0 )
		{
			FreeRTOS_SignalSocketFromISR( xCurrentSocket, pxHigherPriorityTaskWoken );
		}
		#endif
	}
}

//extern void *cur_ssl;
void prvHandleClient( void *pvParameter )
{
static int connectionNumber = 0;
Socket_t xClientSocket = ( Socket_t )pvParameter;
WOLFSSL* ssl = NULL;
int	echoSz = 0;
int ssl_err;
char command[SVR_COMMAND_SIZE+1];
char pcMyId[ 32 ];
int firstRead = pdTRUE, gotFirstG = pdFALSE;

	/* This task is going to use floating point operations.  Therefore it calls
    portTASK_USES_FLOATING_POINT() once on task entry, before entering the loop
    that implements its functionality.  From this point on the task has a floating
    point context. */
	#if( configUSE_TASK_FPU_SUPPORT == 1 )
	{
		portTASK_USES_FLOATING_POINT();
	}
	#endif /* configUSE_TASK_FPU_SUPPORT == 1 */


	snprintf( pcMyId, sizeof( pcMyId ), "%d: port %u", ++connectionNumber, usGetRemotePort( xClientSocket ) );

	FreeRTOS_printf( ( "%s: prvHandleClient started\n", pcMyId ) );
	xSemaphoreTake( xSSL_Semapore, pdMS_TO_TICKS( 60000 ) );
FreeRTOS_printf( ( "%s: Semaphore ENTER\n", pcMyId ) );
	do
	{
		if( FreeRTOS_issocketconnected( xClientSocket ) <= 0 )
		{
			FreeRTOS_printf( ( "%s: client has gone while waiting\n", pcMyId ) );
			FreeRTOS_closesocket( xClientSocket );
			xClientSocket = NULL;
			break;
		}
		FreeRTOS_printf( ( "%s: accept succeeded\n", pcMyId ) );

		ssl = wolfSSL_new(ctx);
		if( ssl == NULL )
		{
			FreeRTOS_printf( ( "%s: SSL_new failed\n", pcMyId ) );
			FreeRTOS_closesocket( xClientSocket );
			xClientSocket = NULL;
			break;
		}
		wolfSSL_set_fd( ssl, ( int ) xClientSocket );

		#ifdef WOLFSSL_DTLS
		{
			wolfSSL_dtls_set_peer(ssl, &client_address, client_len);
		}
		#endif

		#if !defined(NO_FILESYSTEM) && !defined(NO_DH) && !defined(NO_ASN)
		{
			wolfSSL_SetTmpDH_file(ssl, dhParam, SSL_FILETYPE_PEM);
		}
		#elif !defined(NO_DH)
		{
			SetDH(ssl);  /* will repick suites with DHE, higher than PSK */
		}
		#endif

		if( wolfSSL_accept( ssl ) != SSL_SUCCESS )
		{
			FreeRTOS_printf( ( "%s: wolfSSL_accept failed\n", pcMyId ) );
			wolfSSL_free( ssl );
			FreeRTOS_closesocket( xClientSocket );
			xClientSocket = NULL;
			ssl = NULL;
			break;
		}
	#if defined(PEER_INFO)
		showPeer(ssl);
	#endif
	} while( 0 );

FreeRTOS_printf( ( "%s: Semaphore EXIT\n", pcMyId ) );
	xSemaphoreGive( xSSL_Semapore );

	FreeRTOS_printf( ( "%s: prepared %u\n", pcMyId, usGetRemotePort( xClientSocket ) ) );

	if( ( xClientSocket != NULL ) && ( ssl != NULL ) )
	{
		xCurrentSocket = xClientSocket;
	}

	while( ( xClientSocket != NULL ) && ( ssl != NULL ) )
	{
		{
			int ret;
			ret = wolfSSL_get_error( ssl, 0 );
			if( ( ret != 0 ) && ( ret != SSL_ERROR_WANT_READ ) && ( ret != SSL_ERROR_ZERO_RETURN ) )
			{
				FreeRTOS_printf( ( "%s: prvHandleClient: error %d\n", pcMyId, ret ) );
				break;
			}
		}
		if( FreeRTOS_issocketconnected( xClientSocket ) == pdFALSE )
		{
			FreeRTOS_printf( ( "%s: prvHandleClient: socket %d disconnected\n", pcMyId, usGetRemotePort( xClientSocket ) ) );
			break;
		}
		echoSz = wolfSSL_read(ssl, command, sizeof(command)-1);
		if( echoSz <= 0 )
		{
			ssl_err = wolfSSL_get_error( ssl, 0 );
			if( ssl_err == SSL_ERROR_WANT_READ )
			{
			char message[16];
			int iLength;
			int rc;
				iLength = snprintf( message, sizeof message, "Button 1" );
				FreeRTOS_printf( ( "%s: Got interrupted\n", pcMyId ) );
				rc = wolfSSL_write( ssl, message, iLength );
				if( rc != iLength )
				{
					FreeRTOS_printf( ( "%s: wolfSSL_write = %d\n", pcMyId, rc ) );
				}
				continue;
			}
			if( echoSz < 0 )
			{
				FreeRTOS_printf( ( "%s: wolfSSL_read = %d\n", pcMyId, echoSz ) );
				break;
			}
		}
		if( firstRead != pdFALSE )
		{
			firstRead = pdFALSE;  /* browser may send 1 byte 'G' to start */
			if (echoSz == 1 && command[0] == 'G') {
				gotFirstG = pdTRUE;
				continue;
			}
		}
		else if( ( gotFirstG != pdFALSE ) && ( strncmp( command, "ET /", 4 ) == 0 ) )
		{
			strncpy(command, "GET", 4);
			/* fall through to normal GET */
		}
	   
		if ( strncmp( command, "quit", 4 ) == 0 )
		{
			printf("client sent quit command: shutting down!\n");
			iShutdownRequired = pdTRUE;
			break;
		}
		if ( strncmp( command, "break", 5 ) == 0 )
		{
			printf("client sent break command: closing session!\n");
			break;
		}
#ifdef PRINT_SESSION_STATS
		if ( strncmp(command, "printstats", 10 ) == 0 )
		{
			wolfSSL_PrintSessionStats();
			break;
		}
#endif
		if ( strncmp(command, "GET", 3) == 0) {
			char type[]   = "HTTP/1.0 200 ok\r\nContent-type:"
							" text/html\r\n\r\n";
			char header[] = "<html><body BGCOLOR=\"#ffffff\">\n<pre>\n";
			char body[]   = "greetings from wolfSSL\n";
			char footer[] = "</body></html>\r\n\r\n";
		
			strncpy(command, type, sizeof(type));
			echoSz = sizeof(type) - 1;

			strncpy(&command[echoSz], header, sizeof(header));
			echoSz += (int)sizeof(header) - 1;
			strncpy(&command[echoSz], body, sizeof(body));
			echoSz += (int)sizeof(body) - 1;
			strncpy(&command[echoSz], footer, sizeof(footer));
			echoSz += (int)sizeof(footer);

			if (wolfSSL_write(ssl, command, echoSz) != echoSz)
				err_sys("SSL_write failed");
			break;
		}
		//FreeRTOS_printf( ( "%s: Received %d plain bytes\n", pcMyId, echoSz ) );
		command[echoSz] = 0;

		#ifdef ECHO_OUT
			fputs(command, fout);
		#endif

		if( wolfSSL_write( ssl, command, echoSz ) != echoSz )
		{
			err_sys("SSL_write failed");
			break;
		}
	}
	xCurrentSocket = NULL;
	if( ssl != NULL )
	{
		ssl_err = wolfSSL_get_error(ssl, 0);
		FreeRTOS_printf( ( "%s: wolfSSL_read return %d error %d\n", pcMyId, echoSz, ssl_err ) );
		if( ( ssl_err == 0 ) && ( echoSz >= 0 ) && ( xClientSocket != NULL ) )
		{
			wolfSSL_shutdown(ssl);
		}
//		cur_ssl = (void*)ssl;
		wolfSSL_free(ssl);
		ssl = NULL;
	}
	if( xClientSocket != NULL )
	{
//eventLogAdd("close()");
		FreeRTOS_closesocket( xClientSocket );
	}
	vTaskDelete( xTaskGetCurrentTaskHandle() );
}

void vWolfEchoServer( void *pvParameter )
{
Socket_t       xServerSocket = NULL;
WOLFSSL_METHOD* method = 0;

#ifdef WOLFSSL_DTLS
	int    doDTLS = 0;
#endif
int    doPSK = 0;
int	   iHasError = 0;
struct freertos_sockaddr xAddress;
WinProperties_t xWinProps;

	#if( configUSE_TASK_FPU_SUPPORT == 1 )
	{
		portTASK_USES_FLOATING_POINT();
	}
	#endif /* configUSE_TASK_FPU_SUPPORT == 1 */

#ifdef WOLFSSL_DTLS
    doDTLS  = 1;
#endif

#ifdef CYASSL_LEANPSK
    doPSK = 1;
#endif

#if defined(NO_RSA) && !defined(HAVE_ECC)
    doPSK = 1;
#endif

    #if defined(NO_MAIN_DRIVER) && !defined(USE_WINDOWS_API) && \
        !defined(WOLFSSL_SNIFFER) && !defined(WOLFSSL_MDK_SHELL) && \
        !defined(WOLFSSL_TIRTOS)
        //wolfSSLPort = 0;
    #endif
    #if defined(USE_ANY_ADDR)
        useAnyAddr = 1;
    #endif

#ifdef WOLFSSL_TIRTOS
    fdOpenSession(Task_self());
#endif

//    tcp_listen(&xServerSocket, &port, useAnyAddr, doDTLS);

	xServerSocket = FreeRTOS_socket( FREERTOS_AF_INET, FREERTOS_SOCK_STREAM, FREERTOS_IPPROTO_TCP );

	if( xServerSocket == NULL )
	{
		return;
	}
	FreeRTOS_printf( ( "vWolfEchoServer: socket %p created\n", xServerSocket ) );

	xAddress.sin_addr = FreeRTOS_GetIPAddress(); // Single NIC, currently not used
	xAddress.sin_port = FreeRTOS_htons( wolfSSLPort );

	FreeRTOS_bind( xServerSocket, &xAddress, sizeof( xAddress ) );
	FreeRTOS_listen( xServerSocket, 8 );

	FreeRTOS_setsockopt( xServerSocket, 0, FREERTOS_SO_RCVTIMEO, ( void * ) &xReceiveTimeOut, sizeof( BaseType_t ) );
	FreeRTOS_setsockopt( xServerSocket, 0, FREERTOS_SO_SNDTIMEO, ( void * ) &xSendTimeOut, sizeof( BaseType_t ) );

	/* Fill in the buffer and window sizes that will be used by the socket. */
	xWinProps.lTxBufSize = 6 * ipconfigTCP_MSS;
	xWinProps.lTxWinSize = 3;
	xWinProps.lRxBufSize = 6 * ipconfigTCP_MSS;
	xWinProps.lRxWinSize = 3;

	/* Set the window and buffer sizes. */
	FreeRTOS_setsockopt( xServerSocket, 0, FREERTOS_SO_WIN_PROPERTIES, ( void * ) &xWinProps,	sizeof( xWinProps ) );

#if defined(WOLFSSL_DTLS)
    method  = wolfDTLSv1_2_server_method();
#elif  !defined(NO_TLS)
    method = wolfSSLv23_server_method();
#elif defined(WOLFSSL_ALLOW_SSLV3)
    method = wolfSSLv3_server_method();
#else
    #error "no valid server method built in"
#endif
    ctx    = wolfSSL_CTX_new(method);
    /* wolfSSL_CTX_set_session_cache_mode(ctx, SSL_SESS_CACHE_OFF); */

	configASSERT( ctx != NULL );

#if defined(OPENSSL_EXTRA) || defined(HAVE_WEBSERVER)
    wolfSSL_CTX_set_default_passwd_cb(ctx, PasswordCallBack);
#endif

#if defined(HAVE_SESSION_TICKET) && defined(HAVE_CHACHA) && \
                                    defined(HAVE_POLY1305)
    if( TicketInit() != 0 )
	{
        err_sys("unable to setup Session Ticket Key context");
	}
    wolfSSL_CTX_set_TicketEncCb(ctx, myTicketEncCb);
#endif
	xSSL_Semapore = xSemaphoreCreateBinary();
	configASSERT( xSSL_Semapore != NULL );

	xSemaphoreGive( xSSL_Semapore  );

#ifndef NO_FILESYSTEM
    if (doPSK == 0) do
	{
    #ifdef HAVE_NTRU
        /* ntru */
        if( wolfSSL_CTX_use_certificate_file(ctx, ntruCert, SSL_FILETYPE_PEM) != SSL_SUCCESS )
		{
            err_sys("can't load ntru cert file, Please run from wolfSSL home dir");
			iHasError = pdTRUE;
			break;
		}
        if( wolfSSL_CTX_use_NTRUPrivateKey_file(ctx, ntruKey) != SSL_SUCCESS )
		{
            err_sys("can't load ntru key file, Please run from wolfSSL home dir");
			iHasError = pdTRUE;
			break;
		}
    #elif defined(HAVE_ECC) && !defined(WOLFSSL_SNIFFER)
        /* ecc */
        if( wolfSSL_CTX_use_certificate_file(ctx, eccCert, SSL_FILETYPE_PEM) != SSL_SUCCESS )
		{
            err_sys("can't load server cert file, Please run from wolfSSL home dir");
			iHasError = pdTRUE;
			break;
		}

        if( wolfSSL_CTX_use_PrivateKey_file(ctx, eccKey, SSL_FILETYPE_PEM) != SSL_SUCCESS )
            err_sys("can't load server key file, Please run from wolfSSL home dir");
			iHasError = pdTRUE;
			break;
		}
    #elif defined(NO_CERTS)
        /* do nothing, just don't load cert files */
    #else
        /* normal */
        if( wolfSSL_CTX_use_certificate_file(ctx, svrCert, SSL_FILETYPE_PEM) != SSL_SUCCESS )
		{
            err_sys("can't load server cert file, Please run from wolfSSL home dir");
			iHasError = pdTRUE;
			break;
		}
        if( wolfSSL_CTX_use_PrivateKey_file(ctx, svrKey, SSL_FILETYPE_PEM) != SSL_SUCCESS )
		{
            err_sys("can't load server key file, Please run from wolfSSL home dir");
			iHasError = pdTRUE;
			break;
		}
    #endif
    } while ( 0 ); /* doPSK */
#elif !defined( NO_CERTS )
    if( doPSK == 0 )
	{
        load_buffer(ctx, svrCert, CYASSL_CERT);
        load_buffer(ctx, svrKey,  CYASSL_KEY);
    }
#endif

#if defined(WOLFSSL_SNIFFER)
    /* don't use EDH, can't sniff tmp keys */
    wolfSSL_CTX_set_cipher_list( ctx, "AES256-SHA" );
#endif

    if( doPSK != 0 )
	{
#ifndef NO_PSK
        const char *defaultCipherList;

        wolfSSL_CTX_set_psk_server_callback(ctx, my_psk_server_cb);
        wolfSSL_CTX_use_psk_identity_hint(ctx, "cyassl server");
        #ifdef HAVE_NULL_CIPHER
            defaultCipherList = "PSK-NULL-SHA256";
        #elif defined(HAVE_AESGCM) && !defined(NO_DH)
            defaultCipherList = "DHE-PSK-AES128-GCM-SHA256";
        #else
            defaultCipherList = "PSK-AES128-CBC-SHA256";
        #endif
        if (wolfSSL_CTX_set_cipher_list(ctx, defaultCipherList) != SSL_SUCCESS)
            err_sys("server can't set cipher list 2");
#endif
    }

	FreeRTOS_printf( ( "vWolfEchoServer: all prepared with error = %d socket %p\n", iHasError, xServerSocket ) );

    while( ( iHasError == pdFALSE ) && ( iShutdownRequired == pdFALSE ) )
	{
	static int iClientNr = 0;
	char pcClientName[ 32 ];
	Socket_t xClientSocket;
	SOCKADDR_IN_T client_address;
	socklen_t     client_len = sizeof client_address;

        xClientSocket = FreeRTOS_accept( xServerSocket, &client_address, &client_len );

        if( xClientSocket != NULL )
		{
		BaseType_t xResult;
//eventLogAdd("accept() ok");
			FreeRTOS_setsockopt( xClientSocket, 0, FREERTOS_SO_RCVTIMEO, ( void * ) &xReceiveTimeOut, sizeof( BaseType_t ) );
			FreeRTOS_setsockopt( xClientSocket, 0, FREERTOS_SO_SNDTIMEO, ( void * ) &xSendTimeOut, sizeof( BaseType_t ) );

			snprintf( pcClientName, sizeof( pcClientName ), "Client_%d", iClientNr++ );
			xResult = xTaskCreate( prvHandleClient, pcClientName, esHANDLE_CLIENT_TASK_STACK_SIZE, ( void * ) xClientSocket, esHANDLE_CLIENT_TASK_PRIORITY, NULL );
			if( xResult != pdPASS )
			{
				FreeRTOS_printf( ( "xTaskCreate failed (%p)\n", xClientSocket ) );
				FreeRTOS_closesocket( xClientSocket );
				xClientSocket = NULL;
			}
			else
			{
				FreeRTOS_printf( ( "xTaskCreate OK (%p)\n", xClientSocket ) );
			}
		}
    }

    FreeRTOS_closesocket( xServerSocket );
    wolfSSL_CTX_free(ctx);

#ifdef ECHO_OUT
    if( outCreated )
	{
        fclose(fout);
	}
#endif

#if defined(NO_MAIN_DRIVER) && defined(HAVE_ECC) && defined(FP_ECC) \
                            && defined(HAVE_THREAD_LS)
    ecc_fp_free();  /* free per thread cache */
#endif

#ifdef WOLFSSL_TIRTOS
    fdCloseSession(Task_self());
#endif

#if defined(HAVE_SESSION_TICKET) && defined(HAVE_CHACHA) && \
                                    defined(HAVE_POLY1305)
    TicketCleanup();
#endif

	vTaskDelete( xTaskGetCurrentTaskHandle() );
}


static TaskHandle_t xEchoServerTaskHandle = NULL;

int vStartWolfServerTask()
{
#ifdef HAVE_CAVIUM
	int ret = OpenNitroxDevice(CAVIUM_DIRECT, CAVIUM_DEV_ID);
	if( ret != 0 )
	{
		err_sys("vStartWolfServerTask: Cavium OpenNitroxDevice failed");
		return -1;
	}
#endif /* HAVE_CAVIUM */

	wolfSSL_Init();
#if defined(DEBUG_WOLFSSL) && !defined(CYASSL_MDK_SHELL)
	wolfSSL_Debugging_ON();
#endif
	xTaskCreate( vWolfEchoServer, "EchoSrv", esSERVER_TASK_STACK_SIZE, NULL, esSERVER_TASK_PRIORITY, &xEchoServerTaskHandle );
	FreeRTOS_printf( ( "vWolfEchoServer started\n" ) );
	return 0;
}
