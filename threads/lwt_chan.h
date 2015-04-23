/*
 * lwt_chan.h
 *
 *  Created on: Mar 28, 2015
 *      Author: vagrant
 */

#ifndef LWT_CHAN_H_
#define LWT_CHAN_H_

#include "objects.h"

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
