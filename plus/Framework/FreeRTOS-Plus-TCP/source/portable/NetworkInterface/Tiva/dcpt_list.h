

#ifndef DCPT_LIST_H
#define	DCPT_LIST_H

#ifdef	__cplusplus
extern "C" {
#endif

typedef struct xEMAC_DCPT_NODE
{
    struct xEMAC_DCPT_NODE* volatile    next;       // next pointer in virtual space
    tEMACDMADescriptor*                 pxHWDesc;   // the associated hardware descriptor
} EMAC_DCPT_NODE; // Ethernet descriptor node: generic linked descriptor for both TX/RX.

/* as there are no specific alignment reqs for Tiva we just use 32-bit alignment */
#define EMAC_DCPT_LIST_ALIGN      4
typedef struct __attribute__((aligned(4)))
{
    uint32_t                    pad;
    EMAC_DCPT_NODE* volatile    head;   // list head
    EMAC_DCPT_NODE* volatile    tail;
} EMAC_DCPT_LIST;  // single linked list

// Function that adjusts the alignment of stack descriptor lists that need to be x bytes aligned!
// This fixes a gcc allocation issue:
//      not properly allocating aligned data in the stack!
// Note that the size of allocated space needs to be large enough 
// to allow for this adjustment!:
//  allocSize >= align + ObjSize
//  We're using align == 16, ObjSize == 16
#define _EthAlignAdjust(pL)  ((EMAC_DCPT_LIST*)(((uint32_t)(pL) + (EMAC_DCPT_LIST_ALIGN - 1)) & (~(EMAC_DCPT_LIST_ALIGN -1))))
size_t _EnetDescriptorsCount(EMAC_DCPT_LIST* pList, BaseType_t isHwCtrl);

/////  single linked lists manipulation ///////////
//
#define EMAC_ListIsEmpty(pL)       ((pL)->head==0)

size_t EMAC_ListCount(EMAC_DCPT_LIST* pL);
void EMAC_ListAddHead(EMAC_DCPT_LIST* pL, EMAC_DCPT_NODE* pN);
void EMAC_ListAddTail(EMAC_DCPT_LIST* pL, EMAC_DCPT_NODE* pN, const uint32_t validControl);
void EMAC_ListRemove(EMAC_DCPT_LIST* pL, EMAC_DCPT_NODE* pN, EMAC_DCPT_NODE* pPrev);

extern __inline__ EMAC_DCPT_NODE* __attribute__((always_inline)) EMAC_ListRemoveHead(EMAC_DCPT_LIST* pL)
{
    EMAC_DCPT_NODE* pN=pL->head;
    if(pL->head==pL->tail)
    {
        pL->head=pL->tail=0;
    }
    else
    {
        pL->head=pN->next;
    }
    return pN;
}

extern __inline__ void __attribute__((always_inline)) EMAC_ListAppendTail(EMAC_DCPT_LIST* pL, EMAC_DCPT_LIST* pAList, const uint32_t validControl )
{
    EMAC_DCPT_NODE* pN;
    while((pN=EMAC_ListRemoveHead(pAList)))
    {
        EMAC_ListAddTail(pL, pN, validControl);
    }
}

extern __inline__ EMAC_DCPT_LIST* __attribute__((always_inline)) EMAC_ListInit(EMAC_DCPT_LIST* pL)
{
    EMAC_DCPT_LIST* pNL = pL;
    pNL->head = pNL->tail = 0;
    return pNL;
}

extern __inline__ BaseType_t __attribute__((always_inline)) xValidLength( BaseType_t xLength )
{
	if( ( xLength >= ( BaseType_t ) sizeof( struct xARP_PACKET ) ) && ( ( ( uint32_t ) xLength ) <= ipTOTAL_ETHERNET_FRAME_SIZE ) )
		return pdTRUE;
	else return pdFALSE;
}

#ifdef	__cplusplus
}
#endif

#endif	/* DCPT_LIST_H */

