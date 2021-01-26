/* Standard includes. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#include "inc/hw_memmap.h"

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

/* FreeRTOS+TCP includes. */
#include "FreeRTOSIPConfig.h"
#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"
#include "FreeRTOS_IP_Private.h"
#include "NetworkBufferManagement.h"

#include "tiva_mac.h"

#if ( ipconfigPACKET_FILLER_SIZE != 2 )
	#error Please define ipconfigPACKET_FILLER_SIZE as the value '2'
#endif
#define TX_OFFSET    ipconfigPACKET_FILLER_SIZE

#if EMAC_RX_BUFF_SIZE < ( ipconfigNETWORK_MTU + ipSIZE_OF_ETH_HEADER + ipSIZE_OF_ETH_CRC_BYTES + ipSIZE_OF_ETH_OPTIONAL_802_1Q_TAG_BYTES )
	#error "Wrong value selected for EMAC_RX_BUFF_SIZE"
#endif

#ifndef EMAC_PUBLIC_DATA
	static
#endif
tEMACDMADescriptor prxSegments[ ipconfigNIC_N_RX_DESC + ipconfigNIC_N_RX_SPARE_DESC + 1 ]
__attribute__( ( aligned( 4 ) ) );

EMAC_DCPT_NODE xRxDesc[ ipconfigNIC_N_RX_DESC + ipconfigNIC_N_RX_SPARE_DESC + 1 ];
extern TaskHandle_t xEMACTaskHandle;

BaseType_t emac_AppendBusyList( xEMAC * pem,
								EMAC_DCPT_LIST * pBusyList,
								EMAC_DCPT_LIST * pNewList,
								const uint32_t validControl );
BaseType_t emac_RxBuffersAppend( xEMAC * pem,
								 size_t nBuffs );
void clean_dma_txdescs( xEMAC * pem );

/*
 * @brief Initialize DMA descriptors
 */
BaseType_t emac_init_dma( xEMAC * pem )
{
	NetworkBufferDescriptor_t * pxBuffer;
	int iIndex;
	BaseType_t xRc;

	EMAC_ListInit( &pem->xRxFreeList );
	EMAC_ListInit( &pem->xRxBusyList );

	memset( &prxSegments[ ipconfigNIC_N_RX_DESC + ipconfigNIC_N_RX_SPARE_DESC ], 0, sizeof( tEMACDMADescriptor ) );
	prxSegments[ ipconfigNIC_N_RX_DESC + ipconfigNIC_N_RX_SPARE_DESC ].ui32Count = DES1_RX_CTRL_CHAINED;
	xRxDesc[ ipconfigNIC_N_RX_DESC + ipconfigNIC_N_RX_SPARE_DESC ].pxHWDesc = &prxSegments[ ipconfigNIC_N_RX_DESC + ipconfigNIC_N_RX_SPARE_DESC ];
	EMAC_ListAddHead( &pem->xRxBusyList, &xRxDesc[ ipconfigNIC_N_RX_DESC + ipconfigNIC_N_RX_SPARE_DESC ] ); /* the busy list must always have a dummy sw owned tail descriptor */

	/*
	 * Allocate RX descriptors, 1 at a time.
	 */
	for( iIndex = 0; iIndex < ( ipconfigNIC_N_RX_DESC + ipconfigNIC_N_RX_SPARE_DESC ); iIndex++ )
	{
		pxBuffer = pxGetNetworkBufferWithDescriptor( ipTOTAL_ETHERNET_FRAME_SIZE, ( TickType_t ) 0 );

		if( pxBuffer == NULL )
		{
			FreeRTOS_printf( ( "Unable to allocate a network buffer\n" ) );
			return -1;
		}

		prxSegments[ iIndex ].ui32Count = DES1_RX_CTRL_CHAINED;
		prxSegments[ iIndex ].pvBuffer1 = pxBuffer->pucEthernetBuffer;
		prxSegments[ iIndex ].ui32Count |=
			EMAC_RX_BUFF_SIZE << DES1_RX_CTRL_BUFF1_SIZE_S;
		prxSegments[ iIndex ].ui32CtrlStatus = DES0_RX_CTRL_OWN;

		xRxDesc[ iIndex ].pxHWDesc = &prxSegments[ iIndex ];
		EMAC_ListAddTail( &pem->xRxFreeList, &xRxDesc[ iIndex ], DES0_RX_CTRL_OWN );
	}

	xRc = emac_RxBuffersAppend( pem, ipconfigNIC_N_RX_DESC );

	if( xRc < 0 )
	{
		return xRc;
	}

	clean_dma_txdescs( pem );

	return 0;
}

