#include "supp-page-table.h"
#include <debug.h>


unsigned 
supp_hash (const struct hash_elem *e, void *aux UNUSED) {
  const supp_page_table_entry *entry = hash_entry (e, supp_page_table_entry, elem);
  return hash_bytes(&entry->addr, sizeof entry->addr);
}

bool 
supp_hash_compare (const struct hash_elem *a, const struct hash_elem *b,void *aux UNUSED) {
  const supp_page_table_entry *entryA = hash_entry (a,  supp_page_table_entry, elem);
  const supp_page_table_entry *entryB = hash_entry (b,  supp_page_table_entry, elem);

  return entryA->addr < entryB->addr;
}

void 
destroy_elem (struct hash_elem *e, void *aux) {
  supp_page_table_entry *entry = hash_entry (e, supp_page_table_entry, elem);
  free (entry);
}



