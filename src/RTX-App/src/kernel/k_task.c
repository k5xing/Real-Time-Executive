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
 * @file        k_task.c
 * @brief       task management C file
 * @version     V1.2021.05
 * @authors     Yiqing Huang
 * @date        2021 MAY
 *
 * @attention   assumes NO HARDWARE INTERRUPTS
 * @details     The starter code shows one way of implementing context switching.
 *              The code only has minimal sanity check.
 *              There is no stack overflow check.
 *              The implementation assumes only three simple tasks and
 *              NO HARDWARE INTERRUPTS.
 *              The purpose is to show how context switch could be done
 *              under stated assumptions.
 *              These assumptions are not true in the required RTX Project!!!
 *              Understand the assumptions and the limitations of the code before
 *              using the code piece in your own project!!!
 *
 *****************************************************************************/


#include "k_inc.h"
//#include "k_task.h"
#include "k_rtx.h"
#include <LPC17xx.h>
#include <system_LPC17xx.h>
#include "timer.h"
#include "uart_polling.h" 
#include "printf.h"

/*
 *==========================================================================
 *                            GLOBAL VARIABLES
 *==========================================================================
 */

TCB             *gp_current_task = NULL;    // the current RUNNING task
TCB             g_tcbs[MAX_TASKS];          // an array of TCBs
//TASK_INIT       g_null_task_info;           // The null task info
U32             g_num_active_tasks = 0;     // number of non-dormant tasks

QUEUE						ready_queue [5];
MAILBOX					UART_MAILBOX;

extern volatile uint32_t g_timer_count;
TM_TICK tk ={0, 0};
int retval;
QUEUE suspend_tsk;



/*---------------------------------------------------------------------------
The memory map of the OS image may look like the following:
                   RAM1_END-->+---------------------------+ High Address
                              |                           |
                              |                           |
                              |       MPID_IRAM1          |
                              |   (for user space heap  ) |
                              |                           |
                 RAM1_START-->|---------------------------|
                              |                           |
                              |  unmanaged free space     |
                              |                           |
&Image$$RW_IRAM1$$ZI$$Limit-->|---------------------------|-----+-----
                              |         ......            |     ^
                              |---------------------------|     |
                              |      PROC_STACK_SIZE      |  OS Image
              g_p_stacks[2]-->|---------------------------|     |
                              |      PROC_STACK_SIZE      |     |
              g_p_stacks[1]-->|---------------------------|     |
                              |      PROC_STACK_SIZE      |     |
              g_p_stacks[0]-->|---------------------------|     |
                              |   other  global vars      |     |
                              |                           |  OS Image
                              |---------------------------|     |
                              |      KERN_STACK_SIZE      |     |                
             g_k_stacks[15]-->|---------------------------|     |
                              |                           |     |
                              |     other kernel stacks   |     |                              
                              |---------------------------|     |
                              |      KERN_STACK_SIZE      |  OS Image
              g_k_stacks[2]-->|---------------------------|     |
                              |      KERN_STACK_SIZE      |     |                      
              g_k_stacks[1]-->|---------------------------|     |
                              |      KERN_STACK_SIZE      |     |
              g_k_stacks[0]-->|---------------------------|     |
                              |   other  global vars      |     |
                              |---------------------------|     |
                              |        TCBs               |  OS Image
                      g_tcbs->|---------------------------|     |
                              |        global vars        |     |
                              |---------------------------|     |
                              |                           |     |          
                              |                           |     |
                              |        Code + RO          |     |
                              |                           |     V
                 IRAM1_BASE-->+---------------------------+ Low Address
    
---------------------------------------------------------------------------*/ 

/*
 *===========================================================================
 *                            FUNCTIONS
 *===========================================================================
 */

/**************************************************************************//**
 * @brief   	SVC Handler
 * @pre         PSP is used in thread mode before entering SVC Handler
 *              SVC_Handler is configured as the highest interrupt priority
 *****************************************************************************/

