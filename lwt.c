#include "lwt.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

/**
 * @brief The initial thread id
 */
#define INIT_ID 1
/**
 * @brief The default id provided to threads before actually generating them
 */
#define DEFAULT_ID -1

/**
 * @brief Dispatch function for switching between threads
 * @param next The next thead to switch to
 * @param current The current thread
 */
extern void __lwt_dispatch(lwt_t next, lwt_t current);

void __lwt_schedule(void);
void __lwt_trampoline(void);
void *__lwt_stack_get(void);
void __lwt_stack_return(void * stack);

/**
 * @brief Global counter for the thread id
 */
int __next_id = INIT_ID;

/**
 * @brief Pointer to the current thread
 */
lwt_t __current_thread = NULL;
/**
 * @brief Pointer to the original/main thread
 */
lwt_t __original_thread = NULL;

/**
 * @brief List of all active threads created
 */
list_t * __current_threads = NULL;
/**
 * @brief List of all runnable threads
 */
list_t * __runnable_threads = NULL;
/**
 * @brief List of all blocked threads
 */
list_t * __blocked_threads = NULL;
/**
 * @brief List of all zombie threads
 */
list_t * __zombie_threads = NULL;

/**
 * @brief Counter for the id
 * @return The next id to use
 */
int __get_new_id(){
	return __next_id++; //return and then increment
}

/**
 * @brief Gets the current thread
 * @return The current thread
 */
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

/**
 * @brief Gets the counts of the info
 * @param t The info enum to get the counts
 * @return The count for the info enum provided
 * @see lwt_info_t
 */
