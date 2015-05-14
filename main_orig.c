#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "lwt.h"

#define rdtscll(val) __asm__ __volatile__("rdtsc" : "=A" (val))

#define ITER 10000

/*
 * My output on an Intel Core i5-2520M CPU @ 2.50GHz:
 *
 * [PERF] 120 <- fork/join
 * [PERF] 13 <- yield
 * [TEST] thread creation/join/scheduling
 */

void *
fn_bounce(void *d)
{
	int i;
	unsigned long long start, end;

	lwt_yield(LWT_NULL);
	lwt_yield(LWT_NULL);
	rdtscll(start);
	for (i = 0 ; i < ITER ; i++) lwt_yield(LWT_NULL);
	rdtscll(end);
	lwt_yield(LWT_NULL);
	lwt_yield(LWT_NULL);

	if (!d) printf("[PERF] %5lld <- yield\n", (end-start)/(ITER*2));

	return NULL;
}

void *
fn_null(void *d)
{ return NULL; }

#define IS_RESET()						\
        assert( lwt_info(LWT_INFO_NTHD_RUNNABLE) == 1 &&	\
		lwt_info(LWT_INFO_NTHD_ZOMBIES) == 0 &&		\
		lwt_info(LWT_INFO_NTHD_BLOCKED) == 0)

void
test_perf(void)
{
	lwt_t chld1, chld2;
	int i;
	unsigned long long start, end;


	/* Performance tests */
	rdtscll(start);
	for (i = 0 ; i < ITER ; i++) {
		chld1 = lwt_create(fn_null, NULL, 0);
		lwt_join(chld1);
	}
	rdtscll(end);
	printf("[PERF] %5lld <- fork/join\n", (end-start)/ITER);
	IS_RESET();

	chld1 = lwt_create(fn_bounce, (void*)1, 0);
	chld2 = lwt_create(fn_bounce, NULL, 0);
	lwt_join(chld1);
	lwt_join(chld2);
	IS_RESET();
}

void *
fn_identity(void *d)
{ return d; }

void *
fn_nested_joins(void *d)
{
	lwt_t chld;

	if (d) {
		lwt_yield(LWT_NULL);
		lwt_yield(LWT_NULL);
		assert(lwt_info(LWT_INFO_NTHD_RUNNABLE) == 1);
		lwt_die(NULL);
	}
	chld = lwt_create(fn_nested_joins, (void*)1, 0);
	lwt_join(chld);
}

volatile int sched[2] = {0, 0};
volatile int curr = 0;

void *
fn_sequence(void *d)
{
	int i, other, val = (int)d;

	for (i = 0 ; i < ITER ; i++) {
		other = curr;
		curr  = (curr + 1) % 2;
		sched[curr] = val;
		assert(sched[other] != val);
		lwt_yield(LWT_NULL);
	}

	return NULL;
}

void *
fn_join(void *d)
{
	lwt_t t = (lwt_t)d;
	void *r;

	r = lwt_join(d);
	assert(r == (void*)0x37337);
}

void
test_crt_join_sched(void)
{
	lwt_t chld1, chld2;

	printf("[TEST] thread creation/join/scheduling\n");

	/* functional tests: scheduling */
	lwt_yield(LWT_NULL);

	chld1 = lwt_create(fn_sequence, (void*)1, 0);
	chld2 = lwt_create(fn_sequence, (void*)2, 0);
	lwt_join(chld2);
	lwt_join(chld1);
	IS_RESET();

	/* functional tests: join */
	chld1 = lwt_create(fn_null, NULL, 0);
	lwt_join(chld1);
	IS_RESET();

	chld1 = lwt_create(fn_null, NULL, 0);
	lwt_yield(LWT_NULL);
	lwt_join(chld1);
	IS_RESET();

	chld1 = lwt_create(fn_nested_joins, NULL, 0);
	lwt_join(chld1);
	IS_RESET();

	/* functional tests: join only from parents */
	chld1 = lwt_create(fn_identity, (void*)0x37337, 0);
	chld2 = lwt_create(fn_join, chld1, 0);
	lwt_yield(LWT_NULL);
	lwt_yield(LWT_NULL);
	lwt_join(chld2);
	//lwt_join(chld1);
	IS_RESET();

	/* functional tests: passing data between threads */
	chld1 = lwt_create(fn_identity, (void*)0x37337, 0);
	assert((void*)0x37337 == lwt_join(chld1));
	IS_RESET();

	/* functional tests: directed yield */
	chld1 = lwt_create(fn_null, NULL, 0);
	lwt_yield(chld1);
	assert(lwt_info(LWT_INFO_NTHD_ZOMBIES) == 1);
	lwt_join(chld1);
	IS_RESET();
}

int
main(void)
{
	test_perf();
	test_crt_join_sched();

	return 0;
}
