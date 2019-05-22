/**
 * @file cache_mng.c
 * @brief cache management functions
 *
 * @author Mirjana Stojilovic
 * @date 2018-19
 */

#include "error.h"
#include "util.h"
#include "cache_mng.h"
#include "addr_mng.h"
#include "lru.h"

#include <inttypes.h> // for PRIx macros
#include <string.h> // for memset

//=========================================================================
#define PRINT_CACHE_LINE(OUTFILE, TYPE, WAYS, LINE_INDEX, WAY, WORDS_PER_LINE) \
    do { \
            fprintf(OUTFILE, "V: %1" PRIx8 ", AGE: %1" PRIx8 ", TAG: 0x%03" PRIx16 ", values: ( ", \
                        cache_valid(TYPE, WAYS, LINE_INDEX, WAY), \
                        cache_age(TYPE, WAYS, LINE_INDEX, WAY), \
                        cache_tag(TYPE, WAYS, LINE_INDEX, WAY)); \
            for(int i_ = 0; i_ < WORDS_PER_LINE; i_++) \
                fprintf(OUTFILE, "0x%08" PRIx32 " ", \
                        cache_line(TYPE, WAYS, LINE_INDEX, WAY)[i_]); \
            fputs(")\n", OUTFILE); \
    } while(0)

#define PRINT_INVALID_CACHE_LINE(OUTFILE, TYPE, WAYS, LINE_INDEX, WAY, WORDS_PER_LINE) \
    do { \
            fprintf(OUTFILE, "V: %1" PRIx8 ", AGE: -, TAG: -----, values: ( ---------- ---------- ---------- ---------- )\n", \
                        cache_valid(TYPE, WAYS, LINE_INDEX, WAY)); \
    } while(0)

#define DUMP_CACHE_TYPE(OUTFILE, TYPE, WAYS, LINES, WORDS_PER_LINE)  \
    do { \
        for(uint16_t index = 0; index < LINES; index++) { \
            foreach_way(way, WAYS) { \
                fprintf(output, "%02" PRIx8 "/%04" PRIx16 ": ", way, index); \
                if(cache_valid(TYPE, WAYS, index, way)) \
                    PRINT_CACHE_LINE(OUTFILE, const TYPE, WAYS, index, way, WORDS_PER_LINE); \
                else \
                    PRINT_INVALID_CACHE_LINE(OUTFILE, const TYPE, WAYS, index, way, WORDS_PER_LINE);\
            } \
        } \
    } while(0)

//=========================================================================
// see cache_mng.h
int cache_dump(FILE* output, const void* cache, cache_t cache_type)
{
    M_REQUIRE_NON_NULL(output);
    M_REQUIRE_NON_NULL(cache);

    fputs("WAY/LINE: V: AGE: TAG: WORDS\n", output);
    switch (cache_type) {
    case L1_ICACHE:
        DUMP_CACHE_TYPE(output, l1_icache_entry_t, L1_ICACHE_WAYS,
                        L1_ICACHE_LINES, L1_ICACHE_WORDS_PER_LINE);
        break;
    case L1_DCACHE:
        DUMP_CACHE_TYPE(output, l1_dcache_entry_t, L1_DCACHE_WAYS,
                        L1_DCACHE_LINES, L1_DCACHE_WORDS_PER_LINE);
        break;
    case L2_CACHE:
        DUMP_CACHE_TYPE(output, l2_cache_entry_t, L2_CACHE_WAYS,
                        L2_CACHE_LINES, L2_CACHE_WORDS_PER_LINE);
        break;
    default:
        debug_print("%d: unknown cache type", cache_type);
        return ERR_BAD_PARAMETER;
    }
    putc('\n', output);

    return ERR_NONE;
}

static inline uint32_t tag_from_paddr_32b(uint32_t paddr_32b, uint8_t cache_tag_remaining_bits){
    return paddr_32b >> cache_tag_remaining_bits;
}
static inline uint16_t index_from_paddr_32b(uint32_t paddr_32b, uint16_t cache_line_bytes, uint16_t cache_lines){
    return (paddr_32b / cache_line_bytes) % cache_lines;
}

