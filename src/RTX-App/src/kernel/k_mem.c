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
 * @file        k_mem.c
 * @brief       Kernel Memory Management API C Code
 *
 * @version     V1.2021.01.lab2
 * @authors     Yiqing Huang
 * @date        2021 JAN
 *
 * @note        skeleton code
 *
 *****************************************************************************/

#include "k_inc.h"
#include "k_mem.h"

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
							  |                           |     |
							  |---------------------------|     |
							  |                           |     |
							  |      other data           |     |
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
 *                            GLOBAL VARIABLES
 *===========================================================================
 */
// kernel stack size, referred by startup_a9.s
const U32 g_k_stack_size = KERN_STACK_SIZE;
// task proc space stack size in bytes, referred by system_a9.c
const U32 g_p_stack_size = PROC_STACK_SIZE;

// task kernel stacks
U32 g_k_stacks[MAX_TASKS][KERN_STACK_SIZE >> 2] __attribute__((aligned(8)));

// task process stack (i.e. user stack) for tasks in thread mode
// remove this bug array in your lab2 code
// the user stack should come from MPID_IRAM2 memory pool
//U32 g_p_stacks[MAX_TASKS][PROC_STACK_SIZE >> 2] __attribute__((aligned(8)));
U32 g_p_stacks[NUM_TASKS][PROC_STACK_SIZE >> 2] __attribute__((aligned(8)));
typedef struct dnode {
	 struct dnode *prev;
	 struct dnode *next;
 } DNODE;
 
 typedef struct dlist {
	 struct dnode *head;
	 struct dnode *tail;
 } DLIST;
 
 U8 *bit_tree_ram1;
 U8 *bit_tree_ram2;
 U8 ram1_array[MAX_ARRAY_LENGTH_RAM1];          //initialize bit tree
 U8 ram2_array[MAX_ARRAY_LENGTH_RAM2];            //initialize bit tree
 DLIST* free_list_ram1;
 DLIST* free_list_ram2;
 DLIST ram1_freelist[MAX_LEVELS_RAM1];         //initialize free list
 DLIST ram2_freelist[MAX_LEVELS_RAM2];          //initialize free list

/*
 *===========================================================================
 *                            FUNCTIONS
 *===========================================================================
 */
/* set the correponding bit to 1*/
void set_bit(U8 bit_array[], U32 index) {
	 U32 i = index / 8;
	 U32 pos = index % 8;
	 U8 flag = 0x01;
	 flag = flag << pos;
	 bit_array[i] = bit_array[i] | flag;
 }

/* set the corresponding bit to 0 */
void clear_bit (U8 bit_array[], U32 index) {
	 U32 i = index / 8;
	 U32 pos = index % 8;
	 U8 flag = 0x01;
	 flag = flag << pos;
	 flag = ~flag;
	 bit_array[i] = bit_array[i] & flag;
 }

/* return the state of the corresponding bit */
U32 check_bit (U8 bit_array[], U32 index) {
	U32 i = index / 8;
	U32 pos = index % 8;
	U8 flag = 0x01;
	flag = flag << pos;
	
	if (bit_array[i] & flag) {
		return 1;
	} else {
		return 0;
	}
 }


U32 LOG2(U32 val)
{
	U32 curr_power = MIN_BLK_SIZE_LOG2;
	U32 curr_val = MIN_BLK_SIZE;
	while(curr_val < val){
		curr_val = curr_val*2;
		curr_power++;
	}
	return curr_power;
}

/* calculate 2^x */
U32 POWER2(U32 power)
{
	U32 val = 1;
	for(int i=0; i<power; i++){
		val = val*2;
	}
	return val;
}

/* Get the address of block based on index and level value of bit_tree*/
U32 get_address(U32 curr_location, U32 index, U32 level,mpool_t mpid)
{
	U32 curr_address = curr_location;
	if(mpid == MPID_IRAM1){
		curr_address = curr_address - (index % 2) * POWER2(RAM1_SIZE_LOG2 - level);
	}
	else if(mpid == MPID_IRAM2)
	{
		curr_address = curr_address - (index % 2) * POWER2(RAM2_SIZE_LOG2 - level);
	}
	else
	{
		return NULL;
	}
	return curr_address;

}

