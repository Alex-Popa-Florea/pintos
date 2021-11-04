#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

void syscall_init (void);

void exit (int status);
int write (int fd, const void *buffer, unsigned size);

void print_termination_output (void);
#endif /* userprog/syscall.h */
