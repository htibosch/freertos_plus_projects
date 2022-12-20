/*
 * PCAP live player:
 * This module starts a FreeRTOS task which will read a PCAP file and send
 * all packets the the network interface.
 */

/* Standard includes. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

/* FreeRTOS+TCP includes. */
#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"
#include "FreeRTOS_IP_Private.h"
#include "FreeRTOS_ARP.h"
#include "FreeRTOS_UDP_IP.h"
#include "FreeRTOS_DHCP.h"
#if ( ipconfigUSE_DHCPv6 == 1 )
	#include "FreeRTOS_DHCPv6.h"
#endif
#include "NetworkBufferManagement.h"
#include "FreeRTOS_DNS.h"

#if( ipconfigMULTI_INTERFACE != 0 )
	#include "FreeRTOS_Routing.h"
	#include "FreeRTOS_ND.h"
#endif

#if( USE_PLUS_FAT != 0 )
	/* FreeRTOS+FAT includes. */
	#include "ff_headers.h"
	#include "ff_stdio.h"
	#include "ff_ramdisk.h"
	#include "ff_sddisk.h"
#endif

#include "pcap_live_player.h"

#ifndef BUFFER_FROM_WHERE_CALL
	#define BUFFER_FROM_WHERE_CALL( FileName )
#endif

#define MAGIC_EXPECTED  0xa1b2c3d4
#define MAGIC_REVERSED  0xd4c3b2a1

#if( ipconfigMULTI_INTERFACE != 0 )
	static NetworkInterface_t *pxZynq_Interface;
#endif

#define mainLIVE_PLAYER_TASK_PRIORITY	( tskIDLE_PRIORITY + 4 )
#define	mainLIVE_PLAYER_STACK_SIZE		2048

uint32_t to_native_u32(uint32_t aValue);
uint16_t to_native_u16(uint16_t aValue);

typedef struct pcap_hdr_s
{
		uint32_t magic_number;   /* magic number */
		uint16_t version_major;  /* major version number */
		uint16_t version_minor;  /* minor version number */
		int16_t thiszone;       /* GMT to local correction */
		uint32_t sigfigs;        /* accuracy of timestamps */
		uint32_t snaplen;        /* max length of captured packets, in octets */
		uint32_t network;        /* data link type */
} pcap_hdr_t;

typedef struct pcaprec_hdr_s
{
		uint32_t ts_sec;         /* timestamp seconds */
		uint32_t ts_usec;        /* timestamp microseconds */
		uint32_t incl_len;       /* number of octets of packet saved in file */
		uint32_t orig_len;       /* actual length of packet */
		uint8_t data[ 1 ];
} pcaprec_hdr_t;

static BaseType_t needConvert;

uint32_t to_native_u32(uint32_t aValue)
{
	if( needConvert )
	{
		aValue = ( aValue << 24 ) |
			( ( aValue << 8 ) & 0x00ff0000U ) |
			( ( aValue >> 8 ) & 0x0000ff00U ) |
			  ( aValue & 0x000000ffU );
	}
	return aValue;
}

uint16_t to_native_u16(uint16_t aValue)
{
	if( needConvert )
	{
		aValue = ( aValue << 8 ) | ( aValue >> 8 );
	}
	return aValue;
}

struct sEthernetStream
{
	uint8_t *pucBuffer;
	uint8_t *pucCurrentPointer;
	uint8_t *pucLastPointer;
	BaseType_t active;
	TickType_t xFirstTick;
	uint64_t ullStartTime;
	uint32_t ulMessageCount;
	uint32_t ulFailCount;
	size_t ulFileLength, ulBytesRead;
	size_t ulPacketCount;
};

struct sEthernetStream ethStream;

static BaseType_t addressAllowed( uint32_t ulAddress )
{
	BaseType_t index;
	uint8_t ucUnit = ( ulAddress & 0xFF );
	static uint8_t addresses[] = {
		114,
		228,
	};
	if( ( ulAddress >> 24 ) != 10 )
	{
		if( ( ulAddress & 0xffffff00U ) != 0xc0a80100U )
		{
			return pdFALSE;
		}
	}
	if( ucUnit < 20 )
	{
		return pdFALSE;
	}
	for( index = 0; index < ARRAY_SIZE( addresses ); index++ )
	{
		if( ucUnit == addresses[ index ] )
		{
			return pdFALSE;
		}
	}
	return pdTRUE;
}

static uint32_t ulIPAddressConvert( uint32_t ulAddress )
{
	ulAddress = FreeRTOS_ntohl( ulAddress );
	if( addressAllowed( ulAddress ) )
	{
		ulAddress = 0xC0A80200 | ( ulAddress & 0xFF );
	}

	return FreeRTOS_htonl( ulAddress );
}

