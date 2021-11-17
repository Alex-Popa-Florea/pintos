#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/tss.h"
#include "userprog/syscall.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "lib/string.h"
#include "threads/malloc.h"
#include "devices/timer.h"

static thread_func start_process NO_RETURN;
static bool load (const char *cmdline, void (**eip) (void), void **esp);

/* 
  Limit on size of command-line arguments (bytes) 
*/
#define COMMAND_LINE_LIMIT (128)

/*
  Intialise a global list to store all of the pcbs
*/
struct list pcb_list = LIST_INITIALIZER (pcb_list); 
struct lock pcb_list_lock;

tid_t
process_execute (const char *file_name) 
{
  char *fn_copy;
  tid_t tid;

  fn_copy = palloc_get_page (0); 
  if (fn_copy == NULL)
    return TID_ERROR;
  strlcpy (fn_copy, file_name, PGSIZE);

  int command_size = strlen(file_name) + 1;
  char str[command_size];
  strlcpy (str, file_name, command_size);
  char *strPointer;
  char *arg1 = strtok_r(str, " ", &strPointer);

  tid = thread_create (arg1, PRI_DEFAULT, start_process, fn_copy);
  if (tid == TID_ERROR) 
    palloc_free_page (fn_copy); 

  return tid;
}

static bool check_stack_overflow (void *esp, unsigned long dcr) {
  return ((int *) esp - dcr) > 0;
}

/*  
  A thread function that loads a user process and starts it running.
*/
static void
start_process (void *file_name_)
{
  char *whole_file = file_name_;
  struct intr_frame if_;
  bool success;

  char *token, *save_ptr;
  
  int file_size = strlen(whole_file) + 1;
  char str[file_size];
  strlcpy (str, whole_file, file_size);
  
  int limit = COMMAND_LINE_LIMIT / 2;
  /* Array used to store command line arguments. */
  char *argv[limit];

  /* Records the number of command line arguments */
  int argc = 0; 

  token = strtok_r (str, " ", &save_ptr);
  char *file_name = token;

  while (token != NULL && argc < limit) {
    argv[argc] = token;
    argc++;
    token = strtok_r (NULL, " ", &save_ptr);
  }


  /* Initialize interrupt frame and load executable. */
  memset (&if_, 0, sizeof if_);
  if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
  if_.cs = SEL_UCSEG;
  if_.eflags = FLAG_IF | FLAG_MBS;
  success = load (file_name, &if_.eip, &if_.esp);

  palloc_free_page (whole_file);

  pcb *parent_pcb = get_pcb_from_id (thread_current ()->parent_id);
  parent_pcb->load_process_success = success;
  sema_up (&parent_pcb->load_sema);

  if (!success) 
    thread_exit ();

  /* Set up the stack with the tokenised arguments */

  /* Add parsed arguments to stack */
  char *argv_ptrs[argc];
  unsigned long dcr = 0;

  for (int i = argc - 1; i >= 0; i--) {
    dcr = sizeof (char) * (strlen (argv[i]) + 1);
    if (check_stack_overflow (if_.esp, dcr)) {
      if_.esp = if_.esp - dcr;
      strlcpy (if_.esp, argv[i], sizeof (char) * (strlen (argv[i]) + 1));
      argv_ptrs[i] = if_.esp;
    } else {
      exit (-1);
    }
  }

  /* Add word align buffer to the stack */
  dcr = sizeof (uint8_t);
  if (check_stack_overflow (if_.esp, dcr)) {
    if_.esp = if_.esp - dcr;
    uint8_t word_align = 0;
    *(uint8_t *)(if_.esp) = word_align;
  } else {
    exit (-1);
  }

  /* Adds null pointer sentinel to the stack */
  dcr = sizeof (char *);
  if (check_stack_overflow (if_.esp, dcr)) {
    if_.esp = if_.esp - dcr;
    (*(char *)(if_.esp)) = 0; 
  } else {
    exit (-1);
  }
  
  
  /* Adds addresses of arguments to stack */
  for (int j = argc - 1; j >= 0; j--) {
    dcr = sizeof (char *);

    if (check_stack_overflow (if_.esp, dcr)) {
      if_.esp = if_.esp - dcr;
      *(char **)(if_.esp) = argv_ptrs[j];
    } else {
      exit (-1);
    }
  }
  
  /* Adds pointer to the start of argument array to stack */
  dcr = sizeof (char **);
  if (check_stack_overflow (if_.esp, dcr)) {
    if_.esp = if_.esp - dcr;
    char **argv_ptr = if_.esp + sizeof (char **);
    *(char ***)(if_.esp) = argv_ptr;
  } else {
    exit (-1);
  }

  /* Adds number of arguments to stack */
  dcr = sizeof (int);
  if (check_stack_overflow (if_.esp, dcr)) {
    if_.esp = if_.esp - dcr;
    (*(int *)(if_.esp)) = argc;
  } else {
    exit (-1);
  }
  
  
  /* Adds return address to stack */
  if (check_stack_overflow (if_.esp, dcr)) {
    if_.esp = if_.esp - dcr;
    (*(int *)(if_.esp)) = 0;  
  } else {
    exit (-1);
  }

  asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (&if_) : "memory");
  NOT_REACHED ();
}

