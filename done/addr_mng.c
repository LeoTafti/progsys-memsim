/**
 * @file addr_mng.c
 * @brief address management functions (init, print, etc.)
 *
 * @author Juillard Paul, Tafti Leo
 * @date 2019
 */

#include <inttypes.h>

#include "addr.h"
#include "addr_mng.h"
#include "error.h"

#define MAX_9BIT_VALUE 0x1FF
#define MAX_12BIT_VALUE 0xFFF

int init_virt_addr(virt_addr_t * vaddr,
                   uint16_t pgd_entry,
                   uint16_t pud_entry, uint16_t pmd_entry,
                   uint16_t pte_entry, uint16_t page_offset){

	M_REQUIRE_NON_NULL(vaddr);
	M_REQUIRE(pgd_entry  <= MAX_9BIT_VALUE, ERR_BAD_PARAMETER, "PGD entry should be a 9-bit value, was %" PRIX16, pgd_entry);
	M_REQUIRE(pud_entry  <= MAX_9BIT_VALUE, ERR_BAD_PARAMETER, "PUD entry should be a 9-bit value, was %" PRIX16, pud_entry);
	M_REQUIRE(pmd_entry  <= MAX_9BIT_VALUE, ERR_BAD_PARAMETER, "PMD entry should be a 9-bit value, was %" PRIX16, pmd_entry);
	M_REQUIRE(pte_entry  <= MAX_9BIT_VALUE, ERR_BAD_PARAMETER, "PTE entry should be a 9-bit value, was %" PRIX16, pte_entry);
	M_REQUIRE(page_offset <= MAX_12BIT_VALUE, ERR_BAD_PARAMETER, "Page offset should be a 12-bit value, was %" PRIX16, page_offset);

	vaddr->pgd_entry = pgd_entry;
	vaddr->pud_entry = pud_entry;
	vaddr->pmd_entry = pmd_entry;
	vaddr->pte_entry = pte_entry;
	vaddr->page_offset = page_offset;
	vaddr->reserved = 0; //Reserved bits are always set to 0

	return ERR_NONE;
}

int init_virt_addr64(virt_addr_t * vaddr, uint64_t vaddr64){
	M_REQUIRE_NON_NULL(vaddr);

	uint16_t page_offset = vaddr64 & MAX_12BIT_VALUE;

	vaddr64 >>= PAGE_OFFSET;
	uint16_t pte_entry = vaddr64 & MAX_9BIT_VALUE;

	vaddr64 >>= PTE_ENTRY;
	uint16_t pmd_entry = vaddr64 & MAX_9BIT_VALUE;

	vaddr64 >>= PMD_ENTRY;
	uint16_t pud_entry = vaddr64 & MAX_9BIT_VALUE;

	vaddr64 >>= PUD_ENTRY;
	uint16_t pgd_entry = vaddr64 & MAX_9BIT_VALUE;

	return init_virt_addr(vaddr, pgd_entry, pud_entry, pmd_entry, pte_entry, page_offset);
}

int init_phy_addr(phy_addr_t* paddr, uint32_t page_begin, uint32_t page_offset){
	M_REQUIRE_NON_NULL(paddr);
	M_REQUIRE(page_offset <= MAX_12BIT_VALUE, ERR_BAD_PARAMETER, "Page offset should be a 12-bit value, was %" PRIX32, page_offset);
	
	paddr->phy_page_num = page_begin >> PAGE_OFFSET; //page_begin (PAGE_OFFSET) LSbs are discarded
	paddr->page_offset = page_offset;

	return ERR_NONE;
}

uint64_t virt_addr_t_to_uint64_t(const virt_addr_t * vaddr){
	M_REQUIRE_NON_NULL(vaddr);
	return (virt_addr_t_to_virtual_page_number(vaddr) << PAGE_OFFSET) | vaddr->page_offset;
}

uint32_t phy_addr_t_to_uint32_t(const phy_addr_t* paddr){
	M_REQUIRE_NON_NULL(paddr);
	return (paddr->phy_page_num << PAGE_OFFSET) | paddr->page_offset;
}

uint64_t virt_addr_t_to_virtual_page_number(const virt_addr_t * vaddr){
	M_REQUIRE_NON_NULL(vaddr);

	uint64_t vp_number = 0;

	//Packs x_entries fields into vp_number
	vp_number = vp_number | vaddr->pgd_entry;
	vp_number = (vp_number << PUD_ENTRY) | vaddr->pud_entry;
	vp_number = (vp_number << PMD_ENTRY) | vaddr->pmd_entry;
	vp_number = (vp_number << PTE_ENTRY) | vaddr->pte_entry;

	return vp_number;
}

int print_virtual_address(FILE* where, const virt_addr_t* vaddr){
	unsigned int nb_char = 0;
	if(where != NULL){
		nb_char = fprintf(where, "PGD=0x%" PRIX16 "; PUD=0x%" PRIX16 "; PMD=0x%" PRIX16 "; PTE=0x%" PRIX16 "; offset=0x%" PRIX16,
			vaddr->pgd_entry,
			vaddr->pud_entry,
			vaddr->pmd_entry,
			vaddr->pte_entry,
			vaddr->page_offset);
	}
	return nb_char;
}

int print_physical_address(FILE* where, const phy_addr_t* paddr){
	unsigned int nb_char = 0;
	if(where != NULL){
		nb_char = fprintf(where, "page num=0x%" PRIX32 "; offset=0x%" PRIX32,
			paddr->phy_page_num,
			paddr->page_offset);
	}
	return nb_char;
}
