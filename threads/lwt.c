#include "lwt.h"
#include "lwt_chan.h"
#include "lwt_cgrp.h"
#include "lwt_kthd.h"

#include "pthread.h"

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
 * @brief The size of the pool
 */
#define POOL_SIZE 100

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
static unsigned int next_id = INIT_ID;

/**
 * @brief Pointer to the current thread
 */
__thread lwt_t current_thread = NULL;
/**
 * @brief Pointer to the original/main thread
 */
__thread lwt_t original_thread = NULL;

/**
 * @brief List of all active threads created
 */
__thread LIST_HEAD(head_current, lwt) head_current;
/**
 * @brief Head of the list of all runnable threads
 */
__thread lwt_t runnable_threads_head = NULL;

/**
 * @brief Tail of the list of all runnable threads
 */
__thread lwt_t runnable_threads_tail = NULL;


/**
 * @brief Head of ready pool threads
 */
__thread lwt_t ready_pool_threads_head = NULL;

/**
 * @brief Tail of ready pool threads
 */
__thread lwt_t ready_pool_threads_tail = NULL;

/**
 * @brief Counter for the id
 * @return The next id to use
 */
static inline int get_new_id(){
	return next_id++;
}


/**
 * @brief Inserts the given thread to the head of the runnable thread list
 * @param thread The thread to be inserted
 */
static void insert_runnable_head(lwt_t thread){
	if(runnable_threads_head){
		//thread will be new head
		thread->next_runnable = runnable_threads_head;
		thread->previous_runnable = NULL;
		runnable_threads_head->previous_runnable = thread;
		runnable_threads_head = thread;
	}
	else{
		//thread will be head and tail
		thread->previous_runnable = NULL;
		thread->next_runnable = NULL;
		runnable_threads_head = thread;
		runnable_threads_tail = thread;
	}
}

/**
 * @brief Inserts the thread to the head of the pool
 * @param thread The thread to be inserted
 */
static void insert_ready_pool_thread_head(lwt_t thread){
	if(ready_pool_threads_head){
		//thread will be new head
		thread->previous_ready_pool_thread = NULL;
		thread->next_ready_pool_thread = ready_pool_threads_head;
		ready_pool_threads_head->previous_ready_pool_thread = thread;
		ready_pool_threads_head = thread;
	}
	else{
		//thread will be new head and tail
		thread->previous_ready_pool_thread = NULL;
		thread->next_ready_pool_thread = NULL;
		ready_pool_threads_head = thread;
		ready_pool_threads_tail = thread;
	}
}


/**
 * @brief Inserts the given thread to the tail of the runnable thread list
 * @param thread The new thread to be inserted in the list of runnable threads
 */
void __insert_runnable_tail(lwt_t thread){
	if(runnable_threads_tail){
		//thread will be new tail
		thread->previous_runnable = runnable_threads_tail;
		thread->next_runnable = NULL;
		runnable_threads_tail->next_runnable = thread;
		runnable_threads_tail = thread;
	}
	else{
		//thread will be head and tail
		thread->previous_runnable = NULL;
		thread->next_runnable = NULL;
		runnable_threads_head = thread;
		runnable_threads_tail = thread;
	}
}

/**
 * @brief Inserts the pool thread at the tail of the ready pool
 * @param thread The thread to be pushed
 */
static void insert_ready_pool_thread_tail(lwt_t thread){
	if(ready_pool_threads_tail){
		//thread will be new tail
		thread->next_ready_pool_thread = NULL;
		thread->previous_ready_pool_thread = ready_pool_threads_tail;
		ready_pool_threads_tail->next_ready_pool_thread = thread;
		ready_pool_threads_tail = thread;
	}
	else{
		//thread will be head and tail
		thread->previous_ready_pool_thread = NULL;
		thread->next_ready_pool_thread = NULL;
		ready_pool_threads_head = thread;
		ready_pool_threads_tail = thread;
	}
}

/**
 * @brief Removes the thread from the list of runnable threads
 * @param thread The thread to be removed
 */
static void remove_from_runnable_threads(lwt_t thread){
	//detach
	if(thread->next_runnable){
		thread->next_runnable->previous_runnable = thread->previous_runnable;
	}
	if(thread->previous_runnable){
		thread->previous_runnable->next_runnable = thread->next_runnable;
	}
	//update head
	if(thread == runnable_threads_head){
		runnable_threads_head = runnable_threads_head->next_runnable;
	}
	//update tail
	if(thread == runnable_threads_tail){
		runnable_threads_tail = runnable_threads_tail->previous_runnable;
	}
}

/**
 * @brief Removes the thread from the ready pool
 * @param thread The thread to be removed
 */
