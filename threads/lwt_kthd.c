/*
 * lwt_kthd.c
 *
 *  Created on: Apr 12, 2015
 *      Author: vagrant
 */
#include "lwt_kthd.h"
#include "lwt.h"
#include "lwt_chan.h"
#include "assert.h"
#include "pthread.h"
#include "faa.h"
#include "stdio.h"

/**
 * @brief Pointer to the kthd for the pthread
 */
__thread lwt_kthd_t pthread_kthd;

/**
 * @brief Function for the kthd (i.e. pthread) LWT wrapper to perform
 * @param data The kthd data used for storing the params for the create chan call
 * @return NULL
 */
void * pthread_function(void * data){
	__init__();
	struct lwt_kthd_data * thd_data = (struct lwt_kthd_data *)data;
	lwt_t lwt = lwt_create_chan(thd_data->channel_fn, thd_data->channel, thd_data->flags);
	assert(lwt);
	thd_data->ready = 1;
	lwt_signal(thd_data->parent);
	while(pthread_kthd->head_lwts_in_kthd.lh_first){
		//blocked until there's no more lwts in the kthd
		lwt_block(LWT_INFO_NTHD_BLOCKED);
	}
	__destroy__();
	return NULL;
}

/**
 * @brief Creates an N:M kthd
 * @param fn The channel function to run on the remote kthd
 * @param c The channel used as input to that function
 * @param flags The flags for the function
 */
int lwt_kthd_create(lwt_chan_fn_t fn, lwt_chan_t c, lwt_flags_t flags){
	struct lwt_kthd_data data;
	data.channel_fn = fn;
	data.channel = c;
	data.flags = flags;
	data.ready = 0;
	data.parent = lwt_current();

	pthread_attr_t attr;
	pthread_t thread;

	if(pthread_attr_init(&attr)){
		return -1;
	}
	if(pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED)){
		return -1;
	}
	if(pthread_create(&thread, &attr, &pthread_function, &data)){
		return -1;
	}
	while(!data.ready){
		lwt_block(LWT_INFO_NTHD_BLOCKED);
	}
	if(pthread_attr_destroy(&attr)){
		return -1;
	}
	return 0;
}

/**
 * @brief Pops a kthd event from the buffer
 * @param kthd The kthd to pop
 * @return The kthd event for the action to perform in the reaper function
 */
struct kthd_event * __pop_from_buffer(lwt_kthd_t kthd){
	//return null if there's no data; we're to assume that the buffer is sufficiently large
	if(kthd->buffer_head >= kthd->buffer_tail){
		return NULL;
	}
	//grab the current head
	unsigned int head = fetch_and_add(&kthd->buffer_head, 1);
	head = head % EVENT_BUFFER_SIZE;
	struct kthd_event * data = kthd->event_buffer[head];
	kthd->event_buffer[head] = NULL;
	return data;
}

/**
 * @brief Pushes a kthd event into the event buffer
 * @param kthd The kthd to modify
 * @param data The data to insert
 * @return 0 if successful; -1 if not
 */
int __push_to_buffer(lwt_kthd_t kthd, struct kthd_event * data){
	//check if full; we're to asssume the buffer is sufficiently large
	if(kthd->buffer_tail >= kthd->buffer_head + EVENT_BUFFER_SIZE){
		return -1;
	}
	unsigned int tail = fetch_and_add(&kthd->buffer_tail, 1) % EVENT_BUFFER_SIZE;
	kthd->event_buffer[tail] = data;
	//wake up the pthread
	pthread_mutex_lock(&kthd->blocked_mutex);
	if(kthd->is_blocked){
		kthd->is_blocked = 0;
		pthread_cond_signal(&kthd->blocked_cv);
	}
	pthread_mutex_unlock(&kthd->blocked_mutex);
	return 0;
}

/**
 * @brief Initializes a kthd
 * @param lwt The lwt for the kthd
 */
void __init_kthd(lwt_t lwt){
	//ensure block is set to 0's
	pthread_kthd = (lwt_kthd_t)calloc(1, sizeof(struct lwt_kthd));
	assert(pthread_kthd);
	pthread_kthd->pthread = pthread_self();
	pthread_kthd->is_blocked = 0;
	pthread_kthd->buffer_head = 0;
	pthread_kthd->buffer_tail = 0;
	LIST_INIT(&pthread_kthd->head_lwts_in_kthd);
	TAILQ_INIT(&pthread_kthd->head_runnable_threads);
}

/**
 * @brief Function for the reaper lwt; when all other lwts are blocked, processes events for the kthd
 * @param d Data; unused; needed to match file signature
 * @return NULL
 */
