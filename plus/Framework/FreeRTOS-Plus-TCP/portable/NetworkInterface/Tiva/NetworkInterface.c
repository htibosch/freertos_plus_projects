/* Standard includes. */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "inc/hw_ints.h"
#include "inc/hw_nvic.h"
#include "inc/hw_sysctl.h"
#include "inc/hw_types.h"
#include "inc/hw_memmap.h"
#include "inc/hw_emac.h"
#include "driverlib/cpu.h"
#include "driverlib/debug.h"
#include "driverlib/interrupt.h"
#include "driverlib/sysctl.h"
#include "driverlib/rom_map.h"

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

#if ipconfigEMAC_PHY_CONFIG & EMAC_PHY_TYPE_EXTERNAL_MII || ipconfigEMAC_PHY_CONFIG & EMAC_PHY_TYPE_EXTERNAL_RMII
	#error "At now only internal phy mode implemented"
#endif

xEMAC gEMAC;

#ifdef ipconfigEMAC_USE_STAT
	xEMAC_STAT emac_stat;
#endif

#ifdef ipconfigPHY_USE_STAT
	xPHY_STAT phy_stat;
#endif

/* Holds the handle of the task used as a deferred interrupt processor.  The
 * handle is used so direct notifications can be sent to the task for all EMAC/DMA
 * related interrupts. */
TaskHandle_t xEMACTaskHandle;

static void emac_HandlerTask( void * pvParameters );
static BaseType_t emac_LinkReconfigure( xEMAC * pem );
void vEMACRegisterCLICommands( void );

