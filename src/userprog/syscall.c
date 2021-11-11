#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "lib/kernel/console.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "process.h"
#include "devices/shutdown.h"

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  int *addr = f->esp;
  verify_address (addr);
  int system_call = *addr;
  printf ("Call: %d\n", system_call);

  if (system_call == SYS_EXIT) {
    exit (0);
  } else if (system_call == SYS_WRITE){
    //write (1, )
  } else {
    print_termination_output ();
    thread_exit ();
  }
  
}

void 
exit (int status) {
  set_process_status (thread_current (), status);
  print_termination_output ();
  thread_exit ();
}

int 
write (int fd, const void *buffer, unsigned size) {
  if (fd == 1) {
    int split_size = size;
    while (split_size > 500) {
      putbuf (buffer, 500);
      split_size -= 500;
    }
    putbuf (buffer, split_size);    
  }
  return size;
}

int wait (pid_t pid) {
  return process_wait (pid);
}

void halt (void) {
  shutdown_power_off ();
}

void 
print_termination_output (void) {
  printf ("%s:  exit(%d)\n", thread_current ()->name, thread_current ()->process_status);
}