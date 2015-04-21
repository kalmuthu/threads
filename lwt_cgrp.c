/*
 * lwt_cgrp.c
 *
 *  Created on: Mar 28, 2015
 *      Author: vagrant
 */

#include "lwt_cgrp.h"
#include "lwt.h"
#include "lwt_chan.h"

#include "stdlib.h"
#include "assert.h"
#include "stdio.h"


/**
 * @brief Inserts the channel into the head of the group
 * @param group The channel group to insert into
 * @param channel The channel to add
 */
static void channel_insert_into_group_head(lwt_cgrp_t group, lwt_chan_t channel){
	if(group->channel_head){
		//channel will be new head
		channel->next_channel_in_group = group->channel_head->next_channel_in_group;
		channel->previous_channel_in_group = NULL;
		group->channel_head->previous_channel_in_group = channel;
		group->channel_head = channel;
	}
	else{
		//channel will be head and tail
		channel->previous_channel_in_group = NULL;
		channel->next_channel_in_group = NULL;
		group->channel_head = channel;
		group->channel_tail = channel;
	}
}


/**
 * @brief Adds the channel to the tail of the list
 * @param group The group to append to
 * @param channel The channel to add
 */
static void channel_insert_group_tail(lwt_cgrp_t group, lwt_chan_t channel){
	if(group->channel_tail){
		//channel becomes new tail
		channel->previous_channel_in_group = group->channel_tail;
		channel->next_channel_in_group = NULL;
		group->channel_tail->next_channel_in_group = channel;
		group->channel_tail = channel;
	}
	else{
		//channel becomes head and tail
		channel->previous_channel_in_group = NULL;
		channel->next_channel_in_group = NULL;
		group->channel_head = channel;
		group->channel_tail = channel;
	}
}

/**
 * @brief Inserts the channel to the end of the event queue
 * @param group The channel group to append to
 * @param event The event to add
 */
static void insert_into_event_tail(lwt_cgrp_t group, struct event * event){
	if(group->event_tail){
		event->previous_event = group->event_tail;
		event->next_event = NULL;
		group->event_tail->next_event = event;
		group->event_tail = event;
	}
	else{
		event->previous_event = NULL;
		event->next_event = NULL;
		group->event_head = event;
		group->event_tail = event;
	}
}

/**
 * @brief  Adds the channel to the event queue
 * @param group The group to add the event to
 * @param event The event to add
 */
static void insert_into_event_head(lwt_cgrp_t group, struct event * event){
	if(group->event_head){
		//event will be new head
		event->next_event = group->event_head;
		event->previous_event = NULL;
		group->event_head->next_event = event;
		group->event_head = event;
	}
	else{
		//channel will be new head and tail
		event->next_event = NULL;
		event->previous_event = NULL;
		group->event_head = event;
		group->event_tail = event;
	}
}


/**
 * @brief Removes the channel from the group
 * @param channel The channel to remove
 * @param group The group to remove the channel from
 */
static void remove_channel_from_group(lwt_chan_t channel, lwt_cgrp_t group){
	//detach
	if(channel->previous_channel_in_group){
		channel->previous_channel_in_group->next_channel_in_group = channel->next_channel_in_group;
	}
	if(channel->next_channel_in_group){
		channel->next_channel_in_group->previous_channel_in_group = channel->previous_channel_in_group;
	}
	if(channel == group->channel_head){
		group->channel_head = group->channel_head->next_channel_in_group;
	}
	if(channel == group->channel_tail){
		group->channel_tail = group->channel_tail->previous_channel_in_group;
	}
	channel->channel_group = NULL;
}



/**
 * @brief Removes the channel from the event queue
 * @param event The event to remove
 * @param group The group to alter
 */
static void remove_event_from_group(struct event * event, lwt_cgrp_t group){
	if(event->previous_event){
		event->previous_event->next_event = event->next_event;
	}
	if(event->next_event){
		event->next_event->previous_event = event->previous_event;
	}
	if(event == group->event_head){
		group->event_head = group->event_head->next_event;
	}
	if(event == group->event_tail){
		group->event_tail = group->event_tail->previous_event;
	}
}


/**
 * @brief Initializes the event for when data is added to the channel
 * @param channel The channel with the new data
 * @param sender The sender lwt
 * @param data The data being inserted
 */
