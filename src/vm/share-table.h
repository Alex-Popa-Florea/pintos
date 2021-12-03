#ifndef SHARE_TABLE
#define SHARE_TABLE

#include "lib/kernel/hash.h"
#include "filesys/off_t.h"
#include "filesys/file.h"
#include "vm/frame.h"
#include "vm/supp-page-table.h"


/*
  Share hash table
*/
struct hash share_table;

/*
  Lock to ensure synchronized access to the Share Table
*/
struct lock share_table_lock;



/*
  Struct for an entry in the share table
*/
typedef struct {
  int key;                      /* Key for hash table */
  struct inode *inode;          /* File's inode, used to uniquely identify a file */
  off_t ofs;                    /* Offset in the file for current page */ 
  void *page;                   /* Pointer to page if loaded, NULL otherwise */
  struct list sharing_threads;  /* List of all the threads sharing a frame */
  struct hash_elem elem;        /* Hash table elem */
} share_entry;



/*
  Wrapper struct so that a thread can be put in the sharing list of multiple frames
*/
typedef struct {
  struct thread *thread;      /* Pointer to the thread */
  struct list_elem elem;      /* Elem to insert into a list */
} sharing_thread;



/*
  Use the Cantor Pairing Function to create a bijection from the pair
  of struct inode * and ofs to integers, for a unique key in the share table
    These are stored in a supplemental page table entry 
*/
int share_key (supp_pte *);



sharing_thread *create_sharing_thread (struct thread *);


/*
  Creates a share table entry from a supplemental page table entry
  Calculates the key from the file's inode and offset
  Returns the pointer if memory allocation is successful, otherwise NULL
*/
share_entry *create_share_entry (supp_pte *);


/*
  Sets the page member of a share table entry
*/
void set_share_entry_page (share_entry *, void *);


/*
  Initialises the global Share Table
*/
void init_share_table (void);


/* 
  Hash function for the Share Table
*/
unsigned share_hash (const struct hash_elem *, void *);


/* 
  Comparison function for the Share Table using the keys for table entries
*/
bool share_hash_compare (const struct hash_elem *, const struct hash_elem *, void *);


/* 
  Used within hash_destroy to free elements of the Share Table
*/
void destroy_elem (struct hash_elem *, void *);

#endif