//=========================================================================
/**
 * @brief Clean a cache (invalidate, reset...).
 *
 * This function erases all cache data.
 * @param cache pointer to the cache
 * @param cache_type an enum to distinguish between different caches
 * @return error code
 */
#define FLUSH(cache_entry_type, CACHE_LINE, CACHE_WAYS)\
    (void)memset(cache, 0, CACHE_LINE * CACHE_WAYS * sizeof(cache_entry_type))

int cache_flush(void *cache, cache_t cache_type){
    M_REQUIRE_NON_NULL(cache);

    switch(cache_type){
        case L1_ICACHE:
            FLUSH(l1_icache_entry_t, L1_ICACHE_LINE, L1_ICACHE_WAYS);
            break;
        case L1_DCACHE:
            FLUSH(l1_dcache_entry_t, L1_DCACHE_LINE, L1_DCACHE_WAYS);
            break;
        case L2_CACHE:
            FLUSH(l2_cache_entry_t, L2_CACHE_LINE, L2_CACHE_WAYS);
            break;
        default:
            M_EXIT_ERR(ERR_BAD_PARAMETER, "%s", "Unrecognized cache type");
    }
    return ERR_NONE;
}

#undef FLUSH

//=========================================================================
/**
 * @brief Check if a instruction/data is present in one of the caches.
 *
 * On hit, update hit infos to corresponding index
 *         and update the cache-line-size chunk of data passed as the pointer to the function.
 * On miss, update hit infos to HIT_WAY_MISS or HIT_INDEX_MISS.
 *
 * @param mem_space starting address of the memory space
 * @param cache pointer to the beginning of the cache
 * @param paddr pointer to physical address
 * @param p_line pointer to a cache-line-size chunk of data to return
 * @param hit_way (modified) cache way where hit was detected, HIT_WAY_MISS on miss
 * @param hit_index (modified) cache line index where hit was detected, HIT_INDEX_MISS on miss
 * @param cache_type to distinguish between different caches
 * @return error code
 */

#define HIT(cache_entry_type, CACHE_LINE, CACHE_LINES, CACHE_WAYS, CACHE_TAG_REMAINING_BITS)\
    do{\
        uint32_t paddr_32b = phy_addr_t_to_uint32_t(paddr);\
        uint16_t line_index = index_from_paddr_32b(paddr_32b, CACHE_LINE, CACHE_LINES);\
        uint32_t tag = tag_from_paddr_32b(paddr_32b, CACHE_TAG_REMAINING_BITS);\
        \
        foreach_way(way, CACHE_WAYS){\
            cache_entry_type* entry = cache_entry(cache_entry_type, CACHE_WAYS, line_index, way);\
            if(entry->v == INVALID){\
                LRU_age_increase(cache_entry_type, CACHE_WAYS, way, line_index);\
                /*Rest of the cold start is handled like a miss outside of the foreach_way loop*/\
                break;\
            }else if(entry->tag == tag){\
                *hit_way = way;\
                *hit_index = line_index;\
                *p_line = cache_line(cache_entry_type, CACHE_WAYS, line_index, way);\
                LRU_age_update(cache_entry_type, CACHE_WAYS, way, line_index);\
                return ERR_NONE;\
            }\
        }\
        \
        /*Cold start or regular miss*/\
        *hit_way = HIT_WAY_MISS;\
        *hit_index = HIT_INDEX_MISS;\
        \
    } while(0)

int cache_hit (const void * mem_space,
               void * cache,
               phy_addr_t * paddr,
               const uint32_t ** p_line,
               uint8_t *hit_way,
               uint16_t *hit_index,
               cache_t cache_type){
    M_REQUIRE_NON_NULL(mem_space);
    M_REQUIRE_NON_NULL(cache);
    M_REQUIRE_NON_NULL(paddr);
    M_REQUIRE_NON_NULL(p_line);
    M_REQUIRE_NON_NULL(hit_way);
    M_REQUIRE_NON_NULL(hit_index);

    switch(cache_type){
        case L1_ICACHE:
            HIT(l1_icache_entry_t, L1_ICACHE_LINE, L1_ICACHE_LINES, L1_ICACHE_WAYS, L1_ICACHE_TAG_REMAINING_BITS);
            break;
        case L1_DCACHE:
            HIT(l1_dcache_entry_t, L1_DCACHE_LINE, L1_DCACHE_LINES, L1_DCACHE_WAYS, L1_DCACHE_TAG_REMAINING_BITS);
            break;
        case L2_CACHE:
            HIT(l2_cache_entry_t, L2_CACHE_LINE, L2_CACHE_LINES, L2_CACHE_WAYS, L2_CACHE_TAG_REMAINING_BITS);
            break;
        default:
            M_EXIT_ERR(ERR_BAD_PARAMETER, "%s", "Unrecognized cache type");
    }
    return ERR_NONE;
}

