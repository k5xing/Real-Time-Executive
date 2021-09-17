#include "ae_tasks.h"
#include "uart_polling.h"
#include "printf.h"
#include "ae_util.h"
#include "ae_tasks_util.h"

#define     NUM_TESTS       2       // number of tests
#define     NUM_INIT_TASKS  1       // number of tasks during initialization
#define     BUF_LEN         128     // receiver buffer length
#define     MY_MSG_TYPE     100     // some customized message type

const char   PREFIX[]      = "G12-TS1";
const char   PREFIX_LOG[]  = "G12-TS1-LOG";
const char   PREFIX_LOG2[] = "G12-TS1-LOG2";
TASK_INIT    g_init_tasks[NUM_INIT_TASKS];

AE_XTEST     g_ae_xtest;               
AE_CASE      g_ae_cases[NUM_TESTS];
AE_CASE_TSK  g_tsk_cases[NUM_TESTS];

U8 g_buf1[BUF_LEN];
U8 g_buf2[BUF_LEN];
U8 g_buf3[BUF_LEN];
U8 g_buf4[BUF_LEN];
U8 g_buf5[BUF_LEN];
U8 g_buf6[BUF_LEN];
task_t g_tasks[MAX_TASKS];
task_t g_tids[MAX_TASKS];

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

    tasks[0].ptask = &task0;
    g_tids[0]=1;
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
        g_tsk_cases[i].pos = 0;
    }
    printf("%s: START\r\n", PREFIX);
}

void update_ae_xtest(int test_id)
{
    g_ae_xtest.test_id = test_id;
    g_ae_xtest.index = 0;
    g_ae_xtest.num_tests_run++;
}

void gen_req0(int test_id)
{
    g_tsk_cases[test_id].p_ae_case->num_bits = 5;  
    g_tsk_cases[test_id].p_ae_case->results = 0;
    g_tsk_cases[test_id].p_ae_case->test_id = test_id;
    g_tsk_cases[test_id].len = 16; // assign a value no greater than MAX_LEN_SEQ
    g_tsk_cases[test_id].pos_expt = 7;
       
    update_ae_xtest(test_id);
}

void gen_req1(int test_id)
{
    //bits[0:3] pos check, bits[4:12] for exec order check
    g_tsk_cases[test_id].p_ae_case->num_bits = 11;  
    g_tsk_cases[test_id].p_ae_case->results = 0;
    g_tsk_cases[test_id].p_ae_case->test_id = test_id;
    g_tsk_cases[test_id].len = 0;       // N/A for this test
    g_tsk_cases[test_id].pos_expt = 0;  // N/A for this test
    
    update_ae_xtest(test_id);
}


int test0_start(int test_id)
{
    int ret_val = 10;
    
    gen_req0(test_id);

    U8   *p_index    = &(g_ae_xtest.index);
    int  sub_result  = 0;
    
    *p_index = 0;
    strcpy(g_ae_xtest.msg, "task0: creating a HIGH prio task that runs task1 function");
    ret_val = tsk_create(&g_tids[1], &task1, HIGH, 0x200);  /*create a user task */
    sub_result = (ret_val == RTX_OK) ? 1 : 0;
    process_sub_result(test_id, *p_index, sub_result);    
    if ( ret_val != RTX_OK ) {
        sub_result = 0;
        test_exit();
    }

    (*p_index)++;
    strcpy(g_ae_xtest.msg, "task0: creating a HIGH prio task that runs task2 function");
    ret_val = tsk_create(&g_tids[2], &task2, HIGH, 0x200);  /*create a user task */
    sub_result = (ret_val == RTX_OK) ? 1 : 0;
    process_sub_result(test_id, *p_index, sub_result);
    if ( ret_val != RTX_OK ) {
        test_exit();
    }
		
		(*p_index)++;
    strcpy(g_ae_xtest.msg, "task0: creating a HIGH prio task that runs task2 function");
    ret_val = tsk_create(&g_tids[3], &task3, HIGH, 0x200);  /*create a user task */
    sub_result = (ret_val == RTX_OK) ? 1 : 0;
    process_sub_result(test_id, *p_index, sub_result);
    if ( ret_val != RTX_OK ) {
        test_exit();
    }
    
    (*p_index)++;
    sprintf(g_ae_xtest.msg, "task0: creating a mailbox of size %u Bytes", BUF_LEN);
    mbx_t mbx_id = mbx_create(BUF_LEN);  // create a mailbox for itself
    sub_result = (mbx_id >= 0) ? 1 : 0;
    process_sub_result(test_id, *p_index, sub_result);
    if ( ret_val != RTX_OK ) {
        test_exit();
    }
    

    task_t  *p_seq_expt = g_tsk_cases[test_id].seq_expt;
    p_seq_expt[0] = g_tids[0];
    p_seq_expt[1] = g_tids[0];
    p_seq_expt[2] = g_tids[1];
    p_seq_expt[3] = g_tids[1];
    p_seq_expt[4] = g_tids[2];
    p_seq_expt[5] = g_tids[0];
    p_seq_expt[6] = g_tids[0];
    return RTX_OK;
}

