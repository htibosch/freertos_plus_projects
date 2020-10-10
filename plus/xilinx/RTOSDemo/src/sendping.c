/*
    FreeRTOS V9.0.0 - Copyright (C) 2016 Real Time Engineers Ltd.
*/

/* Standard includes. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "queue.h"

/* FreeRTOS+UDP includes. */
#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"


#if ipconfigSUPPORT_OUTGOING_PINGS == 1

/* 100ms wait for ping buffer to be written and send out */
#define ping_READYBLOCK_TIMEOUT		( ( TickType_t ) 100 / portTICK_PERIOD_MS )
/* 2500ms wait for ping reply from remote */
#define ping_REPLYWAIT_TIMEOUT		( ( TickType_t ) 2500 / portTICK_PERIOD_MS )
#define ping_BYTE_SIZE				( 32 )
#define pingPINGREPLY_QUEUE_LENGTH	( 4 )


static void prvPingClientTask( void *pvParameters );

typedef struct pingEvent {
	ePingReplyStatus_t eStatus;
	uint16_t usIdentifier;
} pingEventQueueItem;

QueueHandle_t xPingReplyQueue = NULL;
static const char pingSuccess[] = "Ping reply received\r\n";
static const char pingInvalidChecksum[] = "Ping reply received with invalid checksum\r\n";
static const char pingInvalidData[] = "Ping reply received with invalid data\r\n";
static const char pingNoResponse[] = "No response received\r\n";

/* Called automatically when a reply to an outgoing ping is received. */
void vApplicationPingReplyHook( ePingReplyStatus_t eStatus, uint16_t usIdentifier )
{
	pingEventQueueItem item;
	
	xil_printf ("!!! answered ping, status:%lu (0==ok) id:%u\n", eStatus, usIdentifier);

	item.eStatus = eStatus;
	item.usIdentifier = usIdentifier;
	xQueueSend( xPingReplyQueue, ( void * ) &item, 10 / portTICK_PERIOD_MS );
}

void vStartSendPingTask( uint16_t usTaskStackSize, UBaseType_t uxTaskPriority, const uint8_t ucIPAddress[ ipIP_ADDRESS_LENGTH_BYTES ] )
{
	uint32_t ulIPAddress;

	if( xPingReplyQueue == NULL )
	{
		xPingReplyQueue = xQueueCreate( ( UBaseType_t ) pingPINGREPLY_QUEUE_LENGTH, ( UBaseType_t )sizeof( pingEventQueueItem ) );
		configASSERT( xPingReplyQueue );

		if( xPingReplyQueue != NULL )
		{
			/* convert destination IP address from four separate numerical octets to 32-bit format. */
			ulIPAddress = FreeRTOS_inet_addr_quick( ucIPAddress[ 0 ], ucIPAddress[ 1 ], ucIPAddress[ 2 ], ucIPAddress[ 3 ] );

			/* Create the ping task */
			xTaskCreate( 	prvPingClientTask,						/* The function that implements the task. */
							"Ping",									/* Just a text name for the task to aid debugging. */
							usTaskStackSize,						/* The stack size is defined in FreeRTOSIPConfig.h. */
							(void *) ulIPAddress,					/* The task parameter */
							uxTaskPriority,							/* The priority assigned to the task is defined in FreeRTOSConfig.h. */
							NULL );									/* The task handle is not used. */
		}
	}
}

static void prvPingClientTask( void *pvParameters )
{
	uint16_t Result;
	pingEventQueueItem item;

	for(;;)
	{
		vTaskDelay( 1000 / portTICK_PERIOD_MS );

		Result = FreeRTOS_SendPingRequest( ( uint32_t ) pvParameters, ping_BYTE_SIZE, ping_READYBLOCK_TIMEOUT );
		if( Result == pdFAIL )
		{
			xil_printf("Unable to send ping request\r\n"); //!!!
		}
		else if( xPingReplyQueue != NULL )
		{
			/* Wait for a reply */
			if( xQueueReceive(  xPingReplyQueue,
								&item,
								ping_REPLYWAIT_TIMEOUT ) == pdPASS )
			{
				switch( item.eStatus )
				{
					case eSuccess :
						FreeRTOS_printf( ( pingSuccess ) );
					break;

					case eInvalidChecksum :
						FreeRTOS_printf( ( pingInvalidChecksum ) );
					break;

					case eInvalidData :
						FreeRTOS_printf( ( pingInvalidData ) );
					break;

					default :
					break;
				}
			}
			else
			{
				xil_printf("No Rely, timeout\r\n"); //!!!
				FreeRTOS_printf( ( pingNoResponse ) );
			}
		}
	}
}
#else
void vStartSendPingTask( uint16_t usTaskStackSize, UBaseType_t uxTaskPriority, const uint8_t ucIPAddress[ ipIP_ADDRESS_LENGTH_BYTES ] );
{

}

#endif
