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
 * @param next The next thread to switch to
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
lwt_t __current_threads = NULL;
/**
 * @brief Head of the list of all runnable threads
 */
lwt_t __runnable_threads_head = NULL;

/**
 * @brief Tail of the list of all runnable threads
 */
lwt_t __runnable_threads_tail = NULL;

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
 * @brief Inserts the new thread before the old thread in the current list
 * @param old The old thread
 * @param new The new thread
 */
static void __insert_before_current(lwt_t old, lwt_t new){
	//adjust new
	new->next_current = old;
	new->previous_current = old->previous_current;
	//adjust old
	if(old->previous_current){
		old->previous_current->next_current = new;
	}
	old->previous_current = new;
}

/**
 * @brief Inserts the new thread before the old thread in the sibling list
 * @param old The old thread
 * @param new The new thread
 */
static void __insert_before_sibling(lwt_t old, lwt_t new){
	//adjust new
	new->next_sibling = old;
	new->previous_sibling = old->previous_sibling;
	//adjust old
	if(old->previous_sibling){
		old->previous_sibling->next_sibling = new;
	}
	old->previous_sibling = new;
}

/**
 * @brief Insert the new thread after the old thread in the current thread list
 * @param old The old thread
 * @param new The new thread
 */
static void __insert_after_current(lwt_t old, lwt_t new){
	//adjust new
	new->next_current = old->next_current;
	new->previous_current = old;
	//adjust old
	if(old->next_current){
		old->next_current->previous_current = new;
	}
	old->next_current = new;
}

/**
 * @brief Inserts the new thread after the old thread in the sibling thread list
 * @param old The old thread
 * @param new The new thread
 */
static void __insert_after_sibling(lwt_t old, lwt_t new){
	//adjust new
	new->next_sibling = old->next_sibling;
	new->previous_sibling = old->next_sibling;
	//adjust old
	if(old->next_sibling){
		old->next_sibling->previous_sibling = new;
	}
	old->next_sibling = new;
}

/**
 * @brief Remove the given thread from the current list
 * @param thread The thread to be removed
 */
static void __remove_current(lwt_t thread){
	//detach -> update next  + previous
	if(thread->next_current){
		thread->next_current->previous_current = thread->previous_current;
	}
	if(thread->previous_current){
		thread->previous_current->next_current = thread->next_current;
	}
}

/**
 * @brief Remove the given thread from the sibling list
 * @param thread The thread to be removed
 */
static void __remove_sibling(lwt_t thread){
	//detach -> update next + previous
	if(thread->next_sibling){
		thread->next_sibling->previous_sibling = thread->previous_sibling;
	}
	if(thread->previous_sibling){
		thread->previous_sibling->next_sibling = thread->next_sibling;
	}
}

/**
 * @brief Inserts the given thread to the head of the runnable thread list
 * @param thread The thread to be inserted
 */
static void __insert_runnable_head(lwt_t thread){
	if(__runnable_threads_head){
		//thread will be new head
		thread->next_runnable = __runnable_threads_head;
		thread->previous_runnable = NULL;
		__runnable_threads_head->previous_runnable = thread;
		__runnable_threads_head = thread;
	}
	else{
		//thread will be head and tail
		thread->previous_runnable = NULL;
		thread->next_runnable = NULL;
		__runnable_threads_head = thread;
		__runnable_threads_tail = thread;
	}
}

/**
 * @brief Inserts the given thread to the tail of the runnable thread list
 * @param thread The new thread to be inserted in the list of runnable threads
 */
static void __insert_runnable_tail(lwt_t thread){
	if(__runnable_threads_tail){
		//thread will be new tail
		thread->previous_runnable = __runnable_threads_tail;
		thread->next_runnable = NULL;
		__runnable_threads_tail->next_runnable = thread;
		__runnable_threads_tail = thread;
	}
	else{
		//thread will be head and tail
		thread->previous_runnable = NULL;
		thread->next_runnable = NULL;
		__runnable_threads_head = thread;
		__runnable_threads_tail = thread;
	}
}

/**
 * @brief Removes the thread from the list of runnable threads
 * @param thread The thread to be removed
 */
static void __remove_from_runnable_threads(lwt_t thread){
	//detach
	if(thread->next_runnable){
		thread->next_runnable->previous_runnable = thread->previous_runnable;
	}
	if(thread->previous_runnable){
		thread->previous_runnable->next_runnable = thread->next_runnable;
	}
	//update head
	if(thread == __runnable_threads_head){
		__runnable_threads_head = __runnable_threads_head->next_runnable;
	}
	//update tail
	if(thread == __runnable_threads_tail){
		__runnable_threads_tail = __runnable_threads_tail->previous_runnable;
	}
}

