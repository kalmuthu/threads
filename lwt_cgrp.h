/*
 * lwt_cgrp.h
 *
 *  Created on: Mar 28, 2015
 *      Author: vagrant
 */

#ifndef LWT_CGRP_H_
#define LWT_CGRP_H_

#include "objects.h"

lwt_cgrp_t lwt_cgrp();
int lwt_cgrp_free(lwt_cgrp_t);
int lwt_cgrp_add(lwt_cgrp_t, lwt_chan_t);
int lwt_cgrp_rem(lwt_cgrp_t, lwt_chan_t);
lwt_chan_t lwt_cgrp_wait(lwt_cgrp_t);
void lwt_chan_mark_set(lwt_chan_t, void *);
void * lwt_chan_mark_get(lwt_chan_t);

//private functions
void __init_event(lwt_chan_t);
void __remove_event(lwt_chan_t, lwt_cgrp_t);

#endif /* LWT_CGRP_H_ */
