#ifndef _TIVA_MAC_H
	#define _TIVA_MAC_H

	#include <stdbool.h>
	#include "driverlib/emac.h"
	#include "dcpt_list.h"
	#include "tiva_phy.h"

/* Provide C++ Compatibility */
	#ifdef __cplusplus
		extern "C" {
	#endif

	#if !ipconfigTIVA_MAC_CORRECTLY_BINDED
		#error "Please add tiva_mac_ipconfig.h into your FreeRTOSIPconfig"
	#endif

	#define EMAC_RX_EVENT		0x01
	#define EMAC_CB_EVENT		0x02
	#define EMAC_ERR_EVENT		0x04
	#define EMAC_OVF_EVENT		0x08
	#define EMAC_TX_EVENT		0x10
	#define EMAC_RESET_EVENT	0x20
	#define EMAC_ALL_EVENT		0x3f

	#define ipconfigEMAC_DESC_LOCK_FROM_ISR()								\
	UBaseType_t uxSavedInterruptStatus = portSET_INTERRUPT_MASK_FROM_ISR();	\
	{
	#define ipconfigEMAC_DESC_UNLOCK_FROM_ISR()					 \
	portCLEAR_INTERRUPT_MASK_FROM_ISR( uxSavedInterruptStatus ); \
	}

	#define ipconfigEMAC_DESC_LOCK()	  portENTER_CRITICAL()
	#define ipconfigEMAC_DESC_UNLOCK()	  portEXIT_CRITICAL()

	typedef struct tEMAC_STAT
	{
		volatile uint64_t uBytesReceived;
		volatile uint64_t uBytesSent;
		volatile uint32_t uBadPackets;
		volatile uint32_t uBuffersSent;
		volatile uint32_t uBuffersQueued;
		volatile uint32_t uBuffersDeferQueued;
		volatile uint32_t uBuffersDropped;
		volatile uint32_t uBuffersReceived;
		volatile uint32_t uMultiBuffersReceived;
		volatile uint32_t uNolinkDrops;
		volatile uint32_t uTxDmaNoBuffers;
		volatile uint32_t uRxEventLost;
		volatile uint32_t uTxHwErrors;
		volatile uint32_t uIsrsRx;
		volatile uint32_t uIsrsTx;
		volatile uint32_t uIsrsAbnormal;
		volatile uint32_t uIsrsRxNoBuff;
		volatile uint32_t uIsrsRxOvfErr;
		volatile uint32_t uNoBuffers;
		volatile uint32_t uPhyStateChanges;
		volatile uint32_t uPendingTransmissions;
		volatile uint32_t uLiveResets;
	} xEMAC_STAT;

	#ifdef ipconfigEMAC_USE_STAT

		extern xEMAC_STAT emac_stat;

/**
 * Note: This rather weird construction where we invoke the macro with the
 * name of the field minus its Hungarian prefix is a workaround for a problem
 * experienced with GCC which does not like concatenating tokens after an
 * operator, specifically '.' or '->', in a macro.
 */
		#define EMAC_STATS_INC( x )			do { emac_stat.u ## x++; } while( 0 )
		#define EMAC_STATS_DEC( x )			do { emac_stat.u ## x--; } while( 0 )
		#define EMAC_STATS_ADD( x, inc )	do { emac_stat.u ## x += ( inc ); } while( 0 )
		#define EMAC_STATS_SUB( x, dec )	do { emac_stat.u ## x -= ( dec ); } while( 0 )
	#else
		#define EMAC_STATS_INC( x )
		#define EMAC_STATS_DEC( x )
		#define EMAC_STATS_ADD( x, inc )
		#define EMAC_STATS_SUB( x, dec )
	#endif /* ifdef ipconfigEMAC_USE_STAT */

	typedef struct tEMAC
	{
		volatile uint32_t isr_events;    /* events to EMAC task */

		SemaphoreHandle_t xLock;         /* task interlock */

		bool phyLinkPresent;             /* is PHY found? */
		bool phyLinkNegotiation;         /* are we in negitation? */

		volatile uint32_t phyLinkStatus; /* current link status */
		uint32_t phyLinkPrev;            /* previous value of link status */

		EMAC_DCPT_LIST xTxFreeList;      /* transmit list of free descriptors */
		EMAC_DCPT_LIST xTxBusyList;      /* transmit list of descriptors passed to the Tx Engine */
		                                 /* the xTxBusyList always ends with an sw owned descriptor */
		List_t xTxQueue;                 /* current TX queue; stores TX queued packets */

		EMAC_DCPT_LIST xRxFreeList;      /* receive list of free descriptors */
		EMAC_DCPT_LIST xRxBusyList;      /* receive list of descriptors passed to the Rx Engine */
	} xEMAC;

	extern xEMAC gEMAC;

/*
 * @brief Initialize DMA descriptors
 */
	BaseType_t emac_init_dma( xEMAC * pem );

/*
 * @brief Check RX DMA list for buffer arrival
 */
	BaseType_t emac_check_rx( xEMAC * pem );

/*
 * @brief Add more buffers to RX list
 * Should be called with ISRs disabled
 */
	BaseType_t emac_schedule_newbuffers( xEMAC * pem );

/*
 * @brief Drop all currently transmitting buffers
 */
	BaseType_t emac_purge_tx( xEMAC * pem );

/*
 * @brief rebuild already involved rx-DMA lists
 */
	BaseType_t emac_reschedule_rxbusy( xEMAC * pem );

/*
 * @brief rocess the transmit DMA list, freeing any buffers that have been transmitted since our last interrupt.
 * Occured at ISR level only
 */
	void emac_ack_tx( xEMAC * pem );


	/* Provide C++ Compatibility */
	#ifdef __cplusplus
		}
	#endif

#endif /* _TIVA_MAC_H */

/* *****************************************************************************
 * End of File
 */
