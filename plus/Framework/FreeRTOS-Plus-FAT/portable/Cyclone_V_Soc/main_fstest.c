/*
 * main_fstest -- test the file system.
 * Based on main_full.c
 *
 * hn 5/9/2019
 */

/* Standard includes. */
#include <stdio.h>

/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"

/* FreeRTOS+CLI includes. */
#include "FreeRTOS_CLI.h"

/* Standard demo includes. */
#include "partest.h"

/*-----------------------------------------------------------*/

/* Priorities for the demo application tasks. */
#define mainDEFAULT_PRIORITY				( tskIDLE_PRIORITY )
#define mainUART_COMMAND_CONSOLE_STACK_SIZE	( configMINIMAL_STACK_SIZE * 3UL )

/* The priority used by the UART command console task.  This is very basic and
uses the Altera polling UART driver - so *must* run at the idle priority. */
#define mainUART_COMMAND_CONSOLE_TASK_PRIORITY	( tskIDLE_PRIORITY )

#define NL	"\r\n"
/*-----------------------------------------------------------*/

/*
 * File system related definitions.
 */

#define RAMDISK				1
#define SDCARD				2

/*
 * Select the media type.
 */
#define mainMEDIA_TYPE	SDCARD

/* FreeRTOS+FAT includes. */
#include "ff_stdio.h"
#if( mainMEDIA_TYPE == RAMDISK )
	#include "ff_ramdisk.h"
#else
	#include "ff_sddisk.h"
#endif

#define mainDISK_NAME		"/"

#if( mainMEDIA_TYPE == RAMDISK )
	/* The number and size of sectors that will make up the RAM disk. */
	#define mainRAM_DISK_SECTOR_SIZE	512UL /* Currently fixed! */
	#define mainRAM_DISK_SECTORS		( ( 200UL * 1024UL * 1024UL ) / mainRAM_DISK_SECTOR_SIZE ) /* 200M bytes. */
	#define mainIO_MANAGER_CACHE_SIZE	( 15UL * mainRAM_DISK_SECTOR_SIZE )
#endif

/*-----------------------------------------------------------*/

/*
 * External functions.
 */

/*
 * The task that manages the FreeRTOS+CLI input and output.
 */
extern void vUARTCommandConsoleStart( uint16_t usStackSize, UBaseType_t uxPriority );

/*
 * Register commands that can be used with FreeRTOS+CLI.  The commands are
 * defined in CLI-Commands.c and File-Related-CLI-Command.c respectively.
 */
extern void vRegisterSampleCLICommands( void );

/*
 * Register a set of example CLI commands that interact with the file system.
 * These are very basic.
 */
extern void vRegisterFileSystemCLICommands( void );

/* Helpers. */
extern void vApplicationPrintf( const char *pcFormat, ... );
extern uint32_t ulApplicationNow( void );

/*-----------------------------------------------------------*/

/*
 * Local functions.
 */

/* The init task brings up everything else. */
static void prvInitTask( void *pvParameters );

/* Initialize the file system. */
static void prvInitFileSystem( void );

/* Register test commands. */
static void prvRegisterTestCommands( void );

/* CLI command handlers. */
static BaseType_t prvDoAssert( char *pcWriteBuffer, size_t xWriteBufferLen,
	const char *pcCommandString );
static BaseType_t prvDoSof( char *pcWriteBuffer, size_t xWriteBufferLen,
	const char *pcCommandString );
static BaseType_t prvDoOom( char *pcWriteBuffer, size_t xWriteBufferLen,
	const char *pcCommandString );
static BaseType_t prvDoTfw( char *pcWriteBuffer, size_t xWriteBufferLen,
	const char *pcCommandString );
static BaseType_t prvDoTfr( char *pcWriteBuffer, size_t xWriteBufferLen,
	const char *pcCommandString );
#if( mainMEDIA_TYPE == SDCARD )
	static BaseType_t prvDoFmtsd( char *pcWriteBuffer, size_t xWriteBufferLen,
		const char *pcCommandString );
#endif

/* Test functions. */
static void prvTestFileWrite( const char *pcName, size_t xSize, char *pcOut );
static void prvTestFileRead( const char *pcName, char *pcOut );
static bool prvIsValidFileName( const char *pcs );

/*-----------------------------------------------------------*/

#if( mainMEDIA_TYPE == RAMDISK )
	/* RAM disk storage. */
	static uint8_t ucRAMDisk[ mainRAM_DISK_SECTORS * mainRAM_DISK_SECTOR_SIZE ];
#endif

/* FreeRTOS+FAT disk. */
static FF_Disk_t *pxDisk;

/* Buffer for file read/write test. */
static unsigned char *pcFileBuffer;