void SVC_Handler(void)
{

    U8   svc_number;
    U32  ret  = RTX_OK;                 // default return value of a function
    U32 *args = (U32 *) __get_PSP();    // read PSP to get stacked args
    
    svc_number = ((S8 *) args[6])[-2];  // Memory[(Stacked PC) - 2]
    switch(svc_number) {
        case SVC_RTX_INIT:
            ret = k_rtx_init((RTX_SYS_INFO*) args[0], (TASK_INIT *) args[1], (int) args[2]);
            break;
        case SVC_MEM_ALLOC:
            ret = (U32) k_mpool_alloc(MPID_IRAM1, (size_t) args[0]);
            break;
        case SVC_MEM_DEALLOC:
            ret = k_mpool_dealloc(MPID_IRAM1, (void *)args[0]);
            break;
        case SVC_MEM_DUMP:
            ret = k_mpool_dump(MPID_IRAM1);
            break;
        case SVC_TSK_CREATE:
            ret = k_tsk_create((task_t *)(args[0]), (void (*)(void))(args[1]), (U8)(args[2]), (U32) (args[3]));
            break;
        case SVC_TSK_EXIT:
            k_tsk_exit();
            break;
        case SVC_TSK_YIELD:
            ret = k_tsk_yield();
            break;
        case SVC_TSK_SET_PRIO:
            ret = k_tsk_set_prio((task_t) args[0], (U8) args[1]);
            break;
        case SVC_TSK_GET:
            ret = k_tsk_get((task_t ) args[0], (RTX_TASK_INFO *) args[1]);
            break;
        case SVC_TSK_GETTID:
            ret = k_tsk_gettid();
            break;
        case SVC_TSK_LS:
            ret = k_tsk_ls((task_t *) args[0], (size_t) args[1]);
            break;
        case SVC_MBX_CREATE:
            ret = k_mbx_create((size_t) args[0]);
            break;
        case SVC_MBX_SEND:
            ret = k_send_msg((task_t) args[0], (const void *) args[1]);
            break;
        case SVC_MBX_SEND_NB:
            ret = k_send_msg_nb((task_t) args[0], (const void *) args[1]);
            break;
        case SVC_MBX_RECV:
            ret = k_recv_msg((void *) args[0], (size_t) args[1]);
            break;
        case SVC_MBX_RECV_NB:
            ret = k_recv_msg_nb((void *) args[0], (size_t) args[1]);
            break;
        case SVC_MBX_LS:
            ret = k_mbx_ls((task_t *) args[0], (size_t) args[1]);
            break;
        case SVC_MBX_GET:
            ret = k_mbx_get((task_t) args[0]);
            break;
        case SVC_RT_TSK_SET:
            ret = k_rt_tsk_set((TIMEVAL*) args[0]);
            break;
        case SVC_RT_TSK_SUSP:
            ret = k_rt_tsk_susp();
            break;
        case SVC_RT_TSK_GET:
            ret = k_rt_tsk_get((task_t) args[0], (TIMEVAL *) args[1]);
            break;
        default:
            ret = (U32) RTX_ERR;
    }

    args[0] = ret;      // return value saved onto the stacked R0
}

/**************************************************************************//**
 * @brief   scheduler, pick the TCB of the next to run task
 *
 * @return  TCB pointer of the next to run task
 * @post    gp_curret_task is updated
 * @note    you need to change this one to be a priority scheduler
 *
 *****************************************************************************/
//helper function to verify errno in test cases
int get_errno(){
	return errno;
}

int priority2order(U8 priority)
{
  int order = -1;
  switch(priority)
  {
		case PRIO_RT:
			order = RT_INDEX;
			break;
    case HIGH:
      order = HIGH_INDEX;
      break;
    case MEDIUM:
      order = MEDIUM_INDEX;
      break;
    case LOW:
      order = LOW_INDEX;
      break;
    case LOWEST:
      order = LOWEST_INDEX;
      break;
  case PRIO_NULL:
   order = NULL_INDEX;
  break;
    default:
      break;
  }
  return order;
  
}

U8 order2priority(int order)
{
  U8 priority = 0x79;
  switch(order)
  {
		case RT_INDEX:
			priority = PRIO_RT;
			break;
    case HIGH_INDEX:
      priority = HIGH;
      break;
    case MEDIUM_INDEX:
      priority = MEDIUM;
      break;
    case LOW_INDEX:
      priority = LOW;
      break;
    case LOWEST_INDEX:
      priority = LOWEST;
      break;
		case NULL_INDEX:
			priority = PRIO_NULL;
			break;
    default:
      break;
  }
  return priority;
  
}

/**************************************************************************//**
 * @brief   scheduler, pick the TCB of the next to run task
 *
 * @return  TCB pointer of the next to run task
 * @post    gp_curret_task is updated
 * @note    you need to change this one to be a priority scheduler
 *
 *****************************************************************************/
// always chooses the highest priority in a FCFI order
TCB *scheduler(void)
{
	
    for (int i = 0; i < 5; i++) {
			if (ready_queue[i].HEAD != NULL) { // if the highest prioirty contains ready task
          return pop_head(&ready_queue[i]);                            // pop the first one in the queue
      }
		
    }
    return &g_tcbs[TID_NULL];                                         // return NULL if nothing in the list
}
TCB *scheduler_nrt(void)
{
	
    for (int i = 1; i < 5; i++) {
			if (ready_queue[i].HEAD != NULL) { // if the highest prioirty contains ready task
          return pop_head(&ready_queue[i]);                            // pop the first one in the queue
      }
		
    }
    return &g_tcbs[TID_NULL];                                         // return NULL if nothing in the list
}

/**
 * @brief initialzie the first task in the system
 */
void k_tsk_init_first(TASK_INIT *p_task)
{
    p_task->prio         = PRIO_NULL;
    p_task->priv         = 0;
    p_task->tid          = TID_NULL;
    p_task->ptask        = &task_null;
    p_task->u_stack_size = PROC_STACK_SIZE;
}

