/*                 T E S T _ D A T E T I M E . C
 * BRL-CAD
 *
 * Copyright (c) 2014-2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following
 * disclaimer in the documentation and/or other materials provided
 * with the distribution.
 *
 * 3. The name of the author may not be used to endorse or promote
 * products derived from this software without specific prior written
 * permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "bu.h"


enum {
    CPU_TIMER_ITERATIONS = 1000000,
    CPU_TIMER_SAMPLE_INTERVAL = 1024,
    CPU_TIMER_PARALLEL_MAX_ITERATIONS = 100000000,
    CPU_TIMER_PARALLEL_THREADS = 2
};

static const double CPU_TIMER_BUSY_SCALE = 0.000001;
static const int64_t CPU_TIMER_PARALLEL_TARGET_NSEC = 100000000;
static const int64_t CPU_TIMER_PARALLEL_TOLERANCE_NSEC = 50000000;
static const int64_t CPU_TIMER_BARRIER_SLEEP_USEC = 1000;
static const int64_t CPU_TIMER_BARRIER_TIMEOUT_USEC = 5000000;

struct cpu_timer_parallel_data {
    size_t ncpu;
    size_t ready;
    size_t slots_used;
    int invalid_slot;
    int timer_unavailable;
    int timer_backwards;
    int work_failed;
    int barrier_timeout;
    int64_t thread_delta[CPU_TIMER_PARALLEL_THREADS];
};


static int
test_cpu_timer(int64_t (*timer)(void), const char *timer_name)
{
    int64_t time0 = timer();
    int64_t time1 = time0;
    int i = 0;
    volatile double busy = 0.0;

    if (time0 < 0)
	return 0;

    for (i = 0; i < CPU_TIMER_ITERATIONS; i++) {
	busy += (double)i * CPU_TIMER_BUSY_SCALE;
	if ((i % CPU_TIMER_SAMPLE_INTERVAL) == 0) {
	    int64_t now = timer();

	    if (now < 0)
		bu_exit(1, "ERROR: %s became unavailable!\n", timer_name);
	    if (now < time1)
		bu_exit(1, "ERROR: %s went backwards!\n", timer_name);
	    time1 = now;
	}
    }

    if (busy < 0.0)
	return 1;
    if (time1 < time0)
	bu_exit(1, "ERROR: %s went backwards!\n", timer_name);

    return 0;
}


static void
cpu_timer_parallel_worker(int UNUSED(cpu), void *ptr)
{
    struct cpu_timer_parallel_data *data = (struct cpu_timer_parallel_data *)ptr;
    size_t slot = 0;
    int ready = 0;
    int valid_slot = 0;
    int64_t wait_start = bu_gettime();
    int64_t start = 0;
    int64_t end = 0;
    int i = 0;
    volatile double busy = 0.0;

    bu_semaphore_acquire(BU_SEM_GENERAL);
    if (data->slots_used < CPU_TIMER_PARALLEL_THREADS) {
	slot = data->slots_used;
	data->slots_used++;
	valid_slot = 1;
    } else {
	data->invalid_slot = 1;
    }
    data->ready++;
    bu_semaphore_release(BU_SEM_GENERAL);

    while (!ready) {
	bu_semaphore_acquire(BU_SEM_GENERAL);
	ready = (data->ready >= data->ncpu);
	bu_semaphore_release(BU_SEM_GENERAL);
	if (!ready) {
	    int64_t now = bu_gettime();

	    if (wait_start >= 0 && now >= 0 && now - wait_start > CPU_TIMER_BARRIER_TIMEOUT_USEC) {
		bu_semaphore_acquire(BU_SEM_GENERAL);
		data->barrier_timeout = 1;
		bu_semaphore_release(BU_SEM_GENERAL);
		return;
	    }
	    (void)bu_snooze(CPU_TIMER_BARRIER_SLEEP_USEC);
	}
    }

    start = bu_timer_cpu_thread();
    if (start < 0) {
	bu_semaphore_acquire(BU_SEM_GENERAL);
	data->timer_unavailable = 1;
	bu_semaphore_release(BU_SEM_GENERAL);
	return;
    }

    end = start;
    for (i = 0; i < CPU_TIMER_PARALLEL_MAX_ITERATIONS && end - start < CPU_TIMER_PARALLEL_TARGET_NSEC; i++) {
	busy += (double)i * CPU_TIMER_BUSY_SCALE;
	if ((i % CPU_TIMER_SAMPLE_INTERVAL) == 0) {
	    end = bu_timer_cpu_thread();
	    if (end < start) {
		bu_semaphore_acquire(BU_SEM_GENERAL);
		data->timer_backwards = 1;
		bu_semaphore_release(BU_SEM_GENERAL);
		return;
	    }
	}
    }

    end = bu_timer_cpu_thread();
    bu_semaphore_acquire(BU_SEM_GENERAL);
    if (end < 0) {
	data->timer_unavailable = 1;
    } else if (end < start) {
	data->timer_backwards = 1;
    } else if (busy < 0.0) {
	data->work_failed = 1;
    } else if (valid_slot) {
	data->thread_delta[slot] = end - start;
    }
    bu_semaphore_release(BU_SEM_GENERAL);
}


static int
test_cpu_timer_scope(void)
{
    struct cpu_timer_parallel_data data = {0, 0, 0, 0, 0, 0, 0, 0, {0, 0}};
    int64_t process_start = bu_timer_cpu();
    int64_t process_end = 0;
    int64_t process_delta = 0;
    int64_t thread_delta = 0;
    int64_t tolerance = CPU_TIMER_PARALLEL_TOLERANCE_NSEC;
    size_t i = 0;

    if (process_start < 0 || bu_timer_cpu_thread() < 0)
	return 0;
    if (bu_avail_cpus() < CPU_TIMER_PARALLEL_THREADS)
	return 0;

    data.ncpu = CPU_TIMER_PARALLEL_THREADS;
    bu_parallel(cpu_timer_parallel_worker, data.ncpu, &data);
    process_end = bu_timer_cpu();

    if (process_end < process_start)
	bu_exit(1, "ERROR: Process CPU time went backwards!\n");
    if (data.invalid_slot)
	bu_exit(1, "ERROR: CPU timer test recorded too many worker slots!\n");
    if (data.timer_backwards)
	bu_exit(1, "ERROR: Thread CPU time went backwards in parallel test!\n");
    if (data.barrier_timeout)
	bu_exit(1, "ERROR: CPU timer parallel workers did not start together!\n");
    if (data.work_failed)
	return 1;
    if (data.timer_unavailable)
	return 0;

    process_delta = process_end - process_start;
    for (i = 0; i < CPU_TIMER_PARALLEL_THREADS; i++)
	thread_delta += data.thread_delta[i];

    if (process_delta <= 0 || thread_delta <= 0)
	bu_exit(1, "ERROR: CPU timer parallel test did not record elapsed CPU time!\n");

    if (process_delta / 3 > tolerance)
	tolerance = process_delta / 3;

    if (process_delta + tolerance < thread_delta)
	bu_exit(1, "ERROR: Thread CPU deltas exceed process CPU delta (%lld > %lld)!\n", (long long)thread_delta, (long long)process_delta);

    if (process_delta * 4 < thread_delta * 3)
	bu_exit(1, "ERROR: Process CPU delta does not include parallel worker time (%lld < %lld)!\n", (long long)process_delta, (long long)thread_delta);

    return 0;
}


int
main(int argc, char *argv[])
{
    struct bu_vls result = BU_VLS_INIT_ZERO;
    int64_t curr_time;
    int function_num;

    // Normally this file is part of bu_test, so only set this if it
    // looks like the program name is still unset.
    if (bu_getprogname()[0] == '\0')
	bu_setprogname(argv[0]);

    if (argc != 2) {
	bu_log("Usage: %s {function_num}\n", argv[0]);
	bu_exit(1, "ERROR: wrong number of parameters");
    }

    sscanf(argv[1], "%d", &function_num);

    switch (function_num) {
	case 0:	{
	    int64_t time0 = 0;
	    int64_t time1 = 0;
	    int64_t time2 = 0;
	    int64_t i = 0;
	    size_t counter = 1;

	    time0 = bu_gettime();
	    bu_snooze(BU_SEC2USEC(1));
	    time1 = bu_gettime();

	    if (time1 - time0 <= 0)
		bu_exit(1, "ERROR: We went back in time!\n");

	    /* iterate for exactly 1 second */
	    while (i < 1.0e6 && counter < 1.0e15) {
		counter++;
		time2 = bu_gettime();
		i = time2 - time1;
	    }

	    if (time2 - time1 <= 0)
		bu_exit(1, "ERROR: We looped back in time!\n");

