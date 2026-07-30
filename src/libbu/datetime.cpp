/*                         D A T E T I M E . C
 * BRL-CAD
 *
 * Copyright (c) 2013-2026 United States Government as represented by
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

#include "common.h"

#include <time.h>
#include <string.h>

#ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#ifdef HAVE_SYS_TIMES_H
#  include <sys/times.h>
#endif
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif
#ifdef HAVE_SCHED_H
#  include <sched.h>
#endif
#ifdef HAVE_MACH_THREAD_CPUTIME
#  include <mach/mach.h>
#  include <mach/thread_info.h>
#endif

#include "bresource.h"
#include "bio.h"

#include "bu/log.h"
#include "bu/time.h"
#include "bu/parallel.h"
#include "bu/str.h"
#include "bu/vls.h"

// https://github.com/HowardHinnant/date
// Using until we can rely on C++20 features
#include "./date.h"

/* for strict c90 */
#ifndef HAVE_DECL_GETTIMEOFDAY
extern int gettimeofday(struct timeval *, void *);
#endif

static constexpr int64_t usec_per_sec = 1000000;
static constexpr int64_t nsec_per_sec = 1000000000;
static constexpr int64_t nsec_per_usec = 1000;
#if defined(HAVE_GETPROCESSTIMES) || defined(HAVE_GETTHREADTIMES)
static constexpr int64_t nsec_per_windows_tick = 100;
#endif

extern "C" {
int BU_SEM_DATETIME;
}

#if defined(HAVE_GETPROCESSTIMES) || defined(HAVE_GETTHREADTIMES)
static int64_t
timer_filetime(const FILETIME *kernel_time, const FILETIME *user_time)
{
    ULARGE_INTEGER kernel;
    ULARGE_INTEGER user;

    kernel.LowPart = kernel_time->dwLowDateTime;
    kernel.HighPart = kernel_time->dwHighDateTime;
    user.LowPart = user_time->dwLowDateTime;
    user.HighPart = user_time->dwHighDateTime;

    return (int64_t)((kernel.QuadPart + user.QuadPart) * nsec_per_windows_tick);
}
#endif


#if defined(CLOCK_PROCESS_CPUTIME_ID) || defined(CLOCK_THREAD_CPUTIME_ID)
static int64_t
timer_timespec(const struct timespec *time_val)
{
    return ((int64_t)time_val->tv_sec * nsec_per_sec
	    + (int64_t)time_val->tv_nsec);
}
#endif


#if defined(HAVE_SYS_RESOURCE_H) && (defined(RUSAGE_SELF) || defined(RUSAGE_THREAD))
static int64_t
timer_rusage(const struct rusage *usage)
{
    int64_t usec = (int64_t)usage->ru_utime.tv_sec * usec_per_sec
	+ (int64_t)usage->ru_utime.tv_usec
	+ (int64_t)usage->ru_stime.tv_sec * usec_per_sec
	+ (int64_t)usage->ru_stime.tv_usec;

    return usec * nsec_per_usec;
}
#endif


#if defined(HAVE_MACH_THREAD_CPUTIME)
static int64_t
timer_mach_time_val(const time_value_t *time_val)
{
    return ((int64_t)time_val->seconds * nsec_per_sec
	    + (int64_t)time_val->microseconds * nsec_per_usec);
}

static int64_t
timer_mach_thread(void)
{
    thread_t thread = mach_thread_self();
    thread_basic_info_data_t info;
    mach_msg_type_number_t count = THREAD_BASIC_INFO_COUNT;
    kern_return_t ret = thread_info(thread, THREAD_BASIC_INFO, (thread_info_t)&info, &count);

    (void)mach_port_deallocate(mach_task_self(), thread);

    if (ret != KERN_SUCCESS)
	return -1;

    return timer_mach_time_val(&info.user_time) + timer_mach_time_val(&info.system_time);
}
#endif


#if defined(HAVE_SYS_TIMES_H) && defined(HAVE_SYSCONF)
static int64_t
times_process_cpu_nsec(void)
{
    struct tms usage;
    long ticks_per_sec = sysconf(_SC_CLK_TCK);
    clock_t ticks = 0;

    if (ticks_per_sec <= 0)
	return -1;
    if (times(&usage) == (clock_t)-1)
	return -1;

    ticks = usage.tms_utime + usage.tms_stime;
    return (int64_t)(((long double)ticks * (long double)nsec_per_sec) / (long double)ticks_per_sec);
}
#endif


#if !defined(HAVE_WINDOWS_H)
/* clock() exists on windows but is a low-fidelity wall clock timer */
static int64_t
timer_clock(void)
{
    clock_t cpu_time = clock();

    if (cpu_time == (clock_t)-1)
	return -1;

    return (int64_t)(((long double)cpu_time * (long double)nsec_per_sec) / (long double)CLOCKS_PER_SEC);
}
#endif