/* CLI command definitions. */
static const CLI_Command_Definition_t xCommands[] =
{
	{
		"assert",									/* The command string to type. */
		"assert:" NL " Generate an assertion." NL,	/* Help text. */
		prvDoAssert,								/* The function to run. */
		0											/* No parameters are expected. */
	},
	{
		"sof",
		"sof:" NL " Deplete the stack, i.e. stack overflow." NL,
		prvDoSof,
		0
	},
	{
		"oom",
		"oom:" NL " Deplete the heap, i.e. malloc failure." NL,
		prvDoOom,
		0
	},
	{
		"tfw",
		"tfw <file> <size>:" NL " Test file write performance." NL,
		prvDoTfw,
		2
	},
	{
		"tfr",
		"tfr <file>:" NL " Test file read performance." NL,
		prvDoTfr,
		1
	},
#if( mainMEDIA_TYPE == SDCARD )
	{
		"fmtsd",
		"fmtsd:" NL " Partition and format the SD card." NL,
		prvDoFmtsd,
		0
	},
#endif
};

/*-----------------------------------------------------------*/

/*
 * Test the file system via the CLI commands.
 */
void main_fstest( void )
{
	/* Create the init task. */
	xTaskCreate( prvInitTask, "init", configMINIMAL_STACK_SIZE * 2UL, NULL,
		mainDEFAULT_PRIORITY, NULL );

	/* Start the scheduler. */
	vTaskStartScheduler();

	/* If all is well, the scheduler will now be running, and the following
	line will never be reached.  If the following line does execute, then
	there was either insufficient FreeRTOS heap memory available for the idle
	and/or timer tasks to be created, or vTaskStartScheduler() was called from
	User mode.  See the memory management section on the FreeRTOS web site for
	more details on the FreeRTOS heap http://www.freertos.org/a00111.html.  The
	mode from which main() is called is set in the C start up code and must be
	a privileged mode (not user mode). */
	for( ;; );
}
/*-----------------------------------------------------------*/

void prvInitTask( void *pvParameters )
{
TickType_t xNextWakeTime;

	/* Remove compiler warning about unused parameter. */
	( void ) pvParameters;

	/* Register the standard CLI commands. */
	vRegisterSampleCLICommands();

	/* Register the file system commands. */
	vRegisterFileSystemCLICommands();

	/* Register the test commands. */
	prvRegisterTestCommands();

	/* Start the tasks that implements the command console on the UART, as
	described above. */
	vUARTCommandConsoleStart( mainUART_COMMAND_CONSOLE_STACK_SIZE, mainUART_COMMAND_CONSOLE_TASK_PRIORITY );

	/* Yield to the console task so that the UART is initialized. */
	vTaskDelay( pdMS_TO_TICKS( 10 ) );

	/* Initialize the file system. */
	prvInitFileSystem();

	vApplicationPrintf( "init: the system is up." NL );

	/* Initialise xNextWakeTime - this only needs to be done once. */
	xNextWakeTime = xTaskGetTickCount();

	for( ;; )
	{
		/* Place this task in the blocked state until it is time to run again. */
		vTaskDelayUntil( &xNextWakeTime, pdMS_TO_TICKS( 500UL ) );

		/* Toggle the LED to indicate the OS is still running. */
		vParTestToggleLED( 0 );
	}
}
/*-----------------------------------------------------------*/

void prvInitFileSystem( void )
{
#if( mainMEDIA_TYPE == RAMDISK )
	pxDisk = FF_RAMDiskInit( mainDISK_NAME, ucRAMDisk,
					mainRAM_DISK_SECTORS,
					mainIO_MANAGER_CACHE_SIZE );
	if( pxDisk )
	{
		FF_RAMDiskShowPartition( pxDisk );
	}
	else
	{
		vApplicationPrintf( "ERROR: prvInitFileSystem: RAM disk init failed." NL );
	}
#else
	pxDisk = FF_SDDiskInit( mainDISK_NAME );
	if( ! pxDisk )
	{
		vApplicationPrintf( "ERROR: prvInitFileSystem: SD disk init failed." NL );
	}
#endif
}
/*-----------------------------------------------------------*/

void prvRegisterTestCommands( void )
{
int i;

	for( i = 0; i < sizeof( xCommands ) / sizeof( xCommands[0] ); i++ )
	{
		FreeRTOS_CLIRegisterCommand( xCommands + i );
	}
}
/*-----------------------------------------------------------*/

BaseType_t prvDoAssert( char *pcWriteBuffer, size_t xWriteBufferLen,
	const char *pcCommandString )
{
	configASSERT( 0 );
	return pdFALSE; /* Won't reach here. */
}
/*-----------------------------------------------------------*/

BaseType_t prvDoSof( char *pcWriteBuffer, size_t xWriteBufferLen,
	const char *pcCommandString )
{
	/* Declare a large local variable and ask the scheduler. */
	char cLarge[5000];
	( void ) cLarge;
	taskYIELD();
	return pdFALSE; /* Won't reach here. */
}
/*-----------------------------------------------------------*/

