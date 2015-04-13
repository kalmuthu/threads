/*
 * lwt_kthd.h
 *
 *  Created on: Apr 11, 2015
 *      Author: vagrant
 */

#ifndef LWT_KTHD_H_
#define LWT_KTHD_H_

#include "pthread.h"
#include "lwt.h"
#include "enums.h"

//forward declaration
typedef struct lwt * lwt_t;
typedef struct lwt_channel *lwt_chan_t;
typedef void *(*lwt_chan_fn_t)(lwt_chan_t);

typedef struct lwt_kthd* lwt_kthd_t;

struct lwt_kthd{
	pthread_t pthread;
	lwt_t lwt_head;
	lwt_t lwt_tail;
	int is_blocked;
};

struct lwt_kthd_data{
	lwt_chan_fn_t channel_fn;
	lwt_chan_t channel;
	lwt_flags_t flags;
};

int lwt_kthd_create(lwt_chan_fn_t, lwt_chan_t, lwt_flags_t);

//package functions
void __init_kthd(lwt_t);
void __insert_lwt_into_tail(lwt_kthd_t, lwt_t);
void __remove_thread_from_kthd(lwt_kthd_t, lwt_t);

#endif /* LWT_KTHD_H_ */
