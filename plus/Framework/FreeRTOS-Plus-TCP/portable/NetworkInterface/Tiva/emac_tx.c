/* Standard includes. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "inc/hw_memmap.h"
#include "inc/hw_emac.h"

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

#ifndef EMAC_PUBLIC_DATA
	static
#endif
tEMACDMADescriptor ptxSegments[ ipconfigNIC_N_TX_DESC + 1 ]
__attribute__( ( aligned( 4 ) ) );

#ifdef EMAC_PACKETS_OWNSECTION
	extern uint8_t
	__attribute__( ( aligned( 4 ) ) )
	ptxSpace[ ipconfigNIC_N_TX_DESC * ipconfigTX_UNIT_SIZE ];
#else
	static uint8_t ptxSpace[ ipconfigNIC_N_TX_DESC * ipconfigTX_UNIT_SIZE ]
	__attribute__( ( aligned( 4 ) ) );
#endif

EMAC_DCPT_NODE xTxDesc[ ipconfigNIC_N_TX_DESC + 1 ];

#define TX_CTL_FIELDS    ( DES0_TX_CTRL_INTERRUPT | DES0_TX_CTRL_CHAINED | DES0_TX_CTRL_IP_ALL_CKHSUMS | DES0_TX_CTRL_OWN )

void clean_dma_txdescs( xEMAC * pem )
{
	int index;
	uint8_t * ucTxBuffer;

	/* Clear all TX descriptors and assign memory to each descriptor.
	 * "tx_space" points to the first available TX buffer. */
	ucTxBuffer = ptxSpace;

	memset( ptxSpace, '\0', ipconfigNIC_N_TX_DESC * ipconfigTX_UNIT_SIZE );

	EMAC_ListInit( &pem->xTxFreeList );
	EMAC_ListInit( &pem->xTxBusyList );
	vListInitialise( &pem->xTxQueue );

	memset( &ptxSegments[ ipconfigNIC_N_TX_DESC ], 0, sizeof( tEMACDMADescriptor ) );
	ptxSegments[ ipconfigNIC_N_TX_DESC ].ui32CtrlStatus = DES0_TX_CTRL_CHAINED;
	xTxDesc[ ipconfigNIC_N_TX_DESC ].pxHWDesc = &ptxSegments[ ipconfigNIC_N_TX_DESC ];

	EMAC_ListAddHead( &pem->xTxBusyList, &xTxDesc[ ipconfigNIC_N_TX_DESC ] ); /* the busy list must always have a dummy sw owned tail descriptor */

	for( index = 0; index < ipconfigNIC_N_TX_DESC; index++ )
	{
		/*Use linked list rather than linear list */
		ptxSegments[ index ].ui32Count = 0;
		ptxSegments[ index ].ui32CtrlStatus = DES0_TX_CTRL_CHAINED;
		/*Transmit buffer address */
		ptxSegments[ index ].pvBuffer1 = ucTxBuffer;
		/*Transmit status vector */
		xTxDesc[ index ].pxHWDesc = &ptxSegments[ index ];
		EMAC_ListAddTail( &pem->xTxFreeList, &xTxDesc[ index ], TX_CTL_FIELDS );

		ucTxBuffer += ipconfigTX_UNIT_SIZE;
	}

	EMACTxDMADescriptorListSet( EMAC0_BASE, pem->xTxBusyList.head->pxHWDesc );
}

/*
 * @brief Append buffers to DMA TX descriptors list
 * Should be called with ISRs disabled
 */
BaseType_t emac_AppendTxBusyList( xEMAC * pem,
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
		head->pxHWDesc->ui32CtrlStatus &= ~DES0_TX_CTRL_OWN; /* not hw owned yet! */

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

		head->pxHWDesc->ui32CtrlStatus = DES0_TX_CTRL_CHAINED;
		head->pxHWDesc->pvBuffer1 = 0;
		head->pxHWDesc->ui32Count = 0;
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

		head->pxHWDesc->ui32CtrlStatus = DES0_TX_CTRL_CHAINED;
		head->pxHWDesc->pvBuffer1 = 0;
		head->pxHWDesc->ui32Count = 0;
		EMAC_ListAddTail( pBusyList, head, validControl );  /* end properly, with EOWN=0; */

		tail->pxHWDesc->ui32CtrlStatus |= DES0_TX_CTRL_OWN; /* ready to go! */
	}

	return( oldNodes + newNodes );
}

