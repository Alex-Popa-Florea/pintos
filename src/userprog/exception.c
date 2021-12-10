#include "userprog/exception.h"
#include <inttypes.h>
#include <stdio.h>
#include "userprog/gdt.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "userprog/syscall.h"
#include "vm/supp-page-table.h"
#include "lib/kernel/hash.h"
#include "threads/palloc.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include "threads/vaddr.h"
#include "userprog/process.h"
#include "vm/frame.h"
#include "vm/swap.h"
#include "vm/share-table.h"
#include "string.h"

/*
  The following macros are used to check access faults when
  user programs try to write to the stack below the stack pointer

  These are used to distinguish between stack accesses and
  invalid accesses,
*/

/* 80x86 PUSH instruction may cause a page fault 4 bytes below the stack pointer */
#define PUSH_LIMIT (4)              
/* 80x86 PUSHA instruction may cause a page fault 32 bytes below the stack pointer */
#define PUSHA_LIMIT (32)

/* Number of page faults processed. */
static long long page_fault_cnt;

static void kill (struct intr_frame *);
static void page_fault (struct intr_frame *);

static bool load_page_from_filesys (supp_pte *);

static bool acquire_table_locks (void);
static bool release_table_locks (bool);

static bool acquire_filesys_lock (void);
static bool release_filesys_lock (bool);
/* Registers handlers for interrupts that can be caused by user
   programs.

   In a real Unix-like OS, most of these interrupts would be
   passed along to the user process in the form of signals, as
   described in [SV-386] 3-24 and 3-25, but we don't implement
   signals.  Instead, we'll make them simply kill the user
   process.

   Page faults are an exception.  Here they are treated the same
   way as other exceptions, but this will need to change to
   implement virtual memory.

   Refer to [IA32-v3a] section 5.15 "Exception and Interrupt
   Reference" for a description of each of these exceptions. */
void
exception_init (void) 
{
  /* These exceptions can be raised explicitly by a user program,
     e.g. via the INT, INT3, INTO, and BOUND instructions.  Thus,
     we set DPL==3, meaning that user programs are allowed to
     invoke them via these instructions. */
  intr_register_int (3, 3, INTR_ON, kill, "#BP Breakpoint Exception");
  intr_register_int (4, 3, INTR_ON, kill, "#OF Overflow Exception");
  intr_register_int (5, 3, INTR_ON, kill, "#BR BOUND Range Exceeded Exception");

  /* These exceptions have DPL==0, preventing user processes from
     invoking them via the INT instruction.  They can still be
     caused indirectly, e.g. #DE can be caused by dividing by
     0.  */
  intr_register_int (0, 0, INTR_ON, kill, "#DE Divide Error");
  intr_register_int (1, 0, INTR_ON, kill, "#DB Debug Exception");
  intr_register_int (6, 0, INTR_ON, kill, "#UD Invalid Opcode Exception");
  intr_register_int (7, 0, INTR_ON, kill, "#NM Device Not Available Exception");
  intr_register_int (11, 0, INTR_ON, kill, "#NP Segment Not Present");
  intr_register_int (12, 0, INTR_ON, kill, "#SS Stack Fault Exception");
  intr_register_int (13, 0, INTR_ON, kill, "#GP General Protection Exception");
  intr_register_int (16, 0, INTR_ON, kill, "#MF x87 FPU Floating-Point Error");
  intr_register_int (19, 0, INTR_ON, kill, "#XF SIMD Floating-Point Exception");

  /* Most exceptions can be handled with interrupts turned on.
     We need to disable interrupts for page faults because the
     fault address is stored in CR2 and needs to be preserved. */
  intr_register_int (14, 0, INTR_OFF, page_fault, "#PF Page-Fault Exception");
}

/* Prints exception statistics. */
void
exception_print_stats (void) 
{
  printf ("Exception: %lld page faults\n", page_fault_cnt);
}

/* Handler for an exception (probably) caused by a user process. */
static void
kill (struct intr_frame *f) 
{
  /* This interrupt is one (probably) caused by a user process.
     For example, the process might have tried to access unmapped
     virtual memory (a page fault).  For now, we simply kill the
     user process.  Later, we'll want to handle page faults in
     the kernel.  Real Unix-like operating systems pass most
     exceptions back to the process via signals, but we don't
     implement them. */
     
  /* The interrupt frame's code segment value tells us where the
     exception originated. */
  
  switch (f->cs)
    {
    case SEL_UCSEG:
      /* User's code segment, so it's a user exception, as we
         expected.  Kill the user process.  */
      printf ("%s: dying due to interrupt %#04x (%s).\n",
              thread_name (), f->vec_no, intr_name (f->vec_no));
      intr_dump_frame (f);
      exit (EXIT_ERROR); 

    case SEL_KCSEG:
      /* Kernel's code segment, which indicates a kernel bug.
         Kernel code shouldn't throw exceptions.  (Page faults
         may cause kernel exceptions--but they shouldn't arrive
         here.)  Panic the kernel to make the point.  */
      intr_dump_frame (f);
      PANIC ("Kernel bug - unexpected interrupt in kernel"); 

    default:
      /* Some other code segment?  
         Shouldn't happen.  Panic the kernel. */
      printf ("Interrupt %#04x (%s) in unknown segment %04x\n",
             f->vec_no, intr_name (f->vec_no), f->cs);
      PANIC ("Kernel bug - this shouldn't be possible!");
    }
}

