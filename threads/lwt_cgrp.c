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
#include "sys/queue.h"



/**
 * @brief Initializes the event for when data is added to the channel
 * @param channel The channel with the new data
 * @param sender The sender lwt
 * @param data The data being inserted
 */
void __init_event(lwt_chan_t channel, void * data){
	if(channel->channel_group){
		struct event * event = __create_event(channel, data);
		TAILQ_INSERT_TAIL(&channel->channel_group->head_event, event, events);
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
	return event;
}

/**
 * @brief Removes the event from the group
 * @param event The event being removed
 */
void free_event(struct event * event){
	if(event->channel->channel_group){

	}
	free(event);
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
	LIST_INIT(&group->head_channels_in_group);
	TAILQ_INIT(&group->head_event);
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
	if(group->head_event.tqh_first){
		return -1;
	}
	//remove the group from the channels
	while(group->head_channels_in_group.lh_first){
		LIST_REMOVE(group->head_channels_in_group.lh_first, channels_in_group);
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
	LIST_INSERT_HEAD(&group->head_channels_in_group, channel, channels_in_group);

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
	if(group->head_event.tqh_first){
		//printf("Event queue is not empty\n");
		return 1;
	}
	LIST_REMOVE(channel, channels_in_group);
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
	while(!group->head_event.tqh_first){
		lwt_yield(LWT_NULL);
	}
	__update_lwt_info(lwt_current(), LWT_INFO_NTHD_RUNNABLE);
	group->waiting_thread = NULL;
	lwt_chan_t channel = group->head_event.tqh_first->channel;
	struct event * head_event = group->head_event.tqh_first;
	TAILQ_REMOVE(&group->head_event, head_event, events);
	free(head_event);
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