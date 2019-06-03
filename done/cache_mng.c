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
#include "cache.h"


#include <inttypes.h> // for PRIx macros
#include <string.h> // for memset
#include <stdlib.h> // for *alloc

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
#define FLUSH(cache_entry_type, CACHE_LINES, CACHE_WAYS)\
    (void)memset(cache, 0, CACHE_LINES * CACHE_WAYS * sizeof(cache_entry_type))

int cache_flush(void *cache, cache_t cache_type){
    M_REQUIRE_NON_NULL(cache);

    switch(cache_type){
        case L1_ICACHE:
            FLUSH(l1_icache_entry_t, L1_ICACHE_LINES, L1_ICACHE_WAYS);
            break;
        case L1_DCACHE:
            FLUSH(l1_dcache_entry_t, L1_DCACHE_LINES, L1_DCACHE_WAYS);
            break;
        case L2_CACHE:
            FLUSH(l2_cache_entry_t, L2_CACHE_LINES, L2_CACHE_WAYS);
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
                /* TODO do we do "et l'on mémorisera se fait pour la suite" */\
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
        M_REQUIRE(cache_line_index < CACHE_LINES, ERR_BAD_PARAMETER, "%s", "line doesn't exist in this cache"); \
        M_REQUIRE(cache_way < CACHE_WAYS, ERR_BAD_PARAMETER, "%s", "way doesn't exist in this cache"); \
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
            M_EXIT_ERR(ERR_BAD_PARAMETER, "%s", "Unrecognized cache type");
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
            INIT(paddr, l1_icache_entry_t, L1_ICACHE_TAG_REMAINING_BITS, L1_ICACHE_LINE, L1_ICACHE_WORDS_PER_LINE);
            break;
        case L1_DCACHE:
            INIT(paddr, l1_dcache_entry_t, L1_DCACHE_TAG_REMAINING_BITS, L1_DCACHE_LINE, L1_DCACHE_WORDS_PER_LINE);
            break;
        case L2_CACHE:
            INIT(paddr, l2_cache_entry_t, L2_CACHE_TAG_REMAINING_BITS, L2_CACHE_LINE, L2_CACHE_WORDS_PER_LINE);
            break;
        default :
            M_EXIT_ERR(ERR_BAD_PARAMETER, "%s", "Unrecognized cache type");
    }
    return ERR_NONE;
}

#undef INIT


#define FIND_OLDEST(TYPE, WAYS) \
    do { \
      foreach_way(way, WAYS) { \
        if(!cache_valid(TYPE, WAYS, line_index, way)){ *empty = 1;  return way;}\
        int tmp = cache_age(TYPE, WAYS, line_index, way); \
        if( tmp > max) max = tmp; arg_max = way; \
      } \
      return arg_max; \
    } while(0)

/*@brief find the oldest way or an empty way
  @empty set it to 1 is an empty way was found
  @return the oldest or empty way's index
 */
static int find_oldest_way( void const * const cache,
                            cache_t cache_type,
                            const int line_index,
                            int* const empty){
    int max = -1;
    int arg_max = -1;
    switch(cache_type) {
      case L1_ICACHE:
        FIND_OLDEST(l1_icache_entry_t, L1_ICACHE_WAYS);
        break;
      case L1_DCACHE:
        FIND_OLDEST(l1_dcache_entry_t, L1_DCACHE_WAYS);
        break;
      case L2_CACHE:
        FIND_OLDEST(l2_cache_entry_t, L2_CACHE_WAYS);
        break;
      default: M_EXIT(ERR_BAD_PARAMETER, "%s", "unknown cache type");
    }
}

#undef FIND_OLDEST

#define CACHE_LINE_INDEX(LINE_BITS) \
  do { \
    int line_bits_mask = (1 << LINE_BITS) - 1; \
    return (paddr >> 4) & line_bits_mask; \
  } while(0)

/*@brief get line index in cache from physical address */
static int line_index_from_paddr32( uint32_t paddr, cache_t cache_type) {
  switch(cache_type) {
    // TODO we should test this;
    case L1_ICACHE:
      CACHE_LINE_INDEX(L1_ICACHE_LINE_BITS);
      break;
    case L1_DCACHE:
      CACHE_LINE_INDEX(L1_DCACHE_LINE_BITS);
      break;
    case L2_CACHE:
      CACHE_LINE_INDEX(L2_CACHE_LINE_BITS);
      break;
    default: M_EXIT(ERR_BAD_PARAMETER, "%s", "unknown cache type");
  }
}
#undef CACHE_LINE_INDEX

