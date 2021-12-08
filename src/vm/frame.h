#ifndef VM_FRAME_H
#define VM_FRAME_H

#include "threads/palloc.h"
#include "lib/kernel/list.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include "filesys/file.h"
#include "filesys/off_t.h"
#include "debug.h"

/*
  Struct to store an entry in the frame table 
*/
typedef struct {
  void *page;               /* The page where data is stored */                
  struct list_elem elem;    /* Elem to insert into frame table */


  bool r_bit;               /* Reference bit */

  /* Information needed for sharing */
  struct inode *inode;      /* Inode to find a share table entry */
  off_t ofs;                /* Offset to find a share table entry */
  bool can_be_shared;       /* Records whether the frame is sharable */
  void *creator;            /* Points to supp_pte that created the frame. Unused for shared frame */

} frame_table_entry;


/*
  Global list to track the pages which are occupied
*/
extern struct list frame_table;


/*
  Lock to ensure synchronized access to the frame table
*/
extern struct lock frame_table_lock;


/*
  Initialises a frame table
*/
void init_frame_table (void);


/*
  Tries to allocate a page of memory using the flags provided
  Returns the pointer to page frame if successful, otherwise NULL
*/
frame_table_entry *try_allocate_page (enum palloc_flags flags, void *entry);

/* Evicts page based on the clock algorithm */
void evict (void);

/*
  Sets the inode and offset members of a frame table entry
*/

/*
  Frees the memory for all pages associated with a thread
*/
void free_frame_table_entries_of_thread (struct thread *);

/*
  Frees the given page in a thread's supplemental page table 
  and its corresponding frame table entry
*/
void free_frame_from_supp_pte (struct hash_elem *, void *);

/*  
  Frame table and share table lock needs to be acquired and released at the same time

  The following functions are used to enforce this.
*/

/*
  Acquires the frame table and share table lock
*/
void lock_tables (void);

/*
  Releases the frame table and share table lock
*/
void release_tables (void);

#endif 