/**
 * @file page_walk.c
 * @brief defines a function to walk through page-tables and retrieve the physical address
 *
 * @author Juillard Paul, Tafti Leo
 * @date 2019
 */

#include "addr.h"
#include "addr_mng.h"
#include "error.h"

#define PGD_START 0
static inline pte_t read_page_entry(const pte_t * start, pte_t page_start, uint16_t index);

int page_walk(const void* mem_space, const virt_addr_t* vaddr, phy_addr_t* paddr) {
	
	M_REQUIRE_NON_NULL(mem_space);
	M_REQUIRE_NON_NULL(vaddr);
	
	//initialized to the beginning of addressed space
	pte_t walker = PGD_START;
	
	//get PUD page from PGD
	walker = read_page_entry(mem_space, walker, vaddr->pgd_entry);

	//get PMD entry from PUD
	walker = read_page_entry(mem_space, walker, vaddr->pud_entry);

	//get PTE entry from PMD
	walker = read_page_entry(mem_space, walker, vaddr->pmd_entry);

	//get physical_page_number from PTE
	walker = read_page_entry(mem_space, walker, vaddr->pte_entry);

	//init phy_add	r
	M_REQUIRE(init_phy_addr(paddr, walker, vaddr->page_offset) == ERR_NONE, ERR_MEM, "%s", "page walk unsuccesful");

	return ERR_NONE;
}

#define BYTES_IN_WORD 4
/**
 * @brief read the entry index from page starting at page_start
 * @param start the beginning of the addressed memory space
 * @param page_start the address (in bytes) of the first entry of the page
 * @param index the (word) index of the entry to read
 */
static inline pte_t read_page_entry(const pte_t * start, pte_t page_start, uint16_t index) {
	pte_t i = 0;
	
	//page_start is in bytes
	i += page_start/BYTES_IN_WORD;
	
	//index is in words, must adapt it to byte addressing
	i += index;

	return start[i];
}
