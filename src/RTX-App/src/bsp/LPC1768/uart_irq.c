/*
 ****************************************************************************
 *
 *                  UNIVERSITY OF WATERLOO SE 350 RTX LAB  
 *
 *        Copyright 2020-2021 Yiqing Huang and NXP Semiconductors
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
 * @file        uart_irq.c
 * @brief       UART IRQ handler. It receives input char through RX interrupt
 *              and then writes a string containing the input char through
 *              TX interrupts. 
 *              
 * @version     V1.2021.06
 * @authors     Yiqing Huang and NXP Semiconductors
 * @date        2021 JUN
 *****************************************************************************/

#include "k_inc.h"
#include "k_rtx.h"


uint8_t g_buffer[]= "Task ID  State: SUSPENDED Free Space:         ";
uint8_t *gp_buffer = g_buffer;
uint8_t g_send_char = 0;
uint8_t g_char_in;
uint8_t g_char_out;
uint8_t g_switch_flag = FALSE;
int			g_uart_mailbox_collect = 0;
int 		msg_index = 0;
U8 key_buffer[sizeof(struct rtx_msg_hdr) + 1];

/**************************************************************************//**
 * @brief   initializes the n_uart interrupts
 * @note    it only supports UART0. It can be easily extended to support UART1 IRQ.
 * The step number in the comments matches the item number in Section 14.1 on pg 298
 * of LPC17xx_UM
 *****************************************************************************/
int uart_irq_init(int n_uart) {

    LPC_UART_TypeDef *pUart;

    if ( n_uart ==0 ) {
        /*
        Steps 1 & 2: system control configuration.
        Under CMSIS, system_LPC17xx.c does these two steps
         
        -----------------------------------------------------
        Step 1: Power control configuration. 
                See table 46 pg63 in LPC17xx_UM
        -----------------------------------------------------
        Enable UART0 power, this is the default setting
        done in system_LPC17xx.c under CMSIS.
        Enclose the code for your refrence
        //LPC_SC->PCONP |= BIT(3);
    
        -----------------------------------------------------
        Step2: Select the clock source. 
               Default PCLK=CCLK/4 , where CCLK = 100MHZ.
               See tables 40 & 42 on pg56-57 in LPC17xx_UM.
        -----------------------------------------------------
        Check the PLL0 configuration to see how XTAL=12.0MHZ 
        gets to CCLK=100MHZin system_LPC17xx.c file.
        PCLK = CCLK/4, default setting after reset.
        Enclose the code for your reference
        //LPC_SC->PCLKSEL0 &= ~(BIT(7)|BIT(6));    
            
        -----------------------------------------------------
        Step 5: Pin Ctrl Block configuration for TXD and RXD
                See Table 79 on pg108 in LPC17xx_UM.
        -----------------------------------------------------
        Note this is done before Steps3-4 for coding purpose.
        */
        
        /* Pin P0.2 used as TXD0 (Com0) */
        LPC_PINCON->PINSEL0 |= (1 << 4);  
        
        /* Pin P0.3 used as RXD0 (Com0) */
        LPC_PINCON->PINSEL0 |= (1 << 6);  

        pUart = (LPC_UART_TypeDef *) LPC_UART0;     
        
    } else if ( n_uart == 1) {
        
        /* see Table 79 on pg108 in LPC17xx_UM */ 
        /* Pin P2.0 used as TXD1 (Com1) */
        LPC_PINCON->PINSEL4 |= (2 << 0);

        /* Pin P2.1 used as RXD1 (Com1) */
        LPC_PINCON->PINSEL4 |= (2 << 2);          

        pUart = (LPC_UART_TypeDef *) LPC_UART1;
        
    } else {
        return 1; /* not supported yet */
    } 
    
    /*
    -----------------------------------------------------
    Step 3: Transmission Configuration.
            See section 14.4.12.1 pg313-315 in LPC17xx_UM 
            for baud rate calculation.
    -----------------------------------------------------
    */
    
    /* Step 3a: DLAB=1, 8N1 */
    pUart->LCR = UART_8N1; /* see uart.h file */ 

    /* Step 3b: 115200 baud rate @ 25.0 MHZ PCLK */
    pUart->DLM = 0; /* see table 274, pg302 in LPC17xx_UM */
    pUart->DLL = 9;    /* see table 273, pg302 in LPC17xx_UM */
    
    /* FR = 1.507 ~ 1/2, DivAddVal = 1, MulVal = 2
       FR = 1.507 = 25MHZ/(16*9*115200)
       see table 285 on pg312 in LPC_17xxUM
    */
    pUart->FDR = 0x21;       
    
 

    /*
    ----------------------------------------------------- 
    Step 4: FIFO setup.
           see table 278 on pg305 in LPC17xx_UM
    -----------------------------------------------------
        enable Rx and Tx FIFOs, clear Rx and Tx FIFOs
    Trigger level 0 (1 char per interrupt)
    */
    
    pUart->FCR = 0x07;

    /* Step 5 was done between step 2 and step 4 a few lines above */

    /*
    ----------------------------------------------------- 
    Step 6 Interrupt setting and enabling
    -----------------------------------------------------
    */
    /* Step 6a: 
       Enable interrupt bit(s) wihtin the specific peripheral register.
           Interrupt Sources Setting: RBR, THRE or RX Line Stats
       See Table 50 on pg73 in LPC17xx_UM for all possible UART0 interrupt sources
       See Table 275 on pg 302 in LPC17xx_UM for IER setting 
    */
    /* disable the Divisior Latch Access Bit DLAB=0 */
    pUart->LCR &= ~(BIT(7)); 
    
    /* enable RBR and RLS interrupts */
    pUart->IER = IER_RBR | IER_RLS; 
    
    /* Step 6b: set up UART0 IRQ priority */    
    NVIC_SetPriority(UART0_IRQn, 0x10);
    
    /* Step 6c: enable the UART interrupt from the system level */
    
    if ( n_uart == 0 ) {
        NVIC_EnableIRQ(UART0_IRQn); /* CMSIS function */
    } else if ( n_uart == 1 ) {
        NVIC_EnableIRQ(UART1_IRQn); /* CMSIS function */
    } else {
        return 1; /* not supported yet */
    }
    pUart->THR = '\0';
    return 0;
}

