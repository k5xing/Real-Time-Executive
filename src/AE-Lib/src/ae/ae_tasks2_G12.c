#include "ae_tasks.h"
#include "uart_polling.h"
#include "printf.h"
#include "ae_util.h"
#include "ae_tasks_util.h"

#define     BUF_LEN         128
#define     MY_MSG_TYPE     100     // some customized message type, better move it to common_ext.h
#define     NUM_INIT_TASKS  2       // number of tasks during initialization
#define     NUM_TESTS       1       // number of tests
const char   PREFIX[]      = "G12-TS2";
const char   PREFIX_LOG[]  = "G12-TS2-LOG";
const char   PREFIX_LOG2[] = "G12-TS2-LOG2";
TASK_INIT   g_init_tasks[NUM_INIT_TASKS];
/* 
 * The following arrays can also be dynamic allocated to reduce ZI-data size
 *  They do not have to be global buffers (provided the memory allocator has no bugs)
 */
AE_XTEST     g_ae_xtest;                // test data, re-use for each test
AE_CASE      g_ae_cases[NUM_TESTS];
AE_CASE_TSK  g_tsk_cases[NUM_TESTS];

U8 g_buf1[BUF_LEN];
U8 g_buf2[BUF_LEN];
task_t g_tasks[MAX_TASKS];

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
        tasks[i].priv = 0;
    }
		tasks[0].priv = 1;
    tasks[0].ptask = &priv_task1;
    tasks[1].ptask = &task1;

		
		init_ae_tsk_test();
}
void init_ae_tsk_test(void)
{
    g_ae_xtest.test_id = 0;
    g_ae_xtest.index = 0;
    g_ae_xtest.num_tests = NUM_TESTS;
    g_ae_xtest.num_tests_run = 0;
    
    for ( int i = 0; i< NUM_TESTS; i++ ) {
        g_tsk_cases[i].p_ae_case = &g_ae_cases[i];
        g_tsk_cases[i].p_ae_case->results  = 0x0;
        g_tsk_cases[i].p_ae_case->test_id  = i;
        g_tsk_cases[i].p_ae_case->num_bits = 0;
        g_tsk_cases[i].pos = 0;  // first avaiable slot to write exec seq tid
        // *_expt fields are case specific, deligate to specific test case to initialize
    }
		g_tsk_cases[0].p_ae_case->num_bits = 29;
		g_tsk_cases[0].len = 50; // assign a value no greater than MAX_LEN_SEQ
    g_tsk_cases[0].pos_expt = 25;
		
    printf("%s: START\r\n", PREFIX);
}

void update_ae_xtest(int test_id)
{
    g_ae_xtest.test_id = test_id;
    g_ae_xtest.index = 0;
    g_ae_xtest.num_tests_run++;
}