static void prvEthernetTask( void *pvParameter )
{
	( void ) pvParameter;
	size_t matchCount = 0U;

	for( ;; )
	{
		//ulTaskNotifyTake( pdTRUE, portMAX_DELAY );
		vTaskDelay( 1U );
		if( ethStream.active == pdFALSE )
		{
			continue;
		}
		if( ethStream.xFirstTick == 0U )
		{
			ethStream.xFirstTick = xTaskGetTickCount();
		}

		while( ( ethStream.pucCurrentPointer < ethStream.pucLastPointer ) && ( ethStream.active == pdTRUE ) )
		{
			pcaprec_hdr_t * header = ( pcaprec_hdr_t * ) ethStream.pucCurrentPointer;
			int thisLength = to_native_u32( header->incl_len );
			uint8_t *pucNextPointer = ethStream.pucCurrentPointer + ( int ) ( thisLength + 16 );

			if( pucNextPointer >= ethStream.pucLastPointer )
			{
				/* We're done. */
				FreeRTOS_printf( ( "All %u messages sent, %u failed\n", ( unsigned ) ethStream.ulMessageCount, ethStream.ulFailCount ) );
				if( ethStream.pucBuffer != NULL )
				{
					uint8_t *puc = ethStream.pucBuffer;
					ethStream.pucBuffer = NULL;
					vPortFree( puc );
				}
				( void ) memset( &( ethStream ), 0, sizeof ethStream );
				break;
			}

			/* Time checks. */
			{
				/* Calculate ullCurrentTime in usec. */
				uint64_t ullCurrentTime = ( ( uint64_t ) 1000000U ) * ( ( uint64_t ) to_native_u32( header->ts_sec ) ) + ( ( uint64_t ) to_native_u32( header->ts_usec ) );

				if( ethStream.ullStartTime == 0U )
				{
					ethStream.ullStartTime = ullCurrentTime;
				}

				ullCurrentTime -= ethStream.ullStartTime;

				/* Calculate ulPCAPTimePassed in msec. */
				uint32_t ulPCAPTimePassed = ( uint32_t ) ( ullCurrentTime / ( ( uint64_t ) 1000U ) );
				TickType_t xCurrentTick = xTaskGetTickCount();

				/* Calculate ulFreeRTOSTimePassed in msec. */
				uint32_t ulFreeRTOSTimePassed = xCurrentTick - ethStream.xFirstTick;

				if( ulPCAPTimePassed > ulFreeRTOSTimePassed )
				{
					/* Not yet time for this one. */
					break;
				}
			}

			ethStream.pucCurrentPointer = pucNextPointer;
			ethStream.ulBytesRead += thisLength + 16;
			//uint32_t perc = ( 1000U * ethStream.ulBytesRead ) / ethStream.ulFileLength;
			uint32_t perc = ( 1000U * ethStream.ulMessageCount ) / ethStream.ulPacketCount;
			
			static uint32_t lastPerc = 100U;
			if( lastPerc != perc )
			{
				lastPerc = perc;
				FreeRTOS_printf( ( "Done %u.%u %% %u/%u packets %lu matches\n",
					perc / 10, perc % 10,
					( unsigned ) ethStream.ulMessageCount,
					( unsigned ) ethStream.ulPacketCount,
					( unsigned long ) matchCount ) );
			}

			ProtocolPacket_t *packet = ( ProtocolPacket_t * )header->data;
			uint16_t usFrameType = packet->xUDPPacket.xEthernetHeader.usFrameType;
			uint8_t ucProtocol = packet->xUDPPacket.xIPHeader.ucProtocol;
			BaseType_t xDoSend = pdFALSE;
			BaseType_t xDoCalculate = pdFALSE;
			uint32_t ulSender = 0U;
			uint32_t ulTarget = 0U;
			uint32_t ulSenderNew = 0U;
			uint32_t ulTargetNew = 0U;

			if( usFrameType == ipARP_FRAME_TYPE )
			{
				ARPPacket_t * pxARPFrame = ( ARPPacket_t * ) header->data;
				memcpy( &( ulSender ), pxARPFrame->xARPHeader.ucSenderProtocolAddress, sizeof ( ulSender ) );
				ulTarget = pxARPFrame->xARPHeader.ulTargetProtocolAddress;

				ulTargetNew = ulIPAddressConvert( ulTarget );
				ulSenderNew = ulIPAddressConvert( ulSender );

				if ( ( ulSenderNew != ulSender ) || ( ulTargetNew != ulTarget ) )
				{
					pxARPFrame->xARPHeader.ulTargetProtocolAddress = ulTargetNew;
					memcpy( pxARPFrame->xARPHeader.ucSenderProtocolAddress, &( ulSenderNew ), sizeof ( ulSenderNew ) );
				}
			}
			else if( usFrameType == ipIPv4_FRAME_TYPE )
			{
				ulSender = packet->xUDPPacket.xIPHeader.ulSourceIPAddress;
				ulTarget = packet->xUDPPacket.xIPHeader.ulDestinationIPAddress;
				ulSenderNew = ulIPAddressConvert( ulSender );
				ulTargetNew = ulIPAddressConvert( ulTarget );

				if ( ( ulSenderNew != ulSender ) || ( ulTargetNew != ulTarget ) )
				{
					packet->xUDPPacket.xIPHeader.ulSourceIPAddress = ulSenderNew;
					packet->xUDPPacket.xIPHeader.ulDestinationIPAddress = ulTargetNew;
					xDoCalculate = pdTRUE;
				}
				switch( ucProtocol )
				{
				case ipPROTOCOL_UDP:
					break;
				case ipPROTOCOL_TCP:
					break;
				case ipPROTOCOL_ICMP:
					break;
				}
			}
//			uint32_t ulAllowedIP = FreeRTOS_inet_addr_quick( 10, 60, 0, 2 );
			uint32_t ulAllowedIP = FreeRTOS_inet_addr_quick( 10, 60, 0, 0 );
//FreeRTOS_printf( ( "match %xip = %xip %xip\n",
//	( unsigned ) FreeRTOS_ntohl( ulAllowedIP ),
//	( unsigned ) FreeRTOS_ntohl( ulSender ),
//	( unsigned ) FreeRTOS_ntohl( ulTarget ) ) );
			if(  ( ulAllowedIP == ( ulSender & 0xffff0000U ) ) || ( ulAllowedIP == ( ulTarget && 0xffff0000U ) ) )
			{
				xDoSend = pdTRUE;
				matchCount++;
			}
			xDoSend = pdTRUE;

			if( xDoCalculate )
			{
				IPHeader_t * pxIPHeader;
				TCPPacket_t * pxTCPPacket;
				pxTCPPacket = ( TCPPacket_t * ) header->data;
				pxIPHeader = &pxTCPPacket->xIPHeader;


				pxIPHeader->usHeaderChecksum = 0x00U;
				pxIPHeader->usHeaderChecksum = usGenerateChecksum( 0U, ( uint8_t * ) &( pxIPHeader->ucVersionHeaderLength ), ipSIZE_OF_IPv4_HEADER );
				pxIPHeader->usHeaderChecksum = ~FreeRTOS_htons( pxIPHeader->usHeaderChecksum );

				/* calculate the TCP checksum for an outgoing packet. */
				( void ) usGenerateProtocolChecksum( ( uint8_t * ) pxTCPPacket, thisLength, pdTRUE );

				/* A calculated checksum of 0 must be inverted as 0 means the checksum
				 * is disabled. */
				if( pxTCPPacket->xTCPHeader.usChecksum == 0U )
				{
					pxTCPPacket->xTCPHeader.usChecksum = 0xffffU;
				}
			}

			if( xDoSend == pdTRUE )
			{
				NetworkBufferDescriptor_t * pxNetworkBuffer = pxGetNetworkBufferWithDescriptor( BUFFER_FROM_WHERE_CALL( "main(1).c" ) thisLength, 1000U );
				if( pxNetworkBuffer == NULL )
				{
					ethStream.ulFailCount++;
				}
				else
				{
					memcpy( pxNetworkBuffer->pucEthernetBuffer, header->data, thisLength );

					IPStackEvent_t xSendEvent;

					xSendEvent.eEventType = eNetworkTxEvent;
					xSendEvent.pvData = ( void * ) pxNetworkBuffer;
					#if( ipconfigMULTI_INTERFACE != 0 )
						pxNetworkBuffer->pxInterface = pxZynq_Interface;
					#endif
					if( xSendEventStructToIPTask( &xSendEvent, 0U ) == pdPASS )
					{
						ethStream.ulMessageCount++;
						vTaskDelay( 2U );
					}
					else
					{
						vReleaseNetworkBufferAndDescriptor( pxNetworkBuffer );
						ethStream.ulFailCount++;
					}
				} /* if( pxNetworkBuffer != NULL ) */
			} /* if( xDoSend == pdTRUE ) */
		} /* while( ethStream.pucCurrentPointer < ethStream.pucLastPointer ) */
	} /* for( ;; ) */
}