#if 0
	    bu_log("Called bu_gettime() %zu times\n", counter);
	    bu_log("Time1: %llu\n", (unsigned long)time1);
	    bu_log("Time2: %llu\n", (unsigned long)time2);
	    bu_log("Time delta (Time2-Time1): %d\n", i);
#endif

	    return 0;
	}
	case 1:
	    curr_time = 1087449261LL;
	    bu_utctime(&result, curr_time);
	    if (!BU_STR_EQUAL(result.vls_str, "2004-06-17T05:14:21Z"))
		return 1;
	    break;
	case 2:
	    curr_time = 631152000LL;
	    bu_utctime(&result, curr_time);
	    if (!BU_STR_EQUAL(result.vls_str, "1990-01-01T00:00:00Z"))
		return 1;
	    break;
	case 3:
	    curr_time = 936860949LL;
	    bu_utctime(&result, curr_time);
	    if (!BU_STR_EQUAL(result.vls_str, "1999-09-09T07:09:09Z"))
		return 1;
	    break;
	case 4:
	    curr_time = 1388696601LL;
	    bu_utctime(&result, curr_time);
	    if (!BU_STR_EQUAL(result.vls_str, "2014-01-02T21:03:21Z"))
		return 1;
	    break;
	case 5:
	    curr_time = 0LL;
	    bu_utctime(&result, curr_time);
	    if (!BU_STR_EQUAL(result.vls_str, "1970-01-01T00:00:00Z"))
		return 1;
	    break;
	case 6:
	    curr_time = 1LL;
	    bu_utctime(&result, curr_time);
	    if (!BU_STR_EQUAL(result.vls_str, "1970-01-01T00:00:01Z"))
		return 1;
	    break;
	case 7:
	    curr_time = 1431482805LL;
	    bu_utctime(&result, curr_time);
	    if (!BU_STR_EQUAL(result.vls_str, "2015-05-13T02:06:45Z"))
		return 1;
	    break;
	case 8:
	    curr_time = 2147483647LL;
	    bu_utctime(&result, curr_time);
	    if (!BU_STR_EQUAL(result.vls_str, "2038-01-19T03:14:07Z"))
		return 1;
	    break;
	case 9:
	    curr_time = 2147483649LL;
	    bu_utctime(&result, curr_time);
	    if (!BU_STR_EQUAL(result.vls_str, "2038-01-19T03:14:09Z"))
		return 1;
	    break;
	case 10:
	    curr_time = 3147483649LL;
	    bu_utctime(&result, curr_time);
	    if (!BU_STR_EQUAL(result.vls_str, "2069-09-27T05:00:49Z"))
		return 1;
	    break;
	case 11:
	    return test_cpu_timer(bu_timer_cpu, "Process CPU time");
	case 12:
	    return test_cpu_timer(bu_timer_cpu_thread, "Thread CPU time");
	case 13:
	    return test_cpu_timer_scope();
