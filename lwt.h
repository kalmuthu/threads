/*
 * lwt.h
 *
 *  Created on: Feb 5, 2015
 *      Author: mtrotter
 */

#ifndef LWT_H_
#define LWT_H_

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
	unsigned int lwt_id; //thread id
	lwt_t parent; //parent thread
	lwt_fnt_t start_routine; //start routine
	void * args; //args to store for the routine
	lwt_info_t info; //current status
	//TODO pointer to list
} lwt, *lwt_t;

lwt_t lwt_create(lwt_fnt_t fn, void * data);
void *lwt_join(lwt_t);
void lwt_die(void *);
int lwt_yield(lwt_t);
lwt_t lwt_current();
int lwt_id(lwt_t);
int lwt_info(lwt_info_t t);


#endif /* LWT_H_ */