static void emac_TxSchedList( xEMAC * pem,
							  EMAC_DCPT_LIST * pList )
{
	if( !EMAC_ListIsEmpty( pList ) )
	{
		emac_AppendTxBusyList( pem, &pem->xTxBusyList, pList, 0xffffffffuL );

		if( EMACTxDMADescriptorListGet( EMAC0_BASE ) == NULL )
		{ /* 1st time transmission! */
			/*Starting address of TX descriptor table */
			EMACTxDisable( EMAC0_BASE );
			EMACTxDMADescriptorListSet( EMAC0_BASE, pem->xTxBusyList.head->pxHWDesc );
			EMACTxEnable( EMAC0_BASE );
		}

		/* Tell the transmitter to start (in case it had stopped). */
		EMACTxDMAPollDemand( EMAC0_BASE );
	}
}

BaseType_t emac_schedule_buffer( xEMAC * pem,
								 NetworkBufferDescriptor_t * pxBuffer,
								 EMAC_DCPT_LIST * pList )
{
	BaseType_t xRc = 0;
	EMAC_DCPT_NODE * pxDesc;

	pxDesc = EMAC_ListRemoveHead( &pem->xTxFreeList );

	if( pxDesc )
	{
		/* Write the number of bytes to send */
		pxDesc->pxHWDesc->ui32Count = pxBuffer->xDataLength;
		/* Set SOP and EOP flags since the data fits in a single buffer */
		pxDesc->pxHWDesc->ui32CtrlStatus = DES0_TX_CTRL_FIRST_SEG | DES0_TX_CTRL_LAST_SEG | DES0_TX_CTRL_IP_ALL_CKHSUMS | DES0_TX_CTRL_CHAINED | DES0_TX_CTRL_INTERRUPT;
		/* Copy the address of the buffer. */
		pxDesc->pxHWDesc->pvBuffer1 = pxBuffer->pucEthernetBuffer;
		/*Give the ownership of the descriptor to the DMA */
		pxDesc->pxHWDesc->ui32CtrlStatus |= DES0_TX_CTRL_OWN;

		EMAC_STATS_INC( BuffersQueued );
		EMAC_STATS_ADD( BytesSent, pxBuffer->xDataLength );

		EMAC_ListAddTail( pList, pxDesc, 0xffffffffuL );
	}
	else
	{
		xRc = -pdFREERTOS_ERRNO_ENOSPC;
	}

	return xRc;
}

/*
 * Should be called at ISR level
 */
BaseType_t EMAC_TxSendPacket( xEMAC * pem,
							  NetworkBufferDescriptor_t * pxFirstBuffer )
{
	NetworkBufferDescriptor_t * pxBuffer;
	EMAC_DCPT_LIST * pNewList;
	BaseType_t xRc = 0;
	uint8_t newList[ ( EMAC_DCPT_LIST_ALIGN - 1 ) + sizeof( EMAC_DCPT_LIST ) ];
	EMAC_DCPT_NODE * pN;

	pNewList = EMAC_ListInit( _EthAlignAdjust( newList ) );

	for( pxBuffer = pxFirstBuffer; pxBuffer != NULL; )
	{
		if( xValidLength( pxBuffer->xDataLength ) )
		{
			xRc = emac_schedule_buffer( pem, pxBuffer, pNewList );

			if( xRc < 0 )
			{
				break;
			}
		}
		else
		{
			xRc = -pdFREERTOS_ERRNO_EBADE;
			break;
		}

		pxBuffer = pxBuffer->pxNextBuffer;
	}

	if( xRc >= 0 )
	{
		emac_TxSchedList( pem, pNewList );
	}
	else if( !EMAC_ListIsEmpty( pNewList ) )
	{ /* we failed, put back the removed nodes */
		for( pN = pNewList->head; pN != 0; pN = pN->next )
		{
			pN->pxHWDesc->pvBuffer1 = 0;
		}

		EMAC_ListAppendTail( &pem->xTxFreeList, pNewList, DES0_TX_CTRL_CHAINED );
	}

	return xRc;
}

static BaseType_t emac_AckTxPacket( xEMAC * pem,
									EMAC_DCPT_LIST * pRemList,
									EMAC_DCPT_LIST * pAddList )
{
	EMAC_DCPT_NODE * pEDcpt;
	EMAC_DCPT_NODE * prev, * next;
	int nAcks;
	NetworkBufferDescriptor_t * pxBuffer;

	prev = next = 0;
	nAcks = 0;

	for( pEDcpt = pRemList->head; pEDcpt != 0 && pEDcpt->next != 0; pEDcpt = next )
	{
		if( ( pEDcpt->pxHWDesc->ui32CtrlStatus & DES0_TX_CTRL_OWN ) == 0 )
		{
			next = pEDcpt->next;

			if( pEDcpt->pxHWDesc->ui32CtrlStatus & DES0_TX_STAT_ERR )
			{
				EMAC_STATS_INC( TxHwErrors );
			}

			pxBuffer = pxPacketBuffer_to_NetworkBuffer( pEDcpt->pxHWDesc->pvBuffer1 );
			configASSERT( pxBuffer );
			pEDcpt->pxHWDesc->pvBuffer1 = 0;
			vNetworkBufferReleaseFromISR( pxBuffer );
			EMAC_ListRemove( pRemList, pEDcpt, prev );
			EMAC_ListAddTail( pAddList, pEDcpt, DES0_TX_CTRL_CHAINED ); /* ack this node */

			EMAC_STATS_INC( BuffersSent );

			nAcks++;
		}
		else
		{
			prev = pEDcpt;
			next = pEDcpt->next;
		}
	}

	return nAcks ? 0 : -pdFREERTOS_ERRNO_ENOENT;
}

