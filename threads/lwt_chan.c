/*
 * lwt_chan.c
 *
 *  Created on: Mar 28, 2015
 *      Author: vagrant
 */
#include "lwt_chan.h"
#include "lwt.h"
#include "lwt_cgrp.h"
#include "lwt_kthd.h"

#include "objects.h"

#include "stdio.h"
#include "stdlib.h"
#include "assert.h"
#include "faa.h"

void __insert_sender_to_chan(lwt_chan_t chan, lwt_t lwt){
	if(__get_kthd() == chan->kthd){
		LIST_INSERT_HEAD(&chan->head_senders, lwt, senders);
		chan->snd_cnt++;
	}
	else{
		__init_kthd_event(lwt, chan, chan->receiver->kthd, LWT_REMOTE_ADD_SENDER_TO_CHANNEL, 1);
	}
}

void __remove_sender_from_chan(lwt_chan_t chan, lwt_t lwt){
	if(__get_kthd() == chan->kthd){
		LIST_REMOVE(lwt, senders);
		chan->snd_cnt--;
	}
	else{
		__init_kthd_event(lwt, chan, chan->receiver->kthd, LWT_REMOTE_REMOVE_SENDER_FROM_CHANNEL, 1);
	}
}

void __insert_blocked_sender_to_chan(lwt_chan_t chan, lwt_t lwt){
	if(__get_kthd() == chan->receiver->kthd){
		TAILQ_INSERT_TAIL(&chan->head_blocked_senders, lwt, blocked_senders);
	}
	else{
		__init_kthd_event(lwt, chan, chan->receiver->kthd, LWT_REMOTE_ADD_BLOCKED_SENDER_TO_CHANNEL, 1);
	}
}

void __remove_blocked_sender_from_chan(lwt_chan_t chan, lwt_t lwt){
	if(__get_kthd() == chan->receiver->kthd){
		TAILQ_REMOVE(&chan->head_blocked_senders, lwt, blocked_senders);
	}
	else{
		__init_kthd_event(lwt, chan, chan->receiver->kthd, LWT_REMOTE_REMOVE_BLOCKED_SENDER_FROM_CHANNEL, 1);
	}
}

/**
 * @brief Pushes the data into the buffer
 * @param c The channel to add the data to
 * @param data The data to add
 * If the buffer is full, it will block until it has capacity
 */
static void push_data_into_async_buffer(lwt_chan_t c, void * data){
	//check that the buffer isn't at capacity
	while(c->num_entries >= c->buffer_size){
		//printf("Blocking async sender: %d\n", lwt_current()->id);
		__insert_blocked_sender_to_chan(c, lwt_current());
		lwt_block(LWT_INFO_NSENDING);
	}
	//printf("Writing to buffer on lwt: %d\n", lwt_current()->id);
	//insert data into buffer
	unsigned int start_index = c->start_index;
	unsigned int num_entries = fetch_and_add(&c->num_entries, 1);
	unsigned int index = (start_index + num_entries) % c->buffer_size;
	c->async_buffer[index] = data;
	__init_event(c);
	//printf("Write complete\n");
	lwt_signal(c->receiver);
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

	lwt_current()->sync_buffer = data;
	//insert into blocked queue
	__insert_blocked_sender_to_chan(c, lwt_current());
	c->num_entries = 1;
	__init_event(c);
	if(c->receiver->info == LWT_INFO_NRECEIVING){
		//printf("Signaling receiver that data is ready\n");
		lwt_signal(c->receiver);
		lwt_yield(LWT_NULL);
	}
	//if receiver isn't waiting to receive block
	else{
	//while(lwt_current()->blocked_senders.tqe_next || c->head_blocked_senders.tqh_first == lwt_current()){
		//printf("Blocking sender\n");
		lwt_block(LWT_INFO_NSENDING);
	//}
	}



	//signal receiver
	//printf("Signaling receiver on lwt: %d\n", lwt_current()->id);

	//lwt_signal(c->receiver);
	return 0;
}

/**
 * @brief Pops the data into the buffer
 * @param c The channel to remove the data from
 * @param data The data to remove
 * If the buffer is empty, it will block until there is something to read
 */
void * __pop_data_from_async_buffer(lwt_chan_t c){
	while(c->num_entries <= 0){
		//printf("Blocking async receiver: %d\n", lwt_current()->id);
		lwt_block(LWT_INFO_NRECEIVING);
	}
	//printf("Reading in async receiver: %d\n", lwt_current()->id);
	unsigned int start_index = fetch_and_add(&c->start_index, 1);
	fetch_and_add(&c->num_entries, -1);
	unsigned int index = start_index % c->buffer_size;
	void * data = c->async_buffer[index];
	//update buffer value
	c->async_buffer[index] = NULL;
	//decrement the number of entries
	//c->num_entries--;
	//printf("Async receive complete!\n");
	lwt_t head_blocked_senders = c->head_blocked_senders.tqh_first;
	if(head_blocked_senders){
		__remove_blocked_sender_from_chan(c, head_blocked_senders);
		lwt_signal(head_blocked_senders);
	}
	return data;
}

/**
 * @brief Pops the data from the sync buffer
 * @param c The channel being examined
 */
static void * pop_data_from_sync_buffer(lwt_chan_t c){
	if(!c || c->snd_cnt <= 0){
		perror("NO Senders for receiving channel\n");
		return NULL;
	}
	//__update_lwt_info(lwt_current(), LWT_INFO_NRECEIVING);
	//block until there's a sender
	while(!c->head_blocked_senders.tqh_first){
		//printf("Receiver waiting for sender in %d\n", (int)c);
		lwt_block(LWT_INFO_NRECEIVING);
	}
	//detach the head
	lwt_t sender = c->head_blocked_senders.tqh_first;
	__remove_blocked_sender_from_chan(c, sender);

	void * data = sender->sync_buffer;
	sender->sync_buffer = NULL;
	assert(data);

	lwt_signal(sender);

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
	channel->kthd = current->kthd;
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
		channel->async_buffer = (void **)calloc(1, sizeof(void *));
	}
	channel->sync_buffer = NULL;
	channel->start_index = 0;
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
	__insert_sender_to_chan(sending, c->receiver);
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
	return new_channel;
}


/**
 * @brief Deallocates the channel only if no threads still have references to the channel
 * @param c The channel to deallocate
 */
void lwt_chan_deref(lwt_chan_t c){
	if(c->receiver == lwt_current()){
		//printf("Removing receiver (%d) from channel: %d\n", c->receiver->id, (int)c);
		LIST_REMOVE(c, receiver_channels);
		c->receiver = NULL;
	}
	else if(c->snd_cnt > 0){
		__remove_sender_from_chan(c, lwt_current());
		c->snd_cnt--;
		//printf("Removing sender (%d) from channel: %d\n", lwt_current()->id, (int)c);
	}
	//printf("Current sender count: %d\n", c->snd_cnt);
	if(!c->receiver && c->snd_cnt == 0){
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
	__insert_sender_to_chan(c, new_thread);
	return new_thread;
}

