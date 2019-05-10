/**
 * @memory.c
 * @brief memory management functions (dump, init from file, etc.)
 *
 * @author Jean-Cédric Chappelier
 * @date 2018-19
 */

#if defined _WIN32  || defined _WIN64
#define __USE_MINGW_ANSI_STDIO 1
#endif

#include <stdio.h>  // for FILE
#include <stdlib.h> // for strtoul
#include <stdint.h> // for size_t
#include <string.h> // for memset()
#include <inttypes.h> // for SCNx macros
#include <assert.h>

#include "memory.h"
#include "page_walk.h"
#include "addr_mng.h"
#include "util.h" // for SIZE_T_FMT
#include "addr.h" // for virt_addr_t and phy_addr_t
#include "error.h"

// ======================================================================
/**
 * @brief Tool function to print an address.
 *
 * @param show_addr the format how to display addresses; see addr_fmt_t type in memory.h
 * @param reference the reference address; i.e. the top of the main memory
 * @param addr the address to be displayed
 * @param sep a separator to print after the address (and its colon, printed anyway)
 *
 */
static void address_print(addr_fmt_t show_addr, const void* reference,
                          const void* addr, const char* sep)
{
    switch (show_addr) {
    case POINTER:
        (void)printf("%p", addr);
        break;
    case OFFSET:
        (void)printf("%zX", (const char*)addr - (const char*)reference);
        break;
    case OFFSET_U:
        (void)printf(SIZE_T_FMT, (const char*)addr - (const char*)reference);
        break;
    default:
        // do nothing
        return;
    }
    (void)printf(":%s", sep);
}

// ======================================================================
/**
 * @brief Tool function to print the content of a memory area
 *
 * @param reference the reference address; i.e. the top of the main memory
 * @param from first address to print
 * @param to first address NOT to print; if less that `from`, nothing is printed;
 * @param show_addr the format how to display addresses; see addr_fmt_t type in memory.h
 * @param line_size how many memory byted to print per stdout line
 * @param sep a separator to print after the address and between bytes
 *
 */
static void mem_dump_with_options(const void* reference, const void* from, const void* to,
                                  addr_fmt_t show_addr, size_t line_size, const char* sep)
{
    assert(line_size != 0);
    size_t nb_to_print = line_size;
    for (const uint8_t* addr = from; addr < (const uint8_t*) to; ++addr) {
        if (nb_to_print == line_size) {
            address_print(show_addr, reference, addr, sep);
        }
        (void)printf("%02"PRIX8"%s", *addr, sep);
        if (--nb_to_print == 0) {
            nb_to_print = line_size;
            putchar('\n');
        }
    }
    if (nb_to_print != line_size) putchar('\n');
}

// ======================================================================
// See memory.h for description
int vmem_page_dump_with_options(const void *mem_space, const virt_addr_t* from,
                                addr_fmt_t show_addr, size_t line_size, const char* sep)
{
#ifdef DEBUG
    debug_print("mem_space=%p\n", mem_space);
    (void)fprintf(stderr, __FILE__ ":%d:%s(): virt. addr.=", __LINE__, __func__);
    print_virtual_address(stderr, from);
    (void)fputc('\n', stderr);
#endif
    phy_addr_t paddr;
    zero_init_var(paddr);

    M_EXIT_IF_ERR(page_walk(mem_space, from, &paddr),
                  "calling page_walk() from vmem_page_dump_with_options()");

#ifdef DEBUG
    (void)fprintf(stderr, __FILE__ ":%d:%s(): phys. addr.=", __LINE__, __func__);
    print_physical_address(stderr, &paddr);
    (void)fputc('\n', stderr);
#endif
    const uint32_t paddr_offset = ((uint32_t) paddr.phy_page_num << PAGE_OFFSET);
    const char * const page_start = (const char *)mem_space + paddr_offset;
    const char * const start = page_start + paddr.page_offset;
    const char * const end_line = start + (line_size - paddr.page_offset % line_size);
    const char * const end   = page_start + PAGE_SIZE;
    debug_print("start=%p (offset=%zX)\n", (const void*) start, start - (const char *)mem_space);
    debug_print("end  =%p (offset=%zX)\n", (const void*) end, end   - (const char *)mem_space) ;
    mem_dump_with_options(mem_space, page_start, start, show_addr, line_size, sep);
    const size_t indent = paddr.page_offset % line_size;
    if (indent == 0) putchar('\n');
    address_print(show_addr, mem_space, start, sep);
    for (size_t i = 1; i <= indent; ++i) printf("  %s", sep);
    mem_dump_with_options(mem_space, start, end_line, NONE, line_size, sep);
    mem_dump_with_options(mem_space, end_line, end, show_addr, line_size, sep);
    return ERR_NONE;
}

