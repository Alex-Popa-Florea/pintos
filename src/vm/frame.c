#include "vm/frame.h"

#include "threads/malloc.h"
#include "threads/palloc.h"
#include "lib/kernel/list.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "threads/thread.h"
#include "vm/supp-page-table.h"

struct list frame_table;

static void free_frame_table_entry (frame_table_entry *);

void 
init_frame_table (void) {
  list_init (&frame_table);
  lock_init (&frame_table_lock);
}

frame_table_entry *
try_allocate_page (enum palloc_flags flags) {
  void *page = palloc_get_page (flags);

  if (page) {
    frame_table_entry *new_frame = (frame_table_entry *) malloc (sizeof (frame_table_entry));
    new_frame->page = page;
    lock_acquire (&frame_table_lock);
    list_push_back (&frame_table, &new_frame->elem);
    lock_release (&frame_table_lock);
    return new_frame;
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
free_frame_table_entries_of_thread (struct thread *t) {
  struct hash supp_table = t->supp_page_table;

  hash_apply (&supp_table, &free_frame_from_supp_pte);
}

void 
free_frame_from_supp_pte (struct hash_elem *e, void *aux UNUSED) {
  supp_pte *entry = hash_entry (e, supp_pte, elem);

  frame_table_entry *f = entry->page_frame;
  if (f != NULL) {
    free_frame_table_entry (f);
  }
  entry->page_frame = NULL;
}

frame_nr
get_frame_nr (frame_table_entry *frame_entry) {
  return (int) frame_entry->page / PGSIZE;
} 