void 
init_pcb (pcb *pcb, int id, int parent_id) {
  pcb->id = id;
  pcb->parent_id = parent_id;
  pcb->exit_status = PROCESS_UNTOUCHED_STATUS;
  sema_init (&pcb->wait_sema, 0);
  pcb->has_been_waited_on = false;
  pcb->load_process_success = false;
  sema_init (&pcb->load_sema, 0);
}

void
pcb_set_waiting_on (pcb *pcb, int waiting_on) {
  pcb->waiting_on = waiting_on;
}

bool 
process_has_child (pcb *parent, pid_t child_id) {
  pcb *child_pcb = get_pcb_from_id (child_id);
  if (!child_pcb) {
    return false;
  }
  return parent->id == child_pcb->parent_id;
}

pcb 
*get_pcb_from_id (tid_t tid) { 
  struct list_elem *e;
  lock_acquire (&pcb_list_lock);

  for (e = list_begin (&pcb_list); e != list_end (&pcb_list); e = list_next (e)) {
    pcb *current_pcb = list_entry (e, pcb, elem);
    if (current_pcb->id == tid) {
      lock_release (&pcb_list_lock);
      return current_pcb;
    }
  }
  
  lock_release (&pcb_list_lock);
  return NULL;
}

int
process_wait (tid_t child_tid) 
{
  struct thread *current_thread = thread_current ();
  pcb *current_pcb = get_pcb_from_id (current_thread->tid);
  //ASSERT(current_pcb);
  if (!process_has_child (current_pcb, child_tid)) {
    return -1;
  }

  pcb *child_pcb = get_pcb_from_id (child_tid);
  if (child_pcb->has_been_waited_on) {
    return -1;
  }
  child_pcb->has_been_waited_on = true;

  if (child_pcb->exit_status != PROCESS_UNTOUCHED_STATUS) {
    return child_pcb->exit_status;
  }

  current_pcb->waiting_on = child_tid;
  sema_down (&current_pcb->wait_sema);

  current_pcb->waiting_on = NOT_WAITING;
  int child_exit_status = child_pcb->exit_status;

  return child_exit_status;
}

