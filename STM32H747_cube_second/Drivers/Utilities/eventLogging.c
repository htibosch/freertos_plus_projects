/*
 * eventlogging.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "FreeRTOS.h"
#include "task.h"

#include "eventlogging.h"

void vUDPLoggingFlush( void );

#if USE_CLOCK
#include "setclock.h"
#endif

#if USE_MYMALLOC
#	include "mymalloc.h"
#else
#	include "portable.h"
#endif

#if( __SAM4E16E__ )
void vUDPLoggingFlush( void );
#define	vFlushLogging	vUDPLoggingFlush
#endif

#if USE_LOG_EVENT

extern int lUDPLoggingPrintf( const char *pcFormatString, ... );

SEventLogging xEventLogs;

#if( STATIC_LOG_MEMORY != 0 )

SLogEvent xEvents[LOG_EVENT_COUNT];
const char *strings[LOG_EVENT_COUNT];

#endif

int iEventLogInit()
{
	if( xEventLogs.hasInit == pdFALSE )
	{
		xEventLogs.hasInit = pdTRUE;
#if USE_MYMALLOC
		xEventLogs.events = (SLogEvent *)myMalloc (sizeof xEventLogs.events[0] * LOG_EVENT_COUNT, pdFALSE);
#elif( STATIC_LOG_MEMORY != 0 )
		xEventLogs.events = xEvents;
		{
			int i;
			for (i = 0; i < LOG_EVENT_COUNT; i++) {
				strings[i] = xEventLogs.events[i].pcMessage;
			}
		}
#else
		xEventLogs.events = (SLogEvent *)pvPortMalloc (sizeof xEventLogs.events[0] * LOG_EVENT_COUNT);
#endif
		if( xEventLogs.events != NULL )
		{
			memset (xEventLogs.events, '\0', sizeof xEventLogs.events[0] * LOG_EVENT_COUNT);
			xEventLogs.initOk = pdTRUE;
		}
	}
	return xEventLogs.initOk;
}

int iEventLogClear ()
{
int rc;
	if (!iEventLogInit ()) {
		rc = 0;
	} else {
		rc = xEventLogs.writeIndex;
		xEventLogs.writeIndex = 0;
		xEventLogs.wrapped = pdFALSE;
		xEventLogs.onhold = pdFALSE;
	}
	return rc;
}

#include "hr_gettime.h"

uint64_t ullLastTime;

const char *eventLogLast()
{
	int writeIndex = xEventLogs.writeIndex;
	SLogEvent *pxEvent;
	if (writeIndex > 0) {
		writeIndex--;
	} else {
		writeIndex = LOG_EVENT_COUNT-1;
	}
	pxEvent = &xEventLogs.events[writeIndex];
	return pxEvent->pcMessage;
}

void eventLogAdd (const char *apFmt, ...)
{
int writeIndex;
SLogEvent *pxEvent;
va_list args;
uint64_t ullCurTime;
int len;
uint32_t ulDelta;

	if (!iEventLogInit () || xEventLogs.onhold || apFmt[0] == '\0')
		return;
	writeIndex = xEventLogs.writeIndex++;
	if (xEventLogs.writeIndex >= LOG_EVENT_COUNT) {
#if( EVENT_MAY_WRAP == 0 )
		xEventLogs.writeIndex--;
		return;
#else
		xEventLogs.writeIndex = 1;
		xEventLogs.wrapped++;
		writeIndex = 0;
#endif
	}
	pxEvent = &xEventLogs.events[writeIndex];

	ullCurTime = ullGetHighResolutionTime( );
	if( ullLastTime == 0 )
	{
		ulDelta = 0ul;
	}
	else
	{
		ulDelta = ( uint32_t ) ( ( ullCurTime - ullLastTime ) / 1000ull );
//		if( ulDelta >= 100000ul )
//		{
//			ulDelta = 99999ul;
//		}
	}
	( void ) ulDelta;
	ullLastTime = ullCurTime;
//	len = snprintf( pxEvent->pcMessage, sizeof pxEvent->pcMessage, "%5u ", ( unsigned )ulDelta );
	len = 0;

	va_start (args, apFmt);
	vsnprintf( pxEvent->pcMessage + len, sizeof pxEvent->pcMessage - len, apFmt, args );
	va_end (args);

	pxEvent->ullTimestamp = ullCurTime;
}

void eventFreeze()
{
	xEventLogs.onhold = pdTRUE;
}

void vFlushLogging();

void eventLogDump ()
{
char sec_str[17];
char ms_str[17];
int count;
int index;
int i;
uint64_t ullLastTime;

#if USE_CLOCK
	unsigned cpuTicksSec = clk_getFreq (&clk_cpu);
#elif defined(__AVR32__)
	unsigned cpuTicksSec = 48000000;
#else
	unsigned cpuTicksSec = 1000000;
#endif
unsigned cpuTicksMs = cpuTicksSec / 1000;

eventLogAdd ("now");
xEventLogs.onhold = 1;

count = xEventLogs.wrapped ? LOG_EVENT_COUNT : xEventLogs.writeIndex;
index = xEventLogs.wrapped ? xEventLogs.writeIndex : 0;
ullLastTime = xEventLogs.events[index].ullTimestamp;

	lUDPLoggingPrintf("Nr:    s   ms  us  %d events\n", count);

	for( i = 0; i < count; i++ )
	{
	SLogEvent *pxEvent;
	uint32_t delta, secs, msec;

		pxEvent = xEventLogs.events + index;
		if( pxEvent->ullTimestamp >= ullLastTime )
		{
			delta = ( unsigned ) ( pxEvent->ullTimestamp - ullLastTime );
		}
		else
		{
			delta = 0u;
		}

//		if (delta > 0xFFFF0000)
//			delta = ~0u - delta;

		secs = delta / cpuTicksSec;
		delta %= cpuTicksSec;
		msec = delta / cpuTicksMs;
		delta %= cpuTicksMs;
		//usec = (1000 * delta) / cpuTicksMs;

		if (secs > 60) {
//			secs = msec = 999;
//			usec = 9;
		}
		sprintf( ms_str, "%03lu.", msec );
		if( secs == 0u )
		{
			if( secs == 0)
			{
				sprintf( sec_str, "    " );
				sprintf( ms_str, "%3lu.", msec );
			}
			else
			{
				sprintf( sec_str, "%3lu.", secs );
			}
		}
		else
		{
			sprintf( sec_str, "%3lu.", secs );
		}
//		lUDPLoggingPrintf("%4d: %6u %s%s%03u %s\n", i, delta, sec_str, ms_str, usec, pxEvent->pcMessage);
		lUDPLoggingPrintf("%4d: %6u uS %s\n", i, delta, pxEvent->pcMessage);
		if( ++index >= LOG_EVENT_COUNT )
		{
			index = 0;
		}
		ullLastTime = pxEvent->ullTimestamp;
		//if ((i % 8) == 0)
		vUDPLoggingFlush();
	}
	vTaskDelay( 200 );
	iEventLogClear ();
}

#endif /* USE_LOG_EVENT */
