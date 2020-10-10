/*
    FreeRTOS V8.2.2 - Copyright (C) 2015 Real Time Engineers Ltd.
    All rights reserved
*/

#ifndef SINGLE_TASK_TCP_ECHO_CLIENTS_H
#define SINGLE_TASK_TCP_ECHO_CLIENTS_H

/* The number of instances of the echo client task to create. */
#define echoNUM_ECHO_CLIENTS		( 2 )

void wakeupEchoClient( int aindex, const char *pcHost );

/*
 * Create the TCP echo client tasks.  This is the version where an echo request
 * is made from the same task that listens for the echo reply.
 */
void vStartTCPEchoClientTasks_SingleTasks( uint16_t usTaskStackSize, UBaseType_t uxTaskPriority );
BaseType_t xAreSingleTaskTCPEchoClientsStillRunning( void );

#endif /* SINGLE_TASK_TCP_ECHO_CLIENTS_H */