/*
 * @brief rocess the transmit DMA list, freeing any buffers that have been transmitted since our last interrupt.
 * Occured at ISR level only
 */
void emac_ack_tx( xEMAC * pem )
{
	NetworkBufferDescriptor_t * pxBuffer;
	BaseType_t xRc;

	emac_AckTxPacket( pem, &pem->xTxBusyList, &pem->xTxFreeList );

	/* schedule the next packet */
	while( listCURRENT_LIST_LENGTH( &pem->xTxQueue ) > 0U )
	{
		pxBuffer = ( NetworkBufferDescriptor_t * ) listGET_OWNER_OF_HEAD_ENTRY( &pem->xTxQueue );
		xRc = EMAC_TxSendPacket( pem, pxBuffer );

		if( xRc == 0 )
		{
			uxListRemove( &pxBuffer->xBufferListItem );
		}
		else if( xRc == -pdFREERTOS_ERRNO_ENOSPC )
		{ /* no more descriptors */
			EMAC_STATS_INC( TxDmaNoBuffers );
			break;
		}
		else if( xRc < 0 )
		{ /* it must be an error */
			uxListRemove( &pxBuffer->xBufferListItem );
			vNetworkBufferReleaseFromISR( pxBuffer );

			EMAC_STATS_INC( BuffersDropped );
			break;
		}
	}
}

/*
 * Occured at Task level only
 */
BaseType_t emac_TxPendingPackets( xEMAC * pem,
								  uint32_t * ppScheduled,
								  size_t * xSize )
{
	NetworkBufferDescriptor_t * pxBuffer;
	BaseType_t xRc;
	size_t xMax = 0;

	if( xSize )
	{
		xMax = *xSize;
		*xSize = 0;
	}

	ipconfigEMAC_DESC_LOCK();

	while( listCURRENT_LIST_LENGTH( &pem->xTxQueue ) > 0U )
	{
		pxBuffer = ( NetworkBufferDescriptor_t * ) listGET_OWNER_OF_HEAD_ENTRY( &pem->xTxQueue );
		xRc = EMAC_TxSendPacket( pem, pxBuffer );

		if( xRc == -pdFREERTOS_ERRNO_ENOSPC )
		{
			ipconfigEMAC_DESC_UNLOCK();
			return xRc;
		}

		EMAC_STATS_INC( PendingTransmissions );
		/* on way or another we're done with this packet */
		uxListRemove( &pxBuffer->xBufferListItem );

		if( xRc != 0 )
		{ /* not transmitted */
			vNetworkBufferReleaseFromISR( pxBuffer );
			EMAC_STATS_INC( BuffersDropped );
			break;
		}

		if( xSize && ( xMax > 1 ) )
		{
			*ppScheduled++ = ( uint32_t ) pxBuffer;
			*ppScheduled++ = ( uint32_t ) pxBuffer->pucEthernetBuffer;
			*ppScheduled++ = pxBuffer->xDataLength;
			*xSize += 3;
			xMax -= 3;
		}
	}

	ipconfigEMAC_DESC_UNLOCK();

	return 0;
}

