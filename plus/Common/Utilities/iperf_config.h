
/* Please add this configuration to you FreeRTOSIPConfig.h file: */

#define USE_IPERF								1

#define ipconfigIPERF_PRIORITY_IPERF_TASK	( configTIMER_TASK_PRIORITY - 3 )

#define ipconfigIPERF_HAS_UDP					0
#define ipconfigIPERF_DOES_ECHO_UDP				0

#define ipconfigIPERF_VERSION					3
#define ipconfigIPERF_STACK_SIZE_IPERF_TASK		680

/* When there is a lot of RAM, try these values or higher: */

#define ipconfigIPERF_TX_BUFSIZE				( 18 * ipconfigTCP_MSS )
#define ipconfigIPERF_TX_WINSIZE				( 12 )
#define ipconfigIPERF_RX_BUFSIZE				( 18 * ipconfigTCP_MSS )
#define ipconfigIPERF_RX_WINSIZE				( 12 )

/* The iperf module declares a character buffer to store its send data. */
#define ipconfigIPERF_RECV_BUFFER_SIZE			( 12 * ipconfigTCP_MSS )