BaseType_t prvDoOom( char *pcWriteBuffer, size_t xWriteBufferLen,
	const char *pcCommandString )
{
	/* Allocate a large chunk of memory. */
	pvPortMalloc( 1024 * 1024 );
	return pdFALSE; /* Won't reach here. */
}
/*-----------------------------------------------------------*/

BaseType_t prvDoTfw( char *pcWriteBuffer, size_t xWriteBufferLen,
	const char *pcCommandString )
{
const char *pcArg;
BaseType_t xArgl;
char cName[32];
int iNamel;
size_t xSize;

	/* Get the parameters. */
	pcArg = FreeRTOS_CLIGetParameter( pcCommandString, 1, &xArgl );

	/* CLIGetParameters() just returns a pointer to the start of
	 * the parameter, not null terminated like strtok(). Make sure to
	 * extract out just the wanted parameter by 0-terminating it. */
	iNamel = ( xArgl < 31 ) ? xArgl : 31;
	strncpy( cName, pcArg, iNamel );
	cName[ iNamel ] = '\0';
	if ( ! prvIsValidFileName( cName ) )
	{
		sprintf( pcWriteBuffer, "Invalid file name: %s." NL, cName );
		return pdFALSE; /* No more data to return. */
	}

	pcArg = FreeRTOS_CLIGetParameter( pcCommandString, 2, &xArgl );
	xSize = atoi( pcArg );

	vApplicationPrintf( "DBG: prvDoTfw: name=%s,size=%u." NL,
		cName, xSize);

	prvTestFileWrite( cName, xSize, pcWriteBuffer );

	return pdFALSE; /* No more data to return. */
}
/*-----------------------------------------------------------*/

void prvTestFileWrite( const char *pcName, size_t xSize, char *pcOut )
{
FF_FILE *xFp;
size_t xWritten;
uint32_t ulCrc, ulStart, ulEnd, ulDur, ulSec, ulMsec;
int i;

	/* Allocate the file buffer at a high-enough address. */
	pcFileBuffer = ( void * ) 0x10000000; // 256M

	/* Initialize the file buffer by filling it with a non-0 pattern.
	 * A sequence will be a good test. */
	vApplicationPrintf( "DBG: prvTestFileWrite: filling the buffer..." NL );
	for( i = 0; i < xSize; i++ )
	{
		pcFileBuffer[i] = ( unsigned char ) i; /* let it roll over. */
	}
	vApplicationPrintf( "DBG: prvTestFileWrite: buffer filled." NL );

	xFp = ff_fopen( pcName, "wb" );
	if( !xFp )
	{
		sprintf( pcOut, "Cannot open file %s: error=%d." NL,
			pcName, stdioGET_ERRNO() );
		return;
	}

	/*--------------------------------------------*/
	ulStart = ulApplicationNow();

	xWritten = ff_fwrite( pcFileBuffer, 1, xSize, xFp );
	if( xWritten != xSize )
	{
		ff_fclose( xFp );
		sprintf( pcOut, "File write error %d: written=%u bytes." NL,
			stdioGET_ERRNO(), xWritten );
		return;
	}

	ulEnd = ulApplicationNow();
	/*--------------------------------------------*/

	ff_fclose( xFp );

	ulDur = ulEnd - ulStart;
	ulSec = (uint32_t)(ulDur / 1000ul);
	ulMsec = (uint32_t)(ulDur % 1000ul);

	/* Also compute the file CRC. */
	ulCrc = FF_GetCRC32( pcFileBuffer, xSize );

	sprintf( pcOut, "File %s written successfully: size=%d" NL
		"Duration: %lu ms (%lu.%03lu)" NL
		"CRC32: 0x%08X" NL,
		pcName, xSize, ulDur, ulSec, ulMsec, ( unsigned int ) ulCrc );
}
/*-----------------------------------------------------------*/

bool prvIsValidFileName( const char *pcs )
{
	return (
		! strchr( pcs, '*' ) &&
		! strchr( pcs, ' ' ) &&
		! strchr( pcs, '/' ) &&
		! strchr( pcs, '\\' ) &&
		! strchr( pcs, '.' ) &&
		! strchr( pcs, ',' ) &&
		! strchr( pcs, '>' ) &&
		! strchr( pcs, '<' ) &&
		! strchr( pcs, '|' ) &&
		! strchr( pcs, '"' )
	);
}
/*-----------------------------------------------------------*/

