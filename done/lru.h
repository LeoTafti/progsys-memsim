/**
 * @file lru.h
 * @brief Macros for 'Least Recently Used' cache eviction policy
 *
 * @author Juillard Paul, Tafti Leo
 * @date 2019
 */

/**
* increment ages in each way
* reset (to 0) the age of WAY_INDEX's entry
*/
#define LRU_age_increase(TYPE, WAYS, WAY_INDEX, LINE_INDEX) \
  do { \
    foreach_way(w, WAYS) { \
      if(w == WAY_INDEX) cache_age(TYPE, WAYS, LINE_INDEX, w) = 0; \
      else { \
        TYPE* entry = cache_entry(TYPE, WAYS, LINE_INDEX, w); \
        if(entry->age < (WAYS-1)) (entry->age)++; \
      } \
    } \
  } while(0) 

/**
* increment the age every way-entries that are younger then WAY_INDEX's age
* reset (to 0) the age of WAY_INDEX's entry
*/
#define LRU_age_update(TYPE, WAYS, WAY_INDEX, LINE_INDEX) \
  do { \
    uint8_t thresh = cache_age(TYPE, WAYS, LINE_INDEX, WAY_INDEX); \
    foreach_way(w, WAYS) { \
      /* reset WAY_INDEX-entry's age */ \
      if(w == WAY_INDEX) cache_age(TYPE, WAYS, LINE_INDEX, WAY_INDEX) = 0; \
      /* increment if age is smaller than threshold */ \
      else { \
        TYPE* cache_entry = cache_entry(TYPE,WAYS, LINE_INDEX, w); \
        if(cache_entry->age < thresh) (cache_entry->age)++; \
      } \
    } \
  } while(0)
