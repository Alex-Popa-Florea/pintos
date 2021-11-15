#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#define PROCESS_UNTOUCHED_STATUS (-10)
#define CHILDLESS_PARENT_ID (-50)
#define NOT_WAITING (-100)

#include "lib/user/syscall.h"
#include "threads/thread.h"
#include "threads/synch.h"

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);
void verify_address (const void *vaddr);


/*
    Struct for a process control block, storing a process and relevant info
*/
typedef struct {   
    int id;                         // Id of the thread which equals the id of the process
    int parent_id;                  // A parent_id of -1 means a process has no parent
    int waiting_on;                 // Id of the child process that a parent process waits on
    struct list_elem elem;          // Elem to insert pcb into a list
    struct list_elem child_elem;    // Elem to insert pcb into child list
    struct list children;           // Stores list of children pcbs
    int exit_status;                // Status of the process
    struct semaphore sema;          // Semaphore used to block the thread of the corresponding process
    bool has_been_waited_on;                 // Boolean to track if a wait has already been called on the process
} pcb;

/*
    Initialises a PCB with its id and parent id
*/
void init_pcb (pcb *, int, int);

/*
    Returns true if the process contains the child in its list of children
*/
bool process_has_child (struct list *, pid_t);

/*
    Sets the id of the child process a parent waits on
*/
void pcb_set_waiting_on (pcb *, int);

/*
    Returns the PCB corresponding to an id, NULL if there is no match
*/
pcb *get_pcb_from_id (tid_t);

void print_pcb_ids (void);


#endif /* userprog/process.h */
