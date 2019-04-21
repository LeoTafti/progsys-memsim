/**
 * @file tlb_mng.c
 * @brief TLB TLB management functions for fully-associative TLB
 *
 * @author Juillard Paul, Tafti Leo
 * @date 2019
 */

#include "tlb.h"
#include "tlb_mng.h"
#include "addr.h"
#include "addr_mng.h"
#include "list.h"
#include "page_walk.h"
#include "error.h"

int tlb_flush(tlb_entry_t * tlb) {
  M_REQUIRE_NON_NULL(tlb);
  for(int i = 0; i < TLB_LINES; i++) {
    tlb[i].v = 0;
    tlb[i].tag = 0;
    tlb[i].phy_page_num = 0;
  }

  return ERR_NONE;
}

int tlb_hit(const virt_addr_t * vaddr,
            phy_addr_t * paddr,
            const tlb_entry_t * tlb,
            replacement_policy_t * replacement_policy){
  M_REQUIRE_NON_NULL(vaddr);
  M_REQUIRE_NON_NULL(paddr);
  M_REQUIRE_NON_NULL(tlb);
  M_REQUIRE_NON_NULL(replacement_policy);

  uint64_t tag = virt_addr_t_to_uint64_t(vaddr)>>12;
  uint32_t line_index = 0;
  int hit = 0;
  node_t* m = NULL;

  for_all_nodes_reverse(n, replacement_policy->ll) {
      if( tlb[n->value].tag == tag && tlb[n->value].v) {
        line_index = n->value;
        hit = 1;
        m = n;
        break;
      }
  }

  // TODO a while may be more repsentative of what we are doing
  // while version is currently not functional
  /*node_t* n = replacement_policy->ll->back;
  while( (tlb[line_index].tag != tag || !tlb[line_index].v ) && n->previous != NULL){
        n = n->previous;
        line_index = n->value;
  }
  if((tlb[line_index].tag != tag || !tlb[line_index].v ) && n->previous != NULL)
    hit=1;
  */

  if(hit) {
    //set paddr
    paddr->phy_page_num = tlb[line_index].phy_page_num;
    paddr->page_offset = vaddr->page_offset;
    //update replacement policy
    replacement_policy->move_back(replacement_policy->ll, m);
  }

  return hit;

}

int tlb_insert( uint32_t line_index,
                const tlb_entry_t * tlb_entry,
                tlb_entry_t * tlb) {
  M_REQUIRE_NON_NULL(tlb_entry);
  M_REQUIRE_NON_NULL(tlb);

  tlb[line_index].v = tlb_entry->v;
  tlb[line_index].tag = tlb_entry->tag;
  tlb[line_index].phy_page_num = tlb_entry->phy_page_num;

  return ERR_NONE;
}

int tlb_entry_init( const virt_addr_t * vaddr,
                    const phy_addr_t * paddr,
                    tlb_entry_t * tlb_entry) {
  M_REQUIRE_NON_NULL(vaddr);
  M_REQUIRE_NON_NULL(paddr);
  M_REQUIRE_NON_NULL(tlb_entry);

  tlb_entry->v = 1;
  tlb_entry->tag = virt_addr_t_to_uint64_t(vaddr)>>PAGE_OFFSET;
  tlb_entry->phy_page_num = paddr->phy_page_num;

  return ERR_NONE;
}


int tlb_search( const void * mem_space,
                const virt_addr_t * vaddr,
                phy_addr_t * paddr,
                tlb_entry_t * tlb,
                replacement_policy_t * replacement_policy,
                int* hit_or_miss) {
  M_REQUIRE_NON_NULL(mem_space);
  M_REQUIRE_NON_NULL(vaddr);
  M_REQUIRE_NON_NULL(paddr);
  M_REQUIRE_NON_NULL(tlb);
  M_REQUIRE_NON_NULL(replacement_policy);

  *hit_or_miss = tlb_hit(vaddr, paddr, tlb, replacement_policy);
  if( !(*hit_or_miss)) {

      //translate vaddr
      M_EXIT_IF_ERR(page_walk(mem_space, vaddr, paddr), "page fault!");

      //init tlb_entry
      tlb_entry_t new_entry;
      tlb_entry_init( vaddr, paddr, &new_entry);

      //insert in tlb at the front's line index
      node_t* lru = replacement_policy->ll->front;
      tlb[lru->value] = new_entry;

      //set mru position
      replacement_policy->move_back(replacement_policy->ll, lru);
  }

  return ERR_NONE;
}
