/*                      P A R A L L E L . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2022 United States Government as represented by
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

#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include <string.h>
#include <signal.h>

#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif

#ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
#endif

#ifdef linux
#  include <sys/stat.h>
#endif

#if defined(__FreeBSD__) || defined(__OpenBSD__)
#  include <sys/param.h>
#  include <sys/sysctl.h>
#  include <sys/stat.h>
#endif

#ifdef __APPLE__
#  include <sys/stat.h>
#  include <sys/param.h>
#  include <sys/sysctl.h>
#endif

#ifdef __sp3__
#  include <sys/sysconfig.h>
#  include <sys/var.h>
#endif

#ifdef HAVE_ULOCKS_H
#  include <ulocks.h>
#endif
#ifdef HAVE_SYS_SYSMP_H
#  include <sys/sysmp.h> /* for sysmp() */
#endif

#ifdef HAVE_SCHED_H
#  include <sched.h>
#else
#  ifdef HAVE_SYS_SCHED_H
#    include <sys/sched.h>
#  endif
#endif

#include "bresource.h"

/*
 * multithreading support for SunOS 5.X / Solaris 2.x
 */
#if defined(SUNOS) && SUNOS >= 52
#  include <sys/unistd.h>
#  include <thread.h>
#  include <synch.h>
#  define rt_thread_t thread_t
#endif /* SUNOS */

/*
 * multithread support built on POSIX Threads (pthread) library.
 */
#ifdef HAVE_PTHREAD_H
#  include <pthread.h>
#  define rt_thread_t pthread_t
#endif

#ifdef _WIN32
#  define rt_thread_t HANDLE
#endif

#include "bio.h"

#include "bu/debug.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/parallel.h"
#include "bu/snooze.h"
#include "bu/str.h"

#include "./parallel.h"

/* #define CPP11THREAD */

#if defined(CPP11THREAD)
void parallel_cpp11thread(void (*func)(int, void *), size_t ncpu, void *arg);
#endif /* CPP11THREAD */

int BU_SEM_THREAD;


typedef enum {
    PARALLEL_GET = 0,
    PARALLEL_PUT = 1
} parallel_action_t;


struct parallel_info {
    int id; /* cpu+1 */
    int parent;
    size_t lim;
    size_t started;
    size_t finished;
};


struct thread_data {
    void (*user_func)(int, void *);
    void *user_arg;
    int cpu_id;
    int affinity;
    struct parallel_info *parent;
};


int
bu_parallel_id(void)
{
    return thread_get_cpu();
}


int
bu_is_parallel(void)
{
    /* this routine is deprecated, do not use. */
    return 0;
}


void
bu_nice_set(int newnice)
{
#ifdef HAVE_SETPRIORITY
    int opri, npri;

#  ifndef PRIO_PROCESS     /* necessary for linux */
#    define PRIO_PROCESS 0 /* From /usr/include/sys/resource.h */
#  endif
    opri = getpriority(PRIO_PROCESS, 0);
    setpriority(PRIO_PROCESS, 0, newnice);
    npri = getpriority(PRIO_PROCESS, 0);

    if (UNLIKELY(bu_debug)) {
	bu_log("bu_nice_set() Priority changed from %d to %d\n", opri, npri);
    }

#else /* !HAVE_SETPRIORITY */
    /* no known means to change the nice value */
    if (UNLIKELY(bu_debug)) {
	bu_log("bu_nice_set(%d) Priority NOT changed\n", newnice);
    }
#endif  /* _WIN32 */
}


