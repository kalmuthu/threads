/*
 * lwt_kthd.c
 *
 *  Created on: Apr 12, 2015
 *      Author: vagrant
 */
#include "lwt_kthd.h"
#include "lwt_chan.h"
#include "assert.h"
#include "pthread.h"

/**
 * Pointer to the kthd for the pthread
 */
__thread lwt_kthd_t pthread_kthd;

static void insert_lwt_into_head(lwt_kthd_t kthd, lwt_t thread){
	if(kthd->lwt_head){
		//thread becomes new head
		thread->previous_kthd_thread = NULL;
		thread->next_kthd_thread = kthd->lwt_head;
		kthd->lwt_head->previous_kthd_thread = thread;
		kthd->lwt_head = thread;
	}
	else{
		//thread becomes head and tail
		thread->previous_kthd_thread = NULL;
		thread->next_kthd_thread = NULL;
		kthd->lwt_head = thread;
		kthd->lwt_tail = thread;
	}
}

void __insert_lwt_into_tail(lwt_kthd_t kthd, lwt_t thread){
	if(kthd->lwt_tail){
		//thread becomes new tail
		thread->next_kthd_thread = NULL;
		thread->previous_kthd_thread = kthd->lwt_tail;
		kthd->lwt_tail->next_kthd_thread = thread;
		kthd->lwt_tail = thread;
	}
	else{
		thread->next_kthd_thread = NULL;
		thread->previous_kthd_thread = NULL;
		kthd->lwt_head = thread;
		kthd->lwt_tail = thread;
	}
}

void __remove_thread_from_kthd(lwt_kthd_t kthd, lwt_t thread){
	if(thread->previous_kthd_thread){
		thread->previous_kthd_thread->next_kthd_thread = thread->next_kthd_thread;
	}
	if(thread->next_kthd_thread){
		thread->next_kthd_thread->previous_kthd_thread = thread->previous_kthd_thread;
	}
	//adjust head
	if(kthd->lwt_head == thread){
		kthd->lwt_head = kthd->lwt_head->next_kthd_thread;
	}
	//adjust tail
	if(kthd->lwt_tail == thread){
		kthd->lwt_tail = kthd->lwt_tail->previous_kthd_thread;
	}
}
void * pthread_function(void * data){
	__init__();
	struct lwt_kthd_data * thd_data = (struct lwt_kthd_data *)data;
	lwt_t lwt = lwt_create_chan(thd_data->channel_fn, thd_data->channel, thd_data->flags);
	assert(lwt);
	__init_kthd(lwt);
	while(pthread_kthd->lwt_head){
		lwt_yield(pthread_kthd->lwt_head);
	}
	__destroy__();
	free(pthread_kthd);
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
	if(pthread_attr_destroy(&attr)){
		return -1;
	}
	return 0;
}



void __init_kthd(lwt_t lwt){
	pthread_kthd = (lwt_kthd_t)malloc(sizeof(struct lwt_kthd));
	pthread_kthd->pthread = pthread_self();
	pthread_kthd->is_blocked = 0;
	pthread_kthd->lwt_head = NULL;
	pthread_kthd->lwt_tail = NULL;
}
