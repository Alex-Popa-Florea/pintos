#ifndef SHARE_TABLE
#define SHARE_TABLE

#include "lib/kernel/hash.h"
#include "filesys/off_t.h"
#include "filesys/file.h"
#include "vm/frame.h"


/*
  Share hash table
*/
struct hash share_table;

/*
  Lock to ensure synchronized access to share table
*/
struct lock share_table_lock;


/*
  Struct for an entry in the share table
*/
typedef struct {
  int key;                  /* Key for hash table */
  struct inode *inode;      /* File's inode, used to uniquely identify a file */
  off_t ofs;                /* Offset in the file for current page */ 
  void *page;               /* Pointer to page if loaded, null otherwise */
   
  struct hash_elem elem;    /* Hash table elem */
} share_entry;


/*
  Use the Cantor Pairing Function to create a bijection from the pair
  of struct inode * and ofs to integers, for a unique key in the share table
    These are stored in a supplemental page table entry 
*/
int share_key (supp_pte *);


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
  Hash function for share table
*/
unsigned share_hash (const struct hash_elem *, void *);


/* 
  Comparison function for sharet able using the keys for table entries
*/
bool share_hash_compare (const struct hash_elem *, const struct hash_elem *, void *);


/* 
  Used within hash_destroy to free elements of the share table
*/
void destroy_elem (struct hash_elem *e, void *aux);

#endif