U32 get_buddy_address(U32 curr_location, U32 curr_x, U32 curr_level,mpool_t mpid)
{
	U32 buddy_address = curr_location;
		if(mpid == MPID_IRAM1){
		
			if((curr_x % 2) == 0){
			// curr_node is left node
				buddy_address = curr_location + POWER2(RAM1_SIZE_LOG2 - curr_level);
			}else{
				if((curr_x %2) == 1){
				buddy_address = curr_location - POWER2(RAM1_SIZE_LOG2 - curr_level);
			}
		}
	}
	else if(mpid == MPID_IRAM2)
	{
		if((curr_x % 2) == 0){
			// curr_node is left node
			buddy_address = curr_location + POWER2(RAM2_SIZE_LOG2 - curr_level);
		}else{
			if((curr_x %2) == 1){
				buddy_address = curr_location - POWER2(RAM2_SIZE_LOG2 - curr_level);
			}
		}
	}
	else
	{
		return NULL;
	}
	
	return buddy_address;
}

int pop_block(DLIST *f_list, U32 block_address){
		int counter = 0;
		if(f_list->head == NULL ){
			
			return -1;
		}else{
			if(block_address == (U32)((void *)f_list->head) ){
				DNODE* head_next = f_list->head ->next;
				f_list->head = head_next;
				//not sure if I need to free the old head pointer
				if(head_next != NULL){
					head_next->prev = NULL;
				}else{
					f_list->tail = head_next;
				}
			
			}else{
				DNODE* curr_node = f_list->head;
				
				for(curr_node = f_list->head; curr_node-> next != NULL; curr_node = curr_node -> next){
					if(block_address == (U32)((void *) curr_node)){
						
						DNODE* curr_next = curr_node ->next;
						DNODE* curr_prev = curr_node -> prev;
						curr_prev -> next = curr_next;
						
						if(curr_next != NULL){
							curr_next->prev = curr_prev;
							}
				}
			}
				
		}
	}
		return counter;
}

/* note list[n] is for blocks with order of n */
mpool_t k_mpool_create (int algo, U32 start, U32 end)
{
	mpool_t mpid = MPID_IRAM1;

#ifdef DEBUG_0
	printf("k_mpool_init: algo = %d\r\n", algo);
	printf("k_mpool_init: RAM range: [0x%x, 0x%x].\r\n", start, end);
#endif /* DEBUG_0 */
	
	if (algo != BUDDY ) {
		errno = EINVAL;
		return RTX_ERR;
	}
	
		if ( start == RAM1_START) {
			
			for (int i = 0; i < MAX_ARRAY_LENGTH_RAM1; i++) {
				ram1_array[i] = 0u;
			}
			bit_tree_ram1 = &ram1_array[0];
		
			DNODE* head = (void*) RAM1_START;
			head -> next = NULL;
			head -> prev = NULL;
			ram1_freelist[0].head = head;          //set the head of level 0 to the start of the RAM
			ram1_freelist[0].tail = head;
			
			for(int j = 1; j < MAX_LEVELS_RAM1; j++){     //set the remaining levels to NULL
				ram1_freelist[j].head = NULL;
				ram1_freelist[j].tail = NULL;
			}
			free_list_ram1 = &ram1_freelist[0];

	}
		else if ( start == RAM2_START) {
			
			mpid = MPID_IRAM2;
			
			for (int i = 0; i < MAX_ARRAY_LENGTH_RAM2; i++) {
				ram2_array[i] = 0u;
			}
			bit_tree_ram2 = &ram2_array[0];
			
			
			DNODE* head = (void*) RAM2_START;
			head -> next = NULL;
			head -> prev = NULL;
			ram2_freelist[0].head = head;          //set the head of level 0 to the start of the RAM
			ram2_freelist[0].tail = head;

			for(int j = 1; j < MAX_LEVELS_RAM2; j++){  //set the remaining levels to NULL
				ram2_freelist[j].head = NULL;
				ram2_freelist[j].tail = NULL;
			}
			free_list_ram2 = &ram2_freelist[0];
			
	}
		else {
			errno = EINVAL;
			return RTX_ERR;
	}
	
	return mpid;
}

