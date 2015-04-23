/*
 * lwt_chan.c
 *
 *  Created on: Mar 28, 2015
 *      Author: vagrant
 */
#include "lwt_chan.h"
#include "lwt.h"
#include "lwt_cgrp.h"

#include "objects.h"

#include "stdio.h"
#include "stdlib.h"
#include "assert.h"
#include "faa.h"



/**
 * @brief Pushes the data into the buffer
 * @param c The channel to add the data to
 * @param data The data to add
 * If the buffer is full, it will block until it has capacity
 */
static void push_data_into_async_buffer(lwt_chan_t c, void * data){
	//check that the buffer isn't at capacity
	__update_lwt_info(lwt_current(), LWT_INFO_NSENDING);
	while(c->num_entries >= c->buffer_size){
		TAILQ_INSERT_TAIL(&c->head_blocked_senders, lwt_current(), blocked_senders);
		lwt_yield(c->receiver);
	}
	//insert data into buffer
	unsigned int index = c->end_index % c->buffer_size;
	c->end_index++;
	c->async_buffer[index] = data;
	__init_event(c, data);
	//increment the num of entries
	c->num_entries++;
	//update status
	__update_lwt_info(lwt_current(), LWT_INFO_NTHD_RUNNABLE);
}

/**
 * @brief Pushes the data into the channel sync buffer
 * @param c The channel being modified
 * @param data The data being sent
 */
static int push_data_into_sync_buffer(lwt_chan_t c, void * data){
	//check if there is a receiver
	if(!c || !c->receiver){
		perror("No receiver for sending channel\n");
		return -1;
	}

	//block
	__update_lwt_info(lwt_current(), LWT_INFO_NSENDING);
	TAILQ_INSERT_TAIL(&c->head_blocked_senders, lwt_current(), blocked_senders);
	//check receiver is waiting
	while(c->receiver->info != LWT_INFO_NRECEIVING){
		//printf("Waiting on receiver in channel: %d\n", (int)c);
		lwt_yield(LWT_NULL);
	}
	//check if we can go ahead and send
	while(c->head_blocked_senders.tqh_first && c->head_blocked_senders.tqh_first != lwt_current()){
		//printf("Waiting on blocked senders head in channel: %d\n", (int)c);
		if(c->head_blocked_senders.tqh_first->kthd == lwt_current()->kthd){
			lwt_yield(c->head_blocked_senders.tqh_first);
		}
		else{
			lwt_yield(LWT_NULL);
		}
	}

	c->sync_buffer = data;
	//send data
	__init_event(c, data);


	//wait until receiver picks up buffer
	__update_lwt_info(c->receiver, LWT_INFO_NTHD_RUNNABLE);
	lwt_yield(c->receiver);
	return 0;
}

/**
 * @brief Pops the data into the buffer
 * @param c The channel to remove the data from
 * @param data The data to remove
 * If the buffer is empty, it will block until there is something to read
 */
void * __pop_data_from_async_buffer(lwt_chan_t c){
	__update_lwt_info(lwt_current(), LWT_INFO_NRECEIVING);
	while(c->num_entries <= 0){
		if(c->head_blocked_senders.tqh_first){
			lwt_t sender = c->head_blocked_senders.tqh_first;
			TAILQ_REMOVE(&c->head_blocked_senders, sender, blocked_senders);
			lwt_yield(sender);
		}
		else{
			lwt_yield(LWT_NULL);
		}
	}
	unsigned int index = c->start_index % c->buffer_size;
	void * data = c->async_buffer[index];
	c->start_index++;
	//update buffer value
	c->async_buffer[index] = NULL;
	//decrement the number of entries
	c->num_entries--;
	__update_lwt_info(lwt_current(), LWT_INFO_NTHD_RUNNABLE);
	return data;
}

/**
 * @brief Pops the data from the sync buffer
 * @param c The channel being examined
 */
static void * pop_data_from_sync_buffer(lwt_chan_t c){
	if(!c || !c->head_senders.lh_first){
		perror("NO Senders for receiving channel\n");
		return NULL;
	}
	__update_lwt_info(lwt_current(), LWT_INFO_NRECEIVING);
	//block until there's a sender
	while(!c->head_blocked_senders.tqh_first){
		//lwt_current()->info = LWT_INFO_NRECEIVING;
		//printf("Receiver waiting for sender in %d\n", (int)c);
		lwt_yield(LWT_NULL);
	}
	//detach the head
	lwt_t sender = c->head_blocked_senders.tqh_first;

	while(!c->sync_buffer){
		//lwt_current()->info = LWT_INFO_NRECEIVING;
		//printf("Receiver waiting for buffer to be read in: %d\n", (int)c);
		lwt_yield(sender);
	}

	TAILQ_REMOVE(&c->head_blocked_senders, sender, blocked_senders);
	__update_lwt_info(sender, LWT_INFO_NTHD_RUNNABLE);

	void * data = c->sync_buffer;
	c->sync_buffer = NULL;
	__update_lwt_info(lwt_current(), LWT_INFO_NTHD_RUNNABLE);

	return data;
}