static void remove_ready_pool_thread(lwt_t thread){
	if(thread->next_ready_pool_thread){
		thread->next_ready_pool_thread->previous_ready_pool_thread = thread->previous_ready_pool_thread;
	}
	if(thread->previous_ready_pool_thread){
		thread->previous_ready_pool_thread->next_ready_pool_thread = thread->next_ready_pool_thread;
	}
	//adjust head
	if(thread == ready_pool_threads_head){
		ready_pool_threads_head = ready_pool_threads_head->next_ready_pool_thread;
	}
	//adjust tail
	if(thread == ready_pool_threads_tail){
		ready_pool_threads_tail = ready_pool_threads_tail->previous_ready_pool_thread;
	}
}

/**
 * @brief Gets the thread id
 * @return The id of the thread
 */
int inline lwt_id(lwt_t thread){
	return thread->id;
}


/**
 * @brief Gets the current thread
 * @return The current thread
 */
lwt_t inline lwt_current(){
	return current_thread;
}

/**
 * @brief Gets the counts of the info
 * @param t The info enum to get the counts
 * @return The count for the info enum provided
 * @see lwt_info_t
 */
int lwt_info(lwt_info_t t){
	int count = 0;
	lwt_t current_thread = head_current.lh_first;
	if(t == LWT_INFO_NCHAN){
		while(current_thread){
			lwt_chan_t current_channel = current_thread->head_receiver_channel.lh_first;
			while(current_channel){
				count++;
				current_channel = current_channel->receiver_channels.le_next;
			}
			current_thread = current_thread->current_threads.le_next;
		}
	}
	else{
		while(current_thread){
			if(current_thread->info == t){
				count++;
			}
			current_thread = current_thread->current_threads.le_next;
		}
	}
	return count;
}

/**
 * @brief Initializes the main thread
 * @param thread The main thread
 */
void __init_lwt_main(lwt_t thread){
	thread->id = get_new_id();
	//set the original stack to the current stack pointer
	register long sp asm("esp");
	thread->min_addr_thread_stack = (long *)(sp - STACK_SIZE);
	thread->max_addr_thread_stack = (long *)sp;
	thread->thread_sp = thread->max_addr_thread_stack;

	thread->id = get_new_id();

	thread->parent = NULL;
	LIST_INIT(&thread->head_children);

	thread->next_runnable = NULL;
	thread->previous_runnable = NULL;

	//add to current threads
	LIST_INIT(&head_current);
	LIST_INSERT_HEAD(&head_current, thread, current_threads);

	//set current thread
	current_thread = thread;
	original_thread = thread;

	//init receiving channels
	LIST_INIT(&thread->head_receiver_channel);

	__init_kthd(thread);
	LIST_INSERT_HEAD(&__get_kthd()->head_lwts_in_kthd, thread, lwts_in_kthd);

	thread->info = LWT_INFO_NTHD_RUNNABLE;
}

/**
 * @brief Initializes the provided thread
 * @param thread The thread to init
 */
void __init_new_lwt(lwt_t thread){

	thread->min_addr_thread_stack = __lwt_stack_get();
	thread->max_addr_thread_stack = (long *)(thread->min_addr_thread_stack + STACK_SIZE);
	assert(thread->max_addr_thread_stack);

	//init head to children
	LIST_INIT(&thread->head_children);

	//init receiving channels
	LIST_INIT(&thread->head_receiver_channel);

	//add to the list of threads
	LIST_INSERT_HEAD(&head_current, thread, current_threads);
}

/**
 * @brief Reinitializes the given thread
 * @param thread The thread to reinitialize
 */
void __reinit_lwt(lwt_t thread){
	//set id
	thread->id = get_new_id(); //return id and increment TODO implement atomically

	thread->thread_sp = thread->max_addr_thread_stack - 1;
	//add the function
	*(thread->thread_sp--) = (long)(__lwt_trampoline);

	*(thread->thread_sp--) = (long)0; //ebp
	*(thread->thread_sp--) = (long)0;//ebx
	*(thread->thread_sp--) = (long)0;//edi
	*(thread->thread_sp) = (long)0;//esi

	//set up parent
	thread->parent = NULL;

	//check that there are no children
	assert(!thread->head_children.lh_first);

	thread->previous_ready_pool_thread = NULL;
	thread->next_ready_pool_thread = NULL;

	//check that there are no channels
	assert(!thread->head_receiver_channel.lh_first);

	//reset flags to 0
	thread->flags = LWT_JOIN;

	//add to ready pool
	thread->info = LWT_INFO_NTHD_READY_POOL;
	insert_ready_pool_thread_tail(thread);
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
	 //wait until there's a job available
	 assert(current_thread->start_routine);
	 void * value = current_thread->start_routine(current_thread->args);
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
	//reinit thread
	__reinit_lwt(lwt);
}