void *k_mpool_alloc (mpool_t mpid, size_t size)
{
#ifdef DEBUG_0
	printf("k_mpool_alloc: mpid = %d, size = %d, 0x%x\r\n", mpid, size, size);
#endif /* DEBUG_0 */
		
		if(size == 0)         //return NULL if size < 0
			return NULL;
		
	if(mpid == MPID_IRAM1){                   //IRAM1
			
			if(size > RAM1_SIZE){
				errno = ENOMEM;
				return NULL;
			}
			U32 level = RAM1_SIZE_LOG2-LOG2(size);
			U32 curr_level = level;
	
			while(free_list_ram1[curr_level].head == NULL){    //find a block in the free list
				if(curr_level == 0){            //no block with enough size is free
					errno = ENOMEM;
					return NULL;
				}
				curr_level = curr_level -1;
			}
			
			while(curr_level < level){      //create free blocks to best fit the input size
				
				DNODE* ptr = free_list_ram1[curr_level].head;   //split the block
				DNODE* ptr_next = ptr ->next;
				free_list_ram1[curr_level].head = ptr_next;
				if(ptr_next != NULL)
					ptr_next -> prev = NULL;
				
				U32 diff = (ptr - (DNODE*)RAM1_START)*sizeof(DNODE);         //set corresponding value in bit tree
				U32 x = diff/(POWER2(RAM1_SIZE_LOG2 - curr_level));
				U32 index = POWER2(curr_level)-1+x;
				
				set_bit(bit_tree_ram1,index);
				
				DNODE* child1 = ptr;
				DNODE* child2 =(void*)((char*) ptr + RAM1_SIZE/POWER2(curr_level+1));

				free_list_ram1[curr_level+1].head = child1;    //put the children in the free list
				free_list_ram1[curr_level+1].tail = child2;				//WHAT IF THERE ARE FREE BLOCK AFTER CHILD 2? IS IT BECAUSE THE PREVIOUS LOOP GUARENTEE NO FREE BLOCK IN UPPER LEVELS?
				child1->prev = NULL;
				child1->next = child2;
				child2->prev = child1;
				child2->next = NULL;
				curr_level++;
			}
			
			DNODE* alloc = free_list_ram1[level].head;     //allocate to the head of the desired level free list
			DNODE* alloc_next = alloc->next;
			free_list_ram1[level].head = alloc_next;
			if(alloc_next != NULL)
				alloc_next->prev = NULL;
			
			U32 diff = (alloc - (DNODE*)RAM1_START)*sizeof(DNODE);   //set corresponding value in bit tree
			U32 x = diff/(RAM1_SIZE/POWER2(level));
			U32 index = POWER2(level)-1+x;
			set_bit(bit_tree_ram1,index);
			return alloc;
		}
		
		
		else if(mpid == MPID_IRAM2){
			
			if(size > RAM2_SIZE){
				errno = ENOMEM;
				return NULL;
			}
			U32 level = RAM2_SIZE_LOG2-LOG2(size);      //return NULL if size is not in the range
			U32 curr_level = level;
			
			while(free_list_ram2[curr_level].head == NULL){
				if(curr_level == 0){           //no block with enough size is free
					errno = ENOMEM;
					return NULL;
				}
				curr_level = curr_level -1;
			}
			
			
			while(curr_level < level){           //create free blocks to best fit the input size
				
				DNODE* ptr = free_list_ram2[curr_level].head;
				DNODE* ptr_next = ptr ->next;
				free_list_ram2[curr_level].head = ptr_next;
				if(ptr_next != NULL){
					ptr_next -> prev = NULL;
				}

				U32 diff = (ptr - (DNODE*)RAM2_START)*sizeof(DNODE);         //set corresponding value in bit tree
				U32 x = diff/(POWER2(RAM2_SIZE_LOG2 - curr_level));
				U32 index = POWER2(curr_level)-1+x;
				
				set_bit(bit_tree_ram2,index);
				
				DNODE* child1 = ptr;               //split the block
				DNODE* child2 =(void*)((char*) ptr + RAM2_SIZE/POWER2(curr_level+1));

				free_list_ram2[curr_level+1].head = child1;        //put the children in the free list
				free_list_ram2[curr_level+1].tail = child2;
				child1->prev = NULL;
				child1->next = child2;
				child2->prev = child1;
				child2->next = NULL;
				curr_level++;
				
			}
			
			DNODE* alloc = free_list_ram2[level].head;          //allocate to the head of the desired level free list
			DNODE* alloc_next = alloc->next;
			free_list_ram2[level].head = alloc_next;
			if(alloc_next != NULL){
				alloc_next->prev = NULL;
			}

			U32 diff = (alloc - (DNODE*)RAM2_START)*sizeof(DNODE);         //set corresponding value in bit tree
			U32 x = diff/(RAM2_SIZE/POWER2(level));
			U32 index = POWER2(level)-1+x;
			
			set_bit(bit_tree_ram2,index);
			
			return alloc;
			
		}
		else{
			 errno = EINVAL;
	   return NULL;
		}

}

