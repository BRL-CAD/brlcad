/*                      P A R A L L E L . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2026 United States Government as represented by
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
#if !defined(_WIN32) || defined(__CYGWIN__)
#  include <stdatomic.h>
#endif

#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif

#ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
#endif

#ifdef HAVE_SYS_SYSCALL_H
#  include <sys/syscall.h>
#endif

#ifdef linux
#  include <sys/stat.h>
#endif

#if defined(__FreeBSD__) || defined(__OpenBSD__)
#  include <sys/param.h>
#  include <sys/sysctl.h>
#  include <sys/stat.h>
#endif

#if defined(__FreeBSD__)
#  include <sys/thr.h> // for thr_self
#endif
#if defined(__NetBSD__)
#  include <lwp.h>     // for _lwp_self
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
#include "bu/interrupt.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/parallel.h"
#include "bu/snooze.h"
#include "bu/str.h"

#include "./parallel.h"

/* #define CPP11THREAD */


#if defined(HAVE_SYSCALL) && !defined(HAVE_DECL_SYSCALL) && !defined(syscall)
long syscall(long number, ...);
#endif


#if defined(CPP11THREAD)
void parallel_cpp11thread(void (*func)(int, void *), size_t ncpu, void *arg);
#endif /* CPP11THREAD */

int BU_SEM_THREAD = BU_SEM_ID_THREAD;

#if defined(_WIN32) && !defined(__CYGWIN__)
static LONG bu_cpu_limit = 0;

static size_t
cpu_limit_get(void)
{
    return (size_t)InterlockedCompareExchange(&bu_cpu_limit, 0, 0);
}

static void
cpu_limit_set(size_t ncpu)
{
    (void)InterlockedExchange(&bu_cpu_limit, (LONG)ncpu);
}
#else
static atomic_size_t bu_cpu_limit = ATOMIC_VAR_INIT(0);

static size_t
cpu_limit_get(void)
{
    return atomic_load_explicit(&bu_cpu_limit, memory_order_acquire);
}

static void
cpu_limit_set(size_t ncpu)
{
    atomic_store_explicit(&bu_cpu_limit, ncpu, memory_order_release);
}
#endif


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
#if defined(_WIN32)
    HANDLE thread_handle;
    volatile LONG completed;
#endif
};


int
bu_thread_id(void)
{
#if defined(_WIN32)
    return GetCurrentThreadId();
#elif defined(__FreeBSD__)
    long tid;
    thr_self(&tid);
    return (int)tid;
#elif defined(__NetBSD__)
    return _lwp_self();
#elif defined(__OpenBSD__)
    return getthrid();
#elif defined(HAVE_SYSCALL)
    return syscall(SYS_gettid);
#else
    return -1;
#endif
}


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
	size_t cpu_limit = cpu_limit_get();
	return (cpu_limit && cpu_limit < (size_t)ncpu) ? cpu_limit :
	    (size_t)ncpu;
    }

    /* non-PARALLEL */
    return 1;
}


