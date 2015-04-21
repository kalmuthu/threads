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
	LWT_INFO_NRECEIVING
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


#endif /* ENUMS_H_ */
