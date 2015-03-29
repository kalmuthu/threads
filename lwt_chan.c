/*
 * lwt_chan.c
 *
 *  Created on: Mar 28, 2015
 *      Author: vagrant
 */
#include "lwt_chan.h"
#include "lwt.h"
#include "lwt_cgrp.h"

#include "stdio.h"
#include "stdlib.h"
#include "assert.h"

/**
 * @brief Inserts the new channel before the old channel in the channel list
 * @param old The old channel
 * @param new The new channel
 */
static void insert_before_chan(lwt_chan_t old, lwt_chan_t new){
	//adjust new
	new->next_sibling = old;
	new->previous_sibling = old->previous_sibling;
	//adjust old
	if(old->previous_sibling){
		old->previous_sibling->next_sibling = new;
	}
	old->previous_sibling = new;
}

/**
 * @brief Inserts the new channel after the old channel in the channel list
 * @param old The old thread
 * @param new The new thread
 */
static void insert_after_channel(lwt_chan_t old, lwt_chan_t new){
	//adjust new
	new->next_sibling = old->next_sibling;
	new->previous_sibling = old;
	//adjust old
	if(old->next_sibling){
		old->next_sibling->previous_sibling = new;
	}
	old->next_sibling = new;
}


/**
 * @brief Remove the channel from the list of channels
 * @param channel The channel to be removed
 */
void __remove_channel(lwt_chan_t channel){
	//detach -> update next + previous
	if(channel->next_sibling){
		channel->next_sibling->previous_sibling = channel->previous_sibling;
	}
	if(channel->previous_sibling){
		channel->previous_sibling->next_sibling = channel->next_sibling;
	}
}


/**
 * @brief Inserts the given thread to the head of the blocked sender list
 * @param channel The channel blocking the sender
 * @param thread The thread being blocked
 */
static void insert_blocked_sender_head(lwt_chan_t channel, lwt_t thread){
	if(channel->blocked_senders_head){
		//thread will be new head
		thread->next_blocked_sender = channel->blocked_senders_head;
		thread->previous_blocked_sender = NULL;
		channel->blocked_senders_head->previous_blocked_sender = thread;
		channel->blocked_senders_head = thread;
	}
	else{
		//thread will be head and tail
		thread->previous_blocked_sender = NULL;
		thread->next_blocked_sender = NULL;
		channel->blocked_senders_head = thread;
		channel->blocked_senders_tail = thread;
	}
}

/**
 * @brief Inserts the sender thread into the head of the list
 * @param channel The channel to look at
 * @param thread The thread to insert
 */
static void insert_sender_head(lwt_chan_t channel, lwt_t thread){
	if(channel->senders_head){
		//thread will be new head
		thread->next_sender = channel->senders_head;
		thread->previous_sender = NULL;
		channel->senders_head->previous_sender = thread;
		channel->senders_head = thread;
	}
	else{
		//thread will be head and tail
		thread->next_sender = NULL;
		thread->previous_sender = NULL;
		channel->senders_head = thread;
		channel->senders_tail = thread;
	}
	channel->snd_cnt++;
}


/**
 * @brief Inserts the given thread to the tail of the blocked sender thread list
 * @param channel The channel blocking
 * @param thread The new thread to be insert in the blocked sender thread list
 */
static void insert_blocked_sender_tail(lwt_chan_t channel, lwt_t thread){
	if(channel->blocked_senders_tail){
		//thread will be new tail
		thread->previous_blocked_sender = channel->blocked_senders_tail;
		thread->next_blocked_sender = NULL;
		channel->blocked_senders_tail->next_blocked_sender = thread;
		channel->blocked_senders_tail = thread;
	}
	else{
		//thread will be head and tail
		thread->previous_blocked_sender = NULL;
		thread->next_blocked_sender = NULL;
		channel->blocked_senders_head = thread;
		channel->blocked_senders_tail = thread;
	}
}

/**
 * @brief Removes the senders from the list of sender threads
 * @param channel The channel to append the thread to
 * @param thread The thread being appended
 */
static void insert_sender_tail(lwt_chan_t channel, lwt_t thread){
	if(channel->senders_tail){
		//thread will be new tail
		thread->previous_sender = channel->senders_tail;
		thread->next_sender = NULL;
		channel->senders_tail->next_sender = thread;
		channel->senders_tail = thread;
	}
	else{
		//thread will be new tail and head
		thread->previous_sender = NULL;
		thread->next_sender = NULL;
		channel->senders_head = thread;
		channel->senders_tail = thread;
	}
	channel->snd_cnt++;
}

/**
 * @brief Removes the thread from the list of blocked sender threads
 * @param channel The channel from which the sender will be removed
 * @param thread The thread to be removed
 */
void __remove_from_blocked_sender(lwt_chan_t c, lwt_t thread){
	//detach
	if(thread->next_blocked_sender){
		thread->next_blocked_sender->previous_blocked_sender = thread->previous_blocked_sender;
	}
	if(thread->previous_blocked_sender){
		thread->previous_blocked_sender->next_blocked_sender = thread->next_blocked_sender;
	}
	//update head
	if(thread == c->blocked_senders_head){
		c->blocked_senders_head = c->blocked_senders_head->next_blocked_sender;
	}
	//update tail
	if(thread == c->blocked_senders_tail){
		c->blocked_senders_tail = c->blocked_senders_tail->previous_blocked_sender;
	}
}

/**
 * @brief Removes the thread from the list of senders
 * @param c The channel to be updated
 * @param thread The thread to be removed
 */