void
process_exit (void)
{
  struct thread *cur = thread_current ();
  pcb *current_pcb = get_pcb_from_id  (cur->tid);
  //current_pcb->exit_status = cur->process_status;
  uint32_t *pd;
  
  lock_acquire (&file_system_lock);

  struct list *file_list = &cur->file_list;

  file_close (cur->executable_file);

  struct list_elem *e;
  while (!list_empty (file_list)) {
    e = list_pop_front (file_list);
    process_file *current_file = list_entry (e, process_file, file_elem);
    file_close (current_file->file);
    list_remove (e);
    free (current_file);
  }
  
  lock_release (&file_system_lock);
  lock_acquire (&pcb_list_lock);
  /* When a process exits, free all its child processes which have terminated */
  for (e = list_begin (&pcb_list); e != list_end (&pcb_list);) {
    pcb *child_pcb = list_entry (e, pcb, elem);
    if (child_pcb->parent_id == current_pcb->id && child_pcb->exit_status != PROCESS_UNTOUCHED_STATUS)  {
      e = list_remove (&child_pcb->elem);
      free (child_pcb);
    } else {
      e = list_next (e);
    }
  }
  lock_release (&pcb_list_lock);

  pcb *parent_pcb = get_pcb_from_id (current_pcb->parent_id);
  if (parent_pcb == NULL) {
    lock_acquire (&pcb_list_lock);
    list_remove (&current_pcb->elem);
    lock_release (&pcb_list_lock);
    free (&current_pcb);
  } else {
    if (current_pcb->id == parent_pcb->waiting_on) {
      sema_up (&parent_pcb->wait_sema);
    }
  }


  /* Destroy the current process's page directory and switch back
     to the kernel-only page directory. */
  pd = cur->pagedir;
  if (pd != NULL) {
      /* Correct ordering here is crucial.  We must set
         cur->pagedir to NULL before switching page directories,
         so that a timer interrupt can't switch back to the
         process page directory.  We must activate the base page
         directory before destroying the process's page
         directory, or our active page directory will be one
         that's been freed (and cleared). */
      cur->pagedir = NULL;
      pagedir_activate (NULL);
      pagedir_destroy (pd);
    }
}

void
process_activate (void)
{
  struct thread *t = thread_current ();

  /* Activate thread's page tables. */
  pagedir_activate (t->pagedir);

  /* Set thread's kernel stack for use in processing
     interrupts. */
  tss_update ();
}

/* We load ELF binaries.  The following definitions are taken
   from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

/* For use with ELF types in printf(). */
#define PE32Wx PRIx32   /* Print Elf32_Word in hexadecimal. */
#define PE32Ax PRIx32   /* Print Elf32_Addr in hexadecimal. */
#define PE32Ox PRIx32   /* Print Elf32_Off in hexadecimal. */
#define PE32Hx PRIx16   /* Print Elf32_Half in hexadecimal. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
   This appears at the very beginning of an ELF binary. */
struct Elf32_Ehdr
  {
    unsigned char e_ident[16];
    Elf32_Half    e_type;
    Elf32_Half    e_machine;
    Elf32_Word    e_version;
    Elf32_Addr    e_entry;
    Elf32_Off     e_phoff;
    Elf32_Off     e_shoff;
    Elf32_Word    e_flags;
    Elf32_Half    e_ehsize;
    Elf32_Half    e_phentsize;
    Elf32_Half    e_phnum;
    Elf32_Half    e_shentsize;
    Elf32_Half    e_shnum;
    Elf32_Half    e_shstrndx;
  };

/* Program header.  See [ELF1] 2-2 to 2-4.
   There are e_phnum of these, starting at file offset e_phoff
   (see [ELF1] 1-6). */
struct Elf32_Phdr
  {
    Elf32_Word p_type;
    Elf32_Off  p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
  };

/* Values for p_type.  See [ELF1] 2-3. */
#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

/* Flags for p_flags.  See [ELF3] 2-3 and 2-4. */
#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

static bool setup_stack (void **esp);
static bool validate_segment (const struct Elf32_Phdr *, struct file *);
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
                          uint32_t read_bytes, uint32_t zero_bytes,
                          bool writable);