int k_mpool_dealloc(mpool_t mpid, void *ptr)
{
#ifdef DEBUG_0
	printf("k_mpool_dealloc: mpid = %d, ptr = 0x%x\r\n", mpid, ptr);
#endif /* DEBUG_0 */
	if (ptr != NULL){

		if(mpid == MPID_IRAM1){
			//IRAM 1 Pool
			if((U32)ptr >=  RAM1_START && (U32)ptr <= RAM1_END){
					U32 curr_level =  MAX_LEVELS_RAM1 - 1;
					U32 diff = ((U32)ptr - RAM1_START);
					U32 x = diff/MIN_BLK_SIZE;
					
					U32 node_allocate_status = 2;		//initial value to indicate check_bit has not been performed yet
					U32 index;

					while(node_allocate_status != 1 ){
					  index = POWER2(curr_level)-1+x;		//function from lecture slide 22 for x-th biock
					  node_allocate_status = check_bit(bit_tree_ram1,index);
						if(node_allocate_status == 1 || curr_level <=0 ){
							
							break;
						}else{
							curr_level = curr_level - 1;
							x = x / 2;
						}
					}
					
					if(node_allocate_status == 1 && curr_level > 0){
						U32 buddy_status = 2;
						U32 curr_address = (U32)ptr;
						
						while(buddy_status != 1 && curr_level >0){
							
							clear_bit(bit_tree_ram1, index);
							U32 buddy_index = POWER2(curr_level) -1 + (x ^ 1);		//function from slide 22 for buddy block
							buddy_status = check_bit(bit_tree_ram1,buddy_index);
							U32 buddy_address = get_buddy_address(curr_address, x, curr_level, mpid);
					
							if(buddy_status == 0){
								/* Remove Free buddy node from children level*/
								pop_block(&free_list_ram1[curr_level], buddy_address);
								//move up to parent level to dealloc both buddy and check if parent's buddy can be coalescence
								curr_address = get_address(curr_address, x, curr_level, mpid);
								
								curr_level = curr_level -1;
								x = x / 2;
								index = POWER2(curr_level)-1+x;
							}
					
						}
							DNODE* freed_node = (void *) curr_address;
							DNODE* freed_node_next = free_list_ram1[curr_level].head;
							/* Add the free cosleasced blocked on parent level*/
							freed_node -> prev = NULL;
							/* check if the current head has same address as new block to avoid infinite looping back to self */
							if(freed_node != freed_node_next){
								freed_node -> next = freed_node_next ;
								if(freed_node_next != NULL)
									freed_node_next -> prev = freed_node;
							}
							free_list_ram1[curr_level].head = freed_node;
		
						return RTX_OK;
					}else{
						if(node_allocate_status == 1 && curr_level == 0){
							
							clear_bit(bit_tree_ram1, index);
							//Add the block back to the freelist
							DNODE* head = (void*) RAM1_START;
							head -> next = NULL;
							head -> prev = NULL;
							free_list_ram1[0].head = head;          //set the head of level 0 to the start of the RAM
							free_list_ram1[0].tail = head;
							
							return RTX_OK;
						}else{
							//cannot find the address in all levels or the address is at level 
							return RTX_ERR;
						}
					}
					}else{
				//ptr address does not belong to IRAM 1 Pool
				errno = EFAULT;
			  return RTX_ERR;
			}

		}else{
			if(mpid == MPID_IRAM2){
				//IRAM 2 Pool
				if((U32)ptr >=  RAM2_START && (U32) ptr <= RAM2_END){
					U32 curr_level = MAX_LEVELS_RAM2 - 1;
					U32 diff = ((U32)ptr - RAM2_START);
					U32 x = diff/MIN_BLK_SIZE;
					U32 node_allocate_status = 2;		//initial value to indicate check_bit has not been performed yet
					U32 index;

					while(node_allocate_status != 1 ){
					  index = POWER2(curr_level)-1+x;		//function from lecture slide 22 for x-th biock
					  node_allocate_status = check_bit(bit_tree_ram2,index);
				
						if(node_allocate_status == 1 || curr_level <=0 ){
							
							break;
						}else{
							curr_level = curr_level - 1;
							x = x / 2;
						}
					}
				
					if(node_allocate_status == 1 && curr_level > 0){
						U32 buddy_status = 2;
						U32 curr_address = (U32)ptr;
						while(buddy_status != 1 && curr_level >0){
							
							clear_bit(bit_tree_ram2, index);
							U32 buddy_index = POWER2(curr_level) -1 + (x ^ 1);		//function from slide 22 for buddy block
							buddy_status = check_bit(bit_tree_ram2,buddy_index);
							U32 buddy_address = get_buddy_address(curr_address, x, curr_level, mpid);
						
							if(buddy_status == 0){		
								pop_block(&free_list_ram2[curr_level], buddy_address);			
								//move up to parent level to dealloc both buddy and check if parent's buddy can be coalescence
								curr_address = get_address(curr_address, x, curr_level, mpid);
								/* Remove Free buddy node from children level*/			
								curr_level = curr_level -1;
								x = x / 2;
								index = POWER2(curr_level)-1+x;	
							}
						}
							DNODE* freed_node = (void *) curr_address;
							DNODE* freed_node_next = free_list_ram2[curr_level].head;
							/* Add the free cosleasced blocked on parent level*/
							freed_node -> prev = NULL;
							/* check if the current head has same address as new block to avoid infinite looping back to self */
							if(freed_node != freed_node_next){
								freed_node -> next = freed_node_next ;
								if(freed_node_next != NULL)
									freed_node_next -> prev = freed_node;
							}
							free_list_ram2[curr_level].head = freed_node;
						return RTX_OK;
					}else{
						if(node_allocate_status == 1 && curr_level == 0){	
							clear_bit(bit_tree_ram2, index);
							//Add the block back to the freelist
							DNODE* head = (void*) RAM2_START;
							head -> next = NULL;
							head -> prev = NULL;
							free_list_ram2[0].head = head;          //set the head of level 0 to the start of the RAM
							free_list_ram2[0].tail = head;
							return RTX_OK;
						}else{
							//cannot find the address in all levels or the address is at level 0		
							return RTX_ERR;
						}
					}
				}else{
					//ptr address does not belong to IRAM 2 Pool		
					errno = EFAULT;
					return RTX_ERR;
				}
			}else{
				//invalid Pool ID			
				errno = EINVAL;			
			  return RTX_ERR;
			}
		}
	}else{
		// ptr is null, no operation is performed
		return RTX_ERR;
	}
}

