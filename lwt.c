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
static int __next_id = INIT_ID;

//pointer to the current thread
static lwt_t __current_thread = NULL;
static lwt_t __original_thread = NULL;

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
	else if(t == LWT_INFO_NTHD_BLOCKED){
		if(__blocked_threads){
			count = __blocked_threads->count;
		}
		else{
			count = 0;
		}
	}
	else{
		if(__zombie_threads){
			count = __zombie_threads->count;
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
			lwt->thread_sp = lwt->max_addr_thread_stack;
		}
		else{
			lwt->min_addr_thread_stack = __lwt_stack_get();
			lwt->max_addr_thread_stack = (long *)(lwt->min_addr_thread_stack + STACK_SIZE);
			lwt->thread_sp = lwt->max_addr_thread_stack - 12;
		}


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
		}
		push_list(__current_threads, lwt);

		//add to run queue
		if(lwt->id != INIT_ID){
			if(!__runnable_threads){
				__runnable_threads = (list_t *)malloc(sizeof(list_t));
				init_list(__runnable_threads);
			}
			push_list(__runnable_threads, lwt);
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
	void * value = __current_thread->start_routine(__current_thread->args);
	lwt_die();
}

void * lwt_join(lwt_t thread){
	//ensure current thread isn't thread
	assert(__current_thread != thread);
	//block if the thread hasn't returned yet
	while(__current_thread->info != LWT_INFO_NTHD_ZOMBIES){
		//switch to another thread
		__lwt_schedule();
	}
	//free the thread's stack
	__lwt_stack_return(thread->max_addr_thread_stack);
	return thread->return_value;
}

void lwt_die(void * value){
	__current_thread->return_value = value;
	//check to see if we can return
	while(__current_thread->children->count > 0){
		__lwt_schedule();
	}
	//remove from parent thread
	if(__current_thread->parent){
		remove_list(__current_thread->parent->children, __current_thread);
	}

	//change status to zombie
	__current_thread->info = LWT_INFO_NTHD_ZOMBIES;

	//switch to another thread
	__lwt_schedule();
}

int lwt_yield(lwt_t lwt){
	assert(lwt != __current_thread); //ensure current thread isn't being yielded to itself
	if(lwt == LWT_NULL){
		__lwt_schedule();
	}
	else{
		lwt_t curr_thread = __current_thread;
		__current_thread = lwt;
		//remove it from the runqueue
		remove_list(__runnable_threads, lwt);
		__lwt_dispatch(lwt, curr_thread);
		__lwt_trampoline();
	}
	return 0;
}

void __lwt_schedule(){
	if(__current_thread != peek_list(__runnable_threads)){
		lwt_t curr_thread = __current_thread;
		//move current thread to the end of the queue
		if(__current_thread == LWT_INFO_NTHD_RUNNABLE){
			//initialize runnable threads if necessary
			if(!__runnable_threads){
				__runnable_threads = (list_t *)malloc(sizeof(list_t));
				init_list(__runnable_threads);
			}
			push_list(__runnable_threads, __current_thread);
		}
		else if(__current_thread == LWT_INFO_NTHD_BLOCKED){
			//init blocked threads if necessary
			if(!__blocked_threads){
				__blocked_threads = (list_t *)malloc(sizeof(list_t));
				init_list(__blocked_threads);
			}
			push_list(__blocked_threads, __current_thread);
		}
		else{
			//init zombie threads if necessary
			if(!__zombie_threads){
				__zombie_threads = (list_t *)malloc(sizeof(list_t));
				init_list(__zombie_threads);
			}
			push_list(__zombie_threads, __current_thread);
		}
		//pop the queue
		lwt_t next_thread = (lwt_t)pop_list(__runnable_threads);
		__current_thread = next_thread;
		__lwt_dispatch(next_thread, curr_thread);
		__lwt_trampoline();
	}
}

__attribute__((constructor)) void __init__(){
	lwt_t curr_thread = (lwt_t)malloc(sizeof(lwt));
	curr_thread->id = -1;
	__init_lwt(curr_thread);
	__original_thread = curr_thread;
}

__attribute__((destructor)) void __destroy__(){
	if(__current_threads){
		empty_list_free(__current_threads);
		free(__current_threads);
	}
	if(__runnable_threads){
		while(__runnable_threads->head){
			pop_list(__runnable_threads);
		}
		free(__runnable_threads);
	}
	if(__blocked_threads){
		while(__blocked_threads){
			pop_list(__blocked_threads);
		}
		free(__blocked_threads);
	}
	if(__zombie_threads){
		while(__zombie_threads){
			pop_list(__zombie_threads);
		}
		free(__zombie_threads);
	}
}

lwt_t lwt_create(lwt_fnt_t fn, void * data){
	lwt_t thread = (lwt_t)malloc(sizeof(lwt));
	thread->start_routine = fn;
	thread->args = data;
	__init_lwt(thread);

	return thread;
}

void * test_method(void * param){
	printf("In another thread!!!\n");
	return 0;
}

int
main(int argc, char *argv[])
{
	printf("Starting\n");
	lwt_t new_thread = lwt_create(test_method, NULL);
	printf("Successfully initialized shit!\n");
	lwt_join(new_thread);

	printf("Successfully joined!\n");

	return 0;
}

