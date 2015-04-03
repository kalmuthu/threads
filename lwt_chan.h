/*
 * lwt_chan.h
 *
 *  Created on: Mar 28, 2015
 *      Author: vagrant
 */

#ifndef LWT_CHAN_H_
#define LWT_CHAN_H_

#include "lwt.h"

//forward declarations
typedef struct lwt_cgrp *lwt_cgrp_t;

typedef struct lwt_channel *lwt_chan_t;
typedef void *(*lwt_chan_fn_t)(lwt_chan_t);

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
	/**
	 * Currently has event
	 */
	int has_event;
};

lwt_chan_t lwt_chan(int);
void lwt_chan_deref(lwt_chan_t);
int lwt_snd(lwt_chan_t, void *);
void * lwt_rcv(lwt_chan_t);
int lwt_snd_chan(lwt_chan_t, lwt_chan_t);
lwt_chan_t lwt_rcv_chan(lwt_chan_t);
lwt_t lwt_create_chan(lwt_chan_fn_t, lwt_chan_t, lwt_flags_t);

//private functions
void __remove_channel(lwt_chan_t);
void * __pop_data_from_async_buffer(lwt_chan_t);
void __remove_from_blocked_sendera(lwt_chan_t, lwt_t);

#endif /* LWT_CHAN_H_ */
