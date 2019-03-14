/**
 * @file test-addr.c
 * @brief test code for virtual and physical address types and functions
 *
 * @author Jean-Cédric Chappelier and Atri Bhattacharyya
 * @author Léo Tafti and Paul Juillard
 * @date Jan 2019
 */

// ------------------------------------------------------------
// for thread-safe randomization
#include <time.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>

// ------------------------------------------------------------
#include <check.h>
#include <inttypes.h>
#include <assert.h>

#include "tests.h"
#include "util.h"
#include "addr.h"
#include "addr_mng.h"

// ------------------------------------------------------------
// Preliminary stuff

#define random_value(TYPE)	(TYPE)(rand() % 4096)
#define REPEAT(N) for(unsigned int i_ = 1; i_ <= N; ++i_)

uint64_t generate_Nbit_random(int n)
{
    uint64_t val = 0;
    for(int i = 0; i < n; i++) {
        val <<= 1;
        val |= (unsigned)(rand() % 2);
    }
    return val;
}

    //tests for virtual_addr_t_to_unint64_t
    //tests for init_phy_addr

// ------------------------------------------------------------
// First basic tests, provided to students

START_TEST(init_virt_addr_test)
{
    
    virt_addr_t vaddr;
    zero_init_var(vaddr);

    srand(time(NULL) ^ getpid() ^ pthread_self());
#pragma GCC diagnostic pop

    REPEAT(100) {
        uint16_t pgd_entry   = (uint16_t) generate_Nbit_random(PGD_ENTRY);
        uint16_t pud_entry   = (uint16_t) generate_Nbit_random(PUD_ENTRY);
        uint16_t pmd_entry   = (uint16_t) generate_Nbit_random(PMD_ENTRY);
        uint16_t pte_entry   = (uint16_t) generate_Nbit_random(PTE_ENTRY);
        uint16_t page_offset = (uint16_t) generate_Nbit_random(PAGE_OFFSET);

        (void)init_virt_addr(&vaddr, pgd_entry, pud_entry, pmd_entry, pte_entry, page_offset);

		ck_assert_ptr_nonnull(&vaddr);

        ck_assert_int_eq(vaddr.pgd_entry, pgd_entry);
        ck_assert_int_eq(vaddr.pud_entry, pud_entry);
        ck_assert_int_eq(vaddr.pmd_entry, pmd_entry);
        ck_assert_int_eq(vaddr.pte_entry, pte_entry);
        ck_assert_int_eq(vaddr.page_offset, page_offset);
    }
}
END_TEST


START_TEST(init_virt_addr64_test)
{
    virt_addr_t vaddr;
    zero_init_var(vaddr);

    srand(time(NULL) ^ getpid() ^ pthread_self());
#pragma GCC diagnostic pop

    // check reserved bits are zero
    uint64_t vaddr64 = 0xFFFFFFFFFFFFFFFF; //all ones
    init_virt_addr64(&vaddr, vaddr64);
    ck_assert_int_eq(vaddr.reserved, 0);
    // check all zeros
    vaddr64 = 0x0;
    init_virt_addr64(&vaddr, vaddr64);
    ck_assert_int_eq(vaddr.reserved, 0);
	ck_assert_int_eq(vaddr.pgd_entry, 0);
	ck_assert_int_eq(vaddr.pud_entry, 0);
	ck_assert_int_eq(vaddr.pmd_entry, 0);
	ck_assert_int_eq(vaddr.pte_entry, 0);
	ck_assert_int_eq(vaddr.page_offset, 0);
	// check bit 0 of page offset is 1
	vaddr64 = 0x1;
	init_virt_addr64(&vaddr, vaddr64);
	ck_assert_int_eq(vaddr.pgd_entry, 1);
	// check bit 0 of PTE is 1 and previous are zero
	vaddr64 = 0x1000;
	init_virt_addr64(&vaddr, vaddr64);
	ck_assert_int_eq(vaddr.pgd_entry, 1);
	ck_assert_int_eq(vaddr.page_offset, 0);
	// check bit 0 of PMD is 1 and previous are zero
	vaddr64 = 0x200000;
	init_virt_addr64(&vaddr, vaddr64);
	ck_assert_int_eq(vaddr.pmd_entry, 1);
	ck_assert_int_eq(vaddr.pte_entry, 0);
	ck_assert_int_eq(vaddr.page_offset, 0);
	// check bit 0 of PUD is 1 and previous are zero
	vaddr64 = 0x40000000;
	init_virt_addr64(&vaddr, vaddr64);
	ck_assert_int_eq(vaddr.pud_entry, 1);
	ck_assert_int_eq(vaddr.pmd_entry, 0);
	ck_assert_int_eq(vaddr.pte_entry, 0);
	ck_assert_int_eq(vaddr.page_offset, 0);
	// check bit 0 of PGD is 1 and previous are zero
	vaddr64 = 0x8000000000;
	init_virt_addr64(&vaddr, vaddr64);
	ck_assert_int_eq(vaddr.pgd_entry, 1);
	ck_assert_int_eq(vaddr.pud_entry, 0);
	ck_assert_int_eq(vaddr.pmd_entry, 0);
	ck_assert_int_eq(vaddr.pte_entry, 0);
	ck_assert_int_eq(vaddr.page_offset, 0);
	
	// check null caught
	ck_assert_bad_param(init_virt_addr64(NULL, vaddr64));
	ck_assert_bad_param(init_virt_addr64(&vaddr, NULL));
	// check not 64
	uint16_t not64 = 0x0;
	ck_assert_bad_param(init_virt_addr64(&vaddr, not64));
	// check non_null pointer after init
	init_virt_addr64(&vaddr, vaddr64);
	ck_assert_ptr_nonnull(&vaddr);
	
}
END_TEST

