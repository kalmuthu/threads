/*
 * lwt.h
 *
 *  Created on: Feb 5, 2015
 *      Author: mtrotter
 */
#ifndef LWT_H_
#define LWT_H_

#include "linkedlist.h"
#include <stdlib.h>

#define PAGE_SIZE 4096
#define NUM_PAGES 5
#define STACK_SIZE 4096*5

#define LWT_NULL NULL
#define LWT_YIELD_NO_LWT_TO_YIELD 1

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
	LWT_INFO_NTHD_ZOMBIES
} lwt_info_t;

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
	 * Parent thread
	 */
	lwt_t parent;
	/**
	 * List of children threads
	 */
	list_t * children;

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
};


lwt_t lwt_create(lwt_fnt_t fn, void * data);
void *lwt_join(lwt_t);
void lwt_die(void *);
int lwt_yield(lwt_t);
lwt_t lwt_current();
int lwt_id(lwt_t);
int lwt_info(lwt_info_t t);


#endif /* LWT_H_ */
