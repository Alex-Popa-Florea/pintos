#ifndef VM_FRAME_H
#define VM_FRAME_H

typedef int frame_nr;

typedef struct {
    void *page;
    struct thread *thread;
    struct list_elem elem;
} frame_table_entry;

struct hash_table frame_table;
struct lock frame_table_lock;

void *try_allocate_frame (enum palloc_flags flags);

void free_frame_table_entry (frame_table_entry *frame_table_entry);

void free_frame_table_entries_of_thread (struct thread *t);

frame_nr get_frame_nr (frame_table_entry *frame_entry);

#endif 