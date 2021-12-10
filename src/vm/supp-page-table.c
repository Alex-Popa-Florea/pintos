#include "supp-page-table.h"
#include "threads/malloc.h"
#include <debug.h>
#include "vm/swap.h"

unsigned 
supp_hash (const struct hash_elem *e, void *aux UNUSED) {
  const supp_pte *entry = hash_entry (e, supp_pte, elem);
  return hash_bytes (&entry->uaddr, sizeof entry->uaddr);
}

bool 
supp_hash_compare (const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED) {
  const supp_pte *entryA = hash_entry (a,  supp_pte, elem);
  const supp_pte *entryB = hash_entry (b,  supp_pte, elem);

  return entryA->uaddr < entryB->uaddr;
}

void 
supp_destroy (struct hash_elem *e, void *aux UNUSED) {

  supp_pte *supp_entry = hash_entry (e, supp_pte, elem);

  free_frame_from_supp_pte (e, thread_current ());

  swap_entry search_entry;
  search_entry.supp_pte = supp_entry;
  struct hash_elem *to_delete_swap_elem = hash_find (&swap_table, &search_entry.elem);
  if (to_delete_swap_elem != NULL) {
    swap_entry *to_delete_swap_entry = hash_entry (to_delete_swap_elem, swap_entry, elem);
    hash_delete (&swap_table, to_delete_swap_elem);
    free (to_delete_swap_entry);
  }

  free (supp_entry);
}

supp_pte *
create_supp_pte (struct file *file, off_t ofs, uint8_t *upage,
                 uint32_t read_bytes, uint32_t zero_bytes, bool writable, enum source source) 
{
  supp_pte *entry = (supp_pte *) malloc (sizeof (supp_pte));
  if (!entry) {
    return NULL;
  }
  entry->uaddr = upage;
  entry->file = file;
  entry->ofs = ofs;
  entry->read_bytes = read_bytes;
  entry->zero_bytes = zero_bytes;
  entry->writable = writable;
  entry->page_source = source;
  entry->page_frame = NULL;
  entry->is_in_swap_space = false;

  entry->thread = thread_current ();
  return entry;
}