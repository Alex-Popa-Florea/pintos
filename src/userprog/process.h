#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#define PROCESS_UNTOUCHED_STATUS (-10)
#define CHILDLESS_PARENT_ID (-50)

#include "lib/user/syscall.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include "vm/supp-page-table.h"

/*
  Global list of pcbs
*/
extern struct list pcb_list;

/*
  Global lock to ensure synchronized access to the pcb_list
*/
extern struct lock pcb_list_lock;

/* 
  Starts a new thread running a user program loaded from
  FILENAME.  The new thread may be scheduled (and may even exit)
  before process_execute() returns.  Returns the new process's
  thread id, or TID_ERROR if the thread cannot be created. 
*/
tid_t process_execute (const char *file_name);

/* 
  Waits for thread TID to die and returns its exit status. 
  If it was terminated by the kernel (i.e. killed due to an exception), 
  returns -1.  
  If TID is invalid or if it was not a child of the calling process, or if 
  process_wait() has already been successfully called for the given TID, 
  returns -1 immediately, without waiting.
*/
int process_wait (tid_t);

/* Frees the current process's resources. */
void process_exit (void);

/* Sets up the CPU for running user code in the current
   thread.
   This function is called on every context switch. */
void process_activate (void);

/*
    Struct for a process control block, storing a process and relevant info
*/
typedef struct {   
    int id;                         /* Id of the thread which equals the id of the process */
    int parent_id;                  /* A parent_id of -1 means a process has no parent */
    struct list_elem elem;          /* Elem to insert pcb into a list */
    int exit_status;                /* Status of the process */

    bool has_been_waited_on;        /* Boolean to track if a wait has already been called on the process */
    struct semaphore wait_sema;     /* Semaphore used to block a process when it waits on its child process */
    bool load_process_success;      /* Flag to indicate success of loading a process from an executable */
    struct semaphore load_sema;     /* Semaphore to block a process while it loads a child */
} pcb;

/*
    Initialises a PCB with its id and parent id
*/
void init_pcb (pcb *, int, int);


/*
  Returns true if the process has another process as its child
*/
bool process_has_child (pcb *, pid_t);


/*
  Returns a pointer to the pcb corresponding to an id, NULL if there is no match
  Assumes interrupts are disabled so access is thread-safe
*/
pcb *get_pcb_from_id (tid_t);


/*
  Set the exit status of a pcb to the exit status provided
*/
void set_exit_status (pcb *, int);


/* Adds a mapping from user virtual address UPAGE to kernel
   virtual address KPAGE to the page table.
   If WRITABLE is true, the user process may modify the page;
   otherwise, it is read-only.
   UPAGE must not already be mapped.
   KPAGE should probably be a page obtained from the user pool
   with palloc_get_page().
   Returns true on success, false if UPAGE is already mapped or
   if memory allocation fails.
*/
bool install_page (void *upage, void *kpage, bool writable);


/*
  Create a supplemental page table entry for a stack page
*/
struct hash_elem *set_up_pte_for_stack (void *);


#endif /* userprog/process.h */
