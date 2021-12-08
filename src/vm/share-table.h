#ifndef SHARE_TABLE
#define SHARE_TABLE

#include "lib/kernel/hash.h"
#include "filesys/off_t.h"
#include "filesys/inode.h"
#include "vm/supp-page-table.h"



/*
  Global share hash table
*/
extern struct hash share_table;

/*
  Global lock to ensure synchronized access to the Share Table
*/
extern struct lock share_table_lock;


/*
  Struct for an entry in the share table
*/
typedef struct {
  frame_table_entry *frame;        /* Frame of memory being shared */
  struct list sharing_ptes;        /* List of supplemental page table entries that share the frame */
  struct hash_elem elem;           /* Hash table elem */
} share_entry;



/*
  Creates a share table entry from a supplemental page table entry
  and frame table entry.
  Returns the pointer if memory allocation is successful, otherwise NULL
*/
share_entry *create_share_entry (supp_pte *, frame_table_entry *);


/*
  Initialises the global Share Table
*/
void init_share_table (void);


/* 
  Hash function for the Share Table
*/
unsigned share_hash (const struct hash_elem *, void *);


/* 
  Comparison function for the Share Table using the keys 
  (inode and offset of file) for table entries
*/
bool share_hash_compare (const struct hash_elem *, const struct hash_elem *, void *);

#endif