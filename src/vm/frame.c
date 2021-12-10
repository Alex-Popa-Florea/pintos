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

struct list frame_table;
struct lock frame_table_lock; 

/* Pointer to the list elem for the current frame table entry
   Used for the clock algorithm */
struct list_elem *current_entry_elem;

static bool check_page_access_bit (struct list *, frame_table_entry *);
static struct list_elem *next_frame_table_elem (struct list_elem *e);
static struct list_elem *prev_frame_table_elem (struct list_elem *e);

void 
init_frame_table (void) {
  list_init (&frame_table);
  lock_init (&frame_table_lock);
  current_entry_elem = list_head (&frame_table);
}

/* 
  Creates a new entry in the frame table from the supplied 
  supplemental page table entry
*/
static frame_table_entry *
create_frame (void *kpage, supp_pte *entry) {
  frame_table_entry *new_frame = (frame_table_entry *) malloc (sizeof (frame_table_entry));
  if (new_frame == NULL) {
    return NULL;
  }
  
  new_frame->creator = entry;
  new_frame->kpage = kpage;
  new_frame->r_bit = false;
  new_frame->inode = file_get_inode (entry->file);
  new_frame->ofs = entry->ofs;
  new_frame->can_be_shared = !(entry->writable) && (entry->page_source == DISK);

  list_push_back (&frame_table, &new_frame->elem);
  return new_frame;
}

frame_table_entry *
try_allocate_page (enum palloc_flags flags, void *entry_ptr) {
  supp_pte *entry = (supp_pte *) entry_ptr;
  void *page = palloc_get_page (flags);

  if (page) {
    /* Successful allocation of frame - creates new frame table entry */
    return create_frame (page, entry);
  } else {
    /* No frame is free, eviction required */
    evict ();
    page = palloc_get_page (flags);
    ASSERT (page != NULL);
    return create_frame (page, entry);
  }
}

/*
  Checks the accesses bits of the pages in the list of threads that share a frame
*/
static bool
check_page_access_bit (struct list *entries, frame_table_entry *hand) {
  bool is_accessed = false;
  struct list_elem *e;

  for (e = list_begin (entries); e != list_end (entries); e = list_next (e)) {
    supp_pte *entry = list_entry (e, supp_pte, share_elem);

    struct thread *sharing_thread = entry->thread;
    uint32_t *pd = sharing_thread->pagedir;

    if (pagedir_is_accessed (pd, entry->uaddr)) {
      /* Set the reference bit to true because the page has been accessed */
      hand->r_bit = true;
      is_accessed = true;
      pagedir_set_accessed (pd, entry->uaddr, false);
    }
  }

  return is_accessed;
}

/*
  Eviction for when the frame is shared between multiple threads - must clear for all
*/
static void 
evict_sharing_entries (share_entry *found_share_entry, frame_table_entry *f) {
  struct list *entries = &found_share_entry->sharing_ptes;
  struct list_elem *e;
  
  while (!list_empty (entries)) {
    e = list_pop_front (entries);
    supp_pte *entry = list_entry (e, supp_pte, share_elem);
    pagedir_clear_page (entry->thread->pagedir, entry->uaddr);
    entry->page_frame = NULL;
  }

  if (&f->elem == current_entry_elem) {
    current_entry_elem = prev_frame_table_elem (current_entry_elem);
  }

  hash_delete (&share_table, &found_share_entry->elem);
  list_remove (&f->elem);

  palloc_free_page (f->kpage);
  free (f);
  free (found_share_entry);
}