// TODO change for loop to memcpy
#define CONVERT(FROM_TYPE, FROM_WORDS_PER_LINE, TO_TYPE, TO_TAG_REMAINING_BITS) \
    do { \
    TO_TYPE* to_entry = malloc(sizeof(TO_TYPE)); \
    if(to_entry == NULL){ return NULL; } \
    uint32_t tag = tag_from_paddr_32b(paddr_32b, TO_TAG_REMAINING_BITS); \
    to_entry->v = VALID; \
    to_entry->age = 0; \
    to_entry->tag = tag; \
    for(int i = 0; i < FROM_WORDS_PER_LINE; i++) { \
      to_entry->line[i] = ((FROM_TYPE*) from)->line[i]; \
    } \
    return to_entry; \
  } while(0)

/*TODO: does not hold in general?
simplified because both level of caches have the same line length,
otherwise would need to find the correct cut for the lines */
/* TODO duplication with init? this is basically copy paste
but it is not very convinient to change the macro above as it uses 'local' variables */

/* @brief convert from a cache entry type to another from its physical address
   @param from the cache entry to convert
   @return the converted and newly allocated cache entry
*/
static void* convert(void* from, cache_t from_type, cache_t to_type, const uint32_t paddr_32b) {
  switch(from_type){
    case L1_ICACHE:
      CONVERT(l1_icache_entry_t, L1_ICACHE_WORDS_PER_LINE,
              l2_cache_entry_t, L2_CACHE_TAG_REMAINING_BITS);
      break;
    case L1_DCACHE:
      CONVERT(l1_dcache_entry_t, L1_DCACHE_WORDS_PER_LINE,
              l2_cache_entry_t, L2_CACHE_TAG_REMAINING_BITS);
      break;
    case L2_CACHE:
      if(to_type == L1_ICACHE) {
        CONVERT(l2_cache_entry_t, L2_CACHE_WORDS_PER_LINE,
                l1_icache_entry_t, L1_ICACHE_TAG_REMAINING_BITS);
      }
      else {
        CONVERT(l2_cache_entry_t, L2_CACHE_WORDS_PER_LINE,
                l1_dcache_entry_t, L1_DCACHE_TAG_REMAINING_BITS);
      }
      break;
    default: return NULL;
  }
}
#undef CONVERT

#define EVICT(TYPE, WAYS) \
  do { \
  TYPE* evicted = cache_entry(TYPE, WAYS, line_index, way); \
  evicted->v = INVALID; \
  return evicted; \
  } while(0)

/*@brief evict cache entry at specified line
  @return the evicted cache entry
*/
static void* evict(void const * const cache,
                   cache_t type,
                   const int line_index,
                   const int way) {

  switch(type){
      case L1_ICACHE:
          EVICT(l1_icache_entry_t, L1_ICACHE_WAYS);
          break;
      case L1_DCACHE:
          EVICT(l1_dcache_entry_t, L1_DCACHE_WAYS);
          break;
      case L2_CACHE:
          EVICT(l2_cache_entry_t, L2_CACHE_WAYS);
          break;
      default :
          return NULL;
  }
}

#undef EVICT

/*@brief update the replacement policy at specified line in given cache
*/
static int update_eviction_policy(void * const cache, cache_t cache_type,
                                  const int line_index, const int way,
                                  cache_replace_t replace) {
  switch(replace){
    case LRU:
      switch(cache_type){
        case L1_ICACHE:
          LRU_age_increase(l1_icache_entry_t, L1_ICACHE_WAYS, way, line_index);
        break;
        case L1_DCACHE:
          LRU_age_increase(l1_dcache_entry_t, L1_DCACHE_WAYS, way, line_index);
        break;
        case L2_CACHE:
          LRU_age_increase(l2_cache_entry_t, L2_CACHE_WAYS, way, line_index);
        break;
        default: M_EXIT(ERR_BAD_PARAMETER, "%s", "unknown cache type");
      }
    break;
    default: M_EXIT(ERR_BAD_PARAMETER, "%s", "unknown replacement policy");
    }
  return ERR_NONE;
}

