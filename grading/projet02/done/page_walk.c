/**
 * @file page_walk.c
 * @brief defines a function to walk through page-tables and retrieve the physical address
 *
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
	M_REQUIRE_NON_NULL(paddr);

	//initialized to the beginning of addressed space
	pte_t walker = PGD_START;

	//get PUD entry from PGD
	walker = read_page_entry(mem_space, walker, vaddr->pgd_entry);
	M_REQUIRE(walker != 0, ERR_ADDR, "%s", "Mem space probably not initialized");
	
	//get PMD entry from PUD
	walker = read_page_entry(mem_space, walker, vaddr->pud_entry);
	M_REQUIRE(walker != 0, ERR_ADDR, "%s", "Mem space probably not initialized");

	//get PTE entry from PMD
	walker = read_page_entry(mem_space, walker, vaddr->pmd_entry);
	M_REQUIRE(walker != 0, ERR_ADDR, "%s", "Mem space probably not initialized");

	//get physical_page_number from PTE
	walker = read_page_entry(mem_space, walker, vaddr->pte_entry);
	M_REQUIRE(walker != 0, ERR_ADDR, "%s", "Mem space probably not initialized");

	//init phy_addr
	M_REQUIRE(init_phy_addr(paddr, walker, vaddr->page_offset) == ERR_NONE, ERR_MEM, "%s", "page walk unsuccesful");

	return ERR_NONE;
}

#define BYTES_PER_WORD 4
/**
 * @brief read the entry index from page starting at page_start
 * @param start the beginning of the addressed memory space
 * @param page_start the address (in bytes) of the first entry of the page
 * @param index the (word) index of the entry to read
 */
static inline pte_t read_page_entry(const pte_t * start, pte_t page_start, uint16_t index) {
	return start[page_start/BYTES_PER_WORD + index];
}
