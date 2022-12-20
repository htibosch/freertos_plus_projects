/*
 * telnet.h
 */


#ifndef TELNET_H_

#define TELNET_H_

/* *INDENT-OFF* */
#ifdef __cplusplus
    extern "C" {
#endif
/* *INDENT-ON* */

#ifndef TELNET_PORT_NUMBER
    /* Use the telnet port number 23 by default. */
    #define TELNET_PORT_NUMBER    23
#endif

#ifndef TELNET_MAX_CLIENT_COUNT
    /* Accept at most 4 clients simultaneously. */
    #define TELNET_MAX_CLIENT_COUNT    4
#endif

typedef struct xTelnetClient
{
    Socket_t xSocket;
    struct freertos_sockaddr xAddress;
    struct xTelnetClient * pxNext;
} TelnetClient_t;

typedef struct
{
    Socket_t xParentSocket;
    /* A linked list of clients. */
    TelnetClient_t * xClients;
    /* The port number ( stored host-endian ). */
    BaseType_t xPortNr;
} Telnet_t;

/* Create the server socket. */
BaseType_t xTelnetCreate( Telnet_t * pxTelnet,
                          BaseType_t xPortNr );

/* Send data to a connected client with a certain address.
 * Use NULL to broadcast a message to all connected clients. */
BaseType_t xTelnetSend( Telnet_t * pxTelnet,
                        struct freertos_sockaddr * pxAddress,
                        const char * pcBuffer,
                        BaseType_t xLength );

/* Receive the next packet. */
BaseType_t xTelnetRecv( Telnet_t * pxTelnet,
                        struct freertos_sockaddr * pxAddress,
                        char * pcBuffer,
                        BaseType_t xMaxLength );

/* This semaphore will be connected to all sockets created for telnet. */
#if ( ipconfigSOCKET_HAS_USER_SEMAPHORE == 1 )
    extern SemaphoreHandle_t xTelnetSemaphore;
#endif

/* *INDENT-OFF* */
#ifdef __cplusplus
    }
#endif
/* *INDENT-ON* */

#endif /* ifndef TELNET_H_ */