/* Loads an ELF executable from FILE_NAME into the current thread.
   Stores the executable's entry point into *EIP
   and its initial stack pointer into *ESP.
   Returns true if successful, false otherwise. */
bool
load (const char *file_name, void (**eip) (void), void **esp) 
{
  struct thread *t = thread_current ();
  struct Elf32_Ehdr ehdr;
  struct file *file = NULL;
  off_t file_ofs;
  bool success = false;
  int i;

  lock_acquire (&file_system_lock);

  /* Allocate and activate page directory. */
  t->pagedir = pagedir_create ();
  if (t->pagedir == NULL) 
    goto done;
  process_activate ();

  /* Open executable file. */
  file = filesys_open (file_name);
  if (file == NULL) 
    {
      printf ("load: %s: open failed\n", file_name);
      goto done; 
    }

  /* Read and verify executable header. */
  if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
      || memcmp (ehdr.e_ident, "\177ELF\1\1\1", 7)
      || ehdr.e_type != 2
      || ehdr.e_machine != 3
      || ehdr.e_version != 1
      || ehdr.e_phentsize != sizeof (struct Elf32_Phdr)
      || ehdr.e_phnum > 1024) 
    {
      printf ("load: %s: error loading executable\n", file_name);
      goto done; 
    }

  /* Read program headers. */
  file_ofs = ehdr.e_phoff;
  for (i = 0; i < ehdr.e_phnum; i++) 
    {
      struct Elf32_Phdr phdr;

      if (file_ofs < 0 || file_ofs > file_length (file))
        goto done;
      file_seek (file, file_ofs);

      if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)
        goto done;
      file_ofs += sizeof phdr;
      switch (phdr.p_type) 
        {
        case PT_NULL:
        case PT_NOTE:
        case PT_PHDR:
        case PT_STACK:
        default:
          /* Ignore this segment. */
          break;
        case PT_DYNAMIC:
        case PT_INTERP:
        case PT_SHLIB:
          goto done;
        case PT_LOAD:
          if (validate_segment (&phdr, file)) 
            {
              bool writable = (phdr.p_flags & PF_W) != 0;
              uint32_t file_page = phdr.p_offset & ~PGMASK;
              uint32_t mem_page = phdr.p_vaddr & ~PGMASK;
              uint32_t page_offset = phdr.p_vaddr & PGMASK;
              uint32_t read_bytes, zero_bytes;
              if (phdr.p_filesz > 0)
                {
                  /* Normal segment.
                     Read initial part from disk and zero the rest. */
                  read_bytes = page_offset + phdr.p_filesz;
                  zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
                                - read_bytes);
                }
              else 
                {
                  /* Entirely zero.
                     Don't read anything from disk. */
                  read_bytes = 0;
                  zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
                }
              if (!load_segment (file, file_page, (void *) mem_page,
                                 read_bytes, zero_bytes, writable))
                goto done;
            }
          else
            goto done;
          break;
        }
    }

  /* Set up stack. */
  if (!setup_stack (esp))
    goto done;

  /* Start address. */
  *eip = (void (*) (void)) ehdr.e_entry;

  success = true;

 done:
  /* We arrive here whether the load is successful or not. */
  if (!success) {
    file_close (file);
  }
  if (success) {
    t->executable_file = file;
    file_deny_write (file);
  }
  lock_release (&file_system_lock);
  return success;
}

/* load() helpers. */

static bool install_page (void *upage, void *kpage, bool writable);

/* Checks whether PHDR describes a valid, loadable segment in
   FILE and returns true if so, false otherwise. */