static BaseType_t passEthMessages( xEMAC * pem,
								   NetworkBufferDescriptor_t * pMsg )
{
	IPStackEvent_t xRxEvent;

	#if ( ipconfigHAS_DEBUG_PRINTF == 1 )
		size_t xSize = pMsg->xDataLength;
		uint32_t xAddr0, xAddr1;

		xAddr0 = ( uint32_t ) pMsg;
		xAddr1 = ( uint32_t ) pMsg->pucEthernetBuffer;
	#endif
	xRxEvent.eEventType = eNetworkRxEvent;
	xRxEvent.pvData = ( void * ) pMsg;

	if( xSendEventStructToIPTask( &xRxEvent, ( portTickType ) 1000 ) != pdPASS )
	{
		/* The buffer could not be sent to the stack so	must be released again.
		 * This is a deferred handler taskr, not a real interrupt, so it is ok to
		 * use the task level function here. */
		do
		{
			NetworkBufferDescriptor_t * xNext = pMsg->pxNextBuffer;
			vReleaseNetworkBufferAndDescriptor( pMsg );
			pMsg = xNext;
			EMAC_STATS_INC( RxEventLost );
		} while( pMsg != NULL );

		#if ( ipconfigHAS_DEBUG_PRINTF == 1 )
			FreeRTOS_debug_printf( ( "passEthMessages: Can not queue return packet!\n" ) );
		#endif
		return -pdFREERTOS_ERRNO_EIO;
	}

	#if ( ipconfigHAS_DEBUG_PRINTF == 1 )
		if( xTCPWindowLoggingLevel > 3 )
		{
			FreeRTOS_debug_printf( ( "EMAC: received %x(%x) buffer: %u bytes\n", xAddr0, xAddr1, xSize ) );
		}
	#endif
	return 0;
}

BaseType_t emac_RxGetPacket( xEMAC * pem,
							 NetworkBufferDescriptor_t ** ppPkt,
							 size_t * pnBuffs,
							 uint64_t * pnStatus )
{
	EMAC_DCPT_NODE * pxDesc;
	NetworkBufferDescriptor_t * pxBuffer;
	NetworkBufferDescriptor_t * pxFirstBuffer = NULL;
	size_t nBuffs = 0;

	*ppPkt = NULL;
	*pnBuffs = 0;

	for( pxDesc = pem->xRxBusyList.head; pxDesc != 0 && pxDesc->next != 0; pxDesc = pxDesc->next )
	{
		if( pxDesc->pxHWDesc->ui32CtrlStatus & DES0_RX_CTRL_OWN )
		{
			break; /* not done */
		}
		else if( pxDesc->pxHWDesc->ui32CtrlStatus & DES0_RX_STAT_FIRST_DESC )
		{ /* found the beg of a packet */
			nBuffs = 0;

			while( !( pxDesc->pxHWDesc->ui32CtrlStatus & DES0_RX_CTRL_OWN ) && pxDesc->pxHWDesc->pvBuffer1 )
			{ /* either way, we have to parse the packet */
				pxBuffer = pxPacketBuffer_to_NetworkBuffer( pxDesc->pxHWDesc->pvBuffer1 );
				pxBuffer->xDataLength = ( pxDesc->pxHWDesc->ui32CtrlStatus & DES0_RX_STAT_FRAME_LENGTH_M ) >> DES0_RX_STAT_FRAME_LENGTH_S;
				pxBuffer->pxNextBuffer = NULL;

				if( pxFirstBuffer )
				{
					pxFirstBuffer->pxNextBuffer = pxBuffer;
				}
				else
				{
					pxFirstBuffer = pxBuffer;
				}

				nBuffs++;
				EMAC_STATS_INC( BuffersReceived );

				if( pxDesc->pxHWDesc->ui32CtrlStatus & DES0_RX_STAT_LAST_DESC )
				{ /* end of packet */
					EMAC_STATS_ADD( BytesReceived, pxBuffer->xDataLength );
					*pnBuffs = nBuffs;

					if( pnStatus )
					{
						*pnStatus = ( ( ( uint64_t ) pxDesc->pxHWDesc->ui32ExtRxStatus ) << 32 ) | ( ( uint64_t ) pxDesc->pxHWDesc->ui32CtrlStatus );
					}

					break;
				}

				pxDesc = pxDesc->next;
			}

			break;
		}
	}

	if( nBuffs && pxFirstBuffer )
	{
		*ppPkt = pxFirstBuffer;
		return 0;
	}

	return -pdFREERTOS_ERRNO_ENOENT;
}

