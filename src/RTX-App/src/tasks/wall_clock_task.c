/**
 * @brief The Wall Clock Display Task Template File 
 * @note  The file name and the function name can be changed
 * @see   k_tasks.h
 */

#include "rtx.h"
#include "uart_polling.h"
#include "printf.h"
//#include "timer.h"

#define BUF_LEN 128
#define CLOCK_COLUMN_POS 40

U8 g_buf_cmd_reg[BUF_LEN];
U8 g_buf_asci_escape[8];
void update_clock_string(uint8_t* clock_string, int h, int m, int s){
		clock_string[0] = h/10 + '0';
		clock_string[1] = h%10 + '0';
		clock_string[2] = ':';
		clock_string[3] = m/10 + '0';
		clock_string[4] = m%10 + '0';
		clock_string[5] = ':';
		clock_string[6] = s/10 + '0';
		clock_string[7] = s%10 + '0';
		clock_string[8] = '\r';
	  clock_string[9] = '\n';
}

void update_clock_display(uint8_t* clock_string, int h, int m, int s){
		char * asci_escape_string = "\033[1;72f";
		update_clock_string(clock_string, h, m, s);
		int ret_val = 10;
		int clock_message_length = 10;
		int asci_escape_string_length = 7;
		uint8_t *clock_buffer = mem_alloc(clock_message_length);
										
		size_t msg_hdr_size = sizeof(struct rtx_msg_hdr);
		uint8_t  *clock_buf = &clock_buffer[0];                  // buffer is allocated by the caller */
		struct rtx_msg_hdr *ptr = (void *)clock_buf;
							
		ptr->length = msg_hdr_size + asci_escape_string_length + clock_message_length;         // set the message length
		ptr->type = DISPLAY;                    // set message type
		ptr->sender_tid = TID_WCLCK;                  // set sender id
		clock_buf += msg_hdr_size;
 
		for(int k = 0; k<asci_escape_string_length; k++){
			clock_buf[k] = asci_escape_string[k]; 	// set message data
		}

		for(int k = 0; k<clock_message_length; k++){
			clock_buf[k + asci_escape_string_length] = clock_string[k]; 	// set message data
		}
		#ifdef DEBUG_0
		 printf("WCLCK: clock_string is %s\r\n", clock_string);
		 printf("WCLCK: clock_buf is %s\r\n", clock_buf);
		#endif /* DEBUG_0 */ 
		ret_val = send_msg(TID_CON, (void *)ptr);    // non-blocking send
		if(ret_val != RTX_OK){
			printf("Error occured when sending clock_string\n");
		}
		ret_val = mem_dealloc(clock_buffer);
		if(ret_val != RTX_OK){
			printf("Error occured when deallocating clock_buffer\n");
		}
}

int verify_valid_input(uint8_t *input){
		/* Check if the user input is in the format of HH:MM:SS */
		/*	String Index															012345678 */
		if(input[0] != ' '){
				return -1;
		}
		
		if(input[1] < '0' || input [1] > '2'){
			//First Hour Digit
				return -1;
		}else{
				if(input[1] == '2' && input[2] > '3'){
					//Check if Hour exceeds 24 Hr
					return -1;
				}
		}
		
		if(input[3] != ':' || input[6] != ':'){
				return -1;
		}
		
		if(input[4] < '0' || input[4] >= '6' 
		|| input[7] < '0' || input[7] >= '6'){
			//Check if Minutes and Seconds exceeds 60
				return -1;
		}
		
		if(input[2] < '0' || input[2] > '9' 
		|| input[5] < '0' || input[5] > '9'
		|| input[8] < '0' || input[8] > '9'){
				return -1;
		}
		
		
		return RTX_OK;
		
}

int char_to_int(char key){
		int value = 0;
		value = key - '0';
		return value;
}

