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

/* List of all pcbs as defined in process.c */
extern struct list pcb_list;

/* Lock that ensures only one process can access file system at once */
struct lock file_system_lock; 

/* Array storing information about each system call function */
static syscall_func_info syscall_arr[NUM_SYSCALLS];

/* Helper functions */

static void syscall_handler (struct intr_frame *);

/* Wrapper functions for use in syscall_arr */
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

static process_file *find_file (int);
static void verify_address (const void *);
static void verify_arguments (int *, int);
static void print_termination_output (void);

static void syscall_arr_setup (void);

void
syscall_init (void) 
{
  lock_init (&file_system_lock);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  syscall_arr_setup ();
}

/*
  Executes the function corresponding to the system call calling the handler
*/
static void
syscall_handler (struct intr_frame *f) 
{
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

/* 
  Wrapper function to execute halt() system call 
*/
static void 
halt_wrapper (void) {
  halt ();
}
 

void
halt (void) {
  shutdown_power_off ();
}

/* 
  Wrapper function to execute exit() system call 
*/
static void 
exit_wrapper (int *addr) {
  exit ((int) *(addr + 1));
}

void 
exit (int status) {
  set_process_status (thread_current (), status);
  print_termination_output ();
  thread_exit ();
}

/* 
  Wrapper function to execute exec() system call 
*/
static void 
exec_wrapper (uint32_t *eax , int *addr) {
  *eax = exec ((const char *) *(addr + 1));
}

pid_t 
exec (const char *cmd_line) {
  verify_address (cmd_line);
  if (!cmd_line) {
    return -1;
  } 

  /* Block the current process until it knows the success of the child process load */
  pcb *current_pcb = get_pcb_from_id (thread_current ()->tid);
  pid_t new_process_pid = process_execute (cmd_line);

  sema_down (&current_pcb->load_sema);
  
  if (current_pcb->load_process_success) {
    return new_process_pid;
  } else {
    return -1;
  }

}

/* 
  Wrapper function to execute wait() system call 
*/
static void 
wait_wrapper (uint32_t *eax, int *addr) {
  *eax = wait ((pid_t) *(addr + 1));
}

int 
wait (pid_t pid) {
  return process_wait (pid);
}

/* 
  Wrapper function to execute create() system call 
*/
static void 
create_wrapper (uint32_t *eax, int *addr) {
  *eax = create ((const char *) *(addr + 1), (unsigned) *(addr + 2));
}

bool 
create (const char *file, unsigned initial_size) {
  verify_address (file);
  lock_acquire (&file_system_lock);
  bool success = filesys_create (file, initial_size);
  lock_release (&file_system_lock);
  return success;
}

/* 
  Wrapper function to execute remove() system call 
*/
static void
remove_wrapper (uint32_t *eax, int *addr) {
  *eax = remove ((const char *) *(addr + 1));
}

bool
remove (const char *file) {
  verify_address (file);
  lock_acquire (&file_system_lock);
  bool success = filesys_remove (file);
  lock_release (&file_system_lock);
  return success;
}

/* 
  Wrapper function to execute open() system call 
*/
static void 
open_wrapper (uint32_t *eax, int *addr) {
  *eax = open ((const char *) *(addr + 1));
}

int 
open (const char *file) {
  verify_address (file);
  lock_acquire (&file_system_lock);

  struct file *new_file = filesys_open (file);
  if (new_file == NULL) {
    lock_release (&file_system_lock);
    return -1;
  }


  /*
    Initialise a process file, mapping the file pointer to the file descriptor
  */
  process_file *new_process_file = malloc (sizeof (process_file));   
  new_process_file->file = new_file;
  int file_descriptor = thread_current ()->current_file_descriptor;
  new_process_file->file_descriptor = file_descriptor;

  increment_current_file_descriptor (thread_current ());
  list_push_front (&thread_current ()->file_list, &new_process_file->file_elem);

  lock_release (&file_system_lock);

  return file_descriptor;
}

/* 
  Wrapper function to execute filesize() system call 
*/
static void 
filesize_wrapper (uint32_t *eax, int *addr) {
  *eax = filesize ((int) *(addr + 1));
}

int 
filesize (int fd) {
  lock_acquire (&file_system_lock);

  int file_size = -1;

  process_file *process_file = find_file (fd);
  if (process_file) {
    file_size = file_length (process_file->file);
  }

  lock_release (&file_system_lock);

  return file_size;
}

/* 
  Wrapper function to execute read() system call 
*/
static void
read_wrapper (uint32_t *eax, int *addr) {
  *eax = read ((int) *(addr + 1), (void *) *(addr + 2), (unsigned) *(addr + 3));
}

int 
read (int fd, void *buffer, unsigned size) {
  verify_address (buffer);
  if (fd == 0) {
    return input_getc ();
  }

  lock_acquire (&file_system_lock);

  int bytes_read = -1;

  process_file *process_file = find_file (fd);
  if (process_file) {
    bytes_read = file_read (process_file->file, buffer, size);
  }

  lock_release (&file_system_lock);

  return bytes_read;
}

/* 
  Wrapper function to execute write() system call 
*/
static void
write_wrapper (uint32_t *eax, int *addr) {
  *eax = write ((int) *(addr + 1), (const void *) *(addr + 2), (unsigned) *(addr + 3));
}

int 
write (int fd, const void *buffer, unsigned size) {
  verify_address (buffer);
  if (fd == 0) {
    return 0;
  }

  if (fd == 1) {
    putbuf (buffer, size);    
    return size;
  }

  lock_acquire (&file_system_lock);

  int bytes_written = 0;

  process_file *process_file = find_file (fd);
  if (process_file) {
    bytes_written = file_write (process_file->file, buffer, size);
  }

  lock_release (&file_system_lock);

  return bytes_written;
}

/* 
  Wrapper function to execute seek() system call 
*/
static void
seek_wrapper (int *addr) {
  seek ((int) *(addr + 1), (unsigned) *(addr + 2));
}

void 
seek (int fd, unsigned position) {
  lock_acquire (&file_system_lock);

  process_file *process_file = find_file (fd);
  if (process_file) {
    file_seek (process_file->file, position);
  }

  lock_release (&file_system_lock);

}

/* 
  Wrapper function to execute tell() system call 
*/
static void
tell_wrapper (uint32_t *eax, int *addr) {
  *eax = tell ((int) *(addr + 1));
}

unsigned 
tell (int fd) {
  lock_acquire (&file_system_lock);

  int position = 0;

  process_file *process_file = find_file (fd);
  if (process_file) {
    position = file_tell (process_file->file);
  }

  lock_release (&file_system_lock);

  return position;
}

/* 
  Wrapper function to execute close() system call 
*/
static void
close_wrapper (int *addr) {
  close ((int) *(addr + 1));
}

void 
close (int fd) {
  lock_acquire (&file_system_lock);

  process_file *process_file = find_file (fd);
  if (process_file) {
    file_close (process_file->file);
    list_remove (&process_file->file_elem);
  }
  free (process_file);
  lock_release (&file_system_lock);

  return;
}

/* 
  Finds file with file descriptor fd in the file system.
  Returns pointer to the file, or NULL if file cannot be found. 
*/
static process_file *
find_file (int fd) {

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

/*
  Verifies that the given virutal address is in user space and 
  in the page directory of the current thread.
*/
static void 
verify_address (const void *vaddr) {
  if (!is_user_vaddr (vaddr)) {
    exit (-1);
  }
  if (!pagedir_get_page(thread_current ()->pagedir, vaddr)) {
    exit (-1);
  }
}

/* 
  Verifies the addresses of each of the arguments  
*/
static void 
verify_arguments (int *addr, int num_of_args) {
  for (int i = 1; i <= num_of_args; i++) {
    verify_address ((const void *) (addr + i));
  }
}

/*
  Displays the names and exit status of the thread that has just exited
*/
static void 
print_termination_output (void) {
  printf ("%s: exit(%d)\n", thread_current ()->name, thread_current ()->process_status);
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