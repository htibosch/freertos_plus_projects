
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

/* FreeRTOS+TCP includes. */
#include "FreeRTOSIPConfig.h"
#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"
#include "FreeRTOS_IP_Private.h"
#include "NetworkBufferManagement.h"

#include "tiva_mac.h"

size_t EMAC_ListCount(EMAC_DCPT_LIST* pL)
{
    EMAC_DCPT_NODE*  pN;
    size_t     nNodes=0;
    for(pN=pL->head; pN!=0; pN=pN->next)
    {
        nNodes++;
    }
    return nNodes;
}

size_t _EnetDescriptorsCount(EMAC_DCPT_LIST* pList, BaseType_t isHwCtrl)
{
	EMAC_DCPT_NODE *pEDcpt;
	size_t nDcpts=0, nDcptsNot = 0;
    BaseType_t eo;
	
	for(pEDcpt=pList->head; pEDcpt!=0 && pEDcpt->next!=0; pEDcpt=pEDcpt->next)
	{	// don't count the ending dummy descriptor 
        eo = pEDcpt->pxHWDesc->ui32CtrlStatus & DES0_RX_CTRL_OWN ? pdTRUE : pdFALSE;
		if (eo == isHwCtrl)
		{
			nDcpts++;
		} else nDcptsNot++;
	}
	return nDcpts;	
}

void EMAC_ListAddHead(EMAC_DCPT_LIST* pL, EMAC_DCPT_NODE* pN)
{
    pN->next=pL->head;
    pL->head=pN;
    pN->pxHWDesc->DES3.pLink = pL->head->pxHWDesc;
    if(pL->tail==0)
    {  // empty list
        pL->tail=pN;
    }
}

void EMAC_ListAddTail(EMAC_DCPT_LIST* pL, EMAC_DCPT_NODE* pN, const uint32_t validControl )
{
    pN->next=0;
    pN->pxHWDesc->DES3.pLink = pN->pxHWDesc;
    pN->pxHWDesc->ui32ExtRxStatus = 0;
    pN->pxHWDesc->ui32CtrlStatus &= validControl;
    if(pL->tail==0)
    {
        pL->head=pL->tail=pN;
    }
    else
    {
        pL->tail->next=pN;
        pL->tail->pxHWDesc->DES3.pLink=pN->pxHWDesc;
        pL->tail=pN;
    }
    pL->tail->pxHWDesc->DES3.pLink=0;
}

void EMAC_ListRemove(EMAC_DCPT_LIST* pL, EMAC_DCPT_NODE* pN, EMAC_DCPT_NODE* pPrev)
{
    if ( !pN )
        return;
    if( pL->head==pL->tail )    /* only one element or empty list */
    {
        if ( pL->head == pN )
            pL->head=pL->tail=0;
        else 
            configASSERT( 0 );  /* should not happen */
    } else if ( pL->head == pN ) /* more than one, and it is head */
    {
        pL->head=pN->next;
    } else if ( (pPrev != NULL) && (pPrev->next == pN) ) {
        pPrev->next = pN->next;
        pPrev->pxHWDesc->DES3.pLink = pN->pxHWDesc->DES3.pLink;
        if ( pL->tail == pN ) {
            pL->tail = pN->next;
            if ( pL->tail == 0 ) {
                pL->tail = pPrev;
            }
        }
    } else {
        EMAC_DCPT_NODE *pxDesc;
        for(pxDesc = pL->head; pxDesc!=0; pxDesc=pxDesc->next)
        {
            if ( pxDesc->next == pN ) {
                pxDesc->next = pN->next;
                pxDesc->pxHWDesc->DES3.pLink = pN->pxHWDesc->DES3.pLink;
                if ( pL->tail == pN ) {
                    pL->tail = pN->next;
                    if ( pL->tail == 0 ) {
                        pL->tail = pxDesc;
                    }
                }
                return;
            }
        }
        configASSERT(0);    /* should not happen */
    }
}