void
bu_avail_cpus_set(size_t ncpu)
{
    cpu_limit_set((ncpu > MAX_PSW) ? MAX_PSW : ncpu);
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
    /* Container for recursive invocation data, limits, and current values.
     *
     * FIXME: With MAX_PSW at 4096 this array contributes 512 MiB to
     * libbu's BSS.  Loading libbu under Valgrind consequently emits two
     * "set address range perms: large range" warnings while the dynamic
     * loader establishes permissions on libbu's large RW segment.  Both
     * warnings reproduce by preloading libbu into an otherwise empty
     * process; they are not heap leaks.
     *
     * Do not simply reduce this to MAX_PSW.  The squared capacity was added
     * for recursive bu_parallel() calls, whose live parent and child thread
     * records can exceed MAX_PSW.  This should instead become sparse/dynamic
     * bookkeeping as part of a fix that also keeps public parallel IDs safe
     * for callers' MAX_PSW-sized per-thread arrays.
     */
    static struct parallel_info mapping[MAX_PSW*MAX_PSW] = {{0, 0, 0, 0, 0}};
    struct parallel_info *result = NULL;
    int got_cpu = id;

    /*
     * The ID field is both the allocation marker and the publication point
     * for the rest of a mapping entry.  Allocate, initialize, and release an
     * entry while holding the same lock so an allocator cannot reuse a slot
     * while its previous owner is still clearing it.
     */
    bu_semaphore_acquire(BU_SEM_THREAD);

    switch (action) {
	case PARALLEL_GET:
	    if (id < 0) {
		for (got_cpu = 1; got_cpu < MAX_PSW*MAX_PSW; got_cpu++) {
		    if (mapping[got_cpu].id == 0) {
			mapping[got_cpu].id = got_cpu;
			mapping[got_cpu].started = 0;
			mapping[got_cpu].finished = 0;
			mapping[got_cpu].parent = bu_parallel_id();
			break;
		    }
		}
	    } else {
		if (mapping[got_cpu].id != got_cpu) {
		    /* presumably id == 0 */
		    mapping[got_cpu].id = got_cpu;
		}
	    }

	    if (got_cpu >= 0 && got_cpu < MAX_PSW*MAX_PSW) {
		if (mapping[got_cpu].lim == 0 && max > 0)
		    mapping[got_cpu].lim = max;
		result = &mapping[got_cpu];
	    }
	    break;

	case PARALLEL_PUT:
	    mapping[id].started = mapping[id].finished = mapping[id].lim = mapping[id].parent = 0;
	    mapping[id].id = 0;
	    break;
    }

    bu_semaphore_release(BU_SEM_THREAD);

    if (action == PARALLEL_GET && !result) {
	bu_log("Compile-time parallelism limit reached (%d >= %d).\n", got_cpu, MAX_PSW*MAX_PSW);
	bu_bomb("Unable to track threading.\n");
    }

    return result;
}


static void
parallel_wait_for_slot(int throttle, struct parallel_info *parent, size_t max_threads)
{
    size_t started;
    size_t finished;
    size_t threads;

    /* Explicit thread counts are not throttled and need no bookkeeping. */
    if (!throttle)
	return;

    while (1) {
	bu_semaphore_acquire(BU_SEM_THREAD);
	started = parent->started;
	finished = parent->finished;
	bu_semaphore_release(BU_SEM_THREAD);

	if (started < finished) {
	    /*bu_log("Warning - parent->started (%d) is less than parent->finished (%d)\n", parent->started, parent->finished);*/
	    return;
	}
	threads = started - finished;

	/*bu_log("threads=%d (start %d - done %d)\n", threads, parent->started, parent->finished);
	  bu_log("max_threads=%d, throttle: %d\n", max_threads, throttle);*/

	if (threads < max_threads) {
	    return;
	}
	bu_snooze(BU_SEC2USEC(1));
    }
}


static void *
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
 * Separate stub to call parallel_interface_arg that avoids potential
 * crash on 64-bit Windows. Calls ExitThread to cleanly stop thread.
 */
static DWORD
parallel_interface_arg_stub(struct thread_data *user_thread_data)
{
    parallel_interface_arg(user_thread_data);
    InterlockedExchange(&user_thread_data->completed, TRUE);
    ExitThread(0);
    return 0; /* Extraneous */
}
#endif

#endif /* !HAVE_THREAD_LOCAL || !CPP11THREAD */
#endif /* PARALLEL */


