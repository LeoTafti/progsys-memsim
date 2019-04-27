#include "tlb_hrchy_mng.h"
#include "tlb_hrchy.h"
#include "error.h"
#include "addr.h"

//TODO : switch case with some copy paste is a super ugly, but how can I do better
//(since i need to cast the void* to use it...)?
// (apparently, using unions could help but not solve the problem ?)


/**
 * @brief Clean a TLB (invalidate, reset...).
 *
 * This function erases all TLB data.
 * @param  tlb (generic) pointer to the TLB
 * @param tlb_type an enum to distinguish between different TLBs
 * @return  error code
 */

int tlb_flush(void *tlb, tlb_t tlb_type){
    M_REQUIRE_NON_NULL(tlb);
    
    switch (tlb_type)
    {
    case L1_ITLB:
        l1_itlb_entry_t* tlb_arr = (l1_itlb_entry_t*)tlb;
        for(size_t i = 0; i < L1_ITLB_LINES; i++) {
            tlb_arr[i].v = INVALID;
            tlb_arr[i].tag = 0;
            tlb_arr[i].phy_page_num = 0;
        }
        break;
    case L1_DTLB:
        l1_dtlb_entry_t* tlb_arr = (l1_dtlb_entry_t*)tlb;
        for(size_t i = 0; i < L1_DTLB_LINES; i++) {
            tlb_arr[i].v = INVALID;
            tlb_arr[i].tag = 0;
            tlb_arr[i].phy_page_num = 0;
        }
        break;
    case L2_TLB:
        l2_tlb_entry_t* tlb_arr = (l2_tlb_entry_t*)tlb;
        for(size_t i = 0; i < L2_TLB_LINES; i++) {
            tlb_arr[i].v = INVALID;
            tlb_arr[i].tag = 0;
            tlb_arr[i].phy_page_num = 0;
        }
        break;
    default:
        M_EXIT(ERR_BAD_PARAMETER, "%s", "Unrecognized TLB type");
        break;
    }

    return ERR_NONE;
}

//=========================================================================
/**
 * @brief Check if a TLB entry exists in the TLB.
 *
 * On hit, return success (1) and update the physical page number passed as the pointer to the function.
 * On miss, return miss (0).
 *
 * @param vaddr pointer to virtual address
 * @param paddr (modified) pointer to physical address
 * @param tlb pointer to the beginning of the tlb
 * @param tlb_type to distinguish between different TLBs
 * @return hit (1) or miss (0)
 */

int tlb_hit( const virt_addr_t * vaddr,
             phy_addr_t * paddr,
             const void  * tlb,
             tlb_t tlb_type){
    
    //TODO : implement this. But first, correct the non hierarchical version in tlb_mng.c
    
    //As i did before, the following would go in the switch
    
    uint64_t tag_and_index = virt_addr_t_to_uint64_t(vaddr)>>PAGE_OFFSET;
    
    uint32_t tag = tag_and_index / L...TLB_LINES;
    uint8_t index = tag_and_index % L...TLB_LINES;

    l...tlb_entry* tlb_entry = ((l...tlb_entry*)tlb)[index];
    
    if (tlb_entry->v == VALID && tlb_entry->tag == tag){
        //Entry was found in TLB
        paddr->phy_page_num = tlb_entry->phy_page_num;
        paddr->page_offset = vaddr->page_offset;
        return HIT;
    }else{
        return MISS;
    }

    // switch (tlb_type)
    // {
    // case L1_ITLB:
    //     l1_itlb_entry_t* tlb_arr = (l1_itlb_entry_t*)tlb;
        
    //     break;
    // case L1_DTLB:
    //     l1_dtlb_entry_t* tlb_arr = (l1_dtlb_entry_t*)tlb;
        
    //     break;
    // case L2_TLB:
    //     l2_tlb_entry_t* tlb_arr = (l2_tlb_entry_t*)tlb;
        
    //     break;
    // default:
    //     M_EXIT(ERR_BAD_PARAMETER, "%s", "Unrecognized TLB type");
    //     break;
    // }
}

//=========================================================================
/**
 * @brief Insert an entry to a tlb. Eviction policy is simple since
 *        direct mapping is used.
 * @param line_index the number of the line to overwrite
 * @param tlb_entry pointer to the tlb entry to insert
 * @param tlb pointer to the TLB
 * @param tlb_type to distinguish between different TLBs
 * @return  error code
 */

int tlb_insert( uint32_t line_index,
                const void * tlb_entry,
                void * tlb,
                tlb_t tlb_type){

}

//=========================================================================
/**
 * @brief Initialize a TLB entry
 * @param vaddr pointer to virtual address, to extract tlb tag
 * @param paddr pointer to physical address, to extract physical page number
 * @param tlb_entry pointer to the entry to be initialized
 * @param tlb_type to distinguish between different TLBs
 * @return  error code
 */

int tlb_entry_init( const virt_addr_t * vaddr,
                    const phy_addr_t * paddr,
                    void * tlb_entry,
                    tlb_t tlb_type){
    M_REQUIRE_NON_NULL(vaddr);
    M_REQUIRE_NON_NULL(paddr);
    M_REQUIRE_NON_NULL(tlb_entry);
    
    switch (tlb_type)
    {
    case L1_ITLB:
        l1_itlb_entry_t* entry = (l1_itlb_entry_t*)tlb_entry;
        entry->v = 1;
        entry->tag = virt_addr_t_to_uint64_t(vaddr)>>(PAGE_OFFSET+L1_ITLB_LINES_BITS);
        entry->phy_page_num = paddr->phy_page_num;
        break;
    case L1_DTLB:
        l1_dtlb_entry_t* entry = (l1_dtlb_entry_t*)tlb_entry;
        entry->v = 1;
        entry->tag = virt_addr_t_to_uint64_t(vaddr)>>(PAGE_OFFSET+L1_DTLB_LINES_BITS);
        entry->phy_page_num = paddr->phy_page_num;
        break;
    case L2_TLB:
        l2_tlb_entry_t* entry = (l2_tlb_entry_t*)tlb_entry;
        entry->v = 1;
        entry->tag = virt_addr_t_to_uint64_t(vaddr)>>(PAGE_OFFSET+L2_TLB_LINES_BITS);
        entry->phy_page_num = paddr->phy_page_num;
        break;
    default:
        M_EXIT(ERR_BAD_PARAMETER, "%s", "Unrecognized TLB type");
        break;
    }

  return ERR_NONE;
}

//=========================================================================
/**
 * @brief Ask TLB for the translation.
 *
 * @param mem_space pointer to the memory space
 * @param vaddr pointer to virtual address
 * @param paddr (modified) pointer to physical address (returned from TLB)
 * @param access to distinguish between fetching instructions and reading/writing data
 * @param l1_itlb pointer to the beginning of L1 ITLB
 * @param l1_dtlb pointer to the beginning of L1 DTLB
 * @param l2_tlb pointer to the beginning of L2 TLB
 * @param hit_or_miss (modified) hit (1) or miss (0)
 * @return error code
 */

int tlb_search( const void * mem_space,
                const virt_addr_t * vaddr,
                phy_addr_t * paddr,
                mem_access_t access,
                l1_itlb_entry_t * l1_itlb,
                l1_dtlb_entry_t * l1_dtlb,
                l2_tlb_entry_t * l2_tlb,
                int* hit_or_miss){

}