/* Page fault handler.  This is a skeleton that must be filled in
   to implement virtual memory.  Some solutions to task 2 may
   also require modifying this code.

   At entry, the address that faulted is in CR2 (Control Register
   2) and information about the fault, formatted as described in
   the PF_* macros in exception.h, is in F's error_code member.  The
   example code here shows how to parse that information.  You
   can find more information about both of these in the
   description of "Interrupt 14--Page Fault Exception (#PF)" in
   [IA32-v3a] section 5.15 "Exception and Interrupt Reference". */
static void
page_fault (struct intr_frame *f) 
{
  bool not_present;  /* True: not-present page, false: writing r/o page. */
  bool write;        /* True: access was write, false: access was read. */
  bool user;         /* True: access by user, false: access by kernel. */
  void *fault_addr;  /* Fault address. */
  /* Obtain faulting address, the virtual address that was
     accessed to cause the fault.  It may point to code or to
     data.  It is not necessarily the address of the instruction
     that caused the fault (that's f->eip).
     See [IA32-v2a] "MOV--Move to/from Control Registers" and
     [IA32-v3a] 5.15 "Interrupt 14--Page Fault Exception
     (#PF)". */
  asm ("movl %%cr2, %0" : "=r" (fault_addr));

  /* Turn interrupts back on (they were only off so that we could
     be assured of reading CR2 before it changed). */
  intr_enable ();
  /* Count page faults. */
  page_fault_cnt++;

  /* Determine cause. */
  not_present = (f->error_code & PF_P) == 0;
  write = (f->error_code & PF_W) != 0;
  user = (f->error_code & PF_U) != 0;

  bool held = acquire_filesys_lock ();

  /* 
   If page fault occurred because page is not present, 
   loads the page from the current thread's supplemental page table
  */
  bool load_success = false;
  if (not_present && is_user_vaddr (fault_addr)) {
      struct thread *t = thread_current ();
      supp_pte fault_entry;
      fault_entry.uaddr = pg_round_down (fault_addr);

      struct hash_elem *found_elem = hash_find (&t->supp_page_table, &fault_entry.elem);
      if (!found_elem) {
        load_success = false;
      } else {
        supp_pte *entry = hash_entry (found_elem, supp_pte, elem);

        /* 
          Determine the source of supplemental page table entry to load from
        */
        switch (entry->page_source) {
          case MMAP:
            load_success = load_page_from_filesys (entry);
            break;
          
          case STACK:
            load_success = load_from_outside_filesys (entry);
            break;

          case DISK:
            if (entry->is_in_swap_space) {
              load_success = load_from_outside_filesys (entry);
            } else {
              load_success = load_page_from_filesys (entry);
            }
            break;

          default:
            break;
        }
      }
  } 

  /* 
    Consider stack growth 
    This is possible even if no supplemental page entry was found 
  */

  /* 
    Checks the fault address is not outside the stack's allowed 8 MB 
  */
  bool not_overflow = PHYS_BASE - pg_round_down (fault_addr) <= (1 << 23);

  /* Checks the fault address is above the stack pointer, equal to the stack
     pointer - PUSHA_LIMIT or equal to the stack pointer - PUSH_LIMIT
  */ 
  bool valid_fault_address = (uint32_t*) fault_addr >= (uint32_t *) f->esp
                              || (uint32_t*) fault_addr == (uint32_t *) (f->esp - PUSHA_LIMIT)
                              || (uint32_t*) fault_addr == (uint32_t *) (f->esp - PUSH_LIMIT);

  if (!load_success && not_overflow && valid_fault_address && is_user_vaddr (fault_addr)) {
    struct hash_elem *entry_elem = set_up_pte_for_stack (fault_addr);
    supp_pte *entry = hash_entry (entry_elem, supp_pte, elem);
    load_success = load_from_outside_filesys (entry);
  }

  release_filesys_lock (held);
  
  if (!load_success) {
    exit (EXIT_ERROR);
  }
}

