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
        memcpy(entry->line, &(((word_t*)mem_space)[word_addr]), CACHE_LINE);\
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
        if(!cache_valid(TYPE, WAYS, line_index, way)){ \
          *empty = 1; \
          return way; \
        }\
        int tmp = cache_age(TYPE, WAYS, line_index, way); \
        if(tmp > max) { \
          max = tmp; \
          arg_max = way; \
         } \
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
      default:
        M_EXIT(ERR_BAD_PARAMETER, "%s", "unknown cache type");
    }
}

#undef FIND_OLDEST

#define RECOVER(TYPE, TAG_REMAINING_BITS, CACHE_LINE) \
  do { \
    uint32_t addr = ((TYPE*)from_entry)->tag << TAG_REMAINING_BITS; \
    addr |= line_index * CACHE_LINE; \
    return addr; \
  } while(0);

/* @brief recover the address of the beginning of the line from a cache entry
*/
static uint32_t recover_addr(void* from_entry, cache_t from_type, uint16_t line_index) {
  switch(from_type){
    case L1_ICACHE:
      RECOVER(l1_icache_entry_t, L1_ICACHE_TAG_REMAINING_BITS, L1_ICACHE_LINE);
    case L1_DCACHE:
      RECOVER(l1_dcache_entry_t, L1_DCACHE_TAG_REMAINING_BITS, L1_DCACHE_LINE);
    case L2_CACHE:
      RECOVER(l2_cache_entry_t, L2_CACHE_TAG_REMAINING_BITS, L2_CACHE_LINE);
    default:
      M_EXIT_ERR(ERR_BAD_PARAMETER, "%s", "unknown cache type");
  }
}

#undef RECOVER

// NOTE: This only works assuming that all caches have the same line sizes!
#define CONVERT(FROM_TYPE, TO_TYPE, TO_TAG_REMAINING_BITS, CACHE_LINE) \
    do { \
      uint32_t addr = recover_addr(from_entry, from_cache, line_index); \
      TO_TYPE* entry =  malloc(sizeof(TO_TYPE)); \
      if(entry == NULL) { return NULL; }\
      entry->v = VALID; \
      entry->age = 0; \
      entry->tag = tag_from_paddr_32b(addr, TO_TAG_REMAINING_BITS); \
      memcpy(entry->line, ((FROM_TYPE*)from_entry)->line, CACHE_LINE); \
      return entry; \
    } while(0)

