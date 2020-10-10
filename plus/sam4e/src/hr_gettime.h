/*
*/

#ifndef HR_GETTTIME_H

#define HR_GETTTIME_H

/* SAM4E: Timer initialization function */
void vStartHighResolutionTimer( void );

/* Get the current time measured in uS. The return value is 64-bits. */
uint64_t ullGetHighResolutionTime( void );

#endif /* HR_GETTTIME_H */