static BaseType_t emac_RxAckPacket( xEMAC * pem,
									const NetworkBufferDescriptor_t * pPkt,
									EMAC_DCPT_LIST * pRemList,
									EMAC_DCPT_LIST * pAddList )
{
	EMAC_DCPT_NODE * pEDcpt;
	EMAC_DCPT_NODE * prev, * next;
	int nAcks;
	int pktFound;

	prev = next = 0;
	nAcks = pktFound = 0;

	for( pEDcpt = pRemList->head; pEDcpt != 0 && pEDcpt->next != 0; pEDcpt = next )
	{
		if( ( pEDcpt->pxHWDesc->ui32CtrlStatus & DES0_RX_STAT_FIRST_DESC ) && ( pEDcpt->pxHWDesc->pvBuffer1 == pPkt->pucEthernetBuffer ) )
		{ /* found the beg of a packet */
			pktFound = 1;

			if( pEDcpt->pxHWDesc->ui32CtrlStatus & DES0_RX_CTRL_OWN )
			{
				break; /* hw not done with it */
			}

			if( !( pEDcpt->pxHWDesc->ui32CtrlStatus & DES0_RX_STAT_LAST_DESC ) )
			{
				break;
			}

			next = pEDcpt->next;
			EMAC_ListRemove( pRemList, pEDcpt, prev );
			EMAC_ListAddTail( pAddList, pEDcpt, DES0_RX_CTRL_OWN ); /* ack this node */
			nAcks++;
			pPkt = pPkt->pxNextBuffer;

			if( !pPkt )
			{
				break; /* done */
			}
		}
		else
		{
			prev = pEDcpt;
			next = pEDcpt->next;
		}
	}

	return nAcks ? 0 : ( pktFound ? -pdFREERTOS_ERRNO_EAGAIN : -pdFREERTOS_ERRNO_ENOENT );
}

/*
 * @brief Append buffers to DMA RX descriptors list
 * Should be called with ISRs disabled
 */
