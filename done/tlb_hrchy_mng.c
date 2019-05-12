/**
 * @file list.c
 * @brief Doubly linked list
 *
 * @author Juillard Paul, Tafti Leo
 * @date 2019
 */

#include "tlb_hrchy_mng.h"
#include "tlb_hrchy.h"
#include "page_walk.h"
#include "addr_mng.h"
#include "error.h"
#include "addr.h"

static inline uint64_t tag_and_index_from_vaddr(const virt_addr_t* vaddr){
    return virt_addr_t_to_uint64_t(vaddr)>>PAGE_OFFSET;
}

static inline uint32_t tag_from_tag_and_index(uint64_t tag_and_index, const size_t tlb_lines){
    return tag_and_index / tlb_lines;
}

static inline uint8_t index_from_tag_and_index(uint64_t tag_and_index, const size_t tlb_lines){
    return tag_and_index % tlb_lines;
}

//=========================================================================

#define FLUSH(tlb_entry_type, TLB_LINES) \
    do { \
        tlb_entry_type* tlb_arr = (tlb_entry_type*)tlb; \
        for(size_t i = 0; i < TLB_LINES; i++) { \
            tlb_arr[i].v = INVALID; \
            tlb_arr[i].tag = 0; \
            tlb_arr[i].phy_page_num = 0; \
        } \
    } while(0)

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

#define HIT_TLB(tlb_entry_type, TLB_LINES) \
    do { \
        uint64_t tag_and_index = tag_and_index_from_vaddr(vaddr); \
        uint32_t tag = tag_from_tag_and_index(tag_and_index, TLB_LINES); \
        uint8_t index = index_from_tag_and_index(tag_and_index, TLB_LINES); \
        \
        tlb_entry_type tlb_entry = ((tlb_entry_type*)tlb)[index]; \
        \
        if (tlb_entry.v == VALID && tlb_entry.tag == tag){ \
            /*Entry was found in TLB*/ \
            paddr->phy_page_num = tlb_entry.phy_page_num; \
            paddr->page_offset = vaddr->page_offset; \
            return HIT; \
        }else{ \
            return MISS; \
        } \
    } while(0)

int tlb_hit( const virt_addr_t * vaddr,
             phy_addr_t * paddr,
             const void  * tlb,
             tlb_t tlb_type){

    if(vaddr == NULL || paddr == NULL || tlb == NULL) {
        return MISS;
    }

    switch (tlb_type)
    {
    case L1_ITLB:
        HIT_TLB(l1_itlb_entry_t, L1_ITLB_LINES);
        break;
    case L1_DTLB:
        HIT_TLB(l1_dtlb_entry_t, L1_DTLB_LINES);
        break;
    case L2_TLB:
        HIT_TLB(l2_tlb_entry_t, L2_TLB_LINES);
        break;
    default:
        M_EXIT(ERR_BAD_PARAMETER, "%s", "Unrecognized TLB type");
        break;
    }
}

#undef HIT_TLB

//=========================================================================

#define INSERT(tlb_entry_type, number_lines) \
  do { \
    M_REQUIRE(line_index < number_lines, ERR_BAD_PARAMETER, "%s", "wrong tlb insert"); \
    ((tlb_entry_type*) tlb)[line_index] = *((tlb_entry_type*) tlb_entry); \
  } while(0)

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

#define INIT(tlb_entry_type, TLB_LINES_BITS) \
    do { \
        tlb_entry_type* entry = (tlb_entry_type*)tlb_entry; \
        entry->v = VALID; \
        entry->tag = virt_addr_t_to_virtual_page_number(vaddr)>>TLB_LINES_BITS; \
        entry->phy_page_num = paddr->phy_page_num; \
    } while(0)

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

#define INSERT(tlb_entry_type, TLB_TYPE, TLB_LINES, tlb) \
    do { \
        tlb_entry_type new_entry; \
        tlb_entry_init(vaddr, paddr, &new_entry, TLB_TYPE); \
        \
        uint64_t tag_and_index = tag_and_index_from_vaddr(vaddr); \
        uint8_t index = index_from_tag_and_index(tag_and_index, TLB_LINES); \
        tlb_insert(index, &new_entry, tlb, TLB_TYPE); \
    } while(0)

