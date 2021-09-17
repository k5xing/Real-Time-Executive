/**
 * @brief The KCD Task Template File
 * @note  The file name and the function name can be changed
 * @see   k_tasks.h
 */

#include "uart_polling.h"
#include "printf.h"
#include "rtx.h"
#include "rtx_errno.h"
 typedef struct cmdnode {
	 struct cmdnode *next;
	 char command;
	 task_t command_handler_tid;
 } CMDNODE;
 
 typedef struct cmdlist {
	 struct cmdnode *head;
	 struct cmdnode *tail;
 } CMDLIST;
 
  typedef struct inputnode {
	 struct inputnode *next;
	 char character;
 } INPUTNODE;
 
 typedef struct inputqueue {
	 struct inputnode *head;
	 struct inputnode *tail;
 } INPUTQUEUE;
 
CMDLIST* cmd_reg_list;
CMDLIST cmd_reg_table;
INPUTQUEUE* input_queue;
INPUTQUEUE input_list;
U8 echo_buffer[sizeof(struct rtx_msg_hdr) + 1];
U8 display_buffer[sizeof(struct rtx_msg_hdr) + 1];

	
void task_kcd(void)
{
	
	mbx_t mbx_id = mbx_create(KCD_MBX_SIZE);  // create a mailbox for itself
	if(mbx_id != TID_KCD){
		printf("Warning: Mailbox ID is %d , not the same as TID_KCD(%d)", mbx_id, TID_KCD);
	}
	
	cmd_reg_table.head = NULL;
	cmd_reg_table.tail = NULL;
	
	cmd_reg_list = &cmd_reg_table;
	
	input_list.head = NULL;
	input_list.tail = NULL;
	
	input_queue = &input_list;
	
	char *input = mem_alloc(KCD_CMD_BUF_SIZE);
	if(input == NULL){
		printf("Error occured when allocating memory for input\n");
	}
	char cmd_id = '!';
	int cmd_found = 0;
	int cmd_invalid = 0;
	int cmd_length = 0;
	
	RTX_MSG_HDR *p_buf;
	char *buf = mem_alloc(KCD_MBX_SIZE);
		if(buf == NULL){
			printf("Error occured when allocating memory for buf\n");
		}
	while(TRUE){
		#ifdef DEBUG_0
		printf("kcd running\n");
		#endif /* DEBUG_0 */ 
		recv_msg(buf, KCD_MBX_SIZE);
			
		p_buf = (RTX_MSG_HDR *) buf;
		if(p_buf->type == KCD_REG){
			cmd_id = buf[(p_buf -> length - 1)];
			int cmd_exist = 0;
			if(cmd_reg_list -> head != NULL){
				CMDNODE *current_cmd_node;
				for(current_cmd_node = cmd_reg_list->head; current_cmd_node -> next != NULL; current_cmd_node = current_cmd_node -> next ){
					if(current_cmd_node->command == cmd_id){
						//Found existing cmd registration, update with new tid
						current_cmd_node ->command_handler_tid = p_buf->sender_tid;
						cmd_exist = 1;
						break;
					}
				}
				if(current_cmd_node->command == cmd_id){
					//Found existing cmd registration, update with new tid
					current_cmd_node ->command_handler_tid = p_buf->sender_tid;
					cmd_exist = 1;
				}
			}
			if(cmd_exist == 0){
				//New cmd registration
				CMDNODE *cmd_reg = mem_alloc(sizeof(CMDNODE));
				if(cmd_reg == NULL){
					printf("Error occured when allocating memory for cmd_reg\n");
				}else{
					cmd_reg ->command = cmd_id;
					cmd_reg ->command_handler_tid = p_buf ->sender_tid;
					cmd_reg ->next = NULL;
					if(cmd_reg_list -> head == NULL){
						cmd_reg_list -> head = cmd_reg;
						cmd_reg_list -> tail = cmd_reg_list -> head;
					}else{
						cmd_reg_list ->tail ->next = cmd_reg;
						cmd_reg_list ->tail = cmd_reg;
					}
				}
			}
			cmd_id = '!';
			
		}else{
			if(p_buf -> type == KEY_IN){
				//User Keyboard Input
				
				//Echo Input to DISPLAY
				size_t msg_hdr_size = sizeof(struct rtx_msg_hdr);
				U8  *echo_buf = &echo_buffer[0];                  // buffer is allocated by the caller */
				struct rtx_msg_hdr *ptr = (void *)echo_buf;
				
				ptr->length = msg_hdr_size + 1;         // set the message length
				ptr->type = DISPLAY;                    // set message type
				ptr->sender_tid = TID_KCD;                  // set sender id
				echo_buf += msg_hdr_size;
				char temp = buf[(p_buf -> length - 1)];
				*echo_buf = temp;  				// set message data
				#ifdef DEBUG_0
				printf("Echo character is %c\n\r", temp);
				#endif /* DEBUG_0 */
				int ret_val = 10;
				ret_val = send_msg(TID_CON, (void *)ptr);    // non-blocking send
				if(buf[(p_buf -> length - 1)] == '\r' ){
					// Enter Key Entered
				
					if(input_queue ->head != NULL){
							if(input_queue -> head -> character == '%'){
								//take out '%' from queue
								int rtv = -2;
								INPUTNODE *temp = input_queue -> head;
								input_queue -> head = temp -> next;
								rtv = mem_dealloc(temp);
								if(rtv != 0){
									printf("Error occured when deallocating memory for cmd_reg\r\n");
								}
								if(input_queue ->head != NULL){
									for(int i =0; i<KCD_CMD_BUF_SIZE; i++){
										//pop character
										input[i]=input_queue->head ->character;
										rtv = -2;
										INPUTNODE *temp = input_queue -> head;
										
										input_queue -> head = temp -> next;
										rtv = mem_dealloc(temp);
										if(rtv != 0){
											printf("Error occured when deallocating memory for cmd_reg\r\n");
										}
										cmd_length ++;
										if(input_queue -> head == NULL){
											input_queue -> tail = input_queue ->head;
											#ifdef DEBUG_0
											printf("Command Transfered into input buffer\r\n");
											#endif /* DEBUG_0 */
											break;
										}
									}
									#ifdef DEBUG_0
										printf("***Input String is %s\r\n",input);
									#endif /* DEBUG_0 */
									if(input_queue -> head != NULL){
										// the input is longer than the maximum length
										printf("Error: the input is longer than the maximum length\n");
										cmd_invalid = 1;
										
									}
									
									if(cmd_invalid == 0){
										//COMMAND Identifier Entered
										if(input[0] == 'L' && input[1] == 'T'){
											// %LT Command
											task_t tsk_nd_buffer = 0;
											int tsk_nd_count = 0;
											tsk_nd_count = tsk_ls(&tsk_nd_buffer, MAX_TASKS);
											if(tsk_nd_count != -1){
												char *tsk_info_string = mem_alloc((9+2 +20 + 1));
												int tsk_info_string_index = 0;
												RTX_TASK_INFO tsk_info_buffer;
												task_t current_tid = 0;
												int ret_get = -2;
												int tid_string_length = 0;
												char* tid_header_string = "Task ID: ";
												char* tsk_state_header_string;
												int tsk_state_header_string_length = 0;
												for(int i = 0; i< tsk_nd_count; i++){
													current_tid = *(&tsk_nd_buffer + i);
													ret_get = tsk_get(current_tid, &tsk_info_buffer);
													if(ret_get == RTX_OK){
														int current_tid_temp = current_tid;
														if(current_tid == 0){
															//Setting tid_string for NULL_TASK 
															tid_string_length = 1;
														}else{
															while(current_tid_temp != 0){
																current_tid_temp = current_tid_temp/10;
																tid_string_length ++;
															}
														}
														char* tid_string = mem_alloc(tid_string_length);
														current_tid_temp = current_tid;
														for (int y = tid_string_length-1; y >= 0; --y, current_tid_temp = current_tid_temp/ 10){
															tid_string[y] = (current_tid_temp % 10) + '0';
														}
														
														switch(tsk_info_buffer.state){
															case READY: tsk_state_header_string = " State: READY";
																tsk_state_header_string_length = 13;
																break;
															case RUNNING: tsk_state_header_string = " State: RUNNING";
																tsk_state_header_string_length = 15;
																break;
															case BLK_SEND: tsk_state_header_string = " State: BLK_SEND";
																tsk_state_header_string_length = 16;
																break;
															case BLK_RECV: tsk_state_header_string = " State: BLK_RECV";
																tsk_state_header_string_length = 16;
																break;
															case SUSPENDED: tsk_state_header_string = " State: SUSPENDED";
																tsk_state_header_string_length = 17;
																break;
															default: tsk_state_header_string = " State: UNSPECIFIED";
																tsk_state_header_string_length = 19;
																break;
														}
														
														// Concat the tsk_info_string together with TID and State values
														for(int j = 0; j < 8; j++){
															tsk_info_string[tsk_info_string_index] = tid_header_string[j];
															tsk_info_string_index ++;
														}
														
														for(int j = 0; j < tid_string_length; j++){
															tsk_info_string[tsk_info_string_index] = tid_string[j];
															tsk_info_string_index ++;
														}
														
														for(int j = 0; j < tsk_state_header_string_length; j++){
															tsk_info_string[tsk_info_string_index] = tsk_state_header_string[j];
															tsk_info_string_index ++;
														}
															
														tsk_info_string[tsk_info_string_index] = '\r';
																tsk_info_string_index ++;
														
														tsk_info_string[tsk_info_string_index] = '\n';
														tsk_info_string_index ++;
														
														char *cmd_buffer = mem_alloc(sizeof(struct rtx_msg_hdr) + tsk_info_string_index );
														int ret_val = 10;
														
														size_t msg_hdr_size = sizeof(struct rtx_msg_hdr);
														char  *cmd_buf = cmd_buffer;                  // buffer is allocated by the caller */
														struct rtx_msg_hdr *ptr = (void *)cmd_buf;
														
														ptr->length = msg_hdr_size + tsk_info_string_index;         // set the message length
														ptr->type = DISPLAY;                    // set message type
														ptr->sender_tid = TID_KCD;                  // set sender id
														cmd_buf += msg_hdr_size;
														for(int k = 0; k<tsk_info_string_index; k++){
																	cmd_buf[k] = tsk_info_string[k]; 	// set message data
														}
														#ifdef DEBUG_0
																 printf("KCD: tsk_info_string is %s\r\n", tsk_info_string);
																 printf("KCD: cmd_buf is %s\r\n", cmd_buf);
														#endif /* DEBUG_0 */ 
														ret_val = send_msg(TID_CON, (void *)ptr);    // non-blocking send
														ret_val = mem_dealloc(tid_string);
														if(ret_val != RTX_OK){
																printf("Error occured when deallocating tid_string\n");
														}
														ret_val = mem_dealloc(cmd_buffer);
														if(ret_val != RTX_OK){
																printf("Error occured when deallocating cmd_buffer\n");
														}
														tid_string_length = 0;
														tsk_info_string_index = 0;
													}else{
														printf("Error occured when tsk_get invoked\n");
													}
													
												}
												ret_val = mem_dealloc(tsk_info_string);
												if(ret_val != RTX_OK){
														printf("Error occured when deallocating cmd_buffer\n");
												}
											}else{
												printf("Error occured when tsk_ls was invoked\n");
											}
										}else{
											if(input[0] == 'L' && input[1] == 'M'){
												//%LM Command
												task_t mbx_nd_buffer = 0;
												int mbx_nd_count = 0;
												mbx_nd_count = mbx_ls(&mbx_nd_buffer, MAX_TASKS);
												if(mbx_nd_count != -1){
													int tsk_info_string_index = 0;
													RTX_TASK_INFO tsk_info_buffer;
													task_t current_tid = 0;
													int ret_get = -2;
													
													int tid_string_length = 0;
													char* tid_header_string = "Task ID: ";
													char* tsk_state_header_string;
													int tsk_state_header_string_length = 0;
													
													int mbx_free_space = 0;
													
													int mbx_free_space_string_length = 0;
													char *tsk_info_string = mem_alloc((9+2 +30 + 10 + 2));
														char *cmd_buffer = mem_alloc(sizeof(struct rtx_msg_hdr) + (8+2 +30 + 10 + 2) );
															
													for(int i = 0; i< mbx_nd_count; i++){
														current_tid = *(&mbx_nd_buffer + i);
														ret_get = tsk_get(current_tid, &tsk_info_buffer);
														if(ret_get == RTX_OK){
															
															mbx_free_space = mbx_get(current_tid);
															if(mbx_free_space >= 0){
																
																int current_tid_temp = current_tid;
																if(current_tid == 0){
																	//Setting tid_string for NULL_TASK 
																	tid_string_length = 1;
																}else{
																	while(current_tid_temp != 0){
																		current_tid_temp = current_tid_temp/10;
																		tid_string_length ++;
																	}
																}
																char* tid_string = mem_alloc(tid_string_length);
																current_tid_temp = current_tid;
																for (int x = tid_string_length-1; x >= 0; --x, current_tid_temp = current_tid_temp/ 10){
																	tid_string[x] = (current_tid_temp % 10) + '0';
																}
																int mbx_free_space_temp = mbx_free_space;
																while(mbx_free_space_temp != 0){
																	mbx_free_space_temp = mbx_free_space_temp/10;
																	mbx_free_space_string_length ++;
																}
																char* mbx_free_space_string = mem_alloc(mbx_free_space_string_length);
																mbx_free_space_temp = mbx_free_space;
																for (int x = mbx_free_space_string_length-1; x >= 0; --x, mbx_free_space_temp = mbx_free_space_temp/ 10){
																	mbx_free_space_string[x] = (mbx_free_space_temp % 10) + '0';
																}
																															
																switch(tsk_info_buffer.state){
																	case READY: tsk_state_header_string = " State: READY Free Space: ";
																		tsk_state_header_string_length = 26;
																		break;
																	case RUNNING: tsk_state_header_string = " State: RUNNING Free Space: ";
																		tsk_state_header_string_length = 28;
																		break;
																	case BLK_SEND: tsk_state_header_string = " State: BLK_SEND Free Space: ";
																		tsk_state_header_string_length = 29;
																		break;
																	case BLK_RECV: tsk_state_header_string = " State: BLK_RECV Free Space: ";
																		tsk_state_header_string_length = 28;
																		break;
																	case SUSPENDED: tsk_state_header_string = " State: SUSPENDED Free Space: ";
																		tsk_state_header_string_length = 30;
																		break;
																	default: tsk_state_header_string = " State: UNSPECIFIED\n";
																		tsk_state_header_string_length = 20;
																		break;
																}
																
																// Concat the tsk_info_string together with TID and State values
																for(int j = 0; j < 8; j++){
																	tsk_info_string[tsk_info_string_index] = tid_header_string[j];
																	tsk_info_string_index ++;
																}
																
																for(int j = 0; j < tid_string_length; j++){
																	tsk_info_string[tsk_info_string_index] = tid_string[j];
																	tsk_info_string_index ++;
																}
																
																for(int j = 0; j < tsk_state_header_string_length; j++){
																	tsk_info_string[tsk_info_string_index] = tsk_state_header_string[j];
																	tsk_info_string_index ++;
																}
																
																for(int j = 0; j < mbx_free_space_string_length; j++){
																	tsk_info_string[tsk_info_string_index] = mbx_free_space_string[j];
																	tsk_info_string_index ++;
																}
																tsk_info_string[tsk_info_string_index] = '\r';
																tsk_info_string_index ++;
																
																tsk_info_string[tsk_info_string_index] = '\n';
																tsk_info_string_index ++;
																
															
																int ret_val = 10;
															
																size_t msg_hdr_size = sizeof(struct rtx_msg_hdr);
																char  *cmd_buf = &cmd_buffer[0];                  // buffer is allocated by the caller */
																struct rtx_msg_hdr *ptr = (void *)cmd_buf;
																
																ptr->length = msg_hdr_size + tsk_info_string_index;         // set the message length
																ptr->type = DISPLAY;                    // set message type
																ptr->sender_tid = TID_KCD;                  // set sender id
																cmd_buf += msg_hdr_size;
																//*cmd_buf = *tsk_info_string;    
																for(int k = 0; k<tsk_info_string_index; k++){
																	cmd_buf[k] = tsk_info_string[k]; 	// set message data
																}
																#ifdef DEBUG_0
																 printf("KCD: tsk_info_string is %s\r\n", tsk_info_string);
																 printf("KCD: cmd_buf is %s\r\n", cmd_buf);
																#endif /* DEBUG_0 */ 
																ret_val = send_msg(TID_CON, (void *)ptr);    // non-blocking send
																ret_val = mem_dealloc(tid_string);
																if(ret_val != RTX_OK){
																		printf("Error occured when deallocating tid_string\n");
																}
																ret_val = mem_dealloc(mbx_free_space_string);
																if(ret_val != RTX_OK){
																		printf("Error occured when deallocating mbx_free_space_string\n");
																}
																tsk_info_string_index = 0;
																tid_string_length = 0;
																mbx_free_space_string_length = 0;
															}else{
																printf("Error occured when mbx_get invoked\n");
															}
														}else{
															printf("Error occured when tsk_get invoked\n");
														}
													}
													ret_val = mem_dealloc(tsk_info_string);
													if(ret_val != RTX_OK){
															printf("Error occured when deallocating tsk_info_string\n");
													}
													ret_val = mem_dealloc(cmd_buffer);
													if(ret_val != RTX_OK){
															printf("Error occured when deallocating cmd_buffer\n");
													}

												}else{
													printf("Error occured when tsk_ls was invoked\n");
												}
											}else{
												//normal commands
												task_t cmd_handler_id = TID_NULL;
												CMDNODE *current_cmd_node;
												for(current_cmd_node = cmd_reg_list->head; current_cmd_node -> next != NULL; current_cmd_node = current_cmd_node -> next ){
													if(current_cmd_node->command == input[0]){
														//Found tid of the task handling the inputted command
														cmd_handler_id = current_cmd_node->command_handler_tid;
														cmd_found = 1;
														break;
													}
												}
												if(current_cmd_node->command == input[0]){
													//Found tid of the task handling the inputted command
													cmd_handler_id = current_cmd_node->command_handler_tid;
													cmd_found = 1;
												}
												
												if(cmd_found == 1){
														//check if the command handler exists
														RTX_TASK_INFO cmd_tsk_buffer;
														int ret_get = 0;
														ret_get = tsk_get(cmd_handler_id, &cmd_tsk_buffer);
														if(ret_get != RTX_OK){
															printf("tsk_get failed when checking task handler is non-dormant\r\n");
														}
														if(cmd_tsk_buffer.state == DORMANT){
																printf("The Task Handler %d is in DORMANT\r\n", cmd_handler_id);
																cmd_found = 0;
														}
												}
												
												if(cmd_found == 0){
													char* display_message = "Command not found\r\n";
													int display_message_size = 19;
														size_t msg_hdr_size = sizeof(struct rtx_msg_hdr);
														U8  *display_buf = &display_buffer[0];                  // buffer is allocated by the caller */
														struct rtx_msg_hdr *ptr = (void *)display_buf;
														
														ptr->length = msg_hdr_size + display_message_size;         // set the message length
														ptr->type = DISPLAY;                    // set message type
														ptr->sender_tid = TID_KCD;                  // set sender id
														display_buf += msg_hdr_size;
														for(int k = 0; k<display_message_size; k++){
																	display_buf[k] = display_message[k]; 	// set message data
														}
														send_msg(TID_CON, (void *)ptr);    // non-blocking send
												}else{
													
													char *cmd_buffer = mem_alloc(sizeof(struct rtx_msg_hdr) + cmd_length );
													int ret_val = 10;
													
													size_t msg_hdr_size = sizeof(struct rtx_msg_hdr);
													char  *cmd_buf = &cmd_buffer[0];                  // buffer is allocated by the caller */
													struct rtx_msg_hdr *ptr = (void *)cmd_buf;
													
													ptr->length = msg_hdr_size + cmd_length;         // set the message length
													ptr->type = KCD_CMD;                    // set message type
													ptr->sender_tid = TID_KCD;                  // set sender id
													cmd_buf += msg_hdr_size;
														for(int k = 0; k<cmd_length; k++){
																	cmd_buf[k] = input[k]; 	// set message data
														}
													ret_val = send_msg(cmd_handler_id, (void *)ptr);    // non-blocking send
													ret_val = mem_dealloc(cmd_buffer);
													if(ret_val != RTX_OK){
															printf("Error occured when deallocating cmd_buffer\r\n");
													}

													cmd_found = 0;
													
												}
												cmd_length = 0;
											}
										}
									}
								}else{
									//command with just %
									#ifdef DEBUG_0
										printf("Error: command only has '%' \r\n");
									#endif /* DEBUG_0 */
								}
							}else{
								//command does not start with '%'
								#ifdef DEBUG_0
									printf("command does not start with '%' \r\n");
								#endif /* DEBUG_0 */
								cmd_invalid = 1;
							}
						}else{
							//command does not start with '%'
							#ifdef DEBUG_0
								printf("Empty Input Queue \r\n");
							#endif /* DEBUG_0 */
						}
					
					if(cmd_invalid == 1){
						//discard the remaining characters in the queue
						int ret = -2;
						while(input_queue -> head != NULL){
							//TODO: check if there is edge case in the end
							INPUTNODE *temp = input_queue -> head;
							input_queue -> head = temp -> next;
							ret = mem_dealloc(temp);
							if(ret != 0){
								printf("Error occured when deallocating memory for input_queue\n");
							}
						}
						char* display_message = "Invalid command\r\n";
						int display_message_size = 17;
						size_t msg_hdr_size = sizeof(struct rtx_msg_hdr);
						U8  *display_buf = &display_buffer[0];                  // buffer is allocated by the caller */
						struct rtx_msg_hdr *ptr = (void *)display_buf;
						
						ptr->length = msg_hdr_size + display_message_size;         // set the message length
						ptr->type = DISPLAY;                    // set message type
						ptr->sender_tid = TID_KCD;                  // set sender id
						display_buf += msg_hdr_size;
						for(int k = 0; k<display_message_size; k++){
									display_buf[k] = display_message[k]; 	// set message data
						}
						send_msg(TID_CON, (void *)ptr);    // non-blocking send
						cmd_invalid = 0;
					}
					
				}else{
					// enqueue the input charater
					INPUTNODE *input_char = mem_alloc(sizeof(INPUTNODE));
					if(input_char == NULL){
						printf("Error occured when allocating memory for cmd_reg\n");
					}else{
						input_char ->character = buf[(p_buf -> length - 1)];
						input_char ->next = NULL;
						if(input_queue -> head == NULL){
							input_queue -> head = input_char;
							input_queue -> tail = input_queue -> head;
						}else{
							input_queue ->tail ->next = input_char;
							input_queue -> tail = input_char;
						}
					}
				}
			
			}
			
		}
	}
		mem_dealloc(buf);
		mem_dealloc(input);
		int ret_dealloc = 0;
		while(cmd_reg_list -> head != NULL){
			
			CMDNODE *temp = cmd_reg_list -> head;
			cmd_reg_list -> head = temp -> next;
			ret_dealloc = mem_dealloc(temp);
			if(ret_dealloc != 0){
				printf("Error occured when deallocating memory for cmd_reg\n");
			}
		}
}

/*
 *===========================================================================
 *                             END OF FILE
 *===========================================================================
 */

