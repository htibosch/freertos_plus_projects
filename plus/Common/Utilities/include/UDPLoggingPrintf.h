/*
 * FreeRTOS+TCP <DEVELOPMENT BRANCH>
 * Copyright (C) 2022 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * http://aws.amazon.com/freertos
 * http://www.FreeRTOS.org
 */

#ifndef UDP_LOGGING_PRINTF_H
    #define UDP_LOGGING_PRINTF_H

    #include <stdarg.h>

    #ifdef __cplusplus
        extern "C" {
    #endif

/*
 * Add a formatted string along with a time-stamp to the buffer of logging messages.
 */
    int lUDPLoggingPrintf( const char * pcFormatString,
                           ... );
    int lUDPLoggingVPrintf( const char * pcFormatString,
                            va_list xArgs );

/*
 * Start a task which will send out the logging lines to a UDP address.
 * Only start this task once the TCP/IP stack is up and running.
 */
    void vUDPLoggingTaskCreate( void );

/*
 * vUDPLoggingFlush() can be called in cases where a lot of logging is being
 * generated to try to avoid the logging buffer overflowing.  WARNING - this
 * function will place the calling task into the Blocked state for 20ms.
 */
    void vUDPLoggingFlush( TickType_t uxWaitTicks );

/* this hook will be called for every log line printed. */
    void vUDPLoggingHook( const char * pcMessage,
                          BaseType_t xLength );

/*
 * vUDPLoggingApplicationHook() will be called for every line of logging.
 */
    void vUDPLoggingApplicationHook( const char * pcString );

/* FreeRTOS+TCP includes. */
    #include "FreeRTOS_IP.h"
    #include "FreeRTOS_Sockets.h"

    Socket_t xLoggingGetSocket( void );

    #ifdef __cplusplus
        } /* extern "C" */
    #endif

#endif /* DEMO_LOGGING_H */