/**
 * @brief Create and initialize the whole memory space from a provided
 * (binary) file containing one single dump of the whole memory space.
 *
 * @param filename the name of the memory dump file to read from
 * @param memory (modified) pointer to the begining of the memory
 * @param mem_capacity_in_bytes (modified) total size of the created memory
 * @return error code, *memory shall be NULL in case of error
 *
 */

int mem_init_from_dumpfile(const char* filename, void** memory, size_t* mem_capacity_in_bytes){
	M_REQUIRE_NON_NULL(filename);
	M_REQUIRE_NON_NULL(memory);
	M_REQUIRE_NON_NULL(mem_capacity_in_bytes);

	FILE* mem_dump = fopen(filename, "rb");

	if(mem_dump != NULL){
		//Determine file size
		fseek(mem_dump, 0L, SEEK_END);
		*mem_capacity_in_bytes = (size_t) ftell(mem_dump);
		rewind(mem_dump);

		//Allocate memory
		*memory = calloc(*mem_capacity_in_bytes, 1);

		size_t bytes_read;
		if(*memory != NULL){
			//Initialize the memory from mem_dump file
			bytes_read = fread(*memory, 1, *mem_capacity_in_bytes, mem_dump);
		}
		fclose(mem_dump);

		M_EXIT_IF_NULL(*memory, *mem_capacity_in_bytes);

		if(bytes_read != *mem_capacity_in_bytes){
			free(*memory);
			M_EXIT(ERR_IO, "%s", "Couldn't read the whole memory dump file");
		}
	}else{
		*memory = NULL;
		M_EXIT_ERR(ERR_IO, "Error opening file %s", filename);
	}

	return ERR_NONE;
}


/**
 * @brief Create and initialize the whole memory space from a provided
 * (metadata text) file containing an description of the memory.
 * Its format is:
 *  line1:           TOTAL MEMORY SIZE (size_t)
 *  line2:           PGD PAGE FILENAME
 *  line4:           NUMBER N OF TRANSLATION PAGES (PUD+PMD+PTE)
 *  lines5 to (5+N): LIST OF TRANSLATION PAGES, expressed with two info per line:
 *                       INDEX OFFSET (uint32_t in hexa) and FILENAME
 *  remaining lines: LIST OF DATA PAGES, expressed with two info per line:
 *                       VIRTUAL ADDRESS (uint64_t in hexa) and FILENAME
 *
 * @param filename the name of the memory content description file to read from
 * @param memory (modified) pointer to the begining of the memory
 * @param mem_capacity_in_bytes (modified) total size of the created memory
 * @return error code, *p_memory shall be NULL in case of error
 *
 */
 #define MAX_FILENAME_SIZE 128
 #define MAX_LINE_LENGTH MAX_FILENAME_SIZE+18

static int page_file_read( const phy_addr_t* phy_addr,
            						   const char* page_filename,
            						   void* const memory,
            						   const size_t mem_capacity_in_bytes);

/**
 * @brief Convert a virtual address (given as a uint_64_t) to a physical address, using page_walk
 * @param memory to lookup the mapping
 * @param vaddr64 the virtual address to convert
 * @param paddr (modified) the physical address after conversion
 * @return ERR_NONE if successful, appropriate error code otherwise
 */
static int virt_uint_64_to_phy_addr(void * const memory,
									const uint64_t vaddr64,
									phy_addr_t * const paddr);

