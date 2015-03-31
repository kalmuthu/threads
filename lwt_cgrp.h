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

struct event{
	struct event * previous_event;
	struct event * next_event;
	lwt_chan_t channel;
	void * data;
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
