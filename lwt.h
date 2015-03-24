/*
 * lwt.h
 *
 *  Created on: Feb 5, 2015
 *      Author: mtrotter
 */
#ifndef LWT_H_
#define LWT_H_

#include <stdlib.h>

#define PAGE_SIZE 4096
#define NUM_PAGES 5
#define STACK_SIZE PAGE_SIZE*NUM_PAGES

#define LWT_NULL NULL
#define LWT_YIELD_NO_LWT_TO_YIELD 1

/**
 * @brief The various statuses for a LWT
 */
typedef enum
{
	/**
	 * Thread state is runnable; it can be switched to
	 */
	LWT_INFO_NTHD_RUNNABLE,
	/**
	 * Thread state is blocked; waiting for another thread to complete
	 */
	LWT_INFO_NTHD_BLOCKED,
	/**
	 * Thread state is zombie; thread is dead and needs to be joined
	 */
	LWT_INFO_NTHD_ZOMBIES,
	/**
	 * Number of ready pool threads
	 */
	LWT_INFO_NTHD_READY_POOL,
	/**
	 * Number of channels that are active
	 */
	LWT_INFO_NCHAN,
	/**
	 * Number of threads blocked sending
	 */
	LWT_INFO_NSENDING,
	/**
	 * Number of threads blocked receiving
	 */
	LWT_INFO_NRECEIVING
} lwt_info_t;

typedef void *(*lwt_fnt_t)(void *); //function pointer definition

typedef struct lwt* lwt_t;
typedef struct lwt_channel *lwt_chan_t;
typedef struct lwt_cgrp *lwt_cgrp_t;

typedef void *(*lwt_chan_fn_t)(lwt_chan_t);

/**
 * flags for determining if the lwt is joinable
 */
typedef enum{
	/**
	 * lwt is joinable
	 */
	LWT_JOIN = 0,
	/**
	 * lwt is not joinable
	 */
	LWT_NOJOIN = 1
}lwt_flags_t;



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
};

struct event{
	struct event * previous_event;
	struct event * next_event;
	lwt_chan_t channel;
	void * data;
};

struct lwt_channel{
	/**
	 * The list of senders
	 */
	lwt_t senders;
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
	 * The blocked receiver
	 */
	lwt_t blocked_receiver;
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
	int start_index;
	/**
	 * End index of the buffer
	 */
	int end_index;
	/**
	 * Size of the buffer
	 */
	int buffer_size;
	/**
	 * Current number of entries in buffer
	 */
	int num_entries;
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
};


lwt_t lwt_create(lwt_fnt_t fn, void * data, lwt_flags_t flags);
void *lwt_join(lwt_t);
void lwt_die(void *);
int lwt_yield(lwt_t);
lwt_t lwt_current();
int lwt_id(lwt_t);
int lwt_info(lwt_info_t t);

lwt_chan_t lwt_chan(int);
void lwt_chan_deref(lwt_chan_t);
int lwt_snd(lwt_chan_t, void *);
void * lwt_rcv(lwt_chan_t);
int lwt_snd_chan(lwt_chan_t, lwt_chan_t);
lwt_chan_t lwt_rcv_chan(lwt_chan_t);
lwt_t lwt_create_chan(lwt_chan_fn_t, lwt_chan_t, lwt_flags_t);

lwt_cgrp_t lwt_cgrp();
int lwt_cgrp_free(lwt_cgrp_t);
int lwt_cgrp_add(lwt_cgrp_t, lwt_chan_t);
int lwt_cgrp_rem(lwt_cgrp_t, lwt_chan_t);
lwt_chan_t lwt_cgrp_wait(lwt_cgrp_t);
void lwt_chan_mark_set(lwt_chan_t, void *);
void * lwt_chan_mark_get(lwt_chan_t);

#endif /* LWT_H_ */