int lwt_info(lwt_info_t t){
	int count = 0;
	if(t == LWT_INFO_NTHD_RUNNABLE){
		if(__runnable_threads){
			count = __runnable_threads->count;
		}
		else{
			count = 0;
		}
		count++; //include the main thread
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

/*
void __attribute__((noinline)) __lwt_dispatch(lwt_t next, lwt_t current){
	__asm__ __volatile__ (
		//save callee saved registers
//		"pushl %%ebp\n\t"
		"pushl %%ebx\n\t"
		"pushl %%edi\n\t"
		"pushl %%esi\n\t"

		//move the current stack
		"movl %%esp, %0\n\t"
		//replace the stack
		"movl %1, %%esp\n\t"

		//restore registers
		"popl %%esi\n\t"
		"popl %%edi\n\t"
		"popl %%ebx\n\t"
//		"popl %%ebp\n\t"

		//jump to return routine
		//"jmp 1f\n\t"

		//return routine
		//"1:\n\t"
//		"ret\n\t"
		:
		: "m" ((current->thread_sp)), "m" ((next->thread_sp))
		: "memory"
	);
	printf("Returning\n");
}
*/

/**
 * @brief Initializes the provided thread
 * @param thread The thread to init
 */
void __init_lwt(lwt_t thread){
	//check that the id hasn't been initialized
	if(thread->id < INIT_ID){
		//set id
		thread->id = __get_new_id(); //return id and increment TODO implement atomically

		if(thread->id == INIT_ID){
			//set the original stack to the current stack pointer
			register long sp asm("esp");
			thread->min_addr_thread_stack = (long *)(sp - STACK_SIZE);
			thread->max_addr_thread_stack = (long *)sp;
			thread->thread_sp = thread->max_addr_thread_stack;
		}
		else{
			thread->min_addr_thread_stack = __lwt_stack_get();
			thread->max_addr_thread_stack = (long *)(thread->min_addr_thread_stack + STACK_SIZE);
			assert(thread->max_addr_thread_stack);
			thread->thread_sp = thread->max_addr_thread_stack - 1;
			//add the function
			*(thread->thread_sp--) = (long)(__lwt_trampoline);

			*(thread->thread_sp--) = (long)0; //ebp
			*(thread->thread_sp--) = (long)0;//ebx
			*(thread->thread_sp--) = (long)0;//edi
			*(thread->thread_sp) = (long)0;//esi

			//*(lwt->thread_sp) = (long)__lwt_trampoline;
			//lwt->thread_sp -= 4;
		}



		//set up parent
		thread->parent = __current_thread;
		thread->children = NULL;
		//add to parent (only initial thread won't have the parent)
		if(__current_thread){
			//init children if necessary
			if(!__current_thread->children){
				__current_thread->children = (list_t *)malloc(sizeof(list_t));
				assert(__current_thread->children);
				init_list(__current_thread->children);
			}
			//push lwt
			push_list(__current_thread->children, thread);
		}

		//add to the list of threads
		if(!__current_threads){
			__current_threads = (list_t *)malloc(sizeof(list_t));
			init_list(__current_threads);
		}
		push_list(__current_threads, thread);

		//add to run queue
		thread->info = LWT_INFO_NTHD_RUNNABLE;
		if(thread->id != INIT_ID){
			if(!__runnable_threads){
				__runnable_threads = (list_t *)malloc(sizeof(list_t));
				init_list(__runnable_threads);
			}
			push_list(__runnable_threads, thread);
		}
	}
}

/**
 * @brief Allocates the stack for a LWT and returns it
 */
 void * __lwt_stack_get(){
	long * stack = (long *)malloc(sizeof(long) * STACK_SIZE);
	return stack;
}

 /**
  * @brief Frees the provided stack
  * @param stack The LWT stack to free
  */
 void __lwt_stack_return(void * stack){
	free(stack);
}

 /**
  * @brief Drops in from being scheduled after the initialized thread is switched to and leaps to the function pointer provided
  */
 void __lwt_trampoline(){
	assert(__current_thread->start_routine);
	void * value = __current_thread->start_routine(__current_thread->args);
	lwt_die(value);
}

/**
 * @brief Cleans up the thread on join
 * @param lwt The thread to join on
 */
 void __cleanup_joined_thread(lwt_t lwt){
	if(lwt->parent == NULL){
		return; //ignore original and double frees
	}
	//remove thread from zombie list
	if(__zombie_threads && __zombie_threads->head){
		remove_list(__zombie_threads, lwt);
	}
	//remove from current threads
	if(__current_threads && __current_threads->head){
		remove_list(__current_threads, lwt);
	}
	//clear up resources
	if(lwt->min_addr_thread_stack){
		__lwt_stack_return(lwt->min_addr_thread_stack);
	}
	if(lwt->children){
		while(lwt->children && lwt->children->head){
			pop_list(lwt->children);
		}
		free(lwt->children);
	}
	lwt->parent = NULL;
	free(lwt);
}

/**
 * @brief Cleans up all threads on exit
 */
void __cleanup_thread_end(list_t * list){
	while(list->head){
		lwt_t lwt = pop_list(list);
		//remove thread from run queue
		remove_list(__current_threads, lwt);
		if(lwt != __original_thread){
			//free resources
			if(lwt->children){
				while(lwt->children && lwt->children->head){
					pop_list(lwt->children);
				}
				free(lwt->children);
			}
			if(lwt->min_addr_thread_stack){
				free(lwt->min_addr_thread_stack);
			}
		}
	}
}

/**
 * @brief Joins the provided thread
 * @param thread The thread to join on
 */
void * lwt_join(lwt_t thread){
	//ensure current thread isn't thread
	assert(__current_thread != thread);
	//ensure thread isn't main thread
	assert(thread != __original_thread);
	//block if the thread hasn't returned yet
	while(thread->info != LWT_INFO_NTHD_ZOMBIES){
		__current_thread->info = LWT_INFO_NTHD_BLOCKED;
		//switch to another thread
		__lwt_schedule();
	}
	void * value = thread->return_value;
	//free the thread's stack
	__cleanup_joined_thread(thread);
	//free thread info
	return value;
}

/**
 * @brief Prepares the current thread to be cleaned up
 */
void lwt_die(void * value){
	__current_thread->return_value = value;
	//check to see if we can return
	while(__current_thread->children && __current_thread->children->count > 0){
		__current_thread->info = LWT_INFO_NTHD_BLOCKED;
		__lwt_schedule();
	}
	//remove from parent thread
	if(__current_thread->parent){
		remove_list(__current_thread->parent->children, __current_thread);
		//check if parent can be unblocked
		if(__current_thread->parent->children->count == 0 && __current_thread->parent->info == LWT_INFO_NTHD_BLOCKED){
			__current_thread->parent->info = LWT_INFO_NTHD_RUNNABLE;
			//remove from blocked queue
			remove_list(__blocked_threads, __current_thread->parent);
			//add it to runnable list
			push_list(__runnable_threads, __current_thread->parent);
		}
	}

	//change status to zombie
	__current_thread->info = LWT_INFO_NTHD_ZOMBIES;

	//switch to another thread
	__lwt_schedule();
}

/**
 * @brief Yields to the provided LWT
 * @param lwt The thread to yield to
 * @note Will just schedule normally if LWT_NULL is provided
 * @return 0 if successful
 */
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
		//put it out in front
		push_head_list(__runnable_threads, curr_thread);
		__lwt_dispatch(lwt, curr_thread);
		//__lwt_trampoline();
	}
	return 0;
}