size_t
bu_avail_cpus(void)
{
    int ncpu = -1;

#ifdef PARALLEL

#  if defined(__sp3__)
    if (ncpu < 0) {
	int status;
	int cmd;
	int parmlen;
	struct var p;

	cmd = SYS_GETPARMS;
	parmlen = sizeof(struct var);
	if (sysconfig(cmd, &p, parmlen) != 0) {
	    bu_bomb("bu_parallel(): sysconfig error for sp3");
	}
	ncpu = p.v_ncpus;
    }
#  endif	/* __sp3__ */


#  ifdef __FreeBSD__
    if (ncpu < 0) {
	int maxproc;
	size_t len;
	len = 4;
	if (sysctlbyname("hw.ncpu", &maxproc, &len, NULL, 0) == -1) {
	    perror("sysctlbyname");
	} else {
	    ncpu = maxproc;
	}
    }
#  endif


#  if defined(__APPLE__)
    if (ncpu < 0) {
	size_t len;
	int maxproc;
	int mib[] = {CTL_HW, HW_AVAILCPU};

	len = sizeof(maxproc);
	if (sysctl(mib, 2, &maxproc, &len, NULL, 0) == -1) {
	    perror("sysctl");
	} else {
	    ncpu = maxproc; /* should be able to get sysctl to return maxproc */
	}
    }
#  endif /* __ppc__ */


#  if defined(HAVE_GET_NPROCS)
    if (ncpu < 0) {
	ncpu = get_nprocs(); /* GNU extension from sys/sysinfo.h */
    }
#  endif


    /*
     * multithreading support for SunOS 5.X / Solaris 2.x
     */
#  if defined(_SC_NPROCESSORS_ONLN)
    /* SUNOS and linux (and now Mac 10.6+) */
    if (ncpu < 0) {
	ncpu = sysconf(_SC_NPROCESSORS_ONLN);
	if (ncpu < 0) {
	    perror("Unable to get the number of available CPUs");
	}
    }
#endif


#if defined(_SC_NPROC_ONLN)
    if (ncpu < 0) {
	ncpu = sysconf(_SC_NPROC_ONLN);
	if (ncpu < 0) {
	    perror("Unable to get the number of available CPUs");
	}
    }
#endif


#  if defined(linux)
    if (ncpu < 0) {
	/* old linux method */
	/*
	 * Ultra-kludgey way to determine the number of cpus in a
	 * linux box--count the number of processor entries in
	 * /proc/cpuinfo!
	 */

#    define CPUINFO_FILE "/proc/cpuinfo"
	FILE *fp;
	char buf[128];

	fp = fopen (CPUINFO_FILE, "r");

	if (fp == NULL) {
	    perror (CPUINFO_FILE);
	} else {
	    ncpu = 0;
	    while (bu_fgets(buf, 80, fp) != NULL) {
		if (bu_strncmp (buf, "processor", 9) == 0) {
		    ncpu++;
		}
	    }
	    fclose (fp);
	}
    }
#  endif


#  if defined(_WIN32)
    /* Windows */
    if (ncpu < 0) {
	SYSTEM_INFO sysinfo;

	GetSystemInfo(&sysinfo);
	ncpu = (int)sysinfo.dwNumberOfProcessors;
    }
#  endif


#endif /* PARALLEL */

    if (UNLIKELY(bu_debug & BU_DEBUG_PARALLEL)) {
	/* do not use bu_log() here, this can get called before semaphores are initialized */
	fprintf(stderr, "bu_avail_cpus: counted %d cpus.\n", ncpu);
    }

    if (LIKELY(ncpu > 0)) {
	return ncpu;
    }

    /* non-PARALLEL */
    return 1;
}


/**********************************************************************/


#ifdef PARALLEL

#if !defined(HAVE_THREAD_LOCAL)  || !defined(CPP11THREAD)
/* this function provides book-keeping so that we give out unique
 * thread identifiers and for tracking a thread's parent context.
 *
 * it must be able to keep track of a maximum of MAX_PSW*MAX_PSW
 * threads, where each primary thread kicks off at least as many
 * secondary threads.
 */
