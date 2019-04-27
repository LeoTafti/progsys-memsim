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
  for(size_t i = 0; i < TLB_LINES; i++) {
    tlb[i].v = INVALID;
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

  //FIXME: Léo – We cannot use M_REQUIRE_NON_NULL because here, we need to return 0 if somehting went wrong

  uint64_t tag = virt_addr_t_to_uint64_t(vaddr)>>PAGE_OFFSET;
  //uint32_t line_index = 0; //TODO: Léo – We can remove that, it is redundant if we also keep the node when found
  node_t* m = NULL;
  //int hit = 0       //TODO : Clearer to do the assignement at the end

  for_all_nodes_reverse(n, replacement_policy->ll) {
      if(tlb[n->value].tag == tag && tlb[n->value].v == VALID) { //TODO: Léo – v is 1 if valid, but 1 == false so we need the explicit equality check
        //line_index = n->value;
        //hit = 1;
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
  int hit_or_miss;
  if(m != NULL) {
    hit_or_miss = HIT;
    //set paddr
    paddr->phy_page_num = tlb[m->value].phy_page_num;
    paddr->page_offset = vaddr->page_offset;
    //update replacement policy
    replacement_policy->move_back(replacement_policy->ll, m);
  }else{
    hit_or_miss = MISS;
  }

  return hit_or_miss;

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

  tlb_entry->v = VALID;
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
  
  if(*hit_or_miss == MISS) {

      //translate vaddr
      M_EXIT_IF_ERR(page_walk(mem_space, vaddr, paddr), "page fault!");

      //init tlb_entry
      tlb_entry_t new_entry;
      tlb_entry_init(vaddr, paddr, &new_entry);

      //insert in tlb at the front's line index
      node_t* lru = replacement_policy->ll->front;
      tlb[lru->value] = new_entry;

      //set mru position
      replacement_policy->move_back(replacement_policy->ll, lru);
  }

  return ERR_NONE;
}