/**************************************************************************//**
 * @brief       initialize all boot-time tasks in the system,
 *
 *
 * @return      RTX_OK on success; RTX_ERR on failure
 * @param       task_info   boot-time task information structure pointer
 * @param       num_tasks   boot-time number of tasks
 * @pre         memory has been properly initialized
 * @post        none
 * @see         k_tsk_create_first
 * @see         k_tsk_create_new
 *****************************************************************************/

int k_tsk_init(TASK_INIT *task, int num_tasks)
{
		for (int j = 0; j < 5; j++) {
				ready_queue[j].HEAD = NULL;
				ready_queue[j].TAIL = NULL;
				ready_queue[j].current_size = 0;
		}
		suspend_tsk.HEAD = NULL;
		suspend_tsk.TAIL = NULL;
		suspend_tsk.current_size = 0;
		
    if (num_tasks > MAX_TASKS - 3) {
        return RTX_ERR;
    }
    
    TASK_INIT taskinfo;
		TASK_INIT taskinfo_kcd;
		TASK_INIT taskinfo_con;
		TASK_INIT taskinfo_wclck;
		
		//init global variable (RT)
		g_timer_count = 0;
		retval = 0;
		
		//init timer1
		SystemInit();
		__disable_irq();
		timer_irq_init(TIMER0);
		timer_freerun_init(TIMER1);
		uart1_init();   
    init_printf(NULL, putc);
    __enable_irq();
		
    k_tsk_init_first(&taskinfo);
		
		taskinfo_con.prio         = HIGH;
    taskinfo_con.priv         = 1;
    taskinfo_con.tid          = TID_CON;
    taskinfo_con.ptask        = &task_cdisp;
    taskinfo_con.u_stack_size = PROC_STACK_SIZE;
		
		
		taskinfo_kcd.prio         = HIGH;
    taskinfo_kcd.priv         = 0;
    taskinfo_kcd.tid          = TID_KCD;
    taskinfo_kcd.ptask        = &task_kcd;
    taskinfo_kcd.u_stack_size = PROC_STACK_SIZE;
		
		taskinfo_wclck.prio         = HIGH;
    taskinfo_wclck.priv         = 0;
    taskinfo_wclck.tid          = TID_WCLCK;
    taskinfo_wclck.ptask        = &task_wall_clock;
    taskinfo_wclck.u_stack_size = PROC_STACK_SIZE;
		
    if ( k_tsk_create_new(&taskinfo, &g_tcbs[TID_NULL], TID_NULL) == RTX_OK ) {
        g_num_active_tasks = 1;
        gp_current_task = &g_tcbs[TID_NULL];
    } else {
        g_num_active_tasks = 0;
        return RTX_ERR;
    }	
		
    if ( k_tsk_create_new(&taskinfo_con, &g_tcbs[TID_CON], TID_CON) == RTX_OK ) {
        g_num_active_tasks++;
			//	gp_current_task = &g_tcbs[TID_CON];
				push_tail(&g_tcbs[TID_CON],&ready_queue[priority2order(taskinfo_con.prio)] );
				
    } else {
        g_num_active_tasks = 0;
        return RTX_ERR;
    }
		
		
		if ( k_tsk_create_new(&taskinfo_kcd, &g_tcbs[TID_KCD], TID_KCD) == RTX_OK ) {
        g_num_active_tasks++;
		//		gp_current_task = &g_tcbs[TID_KCD];
				push_tail(&g_tcbs[TID_KCD],&ready_queue[priority2order(taskinfo_kcd.prio)] );
    } else {
        g_num_active_tasks = 0;
        return RTX_ERR;
    }
		
			if ( k_tsk_create_new(&taskinfo_wclck, &g_tcbs[TID_WCLCK], TID_WCLCK) == RTX_OK ) {
        g_num_active_tasks++;
		//		gp_current_task = &g_tcbs[TID_WCLCK];
				push_tail(&g_tcbs[TID_WCLCK],&ready_queue[priority2order(taskinfo_wclck.prio)] );
    } else {
        g_num_active_tasks = 0;
        return RTX_ERR;
    }
		
		U8* mem_alloc = k_mpool_alloc(MPID_IRAM1, UART_MBX_SIZE);
		if (mem_alloc == NULL) {
			errno = ENOMEM;
			return RTX_ERR;
		}
	
		MAILBOX* new_mailbox = &UART_MAILBOX;
		MB* new_buffer = &(new_mailbox -> memory_buffer);
		QUEUE* waiting_list = &(new_mailbox -> waiting_list);
		new_mailbox -> active = 1;
		new_mailbox -> receiver_id = TID_UART;
		new_buffer -> capacity = UART_MBX_SIZE;
		new_buffer -> current_size = 0;
		new_buffer -> memory_start = mem_alloc;
		new_buffer -> memory_end = mem_alloc + UART_MBX_SIZE;
		new_buffer -> head = mem_alloc;
		new_buffer -> tail = mem_alloc;
		waiting_list -> HEAD = NULL;
		waiting_list -> TAIL = NULL;
		waiting_list -> current_size = 0;
		
    // create the rest of the tasks
    for ( int i = 0; i < num_tasks; i++ ) {
        TCB *p_tcb = &g_tcbs[i+1];
        if (k_tsk_create_new(&task[i], p_tcb, i+1) == RTX_OK) {
            g_num_active_tasks++;
						push_tail(p_tcb,&ready_queue[priority2order(p_tcb->prio)]);
        }
    }

		
		gp_current_task = scheduler();
    gp_current_task -> state = RUNNING;
		
    return RTX_OK;
}
/**************************************************************************//**
 * @brief       initialize a new task in the system,
 *              one dummy kernel stack frame, one dummy user stack frame
 *
 * @return      RTX_OK on success; RTX_ERR on failure
 * @param       p_taskinfo  task initialization structure pointer
 * @param       p_tcb       the tcb the task is assigned to
 * @param       tid         the tid the task is assigned to
 *
 * @details     From bottom of the stack,
 *              we have user initial context (xPSR, PC, SP_USR, uR0-uR3)
 *              then we stack up the kernel initial context (kLR, kR4-kR12, PSP, CONTROL)
 *              The PC is the entry point of the user task
 *              The kLR is set to SVC_RESTORE
 *              20 registers in total
 * @note        YOU NEED TO MODIFY THIS FILE!!!
 *****************************************************************************/