portBASE_TYPE xNetworkInterfaceOutputCore( xEMAC * pem,
										   NetworkBufferDescriptor_t * const pxFirst )
{
	BaseType_t xRc;
	NetworkBufferDescriptor_t * pxBuffer = pxFirst;

	#if ( ipconfigHAS_DEBUG_PRINTF == 1 )
		uint32_t xAddrReport[ 24 ];
		size_t xSize = 24, i = 0;
	#endif
	extern TaskHandle_t xEMACTaskHandle;

	if( xEMACTaskHandle && pem->phyLinkPresent && ( pem->phyLinkStatus & ELS_UP ) )
	{
		/* transmit the pending packets...don't transmit out of order */
		#if ( ipconfigHAS_DEBUG_PRINTF == 1 )
			xRc = emac_TxPendingPackets( pem, xAddrReport, &xSize );

			if( xTCPWindowLoggingLevel > 3 )
			{
				if( xSize )
				{
					while( xSize > 1 )
					{
						FreeRTOS_debug_printf( ( "EMAC: %x(%x) buffer scheduled for transmission: %u bytes\n", xAddrReport[ i ], xAddrReport[ i + 1 ], xAddrReport[ i + 2 ] ) );
						xSize -= 3;
						i += 3;
					}
				}
			}
		#else /* if ( ipconfigHAS_DEBUG_PRINTF == 1 ) */
			xRc = emac_TxPendingPackets( pem, NULL, NULL );
		#endif /* if ( ipconfigHAS_DEBUG_PRINTF == 1 ) */

		while( pxBuffer && ( xRc == 0 ) )
		{ /* can schedule some packets */
			ipconfigEMAC_DESC_LOCK();
			{
				xRc = EMAC_TxSendPacket( pem, pxBuffer );
			}
			ipconfigEMAC_DESC_UNLOCK();

			if( ( xRc < 0 ) && ( xRc != -pdFREERTOS_ERRNO_ENOSPC ) )
			{                                                   /* no longer in our queue */
				vReleaseNetworkBufferAndDescriptor( pxBuffer ); /* tx-path normally doesn't use linked buffers so it is safe to return */
				EMAC_STATS_INC( BuffersDropped );
				return xRc;
			}

			if( xRc == -pdFREERTOS_ERRNO_ENOSPC )
			{
				break;
			}

			if( xTCPWindowLoggingLevel > 3 )
			{
				FreeRTOS_debug_printf( ( "EMAC: %x(%x) buffer scheduled for transmission: %u bytes\n", pxBuffer, pxBuffer->pucEthernetBuffer, pxBuffer->xDataLength ) );
			}

			pxBuffer = pxBuffer->pxNextBuffer;
		}

		/* queue what's left */
		ipconfigEMAC_DESC_LOCK();
		{
			while( pxBuffer )
			{
				vListInsertEnd( &pem->xTxQueue, &pxBuffer->xBufferListItem );
				pxBuffer = pxBuffer->pxNextBuffer;
				EMAC_STATS_INC( BuffersDeferQueued );
			}
		}
		ipconfigEMAC_DESC_UNLOCK();
		return pdTRUE;
	}
	else
	{
		/* No link. */
		vReleaseNetworkBufferAndDescriptor( pxBuffer );
		EMAC_STATS_INC( NolinkDrops );
		return pdFALSE;
	}
}

portBASE_TYPE xNetworkInterfaceOutput( NetworkBufferDescriptor_t * const pxFirst,
									   portBASE_TYPE bReleaseAfterSend )
{
	xEMAC * pem = &gEMAC;
	portBASE_TYPE xRc;

	/* actually this should not happen, so we decline to use such buffers */
	if( bReleaseAfterSend == pdFALSE )
	{
		return -pdFREERTOS_ERRNO_EINVAL;
	}

	/* as we implement live reset we should use barrier between EMAC and IP tasks */
	xSemaphoreTake( pem->xLock, portMAX_DELAY );
	xRc = xNetworkInterfaceOutputCore( pem, pxFirst );
	xSemaphoreGive( pem->xLock );
	return xRc;
}

static BaseType_t emac_PurgeTxDescriptors( xEMAC * pem,
										   EMAC_DCPT_LIST * pRemList,
										   EMAC_DCPT_LIST * pAddList )
{
	EMAC_DCPT_NODE * pEDcpt;
	size_t nAcks = 0;
	NetworkBufferDescriptor_t * pxBuffer;

	while( pRemList->head->pxHWDesc->pvBuffer1 )
	{
		pEDcpt = EMAC_ListRemoveHead( pRemList );
		pxBuffer = pxPacketBuffer_to_NetworkBuffer( pEDcpt->pxHWDesc->pvBuffer1 );
		vNetworkBufferReleaseFromISR( pxBuffer );
		EMAC_STATS_INC( BuffersDropped );
		pEDcpt->pxHWDesc->pvBuffer1 = 0;
		EMAC_ListAddTail( pAddList, pEDcpt, DES0_TX_CTRL_CHAINED );
		nAcks++;
	}

	return nAcks;
}

/*
 * @brief Drop all currently transmitting buffers
 */
BaseType_t emac_purge_tx( xEMAC * pem )
{
	BaseType_t xRc;
	NetworkBufferDescriptor_t * pxBuffer;

	ipconfigEMAC_DESC_LOCK();
	{
		xRc = emac_PurgeTxDescriptors( pem, &pem->xTxBusyList, &pem->xTxFreeList );

		while( listCURRENT_LIST_LENGTH( &pem->xTxQueue ) > 0U )
		{
			pxBuffer = ( NetworkBufferDescriptor_t * ) listGET_OWNER_OF_HEAD_ENTRY( &pem->xTxQueue );

			uxListRemove( &pxBuffer->xBufferListItem );
			vNetworkBufferReleaseFromISR( pxBuffer );
			EMAC_STATS_INC( BuffersDropped );

			xRc++;
		}
	}
	ipconfigEMAC_DESC_UNLOCK();
	return xRc;
}
