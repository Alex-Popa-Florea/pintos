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

struct lock file_system_lock; // Lock to ensure only one process can access file system at once

static syscall_func_info syscall_arr[NUM_SYSCALLS];

static void syscall_arr_setup(void);
static void syscall_handler (struct intr_frame *);

/* Wrapper functions for all system call functions that require arguments */
static void halt_wrapper (void);
static void exit_wrapper (int *);
static void exec_wrapper (uint32_t *, int *);
static void wait_wrapper (uint32_t *, int *);
static void create_wrapper (uint32_t *, int *);
static void remove_wrapper (uint32_t *, int *);
static void open_wrapper (uint32_t *, int *);
static void filesize_wrapper (uint32_t *, int *);
static void read_wrapper (uint32_t *, int *);
static void write_wrapper (uint32_t *, int *);
static void seek_wrapper (int *);
static void tell_wrapper (uint32_t *, int *);
static void close_wrapper (int *);

void
syscall_init (void) 
{
  lock_init (&file_system_lock);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  syscall_arr_setup ();
}

/*
  Calls the function corresponding to the system call calling the handler
*/
static void
syscall_handler (struct intr_frame *f) 
{
  //printf ("I called the syscall handler\n");
  int *addr = f->esp;
  verify_address (addr);
  int system_call = *addr;

  syscall_func_info syscall = syscall_arr[system_call];
  syscall_func func = syscall.func;
  int args = syscall.num_args;
  bool has_return = syscall.has_return;

  if (args > 0) {
    verify_arguments (addr, args);
    if (has_return) {
      func (&(f->eax), addr);
    } else {
      func (addr);
    }
  } else {
    if (has_return) {
      func (&(f->eax));
    } else {
      func ();
    }
  }
}

static void 
halt_wrapper (void) {
  halt ();
}

void
halt (void) {
  shutdown_power_off ();
}

static void 
exit_wrapper (int *addr) {
  exit ((int) *(addr + 1));
}

void 
exit (int status) {
  //printf ("I called exit with %d\n", status);

  // Check the argument does not exceed the maximum user address
  if (status > PHYS_BASE) {
    set_process_status (thread_current (), -1);
  } else {
    set_process_status (thread_current (), status);
  }

  print_termination_output ();
  thread_exit ();
}

static void 
exec_wrapper (uint32_t *eax , int *addr) {
  *eax = exec ((const char *) *(addr + 1));
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

static void 
wait_wrapper (uint32_t *eax, int *addr) {
  *eax = wait ((pid_t) *(addr + 1));
}

int 
wait (pid_t pid) {
  //printf ("I called wait\n");
  return process_wait (pid);
}

static void 
create_wrapper (uint32_t *eax, int *addr) {
  *eax = create ((const char *) *(addr + 1), (unsigned) *(addr + 2));
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

static void
remove_wrapper (uint32_t *eax, int *addr) {
  *eax = remove ((const char *) *(addr + 1));
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

static void 
open_wrapper (uint32_t *eax, int *addr) {
  *eax = open ((const char *) *(addr + 1));
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

static void 
filesize_wrapper (uint32_t *eax, int *addr) {
  *eax = filesize ((int) *(addr + 1));
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

static void
read_wrapper (uint32_t *eax, int *addr) {
  *eax = read ((int) *(addr + 1), (void *) *(addr + 2), (unsigned) *(addr + 3));
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

static void
write_wrapper (uint32_t *eax, int *addr) {
  *eax = write ((int) *(addr + 1), (const void *) *(addr + 2), (unsigned) *(addr + 3));
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

static void
seek_wrapper (int *addr) {
  seek ((int) *(addr + 1), (unsigned) *(addr + 2));
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

static void
tell_wrapper (uint32_t *eax, int *addr) {
  *eax = tell ((int) *(addr + 1));
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

static void
close_wrapper (int *addr) {
  close ((int) *(addr + 1));
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

/*
  Initialise syscall_arr to store information about each system call function
*/
static void syscall_arr_setup(void) {

  for (int i = SYS_HALT; i <= SYS_INUMBER; i++) {
    syscall_func_info info = {0};
    switch (i)
    {
      case SYS_HALT:
        info.num_args = 0;
        info.func = &halt_wrapper;
        info.has_return = false;
        break;
      
      case SYS_EXIT:
        info.num_args = 1;
        info.func = &exit_wrapper;
        info.has_return = false;
        break;

      case SYS_EXEC:
        info.num_args = 1;
        info.func = &exec_wrapper;
        info.has_return = true;
        break;

      case SYS_WAIT:
        info.num_args = 1;
        info.func = &wait_wrapper;
        info.has_return = true;
        break;

      case SYS_CREATE:
        info.num_args = 2;
        info.func = &create_wrapper;
        info.has_return = true;
        break;

      case SYS_REMOVE:
        info.num_args = 1;
        info.func = &remove_wrapper;
        info.has_return = true;
        break;

      case SYS_OPEN:
        info.num_args = 1;
        info.func = &open_wrapper;
        info.has_return = true;
        break;

      case SYS_FILESIZE:
        info.num_args = 1;
        info.func = &filesize_wrapper;
        info.has_return = true;
        break;

      case SYS_READ:
        info.num_args = 3;
        info.func = &read_wrapper;
        info.has_return = true;
        break;

      case SYS_WRITE:
        info.num_args = 3;
        info.func = &write_wrapper;
        info.has_return = true;
        break;

      case SYS_SEEK:
        info.num_args = 2;
        info.func = &seek_wrapper;
        info.has_return = false;
        break;

      case SYS_TELL:
        info.num_args = 1;
        info.func = &tell_wrapper;
        info.has_return = true;
        break;

      case SYS_CLOSE:
        info.num_args = 1;
        info.func = &close_wrapper;
        info.has_return = false;
        break;
      
      default:
        break;
    }
    syscall_arr[i] = info;
  }
}