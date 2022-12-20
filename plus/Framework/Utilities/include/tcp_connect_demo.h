/*
 * tcp_connect_demo.c
 *
 * - create a TCP client
 * - connect to TCP server
 * - send data
 * - disconnect
 */

#ifndef TCP_CONNECT_DEMO_H

#define TCP_CONNECT_DEMO_H

/* The parameters 'ulIPAddress' and 'xPortNumber' should be
 * passed as network endian numbers, e.g.
 *
 * uint32_t ulAddress = FreeRTOS_inet_addr_quick( 192, 168, 2, 5 );
 * uint16_t usPort = FreeRTOS_htons( 32002 );
 * handle_user_test (ulAddress, usPort );
 */
void handle_user_test( uint32_t ulIPAddress,
                       BaseType_t xPortNumber );

#endif /* TCP_CONNECT_DEMO_H */
