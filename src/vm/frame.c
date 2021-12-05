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

// /*
//   Frees the memory occupied by a frame table entry
// */
// static void
// free_frame_table_entry (frame_table_entry *frame_table_entry) {
//   lock_acquire (&frame_table_lock);
//   list_remove (&frame_table_entry->elem);
//   lock_release (&frame_table_lock);
//   free (frame_table_entry);
// }

// void
// free_frame_table_entry_from_page (void* page) {
//   lock_acquire (&frame_table_lock);
//   struct list_elem *e;
//   for (e = list_begin (&frame_table); e != list_end (&frame_table);) {
//     frame_table_entry *f = list_entry (e, frame_table_entry, elem);
//     if (f->page == page) {
//       e = list_remove (&f->elem);
//       free (f);
//       return;
//     } else {
//       e = list_next (e);
//     }
//   }
//   lock_release (&frame_table_lock);
// }

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
    if (&f->elem == current_entry_elem) {
      current_entry_elem = next_frame_table_elem (current_entry_elem);
    }

    list_remove (&f->elem);
    pagedir_clear_page (thread_current ()->pagedir, entry->addr);
    entry->page_frame = NULL;
    palloc_free_page (f->page);
    free (f);
  }

  entry->page_frame = NULL;
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

