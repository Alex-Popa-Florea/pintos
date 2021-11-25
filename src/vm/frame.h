#ifndef VM_FRAME_H
#define VM_FRAME_H

/*
    Type for the number which identifies an allocated page
*/
typedef int frame_nr;


/*
    Struct to store an entry in the frame table 
*/
typedef struct {
    void *page;
    struct thread *thread;
    struct list_elem elem;
} frame_table_entry;


/*
    Global list to track the pages which are occupied
*/
struct list frame_table;


/*
    Lock to ensure synchronized access to the page table
*/
struct lock frame_table_lock;


/*
    Initialises a frame table
*/
void init_frame_table (void)


/*
    Tries to allocate a page of memory using the flags provided
    Returns the pointer to page if successful, otherwise NULL
*/
void *try_allocate_page (enum palloc_flags);


/*
    Frees the memory occupied by a frame table entry
*/
void free_frame_table_entry (frame_table_entry *);


/*
    Frees the memory for all pages associated with a thread
*/
void free_frame_table_entries_of_thread (struct thread *);


/*
    Retrieves the number of a page from a frame table entry
*/
frame_nr get_frame_nr (frame_table_entry *);

#endif 