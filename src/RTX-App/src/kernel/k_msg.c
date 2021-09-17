/*
 ****************************************************************************
 *
 *                  UNIVERSITY OF WATERLOO ECE 350 RTX LAB  
 *
 *                     Copyright 2020-2021 Yiqing Huang
 *                          All rights reserved.
 *---------------------------------------------------------------------------
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *  - Redistributions of source code must retain the above copyright
 *    notice and the following disclaimer.
 *
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL COPYRIGHT HOLDERS AND CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *---------------------------------------------------------------------------*/
 

/**************************************************************************//**
 * @file        k_msg.c
 * @brief       kernel message passing routines          
 * @version     V1.2021.06
 * @authors     Yiqing Huang
 * @date        2021 JUN
 *****************************************************************************/

#include "k_inc.h"
#include "k_rtx.h"
#include "k_msg.h"

MAILBOX	g_mailboxes[MAX_TASKS];

// copy memory from source to destination
void mem_cpy(U8* dest, U8* src, size_t cpy_length) {
	for (int i = 0; i < cpy_length; i++) {
		dest[i] = src[i];
	}
}

// return 1 if action is successful, 0 otherwise
int push_buffer(MB* memory_buf, U8* buf) { 
	RTX_MSG_HDR* message_header = (void*) buf;
	size_t message_length = message_header -> length;
	
	// message can not fit inside the ring buffer
	if (can_fit(memory_buf, message_length)!= 1) {
		return 0;
	}
	
	U8* start_addr = memory_buf -> tail;
	if (start_addr + message_length <= memory_buf -> memory_end) {		
		mem_cpy(start_addr, buf, message_length);
		memory_buf -> tail = memory_buf -> tail + message_length;
	} else {	// we require circling of the memory in this case
		size_t length_1 = memory_buf -> memory_end - start_addr;
		size_t length_2 = message_length - length_1;
		mem_cpy(start_addr, buf, length_1);
		mem_cpy(memory_buf -> memory_start, buf + length_1, length_2);
		memory_buf -> tail = memory_buf -> memory_start + length_2;
	}
	memory_buf -> current_size = memory_buf -> current_size + message_length;
	return 1;
}

// return 1 if action is successful
// return 0 if there is currently no message
// return -1 if the buffer is too small to hold the message
int pop_buffer(MB* memory_buf, U8* buf, size_t length) {
	if (memory_buf -> current_size == 0) {
		return 0;
	}
	if (length < MSG_HDR_SIZE) {
		return -1;
	}
	
	// copy header to get the rest of the length
	U8* start_addr = memory_buf -> head;
	if (start_addr + MSG_HDR_SIZE <= memory_buf -> memory_end) {
		mem_cpy(buf, start_addr, MSG_HDR_SIZE);
		start_addr = start_addr + MSG_HDR_SIZE;
	} else {	// we require circling of the memory in this case
		size_t length_1 = memory_buf -> memory_end - start_addr;
		size_t length_2 = MSG_HDR_SIZE - length_1;
		mem_cpy(buf, start_addr, length_1);
		mem_cpy(buf + length_1, memory_buf -> memory_start, length_2);
		start_addr = memory_buf -> memory_start + length_2;
	}
	
	// copy the message itself
	RTX_MSG_HDR* message_header = (void *) buf;
	size_t msg_length = message_header -> length - MSG_HDR_SIZE;
	if ( length < message_header -> length ) {
		return -1;
	}
	if ( start_addr + msg_length <= memory_buf -> memory_end ) {
		mem_cpy(buf + MSG_HDR_SIZE, start_addr, msg_length);
		start_addr = start_addr + msg_length;
	} else {	// we require circling of the memory in this case
		size_t length_1 = memory_buf -> memory_end - start_addr;
		size_t length_2 = msg_length - length_1;
		mem_cpy(buf + MSG_HDR_SIZE, start_addr, length_1);
		mem_cpy(buf + MSG_HDR_SIZE + length_1, memory_buf -> memory_start, length_2);
		start_addr = memory_buf -> memory_start + length_2;
	}
	
	memory_buf -> head = start_addr;
	memory_buf -> current_size = memory_buf -> current_size - message_header -> length;
	return 1;
}

