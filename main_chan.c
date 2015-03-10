#include "lwt.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>

#define ITER 10000
#define SZ 100

struct msort_args{
	int * data;
	int * swap;
	int begin_index;
	int end_index;
};

/**
 * Merge sort in parallel (adapted from wikipedia: http://en.wikipedia.org/wiki/Merge_sort#Parallel_merge_sort)
 */
void * msort(lwt_chan_t main_channel){
	//create channel
	lwt_chan_t my_channel = lwt_chan(0);
	//send channel back
	lwt_snd_chan(main_channel, my_channel);
	//receive args
	struct msort_args * args = lwt_rcv(my_channel);
	if(args->end_index - args->begin_index < 2){
		//send channels
		lwt_snd(main_channel, args);
		//dereference channels
		lwt_chan_deref(my_channel);
		lwt_chan_deref(main_channel);
		return 0;
	}
	int middle_index = (args->begin_index + args->end_index)/2;
	struct msort_args * l_args = (struct msort_args *)malloc(sizeof(struct msort_args));
	l_args->data = args->data;
	l_args->swap = args->swap;
	l_args->begin_index = args->begin_index;
	l_args->end_index = middle_index;
	struct msort_args * r_args = (struct msort_args *)malloc(sizeof(struct msort_args));
	r_args->data = args->data;
	r_args->swap = args->swap;
	r_args->begin_index = middle_index;
	r_args->end_index = args->end_index;
	//create threads
	lwt_t l_lwt = lwt_create_chan(msort, my_channel);
	lwt_t r_lwt = lwt_create_chan(msort, my_channel);
	//receive child channels
	lwt_chan_t l_chan = lwt_rcv_chan(my_channel);
	lwt_chan_t r_chan = lwt_rcv_chan(my_channel);
	//send args
	lwt_snd(l_chan, l_args);
	lwt_snd(r_chan, r_args);
	//receive new args
	assert(lwt_rcv(my_channel));
	assert(lwt_rcv(my_channel));
	//join threads
	assert(lwt_join(l_lwt) == 0);
	assert(lwt_join(r_lwt) == 0);
	//dereference channels
	lwt_chan_deref(l_chan);
	lwt_chan_deref(r_chan);
	lwt_chan_deref(my_channel);
	//free args
	free(l_args);
	free(r_args);
	//merge
	int index;
	int l_head = args->begin_index;
	int r_head = middle_index;
	for(index = args->begin_index; index < args->end_index; ++index){
		if(l_head < middle_index && (r_head >= args->end_index || args->data[l_head] <= args->data[r_head])){
			args->swap[index] = args->data[l_head];
			l_head++;
		}
		else{
			args->swap[index] = args->data[r_head];
			r_head++;
		}
	}
	//copy swap into data
	for(index = args->begin_index; index < args->end_index; ++index){
		args->data[index] = args->swap[index];
	}
	//send updated args back
	lwt_snd(main_channel, args);

	//dereference main channel
	lwt_chan_deref(main_channel);
	return 0;
}

void merge_sort(){
	struct msort_args * args = (struct msort_args *)malloc(sizeof(struct msort_args));
	args->data = (int *)malloc(sizeof(int) * SZ);
	args->swap = (int *)malloc(sizeof(int) * SZ);
	args->begin_index = 0;
	args->end_index = SZ;

	//prep data
	int index;
	srand(time(NULL));
	printf("Data is: [");
	for(index = 0; index < SZ; ++index){
		args->data[index] = rand();
		printf(" %d", args->data[index]);
	}
	printf("]\n");

	//create channel
	lwt_chan_t main_channel = lwt_chan(0);
	//create child worker
	lwt_t child = lwt_create_chan(msort, main_channel);
	//receive child channel
	lwt_chan_t child_channel = lwt_rcv_chan(main_channel);
	//send args
	lwt_snd(child_channel, args);
	//receive args
	assert(lwt_rcv(main_channel));
	//join child
	lwt_join(child);
	//dereference channel
	lwt_chan_deref(child_channel);
	lwt_chan_deref(main_channel);
	//validate args
	int prev = args->data[0];
	printf("Sorted data: [ %d", prev);
	for(index = 1; index < SZ; ++index){
		assert(args->data[index] >= prev);
		prev = args->data[index];
		printf(" %d", prev);
	}
	printf("]\n");
	//free data
	free(args->data);
	free(args->swap);
	free(args);
}

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

void test1(){
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
}

int main(){
	//test1();
	merge_sort();
	return 0;
}
