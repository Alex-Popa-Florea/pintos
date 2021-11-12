#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "lib/user/syscall.h"
#include <list.h>

struct process_file
{
    struct file *file;
    int file_descriptor;

    struct list_elem file_elem;
};

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

struct process_file *file_finder (int fd);
void verify_address (const void *vaddr);
void print_termination_output (void);
#endif /* userprog/syscall.h */
