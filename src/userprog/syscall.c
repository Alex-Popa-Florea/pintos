#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "lib/kernel/console.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "process.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "devices/input.h"
#include "devices/shutdown.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "threads/synch.h"
#include "lib/user/syscall.h"

static void syscall_handler (struct intr_frame *);
struct lock file_system_lock;

void
syscall_init (void) 
{
  lock_init (&file_system_lock);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f) 
{
  int *addr = f->esp;
  verify_address (addr);
  int system_call = *addr;
  printf ("Call: %d\n", system_call);

  switch (system_call)
  {
  case SYS_HALT:
    halt();
    break;
  
  case SYS_EXIT:
    verify_arguments (addr, 1);
    exit ((int) *(addr + 1));
    break;

  case SYS_EXEC:
    verify_arguments (addr, 1);
    f->eax = exec ((const char *) *(addr + 1));
    break;

  case SYS_WAIT:
    verify_arguments (addr, 1);
    f->eax = wait ((pid_t) *(addr + 1));
    break;

  case SYS_CREATE:
    verify_arguments (addr, 2);
    f->eax = create ((const char *) *(addr + 1), (unsigned) *(addr + 2));
    break;

  case SYS_REMOVE:
    verify_arguments (addr, 1);
    f->eax = remove ((const char *) *(addr + 1));
    break;

  case SYS_OPEN:
    verify_arguments (addr, 1);
    f->eax = open ((const char *) *(addr + 1));
    break;

  case SYS_FILESIZE:
    verify_arguments (addr, 1);
    f->eax = filesize ((int) *(addr + 1));
    break;

  case SYS_READ:
    verify_arguments (addr, 3);
    f->eax = read ((int) *(addr + 1), (void *) *(addr + 2), (unsigned) *(addr + 3));
    break;

  case SYS_WRITE:
    verify_arguments (addr, 3);
    f->eax = write ((int) *(addr + 1), (const void *) *(addr + 2), (unsigned) *(addr + 3));
    break;

  case SYS_SEEK:
    verify_arguments (addr, 2);
    seek ((int) *(addr + 1), (unsigned) *(addr + 2));
    break;

  case SYS_TELL:
    verify_arguments (addr, 1);
    f->eax = tell ((int) *(addr + 1));
    break;

  case SYS_CLOSE:
    verify_arguments (addr, 1);
    close ((int) *(addr + 1));
    break;
  
  default:
    break;
  }
  
}

void
halt (void) {
  shutdown_power_off ();
}

void 
exit (int status) {
  set_process_status (thread_current (), status);
  print_termination_output ();
  thread_exit ();
}

pid_t 
exec (const char *cmd_line) {
  if (!cmd_line) {
    return -1;
  }
  lock_acquire (&file_system_lock);

  pid_t new_process_pid = process_execute (cmd_line);

  lock_release (&file_system_lock);

  return new_process_pid;

}

int wait (pid_t pid) {
  return process_wait (pid);
}

bool 
create (const char *file, unsigned initial_size) {

  lock_acquire (&file_system_lock);
  bool success = filesys_create (file, initial_size);
  lock_release (&file_system_lock);
  return success;
}

bool
remove (const char *file) {

  lock_acquire (&file_system_lock);

  bool success = filesys_remove (file);

  lock_release (&file_system_lock);

  return success;
}

int 
open (const char *file) {

  lock_acquire (&file_system_lock);

  struct file *new_file = filesys_open (file);

  if (new_file == NULL) {
    lock_release (&file_system_lock);
    return -1;
  }

  struct process_file process_file;   // ?? is malloc needed or is this file ??
  process_file.file = new_file;
  int file_descriptor = thread_current ()->current_file_descriptor;
  process_file.file_descriptor = file_descriptor;
  thread_current ()->current_file_descriptor++;
  list_push_front (&thread_current ()->file_list, &process_file.file_elem);

  lock_release (&file_system_lock); // ?? here or just after filesys_open (file) ??

  return file_descriptor;
}

int 
filesize (int fd) {

  lock_acquire (&file_system_lock);

  int file_size = -1;

  struct process_file *process_file = file_finder (fd);
  if (process_file) {
    file_size = file_length (process_file->file);
  }

  lock_release (&file_system_lock);

  return file_size;
}

int 
read (int fd, void *buffer, unsigned size) {

  if (fd == 0) {
    return input_getc ();
  }

  lock_acquire (&file_system_lock);

  int bytes_read = -1;

  struct process_file *process_file = file_finder (fd);
  if (process_file) {
    bytes_read = file_read (process_file->file, buffer, size);
  }

  lock_release (&file_system_lock);

  return bytes_read;
}

int 
write (int fd, const void *buffer, unsigned size) {

  if (fd == 0) {
    return 0;
  }

  if (fd == 1) {
    putbuf (buffer, size);    
    return size;
  }

  lock_acquire (&file_system_lock);

  int bytes_written = 0;

  struct process_file *process_file = file_finder (fd);
  if (process_file) {
    bytes_written = file_write (process_file->file, buffer, size);
  }

  lock_release (&file_system_lock);

  return bytes_written;
}

void 
seek (int fd, unsigned position) {

  lock_acquire (&file_system_lock);

  struct process_file *process_file = file_finder (fd);
  if (process_file) {
    file_seek (process_file->file, position);
  }

  lock_release (&file_system_lock);

}

unsigned 
tell (int fd) {

  lock_acquire (&file_system_lock);

  int position = 0;

  struct process_file *process_file = file_finder (fd);
  if (process_file) {
    position = file_tell (process_file->file);
  }

  lock_release (&file_system_lock);

  return position;
}

void 
close (int fd) {

  lock_acquire (&file_system_lock);

  struct process_file *process_file = file_finder (fd);
  if (process_file) {
    file_close (process_file->file);
    list_remove (&process_file->file_elem);
  }

  lock_release (&file_system_lock);

  return;
}

struct process_file *
file_finder (int fd) {
  struct list_elem *element = list_front (&thread_current ()->file_list);

  while (element != list_tail (&thread_current ()->file_list)) {

    struct process_file *process_file = list_entry (element, struct process_file, file_elem);
    
    if (process_file->file_descriptor == fd) {
      return process_file;
    }
    
    element = element->next;
  }
  return NULL;
}

void 
verify_address (const void *vaddr) {
  if (!is_user_vaddr (vaddr)) {
    exit (-1);
  }
  if (!pagedir_get_page(thread_current ()->pagedir, vaddr)) {
    exit (-1);
  }
}

void verify_arguments (int *addr, int num_of_args) {
  for (int i = 1; i < num_of_args + 1; i++) {
    verify_address ((const void *) addr[i]);
  }
}

void 
print_termination_output (void) {
  printf ("%s:  exit(%d)\n", thread_current ()->name, thread_current ()->process_status);
}