/* @brief converts the cache entry 'from' from a 'from_cache' entry to a 'to_cache' entry
*/
static void* convert(void* from_entry, cache_t from_cache, cache_t to_cache, uint16_t line_index){
  switch(to_cache){
    case L1_ICACHE:
      CONVERT(l2_cache_entry_t, l1_icache_entry_t, L1_ICACHE_TAG_REMAINING_BITS, L1_ICACHE_LINE);
    case L1_DCACHE:
      CONVERT(l2_cache_entry_t, l1_dcache_entry_t, L1_DCACHE_TAG_REMAINING_BITS, L1_DCACHE_LINE);
    case L2_CACHE:
      if(from_cache == L1_ICACHE) {
        CONVERT(l1_icache_entry_t, l2_cache_entry_t, L2_CACHE_TAG_REMAINING_BITS, L2_CACHE_LINE);
      }
      else if(from_cache == L1_DCACHE){
        CONVERT(l1_dcache_entry_t, l2_cache_entry_t, L2_CACHE_TAG_REMAINING_BITS, L2_CACHE_LINE);
      }
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

/*@brief Updates the ages according to eviction policy at specified line in given cache*/
static int update_eviction_policy(void * const cache, cache_t cache_type,
                                  const int line_index, const int way,
                                  cache_replace_t replace) {
  switch(replace){
    case LRU:
      switch(cache_type){
        case L1_ICACHE:
          LRU_age_update(l1_icache_entry_t, L1_ICACHE_WAYS, way, line_index);
        break;
        case L1_DCACHE:
          LRU_age_update(l1_dcache_entry_t, L1_DCACHE_WAYS, way, line_index);
        break;
        case L2_CACHE:
          LRU_age_update(l2_cache_entry_t, L2_CACHE_WAYS, way, line_index);
        break;
        default: M_EXIT(ERR_BAD_PARAMETER, "%s", "unknown cache type");
      }
    break;
    default: M_EXIT(ERR_BAD_PARAMETER, "%s", "unknown replacement policy");
    }
  return ERR_NONE;
}

// FIXME: mallocs require free... but it messes things up here

#define L1_INSERT(L1_TYPE, L1_CACHE) \
  do{ \
    /*Is there space in L1? */ \
    uint16_t line_index = index_from_paddr_32b(paddr_32b, L1_ICACHE_LINE, L1_ICACHE_LINES); \
    int empty = 0; \
    uint8_t way = find_oldest_way(l1_cache, L1_CACHE, line_index, &empty); \
    \
    if(empty) { \
      cache_insert(line_index, way, l1_entry, l1_cache, L1_CACHE); \
      update_eviction_policy(l1_cache, L1_CACHE, line_index, way, replace); \
    } \
    else { \
      /* evict (but save) with replacement policy */ \
      L1_TYPE* evicted = evict(l1_cache, L1_CACHE, line_index, way); \
      uint32_t evicted_addr = recover_addr(evicted, L1_CACHE, line_index); \
      l2_cache_entry_t* l2_evicted_entry =  malloc(sizeof(l2_cache_entry_t)); \
      M_EXIT_IF_NULL(l2_evicted_entry, sizeof(l2_cache_entry_t)); \
      l2_evicted_entry->v = VALID; \
      l2_evicted_entry->age = 0; \
      l2_evicted_entry->tag = tag_from_paddr_32b(evicted_addr, L2_CACHE_TAG_REMAINING_BITS); \
      memcpy(l2_evicted_entry->line, evicted->line, L2_CACHE_LINE); \
      /* free(evicted); ... */ \
      \
      /* insert at this index */ \
      cache_insert(line_index, way, l1_entry, l1_cache, L1_CACHE); \
      /* update age */ \
      update_eviction_policy(l1_cache, L1_CACHE, line_index, way, replace); \
      \
      /* ========================================== update L2 after eviction*/ \
      /* L2 space? */ \
      line_index = index_from_paddr_32b(evicted_addr, L2_CACHE_LINE, L2_CACHE_LINES); \
      empty = 0; \
      way = find_oldest_way(l2_cache, L2_CACHE, line_index, &empty);\
      \
      if(empty){ \
        cache_insert(line_index, way, l2_evicted_entry, l2_cache, L2_CACHE); \
        update_eviction_policy(l2_cache, L2_CACHE, line_index, way, replace); \
      } \
      else { \
        /* evict with replacement policy */ \
        void* tmp = evict(l2_cache, L2_CACHE, line_index, way); \
        /* free(tmp); .... */\
        /* insert at free space */ \
        cache_insert(line_index, way, l2_evicted_entry, l2_cache, L2_CACHE); \
        /* update L2 age */ \
        update_eviction_policy(l2_cache, L2_CACHE, line_index, way, replace); \
      } \
    } \
  } while(0)

/*@brief Inserts l1_entry into l1_cache, handling possible eviction and subsequent write to l2 cache*/
static int l1_insert(void* l1_cache, void* l1_entry, cache_t l1_cache_type,
                     l2_cache_entry_t* l2_cache,
                     uint32_t paddr_32b,
                     cache_replace_t replace) {
    switch(l1_cache_type) {
      case L1_ICACHE:
        L1_INSERT(l1_icache_entry_t, L1_ICACHE);
      break;
      case L1_DCACHE:
        L1_INSERT(l1_dcache_entry_t, L1_DCACHE);
      break;
      default: M_EXIT(ERR_BAD_PARAMETER, "%s", "unknown cache type");
    }
    return ERR_NONE;
}

#undef L1_INSERT

static int l2_to_l1(void* l1_cache, cache_t l1_cache_type,
                    l2_cache_entry_t* l2_cache, l2_cache_entry_t* l2_entry,
                    uint32_t paddr_32b,
                    cache_replace_t replace){
  void* l1_entry;
  uint16_t line_index = index_from_paddr_32b(paddr_32b, L2_CACHE_LINE, L2_CACHE_LINES);
  switch(l1_cache_type){
    case L1_ICACHE:
      l1_entry = convert(l2_entry, L2_CACHE, L1_ICACHE, line_index);
      M_EXIT_IF_ERR(l1_insert(l1_cache, l1_entry, L1_ICACHE, l2_cache, paddr_32b, replace), "Error inserting into l1 instruction cache");
      break;
    case L1_DCACHE:
      l1_entry = convert(l2_entry, L2_CACHE, L1_DCACHE, line_index);
      M_EXIT_IF_ERR(l1_insert(l1_cache, l1_entry, L1_DCACHE, l2_cache, paddr_32b, replace), "Error inserting into l1 data cache");
      break;
    default:
      M_EXIT(ERR_BAD_PARAMETER, "%s", "access type is ill defined");
    }


  //Invalidate l2 entry and free it
  l2_entry->v = INVALID;
  //free(l2_entry);
  return ERR_NONE;
}

#define FIND(cache, CACHE_TYPE) \
  M_EXIT_IF_ERR(cache_hit(mem_space, cache, paddr, &p_line, &hit_way, &hit_index, CACHE_TYPE), "Error calling cache hit");

int cache_read(const void * mem_space,
               phy_addr_t * paddr,
               mem_access_t access,
               void * l1_cache,
               void * l2_cache,
               uint32_t * word,
               cache_replace_t replace) {
  M_REQUIRE_NON_NULL(mem_space);
  M_REQUIRE_NON_NULL(paddr);
  M_REQUIRE( (paddr->page_offset & BYTE_SEL_MASK) == 0, ERR_BAD_PARAMETER, "%s", "Address should be word aligned");
  M_REQUIRE_NON_NULL(l1_cache);
  M_REQUIRE_NON_NULL(l2_cache);
  M_REQUIRE_NON_NULL(word);

  uint32_t paddr_32b = phy_addr_t_to_uint32_t(paddr);
  uint8_t word_index = (paddr->page_offset >> BYTE_SEL_BITS) & WORD_SEL_MASK;
  uint32_t line_addr = paddr_32b & ~((WORD_SEL_MASK << BYTE_SEL_BITS) | BYTE_SEL_MASK);

  uint32_t* p_line;
  uint8_t hit_way;
  uint16_t hit_index;

  /* ================================================================== L1-hit? */
  switch(access){
    case INSTRUCTION:
      FIND(l1_cache, L1_ICACHE);
      break;
    case DATA:
      FIND(l1_cache, L1_DCACHE);
      break;
    default:
      M_EXIT(ERR_BAD_PARAMETER, "%s", "access type is ill defined");
  }

  if(hit_way != HIT_WAY_MISS && hit_index != HIT_INDEX_MISS) {
    *word = p_line[word_index];
    return ERR_NONE;
  }

/* ================================================================== L2-hit? */
  FIND(l2_cache, L2_CACHE);
  // L2 HIT
  if(hit_way != HIT_WAY_MISS && hit_index != HIT_INDEX_MISS) {
    void* cache = l2_cache;
    l2_cache_entry_t* l2_entry = cache_entry(l2_cache_entry_t, L2_CACHE_WAYS, hit_index, hit_way);
    *word = l2_entry->line[word_index];
    l2_to_l1(l1_cache, L1_DCACHE, l2_cache, l2_entry, paddr_32b, replace);
  }
  // L2 MISS
  else {
    //Read (whole) line from memory
    p_line = calloc(L2_CACHE_LINE, 1);
    memcpy(p_line, &(((word_t*)mem_space)[line_addr>>BYTE_SEL_BITS]), L2_CACHE_LINE);
    void* entry;
    switch(access){
      case INSTRUCTION:
        //Initialize a new entry by reading from memory
        entry = malloc(sizeof(l1_icache_entry_t));
        M_EXIT_IF_NULL(entry, sizeof(l1_icache_entry_t));
        M_EXIT_IF_ERR(cache_entry_init(mem_space, paddr, entry, L1_ICACHE), "Error calling cache_entry_init");
        M_EXIT_IF_ERR(l1_insert(l1_cache, entry, L1_ICACHE, l2_cache, paddr_32b, replace), "Error inserting in l1 instruction cache (from memory)");

        *word = ((l1_icache_entry_t*)entry)->line[word_index];
        break;
      case DATA:
        //Initialize a new entry by reading from memory
        entry = malloc(sizeof(l1_dcache_entry_t));
        M_EXIT_IF_NULL(entry, sizeof(l1_dcache_entry_t));
        M_EXIT_IF_ERR(cache_entry_init(mem_space, paddr, entry, L1_DCACHE), "Error calling cache_entry_init");
        M_EXIT_IF_ERR(l1_insert(l1_cache, entry, L1_DCACHE, l2_cache, paddr_32b, replace), "Error inserting in l1 data cache (from memory)");

        *word = ((l1_dcache_entry_t*)entry)->line[word_index];
        break;
      default:
        M_EXIT(ERR_BAD_PARAMETER, "%s", "access type is ill defined");
    }
  }

  return ERR_NONE;
}

#undef FIND

//=========================================================================

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

#define WRITE_LINE_IN_MEM(CACHE_LINE) \
    memcpy(&(((word_t*)mem_space)[line_addr>>BYTE_SEL_BITS]), p_line, CACHE_LINE)

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
    uint32_t paddr_32b = phy_addr_t_to_uint32_t(paddr);
    uint32_t line_addr = paddr_32b & ~((WORD_SEL_MASK << BYTE_SEL_BITS) | BYTE_SEL_MASK);

    uint32_t* p_line;
    uint8_t hit_way;
    uint16_t hit_index;

    M_EXIT_IF_ERR(cache_hit(mem_space, l1_cache, paddr, &p_line, &hit_way, &hit_index, L1_DCACHE), "Error calling cache_hit on l1 data cache");
    if(hit_way != HIT_WAY_MISS && hit_index != HIT_INDEX_MISS){
        //Modify one word of the read line and reinsert it in l1 data cache
        MODIFY_AND_REINSERT(l1_cache, l1_dcache_entry_t, L1_DCACHE_WAYS, L1_DCACHE_LINE);

        //Write the whole line in memory (write through cache)
        WRITE_LINE_IN_MEM(L1_DCACHE_LINE);

    }else{

        M_EXIT_IF_ERR(cache_hit(mem_space, l2_cache, paddr, &p_line, &hit_way, &hit_index, L2_CACHE), "Error calling cache_hit on l2 cache");
        if(hit_way != HIT_WAY_MISS && hit_index != HIT_INDEX_MISS){
            p_line[word_index] = *word;
            MODIFY_AND_REINSERT(l2_cache, l2_cache_entry_t, L2_CACHE_WAYS, L2_CACHE_LINE);

            //Bring the information from the l2 cache to the l1 cache
            void* cache = l2_cache;
            l2_cache_entry_t* l2_entry = cache_entry(l2_cache_entry_t, L2_CACHE_WAYS, hit_index, hit_way);
            l2_to_l1(l1_cache, L1_DCACHE, l2_cache, l2_entry, paddr_32b, replace);

            WRITE_LINE_IN_MEM(L2_CACHE_LINE);

        }else{
            //Read (whole) line from memory
            p_line = calloc(L2_CACHE_LINE, 1);
            memcpy(p_line, &(((word_t*)mem_space)[line_addr>>BYTE_SEL_BITS]), L2_CACHE_LINE);

            //Modify word and write back the whole line to main mem.
            p_line[word_index] = *word;
            WRITE_LINE_IN_MEM(L2_CACHE_LINE);

            l1_dcache_entry_t* entry = malloc(sizeof(l1_dcache_entry_t));
            M_REQUIRE_NON_NULL(entry);
            M_EXIT_IF_ERR(cache_entry_init(mem_space, paddr, entry, L1_DCACHE), "Error trying to initialize a new l1 data cache entry");
            M_EXIT_IF_ERR(l1_insert(l1_cache, entry, L1_DCACHE, l2_cache, paddr_32b, replace), "Error inserting in l1 data cache (from memory)");
      }
    }
    return ERR_NONE;
}

#undef WRITE_LINE_IN_MEM
#undef MODIFY_AND_REINSERT

//=========================================================================

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
    uint8_t byte_sel = paddr->page_offset & BYTE_SEL_MASK;

    //Read the whole word (in which the byte is)
    uint32_t word;
    cache_read(mem_space, &word_aligned, DATA, l1_cache, l2_cache, &word, replace);

    //Modify the byte
    ((uint8_t*)&word)[byte_sel] = p_byte;

    //Write it back
    cache_write(mem_space, &word_aligned, l1_cache, l2_cache, &word, replace);

    return ERR_NONE;
}
