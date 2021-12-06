#include "frame.h"

#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "threads/thread.h"
#include "vm/supp-page-table.h"
#include "userprog/syscall.h"
#include "vm/swap.h"
#include "userprog/pagedir.h"
#include <stdio.h>
#include "share-table.h"

/* Global Frame Table */
struct list frame_table;

/* Global Frame Table Lock */
struct lock frame_table_lock; 

/* Pointer to the list elem for the current frame table entry
   Used for the clock algorithm */
struct list_elem *current_entry_elem;


static struct list_elem *next_frame_table_elem (struct list_elem *e);

static struct list_elem *remove_frame_table_elem (struct list_elem *e);


void 
init_frame_table (void) {
  list_init (&frame_table);
  lock_init (&frame_table_lock);
  current_entry_elem = list_head (&frame_table);
}


frame_table_entry *
try_allocate_page (enum palloc_flags flags, void *entry_ptr) {
  supp_pte *entry = (supp_pte *) entry_ptr;
  void *page = palloc_get_page (flags);

  if (page) {
    /* Successful allocation of frame */
    frame_table_entry *new_frame = (frame_table_entry *) malloc (sizeof (frame_table_entry));
    new_frame->page = page;
    new_frame->inode = NULL;
    new_frame->ofs = NULL;
    new_frame->r_bit = false;

    lock_acquire (&frame_table_lock);
    list_push_back (&frame_table, &new_frame->elem);
    lock_release (&frame_table_lock);
    return new_frame;
  } 
  return NULL;
}


void set_inode_and_ofs (frame_table_entry *entry, struct inode *inode, off_t ofs) {
  entry->inode = inode;
  entry->ofs = ofs;
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
  lock_acquire (&share_table_lock);
  supp_pte *entry = hash_entry (e, supp_pte, elem);
  pagedir_clear_page (thread_current ()->pagedir, entry->addr);

  frame_table_entry *f = entry->page_frame;
  if (f != NULL) {

    share_entry search_entry;
    search_entry.key = share_key (entry);
    struct hash_elem *search_elem = hash_find (&share_table, &search_entry.elem);

    if (search_elem != NULL) {
      printf ("share elem: %p\n", search_elem);

      share_entry *found_share_entry = hash_entry (search_elem, share_entry, elem);
      struct list_elem *element;
      for (element = list_begin (&found_share_entry->sharing_threads); element != list_end (&found_share_entry->sharing_threads); element = list_next (element)) {
        sharing_thread *current_thread = list_entry (element, sharing_thread, elem);
        if (current_thread->thread->tid == thread_current ()->tid) {
          list_remove (&current_thread->elem);
          break;
        }
      }

      if (list_size (&found_share_entry->sharing_threads) <= 1) {

        list_remove (&f->elem);
        entry->page_frame = NULL;
        palloc_free_page (f->page);
        free (f);
        
      }
    } else {

      list_remove (&f->elem);
      entry->page_frame = NULL;
      palloc_free_page (f->page);
      free (f);
      
    }
    

  }
  lock_release (&share_table_lock);

}


/*
  Returns the next element for a frame table entry, looping around from
  the end to the start of the list
*/
static struct list_elem *
next_frame_table_elem (struct list_elem *e) {
  struct list_elem *next = list_next (e);
  if (next == list_end (&frame_table)) {
    next = list_begin (&frame_table);
  }
  return next;
}


/*
  Removes an element for a frame table entry and returns the next one, 
  looping around from the end to the start of the list
*/
static struct list_elem *
remove_frame_table_elem (struct list_elem *e) {
  struct list_elem *next = list_remove (e);
  if (next == list_end (&frame_table)) {
    next = list_begin (&frame_table);
  }
  return next;
}