/**
 * @brief Creates the channel on the receiving thread
 * @param sz The size of the buffer
 * @return A pointer to the initialized channel
 */
lwt_chan_t lwt_chan(int sz){
	assert(sz >= 0);
	lwt_chan_t channel = (lwt_chan_t)malloc(sizeof(struct lwt_channel));
	assert(channel);
	lwt_t current = lwt_current();
	channel->receiver = current;
	LIST_INSERT_HEAD(&current->head_receiver_channel, channel, receiver_channels);
	LIST_INIT(&channel->head_senders);
	channel->snd_cnt = 0;
	TAILQ_INIT(&channel->head_blocked_senders);
	//prepare buffer
	if(sz > 0){
		//ensure that the buffer is initialized to NULL
		channel->async_buffer = (void **)calloc(sz, sizeof(void *));
		assert(channel->async_buffer);
	}
	else{
		channel->async_buffer = NULL;
	}
	channel->sync_buffer = NULL;
	channel->start_index = 0;
	channel->end_index = 0;
	channel->buffer_size = sz;
	channel->num_entries = 0;
	//prepare group
	channel->channel_group = NULL;
	//mark
	channel->mark = NULL;
	return channel;
}


/**
 * @brief Sends the data over the channel to the receiver
 * @param c The channel to use for sending
 * @param data The data for sending
 * @return -1 if there is no receiver; 0 if successful
 */
int lwt_snd(lwt_chan_t c, void * data){
	//data must not be NULL
	assert(data);
	if(c->buffer_size > 0){
		push_data_into_async_buffer(c, data);
		return 0;
	}
	else{
		return push_data_into_sync_buffer(c, data);
	}
}


/**
 * @brief Sends sending over the channel c
 * @param c The channel to send sending across
 * @param sending The channel to send
 */
int lwt_snd_chan(lwt_chan_t c, lwt_chan_t sending){
	return lwt_snd(c, sending);
}

/**
 * @brief Receives the data over the channel
 * @param c The channel to use for receiving
 * @return The channel being sent over c
 */
lwt_chan_t lwt_rcv_chan(lwt_chan_t c){
	//add current channel to senders
	lwt_chan_t new_channel = (lwt_chan_t)lwt_rcv(c);
	LIST_INSERT_HEAD(&new_channel->head_senders, lwt_current(), senders);
	new_channel->snd_cnt++;
	return new_channel;
}


/**
 * @brief Deallocates the channel only if no threads still have references to the channel
 * @param c The channel to deallocate
 */
void lwt_chan_deref(lwt_chan_t c){
	if(c->receiver == lwt_current()){
		//printf("Removing receiver\n");
		LIST_REMOVE(c, receiver_channels);
		c->receiver = NULL;
	}
	else{
		LIST_REMOVE(lwt_current(), senders);
		c->snd_cnt--;
		//printf("Removing sender\n");
	}
	if(!c->receiver && !c->head_senders.lh_first){
		//printf("FREEING CHANNEL: %d!!\n", (int)c);
		if(c->async_buffer){
			free(c->async_buffer);
		}
		free(c);
	}
}

/**
 * @brief Receives the data from the channel and returns it
 * @param c The channel to receive from
 * @return The data from the channel
 */
void * lwt_rcv(lwt_chan_t c){
	//ensure only the thread creating the channel is receiving on it
	assert(c->receiver == lwt_current());
	if(c->buffer_size > 0){
		return __pop_data_from_async_buffer(c);
	}
	else{
		return pop_data_from_sync_buffer(c);
	}
}

/**
 * @brief Creates a lwt with the channel as an arg
 * @param fn The function to use to create the thread
 * @param c The channel to send
 * @param flags The flags for the thread
 * @return The thread to return
 */
lwt_t lwt_create_chan(lwt_chan_fn_t fn, lwt_chan_t c, lwt_flags_t flags){
	lwt_t new_thread = lwt_create((lwt_fnt_t)fn, (void*)c, flags);
	LIST_INSERT_HEAD(&c->head_senders, new_thread, senders);
	c->snd_cnt++;
	return new_thread;
}