// return -1 if buffer is full
// return 0 if message cannot fit in the buffer but buffer is not full
// return 1 if the message can fit
int can_fit(MB* memory_buf, size_t message_size) {
	if (memory_buf -> current_size == memory_buf -> capacity) {
		return -1;
	} else if (memory_buf -> capacity - memory_buf -> current_size < message_size) {
		return 0;
	}
	return 1;
}

int k_mbx_create(size_t size) {
#ifdef DEBUG_0
    printf("k_mbx_create: size = %u\r\n", size);
#endif /* DEBUG_0 */
	task_t current_tid = gp_current_task -> tid;
  if (g_mailboxes[current_tid].active == 1) {
		errno = EEXIST;
		return RTX_ERR;
	}
	
	if (size < MIN_MSG_SIZE) {
		errno = EINVAL;
		return RTX_ERR;
	}

	U8* mem_alloc = k_mpool_alloc(MPID_IRAM2, size);
	if (mem_alloc == NULL) {
		errno = ENOMEM;
		return RTX_ERR;
	}
	
	MAILBOX* new_mailbox = &g_mailboxes[current_tid];
	MB* new_buffer = &(new_mailbox -> memory_buffer);
	QUEUE* waiting_list = &(new_mailbox -> waiting_list);
	new_mailbox -> active = 1;
	new_mailbox -> receiver_id = current_tid;
	new_buffer -> capacity = size;
	new_buffer -> current_size = 0;
	new_buffer -> memory_start = mem_alloc;
	new_buffer -> memory_end = mem_alloc + size;
	new_buffer -> head = mem_alloc;
	new_buffer -> tail = mem_alloc;
	waiting_list -> HEAD = NULL;
	waiting_list -> TAIL = NULL;
	waiting_list -> current_size = 0;
	return current_tid;
}

int k_send_msg(task_t receiver_tid, const void *buf) {
#ifdef DEBUG_0
    printf("k_send_msg: receiver_tid = %d, buf=0x%x\r\n", receiver_tid, buf);
#endif /* DEBUG_0 */
	RTX_MSG_HDR* message_header = (RTX_MSG_HDR *) buf;
	
	if(receiver_tid != TID_UART){
		if (g_tcbs[receiver_tid].state == DORMANT) {
			errno = EINVAL;
			return RTX_ERR;
			}
	}
	
	if(message_header -> length < MIN_MSG_SIZE){
		errno = EINVAL;
		return RTX_ERR;
	}
	
	if(receiver_tid != TID_UART){
		if (g_mailboxes[receiver_tid].active == 0 ){
			errno = ENOENT;
			return RTX_ERR;
		}
	}else{
		if(receiver_tid == TID_UART && UART_MAILBOX.active == 0){
			errno = ENOENT;
			return RTX_ERR;
		}
	}
	
	MB* rcv_mb;
	if(receiver_tid == TID_UART){
	 rcv_mb = &UART_MAILBOX.memory_buffer;
	} else{
	 rcv_mb = &g_mailboxes[receiver_tid].memory_buffer;
	}
	
	if ( message_header -> length > rcv_mb->capacity)  {
		errno = EMSGSIZE;
		return RTX_ERR;
	}
	
	int check = can_fit(rcv_mb, message_header -> length);
	QUEUE* recv_wait_list;
	if(receiver_tid == TID_UART){
		recv_wait_list = &UART_MAILBOX.waiting_list;
	}else{
		recv_wait_list = &g_mailboxes[receiver_tid].waiting_list;
	}
	if (check == 1 && recv_wait_list -> current_size == 0) {	// if there is enough space in the buffer and there is nothing in the waiting list
		push_buffer(rcv_mb, (U8 *)buf);	// put the message in the buffer
		if(receiver_tid != TID_UART){
			TCB* rcv_tsk = &g_tcbs[receiver_tid];
			if (rcv_tsk -> state == BLK_RECV) {																	
				push_tail(rcv_tsk, &ready_queue[priority2order(rcv_tsk->prio)]);	// put the receiver task back to ready queue if not
				rcv_tsk -> state = READY;
				if (rcv_tsk -> prio < gp_current_task -> prio) {
					push_head(gp_current_task, &ready_queue[priority2order(gp_current_task->prio)]);
					k_tsk_run_new();		// this should allow the receiving task to continue
				}
			}
		}
	} else {	// if there is not enough space in the ring buffer
		gp_current_task -> state = BLK_SEND;	// the sending task will be blocked
		gp_current_task -> message_header = message_header;
		push_tail(gp_current_task, recv_wait_list);	// go to waiting queue
		k_tsk_run_new();	// msg_recv will always unblock this task and put it back to ready queue
	}
  return RTX_OK;
}

