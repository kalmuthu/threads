/*
 * enums.h
 *
 *  Created on: Apr 13, 2015
 *      Author: vagrant
 */

#ifndef ENUMS_H_
#define ENUMS_H_

/**
 * @brief The various statuses for a LWT
 */
typedef enum
{
	/**
	 * Thread state is runnable; it can be switched to
	 */
	LWT_INFO_NTHD_RUNNABLE,
	/**
	 * Thread state is blocked; waiting for another thread to complete
	 */
	LWT_INFO_NTHD_BLOCKED,
	/**
	 * Thread state is zombie; thread is dead and needs to be joined
	 */
	LWT_INFO_NTHD_ZOMBIES,
	/**
	 * Number of ready pool threads
	 */
	LWT_INFO_NTHD_READY_POOL,
	/**
	 * Number of channels that are active
	 */
	LWT_INFO_NCHAN,
	/**
	 * Number of threads blocked sending
	 */
	LWT_INFO_NSENDING,
	/**
	 * Number of threads blocked receiving
	 */
	LWT_INFO_NRECEIVING,
	/**
	 * Reaper is ready to consume
	 */
	LWT_INFO_REAPER_READY
} lwt_info_t;



/**
 * flags for determining if the lwt is joinable
 */
typedef enum{
	/**
	 * lwt is joinable
	 */
	LWT_JOIN = 0,
	/**
	 * lwt is not joinable
	 */
	LWT_NOJOIN = 1
}lwt_flags_t;

typedef enum{
	/**
	 * Add a lwt sender to a channel
	 */
	LWT_REMOTE_ADD_SENDER_TO_CHANNEL,
	/**
	 * Remove a lwt sender to a channel
	 */
	LWT_REMOTE_REMOVE_SENDER_FROM_CHANNEL,
	/**
	 * Add a blocked lwt to a channel
	 */
	LWT_REMOTE_ADD_BLOCKED_SENDER_TO_CHANNEL,
	/**
	 * Remove a blocked sender from a channel
	 */
	LWT_REMOTE_REMOVE_BLOCKED_SENDER_FROM_CHANNEL,
	/**
	 * Add a channel to a group
	 */
	LWT_REMOTE_ADD_CHANNEL_TO_GROUP,
	/**
	 * Remove a channel from the group
	 */
	LWT_REMOTE_REMOVE_CHANNEL_FROM_GROUP,
	/**
	 * Add an event to remote group
	 */
	LWT_REMOTE_ADD_EVENT_TO_GROUP,
	/**
	 * Remove an event from a remote group
	 */
	LWT_REMOTE_REMOVE_EVENT_FROM_GROUP,
	/**
	 * Signal
	 */
	LWT_REMOTE_SIGNAL
}lwt_remote_op_t;

#endif /* ENUMS_H_ */
