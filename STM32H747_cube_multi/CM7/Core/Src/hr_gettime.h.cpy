/*
*/

#ifndef HR_GETTTIME_H

#define HR_GETTTIME_H

#ifdef __cplusplus
extern "C" {
#endif

#if( HAS_HR_TIMER != 0 )
	/* STM32F4xx: Timer2 initialization function */
	void vStartHighResolutionTimer( void );
	void vStopHighResolutionTimer( void );

	/* Get a high-resolution time: number of uS since booting the device. */
	unsigned long long ullGetHighResolutionTime( void );

	void vSetHighResolutionTime(unsigned long long aMsec);

	extern char hr_started;
#endif	/* HAS_HR_TIMER */

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* HR_GETTTIME_H */