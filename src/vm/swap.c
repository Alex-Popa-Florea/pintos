#include "swap.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include <debug.h>
#include <stdio.h>

struct lock swap_table_lock;
struct lock bitmap_lock;
struct hash swap_table;

static unsigned swap_hash (const struct hash_elem *, void *);
static bool swap_hash_compare (const struct hash_elem *, const struct hash_elem *, void *);

void initialise_swap_space (void) {
    /* Initialise bitmap */
    sector_bitmap = bitmap_create (NUM_SWAP_BLOCK_SECTORS);

    /* Initialise hash table */
    bool hash_created = hash_init (&swap_table, swap_hash, swap_hash_compare, NULL);
    ASSERT (hash_created);

    /* Initialise swap table and bitmap lock */
    lock_init (&swap_table_lock);
    lock_init (&bitmap_lock);
}

bool load_page_into_swap_space (void *page) {
    lock_acquire (&bitmap_lock);
    lock_acquire (&swap_table_lock);

    /*
        Finds first contiguous sectors of BLOCK_SWAP to store whole page
    */

    size_t index = bitmap_scan_and_flip (sector_bitmap, FIRST_SECTOR, SECTORS_PER_PAGE, false);
    if (index == BITMAP_ERROR) {
        lock_release (&swap_table_lock);
        lock_release (&bitmap_lock);
        return false;
    }
    /* Insert first sector into swap table */
    swap_entry *new_entry = (swap_entry *) malloc (sizeof (swap_entry));
    new_entry->page = page;
    new_entry->index = index;
    hash_insert (&swap_table, &new_entry->elem);

    /* 
        Writes each sector of page to a sector of BLOCK_SWAP 
    */
    struct block *swap_block = block_get_role (BLOCK_SWAP);
    void *start_addr = page;
    for (int i = 0; i < SECTORS_PER_PAGE; i++) {
        start_addr += i * BLOCK_SECTOR_SIZE;
        block_write (swap_block, index, start_addr);
        index++;
    }

    lock_release (&swap_table_lock);
    lock_release (&bitmap_lock);
    return true;
}

void retrieve_from_swap_space (void *page, uint8_t *addr) {
    lock_acquire (&swap_table_lock);

    void *start_addr = page;
    struct block *swap_block = block_get_role (BLOCK_SWAP);
    swap_entry entry;
    entry.page = addr;

    /* 
        Find index of first sector of page from swap table 
    */
    struct hash_elem *found_elem = hash_find (&swap_table, &entry.elem);
    size_t index = hash_entry (found_elem, swap_entry, elem)->index;

    /* 
        Reads each sector of page from BLOCK_SWAP into page buffer
        and resets the corresponding bit in sector_bitmap
    */
    for (int i = 0; i < SECTORS_PER_PAGE; i++) {
        start_addr += i * BLOCK_SECTOR_SIZE;
        
        block_read (swap_block, index, start_addr);
        bitmap_flip (sector_bitmap, index);

        index++;
    }
    struct hash_elem *deleted = hash_delete (&swap_table, found_elem);
    free (hash_entry (deleted, swap_entry, elem));

    lock_release (&swap_table_lock);
}

/* 
  Hash function for Swap Table 
*/
static unsigned 
swap_hash (const struct hash_elem *e, void *aux UNUSED) {
  const swap_entry *entry = hash_entry (e, swap_entry, elem);
  return hash_bytes (&entry->page, sizeof entry->page);
}

/* 
  Comparison function for Swap Page Table
    - compares the base addresses (indices) of two hash table entries
*/
static bool 
swap_hash_compare (const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED) {
  const swap_entry *entryA = hash_entry (a, swap_entry, elem);
  const swap_entry *entryB = hash_entry (b, swap_entry, elem);

  return entryA->page < entryB->page;
}