/**
 * @brief Joins the provided thread
 * @param thread The thread to join on
 */
void * lwt_join(lwt_t thread){
	//ensure current thread isn't thread
	assert(current_thread != thread);
	//ensure thread isn't main thread
	assert(thread != original_thread);
	//block if the thread hasn't returned yet
	while(thread->info != LWT_INFO_NTHD_ZOMBIES){
		current_thread->info = LWT_INFO_NTHD_BLOCKED;
		//switch to another thread
		lwt_yield(LWT_NULL);
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
	current_thread->return_value = value;
	//check to see if we can return
	while(current_thread->head_children.lh_first){
		current_thread->info = LWT_INFO_NTHD_BLOCKED;
		lwt_yield(LWT_NULL);
	}
	//remove from parent thread
	if(current_thread->parent){
		LIST_REMOVE(current_thread, siblings);

		//check if parent can be unblocked
		if(!current_thread->parent->head_children.lh_first && current_thread->parent->info == LWT_INFO_NTHD_BLOCKED){
			current_thread->parent->info = LWT_INFO_NTHD_RUNNABLE;
			__insert_runnable_tail(current_thread->parent);
		}
	}

	//reset thread as ready in the thread pool
	if(current_thread->flags == LWT_NOJOIN){
		__reinit_lwt(current_thread);
	}
	else{
		//change status to zombie
		current_thread->info = LWT_INFO_NTHD_ZOMBIES;
	}

	//remove from kthd
	if(current_thread->kthd){
		LIST_REMOVE(current_thread, lwts_in_kthd);
	}

	//switch to another thread
	lwt_yield(LWT_NULL);
}

/**
 * @brief Yields to the provided LWT
 * @param lwt The thread to yield to
 * @note Will just schedule normally if LWT_NULL is provided
 * @return 0 if successful
 */
int lwt_yield(lwt_t lwt){
	assert(lwt != current_thread); //ensure current thread isn't being yielded to itself
	if(lwt == LWT_NULL){
		__lwt_schedule();
	}
	else{
		lwt_t curr_thread = current_thread;
		current_thread = lwt;
		//remove it from the runqueue
		remove_from_runnable_threads(lwt);
		//put it out in front
		insert_runnable_head(curr_thread);
		__lwt_dispatch(lwt, curr_thread);
		//__lwt_trampoline();
	}
	return 0;
}

/**
 * @brief Schedules the next_current thread to switch to and dispatches
 */
void __lwt_schedule(){
	//assert(runnable_threads_head);
	//assert(runnable_threads_head != current_thread);
	if(runnable_threads_head &&
			runnable_threads_head != current_thread){
		lwt_t curr_thread = current_thread;
		//move current thread to the end of the queue
		if(current_thread->info == LWT_INFO_NTHD_RUNNABLE){
			//push current thread to end of runnable
			__insert_runnable_tail(current_thread);
		}
		//pop the queue
		lwt_t next_thread = runnable_threads_head;
		remove_from_runnable_threads(next_thread);
		assert(next_thread);
		current_thread = next_thread;
		__lwt_dispatch(next_thread, curr_thread);
	}
}

void * lwt_buffer(void * d){
	struct kthd_event * event;
	lwt_kthd_t pthread_kthd = __get_kthd();
	while(lwt_current()->kthd){
		pthread_mutex_lock(&pthread_kthd->blocked_mutex);

		event = __pop_from_buffer(pthread_kthd);
		while(!event){
			pthread_kthd->is_blocked = 1;
			pthread_cond_wait(&pthread_kthd->blocked_cv, &pthread_kthd->blocked_mutex);
			pthread_kthd->is_blocked = 0;
			if(!lwt_current()->kthd){
				break;
			}
			event = __pop_from_buffer(pthread_kthd);
		}

		//ignore break condition
		if(event){
			//wait until thread is runnable
			/*while(event->lwt->info == LWT_INFO_NTHD_RUNNABLE){
				lwt_yield(LWT_NULL);
			}*/
			event->lwt->info = event->new_info;
			lwt_yield(event->lwt);
			free(event);
		}

		pthread_mutex_unlock(&pthread_kthd->blocked_mutex);
	}
	return NULL;
}

/**
 * @brief Initializes the LWT by wrapping the current thread as a LWT
 */
__attribute__((constructor)) void __init__(){
	//assert pool size >= 1
	assert(POOL_SIZE >= 1);
	lwt_t curr_thread = (lwt_t)malloc(sizeof(struct lwt));
	assert(curr_thread);
	__init_lwt_main(curr_thread);
	//set up pool
	int num_threads;
	lwt_t new_pool_thread;
	for(num_threads = 0; num_threads < POOL_SIZE; ++num_threads){
		new_pool_thread = (lwt_t)malloc(sizeof(struct lwt));
		assert(new_pool_thread);
		__init_new_lwt(new_pool_thread);
		__reinit_lwt(new_pool_thread);
	}
	//buffer thread is special
	lwt_kthd_t pthread_kthd = __get_kthd();
	pthread_kthd->buffer_thread = lwt_create(lwt_buffer, NULL, LWT_NOJOIN);
	remove_from_runnable_threads(pthread_kthd->buffer_thread);
	LIST_REMOVE(pthread_kthd->buffer_thread, current_threads);
	LIST_REMOVE(pthread_kthd->buffer_thread, siblings);
	LIST_REMOVE(pthread_kthd->buffer_thread, lwts_in_kthd);
	//pthread_kthd->buffer_thread->info = LWT_INFO_NTHD_BLOCKED;
	//set up mutex and cv
	pthread_mutex_init(&pthread_kthd->blocked_mutex, NULL);
	pthread_cond_init(&pthread_kthd->blocked_cv, NULL);
}

/**
 * @brief Cleans up all remaining threads on exit
 */
__attribute__((destructor)) void __destroy__(){
	lwt_kthd_t pthread_kthd = __get_kthd();
	//clean up buffer thread
	if(pthread_kthd->buffer_thread){
		pthread_kthd->buffer_thread->kthd = NULL;
		lwt_yield(pthread_kthd->buffer_thread);
		free(pthread_kthd->buffer_thread);
	}
	//free threads
	lwt_t current = head_current.lh_first;
	lwt_t next = NULL;
	lwt_chan_t rcv_channels = NULL;
	lwt_chan_t next_channel = NULL;

	//check for no-joined threads and switch to them to let them die
	while(current){
		next = current->current_threads.le_next;
		if(current->flags == LWT_NOJOIN){
			lwt_yield(current);
		}

		current = next;
	}

	current = head_current.lh_first;
	next = NULL;

	while(current){
		next = current->current_threads.le_next;

		//remove any channels
		rcv_channels = current->head_receiver_channel.lh_first;
		while(rcv_channels){
			next_channel = rcv_channels->receiver_channels.le_next;
			//free buffer
			if(rcv_channels->async_buffer){
				free(rcv_channels->async_buffer);
			}
			//free group
			lwt_cgrp_t group = rcv_channels->channel_group;
			if(group && group->creator_thread == current){
				//free the buffer
				while(group->head_event.tqh_first){
					TAILQ_REMOVE(&group->head_event, group->head_event.tqh_first, events);
				}
				free(group);
			}

			free(rcv_channels);
			rcv_channels = next_channel;
		}


		if(current != original_thread){
			//printf("FREEING STACK!!\n");
			//remove stack
			__lwt_stack_return(current->min_addr_thread_stack);
			free(current);
		}

		current = next;
	}

	//free original thread
	free(original_thread);
	//free kthd
	pthread_cond_destroy(&pthread_kthd->blocked_cv);
	pthread_mutex_destroy(&pthread_kthd->blocked_mutex);
	free(pthread_kthd);

	pthread_exit(0);
}

/**
 * @brief Creates a LWT using the provided function pointer and the data as input for it
 * @param fn The function pointer to use
 * @param data The data to the function
 * @param flags The flags to be associated with the thread
 * @return A pointer to the initialized LWT
 */
lwt_t lwt_create(lwt_fnt_t fn, void * data, lwt_flags_t flags){
	//wait until there's a free thread
	while(!ready_pool_threads_head){
		lwt_yield(LWT_NULL);
	}

	//pop the head of the ready pool list
	lwt_t thread = ready_pool_threads_head;
	remove_ready_pool_thread(thread);
	//set thread's parent
	thread->parent = current_thread;
	//insert into parent's siblings
	LIST_INSERT_HEAD(&current_thread->head_children, thread, siblings);
	//set status
	thread->info = LWT_INFO_NTHD_RUNNABLE;

	thread->start_routine = fn;
	thread->args = data;
	thread->flags = flags;

	//associate with kthd
	lwt_kthd_t pthread_kthd = __get_kthd();
	thread->kthd = pthread_kthd;
	LIST_INSERT_HEAD(&pthread_kthd->head_lwts_in_kthd, thread, lwts_in_kthd);

	//insert into runnable list
	__insert_runnable_tail(thread);

	return thread;
}
