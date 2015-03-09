#include "lwt.h"
#include <stdio.h>
#include <assert.h>

#define ITER 10000

void * child_ping(lwt_chan_t main_channel){
	printf("Starting child ping channel\n");
	//create the channel
	lwt_chan_t child_1_channel = lwt_chan(0);
	//send it to parent
	lwt_snd_chan(main_channel, child_1_channel);
	//receive children channels
	lwt_chan_t * children = (lwt_chan_t *)malloc(sizeof(lwt_chan_t) * ITER);
	int count = 1;
	int index;
	for(index = 0; index < ITER; ++index){
		children[index] = lwt_rcv_chan(child_1_channel);
	}
	while(count < 100){
		printf("PING COUNT: %d\n", count);
		//send count
		for(index = 0; index < ITER; ++index){
			lwt_snd(children[index], (void *)count);
		}
		printf("PONG COUNT\n");
		//receive count
		count = (int)lwt_rcv(child_1_channel);
		for(index = 1; index < ITER; ++index){
			assert(count == (int)lwt_rcv(child_1_channel));
		}
		if(count >= 100){
			break;
		}
		//update count
		count++;
	}
	lwt_chan_deref(main_channel);
	for(index = 0; index < ITER; ++index){
		lwt_chan_deref(children[index]);
	}
	free(children);
	lwt_chan_deref(child_1_channel);
	printf("CHILD 1 COUNT: %d\n", count);
	return 0;
}

void * child_pong(lwt_chan_t main_channel){
	printf("Starting child function 2\n");
	//create the channel
	lwt_chan_t child_2_channel = lwt_chan(0);
	//send it to parent
	lwt_snd_chan(main_channel, child_2_channel);
	//receive child 1 channel
	lwt_chan_t child_1_channel = lwt_rcv_chan(child_2_channel);
	int count = (int)lwt_rcv(child_2_channel);
	while(count < 100){
		printf("CHILD 2 COUNT: %d\n", count);
		//update count
		count++;
		printf("SENDING COUNT TO CHILD 1\n");
		lwt_snd(child_1_channel, (int)count);
		if(count >= 100){
			break;
		}
		printf("RECEVING COUNT FROM CHILD 1\n");
		count = (int)lwt_rcv(child_2_channel);
	}
	lwt_chan_deref(main_channel);
	lwt_chan_deref(child_1_channel);
	lwt_chan_deref(child_2_channel);
	printf("CHILD 2 COUNT: %d\n", count);
	return 0;
}


int main(){
	printf("Starting channels test\n");
	//create channel
	lwt_chan_t main_channel = lwt_chan(0);
	//create child threads
	lwt_t ping_lwt = lwt_create_chan(child_ping, main_channel);
	lwt_t * pong_lwts = (lwt_t *)malloc(sizeof(lwt_t) * ITER);
	int index;
	for(index = 0; index < ITER; ++index){
		pong_lwts[index] = lwt_create_chan(child_pong, main_channel);
	}
	//receive channels
	lwt_chan_t ping_channel = lwt_rcv_chan(main_channel);
	lwt_chan_t * pong_channels = (lwt_chan_t *)malloc(sizeof(lwt_chan_t) * ITER);
	for(index = 0; index < ITER; ++index){
		pong_channels[index] = lwt_rcv_chan(main_channel);
	}
	//send channels
	for(index = 0; index < ITER; ++index){
		lwt_snd_chan(pong_channels[index], ping_channel);
	}
	for(index = 0; index < ITER; ++index){
		lwt_snd_chan(ping_channel, pong_channels[index]);
	}

	//we're done with the channels
	lwt_chan_deref(ping_channel);
	for(index = 0; index < ITER; ++index){
		lwt_chan_deref(pong_channels[index]);
	}
	lwt_chan_deref(main_channel);
	free(pong_channels);

	//join threads
	lwt_join(ping_lwt);
	for(index = 0; index < ITER; ++index){
		lwt_join(pong_lwts[index]);
	}

	free(pong_lwts);

	return 0;
}
