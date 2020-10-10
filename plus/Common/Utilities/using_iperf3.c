
/* Please define the following macro's in FreeRTOSIPConfig.h */

#define USE_IPERF						        1
#define ipconfigIPERF_DOES_ECHO_UDP		        0

#define ipconfigIPERF_VERSION					3
#define ipconfigIPERF_STACK_SIZE_IPERF_TASK		680

#define ipconfigIPERF_TX_BUFSIZE				( 8 * ipconfigTCP_MSS )
#define ipconfigIPERF_TX_WINSIZE				( 6 )
#define ipconfigIPERF_RX_BUFSIZE				( 8 * ipconfigTCP_MSS )
#define ipconfigIPERF_RX_WINSIZE				( 6 )

/* The iperf module declares a character buffer to store its send data. */
#define ipconfigIPERF_RECV_BUFFER_SIZE			( 2 * ipconfigTCP_MSS )


/* Before using iperf, please start the task by calling: */

	vIPerfInstall();