void __init_event(lwt_chan_t channel, void * data){
	if(channel->channel_group){
		struct event * event = __create_event(channel, data);
		insert_into_event_tail(channel->channel_group, event);
		if(channel->async_buffer && channel->channel_group->waiting_thread && channel->channel_group->waiting_thread->info != LWT_INFO_NTHD_RUNNABLE){
			channel->channel_group->waiting_thread->info = LWT_INFO_NTHD_RUNNABLE;
			__insert_runnable_tail(channel->channel_group->waiting_thread);
		}
	}
}

/**
 * @brief Constructs an event
 * @param channel The channel being sent on
 * @param sender The sending LWT
 * @param data The data being sent
 */
struct event * __create_event(lwt_chan_t channel, void * data){
	struct event * event = (struct event *)malloc(sizeof(struct event));
	assert(event);
	event->data = data;
	event->channel = channel;
	event->previous_event = NULL;
	event->next_event = NULL;
	return event;
}

/**
 * @brief Removes the event from the group
 * @param event The event being removed
 */
void free_event(struct event * event){
	if(event->channel->channel_group){
		remove_event_from_group(event, event->channel->channel_group);
	}
	free(event);
}

/**
 * @brief Pops the event from the group
 * @param group The channel group to alter
 */
void __pop_event(lwt_cgrp_t group){
	if(group->event_head){
		free_event(group->event_head);
	}
}


/**
 * @brief Creates a group of channels
 * @return The group of channels
 * @note By default, the group is empty
 */
lwt_cgrp_t lwt_cgrp(){
	lwt_cgrp_t group = (lwt_cgrp_t)malloc(sizeof(struct lwt_cgrp));
	if(!group){
		return LWT_NULL;
	}
	group->channel_head = NULL;
	group->channel_tail = NULL;
	group->event_head = NULL;
	group->event_tail = NULL;
	group->waiting_thread = NULL;
	group->creator_thread = lwt_current();
	return group;
}



/**
 * @brief Frees the group if there are no pending events
 * @param group The channel group to free
 * @return 0 if successful; -1 if there are pending events
 */
int lwt_cgrp_free(lwt_cgrp_t group){
	if(group->event_head){
		return -1;
	}
	//remove the group from the channels
	while(group->channel_head){
		remove_channel_from_group(group->channel_head, group);
	}
	free(group);
	return 0;
}

/**
 * @brief Adds the channel to the group if the channel hasn't already been added to a group
 * @param group The group to add the channel to
 * @param channel The channel to add
 * @return 0 if successful; -1 if the channel is already part of a group
 */
int lwt_cgrp_add(lwt_cgrp_t group, lwt_chan_t channel){
	if(channel->channel_group){
		return -1;
	}
	channel->channel_group = group;
	channel_insert_group_tail(group, channel);

	return 0;
}

/**
 * @brief Removes the channel from the group
 * @param group The group to remove the channel from
 * @param channel The channel to remove
 * @return 0 if successful; -1 if the channel isn't part of the group; 1 if the group has a pending event
 */
int lwt_cgrp_rem(lwt_cgrp_t group, lwt_chan_t channel){
	if(channel->channel_group != group){
		//printf("Assigned group is not provided channel\n");
		return -1;
	}
	if(group->event_head){
		//printf("Event queue is not empty\n");
		return 1;
	}
	remove_channel_from_group(channel, group);
	return 0;
}

/**
 * @brief Waits until there is a pending event in the queue
 * @param group The group to wait for
 * @return The event in the queue
 */
lwt_chan_t lwt_cgrp_wait(lwt_cgrp_t group){
	group->waiting_thread = lwt_current();
	__update_lwt_info(group->waiting_thread, LWT_INFO_NRECEIVING);
	//wait until there is an event in the queue
	while(!group->event_head){
		lwt_yield(LWT_NULL);
	}
	__update_lwt_info(lwt_current(), LWT_INFO_NTHD_RUNNABLE);
	group->waiting_thread = NULL;
	lwt_chan_t channel = group->event_head->channel;
	__pop_event(group);
	return channel;
}

/**
 * @brief Marks the channel
 * @param channel The channel to mark
 * @param mark The marker to set
 */
void lwt_chan_mark_set(lwt_chan_t channel, void * mark){
	channel->mark = mark;
}

/**
 * @brief Grabs the mark from the channel
 * @param channel The channel to read
 */
void * lwt_chan_mark_get(lwt_chan_t channel){
	return channel->mark;
}