int update_exec_seq(int test_id, task_t tid)
{

    U8 len = g_tsk_cases[test_id].len;
    U8 *p_pos = &g_tsk_cases[test_id].pos;
    task_t *p_seq = g_tsk_cases[test_id].seq;
    p_seq[*p_pos] = tid;
    (*p_pos)++;
    (*p_pos) = (*p_pos)%len;  // preventing out of array bound
    return RTX_OK;
}
void priv_task1(void)
{
		int ret_val=10;
		int  sub_result  = 0;
    task_t tid = tsk_gettid();
    g_tasks[0] = tid;
    TIMEVAL tv;    
    update_ae_xtest(0);
		U8     *p_index    = &(g_ae_xtest.index);
	
    tv.sec  = 5;
    tv.usec = 0;
    update_exec_seq(0, tid);
    printf("priv_task1: TID =%d\r\n", tid); 
   
    /*------------------------------------------------------------------------------
    * call this function after finishing initial real time task set up
    * this function elevates the task to a real-time task 
    *-----------------------------------------------------------------------------*/
    rt_tsk_set(&tv); 
    (*p_index)++;
    strcpy(g_ae_xtest.msg, "creating MEDIUM priority task task2");
		ret_val=tsk_create(&g_tasks[2], &task2, MEDIUM, PROC_STACK_SIZE);
		sub_result = (ret_val == RTX_OK) ? 1 : 0;
    process_sub_result(0, *p_index, sub_result);
		
		(*p_index)++;
		strcpy(g_ae_xtest.msg, "creating MEDIUM priority task task3");
		ret_val=tsk_create(&g_tasks[3], &task3, MEDIUM, PROC_STACK_SIZE);
		sub_result = (ret_val == RTX_OK) ? 1 : 0;
    process_sub_result(0, *p_index, sub_result);
   
		ret_val=tsk_create(&g_tasks[4], &task0, MEDIUM, PROC_STACK_SIZE);
		for (int i = 0; i < 30 ;i++) {
				if(i == 7)
				{
					(*p_index)++;
					strcpy(g_ae_xtest.msg, "Recover priv_task1");
					sub_result = 1;
					process_sub_result(0, *p_index, sub_result);
				}
				if(i%6 == 1)
				{
					update_exec_seq(0, tid);
				}
        char out_char = '0' + i%10;
        for (int j = 0; j < 5; j++ ) {
            uart1_put_char(out_char);
        }
        uart1_put_string("\n\r");
				
        

        if ( i%6 == 0 ) {
            uart1_put_string("priv_task1 before calling rt_tsk_susp\r\n");
            rt_tsk_susp();      // wait till its next period
        }
    }
		strcpy(g_ae_xtest.msg, "finished task1");
		sub_result = 1;
		(*p_index)++;
    process_sub_result(0, *p_index, sub_result);
		
		task_t *p_seq_expt = g_tsk_cases[0].seq_expt;
    p_seq_expt[0] = g_tasks[0];
    p_seq_expt[1] = g_tasks[1];
		p_seq_expt[2] = g_tasks[2];
		p_seq_expt[3] = g_tasks[2];
    p_seq_expt[4] = g_tasks[3];
		p_seq_expt[5] = g_tasks[3];
		p_seq_expt[6] = g_tasks[2];
		p_seq_expt[7] = g_tasks[3];
		p_seq_expt[8] = g_tasks[2];
		p_seq_expt[9] = g_tasks[0];
		p_seq_expt[10] = g_tasks[2];
    p_seq_expt[11] = g_tasks[3];
		p_seq_expt[12] = g_tasks[0];
		p_seq_expt[13] = g_tasks[2];
    p_seq_expt[14] = g_tasks[3];
		p_seq_expt[15] = g_tasks[2];
		p_seq_expt[16] = g_tasks[0];
		p_seq_expt[17] = g_tasks[3];
		p_seq_expt[18] = g_tasks[2];
		p_seq_expt[19] = g_tasks[2];
		p_seq_expt[20] = g_tasks[3];
    p_seq_expt[21] = g_tasks[0];
		p_seq_expt[22] = g_tasks[2];
		p_seq_expt[23] = g_tasks[3];
    p_seq_expt[24] = g_tasks[0];
		
		
		task_t  *p_seq      = g_tsk_cases[0].seq;
		U8      pos         = g_tsk_cases[0].pos;
		U8      pos_expt    = g_tsk_cases[0].pos_expt;
		// output the real execution order
    printf("%s: real exec order: ", PREFIX_LOG);
    int pos_len = (pos > MAX_LEN_SEQ)? MAX_LEN_SEQ : pos;
    for ( int i = 0; i < pos_len; i++) {
        printf("%d -> ", p_seq[i]);
    }
    printf("NIL\r\n");
    
    // output the expected execution order
    printf("%s: expt exec order: ", PREFIX_LOG);
    for ( int i = 0; i < pos_expt; i++ ) {
        printf("%d -> ", p_seq_expt[i]);
    }
    printf("NIL\r\n");
		
		for ( int i = 0; i < pos_expt; i ++ ) {
		(*p_index)++;
		sprintf(g_ae_xtest.msg, "checking execution sequence @ %d", i);
		sub_result = (p_seq[i] == p_seq_expt[i]) ? 1 : 0;
		process_sub_result(0, *p_index, sub_result);
}
		 test_exit();
}


void task1(void)
{
	 task_t tid = tsk_gettid();
   g_tasks[1] = tid;
	 printf("task1: TID =%d\r\n", tid); 
	 update_exec_seq(0, tid);
	 tsk_exit();
}
void task2(void)
{
    task_t tid = tsk_gettid();
    TIMEVAL tv;    
		update_exec_seq(0, tid);
    tv.sec  = 3;
    tv.usec = 0;
    update_exec_seq(0, tid);
    printf("task2: TID =%d\r\n", tid); 
   int i=0;
 
    rt_tsk_set(&tv); 
		
	
		while(1)
		{
        char out_char = 'A' + i%26;
        for (int j = 0; j < 5; j++ ) {
            uart1_put_char(out_char);
        }
        uart1_put_string("\n\r");
				
        if(i%5 == 1)
				{
					update_exec_seq(0, tid);
				}

        if ( i%5 == 0 ) {
            uart1_put_string("task2 before calling rt_tsk_susp.\n\r");
            rt_tsk_susp();      // wait till its next period
        }
				i++;
    }
	  
}
void task3(void)
{
	
    task_t tid = tsk_gettid();
    TIMEVAL tv;    
		update_exec_seq(0, tid);
    tv.sec  = 4;
    tv.usec = 0;
    update_exec_seq(0, tid);
    printf("task3: TID =%d\r\n", tid); 
   int i=0;
 
    rt_tsk_set(&tv); 
		
	
		while(1) {
        char out_char = 'a' + i%26;
        for (int j = 0; j < 5; j++ ) {
            uart1_put_char(out_char);
        }
        uart1_put_string("\n\r");
				
        if(i%4 == 1)
				{
					update_exec_seq(0, tid);
				}

        if ( i%4 == 0 ) {
            uart1_put_string("task3 before calling rt_tsk_susp.\n\r");
            rt_tsk_susp();      // wait till its next period
        }
				i++;
    }
	
	
}
void task0 (void)
{
	while(1) {
        
        //uart1_put_string("non real-time running");
        //uart1_put_string("\n\r");

	}
}