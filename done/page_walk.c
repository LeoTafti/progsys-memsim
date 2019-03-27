#include 
#include 
#include "addr.h";



int page_walk(const void* mem_space, const virt_addr_t* vaddr, phy_addr_t* paddr) {
	
	
	
}





/************** HEPLPER FUNCTIONS ********************/

static inline pte_t read_page_entry(const pte_t * start, pte_t page_start, uint16_t index) { //addr.xxentry
	
	pte_t i = 0;
	i += page_start;
	
	return start[i];
	 
}
