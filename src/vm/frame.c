#include "vm/frame.h"

#include "threads/malloc.h"
#include "threads/palloc.h"
#include "lib/kernel/list.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "threads/thread.h"

struct list frame_table;

void 
init_frame_table (void) {
  list_init (&frame_table);
  lock_init (&frame_table_lock);
}

void *
try_allocate_page (enum palloc_flags flags) {
  void *page = palloc_get_page (flags);

  if (page) {
    frame_table_entry *new_frame = (frame_table_entry *) malloc (sizeof (frame_table_entry));
    new_frame->page = page;
    new_frame->thread = thread_current ();
    lock_acquire (&frame_table_lock);
    list_push_back (&frame_table, &new_frame->elem);
    lock_release (&frame_table_lock);
  } 
  return page;
}

void
free_frame_table_entry (frame_table_entry *frame_table_entry) {
  lock_acquire (&frame_table_lock);
  list_remove (&frame_table_entry->elem);
  //palloc_free_page (frame_table_entry->page);
  lock_release (&frame_table_lock);
  free (frame_table_entry);
}

void
free_frame_table_entry_of_page (void* page) {
  lock_acquire (&frame_table_lock);
  struct list_elem *e;
  for (e = list_begin (&frame_table); e != list_end (&frame_table);) {
    frame_table_entry *f = list_entry (e, frame_table_entry, elem);
    if (f->page == page) {
      e = list_remove (&f->elem);
      //palloc_free_page (page);
      free (f);
    } else {
      e = list_next (e);
    }
  }
  lock_release (&frame_table_lock);
}

void
free_frame_table_entries_of_thread (struct thread *t) {
  lock_acquire (&frame_table_lock);
  struct list_elem *e;
  for (e = list_begin (&frame_table); e != list_end (&frame_table);) {
    frame_table_entry *f = list_entry (e, frame_table_entry, elem);
    if (f->thread == t) {
      e = list_remove (&f->elem);
      //palloc_free_page (f->page);
      free (f);
    } else {
      e = list_next (e);
    }
  }
  lock_release (&frame_table_lock);
}

frame_nr
get_frame_nr (frame_table_entry *frame_entry) {
  return (int) frame_entry->page / PGSIZE;
} 