/**
 * @brief: CMSIS ISR for UART0 IRQ Handler
 */

void UART0_IRQHandler(void)
{
    uint8_t IIR_IntId;        /* Interrupt ID from IIR */          
    LPC_UART_TypeDef *pUart = (LPC_UART_TypeDef *)LPC_UART0;
		
		uint8_t *buf = k_mpool_alloc(MPID_IRAM1,UART_MBX_SIZE);
		if(buf == NULL){
				printf("Error occured when allocating memory for buf\n");
		}
		
#ifdef DEBUG_0
    uart1_put_string("Entering c_UART0_IRQHandler\n\r");
#endif // DEBUG_0

    /* Reading IIR automatically acknowledges the interrupt */
    IIR_IntId = (pUart->IIR) >> 1 ; /* skip pending bit in IIR */ 
    if (IIR_IntId & IIR_RDA) { /* Receive Data Avaialbe */
        /* Read UART. Reading RBR will clear the interrupt */
        g_char_in = pUart->RBR;
#ifdef DEBUG_0
        uart1_put_string("Reading a char = ");
        uart1_put_char(g_char_in);
        uart1_put_string("\n\r");
#endif /* DEBUG_0 */        
       
				size_t msg_hdr_size = sizeof(struct rtx_msg_hdr);
				U8  *key_buf = &key_buffer[0];                  // buffer is allocated by the caller */
				struct rtx_msg_hdr *ptr = (void *)key_buf;
												
				ptr->length = msg_hdr_size + 1;         // set the message length
				ptr->type = KEY_IN;                    // set message type
				ptr->sender_tid = TID_UART;                  // set sender id
				key_buf += msg_hdr_size;
				*key_buf = g_char_in;                             // set message data
				
				int ret = 0;
				ret = k_send_msg_nb(TID_KCD, (void *)ptr); 
				if(ret != RTX_OK){
					printf("Error occured when uart_irq send message nb \n");
				}
        g_send_char = 1;
       

    } else if (IIR_IntId & IIR_THRE) {
        /* THRE Interrupt, transmit holding register becomes empty */
				g_uart_mailbox_collect = 1;
		
				int ret_rec = 0;
				int msg_length = 0;
				if(msg_index == 0){
					ret_rec = k_recv_msg_nb(buf, UART_MBX_SIZE);
					if(ret_rec != RTX_OK){
						printf("Error occured when uart_irq receive message nb");
					}
				
				
				RTX_MSG_HDR *p_buf;
				p_buf = (RTX_MSG_HDR *) buf;
				msg_length = p_buf ->length - sizeof(struct rtx_msg_hdr);
				uint8_t *message = buf + sizeof(struct rtx_msg_hdr);
				mem_cpy(gp_buffer, message, msg_length);
				#ifdef DEBUG_0
					printf("gp_buffer is %s\n", gp_buffer);
				#endif /* DEBUG_0 */
				}
				
				g_uart_mailbox_collect = 0;

        while(msg_index <msg_length){ 
					 
					g_char_out = *gp_buffer;
					 
#ifdef DEBUG_0
            /*uart1_put_string("Writing a char = ");
              uart1_put_char(g_char_out);
              uart1_put_string("\n\r"); 
            */
            /* you could use the printf instead */
            printf("Writing a char = %c \n\r", g_char_out);
#endif /* DEBUG_0 */            
					 while ( !(pUart->LSR & LSR_THRE) ); // spin while waiting for FIFO to be totally empty (could optimize by not needing to check if fewer than [FIFO_DEPTH] chars are going to be sent)

            pUart->THR = g_char_out;
						msg_index++;
            gp_buffer++;
				
				 }
     
#ifdef DEBUG_0
            uart1_put_string("Finish writing. Turning off IER_THRE\n\r");
#endif /* DEBUG_0 */
            pUart->IER ^= IER_THRE; // toggle the IER_THRE bit 
            pUart->THR = '\0';
            g_send_char = 0;
						msg_index = 0;
            gp_buffer = g_buffer;        
        
    } else {  /* not implemented yet */
#ifdef DEBUG_0
            uart1_put_string("Should not get here!\n\r");
#endif /* DEBUG_0 */
        return;
    }    
		int ret = 0;
    ret = k_mpool_dealloc(MPID_IRAM1,buf);
		if(ret != RTX_OK){
				printf("Error occured when deallocating memory for buf\n");
		}
		
    // when interrupt handling is done, if new scheduling decision is made
    // then do context switching to the newly selected task
		
}
/*
 *===========================================================================
 *                             END OF FILE
 *===========================================================================
 */
