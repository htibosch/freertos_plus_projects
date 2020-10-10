
/* Please add this configuration to you FreeRTOSIPConfig.h file: */

#define USE_IPERF								1

#define ipconfigIPERF_DOES_ECHO_UDP				0

#define ipconfigIPERF_VERSION					3
#define ipconfigIPERF_STACK_SIZE_IPERF_TASK		680
#define ipconfigIPERF_TX_BUFSIZE				( 18 * ipconfigTCP_MSS )
#define ipconfigIPERF_TX_WINSIZE				( 12 )
#define ipconfigIPERF_RX_BUFSIZE				( 18 * ipconfigTCP_MSS )
#define ipconfigIPERF_RX_WINSIZE				( 12 )