/**
 * @brief Schedules the next thread to switch to and dispatches
 */
void __lwt_schedule(){

	if(__runnable_threads->count > 0 && __current_thread != peek_list(__runnable_threads)){
		lwt_t curr_thread = __current_thread;
		//move current thread to the end of the queue
		if(__current_thread->info == LWT_INFO_NTHD_RUNNABLE){
			//initialize runnable threads if necessary
			if(!__runnable_threads){
				__runnable_threads = (list_t *)malloc(sizeof(list_t));
				init_list(__runnable_threads);
			}
			push_list(__runnable_threads, __current_thread);
		}
		else if(__current_thread->info == LWT_INFO_NTHD_BLOCKED){
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
		assert(next_thread);
		__current_thread = next_thread;
		__lwt_dispatch(next_thread, curr_thread);
	}
}

/**
 * @brief Initializes the LWT by wrapping the current thread as a LWT
 */
__attribute__((constructor)) void __init__(){
	lwt_t curr_thread = (lwt_t)malloc(sizeof(struct lwt));
	assert(curr_thread);
	curr_thread->id = DEFAULT_ID;
	curr_thread->children = NULL;
	__init_lwt(curr_thread);
	__original_thread = curr_thread;
	__current_thread = __original_thread;
}

/**
 * @brief Cleans up all remaining threads on exit
 */
__attribute__((destructor)) void __destroy__(){
	//free threads
	if(__runnable_threads){
		__cleanup_thread_end(__runnable_threads);
		free(__runnable_threads);
	}
	if(__blocked_threads){
		__cleanup_thread_end(__blocked_threads);
		free(__blocked_threads);
	}
	if(__zombie_threads){
		__cleanup_thread_end(__zombie_threads);
		free(__zombie_threads);
	}
	//free original thread
	if(__original_thread->children){
		free(__original_thread->children);
	}
	free(__original_thread);
}

/**
 * @brief Creates a LWT using the provided function pointer and the data as input for it
 * @param fn The function pointer to use
 * @param data The data to the function
 * @return A pointer to the initialized LWT
 */
lwt_t lwt_create(lwt_fnt_t fn, void * data){
	lwt_t thread = (lwt_t)malloc(sizeof(struct lwt));
	assert(thread);
	thread->start_routine = fn;
	thread->args = data;
	thread->id = DEFAULT_ID;
	__init_lwt(thread);

	return thread;
}

/*
 * Test cases
void * test_method3(void * param){
	printf("Received value: %d\n", *((int *)param));
	return (void *)3;
}

void * test_method2(void * param){
	printf("In yet another thread!!\n");
	int * param1 = (int *)malloc(sizeof(int));
	*param1 = 1;
	int * param2 = (int *)malloc(sizeof(int));
	*param2 = 2;
	lwt_t t1 = lwt_create(test_method3, param1);
	lwt_t t2 = lwt_create(test_method3, param2);
	lwt_join(t2);
	lwt_join(t1);
	free(param1);
	free(param2);
	return NULL;
}

void * test_method1(void * param){
	printf("In Method1!!\n");
	return NULL;
}

void * test_method(void * param){
	printf("In another thread!!!\n");
	lwt_t new_thread = lwt_create(test_method2, NULL);
	lwt_join(new_thread);
	return NULL;
}

int
main(int argc, char *argv[])
{
	printf("Starting\n");
	lwt_t new_thread = lwt_create(test_method, NULL);
	//lwt_t new_thread2 = lwt_create(test_method1, NULL);
	printf("Successfully initialized shit!\n");
	lwt_join(new_thread);

	//lwt_join(new_thread2);

	printf("Successfully joined!\n");

	return 0;
}
*/