void
bu_parallel(void (*func)(int, void *), size_t ncpu, void *arg)
{
    size_t cpu_limit = cpu_limit_get();
    if (ncpu && cpu_limit && ncpu > cpu_limit)
	ncpu = cpu_limit;

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
#ifndef _WIN32
    rt_thread_t thread_tbl[MAX_PSW];
#endif
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
    struct parallel_info root = {0, 0, 0, 0, 0};

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

    /*
     * Threads not created by bu_parallel() all have ID zero.  Give each
     * top-level invocation its own parent context instead of making unrelated
     * callers share mapping[0].  Worker records remain in the mapping so
     * recursive invocations can inherit their parent's limit.
     */
    if (bu_parallel_id() > 0) {
	parent = parallel_mapping(PARALLEL_GET, bu_parallel_id(), ncpu);
    } else {
	root.lim = ncpu;
	parent = &root;
    }

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
	int set_concurrency_error = 0;
	bu_semaphore_acquire(BU_SEM_THREAD);
	if (ncpu > concurrency) {
	    set_concurrency_error = thr_setconcurrency((int)ncpu);
	    if (!set_concurrency_error)
		concurrency = ncpu;
	}
	bu_semaphore_release(BU_SEM_THREAD);
	if (set_concurrency_error)
	    bu_log("ERROR parallel.c/bu_parallel(): thr_setconcurrency(%zd) failed\n", ncpu);
    }

    /* Create the threads */
    for (x = 0; x < ncpu; x++) {
	parallel_wait_for_slot(throttle, parent, ncpu);

	if (thr_create(0, 0, parallel_interface_arg, &thread_context[x], 0, &thread)) {
	    bu_log("ERROR: bu_parallel: thr_create(0x0, 0x0, 0x%x, 0x0, 0, 0x%x) failed for processor thread # %d\n",
		   parallel_interface_arg, &thread, x);
	    parallel_mapping(PARALLEL_PUT, thread_context[x].cpu_id, 0);
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

	if (i >= nthreadc) {
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
	    parallel_mapping(PARALLEL_PUT, thread_context[x].cpu_id, 0);

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
    /* Avoid a busy loop without materially delaying error-path cleanup. */
    const DWORD completion_poll_msec = 1;

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
	thread_context[i].thread_handle = thread;

	if (thread == NULL) {
	    bu_log("bu_parallel(): Error in CreateThread, Win32 error code %d.\n", GetLastError());
	    parallel_mapping(PARALLEL_PUT, thread_context[i].cpu_id, 0);
	} else {
	    nthreadc++;
	}
    }

    /*
     * Wait for each worker individually.  WaitForMultipleObjects() is
     * limited to MAXIMUM_WAIT_OBJECTS handles, which is smaller than the
     * public MAX_PSW limit on Windows.
     */
    for (i = 0; i < ncpu; i++) {
	struct thread_data *worker = &thread_context[i];
	DWORD wait_result;

	if (worker->thread_handle == NULL)
	    continue;

	wait_result = WaitForSingleObject(worker->thread_handle, INFINITE);

	if (wait_result == WAIT_OBJECT_0)
	    continue;

	if (wait_result == WAIT_FAILED) {
	    DWORD error_code = GetLastError();
	    bu_log("bu_parallel(): Error in WaitForSingleObject, Win32 error code %lu.\n", (unsigned long)error_code);
	} else {
	    bu_log("bu_parallel(): Unexpected WaitForSingleObject result %lu.\n", (unsigned long)wait_result);
	}

	/* The native wait did not establish that this worker is finished.  The
	 * completion flag is independent of the thread handle and is published
	 * only after the callback and all libbu worker bookkeeping are done.
	 */
	while (InterlockedCompareExchange(&worker->completed, FALSE, FALSE) == FALSE)
	    Sleep(completion_poll_msec);
    }

    nthreade = 0;
    for (i = 0; i < ncpu; i++) {
	struct thread_data *worker = &thread_context[i];
	int ret;

	if (worker->thread_handle == NULL)
	    continue;

	if ((ret = CloseHandle(worker->thread_handle) == 0)) {
	    /* Thread didn't close properly if return value is zero; don't retry and potentially loop forever.  */
	    bu_log("bu_parallel(): Error closing thread %zu of %zu, Win32 error code %d.\n", nthreade, nthreadc, GetLastError());
	}

	nthreade++;
	worker->thread_handle = NULL;
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