static struct parallel_info *
parallel_mapping(parallel_action_t action, int id, size_t max)
{
    /* container for keeping track of recursive invocation data, limits, current values */
    static struct parallel_info mapping[MAX_PSW*MAX_PSW] = {{0,0,0,0,0}};
    int got_cpu;

    switch (action) {
	case PARALLEL_GET:
	    if (id < 0) {
		bu_semaphore_acquire(BU_SEM_THREAD);
		for (got_cpu = 1; got_cpu < MAX_PSW*MAX_PSW; got_cpu++) {
		    if (mapping[got_cpu].id == 0) {
			mapping[got_cpu].id = got_cpu;
			break;
		    }
		}
		bu_semaphore_release(BU_SEM_THREAD);

		if (got_cpu >= MAX_PSW*MAX_PSW) {
		    bu_log("Compile-time parallelism limit reached (%d >= %d).\n", got_cpu, MAX_PSW*MAX_PSW);
		    bu_bomb("Unable to track threading.\n");
		}

		mapping[got_cpu].started = mapping[got_cpu].finished = 0;
		mapping[got_cpu].parent = bu_parallel_id();

	    } else {
		got_cpu = id;
		if (mapping[got_cpu].id != got_cpu) {
		    /* presumably id == 0 */
		    mapping[got_cpu].id = got_cpu;
		}
	    }

	    if (mapping[got_cpu].lim == 0 && max > 0)
		mapping[got_cpu].lim = max;

	    return &mapping[got_cpu];

	case PARALLEL_PUT:
	    mapping[id].started = mapping[id].finished = mapping[id].lim = mapping[id].parent = 0;
	    mapping[id].id = 0; /* separate to avoid race */
    }

    return NULL;
}


static void
parallel_wait_for_slot(int throttle, struct parallel_info *parent, size_t max_threads)
{
    size_t threads = max_threads;

    while (1) {
	if (parent->started < parent->finished) {
	    /*bu_log("Warning - parent->started (%d) is less than parent->finished (%d)\n", parent->started, parent->finished);*/
	    return;
	}
	threads = parent->started - parent->finished;

	/*bu_log("threads=%d (start %d - done %d)\n", threads, parent->started, parent->finished);
	  bu_log("max_threads=%d, throttle: %d\n", max_threads, throttle);*/

	if (threads < max_threads || !throttle) {
	    return;
	}
	bu_snooze(BU_SEC2USEC(1));
    }
}


HIDDEN void *
parallel_interface_arg(void *utd)
{
    struct thread_data *user_thread_data = (struct thread_data *)utd;

    /* keep track of our parallel ID number */
    thread_set_cpu(user_thread_data->cpu_id);

    if (user_thread_data->affinity) {
	int ret;
	/* lock us onto a core corresponding to our parallel ID number */
	ret = parallel_set_affinity(user_thread_data->cpu_id);
	if (ret) {
	    bu_log("WARNING: encountered unexpected problem setting CPU affinity\n");
	}
    }

    bu_semaphore_acquire(BU_SEM_THREAD);
    user_thread_data->parent->started++;
    bu_semaphore_release(BU_SEM_THREAD);

    (*(user_thread_data->user_func))(user_thread_data->cpu_id, user_thread_data->user_arg);

    bu_semaphore_acquire(BU_SEM_THREAD);
    user_thread_data->parent->finished++;
    bu_semaphore_release(BU_SEM_THREAD);

    parallel_mapping(PARALLEL_PUT, user_thread_data->cpu_id, 0);

    return NULL;
}


#if defined(_WIN32)
/**
 * A separate stub to call parallel_interface_arg that avoids a
 *  potential crash* on 64-bit windows and calls ExitThread to
 *  cleanly stop the thread.
 *  *See ThreadProc MSDN documentation.
 */
HIDDEN DWORD
parallel_interface_arg_stub(struct thread_data *user_thread_data)
{
    parallel_interface_arg(user_thread_data);
    ExitThread(0);
    return 0; /* Extraneous */
}
#endif

#endif /* !HAVE_THREAD_LOCAL || !CPP11THREAD */
#endif /* PARALLEL */