// TODO I think we can merge the last two together
#define INSERT_L2_AND_INVALIDATE_L1(l1_tlb, L1_TLB_LINES, L1_TLB_LINES_BITS) \
    do { \
        /*Create and init. a new L2 tlb entry*/ \
        l2_tlb_entry_t new_entry; \
        tlb_entry_init(vaddr, paddr, &new_entry, L2_TLB); \
        uint64_t tag_and_index = tag_and_index_from_vaddr(vaddr); \
        uint8_t index = index_from_tag_and_index(tag_and_index, L2_TLB_LINES); \
        \
        /*If l2 entry was also in the other l1 tlb, invalidate it*/ \
        INVALIDATE_IF_NECESSARY(l1_tlb, L1_TLB_LINES, L1_TLB_LINES_BITS); \
        \
        /*Finally, insert the new entry in the L2*/ \
        tlb_insert(index, &new_entry, l2_tlb, L2_TLB); \
    } while(0)

#define MASK4 15
#define MASK2 3
#define INVALIDATE_IF_NECESSARY(l1_tlb, L1_TLB_LINES, L1_TLB_LINES_BITS) \
    do { \
        /*Get the previous entry in L2 TLB*/\
        l2_tlb_entry_t old_entry = l2_tlb[index];\
        if(old_entry.v == VALID){ \
            /*Convert l2 entry tag and index into l1 entry tag and index*/\
            uint8_t old_l1_index = index & MASK4 ;\
            uint32_t old_l1_tag = (old_entry.tag << (L2_TLB_LINES_BITS - L1_TLB_LINES_BITS));\
            old_l1_tag |= (index >> L1_TLB_LINES_BITS) & MASK2;\
            /*Search the l1 tlb and invalidate if tag matches*/\
            if(l1_tlb[old_l1_index].tag == old_l1_tag){ l1_tlb[old_l1_index].v = INVALID; }\
        } \
    } while(0)

int tlb_search( const void * mem_space,
                const virt_addr_t * vaddr,
                phy_addr_t * paddr,
                mem_access_t access,
                l1_itlb_entry_t * l1_itlb,
                l1_dtlb_entry_t * l1_dtlb,
                l2_tlb_entry_t * l2_tlb,
                int* hit_or_miss){

    M_REQUIRE_NON_NULL(mem_space);
    M_REQUIRE_NON_NULL(vaddr);
    M_REQUIRE_NON_NULL(paddr);
    M_REQUIRE_NON_NULL(l1_itlb);
    M_REQUIRE_NON_NULL(l1_dtlb);
    M_REQUIRE_NON_NULL(l2_tlb);
    M_REQUIRE_NON_NULL(hit_or_miss);

    //Search in appropriate L1 TLB
    switch (access)
    {
    case INSTRUCTION:
        *hit_or_miss = tlb_hit(vaddr, paddr, l1_itlb, L1_ITLB);
        break;
    case DATA:
        *hit_or_miss = tlb_hit(vaddr, paddr, l1_dtlb, L1_DTLB);
        break;
    default:
        M_EXIT(ERR_BAD_PARAMETER, "%s", "Unrecognized memory access type");
        break;
    }

    if(*hit_or_miss == MISS){
        //Search in L2 TLB
        *hit_or_miss = tlb_hit(vaddr, paddr, l2_tlb, L2_TLB);
        if(*hit_or_miss == HIT){

            //Update appropriate L1 TLB with data found in L2 TLB
            switch (access)
            {
            case INSTRUCTION:
                INSERT(l1_itlb_entry_t, L1_ITLB, L1_ITLB_LINES, l1_itlb);
                break;
            case DATA:
                INSERT(l1_dtlb_entry_t, L1_DTLB, L1_DTLB_LINES, l1_dtlb);
                break;
            default:
                M_EXIT(ERR_BAD_PARAMETER, "%s", "Unrecognized memory access type");
                break;
            }
        }else{ //L2 MISS
            //Translate the virtual address
            M_EXIT_IF_ERR(page_walk(mem_space, vaddr, paddr), "Problem translating virtual address");

            //Insert a new entry in the appropriate L1 TLB for this translation
            //and invalidate corresp. entry in the other L1 TLB (if it was previously valid)
            switch (access)
            {
            case INSTRUCTION:
                INSERT_L2_AND_INVALIDATE_L1(l1_dtlb, L1_DTLB_LINES, L1_DTLB_LINES_BITS);
                INSERT(l1_itlb_entry_t, L1_ITLB, L1_ITLB_LINES, l1_itlb);
                break;
            case DATA:
                INSERT_L2_AND_INVALIDATE_L1(l1_itlb, L1_ITLB_LINES, L1_ITLB_LINES_BITS);
                INSERT(l1_dtlb_entry_t, L1_DTLB, L1_DTLB_LINES, l1_dtlb);
                break;
            default:
                M_EXIT(ERR_BAD_PARAMETER, "%s", "Unrecognized memory access type");
                break;
            }
        }
    }
    return ERR_NONE;
}

 #undef INSERT
 #undef INSERT_L2_AND_INVALIDATE_L1
 #undef INVALIDATE_IF_NECESSARY
