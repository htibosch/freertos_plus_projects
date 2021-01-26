#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

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

#define PHY_NEG_INIT_TMO	 1      /* negotiation initiation timeout, ms. */

#define PHY_NEG_DONE_TMO	 2000   /* negotiation complete timeout, ms. */
                                    /* based on IEEE 802.3 Clause 28 Table 28-9 autoneg_wait_timer value (max 1s) */
#define PHY_RESET_CLR_TMO	 500    /* reset self clear timeout, ms. */
                                    /* IEEE 802.3 Clause 22 Table 22-7 and paragraph "22.2.4.1.1 Reset" (max 0.5s) */

/*
 * Check for negotiation procedure completition
 */
BaseType_t EthPhyNegotiationComplete( bool waitComplete )
{
	BaseType_t xRc;
	TimeOut_t xTimeOut;
	TickType_t xRemainingTime;

	uint16_t phyBMCon, phyStat = 0;

	phyBMCon = MAP_EMACPHYRead( EMAC0_BASE, EMAC_PHY_ADDR, EPHY_BMCR );

	if( phyBMCon & EPHY_BMCR_ANEN )
	{
		phyBMCon = MAP_EMACPHYRead( EMAC0_BASE, EMAC_PHY_ADDR, EPHY_BMCR );
		phyStat = MAP_EMACPHYRead( EMAC0_BASE, EMAC_PHY_ADDR, EPHY_BMSR );

		if( waitComplete )
		{
			if( phyBMCon & EPHY_BMCR_RESTARTAN )
			{ /* not started yet */
				xRemainingTime = pdMS_TO_TICKS( PHY_NEG_INIT_TMO );
				vTaskSetTimeOutState( &xTimeOut );

				do
				{
					phyBMCon = MAP_EMACPHYRead( EMAC0_BASE, EMAC_PHY_ADDR, EPHY_BMCR );

					if( xTaskCheckForTimeOut( &xTimeOut, &xRemainingTime ) == pdTRUE )
					{
						break;
					}
				} while( phyBMCon & EPHY_BMCR_RESTARTAN ); /* wait auto negotiation start */
			}

			if( !( phyBMCon & EPHY_BMCR_RESTARTAN ) )
			{ /* ok, started */
				xRemainingTime = pdMS_TO_TICKS( PHY_NEG_DONE_TMO );
				vTaskSetTimeOutState( &xTimeOut );

				do
				{
					phyStat = MAP_EMACPHYRead( EMAC0_BASE, EMAC_PHY_ADDR, EPHY_BMSR );

					if( xTaskCheckForTimeOut( &xTimeOut, &xRemainingTime ) == pdTRUE )
					{
						break;
					}
				} while( ( phyStat & EPHY_BMSR_ANC ) == 0 ); /* wait auto negotiation done */

				phyStat = MAP_EMACPHYRead( EMAC0_BASE, EMAC_PHY_ADDR, EPHY_BMSR );
			}
		}
	}

	if( !( phyBMCon & EPHY_BMCR_ANEN ) )
	{
		xRc = -pdPHY_NEGOTIATION_INACTIVE; /* no negotiation is taking place! */
	}
	else if( phyBMCon & EPHY_BMCR_RESTARTAN )
	{
		xRc = -pdPHY_NEGOTIATION_NOT_STARTED; /* not started yet/tmo */
	}
	else
	{
		xRc = ( ( phyStat & EPHY_BMSR_ANC ) == 0 ) ? -pdPHY_NEGOTIATION_ACTIVE : 0; /* active/tmo/ok */
	}

	return xRc;
}

/*
 * @brief Query link negotiation results
 */