#define ipCORRECT_CRC			0xffffU

static size_t xCountPackets( struct sEthernetStream *pxStream )
{
	size_t ulPacketCount = 0U;
	uint8_t * puc = pxStream->pucCurrentPointer;

	while( puc < pxStream->pucLastPointer )
	{
		ulPacketCount++;
		pcaprec_hdr_t * header = ( pcaprec_hdr_t * ) puc;
		int thisLength = to_native_u32( header->incl_len );

		{
			/* Check checmsum routines. */
			ProtocolPacket_t *packet = ( ProtocolPacket_t * )header->data;
			uint16_t usFrameType = packet->xUDPPacket.xEthernetHeader.usFrameType;
			uint8_t ucProtocol;
			#if( ipconfigUSE_IPv6 != 0 )
				IPPacket_IPv6_t * pxIPPacket = ( IPPacket_IPv6_t * ) header->data;
				if( usFrameType == ipIPv6_FRAME_TYPE )
				{
					ucProtocol = pxIPPacket->xIPHeader.ucNextHeader;
				}
				else
			#endif
			{
				ucProtocol = packet->xUDPPacket.xIPHeader.ucProtocol;
			}
			if( ( usFrameType == ipIPv4_FRAME_TYPE )
			#if( ipconfigUSE_IPv6 != 0 )
				|| ( usFrameType == ipIPv6_FRAME_TYPE ) 
			#endif
			)
			{
				if( usGenerateProtocolChecksum( header->data, thisLength, pdFALSE ) != ipCORRECT_CRC )
				{
					FreeRTOS_printf( ( "CRC error: Frame 0x%04x Protocol %u\n", FreeRTOS_ntohs( usFrameType ), ucProtocol ) );
				}
			}
		}
		puc = puc + ( int ) ( thisLength + 16 );
	}
	return ulPacketCount;
}

