#include "share-table.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "vm/supp-page-table.h"
#include "filesys/file.h"


struct hash share_table;

struct lock share_table_lock;

int
share_key (supp_pte *pte) {
  struct inode *inode = file_get_inode (pte->file);
  int k1 = inode;
  int k2 = pte->ofs;
  return ((k1 + k2) * (k1 + k2 + 1) + k2) / 2;
}


sharing_thread *
create_sharing_thread (struct thread *t) {
  sharing_thread *share = (sharing_thread *) malloc (sizeof (sharing_thread));
  share->thread = t;
  return share;
}


share_entry *
create_share_entry (supp_pte *pte, void *page) {
    share_entry *entry = (share_entry *) malloc (sizeof (share_entry));
    if (entry == NULL) {
      return NULL;
    }

    entry->key = share_key (pte);
    entry->inode = file_get_inode (pte->file);
    entry->ofs = pte->ofs;
    entry->page = page;
    list_init (&entry->sharing_threads);
    sharing_thread *share = create_sharing_thread (thread_current ());
    list_push_back (&entry->sharing_threads, &share->elem);
    return entry;
}



void set_share_entry_page (share_entry *entry, void *page) {
  entry->page = page;
}


void 
init_share_table (void) {
  hash_init (&share_table, &share_hash, &share_hash_compare, NULL);
  lock_init (&share_table_lock);
}



unsigned 
share_hash (const struct hash_elem *e, void *aux UNUSED) {
  const share_entry *entry = hash_entry (e, share_entry, elem);
  return hash_int (entry->key);
}



bool 
share_hash_compare (const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED) {
  const share_entry *entryA = hash_entry (a,  share_entry, elem);
  const share_entry *entryB = hash_entry (b,  share_entry, elem);

  return entryA->key < entryB->key;
}



void 
share_destroy (struct hash_elem *e, void *aux UNUSED) {
  share_entry *entry = hash_entry (e, share_entry, elem);
  struct list_elem *el;
  for (el = list_begin (&entry->sharing_threads); el != list_end (&entry->sharing_threads); el = list_next (el)) {
    sharing_thread *item = list_entry (el, sharing_thread, elem);
    free (item);
    
  }
  free (entry);
}