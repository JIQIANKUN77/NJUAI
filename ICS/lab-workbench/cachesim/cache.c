#include "common.h"
#include <inttypes.h>

void mem_read(uintptr_t block_num, uint8_t *buf);
void mem_write(uintptr_t block_num, const uint8_t *buf);

static uint64_t cycle_cnt = 0;

void cycle_increase(int n) { cycle_cnt += n; }

// TODO: implement the following functions

struct CACHE_SLOT{
  uint8_t data[BLOCK_SIZE];
  uintptr_t tag;
  bool valid;
  bool dirty;
};

static struct CACHE_SLOT *cache_slot;
static uint32_t cache_total_size_width, cache_associativity_width, cache_group_width;
uintptr_t inner_addr_mask, group_number_mask, tag_mask, block_number_mask;

static inline uintptr_t extract_inner_addr(uintptr_t addr){
  return addr & inner_addr_mask;
}
static inline uintptr_t extract_group_number(uintptr_t addr){
  return (addr & group_number_mask) >> BLOCK_WIDTH;
}

static inline uintptr_t extract_tag(uintptr_t addr){
  return (addr & tag_mask) >> (BLOCK_WIDTH + cache_group_width);
}
static inline uintptr_t extract_block_number(uintptr_t addr){
  return (addr & block_number_mask) >> BLOCK_WIDTH;
}


static inline uintptr_t align_inner_addr(uintptr_t addr){
  return (addr & inner_addr_mask) & (~0x3);
}
static inline uintptr_t map_to_cache_addr(uintptr_t addr, uint32_t i){
  return (extract_group_number(addr) << cache_associativity_width) + i;
}
static inline uintptr_t construct_block_number(uintptr_t tag, uint32_t index){
  return (tag << cache_group_width) | index;
}

static inline uint32_t choose(uint32_t n) { return rand() % n; }

static void write_back(struct CACHE_SLOT* target_cache, uintptr_t addr){
  mem_write(construct_block_number(target_cache->tag, extract_group_number(addr)), target_cache->data);
  target_cache->dirty = false;
}

static void read_from(uintptr_t addr, uint32_t index){
  struct CACHE_SLOT* target_cache = &cache_slot[map_to_cache_addr(addr, index)];
  mem_read(extract_block_number(addr), target_cache->data);
  target_cache->valid = true;
  target_cache->dirty = false;
  target_cache->tag = extract_tag(addr);
}

uint32_t cache_read(uintptr_t addr) {
  uintptr_t addr_tag = extract_tag(addr);
  uintptr_t addr_inner = align_inner_addr(addr);
  uintptr_t cache_addr_start = map_to_cache_addr(addr, 0);
  uintptr_t cache_addr_end = cache_addr_start + (1 << cache_associativity_width);

  for (uintptr_t i = cache_addr_start; i < cache_addr_end; i++) {
    struct CACHE_SLOT* slot = &cache_slot[i];
    if (slot->valid && slot->tag == addr_tag) {
        return *reinterpret_cast<uint32_t*>(&slot->data[addr_inner]);
    }
  }

  // Cache miss
  uint32_t index = choose(1 << cache_associativity_width);
  struct CACHE_SLOT* target_cache = &cache_slot[map_to_cache_addr(addr, index)];
  if (target_cache->dirty) {
    write_back(target_cache, addr);
  }

  read_from(addr, index);
  return *reinterpret_cast<uint32_t*>(&target_cache->data[addr_inner]);
}

void cache_write(uintptr_t addr, uint32_t data, uint32_t wmask) {
  uintptr_t addr_tag = extract_tag(addr);
  uintptr_t addr_inner = align_inner_addr(addr);
  uintptr_t cache_addr_start = map_to_cache_addr(addr, 0);
  uintptr_t cache_addr_end = cache_addr_start + (1 << cache_associativity_width);
  struct CACHE_SLOT* target_cache = nullptr;
  bool hit = false;

  for (uintptr_t i = cache_addr_start; i < cache_addr_end; i++) {
    struct CACHE_SLOT* slot = &cache_slot[i];
    if (slot->valid && slot->tag == addr_tag) {
        target_cache = slot;
        hit = true;
        break;
    }
  }

  if (!hit) {
    uint32_t index = choose(1 << cache_associativity_width);
    target_cache = &cache_slot[map_to_cache_addr(addr, index)];
    if (target_cache->valid && target_cache->dirty) {
      write_back(target_cache, addr);
    }
    read_from(addr, index);
  }

  uint32_t *data_target = reinterpret_cast<uint32_t*>(&target_cache->data[addr_inner]);
  *data_target = (*data_target & ~wmask) | (data & wmask);
  target_cache->dirty = true;
}

void init_cache(int total_size_width, int associativity_width) {
  cache_total_size_width = total_size_width;
  cache_associativity_width = associativity_width;
  cache_group_width = cache_total_size_width - BLOCK_WIDTH - cache_associativity_width;
  
  inner_addr_mask = mask_with_len(BLOCK_WIDTH);
  group_number_mask = mask_with_len(cache_group_width) << BLOCK_WIDTH;
  tag_mask = ~mask_with_len(BLOCK_WIDTH + cache_group_width);
  block_number_mask = ~mask_with_len(BLOCK_WIDTH);

  // Adjust the size of the cache_slot allocation to account for the associativity
  size_t num_slots = (1 << (cache_total_size_width - BLOCK_WIDTH)) / (1 << cache_associativity_width);
  cache_slot = static_cast<struct CACHE_SLOT*>(calloc(num_slots, sizeof(struct CACHE_SLOT)));
  assert(cache_slot);
}
void display_statistic(void) {
}
