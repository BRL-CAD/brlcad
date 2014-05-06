/*                      P A R A L L E L . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2014 United States Government as represented by
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

#ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
#endif

#ifdef HAVE_SYS_RESOURCE_H
#  include <sys/resource.h>
#endif

#ifdef linux
#  include <sys/types.h>
#  ifdef HAVE_SYS_WAIT_H
#    include <sys/wait.h>
#  endif
#  include <sys/stat.h>
#  include <sys/sysinfo.h>
#endif

#if defined(__FreeBSD__) || defined(__OpenBSD__)
#  include <sys/types.h>
#  include <sys/param.h>
#  include <sys/sysctl.h>
#  ifdef HAVE_SYS_WAIT_H
#    include <sys/wait.h>
#  endif
#  include <sys/stat.h>
#endif

#ifdef __APPLE__
#  include <sys/types.h>
#  ifdef HAVE_SYS_WAIT_H
#    include <sys/wait.h>
#  endif
#  include <sys/stat.h>
#  include <sys/param.h>
#  include <sys/sysctl.h>
#endif

#ifdef __sp3__
#  include <sys/types.h>
#  include <sys/sysconfig.h>
#  include <sys/var.h>
#endif

#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#ifdef HAVE_ULOCKS_H
#  include <ulocks.h>
#endif
#ifdef HAVE_SYS_SYSMP_H
#  include <sys/sysmp.h> /* for sysmp() */
#endif

#ifdef HAVE_SYS_WAIT_H
#  include <sys/wait.h>
#endif

#ifdef HAVE_SCHED_H
#  include <sched.h>
#else
#  ifdef HAVE_SYS_SCHED_H
#    include <sys/sched.h>
#  endif
#endif

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

#include "bio.h"

#include "bu/debug.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/parallel.h"
#include "bu/str.h"

#include "./parallel.h"


struct thread_data {
    void (*user_func)(int, genptr_t);
    genptr_t user_arg;
    int cpu_id;
    int counted;
    int affinity;
};


/**
 * process id of the initiating thread. used to shutdown bu_parallel
 * threads/procs.
 */
static int pid_of_initiating_thread = 0;

/* # threads started */
static int parallel_nthreads_started = 0;

/* # threads properly finished */
static int parallel_nthreads_finished = 0;

/* User's arg to his threads */
static genptr_t parallel_arg;

/* user function to run in parallel */
static void (*parallel_func)(int, genptr_t);


int
bu_parallel_id(void)
{
    return thread_get_cpu();
}


int
bu_is_parallel(void)
{
    if (pid_of_initiating_thread != 0)
	return 1;
    return 0;
}


