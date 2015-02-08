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

#define LWT_NULL NULL

typedef enum
{
	LWT_INFO_NTHD_RUNNABLE,
	LWT_INFO_NTHD_BLOCKED,
	LWT_INFO_NTH_ZOMBIES
} lwt_info_t;

typedef void *(*lwt_fnt_t)(void *); //function pointer definition

typedef struct
{
	void * sp; //stack pointer
	unsigned long * thread_stack; //thread_stack
	unsigned long * max_addr_thread_stack;
	unsigned long * min_addr_thread_stack;
	unsigned long * top_addr_thread_stack;


	struct lwt * parent; //parent thread
	list_t * children; //children
	lwt_fnt_t start_routine; //start routine
	void * args; //args to store for the routine
	lwt_info_t info; //current status
	int id; //thread id
} lwt, *lwt_t;

lwt_t lwt_create(lwt_fnt_t fn, void * data);
void *lwt_join(lwt_t);
void lwt_die(void *);
int lwt_yield(lwt_t);
lwt_t lwt_current();
int lwt_id(lwt_t);
int lwt_info(lwt_info_t t);


#endif /* LWT_H_ */
