#ifndef USERPROG_EXCEPTION_H
#define USERPROG_EXCEPTION_H

#include <stdbool.h>
#include "vm/supp-page-table.h"

/* Page fault error code bits that describe the cause of the exception.  */
#define PF_P 0x1    /* 0: not-present page. 1: access rights violation. */
#define PF_W 0x2    /* 0: read, 1: write. */
#define PF_U 0x4    /* 0: kernel, 1: user process. */

void exception_init (void);
void exception_print_stats (void);

/*
  Loads a page from a supplemental page table entry into an active page
*/
bool load_page (supp_pte *entry);

/*
  Loads a stack page from a supplemental page table entry into an active page
*/
bool load_stack_page (supp_pte *entry);

/*
  Loads a memory mapped file page from a supplemental page table entry into an active page
*/
bool load_mmap_page (supp_pte *entry);

#endif /* userprog/exception.h */
