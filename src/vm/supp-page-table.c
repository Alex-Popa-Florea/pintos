#include "supp-page-table.h"
#include "threads/malloc.h"
#include <debug.h>


unsigned 
supp_hash (const struct hash_elem *e, void *aux UNUSED) {
  const supp_pte *entry = hash_entry (e, supp_pte, elem);
  return hash_bytes (&entry->addr, sizeof entry->addr);
}

bool 
supp_hash_compare (const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED) {
  const supp_pte *entryA = hash_entry (a,  supp_pte, elem);
  const supp_pte *entryB = hash_entry (b,  supp_pte, elem);

  return entryA->addr < entryB->addr;
}

void 
destroy_elem (struct hash_elem *e, void *aux UNUSED) {
  supp_pte *entry = hash_entry (e, supp_pte, elem);
  free (entry);
}

supp_pte *
create_supp_pte (struct file *file, off_t ofs, uint8_t *upage,
                 uint32_t read_bytes, uint32_t zero_bytes, bool writable, enum source source) 
{
  supp_pte *entry = (supp_pte *) malloc (sizeof (supp_pte));
  entry->addr = upage;
  entry->file = file;
  entry->ofs = ofs;
  entry->read_bytes = read_bytes;
  entry->zero_bytes = zero_bytes;
  entry->writable = writable;
  entry->is_dirty = false;
  entry->page_source = source;
  entry->page_frame = NULL;
  entry->is_in_swap_space = false;
  return entry;
}