void test1_start(int test_id, int test_id_data)
{  
    gen_req1(1);
    
    U8      pos         = g_tsk_cases[test_id_data].pos;
    U8      pos_expt    = g_tsk_cases[test_id_data].pos_expt;
    task_t  *p_seq      = g_tsk_cases[test_id_data].seq;
    task_t  *p_seq_expt = g_tsk_cases[test_id_data].seq_expt;
       
    U8      *p_index    = &(g_ae_xtest.index);
    int     sub_result  = 0;
    
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
    
    int diff = pos - pos_expt;
    
    // test 1-[0]
    *p_index = 0;
    strcpy(g_ae_xtest.msg, "checking execution shortfalls");
    sub_result = (diff < 0) ? 0 : 1;
    process_sub_result(test_id, *p_index, sub_result);
    
    //test 1-[1]
    (*p_index)++;
    strcpy(g_ae_xtest.msg, "checking unexpected execution once");
    sub_result = (diff == 1) ? 0 : 1;
    process_sub_result(test_id, *p_index, sub_result);
    
    //test 1-[2]
    (*p_index)++;
    strcpy(g_ae_xtest.msg, "checking unexpected execution twice");
    sub_result = (diff == 2) ? 0 : 1;
    process_sub_result(test_id, *p_index, sub_result);
    
    //test 1-[3]
    (*p_index)++;
    strcpy(g_ae_xtest.msg, "checking correct number of executions");
    sub_result = (diff == 0) ? 1 : 0;
    process_sub_result(test_id, *p_index, sub_result);
    
    //test 1-[4:12]
    for ( int i = 0; i < pos_expt; i ++ ) {
        (*p_index)++;
        sprintf(g_ae_xtest.msg, "checking execution sequence @ %d", i);
        sub_result = (p_seq[i] == p_seq_expt[i]) ? 1 : 0;
        process_sub_result(test_id, *p_index, sub_result);
    }
        
    
    test_exit();
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

/**************************************************************************//**
 * @brief   The first task to run in the system
 *****************************************************************************/

void task0(void)
{
	int ret_val = 10;
    task_t tid = tsk_gettid();
    int    test_id = 0;
		U8  *buf = &g_buf1[0];
		size_t msg_hdr_size = sizeof(struct rtx_msg_hdr);
		struct rtx_msg_hdr *ptr = (void *)buf;

    printf("%s: TID = %u, task0 entering\r\n", PREFIX_LOG2, tid);
    update_exec_seq(test_id, tid);

    ret_val = test0_start(test_id);
		TIMEVAL tv; 
		tv.sec  = 1;
    tv.usec = 0;
	
    if ( ret_val == RTX_OK ) {
        printf("%s: TID = %u, task0(NRT): calling rt_tsk_set(5s)\r\n", PREFIX_LOG2, tid);
        rt_tsk_set(&tv);
       
        printf("%s: TID = %u, task0(RT): priority = RT, should return here, calling susp()\r\n", PREFIX_LOG2, tid);
				update_exec_seq(test_id, tid);
				rt_tsk_susp();
			
				printf("%s: TID = %u, task0(RT): susp task been released\r\n", PREFIX_LOG2, tid);
				update_exec_seq(test_id, tid);
				ptr->length = msg_hdr_size + 2;         
				ptr->type = DEFAULT;                    
				ptr->sender_tid = tid;                
				buf += msg_hdr_size;
				*buf = 'A';

				printf("%s: TID = %u, task0: send  message to task1\r\n", PREFIX_LOG2, tid);   
				ret_val = send_msg(g_tids[1], (void *)ptr);
			
				int i=0;
				for(i=0; i<200; i++){
					printf("AAAAA\r\n");
				}
				update_exec_seq(test_id, tid);
    }
    test1_start(test_id + 1, test_id);
}

void task1(void)
{
		int ret_val = 10;
    mbx_t  mbx_id;
    task_t tid = tsk_gettid();
    int    test_id = 0;
    U8     *p_index    = &(g_ae_xtest.index);
    int    sub_result  = 0;
    int 	half_mailbox_size = BUF_LEN /2;

    size_t msg_hdr_size = sizeof(struct rtx_msg_hdr);
    U8  *buf = &g_buf3[0];                  // buffer is allocated by the caller */
    struct rtx_msg_hdr *ptr = (void *)buf;

    printf("%s: TID = %u, task1: entering\r\n", PREFIX_LOG2, tid);   
    update_exec_seq(test_id, tid);

    (*p_index)++;
    sprintf(g_ae_xtest.msg, "task1: creating a mailbox of size %u Bytes", BUF_LEN);
    mbx_id = mbx_create(BUF_LEN);  // create a mailbox for itself
    sub_result = (mbx_id >= 0) ? 1 : 0;
    process_sub_result(test_id, *p_index, sub_result);
    
    if ( mbx_id < 0 ) {
        printf("%s: TID = %u, task2: failed to create a mailbox, terminating tests\r\n", PREFIX_LOG2, tid);
        test_exit();
    }

		TIMEVAL tv; 
		tv.sec  = 1;
    tv.usec = 0;
		printf("%s: TID = %u, task1(NRT): calling rt_tsk_set(1s)\r\n", PREFIX_LOG2, tid);
    rt_tsk_set(&tv);
		
    ret_val = recv_msg_nb(g_buf4, BUF_LEN);
    update_exec_seq(test_id, tid);
		
		printf("%s: TID = %u, task1: hello back to task1\r\n", PREFIX_LOG2, tid);

    tsk_exit();
}

void task2(void)
{
	int ret_val = 10;
    mbx_t  mbx_id;
    task_t tid = tsk_gettid();
    int    test_id = 0;
    int    sub_result  = 0;

    size_t msg_hdr_size = sizeof(struct rtx_msg_hdr);
    U8  *buf = &g_buf5[0];                  // buffer is allocated by the caller */
    struct rtx_msg_hdr *ptr = (void *)buf;

    printf("%s: TID = %u, task2: entering\r\n", PREFIX_LOG2, tid);   
    update_exec_seq(test_id, tid);
		
    tsk_exit();
}

void task3 (void)
{
	while(1) {
        
        //uart1_put_string("non real-time running");
        //uart1_put_string("\n\r");

	}
}