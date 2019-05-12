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

// TODO remove
// also DO NOT run feedback with this anywhere
static void print_content(l1_itlb_entry_t * l1_itlb, l1_dtlb_entry_t * l1_dtlb, l2_tlb_entry_t * l2_tlb) {
  printf("ITLB\n");
  for(int i = 0; i < L1_ITLB_LINES; i++) printf("   I[%d] : %x  V=%d\n", i, l1_itlb[i].tag, l1_itlb[i].v);
  printf("DTLB\n");
  for(int i = 0; i < L1_DTLB_LINES; i++) printf("   D[%d] : %x  V=%d\n", i, l1_dtlb[i].tag, l1_dtlb[i].v);
  printf("L2TLB\n");
  for(int i = 0; i < L2_TLB_LINES; i++)  printf("   L2[%d] : %x  V=%d\n", i, l2_tlb[i].tag, l2_tlb[i].v);
}

static inline uint64_t tag_and_index_from_vaddr(const virt_addr_t* vaddr){
    return virt_addr_t_to_uint64_t(vaddr)>>PAGE_OFFSET;
}

static inline uint32_t tag_from_tag_and_index(uint64_t tag_and_index, const size_t tlb_lines){
    return tag_and_index / tlb_lines;
}

static inline uint8_t index_from_tag_and_index(uint64_t tag_and_index, const size_t tlb_lines){
    return tag_and_index % tlb_lines;
}

/**
 * @brief Clean a TLB (invalidate, reset...).
 *
 * This function erases all TLB data.
 * @param  tlb (generic) pointer to the TLB
 * @param tlb_type an enum to distinguish between different TLBs
 * @return  error code
 */
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
/**
 * @brief Insert an entry to a tlb. Eviction policy is simple since
 *        direct mapping is used.
 * @param line_index the number of the line to overwrite
 * @param tlb_entry pointer to the tlb entry to insert
 * @param tlb pointer to the TLB
 * @param tlb_type to distinguish between different TLBs
 * @return  error code
 */
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
/**
 * @brief Initialize a TLB entry
 * @param vaddr pointer to virtual address, to extract tlb tag
 * @param paddr pointer to physical address, to extract physical page number
 * @param tlb_entry pointer to the entry to be initialized
 * @param tlb_type to distinguish between different TLBs
 * @return  error code
 */

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

#define INSERT(tlb_entry_type, TLB_TYPE, TLB_LINES, tlb) \
    do { \
        tlb_entry_type new_entry; \
        tlb_entry_init(vaddr, paddr, &new_entry, TLB_TYPE); \
        \
        uint64_t tag_and_index = tag_and_index_from_vaddr(vaddr); \
        uint8_t index = index_from_tag_and_index(tag_and_index, TLB_LINES); \
        tlb_insert(index, &new_entry, tlb, TLB_TYPE); \
    } while(0)

//TODO : see if this passes make feedback
//TODO : this is almost the same as INSERT just above with params to insert in L2 tlb, except for the INVALIDATE_IF_NECESSARY call.
//      I do not see a clean way of merging these two to avoid code duplication, other than passing an additional boolean value
//      and do something like :
//      if(must_invalidate){
//            INVALIDATE_IF_NECESSARY(...)
//      }
//      It would be more compact, but less readable and makes no sense when we simply want to insert in a L1 tlb

#define INSERT_L2_AND_INVALIDATE_L1(l1_tlb, L1_TLB_LINES) \
    do { \
        /*Create and init. a new L2 tlb entry*/ \
        l2_tlb_entry_t new_entry; \
        tlb_entry_init(vaddr, paddr, &new_entry, L2_TLB); \
        uint64_t tag_and_index = tag_and_index_from_vaddr(vaddr); \
        uint8_t index = index_from_tag_and_index(tag_and_index, L2_TLB_LINES); \
        \
        /*If l2 entry was also in the other l1 tlb, invalidate it*/ \
        INVALIDATE_IF_NECESSARY(l1_tlb, L1_DTLB_LINES); \
        \
        /*Finally, insert the new entry both in the L2 and the INSTRUCTION tlbs*/ \
        tlb_insert(index, &new_entry, l2_tlb, L2_TLB); \
    } while(0)


#define INVALIDATE_IF_NECESSARY(l1_tlb, L1_TLB_LINES) \
    do { \
        /*Get the previous entry in L2 TLB*/\
        l2_tlb_entry_t old_entry = l2_tlb[index];\
        if(old_entry.v == VALID){ \
            /*Concat the l2 entry tag and index*/\
            uint64_t old_l2_tag_and_index = (old_entry.tag << L2_TLB_LINES_BITS) | index;\
            \
            /*Split again into l1 entry tag and index*/\
            uint32_t old_l1_tag = tag_from_tag_and_index(old_l2_tag_and_index, L1_TLB_LINES);\
            uint8_t old_l1_index = index_from_tag_and_index(old_l2_tag_and_index, L1_TLB_LINES);\
            \
            /*Search the l1 tlb and invalidate if tag matches*/\
            printf("evicting index %d in %s:", old_l1_index, l1_tlb == l1_itlb? "ITLB" : "DTLB");\
            if(l1_tlb[old_l1_index].tag == old_l1_tag){printf("eviction!"); l1_tlb[old_l1_index].v = INVALID; }\
            putchar('\n');\
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
                INSERT_L2_AND_INVALIDATE_L1(l1_dtlb, L1_DTLB_LINES);
                INSERT(l1_itlb_entry_t, L1_ITLB, L1_ITLB_LINES, l1_itlb);
                break;
            case DATA:
                INSERT_L2_AND_INVALIDATE_L1(l1_itlb, L1_ITLB_LINES);
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
