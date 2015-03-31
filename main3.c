#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "lwt.h"
#include "lwt_chan.h"
#include "lwt_cgrp.h"

#define rdtscll(val) __asm__ __volatile__("rdtsc" : "=A" (val))

#define ITER 10000

/* 
 * My output on an Intel Core i5-2520M CPU @ 2.50GHz:
 *
 * [PERF] 120 <- fork/join
 * [PERF] 13 <- yield
 * [TEST] thread creation/join/scheduling
 * [PERF] 48 <- snd+rcv (buffer size 0)
 * [TEST] multisend (channel buffer size 0)
 * [PERF] 27 <- asynchronous snd->rcv (buffer size 100)
 * [TEST] multisend (channel buffer size 100)
 * [TEST] group wait (channel buffer size 0, grpsz 3)
 * [TEST] group wait (channel buffer size 3, grpsz 3)
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
	//TODO change back
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

void *
fn_chan(lwt_chan_t to)
{
	lwt_chan_t from;
	int i;
	
	from = lwt_chan(0);
	lwt_snd_chan(to, from);
	assert(from->snd_cnt);
	for (i = 0 ; i < ITER ; i++) {
		lwt_snd(to, (void*)1);
		assert(2 == (int)lwt_rcv(from));
	}
	lwt_chan_deref(from);
	
	return NULL;
}

void
test_perf_channels(int chsz)
{
	lwt_chan_t from, to;
	lwt_t t;
	int i;
	unsigned long long start, end;

	//assert(LWT_RUNNING == lwt_current()->state);
	from = lwt_chan(chsz);
	assert(from);
	t    = lwt_create_chan(fn_chan, from, 0);
	to   = lwt_rcv_chan(from);
	assert(to->snd_cnt);
	rdtscll(start);
	for (i = 0 ; i < ITER ; i++) {
		assert(1 == (int)lwt_rcv(from));
		lwt_snd(to, (void*)2);
	}
	lwt_chan_deref(to);
	rdtscll(end);
	printf("[PERF] %5lld <- snd+rcv (buffer size %d)\n", 
	       (end-start)/(ITER*2), chsz);
	lwt_join(t);
}

static int sndrcv_cnt = 0;

void *
fn_snder(lwt_chan_t c, int v)
{
	int i;

	for (i = 0 ; i < ITER ; i++) {
		lwt_snd(c, (void*)v);
		sndrcv_cnt++;
	}

	return NULL;
}

void *fn_snder_1(lwt_chan_t c) { return fn_snder(c, 1); }
void *fn_snder_2(lwt_chan_t c) { return fn_snder(c, 2); }

void
test_multisend(int chsz)
{
	lwt_chan_t c;
	lwt_t t1, t2;
	int i, ret[ITER*2], sum = 0, maxcnt = 0;

	printf("[TEST] multisend (channel buffer size %d)\n", chsz);

	c  = lwt_chan(chsz);
	assert(c);
	t1 = lwt_create_chan(fn_snder_2, c, 0);
	t2 = lwt_create_chan(fn_snder_1, c, 0);
	for (i = 0 ; i < ITER*2 ; i++) {
		//if (i % 5 == 0) lwt_yield(LWT_NULL);
		ret[i] = (int)lwt_rcv(c);
		if (sndrcv_cnt > maxcnt) maxcnt = sndrcv_cnt;
		sndrcv_cnt--;
	}
	lwt_join(t1);
	lwt_join(t2);
	
	for (i = 0 ; i < ITER*2 ; i++) {
		sum += ret[i];
		assert(ret[i] == 1 || ret[i] == 2);
	}
	assert(sum == (ITER * 1) + (ITER*2));
	/* 
	 * This is important: Asynchronous means that the buffer
	 * should really fill up here as the senders never block until
	 * the buffer is full.  Thus the difference in the number of
	 * sends and the number of receives should vary by the size of
	 * the buffer.  If your implementation doesn't do this, it is
	 * doubtful you are really doing asynchronous communication.
	 */
	assert(maxcnt >= chsz);

	return;
}

