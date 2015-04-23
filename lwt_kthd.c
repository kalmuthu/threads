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
	while(pthread_kthd->head_lwt_in_kthd.lh_first){
		lwt_yield(LWT_NULL);
	}
	__destroy__();
	return NULL;
}

int lwt_kthd_create(lwt_chan_fn_t fn, lwt_chan_t c, lwt_flags_t flags){
	struct lwt_kthd_data data;
	data.channel_fn = fn;
	data.channel = c;
	data.flags = flags;

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
	//wait until there's a head
	while(!c->head_senders.lh_first){
		lwt_yield(LWT_NULL);
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
	if(kthd->event_buffer[kthd->buffer_tail % EVENT_BUFFER_SIZE] &&
			kthd->buffer_head == kthd->buffer_tail){
		return -1;
	}
	int tail = fetch_and_add(&kthd->buffer_tail, 1) % EVENT_BUFFER_SIZE;
	kthd->event_buffer[tail] = data;
	//wake up buffer thread
	pthread_mutex_lock(&kthd->blocked_mutex);
	if(kthd->is_blocked){
		kthd->is_blocked = 0;
		pthread_cond_signal(&kthd->blocked_cv);
	}
	pthread_mutex_unlock(&kthd->blocked_mutex);
	return 0;
}

static struct kthd_event * init_kthd_event(lwt_t requested_lwt, lwt_info_t new_info){
	struct kthd_event * kthd_event = (struct kthd_event *)malloc(sizeof(struct kthd_event));
	assert(kthd_event);
	kthd_event->lwt = requested_lwt;
	kthd_event->new_info = new_info;
	return kthd_event;
}

void __update_lwt_info(lwt_t requested_lwt, lwt_info_t new_info){
	//check if lwt is on same kthd
	if(requested_lwt->kthd == lwt_current()->kthd){
		//if yes -> no need to communicate via buffer
		requested_lwt->info = new_info;
	}
	else{
		//push kthd event
		struct kthd_event * kthd_event = init_kthd_event(requested_lwt, new_info);
		__push_to_buffer(requested_lwt->kthd, kthd_event);
	}
}

void __init_kthd(lwt_t lwt){
	//ensure block is set to 0's
	pthread_kthd = (lwt_kthd_t)calloc(1, sizeof(struct lwt_kthd));
	assert(pthread_kthd);
	pthread_kthd->pthread = pthread_self();
	lwt->kthd = pthread_kthd;
	LIST_INIT(&pthread_kthd->head_lwt_in_kthd);
}

lwt_kthd_t __get_kthd(){
	return pthread_kthd;
}