BaseType_t emac_hwinit( xEMAC * pem )
{
	uint16_t ui16Val;

	MAP_SysCtlPeripheralEnable( SYSCTL_PERIPH_EMAC0 );
	MAP_SysCtlPeripheralReset( SYSCTL_PERIPH_EMAC0 );
	#if !( ipconfigEMAC_PHY_CONFIG & EMAC_PHY_TYPE_EXTERNAL_MII || ipconfigEMAC_PHY_CONFIG & EMAC_PHY_TYPE_EXTERNAL_RMII )
		/* Enable and reset the internal PHY. */
		MAP_SysCtlPeripheralEnable( SYSCTL_PERIPH_EPHY0 );
		MAP_SysCtlPeripheralReset( SYSCTL_PERIPH_EPHY0 );
		pem->phyLinkPresent = true;
	#else
		/* If PHY is external then reset the PHY before configuring it */
		MAP_EMACPHYWrite( EMAC0_BASE, PHY_PHYS_ADDR, EPHY_BMCR,
						  EPHY_BMCR_MIIRESET );

		while( ( MAP_EMACPHYRead( EMAC0_BASE, PHY_PHYS_ADDR, EPHY_BMCR ) &
				 EPHY_BMCR_MIIRESET ) == EPHY_BMCR_MIIRESET )
		{
		}
	#endif /* if !( ipconfigEMAC_PHY_CONFIG & EMAC_PHY_TYPE_EXTERNAL_MII || ipconfigEMAC_PHY_CONFIG & EMAC_PHY_TYPE_EXTERNAL_RMII ) */

	/* Ensure the MAC is completed its reset. */
	while( !MAP_SysCtlPeripheralReady( SYSCTL_PERIPH_EMAC0 ) )
	{
	}

	/* Set the PHY type and configuration options. */
	MAP_EMACPHYConfigSet( EMAC0_BASE, ipconfigEMAC_PHY_CONFIG );
	/* Initialize and configure the MAC. */
	MAP_EMACInit( EMAC0_BASE, configCPU_CLOCK_HZ, EMAC_BCONFIG_PRIORITY_FIXED | EMAC_BCONFIG_FIXED_BURST, 16, 16, 0 );

	/* Set MAC hardware address */
	MAP_EMACAddrSet( EMAC0_BASE, 0, ipLOCAL_MAC_ADDRESS );

	/* Clear any stray PHY interrupts that may be set. */
	ui16Val = MAP_EMACPHYRead( EMAC0_BASE, EMAC_PHY_ADDR, EPHY_MISR1 );
	ui16Val = MAP_EMACPHYRead( EMAC0_BASE, EMAC_PHY_ADDR, EPHY_MISR2 );

	/* Configure and disable the link status change interrupt in the PHY as we poll for it. */
	ui16Val = MAP_EMACPHYRead( EMAC0_BASE, EMAC_PHY_ADDR, EPHY_SCR );
	ui16Val &= ~( EPHY_SCR_INTEN_EXT | EPHY_SCR_INTOE_EXT );
	MAP_EMACPHYWrite( EMAC0_BASE, EMAC_PHY_ADDR, EPHY_SCR, ui16Val );
	MAP_EMACPHYWrite( EMAC0_BASE, EMAC_PHY_ADDR, EPHY_MISR1, 0 );

	MAP_EMACIntDisable( EMAC0_BASE, EMAC_INT_PHY );

	/* Read the PHY interrupt status to clear any stray events. */
	ui16Val = MAP_EMACPHYRead( EMAC0_BASE, EMAC_PHY_ADDR, EPHY_MISR1 );

	/**
	 * Set MAC filtering options.  We receive all broadcast and mui32ticast
	 * packets along with those addressed specifically for us.
	 */
	MAP_EMACFrameFilterSet( EMAC0_BASE, ( EMAC_FRMFILTER_HASH_AND_PERFECT |
										  #if ipconfigUSE_IGMP == 1
											  EMAC_FRMFILTER_PASS_MULTICAST
										  #else
											  0
										  #endif
										  ) );

	/* Clear any pending MAC interrupts. */
	MAP_EMACIntClear( EMAC0_BASE, MAP_EMACIntStatus( EMAC0_BASE, false ) );

	/* Tell the PHY to start an auto-negotiation cycle. */
	#if ipconfigEMAC_PHY_CONFIG & EMAC_PHY_TYPE_EXTERNAL_MII || ipconfigEMAC_PHY_CONFIG & EMAC_PHY_TYPE_EXTERNAL_RMII
		MAP_EMACPHYWrite( EMAC0_BASE, EMAC_PHY_ADDR, EPHY_BMCR, ( EPHY_BMCR_SPEED |
																  EPHY_BMCR_DUPLEXM | EPHY_BMCR_ANEN | EPHY_BMCR_RESTARTAN ) );
	#else
		MAP_EMACPHYWrite( EMAC0_BASE, EMAC_PHY_ADDR, EPHY_BMCR, ( EPHY_BMCR_ANEN |
																  EPHY_BMCR_RESTARTAN ) );
	#endif
	pem->phyLinkNegotiation = true;
	emac_LinkReconfigure( pem );

	return 0;
}

static BaseType_t emac_LinkReconfigure( xEMAC * pem )
{
	BaseType_t xStatus;
	uint32_t mode, config;

	xStatus = EthPhyNegotiationComplete( false ); /* see if negotiation complete */

	if( xStatus == 0 )
	{
		pem->phyLinkStatus = EthPhyGetNegotiationResult( &config, &mode );

		if( pem->phyLinkStatus & ELS_UP )
		{
			/* negotiation succeeded */
			MAP_EMACConfigSet( EMAC0_BASE, config | EMAC_CONFIG_USE_MACADDR0, mode, EMAC_RX_BUFF_SIZE );
			/* TODO: clear hardware counters */
			xStatus = 0;
		}
	}
	else
	{
		pem->phyLinkStatus = ELS_DOWN;

		if( xStatus == -pdPHY_NEGOTIATION_INACTIVE )
		{
			PHY_STATS_INC( AN_errors );
		}
	}

	return xStatus;
}

