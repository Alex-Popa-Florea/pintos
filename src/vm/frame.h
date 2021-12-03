#ifndef VM_FRAME_H
#define VM_FRAME_H

#include "threads/palloc.h"
#include "lib/kernel/list.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include "vm/swap.h"


/*
  Type for the number which identifies an allocated page
*/
typedef int frame_nr;


/*
  Struct to store an entry in the frame table 
*/
typedef struct {
  void *page;                
  struct list_elem elem;
  bool r_bit;
} frame_table_entry;


/*
  Global list to track the pages which are occupied
*/
struct list frame_table;


/*
  Lock to ensure synchronized access to the frame table
*/
struct lock frame_table_lock;


/*
  Initialises a frame table
*/
void init_frame_table (void);


/*
  Tries to allocate a page of memory using the flags provided
  Returns the pointer to page frame if successful, otherwise NULL
*/
frame_table_entry *try_allocate_page (enum palloc_flags,supp_pte *);


/* Frees the given page and its corresponding frame table entry */
void free_frame_table_entry_from_page (void* page);


/*
  Frees the given page in a thread's supplemental page table 
  and its corresponding frame table entry
*/
void free_frame_from_supp_pte (struct hash_elem *e, void *aux UNUSED);


/*
  Frees the memory for all pages associated with a thread
*/
void free_frame_table_entries_of_thread (struct thread *);


/*
  Retrieves the number of a page from a frame table entry
*/
frame_nr get_frame_nr (frame_table_entry *);

#endif 