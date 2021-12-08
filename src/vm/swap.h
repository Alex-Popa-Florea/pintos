#ifndef SWAP
#define SWAP

#include <stdbool.h>
#include <inttypes.h>
#include <stddef.h>
#include "lib/kernel/bitmap.h"
#include "lib/kernel/hash.h"
#include "threads/vaddr.h"
#include "devices/block.h"
#include "vm/supp-page-table.h"

/* Calculates number of sectors in BLOCK_SWAP */
#define NUM_SWAP_BLOCK_SECTORS (block_size (block_get_role (BLOCK_SWAP)))

/* Calculates the number of sectors needed to store a page */
#define SECTORS_PER_PAGE (PGSIZE / BLOCK_SECTOR_SIZE)

/* Index of first sector of a block */
#define FIRST_SECTOR (0)

/* Bitmap to record occupancy of swap spaces */
struct bitmap *sector_bitmap;

/* Swap Table */
extern struct hash swap_table;

/*
  Struct for an entry in swap table
*/
typedef struct {
	supp_pte *supp_pte;           /* Supplemental Page table entry */
	size_t index;                 /* Index of first block sector in device */

  struct hash_elem elem;          /* Hash table elem */
} swap_entry;

/*
  Lock to ensure synchronized access to the swap table
*/
extern struct lock swap_table_lock;

/*
  Lock to ensure synchronized access to the bitmap
*/
extern struct lock bitmap_lock;

/*
	Initialise Swap Table hash table and bitmap to represent occupied sectors
*/
void initialise_swap_space (void);

/*
	Writes contents of PAGE to first available contiguous SECTORS_PER_PAGE 
	sectors in BLOCK_SWAP
*/
bool load_page_into_swap_space (supp_pte *, void *);

/* 
	Populates EMPTY_PAGE with data from SUPP_ENTRY from swap space
*/
void retrieve_from_swap_space (supp_pte *, void *);

#endif