static BaseType_t emac_WaitLS( xEMAC * pem,
							   TickType_t xMaxTime )
{
	const TickType_t xShortDelay = pdMS_TO_TICKS( 20UL );
	BaseType_t xReturn;
	TimeOut_t xTimeOut;
	TickType_t xRemainingTime;

	if( !pem->phyLinkPresent )
	{
		return -pdFREERTOS_ERRNO_ENODEV;
	}

	xRemainingTime = xMaxTime;
	vTaskSetTimeOutState( &xTimeOut );
	pem->phyLinkPrev = ( uint32_t ) -1;

	for( ; ; )
	{
		if( xTaskCheckForTimeOut( &xTimeOut, &xRemainingTime ) == pdTRUE )
		{
			xReturn = -pdFREERTOS_ERRNO_ETIMEDOUT;
			PHY_STATS_INC( NegTO );
			break;
		}

		if( pem->phyLinkNegotiation )
		{
			emac_LinkReconfigure( pem );
		}

		if( pem->phyLinkStatus & ELS_UP )
		{
			pem->phyLinkNegotiation = 0;
			xReturn = pdTRUE;
			break;
		}

		if( !pem->phyLinkNegotiation )
		{
			pem->phyLinkStatus = EthPhyGetLinkStatus();
		}

		vTaskDelay( xShortDelay );
	}

	return xReturn;
}

BaseType_t xNetworkInterfaceInitialise( void )
{
	BaseType_t xStatus;
	const TickType_t xWaitLinkDelay = portMAX_DELAY, xWaitRelinkDelay = portMAX_DELAY;
	xEMAC * pem = &gEMAC;

	FreeRTOS_printf( ( "EMAC: initialization\n" ) );

	/* Guard against the init function being called more than once. */
	if( xEMACTaskHandle == NULL )
	{
		memset( pem, 0, sizeof( xEMAC ) );
		pem->xLock = xSemaphoreCreateMutex();

		#if ( ipconfigEMAC_INCLUDE_CLI != 0 ) && ( ipconfigEMAC_USE_STAT != 0 )
			vEMACRegisterCLICommands();
		#endif
		xStatus = emac_hwinit( pem );

		if( xStatus != 0 )
		{
			FreeRTOS_printf( ( "EMAC: phy initialization failed....\n" ) );
			return xStatus;
		}

		emac_WaitLS( pem, xWaitLinkDelay );
		emac_init_dma( pem );

		/* Enable the Ethernet MAC transmitter and receiver. */
		MAP_EMACTxEnable( EMAC0_BASE );
		MAP_EMACRxEnable( EMAC0_BASE );

		/* The deferred interrupt handler task is created at the highest
		 * possible priority to ensure the interrupt handler can return directly
		 * to it.  The task's handle is stored in xEMACTaskHandle so interrupts can
		 * notify the task when there is something to process. */
		xTaskCreate( emac_HandlerTask, "EMAC", configEMAC_STACK_SIZE, pem, ipconfigNIC_TASK_PRIORITY, &xEMACTaskHandle );

		/* */
		/* Lower the priority of the Ethernet interrupt handler to less than */
		/* configMAX_SYSCALL_INTERRUPT_PRIORITY.  This is required so that the */
		/* interrupt handler can safely call the interrupt-safe FreeRTOS functions */
		/* if any. */
		/* */
		MAP_IntPrioritySet( INT_EMAC0, configETHERNET_INT_PRIORITY );

		/* Enable the Ethernet RX and TX interrupt source. */
		MAP_EMACIntEnable( EMAC0_BASE, ( EMAC_INT_RECEIVE | EMAC_INT_TRANSMIT |
										 EMAC_INT_RX_NO_BUFFER | EMAC_INT_RX_STOPPED | EMAC_INT_RX_OVERFLOW ) );
		/* Enable the Ethernet interrupt. */
		MAP_IntEnable( INT_EMAC0 );
	}
	else
	{
		/* Initialisation was already performed, just wait for the link. */
		emac_WaitLS( pem, xWaitRelinkDelay );
	}

	/* Only return pdTRUE when the Link Status of the PHY is high, otherwise the
	 * DHCP process and all other communication will fail. */

	return ( pem->phyLinkStatus & ELS_UP ) ? pdTRUE : pdFALSE;
}

/*-----------------------------------------------------------*/

