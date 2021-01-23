/*
 * uncached_memory.c
 *
 * This module will declare 1 MB of memory and switch off the caching for it.
 *
 * pucGetUncachedMemory( ulSize ) returns a trunc of this memory with a length
 * rounded up to a multiple of 4 KB
 *
 * ucIsCachedMemory( pucBuffer ) returns non-zero if a given pointer is NOT
 * within the range of the 1 MB non-cached memory.
 *
 */

/*
 * After "_end", 1 MB of uncached memory will be allocated for DMA transfers.
 * Both the DMA descriptors as well as all EMAC TX-buffers will be allocated in
 * uncached memory.
 */

/* Standard includes. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

/* FreeRTOS+TCP includes. */
#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"
#include "FreeRTOS_IP_Private.h"

#include "x_emacpsif.h"
#include "x_topology.h"
#include "xstatus.h"

#include "xparameters.h"
#include "xparameters_ps.h"
#include "xil_exception.h"
#include "xil_mmu.h"

#include "uncached_memory.h"

/* Reserve 4 MB of memory. */
#define uncMEMORY_SIZE				0x200000ul

/* Make sure that each pointer has an alignment of 4 KB. */
#define uncALIGNMENT_SIZE			0x1000uL

#define DDR_MEMORY_END	(XPAR_PSU_DDR_0_S_AXI_HIGHADDR)

#define uncMEMORY_ATTRIBUTE			NORM_NONCACHE | INNER_SHAREABLE

static void vInitialiseUncachedMemory( void );

static uint32_t *pucHeadOfMemory;
static uint32_t ulMemorySize;
static uint32_t *pucStartOfMemory = NULL;

/* The linker file defines some pseudo variables. '_end' is one of them.
It is located at the first free byte in RAM. */
extern u8 _end;

/*-----------------------------------------------------------*/

uint8_t ucIsCachedMemory( const uint8_t *pucBuffer )
{
uint8_t ucReturn;

	if( ( pucStartOfMemory != NULL ) &&
		( pucBuffer >= pucStartOfMemory ) &&
		( pucBuffer < ( pucStartOfMemory + uncMEMORY_SIZE ) ) )
	{
		ucReturn = pdFALSE;
	}
	else
	{
		ucReturn = pdTRUE;
	}

	return ucReturn;
}
/*-----------------------------------------------------------*/

uint8_t *pucGetUncachedMemory( uint32_t ulSize )
{
uint64_t *pucReturn;
uint32_t ulSkipSize;

	if( pucStartOfMemory == NULL )
	{
		vInitialiseUncachedMemory( );
	}
	if( ( pucStartOfMemory == NULL ) || ( ulSize > ulMemorySize ) )
	{
		pucReturn = NULL;
	}
	else
	{
		pucReturn = pucHeadOfMemory;
		/* Make sure that the next pointer return will have a good alignment. */
		ulSkipSize = ( ulSize + uncALIGNMENT_SIZE ) & ~( uncALIGNMENT_SIZE - 1uL );
		pucHeadOfMemory += ulSkipSize;
		ulMemorySize -= ulSkipSize;
	}

	return pucReturn;
}
/*-----------------------------------------------------------*/

static void vInitialiseUncachedMemory( )
{

	/* At the end of program's space... */
	pucStartOfMemory = ( uint32_t * ) &( _end );

	/* Align the start address to 1 MB boundary. */
	pucStartOfMemory = ( uint32_t * )( ( ( uint32_t )pucStartOfMemory + uncMEMORY_SIZE ) & ( ~( uncMEMORY_SIZE - 1 ) ) );

	if( ( ( u64 )pucStartOfMemory ) + uncMEMORY_SIZE > DDR_MEMORY_END )
	{
		FreeRTOS_printf( ( "vInitialiseUncachedMemory: Can not allocate uncached memory\n" ) );
	}
	else
	{
		/* Some objects want to be stored in uncached memory. Hence the 1 MB
		address range that starts after "_end" is made uncached by setting
		appropriate attributes in the translation table. */
		Xil_SetTlbAttributes( ( u64 ) pucStartOfMemory, uncMEMORY_ATTRIBUTE );

		/* For experiments in the SDIO driver, make the remaining uncached memory
		public */
		pucHeadOfMemory = pucStartOfMemory;
		ulMemorySize = uncMEMORY_SIZE;
		memset( pucStartOfMemory, '\0', uncMEMORY_SIZE );
	}
}