#if 0
	case 11:
	    {
		/* Per POSIX and Microsoft's docs, time should return the time
		 * as seconds elapsed since the POSIX Epoch (midnight, January
		 * 1, 1970).  Since bu_utctime is assuming a time offset from
		 * the epoch, check that bu_gettime and time are more or less
		 * on the same page. */
		struct bu_vls result1 = BU_VLS_INIT_ZERO;
		struct bu_vls result2 = BU_VLS_INIT_ZERO;
		time_t t = time(NULL);
		int64_t t_since_epoc_systime = (int64_t)t * 1.0e6;
		int64_t t_since_epoc_gettime = bu_gettime();
		if (llabs((long long)(t_since_epoc_gettime - t_since_epoc_systime)) > 1.0e6) {
		    bu_exit(1, "ERROR: bu_gettime(%lld) and time(%lld) disagree by > 1.0e6", (long long int)t_since_epoc_gettime, (long long int)t_since_epoc_systime);
		}
		/* If we got this far, bu_utctime should give us the same
		 * result - probably redundant to do so given the numerical
		 * comparison above, but go ahead and make sure the strings
		 * check out as equal. */
		bu_utctime(&result1, t_since_epoc_gettime/1.0e6);
		bu_utctime(&result2, t_since_epoc_systime/1.0e6);
		if (!BU_STR_EQUAL(bu_vls_cstr(&result1), bu_vls_cstr(&result2))) {
		    bu_exit(1, "ERROR: bu_gettime(%s) and time(%s) bu_utctime strings differ", bu_vls_cstr(&result1), bu_vls_cstr(&result2));
		}
		bu_vls_free(&result1);
		bu_vls_free(&result2);
		return 0;
	    }
#endif
    }
    return 0;
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8 cino=N-s
 */