BaseType_t emac_AppendRxBusyList( xEMAC * pem,
								  EMAC_DCPT_LIST * pBusyList,
								  EMAC_DCPT_LIST * pNewList,
								  const uint32_t validControl )
{
	EMAC_DCPT_NODE * head, * tail, * pN;
	size_t newNodes = EMAC_ListCount( pNewList );
	size_t oldNodes = EMAC_ListCount( pBusyList );

	if( newNodes == 0 )
	{
		return -pdFREERTOS_ERRNO_EINVAL;
	}

	configASSERT( oldNodes );
	tail = pBusyList->tail;
	configASSERT( tail->pxHWDesc->pvBuffer1 == 0 );

	if( newNodes > 1 )
	{
		head = EMAC_ListRemoveHead( pNewList );
		head->pxHWDesc->ui32CtrlStatus &= ~DES0_RX_CTRL_OWN; /* not hw owned yet! */

		/* add all the required new descriptors/buffers */
		while( ( pN = EMAC_ListRemoveHead( pNewList ) ) )
		{
			configASSERT( pN->pxHWDesc->pvBuffer1 );
			EMAC_ListAddTail( pBusyList, pN, validControl );
		}

		/* replace the prev tail */
		tail->pxHWDesc->pvBuffer1 = head->pxHWDesc->pvBuffer1;
		tail->pxHWDesc->ui32CtrlStatus = head->pxHWDesc->ui32CtrlStatus;
		tail->pxHWDesc->ui32Count = head->pxHWDesc->ui32Count;

		head->pxHWDesc->ui32CtrlStatus = 0;
		head->pxHWDesc->pvBuffer1 = 0;
		EMAC_ListAddTail( pBusyList, head, validControl );  /* end properly, with EOWN=0; */

		tail->pxHWDesc->ui32CtrlStatus |= DES0_TX_CTRL_OWN; /* ready to go! */
	}
	else
	{
		head = EMAC_ListRemoveHead( pNewList );
		head->pxHWDesc->ui32CtrlStatus &= ~DES0_TX_CTRL_OWN; /* not hw owned yet! */

		/* replace the prev tail */
		tail->pxHWDesc->pvBuffer1 = head->pxHWDesc->pvBuffer1;
		tail->pxHWDesc->ui32CtrlStatus = head->pxHWDesc->ui32CtrlStatus;
		tail->pxHWDesc->ui32Count = head->pxHWDesc->ui32Count;

		head->pxHWDesc->ui32CtrlStatus = 0;
		head->pxHWDesc->pvBuffer1 = 0;
		EMAC_ListAddTail( pBusyList, head, validControl );  /* end properly, with EOWN=0; */

		tail->pxHWDesc->ui32CtrlStatus |= DES0_TX_CTRL_OWN; /* ready to go! */
	}

	return( oldNodes + newNodes );
}

/*
 * Should be called with ISRs disabled
 */
static BaseType_t EMAC_RxAcknowledgePacket( xEMAC * pem,
											const NetworkBufferDescriptor_t * pBuff,
											portBASE_TYPE bGetNewBuffers )
{
	BaseType_t xRc;
	EMAC_DCPT_NODE * pxDesc;

	EMAC_DCPT_LIST * pAckList;
	EMAC_DCPT_LIST * pStickyList;
	uint8_t ackList[ ( EMAC_DCPT_LIST_ALIGN - 1 ) + sizeof( EMAC_DCPT_LIST ) ];
	uint8_t stickyList[ ( EMAC_DCPT_LIST_ALIGN - 1 ) + sizeof( EMAC_DCPT_LIST ) ];

	if( !pBuff )
	{
		return -pdFREERTOS_ERRNO_EINVAL;
	}

	pAckList = EMAC_ListInit( _EthAlignAdjust( ackList ) );
	pStickyList = EMAC_ListInit( _EthAlignAdjust( stickyList ) );

	xRc = emac_RxAckPacket( pem, pBuff, &pem->xRxBusyList, pAckList );

	while( ( pxDesc = EMAC_ListRemoveHead( pAckList ) ) )
	{
		/* add it to the busy list... */
		if( bGetNewBuffers || ( pxDesc->pxHWDesc->pvBuffer1 == 0 ) )
		{
			pxDesc->pxHWDesc->pvBuffer1 = 0;
			EMAC_ListAddTail( &pem->xRxFreeList, pxDesc, DES0_RX_CTRL_OWN );
		}
		else
		{
			pxDesc->pxHWDesc->ui32CtrlStatus |= DES0_RX_CTRL_OWN;
			EMAC_ListAddTail( pStickyList, pxDesc, DES0_RX_CTRL_OWN );
		}
	}

	if( !EMAC_ListIsEmpty( pStickyList ) )
	{
		emac_AppendRxBusyList( pem, &pem->xRxBusyList, pStickyList, DES0_RX_CTRL_OWN ); /* append the descriptors that have valid buffers */
	}

	return xRc;
}

