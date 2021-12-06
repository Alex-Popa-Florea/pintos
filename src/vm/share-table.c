#include "share-table.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "vm/supp-page-table.h"
#include "filesys/file.h"


struct hash share_table;

struct lock share_table_lock;

sharing_thread *
create_sharing_thread (struct thread *t) {
  sharing_thread *share = (sharing_thread *) malloc (sizeof (sharing_thread));
  share->thread = t;
  return share;
}


// share_entry *
// create_share_entry (supp_pte *pte, void *page) {
//     share_entry *entry = (share_entry *) malloc (sizeof (share_entry));
//     if (entry == NULL) {
//       return NULL;
//     }

//     entry->inode = file_get_inode (pte->file);
//     entry->ofs = pte->ofs;
//     entry->page = page;
//     list_init (&entry->sharing_threads);
//     sharing_thread *share = create_sharing_thread (thread_current ());
//     list_push_back (&entry->sharing_threads, &share->elem);
//     return entry;
// }
share_entry *
create_share_entry (supp_pte *pte, frame_table_entry *frame) {
    share_entry *entry = (share_entry *) malloc (sizeof (share_entry));
    if (entry == NULL) {
      return NULL;
    }

    entry->frame = frame;
    list_init (&entry->sharing_pages);
    list_push_back (&entry->sharing_pages, &pte->share_elem);
    return entry;
}



// void set_share_entry_page (share_entry *entry, void *page) {
//   entry->page = page;
// }


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



// void 
// share_destroy (struct hash_elem *e, void *aux UNUSED) {
//   share_entry *entry = hash_entry (e, share_entry, elem);
//   struct list_elem *el;
//   for (el = list_begin (&entry->sharing_pages); el != list_end (&entry->sharing_pages); el = list_next (el)) {
//     supp_pte *item = list_entry (el, supp_pte, share_elem);  //confused ?
//     free (item);
    
//   }
//   free (entry);
// }