void * __lwt_buffer(void * d){
	struct kthd_event * event;
	while(lwt_current()->kthd){

		event = __pop_from_buffer(pthread_kthd);
		if(event){
			//printf("Received event: %d; on kthd: %d\n", (int)event, (int)pthread_kthd);
			switch(event->op){
			case LWT_REMOTE_SIGNAL:
					lwt_signal(event->lwt);
					break;
			case LWT_REMOTE_ADD_SENDER_TO_CHANNEL:
					__insert_sender_to_chan(event->channel, event->lwt);
					break;
			case LWT_REMOTE_REMOVE_SENDER_FROM_CHANNEL:
					__remove_sender_from_chan(event->channel, event->lwt);
					break;
			case LWT_REMOTE_ADD_BLOCKED_SENDER_TO_CHANNEL:
					__insert_blocked_sender_to_chan(event->channel, event->lwt);
					break;
			case LWT_REMOTE_REMOVE_BLOCKED_SENDER_FROM_CHANNEL:
					__remove_blocked_sender_from_chan(event->channel, event->lwt);
					break;
			case LWT_REMOTE_ADD_CHANNEL_TO_GROUP:
					lwt_cgrp_add(event->group, event->channel);
					break;
			case LWT_REMOTE_REMOVE_CHANNEL_FROM_GROUP:
					lwt_cgrp_rem(event->group, event->channel);
					break;
			case LWT_REMOTE_ADD_EVENT_TO_GROUP:
					__init_event(event->channel);
					break;
			case LWT_REMOTE_REMOVE_EVENT_FROM_GROUP:
					__remove_event(event->channel, event->group);
					break;
			default:
				perror("Unknown op provided\n");
			}
			event->is_done = 1;
			if(event->block){
				lwt_signal(event->originator);
			}
			else{
				free(event);
			}
		}
		else{
			pthread_mutex_lock(&pthread_kthd->blocked_mutex);
			//printf("Putting pthread to sleep on kthd: %d\n", (int)pthread_kthd);
			pthread_kthd->is_blocked = 1;
			pthread_cond_wait(&pthread_kthd->blocked_cv, &pthread_kthd->blocked_mutex);
			pthread_mutex_unlock(&pthread_kthd->blocked_mutex);
		}
		lwt_block(LWT_INFO_REAPER_READY);
	}
	return NULL;
}

/**
 * @brief Helper method for returning the current kthd
 * @return The current kthd
 */
lwt_kthd_t __get_kthd(){
	return pthread_kthd;
}

/**
 * @brief Initializes a kthd event
 * @param remote_lwt The lwt to modify
 * @param remote_chan The channel to modify
 * @param remote_group The group to modify
 * @param kthd The kthd to modify
 * @param remote_op The operation to perform
 * @param block Is the operation blocking (generally yes; signal is not)
 */
void __init_kthd_event(lwt_t remote_lwt, lwt_chan_t remote_chan, lwt_cgrp_t remote_group, lwt_kthd_t kthd, lwt_remote_op_t remote_op, int block){
	lwt_t current = lwt_current();
	struct kthd_event * event = (struct kthd_event *)malloc(sizeof(struct kthd_event));
	assert(event);
	event->lwt = remote_lwt;
	event->channel = remote_chan;
	event->originator = current;
	assert(event->originator);
	//assert(event->originator->info == LWT_INFO_NTHD_RUNNABLE);
	event->kthd = kthd;
	event->op = remote_op;
	event->is_done = 0;
	event->block = block;
	/*char * op;
	switch(remote_op){
		case LWT_REMOTE_SIGNAL:
			op = "Signal";
			break;
		case LWT_REMOTE_ADD_SENDER_TO_CHANNEL:
			op = "Add sender to channel";
			break;
		case LWT_REMOTE_REMOVE_SENDER_FROM_CHANNEL:
			op = "Remove sender from channel";
			break;
		case LWT_REMOTE_ADD_BLOCKED_SENDER_TO_CHANNEL:
			op = "Add blocked sender to channel";
			break;
		case LWT_REMOTE_REMOVE_BLOCKED_SENDER_FROM_CHANNEL:
			op = "Remove blocked sender from channel";
			break;
		case LWT_REMOTE_ADD_CHANNEL_TO_GROUP:
			op = "Remote add channel to group";
			break;
		case LWT_REMOTE_REMOVE_CHANNEL_FROM_GROUP:
			op = "Remote remove channel from group";
			break;
		case LWT_REMOTE_ADD_EVENT_TO_GROUP:
			op = "Remote add event to group";
			break;
		case LWT_REMOTE_REMOVE_EVENT_FROM_GROUP:
			op = "Remote remove event from group";
			break;
		default:
			op = "Unknown";
	}
	printf("Created event: %d for op: %s; target lwt: %d; target kthd: %d\n", (int)event, op, (int)remote_lwt, (int)kthd);
	*/
	int result = __push_to_buffer(event->kthd, event);
	while(result != 0){
		lwt_yield(LWT_NULL);
		result = __push_to_buffer(event->kthd, event);
	}
	while(block && event->is_done == 0){
		//printf("Waiting for return signal event\n");
		lwt_block(LWT_INFO_NTHD_BLOCKED);
	}
	if(block){
		//printf("Freeing event: %d\n", (int)event);
		free(event);
	}
}
