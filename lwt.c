#include "lwt.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#define INIT_ID 1

void __lwt_schedule(void);
void __lwt_dispatch(lwt_t next, lwt_t current);
void __lwt_trampoline(void);
void *__lwt_stack_get(void);
void __lwt_stack_return(void *stk);

//global counter for the id
int __next_id = INIT_ID;

//pointer to the current thread
lwt_t __current_thread = NULL;
lwt_t __original_thread = NULL;

list_t * __current_threads = NULL;
list_t * __runnable_threads = NULL;
list_t * __blocked_threads = NULL;
list_t * __zombie_threads = NULL;

/**
 * Counter for the id
 */
int __get_new_id(){
	return __next_id++; //return and then increment
}

lwt_t lwt_current(){
	return __current_thread;
}

/**
 * @brief Gets the thread id
 * @return The id of the thread
 */
int lwt_id(lwt_t thread){
	return thread->id;
}

int lwt_info(lwt_info_t t){
	int count = 0;
	if(t == LWT_INFO_NTHD_RUNNABLE){
		if(__runnable_threads){
			count = __runnable_threads->count;
		}
		else{
			count = 0;
		}
	}
	return count;
}

void __lwt_dispatch(lwt_t next, lwt_t current){
	__asm__ __volatile__ (
		//save callee saved registers
		"pushl %%esi\n\t"
		"pushl %%edi\n\t"
		"pushl %%ebx\n\t"
		"pushl %%ebp\n\t"

		//move the current stack
		"movl %%esp, %0\n\t"
		//replace the stack
		"movl %1, %%esp\n\t"

		//restore registers
		"popl %%ebp\n\t"
		"popl %%ebx\n\t"
		"popl %%edi\n\t"
		"popl %%esi\n\t"

		//jump to return routine
		"jmp return_routine\n\t"

		//return routine
		"return_routine:\n\t"
		"ret\n\t"
		:
		: "m" ((next->thread_sp)), "m" ((current->thread_sp))
		: "memory"
	);
}

void __init_lwt(lwt_t lwt){
	//check that the id hasn't been initialized
	if(lwt->id < INIT_ID){
		//set id
		lwt->id = __get_new_id(); //return id and increment TODO implement atomically

		if(lwt->id == INIT_ID){
			//set the original stack to the current stack pointer
			register long sp asm("esp");
			lwt->min_addr_thread_stack = (long *)(sp - STACK_SIZE);
			lwt->max_addr_thread_stack = (long *)sp;
		}
		else{
			lwt->min_addr_thread_stack = __lwt_stack_get();
			lwt->max_addr_thread_stack = (long *)(lwt->min_addr_thread_stack + STACK_SIZE);
		}
		lwt->thread_sp = lwt->max_addr_thread_stack;

		//set up parent
		lwt->parent = __current_thread;
		//add to parent (only initial thread won't have the parent)
		if(__current_thread){
			//init children if necessary
			if(!__current_thread->children){
				__current_thread->children = (list_t *)malloc(sizeof(list_t));
				assert(__current_thread->children);
				init_list(__current_thread->children);
			}
			//push lwt
			push_list(__current_thread->children, lwt);
		}

		//add to the list of threads
		if(!__current_threads){
			__current_threads = (list_t *)malloc(sizeof(list_t));
			init_list(__current_threads);
			push_list(__current_threads, lwt);
		}
	}
}

void * __lwt_stack_get(){
	unsigned long * stack = (unsigned long *)malloc(sizeof(unsigned long) * STACK_SIZE);
	return stack;
}

void __lwt_stack_return(void * stack){
	free(stack);
}

void __lwt_trampoline(){
	__current_thread->start_routine(__current_thread->args);
}

int lwt_yield(lwt_t lwt){
	assert(lwt != __current_thread); //ensure current thread isn't being yielded to itself
	if(lwt == LWT_NULL){
		__lwt_schedule();
	}
	else{
		__lwt_dispatch(lwt, __current_thread);
	}
	return 0;
}

void __lwt_schedule(){

}

__attribute__((constructor)) void __init__(){
	lwt_t curr_thread = (lwt_t)malloc(sizeof(lwt));
	__init_lwt(curr_thread);
	__original_thread = curr_thread;
}

__attribute__((destructor)) void __destroy__(){
	if(__current_threads){
		empty_list_free(__current_threads);
		free(__current_threads);
	}
}

int
main(int argc, char *argv[])
{
	printf("Successfully initialized shit!\n");
}

