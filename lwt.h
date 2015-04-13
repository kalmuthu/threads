/*
 * lwt.h
 *
 *  Created on: Feb 5, 2015
 *      Author: mtrotter
 */
#ifndef LWT_H_
#define LWT_H_

#include "stdlib.h"
#include "enums.h"
#include "lwt_kthd.h"

/**
 * Size of the a page in the OS -> 4K
 */
#define PAGE_SIZE 4096
/**
 * Number of pages to allocate to the stack
 */
#define NUM_PAGES 5
/**
 * Size of the stack
 */
#define STACK_SIZE PAGE_SIZE*NUM_PAGES

/**
 * Null id for yields
 */
#define LWT_NULL NULL

//forward declaration
typedef struct lwt_channel *lwt_chan_t;
typedef struct lwt_kthd * lwt_kthd_t;

typedef void *(*lwt_fnt_t)(void *); //function pointer definition

typedef struct lwt* lwt_t;


/**
 * @brief The Lightweight Thread (LWT) struct
 */
struct lwt
{
	/**
	 * Pointer to the max address of the stack
	 */
	long * max_addr_thread_stack;
	/**
	 * Pointer to the min address of the statck; used for malloc and free
	 */
	long * min_addr_thread_stack;
	/**
	 * The current thread stack pointer for the thread
	 */
	long * thread_sp;
	/**
	 * The flags associated with the lwt
	 */
	lwt_flags_t flags;

	/**
	 * Parent thread
	 */
	lwt_t parent;
	/**
	 * List of children threads
	 */
	lwt_t children;
	/**
	 * Previous sibling
	 */
	lwt_t previous_sibling;
	/**
	 * Next sibling
	 */
	lwt_t next_sibling;

	/**
	 * Previous current thread
	 */
	lwt_t previous_current;
	/**
	 * Next current thread
	 */
	lwt_t next_current;

	/**
	 * Previous runnable thread
	 */
	lwt_t previous_runnable;
	/**
	 * Next runnable thread
	 */
	lwt_t next_runnable;

	/**
	 * Previous ready pool thread
	 */
	lwt_t previous_ready_pool_thread;
	/**
	 * Next ready pool thread
	 */
	lwt_t next_ready_pool_thread;

	/**
	 * Previous sender thread
	 */
	lwt_t previous_sender;
	/**
	 * Next sender thread
	 */
	lwt_t next_sender;
	/**
	 * Previous blocked sender thread
	 */
	lwt_t previous_blocked_sender;
	/**
	 * Next blocked sender thread
	 */
	lwt_t next_blocked_sender;
	/**
	 * List of receiving channels associated with the thread
	 */
	lwt_chan_t receiving_channels;

	/**
	 * The start routine for the thread to run
	 */
	lwt_fnt_t start_routine;
	/**
	 * The args for the start_routine
	 */
	void * args;
	/**
	 * The return value from the routine
	 */
	void * return_value;

	/**
	 * The current status of the thread
	 */
	lwt_info_t info;

	/**
	 * The id of the thread
	 */
	int id;

	/**
	 * Previous thread in kernel thread list
	 */
	lwt_t previous_kthd_thread;
	/**
	 * Next thread in kernel thread list
	 */
	lwt_t next_kthd_thread;
	/**
	 * Pointer to kthd
	 */
	lwt_kthd_t kthd;
};

lwt_t lwt_create(lwt_fnt_t, void *, lwt_flags_t);
void *lwt_join(lwt_t);
void lwt_die(void *);
int lwt_yield(lwt_t);
lwt_t lwt_current();
int lwt_id(lwt_t);
int lwt_info(lwt_info_t);

void __insert_runnable_tail(lwt_t);

#endif /* LWT_H_ */
