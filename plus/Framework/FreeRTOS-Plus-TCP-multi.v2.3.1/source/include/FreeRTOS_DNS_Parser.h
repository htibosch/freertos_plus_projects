/*
 * FreeRTOS+TCP V2.3.2
 * Copyright (C) 2020 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
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
 * https://github.com/FreeRTOS
 * https://www.FreeRTOS.org
 */

#ifndef FREERTOS_DNS_PARSER_H
#define FREERTOS_DNS_PARSER_H

/* FreeRTOS includes. */
#include "FreeRTOS.h"

/* FreeRTOS+TCP includes. */
#include "FreeRTOS_IP.h"

#include "FreeRTOS_DNS_Globals.h"

/* Standard includes. */
#include <stdint.h>
#if ( ipconfigUSE_DNS != 0 )

/** @brief Flag DNS parsing errors in situations where an IPv4 address is the return
 * type. */

    #if ( ipconfigUSE_DNS_CACHE == 1 ) || ( ipconfigDNS_USE_CALLBACKS == 1 )
        size_t DNS_ReadNameField( ParseSet_t * pxSet,
                                  size_t uxDestLen );
    #endif /* ipconfigUSE_DNS_CACHE || ipconfigDNS_USE_CALLBACKS */

/*
 * Process a response packet from a DNS server.
 * The parameter 'xExpected' indicates whether the identifier in the reply
 * was expected, and thus if the DNS cache may be updated with the reply.
 * The IP address found will be stored in 'ppxAddressInfo' ( IPv4 or IPv6 ).
 * ppxAddressInfo may be NULL if the caller is not interested in the answers.
 */
    uint32_t DNS_ParseDNSReply( uint8_t * pucUDPPayloadBuffer,
                                size_t uxBufferLength,
                                struct freertos_addrinfo ** ppxAddressInfo,
                                BaseType_t xExpected,
                                uint16_t usPort );

    #if ( ipconfigUSE_NBNS == 1 )
        void DNS_TreatNBNS( uint8_t * pucPayload,
                            size_t uxBufferLength,
                            uint32_t ulIPAddress );
    #endif

#endif /* if ( ipconfigUSE_DNS != 0 ) */
#endif /* ifndef FREERTOS_DNS_PARSER_H */
