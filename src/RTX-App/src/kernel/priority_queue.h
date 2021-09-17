//
//  priority_queue.h
//  PriorityQueue
//
//  Created by Rebecca Zhou on 2021-06-14.
//

#ifndef priority_queue_h
#define priority_queue_h
#include "common.h"
#include "common_ext.h"
#include "k_inc.h"


typedef struct priority_queue {
    TCB* HEAD;
    TCB* TAIL;
    unsigned int current_size;
} QUEUE;

void priority_init(QUEUE* queue);
TCB* pop_head(QUEUE* queue);
TCB* pop_tail(QUEUE* queue);
int pop_task(TCB* t_task,QUEUE* queue);
int push_head(TCB* n_head, QUEUE* queue);
int push_tail(TCB* t_tail, QUEUE* queue);
void print_queue(QUEUE* queue);
void ready_queues_init( QUEUE* queues[], QUEUE level0, QUEUE level1, QUEUE level2, QUEUE level3);

#endif /* priority_queue_h */
