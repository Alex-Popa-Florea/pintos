#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "process.h"

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

void 
print_termination_output (void) {
  printf ("%s:  exit(%d)\n", thread_current ()->name, thread_current ()->process_status);
}