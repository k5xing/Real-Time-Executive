#include "ae_tasks.h"
#include "uart_polling.h"
#include "printf.h"
#include "ae_util.h"
#include "ae_tasks_util.h"

#define     NUM_TESTS       1      // number of tests
#define     NUM_INIT_TASKS  2       // number of tasks during initialization
#define     BUF_LEN         128     // receiver buffer length
#define     MY_MSG_TYPE     100     // some customized message type

const char   PREFIX[]      = "G12-TS4";
const char   PREFIX_LOG[]  = "G12-TS4-LOG";
const char   PREFIX_LOG2[] = "G12-TS4-LOG2";
TASK_INIT    g_init_tasks[NUM_INIT_TASKS];
task_t g_tids[MAX_TASKS];


U8 g_buf1[BUF_LEN];
U8 g_buf2[BUF_LEN];

void set_ae_init_tasks (TASK_INIT **pp_tasks, int *p_num)
{
    *p_num = NUM_INIT_TASKS;
    *pp_tasks = g_init_tasks;
    set_ae_tasks(*pp_tasks, *p_num);
}

void set_ae_tasks(TASK_INIT *tasks, int num)
{
    for (int i = 0; i < num; i++ ) {                                                 
        tasks[i].u_stack_size = PROC_STACK_SIZE;    
        tasks[i].prio = HIGH;
        tasks[i].priv = 1;
    }

    tasks[0].ptask = &task0;
		g_tids[0] = 1;
    tasks[1].ptask = &task1;
		g_tids[1] = 2;
		g_tids[2] = 3;
		g_tids[3] = 4;
		printf("G12_TS4: START\r\n");
}



void task0(void)
{
	int ret_val=10;
	mbx_t task0_mb = mbx_create(BUF_LEN);
    size_t msg_hdr_size = sizeof(struct rtx_msg_hdr);
    U8 *buf = &g_buf1[0];  // buffer is allocated by the caller */
    struct rtx_msg_hdr *ptr = (void *)buf;

    ptr->length = msg_hdr_size + 1; 
    ptr->type = KCD_REG; 
    ptr->sender_tid = tsk_gettid(); 
    buf += msg_hdr_size;
    *buf = 'D';

    ret_val = send_msg(TID_KCD, (void *)ptr);
    if(ret_val != RTX_OK){
    	tsk_exit();
    }
    while(1){
			printf("task0 wait for command D \r\n");
    	while (recv_msg_nb(buf, BUF_LEN) != RTX_OK) {   // non-blocking receive    
       // printf("%s:, TID = %u, task0: calling tsk_yield() after recv_msg_nb() \r\n", PREFIX_LOG2, tid);      
        tsk_yield();
			}
    	if(ret_val == RTX_OK){
    		printf("task0 receive command D\r\n");
    	}else{
    		tsk_exit();
    	}
    	if(buf[MSG_HDR_SIZE] == 'D') {
            printf("task0 received D \r\n");
        }
    }
}

void task1(void)
{
	  int ret_val = 10;
		mbx_t task1_mb = mbx_create(BUF_LEN);
		tsk_create(&g_tids[2], &task2, HIGH, 0x200);
    size_t msg_hdr_size = sizeof(struct rtx_msg_hdr);
    U8 *buf = &g_buf2[0];  // buffer is allocated by the caller */
    struct rtx_msg_hdr *ptr = (void *)buf;

    ptr->length = msg_hdr_size + 1; 
    ptr->type = KCD_REG; 
    ptr->sender_tid = tsk_gettid(); 
    buf += msg_hdr_size;
    *buf = 'D';

    ret_val = send_msg(TID_KCD, (void *)ptr);
    if(ret_val != RTX_OK){
    	tsk_exit();
    }
    while(1){
			printf("task1 wait for command D\r\n");
    	while (recv_msg_nb(buf, BUF_LEN) != RTX_OK) {   // non-blocking receive    
       // printf("%s:, TID = %u, task0: calling tsk_yield() after recv_msg_nb() \r\n", PREFIX_LOG2, tid);      
        tsk_yield();
			}
    	if(ret_val == RTX_OK){
    		printf("task1 receive command D\r\n");
    	}else{
    		tsk_exit();
    	}
    	if(buf[MSG_HDR_SIZE] == 'D') {
            printf("task1 received D\r\n");
        }
    }
}
		void task2(void)
{
	  int ret_val = 10;
		mbx_t task2_mb = mbx_create(BUF_LEN);
		tsk_create(&g_tids[3], &task3, HIGH, 0x200);
    size_t msg_hdr_size = sizeof(struct rtx_msg_hdr);
    U8 *buf = &g_buf2[0];  // buffer is allocated by the caller */
    struct rtx_msg_hdr *ptr = (void *)buf;

    ptr->length = msg_hdr_size + 1; 
    ptr->type = KCD_REG; 
    ptr->sender_tid = tsk_gettid(); 
    buf += msg_hdr_size;
    *buf = 'G';

    ret_val = send_msg(TID_KCD, (void *)ptr);
    if(ret_val != RTX_OK){
    	tsk_exit();
    }
    while(1){
			printf("task2 wait for command G\r\n");
    	while (recv_msg_nb(buf, BUF_LEN) != RTX_OK) {   // non-blocking receive    
     //   printf("%s:, TID = %u, task0: calling tsk_yield() after recv_msg_nb() \r\n", PREFIX_LOG2, tid);      
        tsk_yield();
			}
    	if(ret_val == RTX_OK){
    		printf("task2 receive command G\r\n");
    	}else{
    		tsk_exit();
    	}
    	if(buf[MSG_HDR_SIZE] == 'G') {
            printf("task2 received G\r\n");
        }
    }
}
		void task3(void)
{
	  int ret_val = 10;
		mbx_t task1_mb = mbx_create(BUF_LEN);
    size_t msg_hdr_size = sizeof(struct rtx_msg_hdr);
    U8 *buf = &g_buf2[0];  // buffer is allocated by the caller */
    struct rtx_msg_hdr *ptr = (void *)buf;

    ptr->length = msg_hdr_size + 1; 
    ptr->type = KCD_REG; 
    ptr->sender_tid = tsk_gettid(); 
    buf += msg_hdr_size;
    *buf = 'F';

    ret_val = send_msg(TID_KCD, (void *)ptr);
    if(ret_val != RTX_OK){
    	tsk_exit();
    }
    while(1){
			printf("task3 wait for command F\r\n");
		  while (recv_msg_nb(buf, BUF_LEN) != RTX_OK) {   // non-blocking receive    
     //   printf("%s:, TID = %u, task0: calling tsk_yield() after recv_msg_nb() \r\n", PREFIX_LOG2, tid);      
        tsk_yield();
			}
    	if(ret_val == RTX_OK){
    		printf("task3 receive command F\r\n");
    	}else{
    		tsk_exit();
    	}
    	if(buf[MSG_HDR_SIZE] == 'F') {
            printf("task3 received F\r\n");
        }
    }
}

