
#ifndef _TIVA_PHY_H
#define _TIVA_PHY_H

/* Provide C++ Compatibility */
#ifdef __cplusplus
extern "C" {
#endif

#define pdPHY_NEGOTIATION_UNABLE        307 // No negotiation support
#define pdPHY_NEGOTIATION_INACTIVE      308 // No negotiation active
#define pdPHY_NEGOTIATION_NOT_STARTED   309 // Negotiation not started yet
#define pdPHY_NEGOTIATION_ACTIVE        310 // Negotiation active
#define pdPHY_NEGOTIATION_LINKDOWN      311 // Link down after negotiation, negotiation failed
#define pdPHY_DTCT_ERR                  312 // No Phy was detected or it failed to respond to reset command
#define pdPHY_CPBL_ERR                  313 // No match between the capabilities: the Phy supported and the open requested ones
#define pdPHY_CFG_ERR                   314 // Hardware configuration doesn't match the requested open mode

/*
 * @brief Ethernet Link Status Codes
 * @detail Defines the possible status flags of Ethernet link.
 */
enum xEthLinkStat
{
    ELS_DOWN = 0,           // No connection to the LinkPartner
    ELS_UP = 1,             // Link is up 
    ELS_LP_NEG_UNABLE = 2,  // LP non negotiation able
    ELS_REMOTE_FAULT = 4,   // LP fault during negotiation
    ELS_PDF = 8,            // Parallel Detection Fault encountered (when ELS_LP_NEG_UNABLE)
    ELS_LP_PAUSE = 0x10,    // LP supports symmetric pause
    ELS_LP_ASM_DIR = 0x20,  // LP supports asymmetric TX/RX pause operation
    ELS_NEG_TMO = 0x1000,   // LP not there
    ELS_NEG_FATAL_ERR = 0x2000  // An unexpected fatal error occurred during the negotiation
};

#ifdef ipconfigPHY_USE_STAT

typedef struct tPHY_STAT
{
    volatile uint32_t uNegTO;
    volatile uint32_t uAN_disabled;
    volatile uint32_t uAN_errors;
    volatile uint32_t uConfigMismatch;
    volatile uint32_t uPhyNotDetected;
    volatile uint32_t uCapError;
    volatile uint32_t uNegotiationCalls;
    volatile uint32_t uLinkTO;

    volatile uint32_t uInitializationStage;
} xPHY_STAT;

extern xPHY_STAT phy_stat;

/**
 * Note: This rather weird construction where we invoke the macro with the
 * name of the field minus its Hungarian prefix is a workaround for a problem
 * experienced with GCC which does not like concatenating tokens after an
 * operator, specifically '.' or '->', in a macro.
 */
#define PHY_STATS_INC(x) do{ phy_stat.u##x++; } while(0)
#define PHY_STATS_DEC(x) do{ phy_stat.u##x--; } while(0)
#define PHY_STATS_ADD(x, inc) do{ phy_stat.u##x += (inc); } while(0)
#define PHY_STATS_SUB(x, dec) do{ phy_stat.u##x -= (dec); } while(0)
#else
#define PHY_STATS_INC(x)
#define PHY_STATS_DEC(x)
#define PHY_STATS_ADD(x, inc)
#define PHY_STATS_SUB(x, dec)
#endif

/*
 * @brief Check for negotiation procedure completition
 */
BaseType_t EthPhyNegotiationComplete(bool waitComplete);
/*
 * @brief Query link negotiation results
 */
BaseType_t EthPhyGetNegotiationResult(uint32_t *pui32Config, uint32_t *pui32Mode);

/*
 * @brief Get current link status
 */
BaseType_t EthPhyGetLinkStatus();

/* Provide C++ Compatibility */
#ifdef __cplusplus
}
#endif

#endif /* _TIVA_PHY_H */

/* *****************************************************************************
End of File
*/
