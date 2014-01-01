/*                  B U _ S E M A P H O R E . C
 * BRL-CAD
 *
 * Copyright (c) 2013-2014 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file bu_semaphore.c
 *
 * Tests libbu semaphore locking.
 *
 */


#include "common.h"
#include "bu.h"


#ifdef HAVE_UNISTD_H
#include <unistd.h>
#include <signal.h>


static void
_exit_alarm_handler(int sig)
{
    if (sig == SIGALRM) exit(0);
}


int
set_exit_alarm(unsigned seconds)
{
    signal(SIGALRM, _exit_alarm_handler);
    alarm(seconds);
    return 1;
}


#else
#include <windows.h>


static void CALLBACK
_exit_alarm_handler(UINT UNUSED(uTimerID), UINT UNUSED(uMsg), DWORD_PTR UNUSED(dwUser),
	DWORD_PTR UNUSED(dw1), DWORD_PTR UNUSED(dw2))
{
    exit(0);
}


int
set_exit_alarm(unsigned seconds)
{
    return !!timeSetEvent(seconds*1000, 100, (LPTIMECALLBACK)_exit_alarm_handler, NULL, TIME_ONESHOT);
}


#endif


const int SEM = BU_SEM_LAST+1;


static int
repeat_test(unsigned long reps)
{
    unsigned long i;

    for (i = 0; i < reps; i++) bu_semaphore_init(SEM+1);
    for (i = 0; i < reps; i++) bu_semaphore_free();

    return 1;
}



static int
single_thread_test()
{
    if (!set_exit_alarm(1)) {
	bu_log("failed to start alarm; skipping single-thread bu_semaphore test");
	return 1;
    }

    bu_semaphore_init(SEM+1);
    bu_semaphore_acquire(SEM);
    bu_semaphore_acquire(SEM);
    bu_semaphore_free();

    bu_log("single-thread bu_semaphore test failed");
    return 0;
}


struct increment_thread_args { int *parallel, *running; unsigned long reps, *counter; };
static void
increment_thread(int ncpu, genptr_t pargs)
{
    struct increment_thread_args *args = (struct increment_thread_args *)pargs;
    unsigned long i;

    (void)ncpu;

    if (*args->running) *args->parallel = 1;
    *args->running = 1;

    for (i = 0; i < args->reps; i++) {
	bu_semaphore_acquire(SEM);
	++*args->counter;
	bu_semaphore_release(SEM);
    }
    *args->running = 0;
}


static int
parallel_test(unsigned long reps)
{
    const int nthreads = bu_avail_cpus();


    struct increment_thread_args args;
    unsigned long counter = 0, expected = reps*nthreads;
    int parallel = 0, running = 0;
    args.parallel = &parallel;
    args.running = &running;
    args.reps = reps;
    args.counter = &counter;

    bu_semaphore_init(SEM+1);
    bu_parallel(increment_thread, nthreads, &args);
    bu_semaphore_free();

    if (counter != expected) {
	bu_log("parallel-increment bu_semaphore test failed: counter is %lu, expected %lu\n", counter, expected);
	return 0;
    }

    if ((nthreads > 1) && !parallel) {
	bu_log("parallel-increment bu_semaphore test invalid: threads did not run in parallel\n");
	return 0;
    }

    return 1;
}


int
main(int argc, char **argv)
{
    const char * const USAGE = "Usage: %s [-n reps]\n";


    unsigned long nreps = 10000;
    int c;
    int success;

    while ((c = bu_getopt(argc, argv, "n:")) != -1) {
	switch (c) {
	    case 'n': nreps = strtoul(bu_optarg, NULL, 0); break;
	    default: bu_exit(1, USAGE, argv[0]);
	}
    }

    success = repeat_test(nreps) && parallel_test(nreps);

    return !(success && single_thread_test());
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
