
#ifndef _TIVA_MAC_IPCONFIG_H
#define _TIVA_MAC_IPCONFIG_H

#ifndef ipconfigEMAC_PHY_CONFIG
    #define ipconfigEMAC_PHY_CONFIG (EMAC_PHY_TYPE_INTERNAL | EMAC_PHY_INT_MDIX_EN | EMAC_PHY_AN_100B_T_FULL_DUPLEX)
#endif

#ifndef	PHY_LS_HIGH_CHECK_TIME_MS
	/* Check if the LinkSStatus in the PHY is still high after 10 seconds of not
	receiving packets. */
	#define PHY_LS_HIGH_CHECK_TIME_MS	10000
#endif

#ifndef	PHY_LS_LOW_CHECK_TIME_MS
	/* Check if the LinkSStatus in the PHY is still low every second. */
	#define PHY_LS_LOW_CHECK_TIME_MS	1000
#endif

#ifndef niBUFFER_1_PACKET_SIZE    
/* The size of each buffer when BufferAllocation_1 is used:
 * This size should be bigger than EMAC_RX_BUFF_SIZE
http://www.freertos.org/FreeRTOS-Plus/FreeRTOS_Plus_TCP/Embedded_Ethernet_Buffer_Management.html */
    #define niBUFFER_1_PACKET_SIZE ((1536 + ipBUFFER_PADDING + 7) & ~0x07UL)
#endif
    
#define EMAC_RX_BUFF_SIZE       1536    // size of a DMA RX buffer.
#define ipconfigTX_UNIT_SIZE    (niBUFFER_1_PACKET_SIZE)
    
#define EMAC_PUBLIC_DATA        1
#define EMAC_PACKETS_OWNSECTION 0

void emac_failed_to_get_nb();
void emac_ack_buffer_released( void* );

#define iptraceNETWORK_BUFFER_RELEASED( pxBufferAddress ) emac_ack_buffer_released( pxBufferAddress )
#define iptraceNETWORK_BUFFER_RELEASED_FROM_ISR( pxBufferAddress )
#define iptraceFAILED_TO_OBTAIN_NETWORK_BUFFER() emac_failed_to_get_nb()


#define ipconfigTIVA_MAC_CORRECTLY_BINDED   1
    

#endif /* _TIVA_MAC_IPCONFIG_H */

/* *****************************************************************************
 End of File
 */