void vPCAPLivePlay( const char *pcFileName )
{
	static TaskHandle_t xEthernetTaskHandle = NULL;

	FreeRTOS_printf( ( "pcdata_test\n" ) );

	if( xEthernetTaskHandle == NULL )
	{
		#if( ipconfigMULTI_INTERFACE != 0 )
		{
			pxZynq_Interface = FreeRTOS_FirstNetworkInterface();
		}
		#endif
		xTaskCreate( prvEthernetTask, "EthWork", mainLIVE_PLAYER_STACK_SIZE, NULL, mainLIVE_PLAYER_TASK_PRIORITY, &( xEthernetTaskHandle ) );
	}

	FF_FILE * infile = ff_fopen( pcFileName, "rb" );
	if (infile == NULL) {
		FreeRTOS_printf( ( "Can not open %s errno %d\n", pcFileName, stdioGET_ERRNO() ) );
		return;
	}

	FreeRTOS_printf( ( "Opened %s, reading file...\n", pcFileName ) );

	ff_fseek(infile, 0, SEEK_END);

	ethStream.ulFileLength = ff_ftell (infile);

	FreeRTOS_printf( ( "ftell %lu\n", ( unsigned long ) ethStream.ulFileLength ) );

	ff_fseek(infile, 0, SEEK_SET);


	ethStream.pucBuffer = pvPortMalloc( ethStream.ulFileLength );

	if( ethStream.pucBuffer == NULL )
	{
		FreeRTOS_printf( ( "Failed to malloc %lu bytes\n", ( unsigned long ) ethStream.ulFileLength ) );
	}
	else
	{
		size_t count = ff_fread(ethStream.pucBuffer, 1, ethStream.ulFileLength, infile);
		FreeRTOS_printf( ( "Read %lu out of %lu bytes\n",
			( unsigned long ) count,
			( unsigned long ) ethStream.ulFileLength ) );
		pcap_hdr_t *file_header = (pcap_hdr_t *)ethStream.pucBuffer;

		needConvert = ( file_header->magic_number == MAGIC_REVERSED ) ? 1 : 0;

		if( ( file_header->magic_number != MAGIC_EXPECTED ) &&
			( file_header->magic_number != MAGIC_REVERSED ) )
		{
			FreeRTOS_printf( ( "%s has an unknown format. Please use plain unzipped PCAP files\n", pcFileName ) );
		}
		else
		{
			const uint8_t *ucPtr = ( const uint8_t * ) &( file_header->magic_number );
			FreeRTOS_printf( ( "Magic: %08X (%02X %02X %02X %02X) Version %d.%d Need conversion %d\n",
				to_native_u32( file_header->magic_number ),       /* magic number */
				ucPtr[ 0 ], ucPtr[ 1 ], ucPtr[ 2 ], ucPtr[ 3 ],
				to_native_u16( file_header->version_major ),      /* major version number */
				to_native_u16( file_header->version_minor ),      /* minor version number */
				needConvert ) );
			ethStream.pucCurrentPointer = ethStream.pucBuffer + sizeof *file_header;
			ethStream.pucLastPointer = ethStream.pucBuffer + ethStream.ulFileLength;
			ethStream.ulPacketCount = xCountPackets( &( ethStream ) );
			FreeRTOS_printf( ( "Packet count %lu\n", ( unsigned long ) ethStream.ulPacketCount ) );
			ethStream.active = pdTRUE;
		}
	}
	ff_fclose( infile );
	FreeRTOS_printf( ( "All done\n" ) );
}
