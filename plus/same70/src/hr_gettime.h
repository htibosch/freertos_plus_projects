/*
*/

#ifndef HR_GETTTIME_H

#define HR_GETTTIME_H

#ifdef __cplusplus
extern "C" {
#endif

/* SAM4E: Timer initialization function */
void vStartHighResolutionTimer( void );

/* Get the current time measured in uS. The return value is 64-bits. */
uint64_t ullGetHighResolutionTime( void );

/* Stop and start TC-0 timer. */
void vHRStop( void );
void vHRStart( void );

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* HR_GETTTIME_H */