static bool
entry_from_share_table (supp_pte *entry) {

  frame_table_entry search_frame;
  search_frame.inode = file_get_inode (entry->file);
  search_frame.ofs = entry->ofs;

  share_entry search_entry;
  search_entry.frame = &search_frame;

  struct hash_elem *search_elem = hash_find (&share_table, &search_entry.elem);
  if (search_elem != NULL) {
    /* 
      Page exists in share table - already allocated so elligble for sharing 
    */
    share_entry *found_entry = hash_entry (search_elem, share_entry, elem);

    /*
      Add to list of pages that share common frame
    */
    entry->page_frame = found_entry->frame;
    list_push_back (&found_entry->sharing_ptes, &entry->share_elem);


    if (!install_page (entry->uaddr, found_entry->frame->kpage, entry->writable)) {
      return false;
    }

    return true;
  }
  
  return false;
}

/*
  Loads a page from a supplemental page table entry into an active page, 
  using the frame table. If the page is readable, checks the share table and
  inserts if not present. Returns true
*/
static bool
load_page_from_filesys (supp_pte *entry) {

  bool table_held = acquire_table_locks ();

  bool shareable = !entry->writable && entry->page_source == DISK;
  /* 
    Check for existing entry in share table for read-only pages  
  */
  if (shareable) {
    if (entry_from_share_table (entry)) {
      release_table_locks (table_held);;
      return true;
    }
  }

  /*
    Try to acquire a new page of memory. 
  */
  frame_table_entry *new_frame;

  if (entry->page_source == MMAP) {
    new_frame = try_allocate_page (PAL_USER | PAL_ZERO, entry);
  } else {
    new_frame = try_allocate_page (PAL_USER, entry);
  }


  uint8_t *kpage = new_frame->kpage;

  if (kpage == NULL) {
    release_table_locks (table_held);
    return false;
  }
  
  /* 
    Add the page to the process's address space. 
  */
  if (!install_page (entry->uaddr, kpage, entry->writable)) {
    free_frame_from_supp_pte (&entry->elem, thread_current ());
    release_table_locks (table_held);
    return false;
  }

  /*
    Add the frame to the share table if readable 
  */
  if (shareable) {
    share_entry *new_share_entry = create_share_entry (entry, new_frame);
    hash_insert (&share_table, &new_share_entry->elem);
  }

  file_seek (entry->file, entry->ofs);
  off_t bytes_read = file_read (entry->file, kpage, entry->read_bytes);

  if (bytes_read != (off_t) entry->read_bytes) {
    free_frame_from_supp_pte (&entry->elem, thread_current ());
    release_table_locks (table_held);
    return false;
  }

  /* 
    Set the remaining bytes of the page to 0 
  */
  memset (kpage + entry->read_bytes, NULL, entry->zero_bytes);
  entry->page_frame = new_frame;
  release_table_locks (table_held);
  return true;
}


bool 
load_from_outside_filesys (supp_pte *entry) {

  bool table_held = acquire_table_locks ();

  /* 
    Try to acquire an empty frame from the frame table
  */
  frame_table_entry *new_frame = try_allocate_page (PAL_USER, entry);
  uint8_t *kpage = new_frame->kpage;

  if (!kpage) {
    release_table_locks (table_held);
    return false;
  }
  
  /* 
    Try to install supplemental page table into frame 
  */
  if (!install_page (entry->uaddr, kpage, entry->writable)) {
    free_frame_from_supp_pte (&entry->elem, thread_current ());
    release_table_locks (table_held);
    return false;
  }

  if (entry->is_in_swap_space) {
    /*
      Retrieve data from swap space and store in KPAGE  
    */
    retrieve_from_swap_space (entry, kpage);
    entry->is_in_swap_space = false;
  }

  entry->page_frame = new_frame;
  release_table_locks (table_held);
  return true;
}


/*
  Acquires the table locks of the current thread.
  Returns true if the locks are acquired.
*/
static bool
acquire_table_locks () {
  if (!lock_held_by_current_thread (&frame_table_lock)) {
    lock_tables ();
    return true;
  }
  return false;
}


/*
  Releases the table locks if the current thread currently holds them.
  Returns false if lock released.
*/
static bool
release_table_locks (bool held) {
  if (held) {
    held = false;
    release_tables ();
  }
  return held;
}

/*
  Acquires the file system lock of the current thread.
  Returns true if the lock is acquired.
*/
static bool
acquire_filesys_lock () {
  if (!lock_held_by_current_thread (&file_system_lock)) {
    lock_acquire (&file_system_lock);
    return true;
  }
  return false;
}

/*
  Releases the file system lock if the current thread currently holds it.
  Returns false if lock released.
*/
static bool
release_filesys_lock (bool held) {
  if (held) {
    held = false;
    lock_release (&file_system_lock);
  }
  return held;
}