static void remove_from_senders(lwt_chan_t c, lwt_t thread){
	//detach
	if(thread->next_sender){
		thread->next_sender->previous_sender = thread->previous_sender;
	}
	if(thread->previous_sender){
		thread->previous_sender->next_sender = thread->next_sender;
	}
	if(thread == c->senders_head){
		c->senders_head = c->senders_head->next_sender;
	}
	if(thread == c->senders_tail){
		c->senders_tail = c->senders_tail->previous_sender;
	}
	c->snd_cnt--;
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
		lwt_current()->info = LWT_INFO_NSENDING;
		//lwt_yield(LWT_NULL);
		if(c->receiver && c->receiver->info == LWT_INFO_NRECEIVING){
			lwt_yield(c->receiver);
		}
		else{
			lwt_yield(LWT_NULL);
		}
	}
	//insert data into buffer
	c->async_buffer[c->end_index] = data;
	__init_event(c, data);
	//update end index
	if(c->end_index < (c->buffer_size - 1)){
		c->end_index++;
	}
	else{
		c->end_index = 0;
	}
	//increment the num of entries
	c->num_entries++;
	//update status
	lwt_current()->info = LWT_INFO_NTHD_RUNNABLE;

	//check if receiver is trying to receive
	/*if(c->receiver && c->receiver->info == LWT_INFO_NRECEIVING){
		lwt_yield(c->receiver);
	}*/
}


/**
 * @brief Pops the data into the buffer
 * @param c The channel to remove the data from
 * @param data The data to remove
 * If the buffer is empty, it will block until there is something to read
 */
void * __pop_data_from_async_buffer(lwt_chan_t c){
	void * data = c->async_buffer[c->start_index];
	while(!data){
		lwt_current()->info = LWT_INFO_NRECEIVING;
		lwt_t next_thread = c->senders_head;
		while(next_thread && next_thread->info != LWT_INFO_NSENDING){
			next_thread = next_thread->next_sender;
		}
		if(next_thread){
			lwt_yield(next_thread);
		}
		else{
			lwt_yield(LWT_NULL);
		}
		data = c->async_buffer[c->start_index];
	}
	//update buffer value
	c->async_buffer[c->start_index] = NULL;
	//update start index
	if(c->start_index < (c->buffer_size - 1)){
		c->start_index++;
	}
	else{
		c->start_index = 0;
	}
	//decrement the number of entries
	c->num_entries--;
	//update status
	lwt_current()->info = LWT_INFO_NTHD_RUNNABLE;
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
	channel->receiver = lwt_current();
	channel->blocked_receiver = NULL;
	channel->next_sibling = NULL;
	channel->previous_sibling = NULL;
	channel->senders_head = NULL;
	channel->senders_tail = NULL;
	channel->snd_cnt = 0;
	channel->blocked_senders_head = NULL;
	channel->blocked_senders_tail = NULL;
	//prepare buffer
	if(sz > 0){
		channel->async_buffer = (void **)malloc(sizeof(void *) * sz);
		assert(channel->async_buffer);
		int index;
		for(index = 0; index < sz; ++index){
			channel->async_buffer[index] = NULL;
		}
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
	channel->previous_channel_in_group = NULL;
	channel->next_channel_in_group = NULL;
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
	}
	else{
		//check if there is a receiver
		if(!c || !c->receiver){
			perror("No receiver for sending channel\n");
			return -1;
		}
		//block
		lwt_current()->info = LWT_INFO_NSENDING;
		insert_blocked_sender_tail(c, lwt_current());
		//check receiver is waiting
		while(!c->blocked_receiver){
			lwt_yield(LWT_NULL);
		}
		//check if we can go ahead and send
		while(c->blocked_senders_head && c->blocked_senders_head != lwt_current()){
			lwt_yield(LWT_NULL);
		}
		while(c->sync_buffer){
			lwt_yield(LWT_NULL);
		}
		c->sync_buffer = data;
		//send data
		__init_event(c, data);


		//wait until receiver picks up buffer
		while(c->sync_buffer){
			//yield to receiver
			c->receiver->info = LWT_INFO_NTHD_RUNNABLE;
			lwt_yield(c->receiver);
		}
	}
	return 0;
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
	insert_sender_head(new_channel, lwt_current());
	return new_channel;
}


/**
 * @brief Deallocates the channel only if no threads still have references to the channel
 * @param c The channel to deallocate
 */
void lwt_chan_deref(lwt_chan_t c){
	if(c->receiver == lwt_current()){
		printf("Removing receiver\n");
		if(c->next_sibling || c->previous_sibling){
			__remove_channel(c);
		}
		else{
			c->receiver->receiving_channels = NULL;
		}
		c->receiver = NULL;
	}
	else{
		remove_from_senders(c, lwt_current());
		printf("Removing sender\n");
	}
	if(!c->receiver && !c->senders_head && !c->senders_tail){
		printf("FREEING CHANNEL: %d!!\n", (int)c);
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
		if(!c || !c->senders_head || !c->senders_tail){
			perror("NO Senders for receiving channel\n");
			return NULL;
		}
		lwt_current()->info = LWT_INFO_NRECEIVING;
		c->blocked_receiver = lwt_current();
		//ensure buffer is empty
		while(c->sync_buffer){
			lwt_yield(LWT_NULL);
		}
		//block until there's a sender
		while(!c->blocked_senders_head){
			lwt_yield(LWT_NULL);
		}
		//detach the head
		lwt_t sender = c->blocked_senders_head;
		while(!c->sync_buffer){
			sender->info = LWT_INFO_NTHD_RUNNABLE;
			lwt_yield(sender);
		}
		__remove_from_blocked_sender(c, sender);

		c->blocked_receiver = NULL;
		void * data = c->sync_buffer;
		c->sync_buffer = NULL;
		return data;
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
	insert_sender_tail(c, new_thread);
	return new_thread;
}