// TODO remove ints for uint stuff
# define CACHE_COMMUNICATION(L1_TYPE, L1_CACHE) \
  do{ \
    L1_TYPE* l1_entry = convert(l2_entry, L2_CACHE, L1_CACHE, paddr_32b); \
    M_REQUIRE_NON_NULL(l1_entry); \
    /*is there space in L1? */ \
    int line_index = line_index_from_paddr32(paddr_32b, L1_CACHE); \
    int empty = 0; \
    int way = find_oldest_way(l1_cache, L1_CACHE, line_index, &empty); \
    \
    if(empty) { \
      cache_insert(line_index, way, l1_entry, l1_cache, L1_CACHE); \
      update_eviction_policy(l1_cache, L1_CACHE, line_index, way, replace); \
    } \
    else { \
      /* evict (but save) with replacement policy */ \
      L1_TYPE* evicted = evict(l1_cache, L1_CACHE, line_index, way); \
      /* insert at this index */ \
      cache_insert(line_index, way, l1_entry, l1_cache, L1_CACHE); \
      /* update age */ \
      update_eviction_policy(l1_cache, L1_CACHE, line_index, way, replace); \
      \
      /* ========================================== update L2 after eviction*/ \
      l2_cache_entry_t* l2_evicted_entry = convert(evicted, L1_CACHE, L2_CACHE, paddr_32b); \
      \
      M_REQUIRE_NON_NULL(l2_evicted_entry); \
      /* L2 space? */ \
      line_index = line_index_from_paddr32(paddr_32b, L2_CACHE); \
      empty = 0; \
      way = find_oldest_way(l2_cache, L2_CACHE, line_index, &empty);\
      \
      if(empty){ \
        cache_insert(line_index, way, l2_evicted_entry, l2_cache, L2_CACHE); \
        update_eviction_policy(l2_cache, L2_CACHE, line_index, way, replace); \
      } \
      else { \
        /* evict with replacement policy */ \
        evict(l2_cache, L2_CACHE, line_index, way); \
        /* insert at free space */ \
        cache_insert(line_index, way, l2_evicted_entry, l2_cache, L2_CACHE); \
        /* update L2 age */ \
        update_eviction_policy(l2_cache, L2_CACHE, line_index, way, replace); \
      } \
    } \
  } while(0)

/*@brief handle communication from L2 cache to L1 cache and eviction policies!
  @param l2_entry this entry will be converted to an l1_entry
  @param paddr_32b the requested address
  @return err code
*/
static int cache_communication(void* l1_cache, cache_t l1_cache_type,
                               l2_cache_entry_t* l2_cache, l2_cache_entry_t* l2_entry,
                               uint32_t paddr_32b,
                               cache_replace_t replace) {
    switch(l1_cache_type) {
      case L1_ICACHE:
        CACHE_COMMUNICATION(l1_icache_entry_t, L1_ICACHE);
      break;
      case L1_DCACHE:
        CACHE_COMMUNICATION(l1_icache_entry_t, L1_DCACHE);
      break;
      default: M_EXIT(ERR_BAD_PARAMETER, "%s", "unknown cache type");
    }
    return ERR_NONE;
}
#undef CACHE_COMMUNICATION

// TODO check for null returns everywhere it can happen!