static int async_sz = 0;

void *
fn_async_steam(lwt_chan_t to)
{
	int i;
	
	for (i = 0 ; i < ITER ; i++) lwt_snd(to, (void*)(i+1));
	lwt_chan_deref(to);
	
	return NULL;
}

void
test_perf_async_steam(int chsz)
{
	lwt_chan_t from;
	lwt_t t;
	int i;
	unsigned long long start, end;

	async_sz = chsz;
	//assert(LWT_RUNNING == lwt_current()->state);
	from = lwt_chan(chsz);
	assert(from);
	t = lwt_create_chan(fn_async_steam, from, 0);
	assert(lwt_info(LWT_INFO_NTHD_RUNNABLE) == 2);
	rdtscll(start);
	for (i = 0 ; i < ITER ; i++) assert(i+1 == (int)lwt_rcv(from));
	rdtscll(end);
	printf("[PERF] %5lld <- asynchronous snd->rcv (buffer size %d)\n",
	       (end-start)/(ITER*2), chsz);
	lwt_chan_deref(from);
	lwt_join(t);
}

void *
fn_grpwait(lwt_chan_t c)
{
	int i;

	for (i = 0 ; i < ITER ; i++) {
		if ((i % 7) == 0) {
			int j;

			for (j = 0 ; j < (i % 8) ; j++) lwt_yield(LWT_NULL);
		}
		lwt_snd(c, (void*)lwt_id(lwt_current()));
	}
}

#define GRPSZ 3

void
test_grpwait(int chsz, int grpsz)
{
	lwt_chan_t cs[grpsz];
	lwt_t ts[grpsz];
	int i;
	lwt_cgrp_t g;

	printf("[TEST] group wait (channel buffer size %d, grpsz %d)\n", 
	       chsz, grpsz);
	g = lwt_cgrp();
	assert(g);
	
	for (i = 0 ; i < grpsz ; i++) {
		cs[i] = lwt_chan(chsz);
		assert(cs[i]);
		ts[i] = lwt_create_chan(fn_grpwait, cs[i], 0);
		lwt_chan_mark_set(cs[i], (void*)lwt_id(ts[i]));
		lwt_cgrp_add(g, cs[i]);
	}
	assert(lwt_cgrp_free(g) == -1);
	/**
	 * Q: why don't we iterate through all of the data here?
	 * 
	 * A: We need to fix 1) cevt_wait to be level triggered, or 2)
	 * provide a function to detect if there is data available on
	 * a channel.  Either of these would allows us to iterate on a
	 * channel while there is more data pending.
	 */
	for (i = 0 ; i < ((ITER * grpsz)-(grpsz*chsz)) ; i++) {
		lwt_chan_t c;
		int r;

		c = lwt_cgrp_wait(g);
		printf("Group wait complete\n");
		assert(c);
		r = (int)lwt_rcv(c);
		printf("Receive complete: \n");
		int mark = (int)lwt_chan_mark_get(c);
		assert(r == mark);
		printf("Index: %d\n", i);
		//assert(r == (int)lwt_chan_mark_get(c));
	}
	for (i = 0 ; i < grpsz ; i++) {
		lwt_cgrp_rem(g, cs[i]);
		lwt_join(ts[i]);
		lwt_chan_deref(cs[i]);
	}
	assert(!lwt_cgrp_free(g));
	
	return;
}

int
main(void)
{
	test_perf();
	test_perf_channels(0);
	test_perf_async_steam(ITER/10 < 100 ? ITER/10 : 100);
	test_crt_join_sched();
	test_multisend(0);
	test_multisend(ITER/10 < 100 ? ITER/10 : 100);
	test_grpwait(0, 3);
	test_grpwait(3, 3);

	return 0;
}