/* returns a pending RX packet if exists */
NetworkBufferDescriptor_t * emac_receive_packet( xEMAC * pem )
{
	BaseType_t xRc;
	NetworkBufferDescriptor_t * pxBuffer;
	NetworkBufferDescriptor_t * pxFirstBuffer = NULL;
	NetworkBufferDescriptor_t * pxPrevBuffer = NULL;
	size_t nBufs;
	uint64_t xStatus;
	uint32_t xStatus1;

	pxBuffer = NULL;
	nBufs = 0;
	ipconfigEMAC_DESC_LOCK();
	{
		do
		{
			xRc = emac_RxGetPacket( pem, &pxBuffer, &nBufs, &xStatus );

			if( xRc < 0 )
			{ /* done, no more packets */
				xRc = -pdFREERTOS_ERRNO_EBUSY;
			}
			else /* available packet; minimum check */
			{
				xStatus1 = xStatus & 0xffffffffuL;

				if( xStatus1 & DES0_RX_STAT_ERR )
				{ /* corrupted packet; discrd/re-insert it */
					xRc = -pdFREERTOS_ERRNO_EFTYPE;
					EMAC_STATS_INC( BadPackets );
				}
				else
				{
					xRc = 0;
				}
			}

			if( xRc == 0 )
			{ /* valid ETH packet; */
				if( nBufs > 1 )
				{
					EMAC_STATS_ADD( MultiBuffersReceived, nBufs );
				}

				EMAC_RxAcknowledgePacket( pem, pxBuffer, pdTRUE );

				if( !pxFirstBuffer )
				{
					pxFirstBuffer = pxBuffer;
				}
				else
				{
					pxPrevBuffer->pxNextBuffer = pxBuffer;

					while( pxBuffer->pxNextBuffer )
					{
						pxBuffer = pxBuffer->pxNextBuffer;
					}
				}

				pxPrevBuffer = pxBuffer;
			}
			else
			{
				/* failure; acknowledge the packet and re-insert */
				if( pxBuffer )
				{ /* we have a failed packet available */
					EMAC_RxAcknowledgePacket( pem, pxBuffer, pdFALSE );
				}

				break;
			}
		} while( 1 );
	}
	ipconfigEMAC_DESC_UNLOCK();

	return pxFirstBuffer;
}

/*
 * Should be called with ISRs disabled
 */
BaseType_t emac_RxBuffersAppend( xEMAC * pem,
								 size_t nBuffs )
{
	EMAC_DCPT_NODE * pEDcpt;
	BaseType_t xRc;
	EMAC_DCPT_LIST * pNewList;
	size_t n;
	NetworkBufferDescriptor_t * pxNewBuffer;

	uint8_t newList[ ( EMAC_DCPT_LIST_ALIGN - 1 ) + sizeof( EMAC_DCPT_LIST ) ];

	pNewList = EMAC_ListInit( _EthAlignAdjust( newList ) );

	xRc = 0;

	if( nBuffs == 0 )
	{
		nBuffs = UINT_MAX;
	}

	for( n = 0; n < nBuffs; n++ )
	{
		ipconfigEMAC_DESC_LOCK();
		{
			pEDcpt = EMAC_ListRemoveHead( &pem->xRxFreeList );
		}
		ipconfigEMAC_DESC_UNLOCK();

		if( !pEDcpt )
		{
			break;
		}

		if( pEDcpt->pxHWDesc->pvBuffer1 == 0 )
		{
			pxNewBuffer = pxGetNetworkBufferWithDescriptor( ipTOTAL_ETHERNET_FRAME_SIZE, 0 );

			if( !pxNewBuffer )
			{
				ipconfigEMAC_DESC_LOCK();
				{
					EMAC_ListAddTail( &pem->xRxFreeList, pEDcpt, DES0_RX_CTRL_OWN );
				}
				ipconfigEMAC_DESC_UNLOCK();
				break;
			}

			pEDcpt->pxHWDesc->pvBuffer1 = pxNewBuffer->pucEthernetBuffer;
			pEDcpt->pxHWDesc->ui32Count = DES1_RX_CTRL_CHAINED |
										  ( EMAC_RX_BUFF_SIZE << DES1_RX_CTRL_BUFF1_SIZE_S );
		}

		/* ok valid descriptor */
		/* pas it to hw, always use linked descriptors */
		pEDcpt->pxHWDesc->ui32CtrlStatus = DES0_RX_CTRL_OWN; /* The descriptor is initially owned by the DMA */
		EMAC_ListAddTail( pNewList, pEDcpt, DES0_RX_CTRL_OWN );
	}

	if( !EMAC_ListIsEmpty( pNewList ) )
	{
		ipconfigEMAC_DESC_LOCK();
		{
			xRc = emac_AppendRxBusyList( pem, &pem->xRxBusyList, pNewList, DES0_RX_CTRL_OWN );

			if( EMACRxDMADescriptorListGet( EMAC0_BASE ) == NULL )
			{
				EMACRxDMADescriptorListSet( EMAC0_BASE, pem->xRxBusyList.head->pxHWDesc );
			}

			EMACRxDMAPollDemand( EMAC0_BASE );
		}
		ipconfigEMAC_DESC_UNLOCK();
	}
	else
	{
		xRc = -pdFREERTOS_ERRNO_ENOBUFS;
	}

	return xRc;
}

