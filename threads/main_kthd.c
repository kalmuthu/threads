/*
 * main_kthd.c
 *
 *  Created on: Apr 20, 2015
 *      Author: vagrant
 */
#include "lwt_kthd.h"
#include "lwt_chan.h"
#include "lwt.h"

#include "stdio.h"
#include "assert.h"

#define MAX_PING_PONG_VALUE 100

#define ITER 10000

void * kthd_ping(lwt_chan_t ping_channel){
	lwt_chan_t pong_channel = lwt_chan(0);
	assert(pong_channel);
	lwt_snd_chan(ping_channel, pong_channel);
	int value = (int)lwt_rcv(pong_channel);
	while(value < MAX_PING_PONG_VALUE){
		printf("Received: %d at pong kthd\n", value);
		value++;
		lwt_snd(ping_channel, (void *)value);
		lwt_rcv(pong_channel);
	}
	return NULL;
}

void kthd_ping_pong_sync(){
	printf("Starting ping pong test\n");
	lwt_chan_t ping_channel = lwt_chan(0);
	assert(lwt_kthd_create(kthd_ping, ping_channel, LWT_NOJOIN) >= 0);
	lwt_chan_t pong_channel = lwt_rcv_chan(ping_channel);
	int value = 1;
	while(value < MAX_PING_PONG_VALUE){
		lwt_snd(pong_channel, (void *)value);
		value = (int)lwt_rcv(ping_channel);
		printf("Received %d at ping channel\n", value);
		value++;
	}
	assert(value >= MAX_PING_PONG_VALUE);
}

void *
fn_grpwait(lwt_chan_t c)
{
	assert(c->receiver != NULL);
	lwt_snd(c, (void *)lwt_id(lwt_current()));
	int i;

	for (i = 0 ; i < ITER ; i++) {
		if ((i % 7) == 0) {
			int j;

			for (j = 0 ; j < (i % 8) ; j++) lwt_yield(LWT_NULL);
		}
		lwt_snd(c, (void*)lwt_id(lwt_current()));
	}
	lwt_chan_deref(c);
}

#define GRPSZ 3

void
test_grpwait(int chsz, int grpsz)
{
	lwt_chan_t cs[grpsz];
	int ts[grpsz];
	int i;
	lwt_cgrp_t g;

	printf("[TEST] group wait (channel buffer size %d, grpsz %d)\n",
	       chsz, grpsz);
	g = lwt_cgrp();
	assert(g);

	for (i = 0 ; i < grpsz ; i++) {
		cs[i] = lwt_chan(chsz);
		assert(cs[i]);
		assert(!lwt_kthd_create(fn_grpwait, cs[i], 0));
		ts[i] = (int)lwt_rcv(cs[i]);
		lwt_chan_mark_set(cs[i], (void*)(ts[i]));
		lwt_cgrp_add(g, cs[i]);
	}
	//assert(lwt_cgrp_free(g) == -1);
	/**
	 * Q: why don't we iterate through all of the data here?
	 *
	 * A: We need to fix 1) cevt_wait to be level triggered, or 2)
	 * provide a function to detect if there is data available on
	 * a channel.  Either of these would allows us to iterate on a
	 * channel while there is more data pending.
	 */
	//for (i = 0 ; i < ((ITER * grpsz)-(grpsz*chsz)); i++) {
	for(i = 0; i < ITER * grpsz; i++){
		lwt_chan_t c;
		int r;
		c = lwt_cgrp_wait(g);
		assert(c);
		r = (int)lwt_rcv(c);
		assert(r == (int)lwt_chan_mark_get(c));
	}
	for (i = 0 ; i < grpsz ; i++) {
		lwt_cgrp_rem(g, cs[i]);
		lwt_chan_deref(cs[i]);
	}
	int return_code =lwt_cgrp_free(g);
	assert(!return_code);

	return;
}


int main(){
	kthd_ping_pong_sync();
	//test_grpwait(0, 3);
	test_grpwait(3, 3);
	return 0;
}
