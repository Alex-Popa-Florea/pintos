#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#define PROCESS_UNTOUCHED_STATUS (-10)
#define CHILDLESS_PARENT_ID (-50)

#include "lib/user/syscall.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include "vm/supp-page-table.h"

extern struct list pcb_list;
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
bool process_has_child (pcb *parent, pid_t child_id);

/*
  Returns a pointer to the pcb corresponding to an id, NULL if there is no match
  Assumes interrupts are disabled so access is thread-safe
*/
pcb *get_pcb_from_id (tid_t);

/*
  Set the exit status of a pcb to the exit status provided
*/
void set_exit_status (pcb *, int);

/*
  Loads a page from Supplemental Page Table of process into memory
*/
bool load_page (uint8_t *, supp_page_table_entry *);

#endif /* userprog/process.h */
