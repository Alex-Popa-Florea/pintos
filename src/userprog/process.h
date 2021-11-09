#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#define PROCESS_UNTOUCHED_STATUS (-10)

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
    pid_t process_id;
    tid_t thread_id; // Id of the thread corresponding to the process
    pid_t parent_id; // A parent_id of -1 means a process has no parent
    struct list_elem elem;
    struct list children;
    int exit_status;
    struct semaphore sema; // Semaphore used to block the thread of the corresponding process
} pcb;

/*
    Struct wraps pid_t so a list of process ids can be created
*/
typedef struct {
    pid_t process_id;
    struct list_elem elem;
} process_id_elem;


/*
    Initialises a PCB with a process having no children
*/
void init_pcb (pcb *, pid_t, pid_t);

/*
    Returns true if the process contains the child in its list of children
*/
bool process_has_child (struct list *, pid_t);

/*
    Returns the PCB corresponding to a thread, NULL if there is no match
*/
pcb *get_pcb_from_thread (tid_t);


#endif /* userprog/process.h */