int k_tsk_create_new(TASK_INIT *p_taskinfo, TCB *p_tcb, task_t tid)
{
		
    extern U32 SVC_RTE;

    U32 *usp;
    U32 *ksp;

    if (p_taskinfo == NULL || p_tcb == NULL)
    {
        return RTX_ERR;
    }

   
    /*---------------------------------------------------------------
     *  Step1: allocate user stack for the task
     *         stacks grows down, stack base is at the high address
     * ATTENTION: you need to modify the following three lines of code
     *            so that you use your own dynamic memory allocator
     *            to allocate variable size user stack.
     * -------------------------------------------------------------*/
		//printf("Before Allocation:\n");
		p_taskinfo ->u_stack_size = POWER2(LOG2(p_taskinfo ->u_stack_size));
		p_taskinfo -> tid = tid;
//		if(tid != TID_CON){

    p_tcb->psp_base = k_mpool_alloc(MPID_IRAM2, p_taskinfo ->u_stack_size);             // ***you need to change this line***
		
		if (p_tcb->psp_base == NULL) {
				#ifdef DEBUG_0
        printf("Error: There is not enough memory to support operation \n");
				#endif /* DEBUG_0 */
				errno = ENOMEM;
        return RTX_ERR;
    }
		//}else{
			// p_tcb->psp_base = k_alloc_p_stack(TID_CON);
		//	 p_tcb->psp_base = k_mpool_alloc(MPID_IRAM1, p_taskinfo ->u_stack_size);       
	//	}
		usp = (U32 *)(((char*)p_tcb->psp_base) + p_taskinfo ->u_stack_size); 
	
		p_tcb->tid   = tid;
    p_tcb->state = READY;
    p_tcb->prio  = p_taskinfo->prio;
    p_tcb->priv  = p_taskinfo->priv;
		p_tcb->ptask = p_taskinfo->ptask;
		p_tcb->real_time = 0;
		p_tcb->tsk_period = 0;
		p_tcb->tsk_deadline = 0;
		p_tcb->nth_execution = 0;
    
		p_tcb->psp = usp;
		p_tcb->u_stack_size = p_taskinfo ->u_stack_size;
		p_tcb->message_header = NULL;
		

    /*-------------------------------------------------------------------
     *  Step2: create task's thread mode initial context on the user stack.
     *         fabricate the stack so that the stack looks like that
     *         task executed and entered kernel from the SVC handler
     *         hence had the exception stack frame saved on the user stack.
     *         This fabrication allows the task to return
     *         to SVC_Handler before its execution.
     *
     *         8 registers listed in push order
     *         <xPSR, PC, uLR, uR12, uR3, uR2, uR1, uR0>
     * -------------------------------------------------------------*/

    // if kernel task runs under SVC mode, then no need to create user context stack frame for SVC handler entering
    // since we never enter from SVC handler in this case
    
    *(--usp) = INITIAL_xPSR;             // xPSR: Initial Processor State
    *(--usp) = (U32) (p_taskinfo->ptask);// PC: task entry point
        
    // uR14(LR), uR12, uR3, uR3, uR1, uR0, 6 registers
    for ( int j = 0; j < 6; j++ ) {
        
#ifdef DEBUG_0
        *(--usp) = 0xDEADAAA0 + j;
#else
        *(--usp) = 0x0;
#endif
    }
    
    // allocate kernel stack for the task
    ksp = k_alloc_k_stack(tid);
    if ( ksp == NULL ) {
        return RTX_ERR;
    }

    /*---------------------------------------------------------------
     *  Step3: create task kernel initial context on kernel stack
     *
     *         12 registers listed in push order
     *         <kLR, kR4-kR12, PSP, CONTROL>
     * -------------------------------------------------------------*/
    // a task never run before directly exit
    *(--ksp) = (U32) (&SVC_RTE);
    // kernel stack R4 - R12, 9 registers
#define NUM_REGS 9    // number of registers to push
      for ( int j = 0; j < NUM_REGS; j++) {        
#ifdef DEBUG_0
        *(--ksp) = 0xDEADCCC0 + j;
#else
        *(--ksp) = 0x0;
#endif
    }
        
    // put user sp on to the kernel stack
    *(--ksp) = (U32) usp;
    
    // save control register so that we return with correct access level
    if (p_taskinfo->priv == 1) {  // privileged 
        *(--ksp) = __get_CONTROL() & ~BIT(0); 
    } else {                      // unprivileged
        *(--ksp) = __get_CONTROL() | BIT(0);
    }

    p_tcb->msp = ksp;
		
	
    return RTX_OK;
}