BaseType_t EthPhyGetNegotiationResult( uint32_t * pui32Config,
									   uint32_t * pui32Mode )
{
	BaseType_t linkStat;
	uint16_t phyStat = 0, phyExp, lpAD, anadReg;
	uint32_t config, mode, ui32RxMaxFrameSize;

	/* Get the current MAC configuration. */
	MAP_EMACConfigGet( EMAC0_BASE, &config, &mode,
					   &ui32RxMaxFrameSize );

	phyStat = MAP_EMACPHYRead( EMAC0_BASE, EMAC_PHY_ADDR, EPHY_BMSR );

	if( ( phyStat & EPHY_BMSR_ANC ) == 0 )
	{
		linkStat = ( ELS_DOWN | ELS_NEG_TMO );
	}
	else if( !( phyStat & EPHY_BMSR_LINKSTAT ) )
	{
		linkStat = ELS_DOWN;
	}
	else /* we're up and running */
	{
		linkStat = ELS_UP;

		anadReg = MAP_EMACPHYRead( EMAC0_BASE, EMAC_PHY_ADDR, EPHY_ANA ); /* get our advertised capabilities */

		lpAD = EPHY_ANA_10BT;                                             /* lowest priority resolution */

		phyExp = MAP_EMACPHYRead( EMAC0_BASE, EMAC_PHY_ADDR, EPHY_ANER );

		if( phyExp & EPHY_ANER_LPANABLE )
		{ /* ok,valid auto negotiation info */
			lpAD = MAP_EMACPHYRead( EMAC0_BASE, EMAC_PHY_ADDR, EPHY_ANLPA );

			if( lpAD & EPHY_ANLPA_RF )
			{
				linkStat |= ELS_REMOTE_FAULT;
			}

			if( lpAD & EPHY_ANLPA_PAUSE )
			{
				linkStat |= ELS_LP_PAUSE;
			}

			if( lpAD & EPHY_ANLPA_ASMDUP )
			{
				linkStat |= ELS_LP_ASM_DIR;
			}
		}
		else
		{
			linkStat |= ELS_LP_NEG_UNABLE;

			if( phyExp & EPHY_ANER_PDF )
			{
				linkStat |= ELS_PDF;
			}
		}

		/* set the PHY connection params */
		anadReg &= lpAD; /* union matching caps */

		/* get the settings, according to IEEE 802.3 Annex 28B.3 Priority Resolution */

		config &= ~( EMAC_CONFIG_SA_INSERT |
					 EMAC_CONFIG_SA_REPLACE |
					 EMAC_CONFIG_2K_PACKETS |
					 EMAC_CONFIG_STRIP_CRC |
					 EMAC_CONFIG_JABBER_DISABLE |
					 EMAC_CONFIG_JUMBO_ENABLE |
					 EMAC_CONFIG_IF_GAP_MASK |
					 EMAC_CONFIG_CS_DISABLE |
					 EMAC_CONFIG_100MBPS |
					 EMAC_CONFIG_RX_OWN_DISABLE |
					 EMAC_CONFIG_LOOPBACK |
					 EMAC_CONFIG_FULL_DUPLEX |
					 EMAC_CONFIG_CHECKSUM_OFFLOAD |
					 EMAC_CONFIG_RETRY_DISABLE |
					 EMAC_CONFIG_AUTO_CRC_STRIPPING |
					 EMAC_CONFIG_BO_MASK |
					 EMAC_CONFIG_DEFERRAL_CHK_ENABLE |
					 EMAC_CONFIG_PREAMBLE_MASK );
		config |= EMAC_CONFIG_SA_REPLACE | EMAC_CONFIG_STRIP_CRC | EMAC_CONFIG_CHECKSUM_OFFLOAD | EMAC_CONFIG_AUTO_CRC_STRIPPING | EMAC_CONFIG_BO_LIMIT_1024 | EMAC_CONFIG_7BYTE_PREAMBLE;
		mode |= ( 1 << 5 /* DGF */ ) | EMAC_MODE_RX_UNDERSIZED_FRAMES;
		#if ( ipconfigDRIVER_INCLUDED_TX_IP_CHECKSUM == 1 )
			config |= EMAC_MODE_TX_STORE_FORWARD;
		#endif

		if( anadReg & EPHY_ANA_100BTXFD )
		{
			config |= EMAC_CONFIG_100MBPS | EMAC_CONFIG_FULL_DUPLEX | EMAC_CONFIG_IF_GAP_96BITS;
		}
		else if( anadReg & EPHY_ANA_100BTX )
		{
			config |= EMAC_CONFIG_100MBPS | EMAC_CONFIG_HALF_DUPLEX | EMAC_CONFIG_IF_GAP_64BITS | EMAC_CONFIG_RX_OWN_DISABLE | EMAC_CONFIG_DEFERRAL_CHK_ENABLE;
		}
		else if( anadReg & EPHY_ANA_10BTFD )
		{
			config |= EMAC_CONFIG_10MBPS | EMAC_CONFIG_FULL_DUPLEX | EMAC_CONFIG_IF_GAP_96BITS;
		}
		else if( anadReg & EPHY_ANA_10BT )
		{
			config |= EMAC_CONFIG_10MBPS | EMAC_CONFIG_HALF_DUPLEX | EMAC_CONFIG_IF_GAP_64BITS | EMAC_CONFIG_RX_OWN_DISABLE | EMAC_CONFIG_DEFERRAL_CHK_ENABLE;
		}
		else
		{                        /* this should NOT happen! */
			linkStat |= ELS_NEG_FATAL_ERR;
			linkStat &= ~ELS_UP; /* make sure we stop...! */
		}
	}

	if( pui32Config )
	{
		*pui32Config = config;
	}

	if( pui32Mode )
	{
		*pui32Mode = mode;
	}

	return linkStat;
}

/*
 * @brief Get current link status
 */
BaseType_t EthPhyGetLinkStatus()
{
	uint16_t phyStat;
	BaseType_t linkStat;

	/* read the link status */
	phyStat = MAP_EMACPHYRead( EMAC0_BASE, EMAC_PHY_ADDR, EPHY_BMSR );
	linkStat = ( phyStat & EPHY_BMSR_LINKSTAT ) ? ELS_UP : ELS_DOWN;

	if( phyStat & EPHY_BMSR_RFAULT )
	{
		linkStat |= ELS_REMOTE_FAULT;
	}

	return linkStat;
}