void
bu_parallel(void (*func)(int, void *), size_t ncpu, void *arg)
{
#ifndef PARALLEL

    if (!func)
	return; /* nothing to do */

    bu_log("bu_parallel(%zu., %p):  Not compiled for PARALLEL machine, running single-threaded\n", ncpu, arg);
    /* do the work anyways */
    (*func)(0, arg);

#elif defined(HAVE_THREAD_LOCAL) && defined(CPP11THREAD)


    if (!func)
	return; /* nothing to do */

    if (ncpu == 1) {
	func(ncpu, arg);
	return;
    } else if (ncpu > MAX_PSW) {
	bu_log("WARNING: bu_parallel() ncpu(%zd) > MAX_PSW(%d), adjusting ncpu\n", ncpu, MAX_PSW);
	ncpu = MAX_PSW;
    }

    parallel_cpp11thread(func, ncpu, arg);

#else

    struct thread_data *thread_context;
    rt_thread_t thread_tbl[MAX_PSW];
    size_t avail_cpus = 1;
    size_t x;
    size_t i;

    /* number of threads created/ended */
    size_t nthreadc;
    size_t nthreade;

    char *libbu_affinity = NULL;

    /* OFF by default as modern schedulers are smarter than this. */
    int affinity = 0;

    /* ncpu == 0 means throttle our thread creation as slots become available */
    int throttle = 0;

    struct parallel_info *parent;

    rt_thread_t thread;

    if (!func)
	return; /* nothing to do */

    if (UNLIKELY(bu_debug & BU_DEBUG_PARALLEL))
	bu_log("bu_parallel(%zu, %p)\n", ncpu, arg);

    if (ncpu > MAX_PSW) {
	bu_log("WARNING: bu_parallel() ncpu(%zd) > MAX_PSW(%d), adjusting ncpu\n", ncpu, MAX_PSW);
	ncpu = MAX_PSW;
    }

    libbu_affinity = getenv("LIBBU_AFFINITY");
    if (libbu_affinity)
	affinity = (int)strtol(libbu_affinity, NULL, 0x10);
    if (UNLIKELY(bu_debug & BU_DEBUG_PARALLEL)) {
	if (affinity)
	    bu_log("CPU affinity enabled. (LIBBU_AFFINITY=%d)\n", affinity);
	else
	    bu_log("CPU affinity disabled.\n");
    }

    /* if we're in debug mode, allow additional cpus */
    if (!(bu_debug & BU_DEBUG_PARALLEL)) {
	/* otherwise, limit ourselves to what is actually available */
	avail_cpus = bu_avail_cpus();
	if (ncpu > avail_cpus) {
	    bu_log("%zd cpus requested, but only %zu available\n", ncpu, avail_cpus);
	    ncpu = avail_cpus;
	}
    }

    parent = parallel_mapping(PARALLEL_GET, bu_parallel_id(), ncpu);

    if (ncpu < 1) {
	/* want to maximize threading potential, but have to throttle
	 * thread creation.  what is our parallelization limit?
	 */
	throttle = 1;

	/* any "zero" limit scopes propagate upward */
	while (parent->lim == 0 && parent->id > 0) {
	    parent = parallel_mapping(PARALLEL_GET, parent->parent, ncpu);
	}

	/* if the top-most parent is unspecified, use all available cpus */
	if (parent->lim == 0) {
	    ncpu = bu_avail_cpus();
	} else {
	    ncpu = parent->lim;
	}

	/* starting a "zero" bu_parallel means we get one worker
	 * thread back (for this thread)
	 */
	bu_semaphore_acquire(BU_SEM_THREAD);
	if (parent->started > 0)
	    parent->started--;
	bu_semaphore_release(BU_SEM_THREAD);
    } else if (ncpu == 1) {
	/* single cpu case bypasses nearly everything, just invoke */
	(*func)(0, arg);

	parallel_mapping(PARALLEL_PUT, bu_parallel_id(), 0); /* correct? we never got. */
	return;
    }

    thread_context = (struct thread_data *)bu_calloc(ncpu, sizeof(*thread_context), "struct thread_data *thread_context");

    /* Fill in the data of thread_context structures of all threads */
    for (x = 0; x < ncpu; x++) {
	struct parallel_info *next = parallel_mapping(PARALLEL_GET, -1, ncpu);

	thread_context[x].user_func = func;
	thread_context[x].user_arg  = arg;
	thread_context[x].cpu_id    = next->id;
	thread_context[x].affinity  = affinity;
	thread_context[x].parent    = parent;
    }

    /*
     * multithreading support for SunOS 5.X / Solaris 2.x
     */
#  if defined(SUNOS) && SUNOS >= 52

    nthreadc = 0;

    /* Give the thread system a hint... */
    {
	static size_t concurrency = 0; /* Max concurrency we have set */
	if (ncpu > concurrency) {
	    if (thr_setconcurrency((int)ncpu)) {
		bu_log("ERROR parallel.c/bu_parallel(): thr_setconcurrency(%zd) failed\n", ncpu);
		/* Not much to do, lump it */
	    } else {
		concurrency = ncpu;
	    }
	}
    }

    /* Create the threads */
    for (x = 0; x < ncpu; x++) {
	parallel_wait_for_slot(throttle, parent, ncpu);

	if (thr_create(0, 0, parallel_interface_arg, &thread_context[x], 0, &thread)) {
	    bu_log("ERROR: bu_parallel: thr_create(0x0, 0x0, 0x%x, 0x0, 0, 0x%x) failed for processor thread # %d\n",
		   parallel_interface_arg, &thread, x);
	    /* Not much to do, lump it */
	} else {
	    if (UNLIKELY(bu_debug & BU_DEBUG_PARALLEL))
		bu_log("bu_parallel(): created thread: (thread: 0x%x) (loop:%d) (nthreadc:%zu)\n",
		       thread, x, nthreadc);

	    thread_tbl[nthreadc] = thread;
	    nthreadc++;
	}
    }

    if (UNLIKELY(bu_debug & BU_DEBUG_PARALLEL))
	for (i = 0; i < nthreadc; i++)
	    bu_log("bu_parallel(): thread_tbl[%d] = 0x%x\n", i, thread_tbl[i]);

    /*
     * Wait for completion of all threads.  We don't wait for threads
     * in order.  We wait for any old thread but we keep track of how
     * many have returned and whether it is one that we started
     */
    nthreade = 0;
    for (x = 0; x < nthreadc; x++) {
	if (UNLIKELY(bu_debug & BU_DEBUG_PARALLEL))
	    bu_log("bu_parallel(): waiting for thread to complete:\t(loop:%d) (nthreadc:%zu) (nthreade:%zu)\n",
		   x, nthreadc, nthreade);

	if (thr_join((rt_thread_t)0, &thread, NULL)) {
	    /* badness happened */
	    perror("thr_join");
	    bu_log("thr_join() failed");
	}

	/* Check to see if this is one the threads we created */
	for (i = 0; i < nthreadc; i++) {
	    if (thread_tbl[i] == thread) {
		thread_tbl[i] = (rt_thread_t)-1;
		nthreade++;
		break;
	    }
	}

	if ((thread_tbl[i] != (rt_thread_t)-1) && i < nthreadc) {
	    bu_log("bu_parallel(): unknown thread %d completed.\n",
		   thread);
	}

	if (UNLIKELY(bu_debug & BU_DEBUG_PARALLEL))
	    bu_log("bu_parallel(): thread completed: (thread: %d)\t(loop:%d) (nthreadc:%zu) (nthreade:%zu)\n",
		   thread, x, nthreadc, nthreade);
    }

    if (UNLIKELY(bu_debug & BU_DEBUG_PARALLEL))
	bu_log("bu_parallel(): %zu threads created.  %zud threads exited.\n", nthreadc, nthreade);
#  endif	/* SUNOS */

#  if defined(HAVE_PTHREAD_H)

    /* Create the posix threads.
     *
     * Start at 1 so we can treat the parent as thread 0.
     */
    nthreadc = 0;
    for (x = 0; x < ncpu; x++) {
	pthread_attr_t attrs;
	pthread_attr_init(&attrs);
	pthread_attr_setstacksize(&attrs, 10*1024*1024);

	parallel_wait_for_slot(throttle, parent, ncpu);

	if (pthread_create(&thread, &attrs, parallel_interface_arg, &thread_context[x])) {
	    bu_log("ERROR: bu_parallel: pthread_create(0x0, 0x0, 0x%lx, 0x0, 0, %p) failed for processor thread # %zu\n",
		   (unsigned long int)parallel_interface_arg, (void *)&thread, x);

	} else {
	    if (UNLIKELY(bu_debug & BU_DEBUG_PARALLEL)) {
		bu_log("bu_parallel(): created thread: (thread: %p) (loop: %zu) (nthreadc: %zu)\n",
		       (void*)thread, x, nthreadc);
	    }
	    thread_tbl[nthreadc] = thread;
	    nthreadc++;
	}

	/* done with the attributes after create */
	pthread_attr_destroy(&attrs);
    }

    if (UNLIKELY(bu_debug & BU_DEBUG_PARALLEL)) {
	for (i = 0; i < nthreadc; i++) {
	    bu_log("bu_parallel(): thread_tbl[%zu] = %p\n", i, (void *)thread_tbl[i]);
	}
    }

    /*
     * Wait for completion of all threads.
     * Wait for them in order.
     */
    nthreade = 0;
    for (x = 0; x < nthreadc; x++) {
	int ret;

	if (UNLIKELY(bu_debug & BU_DEBUG_PARALLEL))
	    bu_log("bu_parallel(): waiting for thread %p to complete:\t(loop:%zu) (nthreadc:%zu) (nthreade:%zu)\n",
		   (void *)thread_tbl[x], x, nthreadc, nthreade);

	if ((ret = pthread_join(thread_tbl[x], NULL)) != 0) {
	    /* badness happened */
	    bu_log("pthread_join(thread_tbl[%zu]=%p) ret=%d\n", x, (void *)thread_tbl[x], ret);
	}

	nthreade++;
	thread = thread_tbl[x];
	thread_tbl[x] = (rt_thread_t)-1;

	if (UNLIKELY(bu_debug & BU_DEBUG_PARALLEL))
	    bu_log("bu_parallel(): thread completed: (thread: %p)\t(loop:%zu) (nthreadc:%zu) (nthreade:%zu)\n",
		   (void *)thread, x, nthreadc, nthreade);

    }

    if (UNLIKELY(bu_debug & BU_DEBUG_PARALLEL))
	bu_log("bu_parallel(): %zu threads created.  %zu threads exited.\n", nthreadc, nthreade);

#  endif /* end if posix threads */


#  ifdef _WIN32
    /* Create the Win32 threads */
    nthreadc = 0;
    for (i = 0; i < ncpu; i++) {
	parallel_wait_for_slot(throttle, parent, ncpu);

	thread = CreateThread(
	    NULL,
	    0,
	    (LPTHREAD_START_ROUTINE)parallel_interface_arg_stub,
	    &thread_context[i],
	    0,
	    NULL);

	thread_tbl[i] = thread;
	nthreadc++;

	/* Ensure that all successfully created threads are in sequential order.*/
	if (thread_tbl[i] == NULL) {
	    bu_log("bu_parallel(): Error in CreateThread, Win32 error code %d.\n", GetLastError());
	    --nthreadc;
	}
    }


    {
	/* Wait for other threads in the array */
	DWORD returnCode;
	returnCode = WaitForMultipleObjects((DWORD)nthreadc, thread_tbl, TRUE, INFINITE);
	if (returnCode == WAIT_FAILED) {
	    bu_log("bu_parallel(): Error in WaitForMultipleObjects, Win32 error code %d.\n", GetLastError());
	}
    }

    nthreade = 0;
    for (x = 0; x < nthreadc; x++) {
	int ret;
	if ((ret = CloseHandle(thread_tbl[x]) == 0)) {
	    /* Thread didn't close properly if return value is zero; don't retry and potentially loop forever.  */
	    bu_log("bu_parallel(): Error closing thread %zu of %zu, Win32 error code %d.\n", x, nthreadc, GetLastError());
	}

	nthreade++;
	thread_tbl[x] = (rt_thread_t)-1;
    }
#  endif /* end if Win32 threads */

    if (UNLIKELY(bu_debug & BU_DEBUG_PARALLEL))
	bu_log("bu_parallel(%zd) complete\n", ncpu);

    bu_free(thread_context, "struct thread_data *thread_context");

#endif /* PARALLEL */

    return;
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