static bool
validate_segment (const struct Elf32_Phdr *phdr, struct file *file) 
{
  /* p_offset and p_vaddr must have the same page offset. */
  if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK)) 
    return false; 

  /* p_offset must point within FILE. */
  if (phdr->p_offset > (Elf32_Off) file_length (file)) 
    return false;

  /* p_memsz must be at least as big as p_filesz. */
  if (phdr->p_memsz < phdr->p_filesz) 
    return false; 

  /* The segment must not be empty. */
  if (phdr->p_memsz == 0)
    return false;
  
  /* The virtual memory region must both start and end within the
     user address space range. */
  if (!is_user_vaddr ((void *) phdr->p_vaddr))
    return false;
  if (!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz)))
    return false;

  /* The region cannot "wrap around" across the kernel virtual
     address space. */
  if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
    return false;

  /* Disallow mapping page 0.
     Not only is it a bad idea to map page 0, but if we allowed
     it then user code that passed a null pointer to system calls
     could quite likely panic the kernel by way of null pointer
     assertions in memcpy(), etc. */
  if (phdr->p_vaddr < PGSIZE)
    return false;

  /* It's okay. */
  return true;
}

/* Loads a segment starting at offset OFS in FILE at address
   UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
   memory are initialized, as follows:

        - READ_BYTES bytes at UPAGE must be read from FILE
          starting at offset OFS.

        - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.

   The pages initialized by this function must be writable by the
   user process if WRITABLE is true, read-only otherwise.

   Return true if successful, false if a memory allocation error
   or disk read error occurs. */
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable) 
{
  ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
  ASSERT (pg_ofs (upage) == 0);
  ASSERT (ofs % PGSIZE == 0);

  file_seek (file, ofs);
  while (read_bytes > 0 || zero_bytes > 0) 
    {
      /* Calculate how to fill this page.
         We will read PAGE_READ_BYTES bytes from FILE
         and zero the final PAGE_ZERO_BYTES bytes. */
      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      size_t page_zero_bytes = PGSIZE - page_read_bytes;
      
      /* Check if virtual page already allocated */
      struct thread *t = thread_current ();
      uint8_t *kpage = pagedir_get_page (t->pagedir, upage);
      
      if (kpage == NULL){
        
        /* Get a new page of memory. */
        kpage = palloc_get_page (PAL_USER);
        if (kpage == NULL){
          return false;
        }
        
        /* Add the page to the process's address space. */
        if (!install_page (upage, kpage, writable)) 
        {
          palloc_free_page (kpage);
          return false; 
        }        
      }

      /* Load data into the page. */
      if (file_read (file, kpage, page_read_bytes) != (int) page_read_bytes)
        {
          palloc_free_page (kpage);
          return false; 
        }
      memset (kpage + page_read_bytes, 0, page_zero_bytes);

      /* Advance. */
      read_bytes -= page_read_bytes;
      zero_bytes -= page_zero_bytes;
      upage += PGSIZE;
    }
  return true;
}

/* Create a minimal stack by mapping a zeroed page at the top of
   user virtual memory. */
static bool
setup_stack (void **esp) 
{
  uint8_t *kpage;
  bool success = false;

  kpage = palloc_get_page (PAL_USER | PAL_ZERO);
  if (kpage != NULL) 
    {
      success = install_page (((uint8_t *) PHYS_BASE) - PGSIZE, kpage, true);
      if (success)
        *esp = PHYS_BASE;
      else
        palloc_free_page (kpage);
    } 
  return success;
}

/* Adds a mapping from user virtual address UPAGE to kernel
   virtual address KPAGE to the page table.
   If WRITABLE is true, the user process may modify the page;
   otherwise, it is read-only.
   UPAGE must not already be mapped.
   KPAGE should probably be a page obtained from the user pool
   with palloc_get_page().
   Returns true on success, false if UPAGE is already mapped or
   if memory allocation fails. */
static bool
install_page (void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current ();

  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page (t->pagedir, upage) == NULL
          && pagedir_set_page (t->pagedir, upage, kpage, writable));
}

void 
set_exit_status (pcb *p, int status) {
  //printf("Thread %d changed its status from %d to %d\n", t->tid, t->process_status, status);
  p->exit_status = status;
}
