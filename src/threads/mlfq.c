#include <debug.h>
#include "threads/mlfq.h"
#include "threads/thread.h"
#ifdef USERPROG
#include "userprog/process.h"
#endif

/* Records the number of threads in mlfq */
int mlfq_size = 0;

static bool compare_priority_mlfq(
    const struct list_elem *, const struct list_elem *, void * UNUSED
);

/* Adds a new thread to the right queue in multi-level feedback queue */
void add_to_mlfq(mlfq *mult_queue, struct list_elem *thread_elem) {
  struct thread *thread = list_entry(thread_elem, struct thread, elem);
  mlfq_elem *queue_elem = find_elem_of_priority(mult_queue, thread->priority);
  if(queue_elem == EMPTY_QUEUE) {
    mlfq_elem e;
    struct list thread_queue;
    e.priority = thread->priority;
    list_init(&thread_queue);
    e.queue = &thread_queue;
    list_push_back(e.queue, thread_elem);
    list_push_back(mult_queue, &e.elem);
  }
  else{
    list_push_back(queue_elem->queue, thread_elem);
  }
  mlfq_size++;
}

/* Removes a thread from its level in multi-level feedback queue */
void remove_from_mlfq (mlfq *mult_queue, struct list_elem *thread_elem) {
  struct thread *t = list_entry(thread_elem, struct thread, elem);
  mlfq_elem *level = find_elem_of_priority(mult_queue, t->priority);
  list_remove(thread_elem);
  if (list_empty(level->queue)) {
    list_prev(&level->elem)->next = list_next(&level->elem);
  }
  mlfq_size--;
}


/* Returns the priority of highest level in multi-level feedback queue */
int get_highest_priority_mlfq(mlfq *mult_queue) {
  return list_entry(list_max(mult_queue, compare_priority_mlfq, NULL), mlfq_elem, elem)->priority;
}

/* Compares two priority levels in multi-level feedback queue */
static bool compare_priority_mlfq(const struct list_elem *a, const struct list_elem *b, void *aux UNUSED){
  mlfq_elem *A = list_entry(a, mlfq_elem,elem);
  mlfq_elem *B = list_entry(b, mlfq_elem,elem);

  return A->priority < B->priority;
}

/* Compares priority of two threads */
static bool compare_thread_recent_cpu(const struct list_elem *a, const struct list_elem *b, void *aux UNUSED){
  struct thread *A = list_entry(a, struct thread, elem);
  struct thread *B = list_entry(b, struct thread, elem);

  return convert_int_nearest(A->recent_cpu) < convert_int_nearest(B->recent_cpu);
}

/* Returns thread with highest priority in multi-level feedback queue */
struct list_elem *get_highest_thread_mlfq(mlfq *mult_queue) {
  mlfq_elem *elem = list_entry(list_max(mult_queue, compare_priority_mlfq, NULL), mlfq_elem, elem);
  return list_max(elem->queue, compare_thread_recent_cpu, NULL);
}

/* Finds level of priority 'priority' in multi-level feedback queue */
mlfq_elem *find_elem_of_priority(mlfq *mult_queue, int priority){
  struct list_elem *e;
  for (e = list_begin (mult_queue); e != list_end (mult_queue); e = list_next (e)){
    mlfq_elem *queue_elem = list_entry(e, mlfq_elem, elem);
    if(queue_elem->priority == priority){
      return queue_elem;
    }
  }
  return EMPTY_QUEUE;
}

int size_mlfq() {
  return mlfq_size;
}
