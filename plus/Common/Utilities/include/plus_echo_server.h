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

#ifndef __PLUS_ECHO_SERVER_H__
    #define __PLUS_ECHO_SERVER_H__

    #ifdef __cplusplus
        extern "C" {
    #endif

    #define ECHO_BUFFER_LENGTH       7300

    #define ECHO_SERVER_PORT         32002

    #define CLIENT_SEND_COUNT        ( 1024u * 1024u )
    #define CLIENT_SEND_COUNT_MAX    ( 0x7fffffff )

/* Number of bytes to send per session, default CLIENT_SEND_COUNT */
    extern size_t uxClientSendCount;

    extern UBaseType_t uxServerAbort;

    #define ECHO_SERVER_LOOPBACK_IP_ADDRESS    0x7f000001

    #define PLUS_TEST_TX_BUFSIZE               2 * 1460 /* Units of bytes. */
    #define PLUS_TEST_TX_WINSIZE               1        /* Size in units of MSS */
    #define PLUS_TEST_RX_BUFSIZE               2 * 1460 /* Units of bytes. */
    #define PLUS_TEST_RX_WINSIZE               1        /* Size in units of MSS */

/*#define PLUS_TEST_TX_BUFSIZE				4*1460	/ * Units of bytes. * / */
/*#define PLUS_TEST_TX_WINSIZE				2		/ * Size in units of MSS * / */
/*#define PLUS_TEST_RX_BUFSIZE				4*1460	/ * Units of bytes. * / */
/*#define PLUS_TEST_RX_WINSIZE				2		/ * Size in units of MSS * / */

    extern uint32_t echoServerIPAddress( void );


    extern char pcPlusBuffer[ ECHO_BUFFER_LENGTH ];
    extern int plus_test_active;
    extern int plus_test_two_way;

    void plus_echo_start( int aValue );

    void plus_echo_application_thread( void * parameters );
    void plus_echo_client_thread( void * parameters );

    #ifdef __cplusplus
        } /* extern "C" */
    #endif

#endif /* ifndef __PLUS_ECHO_SERVER_H__ */
