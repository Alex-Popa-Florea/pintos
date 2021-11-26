#ifndef SUPP_PAGE_TABLE
#define SUPP_PAGE_TABLE

#include "lib/kernel/hash.h"
#include "filesys/off_t.h"


typedef struct {
  uint8_t *addr;        /* Upage also the key */
  struct file *file;  
  off_t ofs;  
  uint32_t read_bytes; 
  uint32_t zero_bytes; 
  bool writable;

  struct hash_elem hash_elem;
} supp_page_table_entry;

unsigned supp_hash(const struct hash_elem *, void *);
bool supp_hash_compare (const struct hash_elem *, const struct hash_elem *,void *);

#endif