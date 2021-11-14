#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "lib/user/syscall.h"
#include <list.h>

#define NUM_SYSCALLS (20)

/*
    Struct to map file pointers to file descriptors
*/
typedef struct
{
    struct file *file;
    int file_descriptor;
    struct list_elem file_elem;
} process_file;


typedef void (*syscall_func)();     // Generic type for system call functions

/*
    Struct to store information about each system call function
*/
typedef struct {
    int num_args;             // Number of arguments required by func
    syscall_func func;        // System call function func
    bool has_return;          // Records whether func returns a value or not
} syscall_func_info;


void syscall_init (void);

void halt (void);
void exit (int status);
pid_t exec (const char *cmd_line);
int wait (pid_t pid);
bool create (const char *file, unsigned initial_size);
bool remove (const char *file);
int open (const char *file);
int filesize (int fd);
int read (int fd, void *buffer, unsigned size);
int write (int fd, const void *buffer, unsigned size);
void seek (int fd, unsigned position);
unsigned tell (int fd);
void close (int fd);

process_file *file_finder (int fd);
void verify_address (const void *vaddr);
void verify_arguments (int *addr, int num_of_args);
void print_termination_output (void);
bool is_filename_valid (const char *);
#endif /* userprog/syscall.h */