/*
 * @brief Check RX DMA list for buffer arrival
 */
BaseType_t emac_check_rx( xEMAC * pem )
{
	NetworkBufferDescriptor_t * pMsg;

	pMsg = emac_receive_packet( pem );

	if( !pMsg )
	{
		return 0;
	}

	passEthMessages( pem, pMsg );
	return 1;
}

/*
 * @brief Add more buffers to RX list
 * Should be called with ISRs disabled
 */
BaseType_t emac_schedule_newbuffers( xEMAC * pem )
{
	size_t nBufScheduled, newBuffers;
	BaseType_t xRc = 0;

	ipconfigEMAC_DESC_LOCK();
	{
		nBufScheduled = _EnetDescriptorsCount( &pem->xRxBusyList, pdTRUE );
	}
	ipconfigEMAC_DESC_UNLOCK();

	if( nBufScheduled <= ipconfigNIC_N_RX_LOWTH )
	{
		newBuffers = ipconfigNIC_N_RX_DESC - nBufScheduled;

		if( newBuffers )
		{
			xRc = emac_RxBuffersAppend( pem, newBuffers );

			if( xRc < 0 )
			{
				xRc = nBufScheduled;
			}
		}
	}
	else
	{
		xRc = nBufScheduled;
	}

	return xRc;
}

/*
 * @brief rebuild already involved rx-DMA lists
 */
BaseType_t emac_reschedule_rxbusy( xEMAC * pem )
{
	EMAC_DCPT_NODE * pN, * pNp = NULL, * next;
	size_t xLen;

	ipconfigEMAC_DESC_LOCK();
	{
		xLen = EMAC_ListCount( &pem->xRxBusyList );
		configASSERT( !( ( xLen < 2 ) && ( EMAC_ListCount( &pem->xRxFreeList ) < ipconfigNIC_N_RX_DESC ) ) );

		for( pN = pem->xRxBusyList.head; pN && ( pN->next != 0 ); pN = next )
		{
			if( pN->pxHWDesc->pvBuffer1 == 0 )
			{
				next = pN->next;
				EMAC_ListRemove( &pem->xRxBusyList, pN, pNp );
				EMAC_ListAddTail( &pem->xRxFreeList, pN, DES0_RX_CTRL_OWN );
			}
			else
			{
				pN->pxHWDesc->ui32CtrlStatus = DES0_RX_CTRL_OWN;
				pNp = pN;
				next = pN->next;
			}
		}

		EMACRxDMADescriptorListSet( EMAC0_BASE, pem->xRxBusyList.head->pxHWDesc );
		EMACRxDMAPollDemand( EMAC0_BASE );
	}
	ipconfigEMAC_DESC_UNLOCK();
	return 0;
}
