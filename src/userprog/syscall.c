#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include <string.h>
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
#include "threads/malloc.h"
#include "lib/user/syscall.h"

static void syscall_handler (struct intr_frame *);
struct lock file_system_lock; // Lock to ensure only one process can access file system at once

void
syscall_init (void) 
{
  lock_init (&file_system_lock);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f) 
{
  //printf ("I called the syscall handler\n");
  int *addr = f->esp;
  verify_address (addr);
  int system_call = *addr;
  //printf ("Call: %d\n", system_call);

  switch (system_call)
  {
    case SYS_HALT:
      //printf ("SYS_HALT\n");
      halt();
      break;
    
    case SYS_EXIT:
      //printf ("SYS_EXIT\n");
      verify_arguments (addr, 1);
      exit ((int) *(addr + 1));
      break;

    case SYS_EXEC:
      //printf ("SYS_EXEC\n");
      verify_arguments (addr, 1);
      f->eax = exec ((const char *) *(addr + 1));
      break;

    case SYS_WAIT:
      //printf ("SYS_WAIT\n");
      verify_arguments (addr, 1);
      f->eax = wait ((pid_t) *(addr + 1));
      break;

    case SYS_CREATE:
      //printf ("SYS_CREATE\n");
      verify_arguments (addr, 2);
      f->eax = create ((const char *) *(addr + 1), (unsigned) *(addr + 2));
      break;

    case SYS_REMOVE:
      //printf ("SYS_REMOVE\n");
      verify_arguments (addr, 1);
      f->eax = remove ((const char *) *(addr + 1));
      break;

    case SYS_OPEN:
      //printf ("SYS_OPEN\n");
      verify_arguments (addr, 1);
      f->eax = open ((const char *) *(addr + 1));
      break;

    case SYS_FILESIZE:
      //printf ("SYS_FILESIZE\n");
      verify_arguments (addr, 1);
      f->eax = filesize ((int) *(addr + 1));
      break;

    case SYS_READ:
      //printf ("SYS_READ\n");
      verify_arguments (addr, 3);
      f->eax = read ((int) *(addr + 1), (void *) *(addr + 2), (unsigned) *(addr + 3));
      break;

    case SYS_WRITE:
      //printf ("SYS_WRITE\n");
      //verify_address (addr + 7);
      //verify_address
      verify_arguments (addr, 3);
      f->eax = write ((int) *(addr + 1), (const void *) *(addr + 2), (unsigned) *(addr + 3));
      break;

    case SYS_SEEK:
      //printf ("SYS_SEEK\n");
      verify_arguments (addr, 2);
      seek ((int) *(addr + 1), (unsigned) *(addr + 2));
      break;

    case SYS_TELL:
      //printf ("SYS_TELL\n");
      verify_arguments (addr, 1);
      f->eax = tell ((int) *(addr + 1));
      break;

    case SYS_CLOSE:
      //printf ("SYS_CLOSE\n");
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
  //printf ("I called exit with %d\n", status);
  set_process_status (thread_current (), status);
  print_termination_output ();
  thread_exit ();
}

pid_t 
exec (const char *cmd_line) {
  verify_address (cmd_line);
  //printf ("I called exec with %s\n", cmd_line);
  if (!cmd_line) {
    return -1;
  } 
  
  lock_acquire (&file_system_lock);

  // Extract the name of the file from the command
  int command_size = strlen(cmd_line) + 1;
  char str[command_size];
  strlcpy (str, cmd_line, command_size);
  char *strPointer;
  char *file_name = strtok_r(str, " ", &strPointer);

  if (!is_filename_valid (file_name)) {
    lock_release (&file_system_lock);
    return -1;
  }

  pid_t new_process_pid = process_execute (cmd_line);

  lock_release (&file_system_lock);

  return new_process_pid;

}

int wait (pid_t pid) {
  //printf ("I called wait\n");
  return process_wait (pid);
}

bool 
create (const char *file, unsigned initial_size) {
  verify_address (file);
  //printf ("I called create with %s, %d\n", file, initial_size);
  lock_acquire (&file_system_lock);
  bool success = filesys_create (file, initial_size);
  lock_release (&file_system_lock);
  return success;
}

bool
remove (const char *file) {
  verify_address (file);
  //printf ("I called remove with %s\n", file);
  lock_acquire (&file_system_lock);
  bool success = filesys_remove (file);
  lock_release (&file_system_lock);
  return success;
}

int 
open (const char *file) {
  verify_address (file);
  //printf ("I called open with %s\n", file);
  lock_acquire (&file_system_lock);

  struct file *new_file = filesys_open (file);
  if (new_file == NULL) {
    lock_release (&file_system_lock);
    return -1;
  }


  // Initialise a process file mapping the file pointer to the file descriptor
  process_file *process_file = malloc (sizeof (process_file));   // ?? is malloc needed or is this file ??
  process_file->file = new_file;
  int file_descriptor = thread_current ()->current_file_descriptor;
  process_file->file_descriptor = file_descriptor;

  increment_current_file_descriptor (thread_current ());
  list_push_front (&thread_current ()->file_list, &process_file->file_elem);

  lock_release (&file_system_lock);

  return file_descriptor;
}

int 
filesize (int fd) {
  //printf ("I called filesize with %d\n", fd);
  lock_acquire (&file_system_lock);

  int file_size = -1;

  process_file *process_file = file_finder (fd);
  if (process_file) {
    file_size = file_length (process_file->file);
  }

  lock_release (&file_system_lock);

  return file_size;
}

int 
read (int fd, void *buffer, unsigned size) {
  verify_address (buffer);
  //printf ("I called read with %d, %s, %d\n", fd, buffer, size);
  if (fd == 0) {
    return input_getc ();
  }

  lock_acquire (&file_system_lock);

  int bytes_read = -1;

  process_file *process_file = file_finder (fd);
  if (process_file) {
    bytes_read = file_read (process_file->file, buffer, size);
  }

  lock_release (&file_system_lock);

  return bytes_read;
}

int 
write (int fd, const void *buffer, unsigned size) {
  verify_address (buffer);
  //printf ("I called write with %d, %s, %d\n", fd, buffer, size);
  if (fd == 0) {
    return 0;
  }

  if (fd == 1) {
    putbuf (buffer, size);    
    return size;
  }

  lock_acquire (&file_system_lock);

  int bytes_written = 0;

  process_file *process_file = file_finder (fd);
  if (process_file) {
    bytes_written = file_write (process_file->file, buffer, size);
  }

  lock_release (&file_system_lock);

  return bytes_written;
}

void 
seek (int fd, unsigned position) {
  //printf ("I called read with %d, %d\n", fd, position);
  lock_acquire (&file_system_lock);

  process_file *process_file = file_finder (fd);
  if (process_file) {
    file_seek (process_file->file, position);
  }

  lock_release (&file_system_lock);

}

unsigned 
tell (int fd) {
  //printf ("I called tell with %d\n", fd);
  lock_acquire (&file_system_lock);

  int position = 0;

  process_file *process_file = file_finder (fd);
  if (process_file) {
    position = file_tell (process_file->file);
  }

  lock_release (&file_system_lock);

  return position;
}

void 
close (int fd) {
  //printf ("I called close with %d\n", fd);
  lock_acquire (&file_system_lock);

  process_file *process_file = file_finder (fd);
  if (process_file) {
    file_close (process_file->file);
    //file_allow_write (process_file->file);
    list_remove (&process_file->file_elem);
  }
  free (process_file);
  lock_release (&file_system_lock);

  return;
}

process_file *
file_finder (int fd) {

  struct list *file_list = &thread_current ()->file_list;
  
  if (list_empty (file_list)) {
    return NULL;
  }

  struct list_elem *e;
  for (e = list_begin (file_list); e != list_end (file_list); e = list_next (e)) {
    process_file *current_file = list_entry (e, process_file, file_elem);
    if (current_file->file_descriptor == fd) {
      return current_file;
    }
  }
  return NULL;
}

// for (e = list_begin (&pcb_list); e != list_end (&pcb_list); e = list_next (e)) {
//     pcb *current_pcb = list_entry (e, pcb, elem);
//     if (current_pcb->id == tid) {
//       return current_pcb;
//     }
//   }

void 
verify_address (const void *vaddr) {
  if (!is_user_vaddr (vaddr)) {
    //printf ("Vaddr failed\n");
    exit (-1);
  }
  if (!pagedir_get_page(thread_current ()->pagedir, vaddr)) {
    //printf ("Pagedir failed\n");
    exit (-1);
  }
  //printf ("Passed argument verification\n");
}

void verify_arguments (int *addr, int num_of_args) {
  //printf ("I am verifying the arguments\n");
  for (int i = 1; i < num_of_args + 1; i++) {
    //printf ("Argument %d\n", i);
    verify_address ((const void *) addr + i);
  }
}

void 
print_termination_output (void) {
  printf ("%s: exit(%d)\n", thread_current ()->name, thread_current ()->process_status);
}


bool
is_filename_valid (const char *filename) {
  return filesys_open (filename) != NULL;
}
