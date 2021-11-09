#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "lib/user/syscall.h"

void syscall_init (void);

void exit (int status);
int write (int fd, const void *buffer, unsigned size);
int wait (pid_t pid);

void print_termination_output (void);
#endif /* userprog/syscall.h */
