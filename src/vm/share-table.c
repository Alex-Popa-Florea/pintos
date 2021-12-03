#include "share-table.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "vm/supp-page-table.h"


int
share_key (supp_pte *pte) {
  int k1 = (int) pte->file->inode;
  int k2 = pte->ofs;
  return 0.5 * (k1 + k2) * (k1 + k2 + 1) + k2;
}


share_entry *
create_share_entry (supp_pte *pte) {
    share_entry *entry = (share_entry *) malloc (sizeof (share_entry));
    if (entry == NULL) {
      return NULL;
    }

    entry->key = calculate_key (pte->file, pte->ofs);
    entry->inode = pte->file->inode;
    entry->ofs = pte->ofs;
    entry->page = NULL;
    return entry;
}


void 
init_share_table (void) {
  list_init (&share_table);
  lock_init (&share_table_lock);
}


unsigned 
share_hash (const struct hash_elem *e, void *aux UNUSED) {
  const share_entry *entry = hash_entry (e, share_entry, elem);
  return hash_int (entry->key);
}


bool 
supp_hash_compare (const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED) {
  const share_entry *entryA = hash_entry (a,  share_entry, elem);
  const share_entry *entryB = hash_entry (b,  share_entry, elem);

  return entryA->key < entryB->key;
}


void 
destroy_elem (struct hash_elem *e, void *aux UNUSED) {
  share_entry *entry = hash_entry (e, share_entry, elem);
  free (entry);
}