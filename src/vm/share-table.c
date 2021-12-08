#include "share-table.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "vm/supp-page-table.h"
#include "filesys/file.h"


struct hash share_table;
struct lock share_table_lock;

share_entry *
create_share_entry (supp_pte *pte, frame_table_entry *frame) {
    share_entry *entry = (share_entry *) malloc (sizeof (share_entry));
    if (entry == NULL) {
      return NULL;
    }

    entry->frame = frame;
    list_init (&entry->sharing_ptes);
    list_push_back (&entry->sharing_ptes, &pte->share_elem);
    return entry;
}



void 
init_share_table (void) {
  hash_init (&share_table, &share_hash, &share_hash_compare, NULL);
  lock_init (&share_table_lock);
}



unsigned 
share_hash (const struct hash_elem *e, void *aux UNUSED) {
  const share_entry *entry = hash_entry (e, share_entry, elem);
  return hash_int ((int) entry->frame->inode) + hash_int (entry->frame->ofs);
}



bool 
share_hash_compare (const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED) {
  const share_entry *entryA = hash_entry (a,  share_entry, elem);
  const share_entry *entryB = hash_entry (b,  share_entry, elem);
  
  struct inode *entryA_inode = entryA->frame->inode;
  struct inode *entryB_inode = entryB->frame->inode;
  off_t entryA_ofs = entryA->frame->ofs;
  off_t entryB_ofs = entryB->frame->ofs;

  if (entryA_inode < entryB_inode) {
    return true;
  } else if (entryA_inode == entryB_inode){
    return entryA_ofs < entryB_ofs;
  } else {
    return false;
  }
}
