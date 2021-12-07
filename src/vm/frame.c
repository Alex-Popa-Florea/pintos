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

static bool check_page_access_bit (struct list *, frame_table_entry *);
static struct list_elem *next_frame_table_elem (struct list_elem *e);

static struct list_elem *remove_frame_table_elem (struct list_elem *e);


void 
init_frame_table (void) {
  list_init (&frame_table);
  lock_init (&frame_table_lock);
  current_entry_elem = list_head (&frame_table);
}

static frame_table_entry *
create_frame (void *page, supp_pte *entry) {
  frame_table_entry *new_frame = (frame_table_entry *) malloc (sizeof (frame_table_entry)); // check for malloc
    if (new_frame == NULL) {
      return NULL;
    }
    new_frame->creator = entry;
    new_frame->page = page;
    new_frame->r_bit = false;
    new_frame->inode = file_get_inode (entry->file);
    new_frame->ofs = entry->ofs;
    new_frame->can_be_shared = !(entry->writable) && (entry->page_source == DISK);

    list_push_back (&frame_table, &new_frame->elem);
    //printf ("we added normally %p on page %p\n", entry->addr, page);
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
    return try_allocate_page (flags, entry_ptr);
  }
}

static bool
check_page_access_bit (struct list *entries, frame_table_entry *hand) {
  bool is_accessed = false;
  struct list_elem *e;

  for (e = list_begin (entries); e != list_end (entries); e = list_next (e)) {
    supp_pte *entry = list_entry (e, supp_pte, share_elem);

    struct thread *sharing_thread = entry->thread;
    uint32_t *pd = sharing_thread->pagedir;

    if (pagedir_is_accessed (pd, hand->page)) {
      /* Set the reference bit to true because the page has been accessed */
      hand->r_bit = true;
      is_accessed = true;
      pagedir_set_accessed (pd, hand->page, false);
    }
  }

  return is_accessed;
}

static void 
evict_sharing_entries (share_entry *found_share_entry, frame_table_entry *f) {

  struct list entries = found_share_entry->sharing_ptes;

  struct list_elem *e;

  for (e = list_begin (&entries); e != list_end (&entries); ) {
    supp_pte *entry = list_entry (e, supp_pte, share_elem);
    pagedir_clear_page (entry->thread->pagedir, entry->addr);
    entry->page_frame = NULL;
    e = list_remove (e);
  }

  list_remove (&f->elem);
  palloc_free_page (f);
  free (f);
  hash_delete (&share_table, &found_share_entry->elem);
  free (found_share_entry);
}

/* Evicts page based on the clock algorithm */
void
evict (void) {
  //printf ("we try to evict\n");
  /* Exit loop once a page has been evicted */
  bool evicted = false;
  frame_table_entry *hand;
  while (!evicted) {
    // Hand points to the current frame table entry
    current_entry_elem = next_frame_table_elem (current_entry_elem);
    hand = list_entry (current_entry_elem, frame_table_entry, elem);
    //printf ("we loop through, now at page  %p\n", hand->page);

    if (hand->can_be_shared) {
      //printf ("we think to evict shared  %p\n", hand->page);
      share_entry search;
      search.frame = hand;

      struct hash_elem *search_elem = hash_find (&share_table, &search.elem);
      if (search_elem != NULL) {
        share_entry *found_entry = hash_entry (search_elem, share_entry, elem);
        if (!check_page_access_bit (&found_entry->sharing_ptes, hand)) {

          if (hand->r_bit == false) {
            //printf ("we evict shared  %p\n", hand->page);

            /* Evict the first page without a set reference bit */
            evict_sharing_entries (found_entry, hand);
            
            evicted = true;
          } else {
            hand->r_bit = false;
          }
        }
      }
    } else {
      //printf ("we think to evict single  %p\n", hand->page);
      supp_pte *to_be_evicted_entry = (supp_pte *) hand->creator;
      
      struct thread *eviction_thread = to_be_evicted_entry->thread;
      uint32_t *pd = eviction_thread->pagedir;

      if (pagedir_is_accessed (pd, hand->page)) {
        /* Set the reference bit to true because the page has been accessed */
        hand->r_bit = true;
        pagedir_set_accessed (pd, hand->page, false);
      } else {
        if (hand->r_bit == false) {
          //printf ("we evict single  %p\n", hand->page);

          /* Evict the first page without a set reference bit */
          if (to_be_evicted_entry->page_source == MMAP) {
            //printf ("we evict mmap  %p\n", hand->page);
            /* Unmap memory-mapped files */
            struct list *file_list = &eviction_thread->mmapped_file_list;
            struct list_elem *e;

            for (e = list_begin (file_list); e != list_end (file_list); e = list_next (e)) {
              mapped_file *map_entry = list_entry (e, mapped_file, mapped_elem);
              if (map_entry->entry->page_frame->page == hand->page) { 
                munmap_for_thread (map_entry->mapping, eviction_thread);
              }
            }
          } else {
            //printf ("we evict non mmap  %p\n", hand->page);
            if (to_be_evicted_entry->page_source == STACK || pagedir_is_dirty (pd, hand->page)) {
              //printf ("we evict loadable  %p\n", hand->page);
              /* Stack pages and dirty pages are written to swap space */
              to_be_evicted_entry->is_in_swap_space = true;
              load_page_into_swap_space (to_be_evicted_entry->addr, hand->page);
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
free_frame_table_entries_of_thread (struct thread *t) {
  lock_tables ();
  struct hash supp_table = t->supp_page_table;
  hash_apply (&supp_table, &free_frame_from_supp_pte);
  release_tables ();
}


void 
free_frame_from_supp_pte (struct hash_elem *e, void *aux) {
  struct thread *t = (struct thread *) aux;
  supp_pte *entry = hash_entry (e, supp_pte, elem);
  pagedir_clear_page (t->pagedir, entry->addr);

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
          current_entry_elem = next_frame_table_elem (current_entry_elem);
        }
        list_remove (&f->elem);
        entry->page_frame = NULL;
        palloc_free_page (f->page);
        free (f);
        hash_delete (&share_table, &found_share_entry->elem);
        free (found_share_entry);
      }
    } else {
      if (&f->elem == current_entry_elem) {
        current_entry_elem = next_frame_table_elem (current_entry_elem);
      }
      list_remove (&f->elem);
      entry->page_frame = NULL;
      palloc_free_page (f->page);
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