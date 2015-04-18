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
	/**
	 * The previous event
	 */
	struct event * previous_event;
	/**
	 * The next event
	 */
	struct event * next_event;
	/**
	 * The channel with the new event
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
	 * Head of the list of channels
	 */
	lwt_chan_t channel_head;
	/**
	 * Tail of the list of channels
	 */
	lwt_chan_t channel_tail;
	/**
	 * Head of the event queue
	 */
	struct event * event_head;
	/**
	 * Tail of the event queue
	 */
	struct event * event_tail;
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
	 * The list of senders head
	 */
	lwt_t senders_head;
	/**
	 * The list of senders tail
	 */
	lwt_t senders_tail;
	/**
	 * The number of senders
	 */
	int snd_cnt;
	/**
	 * The head of the blocked senders
	 */
	lwt_t blocked_senders_head;
	/**
	 * The tail of the blocked senders
	 */
	lwt_t blocked_senders_tail;
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
	 * Previous sibling channel
	 */
	lwt_chan_t previous_sibling;
	/**
	 * Next sibling channel
	 */
	lwt_chan_t next_sibling;

	/**
	 * Channel group
	 */
	lwt_cgrp_t channel_group;
	/**
	 * Previous channel in group
	 */
	lwt_chan_t previous_channel_in_group;
	/**
	 * Next channel in group
	 */
	lwt_chan_t next_channel_in_group;
	/**
	 * Mark for channel
	 */
	void * mark;
};

struct kthd_event{
	void * data;
	lwt_cgrp_t group;
	lwt_chan_t channel;
	lwt_t owner;
};

struct lwt_kthd{
	pthread_t pthread;
	lwt_t lwt_head;
	lwt_t lwt_tail;
	int is_blocked;
	pthread_mutex_t blocked_mutex;
	pthread_cond_t blocked_cv;
	lwt_t buffer_thread;
	struct kthd_event * event_buffer[EVENT_BUFFER_SIZE];
	unsigned int buffer_head;
	unsigned int buffer_tail;
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
	 * List of children threads
	 */
	lwt_t children;
	/**
	 * Previous sibling
	 */
	lwt_t previous_sibling;
	/**
	 * Next sibling
	 */
	lwt_t next_sibling;

	/**
	 * Previous current thread
	 */
	lwt_t previous_current;
	/**
	 * Next current thread
	 */
	lwt_t next_current;

	/**
	 * Previous runnable thread
	 */
	lwt_t previous_runnable;
	/**
	 * Next runnable thread
	 */
	lwt_t next_runnable;

	/**
	 * Previous ready pool thread
	 */
	lwt_t previous_ready_pool_thread;
	/**
	 * Next ready pool thread
	 */
	lwt_t next_ready_pool_thread;

	/**
	 * Previous sender thread
	 */
	lwt_t previous_sender;
	/**
	 * Next sender thread
	 */
	lwt_t next_sender;
	/**
	 * Previous blocked sender thread
	 */
	lwt_t previous_blocked_sender;
	/**
	 * Next blocked sender thread
	 */
	lwt_t next_blocked_sender;
	/**
	 * List of receiving channels associated with the thread
	 */
	lwt_chan_t receiving_channels;

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
	 * Previous thread in kernel thread list
	 */
	lwt_t previous_kthd_thread;
	/**
	 * Next thread in kernel thread list
	 */
	lwt_t next_kthd_thread;
	/**
	 * Pointer to kthd
	 */
	lwt_kthd_t kthd;
};


#endif /* OBJECTS_H_ */
