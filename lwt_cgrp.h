/*
 * lwt_cgrp.h
 *
 *  Created on: Mar 28, 2015
 *      Author: vagrant
 */

#ifndef LWT_CGRP_H_
#define LWT_CGRP_H_

#include "lwt_chan.h"

typedef struct lwt_cgrp *lwt_cgrp_t;
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

lwt_cgrp_t lwt_cgrp();
int lwt_cgrp_free(lwt_cgrp_t);
int lwt_cgrp_add(lwt_cgrp_t, lwt_chan_t);
int lwt_cgrp_rem(lwt_cgrp_t, lwt_chan_t);
lwt_chan_t lwt_cgrp_wait(lwt_cgrp_t);
void lwt_chan_mark_set(lwt_chan_t, void *);
void * lwt_chan_mark_get(lwt_chan_t);

//private functions
void __init_event(lwt_chan_t, void *);
void __pop_event(lwt_cgrp_t);

#endif /* LWT_CGRP_H_ */
