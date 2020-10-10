/*
 * udp_self_test.c
 * An example of using the loop-back mechanism.
 * Tested in the Windows Simulator project
 * Every second, it will exchange packets between UDP sockets.
 */

#define usTEST_UDP_PORT_1		20006
#define usTEST_UDP_PORT_2		20007

static void loopBackTestTask( void *pvParameter)
{
	Socket_t xUDPSocket1 = FreeRTOS_socket( FREERTOS_AF_INET, FREERTOS_SOCK_DGRAM, FREERTOS_IPPROTO_UDP );
	Socket_t xUDPSocket2 = FreeRTOS_socket( FREERTOS_AF_INET, FREERTOS_SOCK_DGRAM, FREERTOS_IPPROTO_UDP );
	BaseType_t xValid = xUDPSocket1 != NULL && xUDPSocket1 != FREERTOS_INVALID_SOCKET && xUDPSocket2 != NULL && xUDPSocket2 != FREERTOS_INVALID_SOCKET;
	uint32_t ulIPAddress;

    FreeRTOS_GetAddressConfiguration( &ulIPAddress, NULL, NULL, NULL );
	if( xValid )
	{
		struct freertos_sockaddr xAddress;
		BaseType_t xReceiveTimeOut = 0, xSendTimeOut = 1000;

		( void ) memset ( &xAddress, '\0', sizeof( xAddress ) );
		xAddress.sin_addr = ulIPAddress;	/* Is in network byte order. */
		xAddress.sin_port = FreeRTOS_ntohs( usTEST_UDP_PORT_1 );
		FreeRTOS_bind( xUDPSocket1, &xAddress, sizeof( xAddress ) );

		FreeRTOS_setsockopt( xUDPSocket1, 0, FREERTOS_SO_RCVTIMEO, &xReceiveTimeOut, sizeof( xReceiveTimeOut ) );
		FreeRTOS_setsockopt( xUDPSocket1, 0, FREERTOS_SO_SNDTIMEO, &xSendTimeOut, sizeof( xSendTimeOut ) );

		xAddress.sin_addr = ulIPAddress;	/* Is in network byte order. */
		xAddress.sin_port = FreeRTOS_ntohs( usTEST_UDP_PORT_2 );
		FreeRTOS_bind( xUDPSocket2, &xAddress, sizeof( xAddress ) );

		FreeRTOS_setsockopt( xUDPSocket2, 0, FREERTOS_SO_RCVTIMEO, &xReceiveTimeOut, sizeof( xReceiveTimeOut ) );
		FreeRTOS_setsockopt( xUDPSocket2, 0, FREERTOS_SO_SNDTIMEO, &xSendTimeOut, sizeof( xSendTimeOut ) );
	}
	for( ;; )
	{
		vTaskDelay( 1000 );
		if( xValid )
		{
			struct freertos_sockaddr xDestinationAddress;
			struct freertos_sockaddr xFromAddress;
			const char pcSendString1[] = "Hello world 1";
			const char pcSendString2[] = "Hello world other 2";
			char buffer[128];
			BaseType_t xLength1, xLength2;
			socklen_t xAddressLength;

			xDestinationAddress.sin_addr = ulIPAddress;	/* Is in network byte order. */
			xDestinationAddress.sin_port = FreeRTOS_ntohs( usTEST_UDP_PORT_2 );
			FreeRTOS_sendto( xUDPSocket1, (const void *)pcSendString2, sizeof pcSendString2, 0, &xDestinationAddress, sizeof( xDestinationAddress ) );

			xDestinationAddress.sin_addr = ulIPAddress;	/* Is in network byte order. */
			xDestinationAddress.sin_port = FreeRTOS_ntohs( usTEST_UDP_PORT_1 );
			FreeRTOS_sendto( xUDPSocket2, (const void *)pcSendString1, sizeof pcSendString1, 0, &xDestinationAddress, sizeof( xDestinationAddress ) );
			
			xAddressLength = sizeof( xFromAddress );
			xLength1 = FreeRTOS_recvfrom( xUDPSocket1, (void *)buffer, sizeof( buffer ), 0, &xFromAddress, &xAddressLength );
			xLength2 = FreeRTOS_recvfrom( xUDPSocket2, (void *)buffer, sizeof( buffer ), 0, &xFromAddress, &xAddressLength );
			vLoggingPrintf( "Received %d and %d bytes\n", xLength1, xLength2 );
		}
	}
}
