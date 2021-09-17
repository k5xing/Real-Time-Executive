//
//  priority_queue -> c
//  PriorityQueue
//
//  Created by Rebecca Zhou on 2021-06-14.
//

#include "priority_queue.h"

// pop the first one in the queue which is the head
TCB* pop_head(QUEUE* queue) {
    TCB* temp = queue -> HEAD;
    if (temp == NULL) {                                         // return NULL on empty queue
        return NULL;
    } else {
        queue -> HEAD = queue -> HEAD -> next;
        if (queue -> HEAD == NULL) {
            queue -> TAIL = NULL;
        } else {
            queue -> HEAD -> prev = NULL;                       // HEAD pointer cannot have prev TCB
        }
				queue -> current_size = queue -> current_size - 1;      // decrease the size of the queue
				temp -> next = NULL;
				temp -> prev = NULL;
		}
		if (temp -> state == BLK_SEND) {
			temp -> state = READY;																		// set the state to READY if it is poped from waiting list
		} else {
			temp -> state = RUNNING;                                    // set the state to RUNNING
		}
    
  
		return temp;
};
// pop the last one in the queue which is the tail
TCB* pop_tail(QUEUE* queue) {
    TCB* temp = queue -> TAIL;
    if (temp == NULL || queue -> current_size == 0) {           // return NULL on empty queue
        return NULL;
    } else {
        queue -> TAIL = queue -> TAIL -> prev;
        if (queue -> TAIL == NULL) {
            queue -> HEAD = NULL;
        } else {
            queue -> TAIL -> next = NULL;                       // TAIL pointer cannot have next TCB
        }
        queue -> current_size = queue -> current_size - 1;      // decreases the size of the queue
				temp -> next = NULL;
				temp -> prev = NULL;
    }
    /* you may comment this line out if poping the tail does not change the state */
    // temp -> state = RUNNING;                                    // set the state to RUNNING
    return temp;
};

// remove specific task
int pop_task(TCB* t_task,QUEUE* queue) {
    if(queue->HEAD == NULL || queue-> TAIL == NULL){
        return -1;                                              // return -1 if the queue is empty
    }else{
        if(queue->HEAD == t_task){
            pop_head(queue);
						t_task ->state = READY;
            //queue -> current_size = queue -> current_size - 1;
            return 1;
        }else if(queue->TAIL == t_task){
						pop_tail(queue);
					//queue -> current_size = queue -> current_size - 1;
            return 1;
        }else{
            TCB *temp = queue->HEAD->next;
            while(temp != queue->TAIL){
                if(temp == t_task){
                    TCB *pre_tcb = temp -> prev;
                    TCB *next_tcb = temp -> next;
                    pre_tcb -> next = next_tcb;
                    next_tcb -> prev = pre_tcb;
                    queue -> current_size = queue -> current_size - 1;
                    return 1;                                   //return 1 if remove
                }else{
                    temp = temp -> next;
                }
            }
            return 0;                                           //return 0 if not detect task
        }
    }
}
// push a new TCB into the front of the prioirty queue
// returns 1 if it is successfully added
int push_head(TCB* t_head, QUEUE* queue) {
    if (t_head == NULL) {
        return -1;                                              // return -1 if the new TCB is NULL
    } else {
        if (queue -> HEAD == NULL) {                            // if the queue is empty
            queue -> HEAD = t_head;                             // set both TAIL and HEAD to point to new TCB
            queue -> TAIL = t_head;
        } else {
            t_head -> next = queue -> HEAD;
            queue -> HEAD -> prev = t_head;
            queue -> HEAD = t_head;
        }
        queue -> current_size = queue -> current_size + 1;      // increase the queue size
        /* set the state to READY as it gets pushed back to the queue */
				if(t_head->state != SUSPENDED){
						queue -> HEAD -> state = READY;
				}
				queue -> HEAD -> prev = NULL;
    }
    return 1;
};

// push a new TCB into the back of the priority queue
// returns 1 if it is successfully added
int push_tail(TCB* t_tail, QUEUE* queue) {
    if (t_tail == NULL) {
        return -1;                                              // return -1 if the new TCB is NULL
    } else {
        if (queue -> TAIL == NULL) {                            // if the queue is empty
            queue -> HEAD = t_tail;                             // set both TAIL and HEAD to point to new TCB
            queue -> TAIL = t_tail;
        }else {
						if(t_tail->real_time == 1){
							if (t_tail -> prio != PRIO_RT_LB){
									errno = EPERM;
									return RTX_ERR;
							}
							TCB* temp_tcb = queue -> HEAD;
							while(temp_tcb -> tsk_deadline < t_tail -> tsk_deadline){
									temp_tcb = temp_tcb -> next;
									if(temp_tcb == NULL){
										break;
									}
							}
							if(temp_tcb ==queue -> HEAD){
									queue -> HEAD -> prev = t_tail;
									t_tail -> next = queue -> HEAD;
									queue -> HEAD = t_tail;
							}
							else if(temp_tcb != NULL){
									t_tail -> prev = temp_tcb -> prev;
									t_tail -> next = temp_tcb;
									temp_tcb -> prev -> next = t_tail;
									temp_tcb -> prev = t_tail;
									
							}else{
									queue -> TAIL -> next = t_tail;
									t_tail -> prev = queue -> TAIL;
									queue -> TAIL = t_tail;
								
							}
						}else{
							t_tail -> prev = queue -> TAIL;
							queue -> TAIL -> next = t_tail;
							queue -> TAIL = t_tail;
						} 
        }
        queue -> current_size = queue -> current_size + 1;
				queue -> TAIL -> next = NULL;
				// if the queue is ready queue not wait list
				if (t_tail -> state != BLK_SEND && t_tail->state !=SUSPENDED) {
					/* set the state to READY as it gets pushed back to the queue */
					queue -> TAIL -> state = READY;
				}
    }
    return 1;
};

// print the entire queue
// testing purpose only
void print_queue(QUEUE* queue) {
    TCB* current = queue -> HEAD;
    while (current != NULL) {
        printf("%d ", current -> tid);
        current = current -> next;
    }
    printf("\n");
}