void
bu_utctime(struct bu_vls *vls_gmtime, const int64_t time_val)
{
    static const char *nulltime = "0000-00-00T00:00:00Z";

    // NOTE - once we bump to C++20 we can used std::format and remove date.h
    std::string iso;
    bu_semaphore_acquire(BU_SEM_DATETIME);
    try {
	auto sval = std::chrono::seconds(time_val);
	auto tmpt = std::chrono::system_clock::time_point(sval);
	iso = date::format("%FT%TZ", date::floor<std::chrono::seconds>(tmpt));
    } catch (...) {
	bu_log("Exception thrown by date.h\n");
    }
    bu_semaphore_release(BU_SEM_DATETIME);

    if (!iso.length()) {
	/* error: but set something, an invalid "NULL" time. */
	bu_vls_sprintf(vls_gmtime, "%s", nulltime);
	return;
    }

    bu_vls_sprintf(vls_gmtime, "%s", iso.c_str());
}


/* FIXME: Need to document whether this function should
 * be returning wallclock or cpu time.
 */
int64_t
bu_gettime(void)
{
#if defined(HAVE_SYS_TIME_H)

    struct timeval nowTime;

    gettimeofday(&nowTime, NULL);
    return ((int64_t)nowTime.tv_sec * usec_per_sec
	    + (int64_t)nowTime.tv_usec);

#elif defined(HAVE_WINDOWS_H)

    FILETIME ft;
    ULARGE_INTEGER ut;
    static constexpr unsigned long long WINDOWS_UNIX_EPOCH_TICKS = 116444736000000000ULL;
    static constexpr unsigned long long WINDOWS_TICKS_PER_USEC = 10ULL;
    long long nowTime;
    GetSystemTimePreciseAsFileTime(&ft);
    ut.LowPart = ft.dwLowDateTime;
    ut.HighPart = ft.dwHighDateTime;
    nowTime = (ut.QuadPart - WINDOWS_UNIX_EPOCH_TICKS)/WINDOWS_TICKS_PER_USEC;
    return nowTime;

#else

#  warning "bu_gettime() implementation missing for this machine type"
    bu_log("WARNING, no bu_gettime implementation for this machine type.\n");
    return -1;

#endif
}


int64_t
bu_timer_cpu(void)
{

#if defined(HAVE_GETPROCESSTIMES)
    {
	FILETIME create_time, exit_time, kernel_time, user_time;

	if (!GetProcessTimes(GetCurrentProcess(), &create_time, &exit_time, &kernel_time, &user_time))
	    return -1;

	return timer_filetime(&kernel_time, &user_time);
    }

#elif defined(CLOCK_PROCESS_CPUTIME_ID)
    {
	struct timespec process_time;

	if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &process_time) == 0)
	    return timer_timespec(&process_time);
    }

#elif defined(HAVE_SYS_RESOURCE_H) && defined(RUSAGE_SELF)
    {
	struct rusage usage;

	if (getrusage(RUSAGE_SELF, &usage) == 0)
	    return timer_rusage(&usage);
    }

#elif defined(HAVE_SYS_TIMES_H) && defined(HAVE_SYSCONF)
    {
	int64_t times_cpu_time = times_process_cpu_nsec();

	if (times_cpu_time >= 0)
	    return times_cpu_time;
    }

#elif !defined(HAVE_WINDOWS_H)

    return timer_clock();

#else

#  warning "bu_getctime() implementation missing for this machine type"
    bu_log("WARNING, no bu_getctime implementation for this machine type.\n");
    return -1;

#endif
}


int64_t
bu_timer_cpu_thread(void)
{

#if defined(HAVE_GETTHREADTIMES)
    {
	FILETIME create_time, exit_time, kernel_time, user_time;

	if (!GetThreadTimes(GetCurrentThread(), &create_time, &exit_time, &kernel_time, &user_time))
	    return -1;

	return timer_filetime(&kernel_time, &user_time);
    }

#elif defined(CLOCK_THREAD_CPUTIME_ID)
    {
	struct timespec thread_time;

	if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &thread_time) == 0)
	    return timer_timespec(&thread_time);
    }

#elif defined(HAVE_SYS_RESOURCE_H) && defined(RUSAGE_THREAD)
    {
	struct rusage usage;

	if (getrusage(RUSAGE_THREAD, &usage) == 0)
	    return timer_rusage(&usage);
    }

#elif defined(HAVE_MACH_THREAD_CPUTIME)
    {
	int64_t mach_cpu_time = timer_mach_thread();

	if (mach_cpu_time >= 0)
	    return mach_cpu_time;
    }
#endif

#  warning "bu_thread_getctime() implementation missing for this machine type"
    bu_log("WARNING, no bu_thread_getctime implementation for this machine type.\n");
    return -1;

#endif
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
