#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include <list.h>
#include "lib/user/syscall.h"
#include "vm/supp-page-table.h"

/* Current number of system call functions recognised in Pintos */
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

/*
    Struct to map supplemental page table entries for memory mapped files to mapids
*/
typedef struct
{
    supp_pte *entry;                     /* Supplemental Page Table entry corresponding to the mapping */
    mapid_t mapping;                     /* ID of the mapping */
    struct list_elem mapped_elem;        /* List elem - each thread contains a list of memory mapped files */
} mapped_file;

/*
    Generic type for system call functions
*/
typedef void (*syscall_func)();

/*
    Struct to store information about each system call function
*/
typedef struct {
    int num_args;             /* Number of arguments required by func */
    syscall_func func;        /* System call function func */
    bool has_return;          /* Records whether func returns a value or not */
} syscall_func_info;

/*
    Global file system lock - allows multiple files to access same lock
*/
extern struct lock file_system_lock; 

/*
    Initialises syscall system
*/
void syscall_init (void);

/* 
  System call that terminates Pintos 
*/ 
void halt (void);

/* 
  System call that terminates curret user program 
*/ 
void exit (int status);

/* 
  System call that runs the executable given in cmd_line. 
  Returns new process's pid.
*/ 
pid_t exec (const char *cmd_line);

/* 
  System call that waits for a child process pid. 
  Returns the child’s exit status.
*/ 
int wait (pid_t pid);

/* 
  System call that creates a new file of intial size initial_size.
  Returns true if successful, false otherwise
*/
bool create (const char *file, unsigned initial_size);

/* 
  System call that deletes given file. 
  Returns true if successful, false otherwise.
*/
bool remove (const char *file);

/* 
  System call that opens the given file.
  Returns the file's file descriptor if successful, otherwise -1 
*/
int open (const char *file);

/* 
  System call that returns the size (bytes) of file open as fd
*/
int filesize (int fd);

/* 
  System call that reads size bytes from file open as fd into buffer.
  Returns number of bytes read, or -1 if unsuccessful.
*/
int read (int fd, void *buffer, unsigned size);

/* 
  System call that writes size bytes from buffer into file open as fd.
  Returns number of bytes written, or 0 if unsuccessful.
*/
int write (int fd, const void *buffer, unsigned size);

/* 
  System call that changes next byte to be read/ written 
  in open file fd to position (bytes) from beginning of file
*/
void seek (int fd, unsigned position);

/* 
  System call that returns the position (bytes) from beginning of file 
  of next byte to be read/ written in open file fd 
*/
unsigned tell (int fd);

/* 
  System call that closes file descriptor fd 
*/
void close (int fd);

/* 
  System call that mapps opened file in memory at address addr.
*/
mapid_t mmap (int fd, void *addr);

/* 
  System call that unmapps file of mapid mapping from memory.
*/
void munmap (mapid_t mapping);

#endif /* userprog/syscall.h */
