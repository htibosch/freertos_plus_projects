BaseType_t xPhyCheckLinkStatus( EthernetPhy_t *pxPhyObject, BaseType_t xHadReception )
{
uint32_t ulStatus, ulBitMask = 1u;
BaseType_t xPhyIndex;
BaseType_t xNeedCheck = pdFALSE;

	if( xHadReception > 0 )
	{
		/* A packet was received. No need to check for the PHY status now,
		but set a timer to check it later on. */
		vTaskSetTimeOutState( &( pxPhyObject->xLinkStatusTimer ) );
		pxPhyObject->xLinkStatusRemaining = pdMS_TO_TICKS( ipconfigPHY_LS_HIGH_CHECK_TIME_MS );
		for( xPhyIndex = 0; xPhyIndex < pxPhyObject->xPortCount; xPhyIndex++, ulBitMask <<= 1 )
		{
			if( ( pxPhyObject->ulLinkStatusMask & ulBitMask ) == 0ul )
			{
				pxPhyObject->ulLinkStatusMask |= ulBitMask;
				FreeRTOS_printf( ( "xPhyCheckLinkStatus: PHY LS now %02lX\n", pxPhyObject->ulLinkStatusMask ) );
				xNeedCheck = pdTRUE;
			}
		}
	}
	else if( xTaskCheckForTimeOut( &( pxPhyObject->xLinkStatusTimer ), &( pxPhyObject->xLinkStatusRemaining ) ) != pdFALSE )
	{
		/* Frequent checking the PHY Link Status can affect for the performance of Ethernet controller.
		As long as packets are received, no polling is needed.
		Otherwise, polling will be done when the 'xLinkStatusTimer' expires. */
		for( xPhyIndex = 0; xPhyIndex < pxPhyObject->xPortCount; xPhyIndex++, ulBitMask <<= 1 )
		{
		BaseType_t xPhyAddress = pxPhyObject->ucPhyIndexes[ xPhyIndex ];

			if( pxPhyObject->fnPhyRead( xPhyAddress, phyREG_01_BMSR, &ulStatus ) == 0 )
			{
				if( !!( pxPhyObject->ulLinkStatusMask & ulBitMask ) != !!( ulStatus & phyBMSR_LINK_STATUS ) )
				{
					if( ( ulStatus & phyBMSR_LINK_STATUS ) != 0 )
					{
						pxPhyObject->ulLinkStatusMask |= ulBitMask;
					}
					else
					{
						pxPhyObject->ulLinkStatusMask &= ~( ulBitMask );
					}
					FreeRTOS_printf( ( "xPhyCheckLinkStatus: PHY LS now %02lX\n", pxPhyObject->ulLinkStatusMask ) );
					xNeedCheck = pdTRUE;
				}
			}
		}
		vTaskSetTimeOutState( &( pxPhyObject->xLinkStatusTimer ) );
		if( ( pxPhyObject->ulLinkStatusMask & ( ulBitMask >> 1 ) ) != 0 )
		{
			/* The link status is high, so don't poll the PHY too often. */
			printf( "Link status high - pxPhyObject->ulLinkStatusMask: %d\r\n", pxPhyObject->ulLinkStatusMask );
			pxPhyObject->xLinkStatusRemaining = pdMS_TO_TICKS( ipconfigPHY_LS_HIGH_CHECK_TIME_MS );
		}
		else
		{
			/* The link status is low, polling may be done more frequently. */
			printf( "Link status low - pxPhyObject->ulLinkStatusMask: %d\r\n", pxPhyObject->ulLinkStatusMask );
			pxPhyObject->xLinkStatusRemaining = pdMS_TO_TICKS( ipconfigPHY_LS_LOW_CHECK_TIME_MS );
		}
	}
	return xNeedCheck;
}