# define FIND(cache, CACHE_TYPE) \
    do { \
      err = cache_hit(mem_space, cache, paddr, &line, &hit_way, &hit_line, CACHE_TYPE); \
      M_EXIT_IF_ERR(err, "error in chache hit"); \
    } while(0)

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
               cache_replace_t replace) {
/* vérification d'usage (addresse aligned) */
  M_REQUIRE_NON_NULL(mem_space);
  M_REQUIRE_NON_NULL(paddr);
  M_REQUIRE( (paddr->page_offset & BYTE_SEL_MASK) == 0, ERR_BAD_PARAMETER, "%s", "Address should be word aligned");
  M_REQUIRE_NON_NULL(l1_cache);
  M_REQUIRE_NON_NULL(l2_cache);
  M_REQUIRE_NON_NULL(word);

  uint8_t word_index = (paddr->page_offset >> BYTE_SEL_BITS) & WORD_SEL_MASK;
  int err = ERR_NONE;
/* ================================================================== L1-hit? */
  const uint32_t* line;
  uint8_t hit_way;
  uint16_t hit_line;

  switch(access){
    case INSTRUCTION:
      FIND(l1_cache, L1_ICACHE);
      break;
    case DATA:
      FIND(l1_cache, L1_DCACHE);
      break;
    default: M_EXIT(ERR_BAD_PARAMETER, "%s", "access type is ill defined");
  }

  if(hit_way != HIT_WAY_MISS && hit_line != HIT_INDEX_MISS) {
    // get word and return
    *word = line[word_index];
    return err;
  }

/* ================================================================== L2-hit? */
  // TODO we dont really need the cache entry here, the line would be enough
  // Léo – Is the malloc really needed ?
  l2_cache_entry_t* l2_entry = malloc(sizeof(l2_cache_entry_t));
  M_REQUIRE_NON_NULL(l2_entry);
  FIND(l2_cache, L2_CACHE); // TODO this sets 'line', use it instead of l2_entry
  // L2 HIT
  if(hit_way != HIT_WAY_MISS && hit_line != HIT_INDEX_MISS) {
    //get line
    void* cache = l2_cache; // TODO beurk but it dies in 2 lines
    l2_entry = cache_entry(l2_cache_entry_t, L2_CACHE_WAYS, hit_line, hit_way);
    //invalidate L2 cache entry
    cache_valid(l2_cache_entry_t, L2_CACHE_WAYS, hit_line, hit_way) = INVALID;
    *word = l2_entry->line[word_index];
  }
  // L2 MISS
  else {
    //fetch in main mem
    err =  cache_entry_init(mem_space, paddr, l2_entry, L2_CACHE);
    M_REQUIRE( err == ERR_NONE, err, "%s", "Failed to initiate a L2_CACHE entry");
    // NOTE: using an L2 entry induces an unnecessary conversion to l1_*cache_entry_t
    // but allows to write a single macro to update L1
    // thoughts : use another macro to convert from L2 entry to L1
    // and write the UPDATEL1 macro with an adapted cache entry type?
    *word = l2_entry->line[word_index];
  }

  /* ========================================================= eviction policy*/
  /*
-CURRENT:
  if L1 has free space
    insert mainmem L1
  else
    evict L1
    insert mainmem L1
    if l2 has free space
      insert evicted1 L2
    else
      evict L2
      insert evicted1 L2

-LESS CODE DUPLICATION: objective
  if !L1 has free space
      evict L1
      if !l2 has free space
          evict L2
          insert evicted1 L2
      insert evicted1 L2
  insert mainmem L1
  */
  uint32_t paddr_32b = phy_addr_t_to_uint32_t(paddr);
  switch(access){
    case INSTRUCTION:
    err = cache_communication(l1_cache, L1_ICACHE, l2_cache, l2_entry, paddr_32b, replace);
    M_EXIT_IF_ERR(err, "cache communication failed\n");
    break;
    case DATA:
    err = cache_communication(l1_cache, L1_DCACHE, l2_cache, l2_entry, paddr_32b, replace);
    break;
    default: M_EXIT(ERR_BAD_PARAMETER, "%s", "access type is ill defined");
  }

  return err;
}

/*
GUIDELINES
https://www.youtube.com/watch?v=eY52Zsg-KVI
vérification d'usage (addresse aligned)
    L1 hit? get word and return
  - else
    L2 hit?
      L2 hit: update corresponding L1
              invalidate L2 cache entry
              get word and return
      L2 miss: fetch in main mem
               update corresponding L1
               get word and return

L2 -> L1 :
  convert L2 entry to L1 entry
  compute L1 tag
  invalidate L2 entry
  is there space in L1?
    yes: insert at free space
         update L1 age (cf cache hit)
    no: evict (but save) with replacement policy and get index
        insert at this index
        update age
        L2 space? goto * for L2 and the evicted data
*/
#undef FIND

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
                    cache_replace_t replace){

    M_REQUIRE_NON_NULL(mem_space);
    M_REQUIRE_NON_NULL(p_paddr);
    M_REQUIRE_NON_NULL(l1_cache);
    M_REQUIRE_NON_NULL(l2_cache);
    M_REQUIRE_NON_NULL(p_byte);

    //Word-align the address
    phy_addr_t word_aligned = *p_paddr;
    word_aligned.page_offset = word_aligned.page_offset & ~BYTE_SEL_MASK;
    uint8_t byte_sel = p_paddr->page_offset & BYTE_SEL_MASK;

    uint32_t word;
    M_EXIT_IF_ERR(cache_read(mem_space, &word_aligned, access, l1_cache, l2_cache, &word, replace), "Error trying to read the cache");

    //Get the required byte
    *p_byte = ((uint8_t*)&word)[byte_sel];

    return ERR_NONE;
}

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

#define WRITE_LINE_IN_MEM(CACHE_LINE) \
    memcpy(&(((word_t*)mem_space)[line_addr]), p_line, CACHE_LINE)

#define MODIFY_AND_REINSERT(l_cache, cache_entry_type, CACHE_WAYS, CACHE_LINE) \
do{\
    void* cache = l_cache;\
    cache_entry_type* entry = cache_entry(cache_entry_type, CACHE_WAYS, hit_index, hit_way);\
    \
    /*Rewrite modified line in cache*/\
    memcpy(entry->line, p_line, CACHE_LINE);\
    \
    LRU_age_update(cache_entry_type, CACHE_WAYS, hit_way, hit_index);\
} while(0)

