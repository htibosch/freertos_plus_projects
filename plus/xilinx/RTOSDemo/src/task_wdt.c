/*
 * task_wdt.c
 *
 * Some sample code to create a Watchdog Timer per task.
 */

/* Standard includes. */
#include <stdio.h>
#include <string.h>
#include <time.h>

/* FreeRTOS includes. */
#include <FreeRTOS.h>
#include "task.h"
#include "timers.h"
#include "queue.h"
#include "semphr.h"

/*
 * The thread local pointers form an array.
 * Whe you also use FreeRTOF+FAT, 'configNUM_THREAD_LOCAL_STORAGE_POINTERS' will
 * be defined as 3 or more.
 * The following indexes will be in use already:
 */

#define stdioERRNO_THREAD_LOCAL_OFFSET 		( ffconfigCWD_THREAD_LOCAL_INDEX + 0 )
#define stdioCWD_THREAD_LOCAL_OFFSET		( ffconfigCWD_THREAD_LOCAL_INDEX + 1 )
#define stdioFF_ERROR_THREAD_LOCAL_OFFSET	( ffconfigCWD_THREAD_LOCAL_INDEX + 2 )

/*
 * Depending on whether you already use Free+FAT, define the following
 * in your "FreeRTOSConfig.h" :
 */

/* In case you also use +FAT */
	#define configNUM_THREAD_LOCAL_STORAGE_POINTERS     4
	#define LOCAL_STORAGE_WDT_INDEX                     3

/* In case you do not use +FAT */
//	#define configNUM_THREAD_LOCAL_STORAGE_POINTERS     1
//	#define LOCAL_STORAGE_WDT_INDEX                     0

struct xTaskWDT;

struct xTaskWDT
{
	/* The owning handle. */
	TaskHandle_t *xHandle;
	/* The previous struct of this type. */
	struct xTaskWDT *xPrevious;
	/* Last value of timer-tick. */
	TickType_t xLastTimeTick;
	/* Number of times the task called task_touch_wdt(). */
	uint32_t ulCycleCount;
	/* Add more data if you like. */
};

typedef struct xTaskWDT TaskWDT_t;

static TaskWDT_t *xLastCreated = NULL;

static TaskWDT_t *wdt_get_struct()
{
TaskWDT_t *pxMyPointer;
TaskHandle_t xMyHandle = xTaskGetCurrentTaskHandle();

	pxMyPointer = ( TaskWDT_t *)pvTaskGetThreadLocalStoragePointer( xMyHandle, LOCAL_STORAGE_WDT_INDEX );
	if( pxMyPointer == NULL )
	{
		pxMyPointer = ( TaskWDT_t * )pvPortMalloc( sizeof( TaskWDT_t ) );
		if( pxMyPointer != NULL )
		{
			memset( pxMyPointer, '\0', sizeof *pxMyPointer );
			pxMyPointer->xPrevious = xLastCreated;
			pxMyPointer->xHandle = xMyHandle;
			xLastCreated = pxMyPointer;
			vTaskSetThreadLocalStoragePointer( xMyHandle, LOCAL_STORAGE_WDT_INDEX, pxMyPointer );
		}
	}
	return ( TaskWDT_t *)pxMyPointer;
}

void task_wdt_check()
{
TaskWDT_t *pxTaskPointer;
BaseType_t xTaskCount = 0;
TickType_t xNow, xAge;
TickType_t xMaxTicks = pdMS_TO_TICKS( 10000 ); /* Use a maximum time of 10-second. */

	xNow = xTaskGetTickCount();
	/* Stop the scheduler for a moment to make sure that
	the linked list of 'TaskWDT_t' will not change.
	Do not use long operations. */
	vTaskSuspendAll();
	{
		pxTaskPointer = xLastCreated;
		while( pxTaskPointer != NULL )
		{
			xTaskCount++;

			/* Check the contents of 'pxTaskPointer'. */
			xAge = xNow - pxTaskPointer->xLastTimeTick;
			if( xAge > xMaxTicks )
			{
				/* This task has not reported itself for 10 seconds! */
			}
			/* Jump to the previous task. */
			pxTaskPointer = pxTaskPointer->xPrevious;
		}
	}
	xTaskResumeAll();
}

void task_touch_wdt()
{
TaskWDT_t *xWDTStruct = wdt_get_struct();

	if( xWDTStruct != NULL )
	{
		xWDTStruct->xLastTimeTick = xTaskGetTickCount();
		xWDTStruct->ulCycleCount++;
	}
}


/* As an example, let every task report it self regularly. */

void pvMyTask( void *pvParameter )
{
	for( ;; )
	{
		/* ... */
		task_touch_wdt();
	}
}