#undef HIT

//=========================================================================
/**
 * @brief Insert an entry to a cache.
 *
 * @param cache_line_index the number of the line to overwrite
 * @param cache_way the number of the way where to insert
 * @param cache_line_in pointer to the cache line to insert
 * @param cache pointer to the cache
 * @param cache_type to distinguish between different caches
 * @return error code
 */
#define INSERT(cache_entry_type, CACHE_LINES, CACHE_WAYS) \
    do{ \
        M_REQUIRE(cache_line_index < CACHE_LINES, "%s", "line doesn't exist in this cache"); \
        M_REQUIRE(cache_way < CACHE_WAYS, "%s", "way doesn't exist in this cache"); \
        \
        cache_entry_type* new_entry = (cache_entry_type*) cache_line_in;\
        *cache_entry(cache_entry_type, CACHE_WAYS, cache_line_index, cache_way) = *new_entry; \
    } while(0)

int cache_insert(uint16_t cache_line_index,
                 uint8_t cache_way,
                 const void * cache_line_in,
                 void * cache,
                 cache_t cache_type){
    M_REQUIRE_NON_NULL(cache_line_in);
    M_REQUIRE_NON_NULL(cache);

    switch(cache_type){
        case L1_ICACHE:
            INSERT(l1_icache_entry_t, L1_ICACHE_LINES, L1_ICACHE_WAYS);
            break;
        case L1_DCACHE:
            INSERT(l1_dcache_entry_t, L1_DCACHE_LINES, L1_DCACHE_WAYS);
            break;
        case L2_CACHE:
            INSERT(l2_cache_entry_t, L2_CACHE_LINES, L2_CACHE_WAYS);
            break;
        default:
            M_EXIT_ERR(ERR_BAD_PARAMETER, "s", "Unrecognized cache type");
    }

    return ERR_NONE;

}

#undef INSERT

//=========================================================================
/**
 * @brief Initialize a cache entry (write to the cache entry for the first time)
 *
 * @param mem_space starting address of the memory space
 * @param paddr pointer to physical address, to extract the tag
 * @param cache_entry pointer to the entry to be initialized
 * @param cache_type to distinguish between different caches
 * @return error code
 */
#define INIT(paddr, cache_entry_type, CACHE_TAG_REMAINING_BITS, CACHE_LINE, CACHE_WORDS_PER_LINE)\
    do{\
        uint32_t paddr_32b = phy_addr_t_to_uint32_t(paddr);\
        uint32_t tag = tag_from_paddr_32b(paddr_32b, CACHE_TAG_REMAINING_BITS);\
        \
        cache_entry_type* entry = (cache_entry_type*)cache_entry;\
        entry->v = VALID;\
        entry->age = 0;\
        entry->tag = tag;\
        \
        /*Initialize the cache entry line by fetching from memory*/\
        uint32_t word_addr = (paddr_32b / CACHE_LINE) * CACHE_WORDS_PER_LINE;\
        for(size_t i = 0; i < CACHE_WORDS_PER_LINE; i++){\
            entry->line[i] = ((word_t*)mem_space)[word_addr+i];\
        }\
    }while(0)

