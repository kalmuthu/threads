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
 * Pointer to the kthd for the pthread
 */
__thread lwt_kthd_t pthread_kthd;

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

struct kthd_event * __pop_from_buffer(lwt_kthd_t kthd){
	//check if empty
	if(!kthd->event_buffer[kthd->buffer_head % EVENT_BUFFER_SIZE] &&
			kthd->buffer_head == kthd->buffer_tail){
		return NULL;
	}
	int head = fetch_and_add(&kthd->buffer_head, 1) % EVENT_BUFFER_SIZE;
	struct kthd_event * data = kthd->event_buffer[head];
	kthd->event_buffer[head] = NULL;
	return data;
}

int __push_to_buffer(lwt_kthd_t kthd, struct kthd_event * data){
	//check if full
	if(kthd->event_buffer[kthd->buffer_tail % EVENT_BUFFER_SIZE] != NULL &&
			kthd->buffer_head == kthd->buffer_tail){
		return -1;
	}
	int tail = fetch_and_add(&kthd->buffer_tail, 1) % EVENT_BUFFER_SIZE;
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

void __init_kthd(lwt_t lwt){
	//ensure block is set to 0's
	pthread_kthd = (lwt_kthd_t)calloc(1, sizeof(struct lwt_kthd));
	assert(pthread_kthd);
	pthread_kthd->pthread = pthread_self();
	pthread_kthd->is_blocked = 0;
	LIST_INIT(&pthread_kthd->head_lwts_in_kthd);
	TAILQ_INIT(&pthread_kthd->head_runnable_threads);
}

void * __lwt_buffer(void * d){
	struct kthd_event * event;
	while(lwt_current()->kthd){

		event = __pop_from_buffer(pthread_kthd);
		if(event){
			printf("Received event: %d; on kthd: %d\n", (int)event, (int)pthread_kthd);
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
			printf("Putting pthread to sleep on kthd: %d\n", (int)pthread_kthd);
			pthread_kthd->is_blocked = 1;
			pthread_cond_wait(&pthread_kthd->blocked_cv, &pthread_kthd->blocked_mutex);
			pthread_mutex_unlock(&pthread_kthd->blocked_mutex);
		}
		lwt_block(LWT_INFO_REAPER_READY);
	}
	return NULL;
}

lwt_kthd_t __get_kthd(){
	return pthread_kthd;
}

void __init_kthd_event(lwt_t remote_lwt, lwt_chan_t remote_chan, lwt_kthd_t kthd, lwt_remote_op_t remote_op, int block){
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
	char * op;
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
	__push_to_buffer(event->kthd, event);
	while(block && event->is_done == 0){
		printf("Waiting for return signal event\n");
		lwt_block(LWT_INFO_NTHD_BLOCKED);
	}
	if(block){
		printf("Freeing event: %d\n", (int)event);
		free(event);
	}
}
