#include "vm/frame.h"

#include "threads/malloc.h"
#include "threads/palloc.h"
#include "lib/kernel/list.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "threads/thread.h"
#include "vm/supp-page-table.h"
#include "userprog/syscall.h"
#include "vm/swap.h"
#include "userprog/pagedir.h"
#include <stdio.h>

/* Global Frame Table */
struct list frame_table;

/* Global Frame Table Lock */
struct lock frame_table_lock; 

void 
init_frame_table (void) {
  list_init (&frame_table);
  lock_init (&frame_table_lock);
}

frame_table_entry *
try_allocate_page (enum palloc_flags flags, void *entry_ptr) {
  supp_pte *entry = (supp_pte *) entry_ptr;
  void *page = palloc_get_page (flags);

  if (page) {
    // Successful allocation of frame
    frame_table_entry *new_frame = (frame_table_entry *) malloc (sizeof (frame_table_entry));
    new_frame->page = page;
    new_frame->r_bit = false;

    lock_acquire (&frame_table_lock);
    list_push_back (&frame_table, &new_frame->elem);
    lock_release (&frame_table_lock);
    return new_frame;

  } else {
    // No frame is free, eviction required

    lock_acquire (&frame_table_lock);
    struct list_elem *e = list_begin (&frame_table);

    // One page has been evicted
    bool evicted = false;

    // Whole list has been iterated through and reference bits has been updated
    bool updated = false;
    while (!evicted) {

      // Hand points to the current frame table entry
      frame_table_entry *hand = list_entry (e, frame_table_entry, elem);
      uint32_t *pd = thread_current ()->pagedir;

      if (pagedir_is_accessed (pd, hand->page) && !updated) {
        hand->r_bit = true;
        e = list_next (e);
        pagedir_set_accessed (pd, hand->page, false);
      } else {

        if (hand->r_bit == false) {

          //evict
          //deal with dirty bits
          if (entry->page_source == STACK) {
            entry->page_source = SWAP_SPACE;
            load_page_into_swap_space (hand->page);   //collapse into 1 later 

          } else if (pagedir_is_dirty (pd, hand->page)) {
            entry->page_source = SWAP_SPACE;
            load_page_into_swap_space (hand->page);
          }
          if (entry->page_source == MMAP) {
            struct list *file_list = &thread_current ()->mmapped_file_list;
            struct list_elem *e;

            for (e = list_begin (file_list); e != list_end (file_list); e = list_next (e)) {
              mapped_file *map_entry = list_entry (e, mapped_file, mapped_elem);
              if (map_entry->entry->page_frame->page == entry->page_frame->page) {  //maybe match entries or addr?
                munmap (map_entry->mapping);
              }
            }
          } else {
            e = list_remove (&hand->elem);

            struct hash_iterator i;

            hash_first (&i, &thread_current ()->supp_page_table);
            while (hash_next (&i)) {
              supp_pte *old_entry = hash_entry (hash_cur (&i), supp_pte, elem);
              if (old_entry->page_frame) {
                if (old_entry->page_frame->page == hand->page) {
                  old_entry->page_frame = NULL;
                  pagedir_clear_page (thread_current ()->pagedir, old_entry->addr);
                }
              }
            }

            palloc_free_page (hand->page);
            free (hand);
          }
          evicted = true;
        } else {
          hand->r_bit = false;
          e = list_next (e);
        }
      }

      if (e == list_end (&frame_table)) {
        e = list_begin (&frame_table);
        updated = true;
      }
    }
    lock_release (&frame_table_lock);
    return try_allocate_page (flags, entry);
  }
  
}

void
free_frame_table_entries_of_thread (struct thread *t) {
  lock_acquire (&frame_table_lock);
  struct hash supp_table = t->supp_page_table;

  hash_apply (&supp_table, &free_frame_from_supp_pte);
  lock_release (&frame_table_lock);
}

void 
free_frame_from_supp_pte (struct hash_elem *e, void *aux UNUSED) {
  supp_pte *entry = hash_entry (e, supp_pte, elem);

  frame_table_entry *f = entry->page_frame;
  if (f != NULL) {
    list_remove (&f->elem);
    pagedir_clear_page (thread_current ()->pagedir, entry->addr);
    entry->page_frame = NULL;
    palloc_free_page (f->page);
    free (f);
  }
  entry->page_frame = NULL;
}