void
bu_kill_parallel(void)
{
    if (pid_of_initiating_thread == 0)
	return;

    if (pid_of_initiating_thread == bu_process_id())
	return;

    bu_terminate(pid_of_initiating_thread);

    return;
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

HIDDEN void
parallel_interface_arg(struct thread_data *user_thread_data)
{
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

    if (!user_thread_data->counted) {
	bu_semaphore_acquire(BU_SEM_SYSCALL);
	parallel_nthreads_started++;
	bu_semaphore_release(BU_SEM_SYSCALL);
    }

    (*(user_thread_data->user_func))(user_thread_data->cpu_id, user_thread_data->user_arg);

    if (!user_thread_data->counted) {
	bu_semaphore_acquire(BU_SEM_SYSCALL);
	parallel_nthreads_finished++;
	bu_semaphore_release(BU_SEM_SYSCALL);
    }
}


#endif /* PARALLEL */


void
bu_parallel(void (*func)(int, genptr_t), int ncpu, genptr_t arg)
{
    /* avoid using the 'register' keyword in here "just in case" */

#ifndef PARALLEL

    bu_log("bu_parallel(%d., %p):  Not compiled for PARALLEL machine, running single-threaded\n", ncpu, arg);
    /* do the work anyways */
    (*func)(0, arg);

#else

    struct thread_data *user_thread_data_bu;
    int avail_cpus = 1;
    int x;

    char *libbu_affinity = NULL;

    /* OFF by default until linux issue is debugged */
    int affinity = 0;

    /*
     * multithreading support for SunOS 5.X / Solaris 2.x
     */
#  if defined(SUNOS) && SUNOS >= 52
    static int concurrency = 0; /* Max concurrency we have set */
#  endif
#  if (defined(SUNOS) && SUNOS >= 52) || defined(HAVE_PTHREAD_H)
    int nthreadc;
    int nthreade;
    rt_thread_t thread;
    rt_thread_t thread_tbl[MAX_PSW];
    int i;
#  endif /* SUNOS */
#  ifdef WIN32
    int nthreadc = 0;
    HANDLE hThreadArray[MAX_PSW] = {0};
    int i;
#  endif /* WIN32 */

    if (UNLIKELY(bu_debug & BU_DEBUG_PARALLEL))
	bu_log("bu_parallel(%d, %p)\n", ncpu, arg);

    if (UNLIKELY(pid_of_initiating_thread))
	bu_bomb("bu_parallel() called from within parallel section\n");

    pid_of_initiating_thread = bu_process_id();

    if (ncpu > MAX_PSW) {
	bu_log("WARNING: bu_parallel() ncpu(%d) > MAX_PSW(%d), adjusting ncpu\n", ncpu, MAX_PSW);
	ncpu = MAX_PSW;
    }
    parallel_nthreads_started = 0;
    parallel_nthreads_finished = 0;
    parallel_func = func;
    parallel_arg = arg;

    libbu_affinity = getenv("LIBBU_AFFINITY");
    if (libbu_affinity)
	affinity = (int)strtol(libbu_affinity, NULL, 0x10);
    if (UNLIKELY(bu_debug & BU_DEBUG_PARALLEL)) {
	if (affinity)
	    bu_log("CPU affinity enabled. (LIBBU_AFFINITY=%d)\n", affinity);
	else
	    bu_log("CPU affinity disabled.\n", affinity);
    }

    user_thread_data_bu = (struct thread_data *)bu_calloc(ncpu, sizeof(*user_thread_data_bu), "struct thread_data *user_thread_data_bu");

    /* Fill in the data of user_thread_data_bu structures of all threads */
    for(x = 0; x < ncpu; x++) {
	user_thread_data_bu[x].user_func = func;
	user_thread_data_bu[x].user_arg  = arg;
	user_thread_data_bu[x].cpu_id    = x;
	user_thread_data_bu[x].counted   = 0;
	user_thread_data_bu[x].affinity  = affinity;
    }

    /* if we're in debug mode, allow additional cpus */
    if (!(bu_debug & BU_DEBUG_PARALLEL)) {
	avail_cpus = bu_avail_cpus();
	if (ncpu > avail_cpus) {
	    bu_log("%d cpus requested, but only %d available\n", ncpu, avail_cpus);
	    ncpu = avail_cpus;
	}
    }

    /*
     * multithreading support for SunOS 5.X / Solaris 2.x
     */
#  if defined(SUNOS) && SUNOS >= 52

    thread = 0;
    nthreadc = 0;

    /* Give the thread system a hint... */
    if (ncpu > concurrency) {
	if (thr_setconcurrency(ncpu)) {
	    bu_log("ERROR parallel.c/bu_parallel(): thr_setconcurrency(%d) failed\n",
		   ncpu);
	    /* Not much to do, lump it */
	} else {
	    concurrency = ncpu;
	}
    }

    /* Create the threads */
    for (x = 0; x < ncpu; x++) {

	if (thr_create(0, 0, (void *(*)(void *))parallel_interface_arg, &user_thread_data_bu[x], 0, &thread)) {
	    bu_log("ERROR: bu_parallel: thr_create(0x0, 0x0, 0x%x, 0x0, 0, 0x%x) failed for processor thread # %d\n",
		   parallel_interface_arg, &thread, x);
	    /* Not much to do, lump it */
	} else {
	    if (UNLIKELY(bu_debug & BU_DEBUG_PARALLEL))
		bu_log("bu_parallel(): created thread: (thread: 0x%x) (loop:%d) (nthreadc:%d)\n",
		       thread, x, nthreadc);

	    thread_tbl[nthreadc] = thread;
	    nthreadc++;
	}
    }

    if (UNLIKELY(bu_debug & BU_DEBUG_PARALLEL))
	for (i = 0; i < nthreadc; i++)
	    bu_log("bu_parallel(): thread_tbl[%d] = 0x%x\n",
		   i, thread_tbl[i]);

    /*
     * Wait for completion of all threads.  We don't wait for threads
     * in order.  We wait for any old thread but we keep track of how
     * many have returned and whether it is one that we started
     */
    thread = 0;
    nthreade = 0;
    for (x = 0; x < nthreadc; x++) {
	if (UNLIKELY(bu_debug & BU_DEBUG_PARALLEL))
	    bu_log("bu_parallel(): waiting for thread to complete:\t(loop:%d) (nthreadc:%d) (nthreade:%d)\n",
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
	    bu_log("bu_parallel(): thread completed: (thread: %d)\t(loop:%d) (nthreadc:%d) (nthreade:%d)\n",
		   thread, x, nthreadc, nthreade);
    }

    if (UNLIKELY(bu_debug & BU_DEBUG_PARALLEL))
	bu_log("bu_parallel(): %d threads created.  %d threads exited.\n",
	       nthreadc, nthreade);
#  endif	/* SUNOS */

#  if defined(HAVE_PTHREAD_H)

    thread = 0;
    nthreadc = 0;

    /* Create the posix threads.
     *
     * Start at 1 so we can treat the parent as thread 0.
     */
    for (x = 0; x < ncpu; x++) {
	pthread_attr_t attrs;
	pthread_attr_init(&attrs);
	pthread_attr_setstacksize(&attrs, 10*1024*1024);

	if (pthread_create(&thread, &attrs, (void *(*)(void *))parallel_interface_arg, &user_thread_data_bu[x])) {
	    bu_log("ERROR: bu_parallel: pthread_create(0x0, 0x0, 0x%lx, 0x0, 0, %p) failed for processor thread # %d\n",
		   (unsigned long int)parallel_interface_arg, (void *)&thread, x);
	    /* Not much to do, lump it */
	} else {
	    if (UNLIKELY(bu_debug & BU_DEBUG_PARALLEL)) {
		bu_log("bu_parallel(): created thread: (thread: %p) (loop: %d) (nthreadc: %d)\n",
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
	    bu_log("bu_parallel(): thread_tbl[%d] = %p\n",
		   i, (void *)thread_tbl[i]);
	}
#    ifdef SIGINFO
	/* may be BSD-only (calls _thread_dump_info()) */
	raise(SIGINFO);
#    endif
    }

    /*
     * Wait for completion of all threads.
     * Wait for them in order.
     */
    thread = 0;
    nthreade = 0;
    for (x = 0; x < nthreadc; x++) {
	int ret;

	if (UNLIKELY(bu_debug & BU_DEBUG_PARALLEL))
	    bu_log("bu_parallel(): waiting for thread %p to complete:\t(loop:%d) (nthreadc:%d) (nthreade:%d)\n",
		   (void *)thread_tbl[x], x, nthreadc, nthreade);

	if ((ret = pthread_join(thread_tbl[x], NULL)) != 0) {
	    /* badness happened */
	    bu_log("pthread_join(thread_tbl[%d]=%p) ret=%d\n", x, (void *)thread_tbl[x], ret);
	}

	nthreade++;
	thread_tbl[x] = (rt_thread_t)-1;

	if (UNLIKELY(bu_debug & BU_DEBUG_PARALLEL))
	    bu_log("bu_parallel(): thread completed: (thread: %p)\t(loop:%d) (nthreadc:%d) (nthreade:%d)\n",
		   (void *)thread, x, nthreadc, nthreade);
    }

    if (UNLIKELY(bu_debug & BU_DEBUG_PARALLEL))
	bu_log("bu_parallel(): %d threads created.  %d threads exited.\n",
	       nthreadc, nthreade);

#  endif /* end if posix threads */


#  ifdef WIN32
    /* Create the Win32 threads */

    for (i = 0; i < ncpu; i++) {
	DWORD pdwThreadId;

	hThreadArray[i] = CreateThread(
	    NULL,
	    0,
	    (LPTHREAD_START_ROUTINE)&parallel_interface_arg,
	    &user_thread_data_bu[i],
	    0,
	    &pdwThreadId);

	if (hThreadArray[i] == NULL) {
	    bu_log("bu_parallel(): Error in CreateThread");
	}

	nthreadc++;
    }
    /* Wait for other threads in the array */

    WaitForMultipleObjects(nthreadc, hThreadArray, TRUE, INFINITE);
    for (x = 0; x < nthreadc; x++) {
	int ret;
	if ((ret = CloseHandle(hThreadArray[x]) != 0)) {
	    /* The thread not closing properly */
	    bu_log("bu_parallel(): Error closing threads");
	    x--;
	}
    }
#  endif /* end if Win32 threads */

    /*
     * Ensure that all the threads are REALLY finished.  On some
     * systems, if threads core dump, the rest of the gang keeps
     * going, so this can actually happen (sigh).
     */
    if (UNLIKELY(parallel_nthreads_finished != parallel_nthreads_started)) {
	bu_log("*** ERROR bu_parallel(%d): %d workers did not finish!\n\n",
	       ncpu, ncpu - parallel_nthreads_finished);
    }
    if (UNLIKELY(parallel_nthreads_started != ncpu)) {
	bu_log("bu_parallel() NOTICE:  only %d workers started, expected %d\n",
	       parallel_nthreads_started, ncpu);
    }

    if (UNLIKELY(bu_debug & BU_DEBUG_PARALLEL))
	bu_log("bu_parallel(%d) complete, now serial\n", ncpu);

#  if defined(unix) || defined(__unix)
    /* Cray is known to wander among various pids, perhaps others.
     *
     * At this point, all multi-tasking activity should have ceased,
     * and we should be just a single UNIX process with our original
     * PID and open file table (kernel struct u).  If not, then any
     * output may be written into the wrong file.
     */
    x = bu_process_id();
    if (UNLIKELY(pid_of_initiating_thread != x)) {
	bu_log("WARNING: bu_parallel():  PID of initiating thread changed from %d to %d, open file table may be botched!\n",
	       pid_of_initiating_thread, x);
    }
#  endif
    pid_of_initiating_thread = 0;	/* No threads any more */

    bu_free(user_thread_data_bu, "struct thread_data *user_thread_data_bu");

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
