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


// ===================== TC1 ===================== 
START_TEST(init_virt_addr_test)
{
    virt_addr_t vaddr;
    zero_init_var(vaddr);
    uint16_t pgd_entry   = 0;
	uint16_t pud_entry   = 0;
	uint16_t pmd_entry   = 0;
	uint16_t pte_entry   = 0;
	uint16_t page_offset = 0;

    srand(time(NULL) ^ getpid() ^ pthread_self());
#pragma GCC diagnostic pop

	// check ERR_NONE on correct input
	ck_assert_err_none( init_virt_addr(&vaddr, pgd_entry, pud_entry, pmd_entry, pte_entry, page_offset));

    REPEAT(100) {
        pgd_entry   = (uint16_t) generate_Nbit_random(PGD_ENTRY);
        pud_entry   = (uint16_t) generate_Nbit_random(PUD_ENTRY);
        pmd_entry   = (uint16_t) generate_Nbit_random(PMD_ENTRY);
        pte_entry   = (uint16_t) generate_Nbit_random(PTE_ENTRY);
        page_offset = (uint16_t) generate_Nbit_random(PAGE_OFFSET);

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

// ===================== TC2 ===================== 
START_TEST(init_virt_addr64_test)
{
    virt_addr_t vaddr;
    zero_init_var(vaddr);
    uint64_t vaddr64 = 0;

    srand(time(NULL) ^ getpid() ^ pthread_self());
#pragma GCC diagnostic pop

	// check ERR_NONE on correct input
	ck_assert_err_none(init_virt_addr64(&vaddr,vaddr64));

    // check reserved bits are zero
    vaddr64 = 0xFFFFFFFFFFFFFFFF; //all ones
    init_virt_addr64(&vaddr, vaddr64);
    ck_assert_int_eq(vaddr.reserved, 0);
    
    // check all zeros
    vaddr64 = 0;
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
	ck_assert_int_eq(vaddr.page_offset, 1);
	// check bit 0 of PTE is 1 and previous are zero
	vaddr64 = 0x1000;
	init_virt_addr64(&vaddr, vaddr64);
	ck_assert_int_eq(vaddr.pte_entry, 1);
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
	
	// check not 64
	// TODO should this error?
	//uint16_t not64 = 0x0;
	//ck_assert_bad_param(init_virt_addr64(&vaddr, not64));
	
	// check non_null pointer after init
	init_virt_addr64(&vaddr, vaddr64);
	ck_assert_ptr_nonnull(&vaddr);
	
}
END_TEST

// ===================== TC3 ===================== 
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
	
	// check ERR_NONE for correct input
	// TODO: how to check for error if the function returns a value?
	// (void) init_virt_addr64(&vaddr, (uint64_t) 0xC0FEE); // some arbitrary value
	// ck_assert_err_none(virt_addr_t_to_virtual_page_number(&vaddr));

	// randomized check for correctness
	// coding this more rigorously  <=> coding the method...
	
    REPEAT(100) {
        uint16_t pgd_entry   = (uint16_t) generate_Nbit_random(PGD_ENTRY);
        uint16_t pud_entry   = (uint16_t) generate_Nbit_random(PUD_ENTRY);
        uint16_t pmd_entry   = (uint16_t) generate_Nbit_random(PMD_ENTRY);
        uint16_t pte_entry   = (uint16_t) generate_Nbit_random(PTE_ENTRY);
        uint16_t page_offset = (uint16_t) generate_Nbit_random(PAGE_OFFSET);

        (void)init_virt_addr(&vaddr, pgd_entry, pud_entry, pmd_entry, pte_entry, page_offset);
		(void)init_virt_addr64(&vpgnb, virt_addr_t_to_virtual_page_number(&vaddr));
		
        ck_assert_ptr_nonnull(&vpgnb);
        
    }
}
END_TEST


// ===================== TC4 ===================== 
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
	
	// check ERR_NONE on correct input
	ck_assert_err_none(virt_addr_t_to_uint64_t(&vaddr));

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

// ===================== TC5 ===================== 
START_TEST(init_phy_addr_test)
{
    phy_addr_t pa;
    phy_addr_t pb;
    zero_init_var(pa); // TODO what does this actually do?
    zero_init_var(pb);
    uint32_t page_begin = 0;
    uint32_t page_offset = 0;
    

    srand(time(NULL) ^ getpid() ^ pthread_self());
#pragma GCC diagnostic pop

    // check null caught
	ck_assert_bad_param(init_phy_addr(NULL, page_begin, page_offset));

	// check ERR_NONE on correct input
	ck_assert_err_none(init_phy_addr(&pa, page_begin, page_offset));

	// check PAGE-OFFSET-lsbs are not significant
	init_phy_addr(&pa, page_begin, page_offset);
	page_begin = 0xFFF; // PAGE-OFFSET lsbs set to 1
	init_phy_addr(&pb, page_begin, page_offset);
	ck_assert_int_eq(pa.phy_page_num, pb.phy_page_num);
	
	// check msb of page_offset are not significant
	page_offset = 0; 
	init_phy_addr(&pa, page_begin, page_offset);
	page_offset = 0xF000; //only bits not in PAGE-OFFSET-lsbs set to 1
	init_phy_addr(&pb, page_begin, page_offset);
	ck_assert_int_eq(pa.page_offset, pb.page_offset);
	
	
	/* Entry too big entry for offset is treated as an error
	 * these tests are not relevant, they result in a BAD_PARAM
	 * 
	// randomized  check for correctness based on independance of:
	// page_begin's lsb
	// page_offset's msb
    uint16_t noise4;
    uint16_t noise12;
    REPEAT(100) {
		page_begin  = (uint64_t) generate_Nbit_random(32);
        page_offset = (uint64_t) generate_Nbit_random(16);
        
        noise12 = (uint16_t) generate_Nbit_random(12);
        noise4 = (uint16_t) generate_Nbit_random(4);
        
		(void) init_phy_addr(&pa, page_begin, page_offset);
		
		// add noise
		// page begin 12lsb should not impact
		// page offset msbs should not impact
		page_begin |= noise12;
		page_offset |= noise4 << 12;
		
		ck_assert_bad_param(init_phy_addr(&pb, page_begin, page_offset));
		
		ck_assert_int_eq(pa.phy_page_num, pb.phy_page_num);
		ck_assert_int_eq(pa.page_offset, pb.page_offset);
    }
    */
    
}
END_TEST

// ======================================================================
Suite* addr_test_suite()
{
	printf("================ STARTING TEST-ADDR TESTS ================\n");
	
    Suite* s = suite_create("Virtual and Physical Address Tests");

    Add_Case(s, tc1, "VIRT ADDR");
    tcase_add_test(tc1, init_virt_addr_test);
    
    Add_Case(s, tc2, "VIRT ADDR");
    tcase_add_test(tc2, init_virt_addr64_test);
    
    Add_Case(s, tc3, "VIRT ADDR TO PAGE_NUM");
    tcase_add_test(tc3, virt_addr_t_to_virtual_page_number_test);
    
    Add_Case(s, tc4, "VIRT ADDR TO INT64");
    tcase_add_test(tc4, virtual_addr_t_to_uint64_t_test);
    
    Add_Case(s, tc5, "PHY ADDR");
    tcase_add_test(tc5, init_phy_addr_test);
    

    return s;
}

TEST_SUITE(addr_test_suite)

