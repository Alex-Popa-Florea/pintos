#ifndef SUPP_PAGE_TABLE
#define SUPP_PAGE_TABLE

#include "lib/kernel/hash.h"
#include "filesys/off_t.h"
#include "filesys/file.h"
#include "vm/frame.h"

/* Enum to represent source of page */
enum source {
  MMAP,                          /* Memory mapped file */   
  STACK,                         /* Stack page */  
  SWAP_SPACE,                    /* Swap space */
  DISK                           /* File system */
};

/*
  Struct for an entry in a supplemental page table, a hash table
*/
typedef struct {
  uint8_t *addr;                      /* Base address of the page (key) */
  struct file *file;                  /* File storing the relevent data for process */
  off_t ofs;                          /* Offset in the file for current page */ 
  uint32_t read_bytes;                /* No. of bytes that must be read to initialise page */
  uint32_t zero_bytes;                /* Bytes to be zeroed in virtual memory */
  bool writable;                      /* Records if page should be writable or read-only */
  bool is_dirty;                      /* Records if a page has been modified, and hence needs writing to disk */
  enum source page_source;            /* Records the source of the page */
  frame_table_entry *page_frame;      /* Pointer to page frame if page is loaded to frame table, null otherwise */
   
  struct hash_elem elem;              /* Hash table elem */
} supp_pte;


/* 
  Hash function for Supplemental Page Table 
*/
unsigned supp_hash (const struct hash_elem *, void *);

/* 
  Comparison function for Supplemental Page Table
    - compares the base addresses (keys) of two hash table entries
*/
bool supp_hash_compare (const struct hash_elem *, const struct hash_elem *,void *);


/* 
  Used within hash_destroy to free elements of a supp_page_table, taking in
  an auxiliary parameter
*/
void supp_destroy (struct hash_elem *e, void *);


/*
  Create a supplemental page table entry with the provided parameters
*/
supp_pte *create_supp_pte (struct file *, off_t, uint8_t *,
                           uint32_t, uint32_t, bool, enum source);

#endif