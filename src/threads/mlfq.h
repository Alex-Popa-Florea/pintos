#ifndef THREADS_MLFQ_H
#define THREADS_MLFQ_H

#include "lib/kernel/list.h"
#define EMPTY_QUEUE NULL

/* Multi-Level Feedback Queue
   = Array of pointers to different list structs, each corresponding
     to a priority level
 */
typedef struct list mlfq;

/* Stores a Priority Queue of threads for Multi-level Feedback Queue  */
typedef struct {
   int priority;           /* Priority of threads stored in queue */
   int size;               /* Size of queue in struct */        /*saves time O(n), n = num of prios saves recaluclation of queue lenghts*/
   struct list *queue;     /* Queuestoring threads of same priority */
   struct list_elem elem;  /* List elem for multqueue */
} mlfq_elem;

void add_to_mlfq (mlfq *, struct list_elem *);
void remove_from_mlfq (mlfq *, struct list_elem *);
int get_highest_priority_mlfq (mlfq *);
struct list_elem *get_highest_thread_mlfq (mlfq *);
mlfq_elem *find_elem_of_priority (mlfq *, int);

#endif /* threads/mlfq.h */