int k_mpool_dump (mpool_t mpid)
{
#ifdef DEBUG_0
	printf("k_mpool_dump: mpid = %d\r\n", mpid);
#endif /* DEBUG_0 */
	int free_space = 0;
		
		if(mpid == MPID_IRAM1){
			U32 curr_level = MAX_LEVELS_RAM1;
			do
			{
				curr_level --;
				DNODE* tmp = free_list_ram1[curr_level].head;
				while(tmp!=NULL)
				{
					printf("0x%x: 0x%x\r\n", tmp, RAM1_SIZE/(POWER2(curr_level)));
					free_space++;
					tmp = tmp->next;
				}
			} while(curr_level > 0);
		}
		else if(mpid == MPID_IRAM2){
			U32 curr_level = MAX_LEVELS_RAM2;
			do
			{
				curr_level --;
				DNODE* tmp = free_list_ram2[curr_level].head;
				while(tmp!=NULL)
				{
					printf("0x%x: 0x%x\r\n", tmp, RAM2_SIZE/(POWER2(curr_level)));
					free_space++;
					tmp = tmp->next;
				}
			} while(curr_level > 0);
		}
		else{
			errno = EINVAL;
			return 0;
		}
		printf("%d free memory block(s) found\r\n", free_space);
	return free_space;
}

int k_mem_init(int algo)
{
#ifdef DEBUG_0
	printf("k_mem_init: algo = %d\r\n", algo);
#endif /* DEBUG_0 */
		
	if ( k_mpool_create(algo, RAM1_START, RAM1_END) < 0 ) {
		return RTX_ERR;
	}
	
	if ( k_mpool_create(algo, RAM2_START, RAM2_END) < 0 ) {
		return RTX_ERR;
	}
	
	return RTX_OK;
}

/**
 * @brief allocate kernel stack statically
 */
U32* k_alloc_k_stack(task_t tid)
{
	
	if ( tid >= MAX_TASKS) {
		errno = EAGAIN;
		return NULL;
	}
	U32 *sp = g_k_stacks[tid+1];
	
	// 8B stack alignment adjustment
	if ((U32)sp & 0x04) {   // if sp not 8B aligned, then it must be 4B aligned
		sp--;               // adjust it to 8B aligned
	}
	return sp;
}

/**
 * @brief allocate user/process stack statically
 * @attention  you should not use this function in your lab
 */

U32* k_alloc_p_stack(task_t tid)
{
	if ( tid >= NUM_TASKS ) {
		errno = EAGAIN;
		return NULL;
	}
	U32 *sp = g_p_stacks[tid+1];
	// 8B stack alignment adjustment
	if ((U32)sp & 0x04) {   // if sp not 8B aligned, then it must be 4B aligned
		sp--;               // adjust it to 8B aligned
	}
	return sp;
}

/*
 *===========================================================================
 *                             END OF FILE
 *===========================================================================
 */

