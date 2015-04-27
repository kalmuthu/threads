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

int main(){
	kthd_ping_pong_sync();
	return 0;
}
