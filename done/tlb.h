#pragma once

/**
 * @file tlb.h
 * @brief definitions associated to a fully-associative TLB
 *
 * @author Mirjana Stojilovic
 * @date 2018-19
 */

#include "addr.h"

#include <stdint.h>

#define TLB_LINES 128 // the number of entries

typedef struct __tlb_entry__ {
  uint64_t tag : VIRT_PAGE_NUM;
  uint32_t phy_page_num : PHY_PAGE_NUM;
  uint8_t v : 1;
} tlb_entry_t;