void task_wall_clock(void)
{
		mbx_t mbx_id = mbx_create(CON_MBX_SIZE);  // create a mailbox for itself
		if(mbx_id != TID_WCLCK){
			printf("Warning: Mailbox ID is %d , not the same as TID_WCLCK(%d)\r\n", mbx_id, TID_KCD);
		}
	  size_t msg_hdr_size = sizeof(struct rtx_msg_hdr);
    U8 *buf1 = &g_buf_cmd_reg[0];  // buffer is allocated by the caller */
    struct rtx_msg_hdr *ptr1 = (void *)buf1;
    ptr1->length = msg_hdr_size + 1;  // set the message length
    ptr1->type = KCD_REG;             // set message type
    ptr1->sender_tid = TID_WCLCK;  // set sender id
    buf1 += msg_hdr_size;
    *buf1 = 'W';

    int ret = send_msg(TID_KCD, (void *)ptr1);  // blocking send
    if (ret != RTX_OK) {
			printf("Error occured when TID_WCLCK tries to register %W Command\r\n");
		}
		int hours = 10;
		int minutes = 10;
		int seconds = 10;
		 
		int clock_visible=0;
		int clock_just_set =0;
		/* elevate wall_clock_task to real-time task and set its period to 1 second */
		
		TIMEVAL one_second;
		one_second.sec = 1;
		ret = rt_tsk_set(&one_second);
		if(ret != RTX_OK){
			printf("Error occured when TID_WCLCK tries to elevate itself to real-time task\r\n");
		}
		RTX_MSG_HDR *p_buf;
		uint8_t *buf = mem_alloc(CON_MBX_SIZE);
		if(buf == NULL){
			printf("Error occured when allocating memory for buf\r\n");
		}
		uint8_t * clock_string = mem_alloc(sizeof(char)*10);
		if(clock_string == NULL){
			printf("Error occured when allocating memory for clock_string\r\n");
		}
		while(1){
					#ifdef DEBUG_0
					printf("wall_clock running\r\n");
					#endif /* DEBUG_0 */ 
					
					ret = recv_msg_nb(buf, CON_MBX_SIZE);
					
					if(clock_visible == 1){
						//	if(clock_just_set == 0 ){
								seconds++;
								if(seconds >= 60){
										seconds = 0;
										minutes++;
								}
								
								if(minutes >= 60) {
										minutes = 0;
										hours++;
								}
								
								if(hours >= 24){
										hours = 0;
										minutes = 0;
										seconds = 0;
								}
								update_clock_display(clock_string,hours,minutes,seconds);
						// }else{
						//			clock_just_set = 0;
						// }
					}
			
					if(ret == RTX_OK){
					p_buf = (RTX_MSG_HDR *) buf;
					
				 if(p_buf ->type == KCD_CMD){
					/* Received Command Data from KCD */
					 #ifdef DEBUG_0
					  printf("buf[MSG_HDR_SIZE] is %c\r\n", buf[MSG_HDR_SIZE]);
					  printf("buf[MSG_HDR_SIZE+1] is %c\r\n", buf[MSG_HDR_SIZE+1]);
					  printf("buf[MSG_HDR_SIZE+2] is %c\r\n", buf[MSG_HDR_SIZE+2]);
					 #endif /* DEBUG_0 */ 
					 if(buf[MSG_HDR_SIZE+1] == 'R'){
					 /* %WR Clock Reset Command */
							  hours = 0;
							  minutes = 0;
							  seconds = 0;
							  update_clock_display(clock_string,hours,minutes,seconds);
								clock_visible = 1;
								clock_just_set = 1;
					 }else{
							if(buf[MSG_HDR_SIZE+1] == 'S'){
								/* %WS Clock Set Command */
									int string_length = 0;
									uint8_t * user_input;
									string_length = p_buf ->length - sizeof(struct rtx_msg_hdr);
									
								  //get the string after %WS
									user_input = buf + 1 + 1 + sizeof(struct rtx_msg_hdr);
									int ret = 10;
									ret = verify_valid_input(user_input);
									if(ret != RTX_OK || string_length  > 9 + sizeof(struct rtx_msg_hdr)){
										//printf("Invalid Time Value Entered, please enter time with HH:MM:SS format\r\n");
									}else{
										 /* Time format				 HH:MM:SS */
										/*	String Index			012345678 */
										 hours = char_to_int(user_input[1])*10 + char_to_int(user_input[2]);
										 minutes = char_to_int(user_input[4])*10 + char_to_int(user_input[5]);
										 seconds = char_to_int(user_input[7])*10 + char_to_int(user_input[8]);
										 update_clock_display(clock_string,hours,minutes,seconds);
										 clock_visible = 1;
										 clock_just_set = 1;
									}
									
							}else{
								if(buf[MSG_HDR_SIZE+1] == 'T'){
									/* %WT Clock Remove Command */
									clock_visible = 0;
									clock_just_set = 0;
									char * asci_escape_string = "\033[1;72f";
									int ret_val = 10;
									int clock_message_length = 10;
									int asci_escape_string_length = 7;
									uint8_t *clock_buffer = mem_alloc(clock_message_length);
																	
									size_t msg_hdr_size = sizeof(struct rtx_msg_hdr);
									uint8_t  *clock_buf = &clock_buffer[0];                  // buffer is allocated by the caller */
									struct rtx_msg_hdr *ptr = (void *)clock_buf;
														
									ptr->length = msg_hdr_size + asci_escape_string_length + clock_message_length;         // set the message length
									ptr->type = DISPLAY;                    // set message type
									ptr->sender_tid = TID_WCLCK;                  // set sender id
									clock_buf += msg_hdr_size;
							 
									for(int k = 0; k<asci_escape_string_length; k++){
										clock_buf[k] = asci_escape_string[k]; 	// set message data
									}
							
									for(int k = 0; k<clock_message_length-2; k++){
										clock_buf[k + asci_escape_string_length] = ' '; 	// set message data
									}
									clock_buf[asci_escape_string_length + clock_message_length - 2] = '\r';
									clock_buf[asci_escape_string_length + clock_message_length - 1] = '\n';
									#ifdef DEBUG_0
									 printf("WCLCK: clock_string is %s\r\n", clock_string);
									 printf("WCLCK: clock_buf is %s\r\n", clock_buf);
									#endif /* DEBUG_0 */ 
									ret_val = send_msg(TID_CON, (void *)ptr);    // non-blocking send
									if(ret_val != RTX_OK){
										printf("Error occured when sending clock_string\n");
									}
									ret_val = mem_dealloc(clock_buffer);
									if(ret_val != RTX_OK){
										printf("Error occured when deallocating clock_buffer\n");
									}
								}
							}
					 }
					 
				 }
			 }
				 	
					rt_tsk_susp();
			 }
	 
}

/*
 *===========================================================================
 *                             END OF FILE
 *===========================================================================
 */

