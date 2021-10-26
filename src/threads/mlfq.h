#ifndef THREADS_MLFQ_H
#define THREADS_MLFQ_H

#include "lib/kernel/list.h"
#define EMPTY_QUEUE NULL

/* Multi-Level Feedback Queue
    - Stores elements of type mlfq_elem, each with a queue of threads of 
      a certain priority    
 */
typedef struct list mlfq;

/* Stores a Priority Queue of threads for Multi-level Feedback Queue  */
typedef struct {
   int priority;           /* Priority of threads stored in queue */
   struct list *queue;     /* Queue storing threads of same priority */
   struct list_elem elem;  /* List elem for multqueue */
} mlfq_elem;

void add_to_mlfq (mlfq *, struct list_elem *);
void remove_thread_mlfq (mlfq *, struct list_elem *);
int get_highest_priority_mlfq (mlfq *);
struct list_elem *get_highest_thread_mlfq (mlfq *);
mlfq_elem *find_elem_of_priority (mlfq *, int);
int size_of_mlfq(void);

#endif /* threads/mlfq.h */