#if ( ipconfigNETWORK_INTERFACE_ALLOCATES_BUFFERS == 1 )

	#ifdef EMAC_PACKETS_OWNSECTION
		extern uint8_t
		__attribute__( ( aligned( 16 ) ) )
		ucNetworkPackets[ ipconfigNUM_NETWORK_BUFFER_DESCRIPTORS * niBUFFER_1_PACKET_SIZE ];
	#else
		static uint8_t
		__attribute__( ( aligned( 16 ) ) )
		ucNetworkPackets[ ipconfigNUM_NETWORK_BUFFER_DESCRIPTORS * niBUFFER_1_PACKET_SIZE ];
	#endif
	void vNetworkInterfaceAllocateRAMToBuffers( NetworkBufferDescriptor_t pxNetworkBuffers[ ipconfigNUM_NETWORK_BUFFER_DESCRIPTORS ] )
	{
		uint8_t * ucRAMBuffer = &ucNetworkPackets[ 0 ];
		uint32_t ul;

		for( ul = 0; ul < ipconfigNUM_NETWORK_BUFFER_DESCRIPTORS; ul++ )
		{
			pxNetworkBuffers[ ul ].pucEthernetBuffer = ucRAMBuffer + ipBUFFER_PADDING;
			*( ( uint32_t * ) ucRAMBuffer ) = ( uint32_t ) ( &( pxNetworkBuffers[ ul ] ) );
			ucRAMBuffer += niBUFFER_1_PACKET_SIZE;
		}
	}

#endif /* ipconfigNETWORK_INTERFACE_ALLOCATED_BUFFERS */
/*-----------------------------------------------------------*/

/**
 * @brief Tiva C Ethernet MAC interrupt service routine
 **/
void vEthInterruptHandler( void )
{
	uint32_t status;
	BaseType_t xHigherPriorityTaskWoken = pdFALSE;
	BaseType_t doNotify = pdFALSE;
	xEMAC * pem = &gEMAC;

	status = MAP_EMACIntStatus( EMAC0_BASE, true );

	/* Update our debug interrupt counters. */
	if( status & EMAC_INT_ABNORMAL_INT )
	{
		EMAC_STATS_INC( IsrsAbnormal );
	}

	/* */
	/* If the interrupt really came from the Ethernet and not our */
	/* timer, clear it. */
	/* */
	if( status )
	{
		MAP_EMACIntClear( EMAC0_BASE, status );
	}

	/* */
	/* Disable the Ethernet interrupts.  Since the interrupts have not been */
	/* handled, they are not asserted.  Once they are handled by the Ethernet */
	/* interrupt task, it will re-enable the interrupts. */
	/* */

	/* Process the transmit DMA list, freeing any buffers that have been
	 * transmitted since our last interrupt.
	 */
	if( status & ( EMAC_INT_TRANSMIT | EMAC_INT_TX_STOPPED ) )
	{
		EMAC_STATS_INC( IsrsTx );
		emac_ack_tx( pem );
		#if ( ipconfigHAS_DEBUG_PRINTF == 1 )
			pem->isr_events |= EMAC_TX_EVENT;
			doNotify = pdTRUE;
		#endif
	}

	if( status & ( EMAC_INT_RECEIVE | EMAC_INT_RX_STOPPED ) )
	{
		EMAC_STATS_INC( IsrsRx );
		MAP_EMACIntDisable( EMAC0_BASE, EMAC_INT_RECEIVE | EMAC_INT_RX_STOPPED );

		pem->isr_events |= EMAC_RX_EVENT;
		doNotify = pdTRUE;
	}

	if( status & EMAC_INT_RX_NO_BUFFER )
	{
		EMAC_STATS_INC( IsrsRxNoBuff );

		MAP_EMACIntDisable( EMAC0_BASE, EMAC_INT_RX_NO_BUFFER );

		pem->isr_events |= EMAC_INT_RX_NO_BUFFER;
		doNotify = pdTRUE;
	}

	if( status & EMAC_INT_RX_OVERFLOW )
	{
		EMAC_STATS_INC( IsrsRxOvfErr );

		MAP_EMACIntDisable( EMAC0_BASE, EMAC_INT_RX_OVERFLOW );

		pem->isr_events |= EMAC_OVF_EVENT;
		doNotify = pdTRUE;
	}

	if( ( doNotify == pdTRUE ) && ( xEMACTaskHandle != NULL ) )
	{
		vTaskNotifyGiveFromISR( xEMACTaskHandle, &xHigherPriorityTaskWoken );
	}

	portEND_SWITCHING_ISR( xHigherPriorityTaskWoken );
}