int cache_entry_init(const void * mem_space,
                     const phy_addr_t * paddr,
                     void * cache_entry,
                     cache_t cache_type){
    M_REQUIRE_NON_NULL(mem_space);
    M_REQUIRE_NON_NULL(paddr);
    M_REQUIRE_NON_NULL(cache_entry);

    switch(cache_type){
        case L1_ICACHE:
            INIT(paddr, l1_icache_entry_t, L1_ICACHE_TAG_REMAINING_BITS, L1_ICACHE_LINES, L1_ICACHE_WORDS_PER_LINE);
            break;
        case L1_DCACHE:
            INIT(paddr, l1_dcache_entry_t, L1_DCACHE_TAG_REMAINING_BITS, L1_DCACHE_LINES, L1_DCACHE_WORDS_PER_LINE);
            break;
        case L2_CACHE:
            INIT(paddr, l2_cache_entry_t, L2_CACHE_TAG_REMAINING_BITS, L2_CACHE_LINES, L2_CACHE_WORDS_PER_LINE);
            break;
        default :
            M_EXIT_ERR(ERR_BAD_PARAMETER, "%s", "Unrecognized cache type");
    }
    return ERR_NONE;
}

#undef INIT

//=========================================================================
/**
 * @brief Ask cache for a word of data.
 *  Exclusive policy (https://en.wikipedia.org/wiki/Cache_inclusion_policy)
 *      Consider the case when L2 is exclusive of L1. Suppose there is a
 *      processor read request for block X. If the block is found in L1 cache,
 *      then the data is read from L1 cache and returned to the processor. If
 *      the block is not found in the L1 cache, but present in the L2 cache,
 *      then the cache block is moved from the L2 cache to the L1 cache. If
 *      this causes a block to be evicted from L1, the evicted block is then
 *      placed into L2. This is the only way L2 gets populated. Here, L2
 *      behaves like a victim cache. If the block is not found neither in L1 nor
 *      in L2, then it is fetched from main memory and placed just in L1 and not
 *      in L2.
 *
 * @param mem_space pointer to the memory space
 * @param paddr pointer to a physical address
 * @param access to distinguish between fetching instructions and reading/writing data
 * @param l1_cache pointer to the beginning of L1 CACHE
 * @param l2_cache pointer to the beginning of L2 CACHE
 * @param word pointer to the word of data that is returned by cache
 * @param replace replacement policy
 * @return error code
 */
int cache_read(const void * mem_space,
               phy_addr_t * paddr,
               mem_access_t access,
               void * l1_cache,
               void * l2_cache,
               uint32_t * word,
               cache_replace_t replace);

//=========================================================================
/**
 * @brief Ask cache for a byte of data. Endianess: LITTLE.
 *
 * @param mem_space pointer to the memory space
 * @param p_addr pointer to a physical address
 * @param access to distinguish between fetching instructions and reading/writing data
 * @param l1_cache pointer to the beginning of L1 CACHE
 * @param l2_cache pointer to the beginning of L2 CACHE
 * @param byte pointer to the byte to be returned
 * @param replace replacement policy
 * @return error code
 */
int cache_read_byte(const void * mem_space,
                    phy_addr_t * p_paddr,
                    mem_access_t access,
                    void * l1_cache,
                    void * l2_cache,
                    uint8_t * p_byte,
                    cache_replace_t replace);

//=========================================================================
/**
 * @brief Change a word of data in the cache.
 *  Exclusive policy (see cache_read)
 *
 * @param mem_space pointer to the memory space
 * @param paddr pointer to a physical address
 * @param l1_cache pointer to the beginning of L1 CACHE
 * @param l2_cache pointer to the beginning of L2 CACHE
 * @param word const pointer to the word of data that is to be written to the cache
 * @param replace replacement policy
 * @return error code
 */
int cache_write(void * mem_space,
                phy_addr_t * paddr,
                void * l1_cache,
                void * l2_cache,
                const uint32_t * word,
                cache_replace_t replace);

//=========================================================================
/**
 * @brief Write to cache a byte of data. Endianess: LITTLE.
 *
 * @param mem_space pointer to the memory space
 * @param paddr pointer to a physical address
 * @param l1_cache pointer to the beginning of L1 ICACHE
 * @param l2_cache pointer to the beginning of L2 CACHE
 * @param p_byte pointer to the byte to be returned
 * @param replace replacement policy
 * @return error code
 */
int cache_write_byte(void * mem_space,
                     phy_addr_t * paddr,
                     void * l1_cache,
                     void * l2_cache,
                     uint8_t p_byte,
                     cache_replace_t replace);