BaseType_t prvDoTfr( char *pcWriteBuffer, size_t xWriteBufferLen,
	const char *pcCommandString )
{
const char *pcArg;
BaseType_t xArgl;
char cName[32];
int iNamel;

	/* Get the parameters. */
	pcArg = FreeRTOS_CLIGetParameter( pcCommandString, 1, &xArgl );
	iNamel = ( xArgl < 31 ) ? xArgl : 31;
	strncpy( cName, pcArg, iNamel );
	cName[ iNamel ] = '\0';
	if ( ! prvIsValidFileName( cName ) )
	{
		sprintf( pcWriteBuffer, "Invalid file name: %s." NL, cName );
		return pdFALSE; /* No more data to return. */
	}

	vApplicationPrintf( "DBG: prvDoTfr: name=%s." NL, cName );

	prvTestFileRead( cName, pcWriteBuffer );

	return pdFALSE; /* No more data to return. */
}
/*-----------------------------------------------------------*/

void prvTestFileRead( const char *pcName, char *pcOut )
{
FF_FILE *xFp;
size_t xRead;
uint32_t ulCrc, ulStart, ulEnd, ulDur, ulSec, ulMsec;
size_t xSize;

	/* Allocate the file buffer at a high-enough address. */
	pcFileBuffer = ( void * ) 0x10000000; // 256M

	/* Open the file first. */
	xFp = ff_fopen( pcName, "rb" );
	if( !xFp )
	{
		sprintf( pcOut, "Cannot open file %s: error=%d." NL,
			pcName, stdioGET_ERRNO() );
		return;
	}

	/* Determine the file size. */
	xSize = ff_filelength( xFp );
	vApplicationPrintf( "DBG: prvTestFileRead: size=%d." NL, xSize );

	/* Initialize the buffer to all 0's. */
	memset( pcFileBuffer, 0, xSize );

	/*--------------------------------------------*/
	ulStart = ulApplicationNow();

	xRead = ff_fread( pcFileBuffer, 1, xSize, xFp );
	if( xRead != xSize )
	{
		ff_fclose( xFp );
		sprintf( pcOut, "File read error %d: read=%u bytes." NL,
			stdioGET_ERRNO(), xRead );
		return;
	}

	ulEnd = ulApplicationNow();
	/*--------------------------------------------*/

	ff_fclose( xFp );

	ulDur = ulEnd - ulStart;
	ulSec = (uint32_t)(ulDur / 1000ul);
	ulMsec = (uint32_t)(ulDur % 1000ul);

	/* Also compute the file CRC. */
	ulCrc = FF_GetCRC32( pcFileBuffer, xSize );

	sprintf( pcOut, "File %s read successfully: size=%d" NL
		"Duration: %lu ms (%lu.%03lu)" NL
		"CRC32: 0x%08X" NL,
		pcName, xSize, ulDur, ulSec, ulMsec, ( unsigned int ) ulCrc );
}
/*-----------------------------------------------------------*/

#if( mainMEDIA_TYPE == SDCARD )

#define HIDDEN_SECTOR_COUNT		8
#define PRIMARY_PARTITIONS		1

BaseType_t prvDoFmtsd( char *pcWriteBuffer, size_t xWriteBufferLen,
	const char *pcCommandString )
{
	FF_PartitionParameters_t xPartition;
	FF_Error_t xError;

	/* Media cannot be used until it has been partitioned.  In this
	 * case a single partition is to be created that fills all available space - so
	 * by clearing the xPartition structure to zero. */
	memset( &xPartition, 0x00, sizeof( xPartition ) );
	xPartition.ulSectorCount = pxDisk->ulNumberOfSectors;
	xPartition.ulHiddenSectors = HIDDEN_SECTOR_COUNT;
	xPartition.xPrimaryCount = PRIMARY_PARTITIONS;
	xPartition.eSizeType = eSizeIsQuota;

	/* Perform the partitioning. */
	xError = FF_Partition( pxDisk, &xPartition );

	/* Was the disk partitioned successfully? */
	if( FF_isERR( xError ) == pdFALSE )
	{
		/* The disk was partitioned successfully.  Format the first partition. */
		/* Use the SD card specific function. */
		xError = FF_SDDiskFormat( pxDisk, 0 ); /* Partition 0. */

		if( FF_isERR( xError ) == pdFALSE )
		{
			sprintf( pcWriteBuffer, "Successfully formatted disk." NL );
		}
		else
		{
			sprintf( pcWriteBuffer, "ERROR: Format failed: %s (0x%08x)." NL,
				FF_GetErrMessage( xError ), ( unsigned int ) xError );
		}
	}
	else
	{
		sprintf( pcWriteBuffer, "ERROR: Partition failed: %s (0x%08x)." NL,
			FF_GetErrMessage( xError ), ( unsigned int ) xError );
	}

	return pdFALSE; /* No more data to return. */
}
/*-----------------------------------------------------------*/

#endif /* ( mainMEDIA_TYPE == SDCARD ) */