/**
 * @brief Gets the counts of the info
 * @param t The info enum to get the counts
 * @return The count for the info enum provided
 * @see lwt_info_t
 */
int lwt_info(lwt_info_t t){
	int count = 0;
	lwt_t current_thread = __current_threads;
	while(current_thread){
		if(current_thread->info == t){
			count++;
		}
		current_thread = current_thread->next_current;
	}
	return count;
}

/**
 * @brief Initializes the main thread
 * @param thread The main thread
 */
void __init_lwt_main(lwt_t thread){
	thread->id = __get_new_id();
	//set the original stack to the current stack pointer
	register long sp asm("esp");
	thread->min_addr_thread_stack = (long *)(sp - STACK_SIZE);
	thread->max_addr_thread_stack = (long *)sp;
	thread->thread_sp = thread->max_addr_thread_stack;

	thread->id = __get_new_id();

	thread->parent = NULL;
	thread->children = NULL;

	thread->previous_sibling = NULL;
	thread->next_sibling = NULL;

	thread->previous_current = NULL;
	thread->next_current = NULL;

	thread->next_runnable = NULL;
	thread->previous_runnable = NULL;

	//add to current threads
	__current_threads = thread;

	//set current thread
	__current_thread = thread;
	__original_thread = thread;

	thread->info = LWT_INFO_NTHD_RUNNABLE;
}

/**
 * @brief Initializes the provided thread
 * @param thread The thread to init
 */
void __init_lwt(lwt_t thread){
	//check that the id hasn't been initialized
	if(thread->id < INIT_ID){
		//set id
		thread->id = __get_new_id(); //return id and increment TODO implement atomically

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

		//set up parent
		thread->parent = __current_thread;
		thread->children = NULL;

		thread->previous_sibling = NULL;
		thread->next_sibling = NULL;
		//add to parent (only initial thread won't have the parent)
		if(__current_thread->children){
			__insert_after_sibling(__current_thread->children, thread);
		}
		else{
			__current_thread->children = thread;
		}

		//add to the list of threads
		__insert_after_current(__current_thread, thread);

		//add to run queue
		thread->info = LWT_INFO_NTHD_RUNNABLE;
		__insert_runnable_tail(thread);
	}
}

/**
 * @brief Allocates the stack for a LWT and returns it
 */
 void * __lwt_stack_get(){
	long * stack = (long *)malloc(sizeof(long) * STACK_SIZE);
	assert(stack);
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
	//remove from current threads
	__remove_current(lwt);
	//clear up resources
	if(lwt->min_addr_thread_stack){
		__lwt_stack_return(lwt->min_addr_thread_stack);
	}
	free(lwt);
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
	while(__current_thread->children){
		__current_thread->info = LWT_INFO_NTHD_BLOCKED;
		__lwt_schedule();
	}
	//remove from parent thread
	if(__current_thread->parent){
		//check siblings
		if(__current_thread->next_sibling ||
				__current_thread->previous_sibling){
			__remove_sibling(__current_thread);
		}
		else{
			__current_thread->parent->children = NULL;
		}

		//check if parent can be unblocked
		if(!__current_thread->parent->children && __current_thread->parent->info == LWT_INFO_NTHD_BLOCKED){
			__current_thread->parent->info = LWT_INFO_NTHD_RUNNABLE;
			__insert_runnable_tail(__current_thread->parent);
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
		__remove_from_runnable_threads(lwt);
		//put it out in front
		__insert_runnable_head(curr_thread);
		__lwt_dispatch(lwt, curr_thread);
		//__lwt_trampoline();
	}
	return 0;
}

/**
 * @brief Schedules the next_current thread to switch to and dispatches
 */
void __lwt_schedule(){

	if(__runnable_threads_head && __runnable_threads_head != __current_thread){
		lwt_t curr_thread = __current_thread;
		//move current thread to the end of the queue
		if(__current_thread->info == LWT_INFO_NTHD_RUNNABLE){
			//push current thread to end of runnable
			__insert_runnable_tail(__current_thread);
		}
		//pop the queue
		lwt_t next_thread = __runnable_threads_head;
		__remove_from_runnable_threads(next_thread);
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
	__init_lwt_main(curr_thread);
}

/**
 * @brief Cleans up all remaining threads on exit
 */
__attribute__((destructor)) void __destroy__(){
	//free threads
	lwt_t current_thread = __current_threads;
	lwt_t next = NULL;
	while(current_thread){
		next = current_thread->next_current;
		if(current_thread != __original_thread){
			free(current_thread);
		}
		current_thread = next;
	}
	//free original thread
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
*/
/*
void * test_method(void * param){
	printf("In another thread!!!\n");
	//lwt_t new_thread = lwt_create(test_method2, NULL);
	//lwt_join(new_thread);
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
