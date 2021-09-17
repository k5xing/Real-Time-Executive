/*
 ****************************************************************************
 *
 *                  UNIVERSITY OF WATERLOO ECE 350 RTOS LAB
 *
 *                     Copyright 2020-2021 Yiqing Huang
 *                          All rights reserved.
 *
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
 ****************************************************************************
 */

/**************************************************************************//**
 * @file        k_msg.h
 * @brief       kernel message passing header file
 *
 * @version     V1.2021.06
 * @authors     Yiqing Huang
 * @date        2021 JUN
 *****************************************************************************/

 
#ifndef K_MSG_H_
#define K_MSG_H_

#include "k_inc.h"
#include "priority_queue.h"

typedef struct ring_buffer {
	U8* memory_start;													// points to the start of the buffer
	U8* memory_end;														// points to the end of the buffer
	size_t capacity;
	size_t current_size;
	U8* head;																	// keep track of read
	U8* tail;																	// keep track of write
} MB;

typedef struct mailbox {
	int			active;														// sets to 1 when it is in use, 0 otherwise
	task_t 	receiver_id;											// default value 0
	MB 			memory_buffer; 
	QUEUE		waiting_list;
} MAILBOX;

extern MAILBOX 			g_mailboxes[MAX_TASKS];
extern MAILBOX			UART_MAILBOX;
extern int					g_uart_mailbox_collect;

void mem_cpy				(U8* dest, U8* src, size_t cpy_length);
void mailboxes_init	(MAILBOX* mailbox);

int push_buffer			(MB* memory_buf, U8* buf);
int pop_buffer			(MB* memory_buf, U8* buf, size_t length);
int can_fit					(MB* memory_buf, size_t message_size);

int k_mbx_create    (size_t size);
int k_send_msg      (task_t receiver_tid, const void *buf);
int k_send_msg_nb   (task_t receiver_tid, const void *buf);
int k_recv_msg      (void *buf, size_t len);
int k_recv_msg_nb   (void *buf, size_t len);
int k_mbx_ls        (task_t *buf, size_t count);
int k_mbx_get       (task_t tid);

#endif // ! K_MSG_H_

/*
 *===========================================================================
 *                             END OF FILE
 *===========================================================================
 */
