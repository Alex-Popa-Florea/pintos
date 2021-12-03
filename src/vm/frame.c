#include "vm/frame.h"

#include "debug.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "lib/kernel/list.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "threads/thread.h"
#include "vm/supp-page-table.h"
#include "userprog/pagedir.h"
#include "userprog/syscall.h"
#include "vm/swap.h"



struct list frame_table;

static void free_frame_table_entry (frame_table_entry *);

void 
init_frame_table (void) {
  list_init (&frame_table);
  lock_init (&frame_table_lock);
}

frame_table_entry *
try_allocate_page (enum palloc_flags flags,supp_pte *entry) {
  void *page = palloc_get_page (flags);

  if (page) {
    frame_table_entry *new_frame = (frame_table_entry *) malloc (sizeof (frame_table_entry));
    new_frame->page = page;
    lock_acquire (&frame_table_lock);
    list_push_back (&frame_table, &new_frame->elem);
    lock_release (&frame_table_lock);
    return new_frame;
  } else {
    lock_acquire (&frame_table_lock);
    struct list_elem *e = list_begin(&frame_table);
    bool evicted;
    bool updated;
    while(!updated && !evicted){
      frame_table_entry *hand = list_entry(e,frame_table_entry,elem);
      uint32_t pd = thread_current ()->pagedir;

      if(pagedir_is_accessed(pd,hand->page)) {
        hand->r_bit = true;
        e = list_next(e);
      } else {
        if (hand->r_bit == false) {

          //evict
          //deal with dirty bits
          if (entry->page_source == MMAP) {
            struct list *file_list = &thread_current ()->mmapped_file_list;
            struct list_elem *e;

            for (e = list_begin (file_list); e != list_end (file_list); e = list_next (e)) {
              mapped_file *map_entry = list_entry (e, mapped_file, mapped_elem);
              if(map_entry->entry->page_frame->page == entry->page_frame->page){  //maybe match entries or addr?
                munmap(map_entry->mapping);
              }
            }  
          } else if (entry->page_source == STACK) {
            load_page_into_swap_space(entry->page_frame->page);   //collapse into 1 later 
          } else if (pagedir_is_dirty(pd,hand->page)){
            load_page_into_swap_space(entry->page_frame->page);
          }
          free(hand);
          e = list_remove (&hand->elem);
          evicted = true;

        } else {
          hand->r_bit = false;
          e = list_next(e);
        }
      }

      if (e == list_end(&frame_table)){
        e = list_begin(&frame_table);
        updated = true;
      }

    }
    lock_release (&frame_table_lock);
  }
  
  return NULL;
}

/*
  Frees the memory occupied by a frame table entry
*/
static void
free_frame_table_entry (frame_table_entry *frame_table_entry) {
  lock_acquire (&frame_table_lock);
  list_remove (&frame_table_entry->elem);
  lock_release (&frame_table_lock);
  free (frame_table_entry);
}

void
free_frame_table_entry_from_page (void* page) {
  lock_acquire (&frame_table_lock);
  struct list_elem *e;
  for (e = list_begin (&frame_table); e != list_end (&frame_table);) {
    frame_table_entry *f = list_entry (e, frame_table_entry, elem);
    if (f->page == page) {
      e = list_remove (&f->elem); // can we just lock this section and use free_frame_table_entry?
      free (f);
      return;
    } else {
      e = list_next (e);
    }
  }
  lock_release (&frame_table_lock);  //do we have to lock the whole thing?
}

void
free_frame_table_entries_of_thread (struct thread *t) {
  struct hash supp_table = t->supp_page_table;

  hash_apply (&supp_table, &free_frame_from_supp_pte);
  // do we need to lock the spt??
}

void 
free_frame_from_supp_pte (struct hash_elem *e, void *aux UNUSED) {
  supp_pte *entry = hash_entry (e, supp_pte, elem);

  frame_table_entry *f = entry->page_frame;
  if (f != NULL) {
    frame_table_entry (f);
  }
  entry->page_frame = NULL;
}

frame_nr
get_frame_nr (frame_table_entry *frame_entry) {
  return (int) frame_entry->page / PGSIZE;
} 