START_TEST(virt_addr_t_to_virtual_page_number_test)
{
    
    virt_addr_t vaddr;
    virt_addr_t vpgnb;
    zero_init_var(vaddr);
    zero_init_var(vpgnb);
    

    srand(time(NULL) ^ getpid() ^ pthread_self());
#pragma GCC diagnostic pop

    // check null caught
	ck_assert_bad_param(virt_addr_t_to_virtual_page_number(NULL));

    REPEAT(100) {
        uint16_t pgd_entry   = (uint16_t) generate_Nbit_random(PGD_ENTRY);
        uint16_t pud_entry   = (uint16_t) generate_Nbit_random(PUD_ENTRY);
        uint16_t pmd_entry   = (uint16_t) generate_Nbit_random(PMD_ENTRY);
        uint16_t pte_entry   = (uint16_t) generate_Nbit_random(PTE_ENTRY);
        uint16_t page_offset = (uint16_t) generate_Nbit_random(PAGE_OFFSET);

        (void)init_virt_addr(&vaddr, pgd_entry, pud_entry, pmd_entry, pte_entry, page_offset);
		(void)init_virt_addr64(&vpgnb, virt_addr_t_to_virtual_page_number(&vaddr));
		
        ck_assert_ptr_nonnull(&vpgnb);

        ck_assert_int_eq(vpgnb.pgd_entry, 0);
        ck_assert_int_eq(vpgnb.pud_entry, pgd_entry);
        ck_assert_int_eq(vpgnb.pmd_entry, pud_entry);
        ck_assert_int_eq(vpgnb.pte_entry, pmd_entry);
        ck_assert_int_eq(vpgnb.page_offset, pte_entry);
        
    }
}
END_TEST


//tests for virtual_addr_t_to_uint64_t
START_TEST(virtual_addr_t_to_uint64_t_test)
{
    virt_addr_t vaddr;
    zero_init_var(vaddr);
    uint64_t in = 0;
    uint64_t out = 0;
    

    srand(time(NULL) ^ getpid() ^ pthread_self());
#pragma GCC diagnostic pop

    // check null caught
	ck_assert_bad_param(virt_addr_t_to_uint64_t(NULL));

	// check gen from 64 and translate to 64 is identity
    REPEAT(100) {
		in = 0; // reset and reserved at 0
        in |= (uint64_t) generate_Nbit_random(48);
        
		(void)  init_virt_addr64(&vaddr, in);
        out = virt_addr_t_to_uint64_t(&vaddr);

        ck_assert_int_eq(out, in);
    }
}
END_TEST


// ======================================================================
Suite* addr_test_suite()
{
    Suite* s = suite_create("Virtual and Physical Address Tests");

    Add_Case(s, tc1, "tests for init_virt_addr");
    tcase_add_test(tc1, init_virt_addr_test);
    Add_Case(s, tc2, "tests for init_virt_addr64");
    tcase_add_test(tc2, init_virt_addr64_test);
    Add_Case(s, tc3, "tests for virt_addr_t_to_virtual_page_number");
    tcase_add_test(tc3, virt_addr_t_to_virtual_page_number_test);
    Add_Case(s, tc4, "tests for virtual_addr_t_to_uint64_t");
    tcase_add_test(tc4, virtual_addr_t_to_uint64_t_test);

    return s;
}

TEST_SUITE(addr_test_suite)
