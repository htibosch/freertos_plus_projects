/* Standard includes. */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

/* FreeRTOS+CLI includes. */
#include "FreeRTOS_CLI.h"

/* FreeRTOS+TCP includes. */
#include "FreeRTOSIPConfig.h"
#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"
#include "FreeRTOS_IP_Private.h"
#include "NetworkBufferManagement.h"

#include "tiva_mac.h"

#if ( ipconfigEMAC_INCLUDE_CLI != 0 ) && ( ipconfigEMAC_USE_STAT != 0 )

	static BaseType_t prvDisplayEMACStats( char * pcWriteBuffer,
										   size_t xWriteBufferLen,
										   const char * pcCommandString );

/* Structure that defines the "ip-debug-stats" command line command. */
	static const CLI_Command_Definition_t xEmacStats =
	{
		"emac-stats",        /* The command string to type. */
		"\r\nemac-stats:\r\n Shows EMAC stat counters.\r\n",
		prvDisplayEMACStats, /* The function to run. */
		0                    /* No parameters are expected. */
	};

	static BaseType_t prvDisplayEMACStats( char * pcWriteBuffer,
										   size_t xWriteBufferLen,
										   const char * pcCommandString )
	{
		static BaseType_t xIndex = -1;
		BaseType_t xReturn;
		static const char * pszDesc[] =
		{
			"Bytes received",
			"Byte sent",
			"Bad packets",
			"Buffers sent",
			"Buffers queued",
			"Buffers defer-queued",
			"Buffers dropped",
			"Buffers received",
			"Multi buffers received",
			"No-link drops",
			"Tx-DMA No buffers",
			"Rx-Event lost",
			"Tx HwErrors",
			"Isrs Rx",
			"Isrs Tx",
			"Isrs Abnormal",
			"Isrs Rx-NoBuff",
			"Isrs RxOvf",
			"No buffers",
			"Phy LS changes",
			"Pending transmissions",
			"Live resets"
		};
		const uint32_t xCounters = ARRAY_SIZE( pszDesc );
		uint32_t xCtr;
		int xRc;

		/* Remove compile time warnings about unused parameters, and check the
		 * write buffer is not NULL.  NOTE - for simplicity, this example assumes the
		 * write buffer length is adequate, so does not check for buffer overflows. */
		( void ) pcCommandString;

		if( pcWriteBuffer == NULL )
		{
			return pdFALSE;
		}

		xIndex++;

		if( xIndex < xCounters )
		{
			switch( xIndex )
			{
				case 0:
					xRc = snprintf( pcWriteBuffer, xWriteBufferLen, "%30s: %llu\r\n", pszDesc[ xIndex ], emac_stat.uBytesReceived );
					break;

				case 1:
					xRc = snprintf( pcWriteBuffer, xWriteBufferLen, "%30s: %llu\r\n", pszDesc[ xIndex ], emac_stat.uBytesSent );
					break;

				default:
					xCtr = ( ( uint32_t * ) &emac_stat.uBadPackets )[ xIndex - 2 ];
					xRc = snprintf( pcWriteBuffer, xWriteBufferLen, "%30s: %u\r\n", pszDesc[ xIndex ], xCtr );
					break;
			}

			if( xRc < 0 )
			{
				goto err;
			}

			xReturn = pdPASS;
		}
		else
		{
err:
			/* Reset the index for the next time it is called. */
			xIndex = -1;

			/* Ensure nothing remains in the write buffer. */
			pcWriteBuffer[ 0 ] = 0x00;
			xReturn = pdFALSE;
		}

		return xReturn;
	}

	void vEMACRegisterCLICommands( void )
	{
		/* Register all the command line commands defined immediately above. */
		FreeRTOS_CLIRegisterCommand( &xEmacStats );
	}

#endif /* ipconfigEMAC_INCLUDE_CLI */
