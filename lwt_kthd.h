/*
 * lwt_kthd.h
 *
 *  Created on: Apr 11, 2015
 *      Author: vagrant
 */

#ifndef LWT_KTHD_H_
#define LWT_KTHD_H_

#include "objects.h"


int lwt_kthd_create(lwt_chan_fn_t, lwt_chan_t, lwt_flags_t);

//package functions
void __init_kthd(lwt_t);
void __insert_lwt_into_tail(lwt_kthd_t, lwt_t);
void __remove_thread_from_kthd(lwt_kthd_t, lwt_t);
struct kthd_event * __pop_from_buffer(lwt_kthd_t);
int __push_to_buffer(lwt_kthd_t, struct kthd_event *);

#endif /* LWT_KTHD_H_ */
