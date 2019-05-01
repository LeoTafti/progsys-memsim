#include "tlb_hrchy_mng.h"
#include "tlb_hrchy.h"
#include "error.h"
#include "addr.h"



/**
 * @brief Clean a TLB (invalidate, reset...).
 *
 * This function erases all TLB data.
 * @param  tlb (generic) pointer to the TLB
 * @param tlb_type an enum to distinguish between different TLBs
 * @return  error code
 */
#define FLUSH(tlb_entry_type, TLB_LINES) \
    tlb_entry_type* tlb_arr = (tlb_entry_type*)tlb; \
    for(size_t i = 0; i < TLB_LINES; i++) { \
        tlb_arr[i].v = INVALID; \
        tlb_arr[i].tag = 0; \
        tlb_arr[i].phy_page_num = 0; \
    }

int tlb_flush(void *tlb, tlb_t tlb_type){
    M_REQUIRE_NON_NULL(tlb);

    switch (tlb_type)
    {
    case L1_ITLB:
        FLUSH(l1_itlb_entry_t, L1_ITLB_LINES);
        break;
    case L1_DTLB:
        FLUSH(l1_dtlb_entry_t, L1_DTLB_LINES);
        break;
    case L2_TLB:
        FLUSH(l2_tlb_entry_t, L2_TLB_LINES);
        break;
    default:
        M_EXIT(ERR_BAD_PARAMETER, "%s", "Unrecognized TLB type");
        break;
    }

    return ERR_NONE;
}

#undef FLUSH

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

#define HIT(tlb_entry_type, TLB_LINES) \
    uint64_t tag_and_index = virt_addr_t_to_uint64_t(vaddr)>>PAGE_OFFSET; \
    \
    uint32_t tag = tag_and_index / TLB_LINES; \
    uint8_t index = tag_and_index % TLB_LINES; \
    \
    tlb_entry_type* tlb_entry = ((tlb_entry_type*)tlb)[index]; \
    \
    if (tlb_entry->v == VALID && tlb_entry->tag == tag){ \
        /*Entry was found in TLB*/ \
        paddr->phy_page_num = tlb_entry->phy_page_num; \
        paddr->page_offset = vaddr->page_offset; \
        return HIT; \
    }else{ \
        return MISS; \
    } \

int tlb_hit( const virt_addr_t * vaddr,
             phy_addr_t * paddr,
             const void  * tlb,
             tlb_t tlb_type){

    if(vaddr == NULL || paddr == NULL || tlb == NULL) {
        return MISS;
    }

    //As i did before, the following would go in the switch

    // uint64_t tag_and_index = virt_addr_t_to_uint64_t(vaddr)>>PAGE_OFFSET;

    // uint32_t tag = tag_and_index / L...TLB_LINES;
    // uint8_t index = tag_and_index % L...TLB_LINES;

    // l...tlb_entry* tlb_entry = ((l...tlb_entry*)tlb)[index];

    // if (tlb_entry->v == VALID && tlb_entry->tag == tag){
    //     //Entry was found in TLB
    //     paddr->phy_page_num = tlb_entry->phy_page_num;
    //     paddr->page_offset = vaddr->page_offset;
    //     return HIT;
    // }else{
    //     return MISS;
    // }

    // if(vaddr == NULL || paddr == NULL || tlb == NULL || replacement_policy == NULL) return MISS;

    // uint64_t tag = virt_addr_t_to_uint64_t(vaddr)>>PAGE_OFFSET;
    // node_t* m = NULL;

    // for_all_nodes_reverse(n, replacement_policy->ll) {
    //     if(tlb[n->value].tag == tag && tlb[n->value].v == VALID) {
    //         m = n;
    //         break;
    //     }
    // }

    // int hit_or_miss;
    // if(m != NULL) {
    //     hit_or_miss = HIT;
    //     //set paddr
    //     paddr->phy_page_num = tlb[m->value].phy_page_num;
    //     paddr->page_offset = vaddr->page_offset;
    //     //update replacement policy
    //     replacement_policy->move_back(replacement_policy->ll, m);
    // }else{
    //     hit_or_miss = MISS;
    // }
    // return hit_or_miss;

    // return ERR_NONE;

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

#undef HIT

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
#define INSERT(tlb_entry_type, number_lines)\
  M_REQUIRE(line_index < number_lines, ERR_BAD_PARAMETER, "%s", "wrong tlb insert");\
  (tlb_entry_type*) tlb[line_index] = *((tlb_entry_type*)tlb_entry)\

int tlb_insert( uint32_t line_index,
                const void * tlb_entry,
                void * tlb,
                tlb_t tlb_type){

  M_REQUIRE_NON_NULL(tlb_entry);
  M_REQUIRE_NON_NULL(tlb);

  switch (tlb_type)
  {
  case L1_ITLB:
      INSERT(l1_itlb_entry_t, L1_ITLB_LINES);
      break;
  case L1_DTLB:
      INSERT(l1_dtlb_entry_t, L1_DTLB_LINES);
      break;
  case L2_TLB:
      INSERT(l2_tlb_entry_t, L2_TLB_LINES);
      break;
  default:
      M_EXIT(ERR_BAD_PARAMETER, "%s", "Unrecognized TLB type");
      break;
  }
  return ERR_NONE;
}

#undef INSERT
//=========================================================================
/**
 * @brief Initialize a TLB entry
 * @param vaddr pointer to virtual address, to extract tlb tag
 * @param paddr pointer to physical address, to extract physical page number
 * @param tlb_entry pointer to the entry to be initialized
 * @param tlb_type to distinguish between different TLBs
 * @return  error code
 */

#define INIT(tlb_entry_type, TLB_LINES_BITS) \
    tlb_entry_type* entry = (tlb_entry_type*)tlb_entry; \
    entry->v = 1; \
    entry->tag = virt_addr_t_to_uint64_t(vaddr)>>(PAGE_OFFSET+L2_TLB_LINES_BITS); \
    entry->phy_page_num = paddr->phy_page_num;

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
        INIT(l1_itlb_entry_t, L1_ITLB_LINES_BITS);
        break;
    case L1_DTLB:
        INIT(l1_dtlb_entry_t, L1_DTLB_LINES_BITS);
        break;
    case L2_TLB:
        INIT(l2_tlb_entry_t, L2_TLB_LINES_BITS);
        break;
    default:
        M_EXIT(ERR_BAD_PARAMETER, "%s", "Unrecognized TLB type");
        break;
    }

  return ERR_NONE;
}

#undef INIT

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