int k_send_msg_nb(task_t receiver_tid, const void *buf) {
#ifdef DEBUG_0
    printf("k_send_msg_nb: receiver_tid = %d, buf=0x%x\r\n", receiver_tid, buf);
#endif /* DEBUG_0 */
	RTX_MSG_HDR* message_header = (RTX_MSG_HDR *) buf;
	
		if(receiver_tid != TID_UART){
		if (g_tcbs[receiver_tid].state == DORMANT) {
			errno = EINVAL;
			return RTX_ERR;
			}
	}
	
	if(message_header -> length < MIN_MSG_SIZE){
		errno = EINVAL;
		return RTX_ERR;
	}
	
	if(receiver_tid != TID_UART){
		if (g_mailboxes[receiver_tid].active == 0 ){
			errno = ENOENT;
			return RTX_ERR;
		}
	}else{
		if(receiver_tid == TID_UART && UART_MAILBOX.active == 0){
			errno = ENOENT;
			return RTX_ERR;
		}
	}
	
	
	MB* rcv_mb;
	
	if(receiver_tid == TID_UART){
	 rcv_mb = &UART_MAILBOX.memory_buffer;
	} else{
	 rcv_mb = &g_mailboxes[receiver_tid].memory_buffer;
	}
	
	if ( message_header -> length > rcv_mb->capacity)  {
		errno = EMSGSIZE;
		return RTX_ERR;
	}
	QUEUE* recv_wait_list;
	if(receiver_tid == TID_UART){
		recv_wait_list = &UART_MAILBOX.waiting_list;
	}else{
		recv_wait_list = &g_mailboxes[receiver_tid].waiting_list;
	}
	
	int check = can_fit(rcv_mb, message_header -> length);
	if (check != 1 || recv_wait_list->current_size != 0) {	// if it cannot fit or there is any task in the waiting list
		errno = ENOSPC;
		return RTX_ERR;
	}
	push_buffer(rcv_mb, (U8 *)buf);
	
	if(receiver_tid != TID_UART){
		TCB* rcv_tsk = &g_tcbs[receiver_tid];
		if (rcv_tsk -> state  == BLK_RECV) {
			push_tail(rcv_tsk, &ready_queue[priority2order(rcv_tsk->prio)]);		// put the receiver task back to ready queue if not
			rcv_tsk -> state = READY;
			if (rcv_tsk -> prio < gp_current_task -> prio && message_header ->sender_tid != TID_UART) {
				push_head(gp_current_task, &ready_queue[priority2order(gp_current_task->prio)]);
				k_tsk_run_new();		// this should allow the receiving task to continue
			}
		}
	}
	
  return RTX_OK;
}

int k_recv_msg(void *buf, size_t len) {
#ifdef DEBUG_0
    printf("k_recv_msg: buf=0x%x, len=%d\r\n", buf, len);
#endif /* DEBUG_0 */
		if (buf == NULL) {
		errno = EFAULT;
		return RTX_ERR;
	}

	MAILBOX* recv_mb;
	if(g_uart_mailbox_collect == 1){
			recv_mb = &UART_MAILBOX;
	}else{
			recv_mb = &g_mailboxes[gp_current_task -> tid];
	}
	
	if (recv_mb -> active == 0) {
		errno = ENOENT;
		return RTX_ERR;
	}
	
	MB* recv_ring_buf = &(recv_mb -> memory_buffer);
	
	int check = pop_buffer(recv_ring_buf, (U8 *)buf, len);
	if (check == -1) {	// the buffer is too small
		errno = ENOSPC;
		return RTX_ERR;
	} else if (check == 0) {	// there is currently no message in the ring buffer
		gp_current_task -> state = BLK_RECV;	// set the state to block receive
		k_tsk_run_new();	// we do not want to put this task back in the ready_queue
		pop_buffer(recv_ring_buf, (U8 *)buf, len);	// get the buffer when we have a message
	}
	
	QUEUE* recv_wait_list = &(recv_mb -> waiting_list);
	// if the size of the waiting list is not zero AND the head is not pointing to NULL
	while (recv_wait_list -> current_size > 0 && recv_wait_list -> HEAD != NULL) {
		TCB* first_wait_tsk = recv_wait_list -> HEAD;	// peak for the first task
		check = can_fit(recv_ring_buf, first_wait_tsk -> message_header -> length);
		if (check != 1) {	// leave the loop if cannot fit
			break;
		}
		first_wait_tsk = pop_head(recv_wait_list);	// pop from the waiting list
		U8* buf = (U8 *)first_wait_tsk -> message_header;	// find the message header
		push_buffer(recv_ring_buf, buf);	// push the message to the ring buffer
		push_tail(first_wait_tsk, &ready_queue[priority2order(first_wait_tsk->prio)]);	// push the sending task back to the ready queue and change its state to READY
	}
	
	push_head(gp_current_task, &ready_queue[priority2order(gp_current_task->prio)]);	// put the current task back to the HEAD of its ready queue with the old prio
	k_tsk_run_new();	// this will decide who should run next
  return RTX_OK;
}

