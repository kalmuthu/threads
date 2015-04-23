/*
 * objects.h
 *
 *  Created on: Apr 14, 2015
 *      Author: vagrant
 */

#ifndef OBJECTS_H_
#define OBJECTS_H_

#include "pthread.h"
#include "stdlib.h"

#include <sys/queue.h>

#include "enums.h"

/**
 * Size of the event buffer
 */
#define EVENT_BUFFER_SIZE 100

/**
 * Size of the a page in the OS -> 4K
 */
#define PAGE_SIZE 4096
/**
 * Number of pages to allocate to the stack
 */
#define NUM_PAGES 5
/**
 * Size of the stack
 */
#define STACK_SIZE PAGE_SIZE*NUM_PAGES

/**
 * Null id for yields
 */
#define LWT_NULL NULL

typedef struct lwt_kthd* lwt_kthd_t;

typedef struct lwt_cgrp* lwt_cgrp_t;

typedef struct lwt_channel *lwt_chan_t;
typedef void *(*lwt_chan_fn_t)(lwt_chan_t);

typedef void *(*lwt_fnt_t)(void *); //function pointer definition

typedef struct lwt* lwt_t;

/**
 * @brief Event data
 */
struct event{
	TAILQ_ENTRY(event) events;
	/**
	 * The receiving channel with the new event
	 */
	lwt_chan_t channel;
	/**
	 * The data being added to the channel
	 */
	void * data;
};



/**
 * @brief Channel group for handling events within a group
 */
struct lwt_cgrp{
	/**
	 * Definition of the head in the channels
	 */
	LIST_HEAD(head_channels_in_group, lwt_channel) head_channels_in_group;
	/**
	 * Definition of the head node for the event queue
	 */
	TAILQ_HEAD(head_event, event) head_event;
	/**
	 * Waiting thread
	 */
	lwt_t waiting_thread;
	/**
	 * Creator thread
	 */
	lwt_t creator_thread;
};

/**
 * @brief The channel for synchronous and asynchronous communication
 */
struct lwt_channel{
	/**
	 * Definition of the senders head pointer
	 */
	LIST_HEAD(head_senders, lwt) head_senders;
	/**
	 * The number of senders
	 */
	int snd_cnt;
	/**
	 * Definition of the blocked senders head pointer
	 */
	TAILQ_HEAD(head_blocked_senders, lwt) head_blocked_senders;
	/**
	 * The receiving thread
	 */
	lwt_t receiver;
	/**
	 * Sync buffer to be passed to the channel
	 */
	void * sync_buffer;
	/**
	 * Async Buffer to be passed to the channel
	 */
	void ** async_buffer;
	/**
	 * Start index of the buffer
	 */
	unsigned int start_index;
	/**
	 * End index of the buffer
	 */
	unsigned int end_index;
	/**
	 * Size of the buffer
	 */
	unsigned int buffer_size;
	/**
	 * Current number of entries in buffer
	 */
	unsigned int num_entries;
	/**
	 * List of receiver channels in a lwt
	 */
	LIST_ENTRY(lwt_channel) receiver_channels;

	/**
	 * Channel group
	 */
	lwt_cgrp_t channel_group;
	/**
	 * Channels in group entries
	 */
	LIST_ENTRY(lwt_channel) channels_in_group;
	/**
	 * Mark for channel
	 */
	void * mark;
};

struct kthd_event{
	lwt_t lwt;
	lwt_info_t new_info;
};

struct lwt_kthd{
	pthread_t pthread;
	/**
	 * Point to the head of the list of lwts associated with a kthd
	 */
	LIST_HEAD(head_lwts_in_kthd, lwt) head_lwts_in_kthd;
	int is_blocked;
	pthread_mutex_t blocked_mutex;
	pthread_cond_t blocked_cv;
	lwt_t buffer_thread;
	struct kthd_event * event_buffer[EVENT_BUFFER_SIZE];
	volatile unsigned int buffer_head;
	volatile unsigned int buffer_tail;
};

struct lwt_kthd_data{
	lwt_chan_fn_t channel_fn;
	lwt_chan_t channel;
	lwt_flags_t flags;
};



/**
 * @brief The Lightweight Thread (LWT) struct
 */
struct lwt
{
	/**
	 * Pointer to the max address of the stack
	 */
	long * max_addr_thread_stack;
	/**
	 * Pointer to the min address of the statck; used for malloc and free
	 */
	long * min_addr_thread_stack;
	/**
	 * The current thread stack pointer for the thread
	 */
	long * thread_sp;
	/**
	 * The flags associated with the lwt
	 */
	lwt_flags_t flags;

	/**
	 * Parent thread
	 */
	lwt_t parent;
	/**
	 * Head of the list of children lwt's associated with the lwt
	 */
	LIST_HEAD(head_children, lwt) head_children;
	/**
	 * Pointers to sibling threads
	 */
	LIST_ENTRY(lwt) siblings;

	/**
	 * Pointers to the current threads
	 */
	LIST_ENTRY(lwt) current_threads;

	/**
	 * Pointers to runnable threads
	 */
	TAILQ_ENTRY(lwt) runnable_threads;

	/**
	 * Previous ready pool thread
	 */
	lwt_t previous_ready_pool_thread;
	/**
	 * Next ready pool thread
	 */
	lwt_t next_ready_pool_thread;

	/**
	 * List of senders
	 */
	LIST_ENTRY(lwt) senders;
	/**
	 * List of blocked senders
	 */
	TAILQ_ENTRY(lwt) blocked_senders;
	/**
	 * Head of the receiver channels associated with the lwt
	 */
	LIST_HEAD(head_receiver_channel, lwt_channel) head_receiver_channel;

	/**
	 * The start routine for the thread to run
	 */
	lwt_fnt_t start_routine;
	/**
	 * The args for the start_routine
	 */
	void * args;
	/**
	 * The return value from the routine
	 */
	void * return_value;

	/**
	 * The current status of the thread
	 */
	lwt_info_t info;

	/**
	 * The id of the thread
	 */
	int id;

	/**
	 * List of lwts in the kthd
	 */
	LIST_ENTRY(lwt) lwts_in_kthd;
	/**
	 * Pointer to kthd
	 */
	lwt_kthd_t kthd;
};


#endif /* OBJECTS_H_ */