int mem_init_from_description(const char* master_filename,
                              void** memory,
                              size_t* mem_capacity_in_bytes) {

	M_REQUIRE_NON_NULL(master_filename);
	M_REQUIRE_NON_NULL(memory);
	M_REQUIRE_NON_NULL(mem_capacity_in_bytes);

	FILE* f = fopen(master_filename, "r");
	M_REQUIRE_NON_NULL_CUSTOM_ERR(f, ERR_IO);

/*
 *  line1:           TOTAL MEMORY SIZE (size_t)
 *  line2:           PGD PAGE FILENAME
 *  line4:           NUMBER N OF TRANSLATION PAGES (PUD+PMD+PTE)
 *  lines5 to (5+N): LIST OF TRANSLATION PAGES, expressed with two info per line:
 *                       INDEX OFFSET (uint32_t in hexa) and FILENAME
 *  remaining lines: LIST OF DATA PAGES, expressed with two info per line:
 *                       VIRTUAL ADDRESS (uint64_t in hexa) and FILENAME
 */

	/*** ALLOCATE THE WHOLE PHYSICAL MEMORY SPACE (FOR ALL THE PAGES) ***/
	fscanf(f, "%zu", mem_capacity_in_bytes);
	*memory  = calloc(*mem_capacity_in_bytes, 1);

	if(*memory == NULL) { fclose(f); return ERR_MEM; }

	/*** WRITE THE TRANSLATION PAGES IN MEMORY ***/
	char filename[MAX_FILENAME_SIZE];
	phy_addr_t paddr;
	uint32_t paddr_32b = 0u;
	error_code err = ERR_NONE;

	// PGD
	if(fscanf(f, "%s", filename) != 1){
		fclose(f); return ERR_IO;
	}

	if((err = init_phy_addr(&paddr, 0, 0)) != ERR_NONE) { //PGD starts at phy addr. 0
    	fclose(f); return err;
  	}
	if(page_file_read(&paddr, filename, *memory, *mem_capacity_in_bytes) != ERR_NONE){
    	fclose(f); return ERR_IO;
  	}

  // OTHER TRANSLATION PAGES
	int nb_pages;
	if(fscanf(f, "%d", &nb_pages) != 1){
		fclose(f); return ERR_IO;
	}

	uint64_t vaddr64 = 0ul;
	for(int i = 0; i < nb_pages; i++) {
		if(fscanf(f, "%x %s", &paddr_32b, filename) != 2){
			fclose(f); return ERR_IO;
		}

		if((err = init_phy_addr(&paddr, paddr_32b, 0)) != ERR_NONE) {
        	fclose(f);
        	return err;
    	}
		if(page_file_read(&paddr, filename, *memory, *mem_capacity_in_bytes) != ERR_NONE) {
      		fclose(f);
     		return ERR_IO;
    	}
  	}

  /*** WRITE DATA PAGES ***/
	while(fscanf(f, "%lx %s", &vaddr64, filename) == 2 && !feof(f)) {
		//FIXME: This check fails for some reason,
    // but if we don't do it everything works fine (make check passes... find out why / how to do it properly ?)
    // paul - the problem is an EOF because the last line doesnt have a \n so scnaf return -1
    // anyway it was very weird to first fgets(newline) and then fscanf(f)

		if((err = virt_uint_64_to_phy_addr(*memory, vaddr64, &paddr)) != ERR_NONE) {
      		fclose(f);
      		return err;
    	}
		if(page_file_read(&paddr, filename, *memory, *mem_capacity_in_bytes) != ERR_NONE) {
      		fclose(f);
      		return ERR_IO;
    	}
	}
	fclose(f);
	return ERR_NONE;
}

/**
 * @brief Translates a virtual address, given as a 64-bit value, in the corresponding physical address
 */
static int virt_uint_64_to_phy_addr(void * const memory, const uint64_t vaddr64, phy_addr_t * paddr){
	virt_addr_t vaddr;

	if(init_virt_addr64(&vaddr, vaddr64) != ERR_NONE) { return ERR_ADDR; }
	if(page_walk(memory, &vaddr, paddr) != ERR_NONE) { return ERR_MEM; }

	return ERR_NONE;
}

/**
 * @brief Reads the data from a page file and puts it in memory at given address
 * @param phy_addr physical address in memory, where to write the page
 * @param page_filename the file name of the page to read the data from
 * @param memory the memory space to write the page in
 * @param mem_capacity_in_bytes the memory capacity, to ensure the is enough space
 * @return ERR_NONE if sucessful, appropriate error code otherwise
 */
static int page_file_read(
	const phy_addr_t* phy_addr,
	const char* page_filename,
	void* const memory,
	const size_t mem_capacity_in_bytes) {

	FILE* page_file = fopen(page_filename, "rb");
	M_REQUIRE_NON_NULL_CUSTOM_ERR(page_file, ERR_IO);

	size_t bytes_read;
	uint32_t phy_addr_32b = phy_addr_t_to_uint32_t(phy_addr);

	M_REQUIRE((phy_addr_32b & MAX_12BIT_VALUE) == 0, ERR_ADDR, "%s", "Address should be aligned with the beggining of the page");

	//Check that it fits in allocated memory from given address
	if(phy_addr_32b + PAGE_SIZE <= mem_capacity_in_bytes){
		//Read the page_file and write it in memory
		bytes_read = fread(&((byte_t*)memory)[phy_addr_32b], 1, PAGE_SIZE, page_file);
	}else{
		fclose(page_file);
		M_EXIT_ERR(ERR_MEM, "%s", "Not enough space to store the whole page file in memory from given physical address");
	}

	fclose(page_file);
	M_REQUIRE(bytes_read == PAGE_SIZE, ERR_IO, "%s", "Couldn't read the whole page file");

	return ERR_NONE;
}