/*
 * @brief Gracefully reset EMAC
 */
BaseType_t emac_live_reset( xEMAC * pem )
{
	BaseType_t xRc = 0;

	xSemaphoreTake( pem->xLock, portMAX_DELAY );
	MAP_EMACIntDisable( EMAC0_BASE, ( EMAC_INT_RECEIVE | EMAC_INT_TRANSMIT |
									  EMAC_INT_TX_STOPPED | EMAC_INT_RX_NO_BUFFER |
									  EMAC_INT_RX_STOPPED | EMAC_INT_PHY ) );
	EMAC_STATS_INC( LiveResets );

	MAP_SysCtlPeripheralReset( SYSCTL_PERIPH_EMAC0 );
	MAP_SysCtlPeripheralReset( SYSCTL_PERIPH_EPHY0 );

	/* Ensure the MAC is completed its reset. */
	while( !MAP_SysCtlPeripheralReady( SYSCTL_PERIPH_EMAC0 ) )
	{
	}

	emac_purge_tx( pem ); /* release buffers and descriptors in tx-path */

	vTaskDelay( 100 );

	xRc = emac_hwinit( pem );
	pem->isr_events = 0;

	configASSERT( ( xRc < 0 ? 0 : 1 ) );

	emac_reschedule_rxbusy( pem ); /* rebuild rxbusy list */

	while( emac_schedule_newbuffers( pem ) < 2 )
	{
		vTaskDelay( 10 );
	}

	xRc = emac_WaitLS( pem, pdMS_TO_TICKS( 10000 ) );

	if( xRc == pdTRUE )
	{
		/* Enable the Ethernet RX and TX interrupt source. */
		MAP_EMACIntEnable( EMAC0_BASE, ( EMAC_INT_RECEIVE | EMAC_INT_TRANSMIT |
										 EMAC_INT_RX_NO_BUFFER | EMAC_INT_RX_STOPPED | EMAC_INT_RX_OVERFLOW ) );
		/* Enable the Ethernet interrupt. */
		MAP_IntEnable( INT_EMAC0 );
	}
	else
	{
		PHY_STATS_INC( LinkTO );
	}

	xSemaphoreGive( pem->xLock );
	return xRc;
}