/**************************************************************************//**
 * @brief       switching kernel stacks of two TCBs
 * @param       p_tcb_old, the old tcb that was in RUNNING
 * @return      RTX_OK upon success
 *              RTX_ERR upon failure
 * @pre         gp_current_task is pointing to a valid TCB
 *              gp_current_task->state = RUNNING
 *              gp_crrent_task != p_tcb_old
 *              p_tcb_old == NULL or p_tcb_old->state updated
 * @note        caller must ensure the pre-conditions are met before calling.
 *              the function does not check the pre-condition!
 * @note        The control register setting will be done by the caller
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 * @attention   CRITICAL SECTION
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 *
 *****************************************************************************/
__asm void k_tsk_switch(TCB *p_tcb_old)
{
        PRESERVE8
        EXPORT  K_RESTORE
        
        PUSH    {R4-R12, LR}                // save general pupose registers and return address
        MRS     R4, CONTROL                 
        MRS     R5, PSP
        PUSH    {R4-R5}                     // save CONTROL, PSP
        STR     SP, [R0, #TCB_MSP_OFFSET]   // save SP to p_old_tcb->msp
K_RESTORE
        LDR     R1, =__cpp(&gp_current_task)
        LDR     R2, [R1]
        LDR     SP, [R2, #TCB_MSP_OFFSET]   // restore msp of the gp_current_task
        POP     {R4-R5}
        MSR     PSP, R5                     // restore PSP
        MSR     CONTROL, R4                 // restore CONTROL
        ISB                                 // flush pipeline, not needed for CM3 (architectural recommendation)
        POP     {R4-R12, PC}                // restore general purpose registers and return address
}


__asm void k_tsk_start(void)
{
        PRESERVE8
        B K_RESTORE
}


/**************************************************************************//**
 * @brief       run a new thread. The caller becomes READY and
 *              the scheduler picks the next ready to run task.
 * @return      RTX_ERR on error and zero on success
 * @pre         gp_current_task != NULL && gp_current_task == RUNNING
 * @post        gp_current_task gets updated to next to run task
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 * @attention   CRITICAL SECTION
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 *****************************************************************************/
int k_tsk_run_new(void)
{
    TCB *p_tcb_old = NULL;
    
    if (gp_current_task == NULL) {
        return RTX_ERR;
    }

		if(gp_current_task->real_time == 1){
			return RTX_OK;
		}
    p_tcb_old = gp_current_task;
    gp_current_task = scheduler();
		
    if ( gp_current_task == NULL  ) {
        gp_current_task = p_tcb_old;        // revert back to the old task
        return RTX_ERR;
    }
		
		// at this point, gp_current_task != NULL and p_tcb_old != NULL
		if (gp_current_task != p_tcb_old) {
				gp_current_task->state = RUNNING;   // change state of the to-be-switched-in  tcb
				if (p_tcb_old -> state != BLK_RECV && p_tcb_old -> state != BLK_SEND && p_tcb_old -> state != SUSPENDED) {
					p_tcb_old->state = READY;           // change state of the to-be-switched-out tcb
				}
				k_tsk_switch(p_tcb_old);            // switch kernel stacks       
		}
    return RTX_OK;
}

 
/**************************************************************************//**
 * @brief       yield the cpu
 * @return:     RTX_OK upon success
 *              RTX_ERR upon failure
 * @pre:        gp_current_task != NULL &&
 *              gp_current_task->state = RUNNING
 * @post        gp_current_task gets updated to next to run task
 * @note:       caller must ensure the pre-conditions before calling.
 *****************************************************************************/
int k_tsk_yield(void)
{
		if(gp_current_task == NULL || gp_current_task -> state != RUNNING) {
      return RTX_ERR;
		}
		if(gp_current_task->real_time == 1){
			return RTX_OK;
		}
    U8 current_prio = gp_current_task -> prio;
    int current_order = priority2order(current_prio);

    if(current_order == -1)
      return RTX_ERR;
		if(current_prio != 0xff){
			push_tail(gp_current_task,&ready_queue[current_order]);
		}
		
    return k_tsk_run_new();
}

/**
 * @brief   get task identification
 * @return  the task ID (TID) of the calling task
 */
task_t k_tsk_gettid(void)
{
		//if(g_timer_count%100==0)
			//printf("g_timer_count: %d\r\n", g_timer_count);
    return gp_current_task->tid;
}

/*
 *===========================================================================
 *                             TO BE IMPLEMETED IN LAB2
 *===========================================================================
 */

int k_tsk_create(task_t *task, void (*task_entry)(void), U8 prio, U32 stack_size)
{
#ifdef DEBUG_0
    printf("k_tsk_create: entering...\n\r");
    printf("task = 0x%x, task_entry = 0x%x, prio=%d, stack_size = %d\n\r", task, task_entry, prio, stack_size);
#endif /* DEBUG_0 */
    U32 stack_size_value = stack_size;
    if(stack_size_value < PROC_STACK_SIZE){
      stack_size_value = PROC_STACK_SIZE;
    }
		
    //find the next available slot in the g_tcb array for tid value 
    task_t task_id;
    for(task_id = 1; task_id < MAX_TASKS-3; task_id++){
      if(g_tcbs[task_id].state == DORMANT){
        #ifdef DEBUG_0
          printf("Found Dormant Task at index %d\n", task_id);
        #endif /* DEBUG_0 */
        break;
      }
    }
    
    if(task_id == (MAX_TASKS-3) && g_tcbs[task_id].state != DORMANT){
      #ifdef DEBUG_0
        printf("Error: The system has reached maximum number of tasks \n");
      #endif /* DEBUG_0 */
      errno = EAGAIN;
      return -1;
    }
    
    int priority_index = -1;
    priority_index = priority2order(prio);
    if(priority_index == -1){
      #ifdef DEBUG_0
        printf("Error: The prio value is not valid \n");
      #endif /* DEBUG_0 */
      errno = EINVAL;
      return -1;
    }
		
		TASK_INIT new_taskinfo;
		new_taskinfo.priv = 0;
		new_taskinfo.prio = prio;
		new_taskinfo.ptask = task_entry;
		new_taskinfo.tid = task_id;
		new_taskinfo.u_stack_size = stack_size_value;
    
		if(k_tsk_create_new(&new_taskinfo, &g_tcbs[task_id], task_id) == -1){
			return -1;
		}
		
    // Add the TCB to the corresponding Priority Queue 
		if(g_tcbs[task_id].prio >= gp_current_task->prio)
    push_tail(&g_tcbs[task_id], &ready_queue[priority_index]);
    //stores the ID of the new task in the task buffer
    *task = task_id;
   
   
		if(g_tcbs[task_id].prio < gp_current_task->prio){
						TCB *p_old_tcb = gp_current_task;
						gp_current_task = &g_tcbs[task_id];
						gp_current_task->state = RUNNING;
						push_head(p_old_tcb, &ready_queue[priority2order(p_old_tcb->prio)]);
						k_tsk_switch(p_old_tcb);
		}
			
  return RTX_OK;
}

void k_tsk_exit(void) 
{
#ifdef DEBUG_0
    printf("k_tsk_exit: entering...\n\r");
#endif /* DEBUG_0 */
    if(gp_current_task != NULL)
    {
			
			//check deadline for current task
			if(gp_current_task->real_time == 1){
				if((gp_current_task->tsk_deadline) < g_timer_count)
					printf("Job %d of task %d missed its deadline\r\n", gp_current_task->nth_execution, gp_current_task->tid);
				}
				
		/*		 TCB* highest_task = scheduler();
				 if(highest_task->real_time ==1){
					 highest_task->state = READY;
					 push_head(highest_task, &ready_queue[priority2order(highest_task->prio)]);
				 }else{
						TCB* p_tcb_old = gp_current_task;
						gp_current_task = highest_task;
						gp_current_task->state = RUNNING;   // change state of the to-be-switched-in  tcb
						k_tsk_switch(p_tcb_old);   
				} */
				TCB *p_old_tcb = gp_current_task;
				gp_current_task = scheduler();
				gp_current_task->state = RUNNING;
				k_tsk_switch(p_old_tcb);
					
			}else{
				TCB *p_tcb_old = gp_current_task;
				p_tcb_old->state = DORMANT;
				k_mpool_dealloc(MPID_IRAM2, p_tcb_old -> psp_base );		// deallocate task
				
				MAILBOX* p_box_old = &(g_mailboxes[gp_current_task -> tid]);
				if (p_box_old -> active  == 1) {	// only dealloc if the task has a mailbox
					MB* p_buf_old = &(p_box_old -> memory_buffer);
					p_box_old -> active = 0;	// deactivate mailbox
					p_buf_old -> capacity = 0;	// reset the ring buffer capacity
					k_mpool_dealloc(MPID_IRAM2, p_buf_old -> memory_start);	// deallocate mailbox
				}
				
				k_tsk_run_new();    
			}			

    return;
}

int k_tsk_set_prio(task_t task_id, U8 prio) 
{
#ifdef DEBUG_0
    printf("k_tsk_set_prio: entering...\n\r");
    printf("task_id = %d, prio = %d.\n\r", task_id, prio);
#endif /* DEBUG_0 */   
	int new_prio = priority2order(prio);
	
  if( task_id > MAX_TASKS-1||  (new_prio>5)    ||  (new_prio<0))
	{
		errno = EINVAL;
		return RTX_ERR;
	}
  TCB *new_tcb = &g_tcbs[task_id];
	if(new_tcb ->prio == PRIO_NULL || prio == PRIO_NULL)
	{
		errno = EINVAL;
		return RTX_ERR;
	}
  
	if((new_tcb == NULL)    ||    (gp_current_task->priv==0 && new_tcb->priv==1)
			||  (prio != PRIO_NULL && task_id==0)   ||  (prio == PRIO_NULL && task_id!=0)   ||  (new_tcb->state==DORMANT) 
	){
			errno = EPERM;
			return RTX_ERR;
	
	}
	if((new_tcb->real_time == 1)&& (prio != PRIO_RT_LB)){
			errno = EPERM;
			return RTX_ERR;
	}
	
	if((new_tcb->real_time == 0)&& (prio == PRIO_RT_LB)){
			errno = EPERM;
			return RTX_ERR;
	}
	
	if (new_tcb -> state == BLK_RECV || new_tcb -> state == BLK_SEND) {
		new_tcb -> prio = prio;	// only need to update the new priority if the task is blocked for any reason
	}
	/*******************real time logic start***************************/
	if (new_tcb -> real_time != 1){
		
		if(new_tcb -> prio != prio){
     U8 new_tcb_pre_prio = new_tcb -> prio;
 
		 if(gp_current_task ->prio > prio && gp_current_task ->tid != task_id){
				
			  new_tcb -> prio = prio;
        TCB *p_old_tcb = gp_current_task;
        gp_current_task = new_tcb;
			 	pop_task(new_tcb, &ready_queue[priority2order(new_tcb_pre_prio)]);
				push_head(p_old_tcb, &ready_queue[priority2order(p_old_tcb->prio)]);
				gp_current_task->state = RUNNING;
				k_tsk_switch(p_old_tcb);
                
			}else if(gp_current_task ->tid == task_id){
				if(gp_current_task->prio < prio)
				{
					 TCB* highest_task = &g_tcbs[TID_NULL];
					 for (int i = 0; i < 5; i++) {
							if (ready_queue[i].HEAD != NULL) { // if the highest prioirty contains ready task
								highest_task = ready_queue[i].HEAD;
								break;
						}
					}
				  gp_current_task->prio = prio;
					
					if(highest_task->prio <= prio)
					{
							TCB *p_old_tcb = gp_current_task;
							gp_current_task = highest_task;
							pop_head(&ready_queue[priority2order(highest_task->prio)]);
							push_tail(p_old_tcb, &ready_queue[priority2order(prio)]);
							gp_current_task->state = RUNNING;
							k_tsk_switch(p_old_tcb);
					}
					 
						
				}
			} else {
								new_tcb -> prio = prio;
								pop_task(new_tcb, &ready_queue[priority2order(new_tcb_pre_prio)]);
			 					push_tail(new_tcb, &ready_queue[priority2order(prio)]);
     }
   }
 }else{
	 new_tcb->prio = prio;
 }
    return RTX_OK;
}
		
		
	
	
   /******************real time logic end****************************/
   

/**
 * @brief   Retrieve task internal information 
 * @note    this is a dummy implementation, you need to change the code
 */
int k_tsk_get(task_t tid, RTX_TASK_INFO *buffer)
{
#ifdef DEBUG_0
    printf("k_tsk_get: entering...\n\r");
    printf("tid = %d, buffer = 0x%x.\n\r", tid, buffer);
#endif /* DEBUG_0 */    
  
    if (buffer == NULL ) {
				errno = EFAULT;
        return RTX_ERR;
    }
		if ( tid >MAX_TASKS-1) {
				errno = EINVAL;
        return RTX_ERR;
    }
        TCB *this_tcb = &g_tcbs[tid];
			
      //  if(this_tcb -> state == DORMANT){
			//			buffer->tid           = tid;
       //     buffer->state         = this_tcb->state;
            //return RTX_ERR;
       // }else{
            buffer->tid           = tid;
            buffer->prio          = this_tcb->prio;
            buffer->priv          = this_tcb->priv;
            buffer->state         = this_tcb->state;
            buffer->k_sp          = (U32)(this_tcb->msp);
            buffer->u_sp          = (U32)(this_tcb->psp);
            buffer->k_sp_base     = (U32)(g_k_stacks[tid] + (KERN_STACK_SIZE >> 2));
            buffer->u_sp_base     = (U32)(this_tcb->psp_base);


            buffer->u_stack_size  = this_tcb->u_stack_size;
            buffer->k_stack_size  = KERN_STACK_SIZE;
            
            U32 *task_entry = this_tcb->priv == 1? g_k_stacks[tid] + (KERN_STACK_SIZE >> 2):(void *)this_tcb->ptask;
            buffer->ptask         = (void(*)())(task_entry); 
						
						if (gp_current_task && gp_current_task->tid == tid) {
								buffer->k_sp = __get_MSP();
								buffer->u_sp = __get_PSP();
						}
       // }
    
		return RTX_OK;
}

int k_tsk_ls(task_t *buf, size_t count){
#ifdef DEBUG_0
    printf("k_tsk_ls: buf=0x%x, count=%u\r\n", buf, count);
#endif /* DEBUG_0 */
	if (buf == NULL) {
		errno = EFAULT;
		return RTX_ERR;
	}
	int non_dormant = 0;
	for (task_t i = 0; i < MAX_TASKS; i++) {
		if (&g_tcbs[i] != NULL && g_tcbs[i].state != DORMANT) {
			*(buf+non_dormant) = i;
			non_dormant++;
		}
		if (non_dormant == count) {
			return count;
		}
	}
  return non_dormant;
}


int k_rt_tsk_set(TIMEVAL *p_tv)
{
#ifdef DEBUG_0
    printf("k_rt_tsk_set: p_tv = 0x%x\r\n", p_tv);
#endif /* DEBUG_0 */
	
	if(gp_current_task->real_time == 1){
			errno = EPERM;
			return RTX_ERR;
		}
		U32 set_period = (p_tv->sec)*1000000 + (p_tv->usec); // unit is 1ms
		if((set_period == 0)&&(set_period%2500 !=0)){
			errno = EINVAL;
			return RTX_ERR;
		}
		U32 period_round = set_period / (500);
		gp_current_task->real_time = 1;
		gp_current_task->tsk_period = period_round;
		U32 tsk_deadline = g_timer_count +1 + period_round;
		gp_current_task->tsk_deadline = tsk_deadline;
		
		gp_current_task->prio = PRIO_RT;
		gp_current_task->state = READY;
		push_tail(gp_current_task, &ready_queue[priority2order(PRIO_RT)]);
		
		TCB *p_old_tcb = gp_current_task;
		gp_current_task = scheduler();
		gp_current_task->state = RUNNING;
		k_tsk_switch(p_old_tcb);
		
    return RTX_OK;   
}

int k_rt_tsk_susp(void)
{
#ifdef DEBUG_0
    printf("k_rt_tsk_susp: entering\r\n");
#endif /* DEBUG_0 */
		if(gp_current_task->real_time == 0){
			errno = EPERM;
			return RTX_ERR;
		}
		if(gp_current_task->real_time == 0){
		 errno = EPERM;
		 return RTX_ERR;
		}
		gp_current_task->state = SUSPENDED;
		
		if((gp_current_task->tsk_deadline) < g_timer_count)
		{
			printf("Job %d of task %d missed its deadline\r\n", gp_current_task->nth_execution, gp_current_task->tid);
		  U32 missed_period = (g_timer_count - (gp_current_task->tsk_deadline))/(gp_current_task->tsk_period);
		  gp_current_task->tsk_deadline = gp_current_task->tsk_deadline + (missed_period+1)*gp_current_task->tsk_period;
		}
		else 
		{
			gp_current_task->tsk_deadline = gp_current_task->tsk_deadline + gp_current_task->tsk_period;
		}
		
		push_tail(gp_current_task, &suspend_tsk);
		
		TCB *p_old_tcb = gp_current_task;
		gp_current_task = scheduler();
		gp_current_task->state = RUNNING;
		k_tsk_switch(p_old_tcb);
		
    return RTX_OK;
}

int k_rt_tsk_get(task_t tid, TIMEVAL *buffer)
{
#ifdef DEBUG_0
    printf("k_rt_tsk_get: entering...\n\r");
    printf("tid = %d, buffer = 0x%x.\n\r", tid, buffer);
#endif /* DEBUG_0 */    
    if (buffer == NULL) {
        return RTX_ERR;
    }   
    TCB* point_task = NULL;
		point_task = &g_tcbs[tid];
		if(point_task == NULL){
			return RTX_ERR;
		}
		if(point_task->real_time == 0){
			errno = EINVAL;
			return RTX_ERR;
		}
		if(gp_current_task->real_time == 0){
			errno = EPERM;
			return RTX_ERR;
		}
		U32 tsk_period = point_task->tsk_period;
		U32 tsk_sec = (tsk_period * 500) %1000000;
		U32 tsk_usec = (tsk_period * 500)- tsk_sec*1000000;
    buffer->sec  = tsk_sec;
    buffer->usec = tsk_usec;
    
    return RTX_OK;
}
/*
 *===========================================================================
 *                             END OF FILE
 *===========================================================================
 */