int cache_write(void * mem_space,
                phy_addr_t * paddr,
                void * l1_cache,
                void * l2_cache,
                const uint32_t * word,
                cache_replace_t replace){
    M_REQUIRE_NON_NULL(mem_space);
    M_REQUIRE_NON_NULL(paddr);
    M_REQUIRE_NON_NULL(l1_cache);
    M_REQUIRE_NON_NULL(l2_cache);
    M_REQUIRE_NON_NULL(word);

    M_REQUIRE((paddr->page_offset & BYTE_SEL_MASK) == 0, ERR_BAD_PARAMETER, "%s", "Address should be word aligned");

    uint8_t word_index = (paddr->page_offset >> BYTE_SEL_BITS) & WORD_SEL_MASK;

    uint32_t paddr_32b = phy_addr_t_to_uint32_t(paddr);\
    uint32_t line_addr = paddr_32b & ~((WORD_SEL_MASK << BYTE_SEL_BITS) | BYTE_SEL_MASK); \

    uint32_t* p_line;
    uint8_t hit_way;
    uint16_t hit_index;
    M_EXIT_IF_ERR(cache_hit(mem_space, l1_cache, paddr, &p_line, &hit_way, &hit_index, L1_DCACHE);, "Error calling cache_hit on l1 data cache");
    if(hit_way != HIT_WAY_MISS && hit_index != HIT_INDEX_MISS){

        //Modify one word of the read line and reinsert it in l1 data cache
        p_line[word_index] = *word;
        MODIFY_AND_REINSERT(l1_cache, l1_dcache_entry_t, L1_DCACHE_WAYS, L1_DCACHE_LINE);

        //Write the whole line in memory (write through cache)
        WRITE_LINE_IN_MEM(L1_DCACHE_LINE);
    }else{
        M_EXIT_IF_ERR(cache_hit(mem_space, l2_cache, paddr, &p_line, &hit_way, &hit_index, L2_CACHE), "Error calling cache_hit on l2 cache");
        if(hit_way != HIT_WAY_MISS){
            p_line[word_index] = *word;\
            MODIFY_AND_REINSERT(l2_cache, l2_cache_entry_t, L2_CACHE_WAYS, L2_CACHE_LINE);

            //Bring the information from the l2 cache to the l1 cache
            void* cache = l2_cache;
            l2_cache_entry_t* l2_entry = cache_entry(l2_cache_entry_t, L2_CACHE_WAYS, hit_index, hit_way);
            cache_communication(l1_cache, L1_DCACHE, l2_cache, l2_entry, paddr_32b, replace);

            WRITE_LINE_IN_MEM(L2_CACHE_LINE);
        }else{
            //Read (whole) line from memory
            memcpy(p_line, &(((word_t*)mem_space)[line_addr]), L2_CACHE_LINE);

            //Modify word and write back the whole line to main mem.
            p_line[word_index] = *word;
            WRITE_LINE_IN_MEM(L2_CACHE_LINE);

            l1_dcache_entry_t entry;
            M_EXIT_IF_ERR(cache_entry_init(mem_space, paddr, &entry, L1_DCACHE), "Error trying to initialize a new l1 data cache entry");
            M_EXIT_IF_ERR(cache_insert(hit_index, hit_way, &entry, l1_cache, L1_DCACHE), "Error trying to insert into the l1 data cache");
        }
    }
    return ERR_NONE;
}

#undef WRITE_LINE_IN_MEM
#undef MODIFY_AND_REINSERT

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
                     cache_replace_t replace){
    M_REQUIRE_NON_NULL(mem_space);
    M_REQUIRE_NON_NULL(paddr);
    M_REQUIRE_NON_NULL(l1_cache);
    M_REQUIRE_NON_NULL(l2_cache);

    //Word align the address and get index of the desired byte
    phy_addr_t word_aligned = *paddr;
    word_aligned.page_offset = word_aligned.page_offset & (~BYTE_SEL_MASK);
    uint8_t byte_sel = paddr->page_offset & ~BYTE_SEL_MASK;

    //Read the whole word (in which the byte is)
    //TODO : possibly wrong to use cache_read, since it is not said so in the instructions...
    //Maybe we should directly read the memory. See forum, somebody asked and I up'd the question
    uint32_t word;
    cache_read(mem_space, &word_aligned, DATA, l1_cache, l2_cache, &word, replace);

    //Modify the byte
    ((uint8_t*)&word)[byte_sel] = p_byte;

    //Write it back
    cache_write(mem_space, &word_aligned, l1_cache, l2_cache, &word, replace);

    return ERR_NONE;
}