static void emac_HandlerTask( void * pvParameters )
{
	TimeOut_t xPhyTime;
	TickType_t xPhyRemTime;
	BaseType_t xResult = 0;

	#if ( ipconfigHAS_DEBUG_PRINTF == 1 )
		uint32_t newBufSent = 0, v;
	#endif
	uint32_t ev;
	size_t xErrors = 0;

	const TickType_t ulMaxBlockTime = pdMS_TO_TICKS( 50UL );
	xEMAC * pem = ( xEMAC * ) pvParameters;

	vTaskSetTimeOutState( &xPhyTime );
	xPhyRemTime = pdMS_TO_TICKS( PHY_LS_LOW_CHECK_TIME_MS );

	FreeRTOS_debug_printf( ( "EMAC task started\n" ) );

	for( ; ; )
	{
		if( ( pem->isr_events & EMAC_ALL_EVENT ) == 0 )
		{
			/* No events to process now, wait for the next. */
			ulTaskNotifyTake( pdTRUE, ulMaxBlockTime );
		}

		xResult = 0;
		ev = pem->isr_events;

		if( ( ev & ( EMAC_RX_EVENT | EMAC_ERR_EVENT | EMAC_OVF_EVENT ) ) != 0 )
		{
			xResult = emac_check_rx( pem );
			emac_schedule_newbuffers( pem );

			if( ( ev & ( EMAC_OVF_EVENT | EMAC_ERR_EVENT ) ) && ( ++xErrors > 3 ) )
			{
				/* we're out-of-sync */
				configASSERT( emac_live_reset( pem ) >= 0 );
				ev = 0;
				xErrors = 0;
			}
			else
			{
				if( xResult )
				{
					xErrors = 0;
				}

				ipconfigEMAC_DESC_LOCK();
				{
					pem->isr_events &= ~( EMAC_RX_EVENT | EMAC_ERR_EVENT | EMAC_OVF_EVENT );
					MAP_EMACIntEnable( EMAC0_BASE, ( EMAC_INT_RECEIVE | EMAC_INT_RX_NO_BUFFER | EMAC_INT_RX_STOPPED | EMAC_INT_RX_OVERFLOW ) );
				}
				ipconfigEMAC_DESC_UNLOCK();
			}
		}
		else
		{
			emac_schedule_newbuffers( pem );
		}

		if( ( ev & EMAC_RESET_EVENT ) != 0 )
		{
			configASSERT( emac_live_reset( pem ) >= 0 );
			ev = 0;
			xErrors = 0;
		}

		#if ( ipconfigHAS_DEBUG_PRINTF == 1 )
			if( ( ev & EMAC_TX_EVENT ) != 0 )
			{
				v = emac_stat.uBuffersSent;

				if( v != newBufSent )
				{
					if( xTCPWindowLoggingLevel > 3 )
					{
						FreeRTOS_debug_printf( ( "EMAC: %u buffers transmission finished\n", v - newBufSent ) );
					}

					newBufSent = v;
				}

				ipconfigEMAC_DESC_LOCK();
				{
					pem->isr_events &= ~( EMAC_TX_EVENT );
				}
				ipconfigEMAC_DESC_UNLOCK();
			}
		#endif /* if ( ipconfigHAS_DEBUG_PRINTF == 1 ) */

		if( ( ev & EMAC_CB_EVENT ) != 0 )
		{
			ipconfigEMAC_DESC_LOCK();
			{
				pem->isr_events &= ~EMAC_CB_EVENT;
			}
			ipconfigEMAC_DESC_UNLOCK();
			emac_schedule_newbuffers( pem );
		}

		if( xResult > 0 )
		{
			/* A packet was received. No need to check for the PHY status now,
			 * but set a timer to check it later on. */
			vTaskSetTimeOutState( &xPhyTime );
			xPhyRemTime = pdMS_TO_TICKS( PHY_LS_HIGH_CHECK_TIME_MS );
			xResult = 0;
		}
		else if( xTaskCheckForTimeOut( &xPhyTime, &xPhyRemTime ) != pdFALSE )
		{
			pem->phyLinkStatus = EthPhyGetLinkStatus();

			if( pem->phyLinkStatus != pem->phyLinkPrev )
			{
				EMAC_STATS_INC( PhyStateChanges );
				pem->phyLinkPrev = pem->phyLinkStatus;
				FreeRTOS_printf( ( "*** PHY LS now %x\n", pem->phyLinkStatus ) );

				if( ( pem->phyLinkStatus & ELS_UP ) == 0 )
				{
					FreeRTOS_debug_printf( ( "EMAC: Shutdown\n" ) );

					emac_purge_tx( pem );
					FreeRTOS_NetworkDown();
				}
			}

			vTaskSetTimeOutState( &xPhyTime );

			if( ( pem->phyLinkStatus & ELS_UP ) != 0 )
			{
				xPhyRemTime = pdMS_TO_TICKS( PHY_LS_HIGH_CHECK_TIME_MS );
			}
			else
			{
				xPhyRemTime = pdMS_TO_TICKS( PHY_LS_LOW_CHECK_TIME_MS );
			}
		}
	}
}

void emac_failed_to_get_nb()
{
	EMAC_STATS_INC( NoBuffers );
}


void emac_ack_buffer_released( void * pvBuffer )
{
	xEMAC * pem = &gEMAC;

	ipconfigEMAC_DESC_LOCK();
	{
		pem->isr_events |= EMAC_CB_EVENT;
	}
	ipconfigEMAC_DESC_UNLOCK();

	if( xEMACTaskHandle )
	{
		xTaskNotifyGive( xEMACTaskHandle );
	}
}

/*
 * @brief Get PHY status
 */
BaseType_t xGetPhyLinkStatus( void )
{
	if( gEMAC.phyLinkStatus & ELS_UP )
	{
		return pdTRUE;
	}

	return pdFALSE;
}
