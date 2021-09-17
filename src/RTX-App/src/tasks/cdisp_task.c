/**
 * @brief The Console Display Task Template File 
 * @note  The file name and the function name can be changed
 * @see   k_tasks.h
 */
 
#include "k_rtx.h"
#include <LPC17xx.h>
#include "uart_polling.h"
#include "printf.h"
#include "rtx.h"

void task_cdisp(void)
{

		LPC_UART_TypeDef *pUart;
	
		mbx_t mbx_id = mbx_create(CON_MBX_SIZE);  // create a mailbox for itself
		if(mbx_id != TID_CON){
			 printf("Warning: Mailbox ID is %d , not the same as TID_KCD(%d)", mbx_id, TID_KCD);
		}
	
		 RTX_MSG_HDR *p_buf;
		 pUart = (LPC_UART_TypeDef *) LPC_UART0;
		 uint8_t *buf = mem_alloc(CON_MBX_SIZE);
		 if(buf == NULL){
								printf("Error occured when allocating memory for buf\n");
		 }
		 
		 while(1){
					#ifdef DEBUG_0
					printf("cdisp running\n");
					#endif /* DEBUG_0 */ 
					recv_msg(buf, CON_MBX_SIZE);
					
					p_buf = (RTX_MSG_HDR *) buf;
			
					if(p_buf ->type == DISPLAY){			
						int string_length = 0;
						uint8_t * uart_message;
						string_length = p_buf ->length - sizeof(struct rtx_msg_hdr);
						
						uart_message = buf + sizeof(struct rtx_msg_hdr);
						uint8_t *cmd_buffer = mem_alloc(string_length);
						
		
						int ret_val = 10; 
						size_t msg_hdr_size = sizeof(struct rtx_msg_hdr);
						uint8_t  *cmd_buf = cmd_buffer;                  // buffer is allocated by the caller 
						struct rtx_msg_hdr *ptr = (void *)cmd_buf;
								
						ptr->length = msg_hdr_size + string_length;         // set the message length
						ptr->type = DISPLAY;                    // set message type
						ptr->sender_tid = TID_CON;                  // set sender id 
						cmd_buf += msg_hdr_size;                        
						*cmd_buf = *uart_message;   												// set message data
						mem_cpy(cmd_buf,uart_message,string_length);
						#ifdef DEBUG_0
						printf("cdisp: uart_message is %s\n", uart_message);
						printf("cdisp: cmd_buf is %s \n", cmd_buf);
						#endif /* DEBUG_0 */ 
						ret_val = send_msg(TID_UART, (void *)ptr);    // blocking send 
						ret_val = mem_dealloc(cmd_buffer);
						if(ret_val != RTX_OK){
							printf("Error occured when deallocating cmd_buffer\n");
						}
						for(int i = 0; i <10000; i ++){
							//add some delays for RAM printing
						}
							pUart->IER = IER_THRE | IER_RLS | IER_RBR;
					} 				
		 }

}

/*
 *===========================================================================
 *                             END OF FILE
 *===========================================================================
 */

