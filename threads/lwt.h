/*
 * lwt.h
 *
 *  Created on: Feb 5, 2015
 *      Author: mtrotter
 */
#ifndef LWT_H_
#define LWT_H_

#include "objects.h"


lwt_t lwt_create(lwt_fnt_t, void *, lwt_flags_t);
void *lwt_join(lwt_t);
void lwt_die(void *);
int lwt_yield(lwt_t);
lwt_t lwt_current();
int lwt_id(lwt_t);
int lwt_info(lwt_info_t);

void __insert_runnable_tail(lwt_t);
void __init__();
void __destroy__();

#endif /* LWT_H_ */
