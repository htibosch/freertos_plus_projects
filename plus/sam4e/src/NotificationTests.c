#include <stdlib.h>

/* Scheduler includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

//#include "DeviceRedirection.h"
//#include "DeviceSpecific.h"
//#include "TimebaseTasks.h"
//#include "ApplicationTask.h"
//#include "Display.h"

#define NOTIFICATION_TASK_PERIOD				( ( TickType_t ) 2000 / portTICK_PERIOD_MS )

static void prvTask1( void *pvParameters );
static void prvTask2( void *pvParameters );

/* Handles for the tasks create by main(). */
static TaskHandle_t xTask1 = NULL, xTask2 = NULL;


void NotificationTasks(void)
{
    /* Create two tasks that send notifications back and forth to each other. */
    xTaskCreate( prvTask1, "Task1", 4 * configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY+1, &xTask1 );
    xTaskCreate( prvTask2, "Task2", 4 * configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY+1, &xTask2 );    
    
    
}
int lUDPLoggingPrintf( const char *pcFormatString, ... );

static void prvTask1( void *pvParameters )
{
    /* Used to wake the task at the correct frequency. */
    TickType_t xLastExecutionTime; 
    
    /* Remove compiler warnings. */
	( void ) pvParameters;
 
    /* Initialise xLastExecutionTime so the first call to vTaskDelayUntil()
	works correctly. */
	xLastExecutionTime = xTaskGetTickCount();
    
    for( ;; )
    {
		lUDPLoggingPrintf("Task-1 Delay\n");
        /* Wait until it is time for the next cycle. */
		vTaskDelayUntil( &xLastExecutionTime, NOTIFICATION_TASK_PERIOD );
        
//        portENTER_CRITICAL();
//        CMP_ON = 1;
//        portEXIT_CRITICAL();
		lUDPLoggingPrintf("Task-1 Give\n");
        /* Send a notification to prvTask2(), bringing it out of the Blocked
        state. */
        xTaskNotifyGive( xTask2 );

		lUDPLoggingPrintf("Task-1 Take\n");
        /* Block to wait for prvTask2() to notify this task. */
        ulTaskNotifyTake( pdTRUE, portMAX_DELAY );
    }
}
/*-----------------------------------------------------------*/

static void prvTask2( void *pvParameters )
{
    /* Used to wake the task at the correct frequency. */
    TickType_t xLastExecutionTime; 
    
    /* Remove compiler warnings. */
	( void ) pvParameters;

	/* Initialise xLastExecutionTime so the first call to vTaskDelayUntil()
	works correctly. */
	xLastExecutionTime = xTaskGetTickCount();
    
    for( ;; )
    {
		lUDPLoggingPrintf("Task-2 Delay\n");
        /* Wait until it is time for the next cycle. */
		vTaskDelayUntil( &xLastExecutionTime, NOTIFICATION_TASK_PERIOD );
        
//        portENTER_CRITICAL();
//        CMP_ON = 0;
//        portEXIT_CRITICAL();
        
		lUDPLoggingPrintf("Task-2 Take\n");
        /* Block to wait for prvTask1() to notify this task. */
        ulTaskNotifyTake( pdTRUE, portMAX_DELAY );

		lUDPLoggingPrintf("Task-2 Give\n");
        /* Send a notification to prvTask1(), bringing it out of the Blocked
        state. */
        xTaskNotifyGive( xTask1 );
    }
}