/* Evicts page based on the clock page replacement algorithm */
void
evict (void) {
  
/* Exit loop once a page has been evicted */
  bool evicted = false;
  frame_table_entry *hand;
  while (!evicted) {
    current_entry_elem = next_frame_table_elem (current_entry_elem);
    hand = list_entry (current_entry_elem, frame_table_entry, elem);

    if (hand->can_be_shared) {
      share_entry search;
      search.frame = hand;

      struct hash_elem *search_elem = hash_find (&share_table, &search.elem);
      if (search_elem != NULL) {
        share_entry *found_entry = hash_entry (search_elem, share_entry, elem);
        if (!check_page_access_bit (&found_entry->sharing_ptes, hand)) {

          if (hand->r_bit == false) {

            /* Evict the first page without a set reference bit */
            evict_sharing_entries (found_entry, hand);            
            evicted = true;
          } else {
            hand->r_bit = false;
          }
        }
      }
    } else {
      supp_pte *to_be_evicted_entry = (supp_pte *) hand->creator;
      
      struct thread *eviction_thread = to_be_evicted_entry->thread;
      uint32_t *pd = eviction_thread->pagedir;

      if (pagedir_is_accessed (pd, to_be_evicted_entry->uaddr)) {
        /* Set the reference bit to true because the page has been accessed */
        hand->r_bit = true;
        pagedir_set_accessed (pd, to_be_evicted_entry->uaddr, false);
      } else {
        if (hand->r_bit == false) {

          /* Evict the first page without a set reference bit */
          if (to_be_evicted_entry->page_source == MMAP) {
            
            struct list *mapped_list = &eviction_thread->mmapped_file_list;
            struct list_elem *e;

            for (e = list_begin (mapped_list); e != list_end (mapped_list); e = list_next (e)) {
              mapped_file *map_entry = list_entry (e, mapped_file, mapped_elem);
              if (map_entry->entry->page_frame->kpage == hand->kpage) { 

                if (pagedir_is_dirty (pd, to_be_evicted_entry->uaddr)) {
                  file_write_at (to_be_evicted_entry->file, to_be_evicted_entry->page_frame->kpage, to_be_evicted_entry->read_bytes, to_be_evicted_entry->ofs);
                }

                free_frame_from_supp_pte (&to_be_evicted_entry->elem, eviction_thread);
                    
                hash_delete (&eviction_thread->supp_page_table, &to_be_evicted_entry->elem);
                free (to_be_evicted_entry);
                    
                list_remove (e);
                free (map_entry);

                break;
              }
            }
          } else {
            if (to_be_evicted_entry->page_source == STACK || pagedir_is_dirty (pd, hand->kpage)) {
              /* Stack pages and dirty pages are written to swap space */
              to_be_evicted_entry->is_in_swap_space = true;
              load_page_into_swap_space (to_be_evicted_entry, hand->kpage);
            }
            /* Remove the evicted page from the frame table */
            free_frame_from_supp_pte (&to_be_evicted_entry->elem, eviction_thread);
          }

          evicted = true;
        } else {
          hand->r_bit = false;
          
        }
      }
    }

  }
}

void 
free_frame_from_supp_pte (struct hash_elem *e, void *aux) {
  struct thread *t = (struct thread *) aux;
  supp_pte *entry = hash_entry (e, supp_pte, elem);
  pagedir_clear_page (t->pagedir, entry->uaddr);
  frame_table_entry *f = entry->page_frame;

  if (f != NULL) {
    if (f->can_be_shared) {
      share_entry search_entry;
      search_entry.frame = f;

      struct hash_elem *search_elem = hash_find (&share_table, &search_entry.elem);
      share_entry *found_share_entry = hash_entry (search_elem, share_entry, elem);
      list_remove (&entry->share_elem); 

      if (list_empty (&found_share_entry->sharing_ptes)) {
        if (&f->elem == current_entry_elem) {
          current_entry_elem = prev_frame_table_elem (current_entry_elem);
        }
        struct hash_elem *deleted = hash_delete (&share_table, &found_share_entry->elem);
        list_remove (&f->elem);
        entry->page_frame = NULL;
        palloc_free_page (f->kpage);
        free (f);
        ASSERT(deleted);
        free (found_share_entry);
      }
    } else {
      if (&f->elem == current_entry_elem) {
        current_entry_elem = prev_frame_table_elem (current_entry_elem);
      }
      list_remove (&f->elem);
      entry->page_frame = NULL;
      palloc_free_page (f->kpage);
      free (f);
    }
    
  }

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
  Returns the prev element for a frame table entry
*/
static struct list_elem *
prev_frame_table_elem (struct list_elem *e) {
  struct list_elem *prev = list_prev (e);
  return prev;
}

void
lock_tables (void) {
  lock_acquire (&frame_table_lock);
  lock_acquire (&share_table_lock);
}

void
release_tables (void) {
  lock_release (&share_table_lock);
  lock_release (&frame_table_lock);
}