int k_recv_msg_nb(void *buf, size_t len) {
#ifdef DEBUG_0
    printf("k_recv_msg_nb: buf=0x%x, len=%d\r\n", buf, len);
#endif /* DEBUG_0 */
	if (buf == NULL) {
		errno = EFAULT;
		return RTX_ERR;
	}
	MAILBOX* recv_mb;
	if(g_uart_mailbox_collect == 1){
			recv_mb = &UART_MAILBOX;
	}else{
			recv_mb = &g_mailboxes[gp_current_task -> tid];
	}
	
	if (recv_mb -> active == 0) {
		errno = ENOENT;
		return RTX_ERR;
	}
	
	MB* recv_ring_buf = &(recv_mb -> memory_buffer);
	if (recv_ring_buf -> current_size == 0) {
		errno = ENOMSG;
		return RTX_ERR;
	}
	
	int check = pop_buffer(recv_ring_buf, (U8 *)buf, len);
	if (check == -1) {
		errno = ENOSPC;
		return RTX_ERR;
	}
	
	QUEUE* recv_wait_list = &(recv_mb -> waiting_list);
	// if the size of the waiting list is not zero AND the head is not pointing to NULL
	while (recv_wait_list -> current_size > 0 && recv_wait_list -> HEAD != NULL) {
		TCB* first_wait_tsk = recv_wait_list -> HEAD;	// peak for the first task
		check = can_fit(recv_ring_buf, first_wait_tsk -> message_header -> length);
		if (check != 1) {	// leave the loop if cannot fit
			break;
		}
		first_wait_tsk = pop_head(recv_wait_list);	// pop from the waiting list
		U8* buf = (U8 *)first_wait_tsk -> message_header;	// find the message header
		check = push_buffer(recv_ring_buf, buf);	// push the message to the ring buffer
		push_tail(first_wait_tsk, &ready_queue[priority2order(first_wait_tsk->prio)]);	// push the sending task back to the ready queue and change its state to READY
	}
	if(g_uart_mailbox_collect != 1){
		// not UART Handler running now
		push_head(gp_current_task, &ready_queue[priority2order(gp_current_task->prio)]);
		k_tsk_run_new();
	}
  return RTX_OK;
}

int k_mbx_ls(task_t *buf, size_t count) {
#ifdef DEBUG_0
    printf("k_mbx_ls: buf=0x%x, count=%u\r\n", buf, count);
#endif /* DEBUG_0 */
	if (buf == NULL) {
		errno = EFAULT;
		return RTX_ERR;
	}
	int with_mailbox = 0;
	for (task_t i = 0; i < MAX_TASKS; i++) {
		if (&g_tcbs[i] != NULL && g_tcbs[i].state != DORMANT && g_mailboxes[i].active == 1) {
			*(buf + with_mailbox) = i;
			with_mailbox++;
		}
		if (with_mailbox == count) {
			return count;
		}
	}
  return with_mailbox;
}

int k_mbx_get(task_t tid)
{
#ifdef DEBUG_0
    printf("k_mbx_get: tid=%u\r\n", tid);
#endif /* DEBUG_0 */
	if (&g_tcbs[tid] == NULL || g_tcbs[tid].state == DORMANT || g_mailboxes[tid].active != 1) {
		errno = ENOENT;
		return RTX_ERR;
	}
	size_t free_space = g_mailboxes[tid].memory_buffer.capacity -  g_mailboxes[tid].memory_buffer.current_size;
  return (int) free_space;
}
/*
 *===========================================================================
 *                             END OF FILE
 *===========================================================================
 */
