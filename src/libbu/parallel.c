/*                      P A R A L L E L . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
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

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include <string.h>
#include <signal.h>

#include "bu.h"

#ifdef linux
#  include <sys/time.h>
#  include <sys/types.h>
#  include <sys/resource.h>
#  ifdef HAVE_SYS_WAIT_H
#    include <sys/wait.h>
#  endif
#  include <sys/stat.h>
#  include <sys/sysinfo.h>
#endif

#ifdef __FreeBSD__
#  include <sys/types.h>
#  include <sys/time.h>
#  include <sys/resource.h>
#  include <sys/sysctl.h>
#  ifdef HAVE_SYS_WAIT_H
#    include <sys/wait.h>
#  endif
#  include <sys/stat.h>
#endif

#ifdef __APPLE__
#  include <sys/types.h>
#  include <sys/time.h>
#  include <sys/resource.h>
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

#ifdef CRAY
#  include <sys/category.h>
#  include <sys/resource.h>
#  include <sys/types.h>
#  ifdef CRAY1
#    include <sys/machd.h>	/* For HZ */
#  endif
#endif

#ifdef CRAY2
#  undef MAXINT
#  include <sys/param.h>
#endif

#ifdef HEP
#  include <synch.h>
#  undef stderr
#  define stderr stdout
#endif /* HEP */

#if defined(alliant) && !defined(i860)
/* Alliant FX/8 */
#  include <cncall.h>
#endif

#if (defined(sgi) && defined(mips)) || (defined(__sgi) && defined(__mips))
/* XXX hack that should eventually go away when it can be verified */
#  define SGI_4D      1
#  define _SGI_SOURCE 1 /* IRIX 5.0.1 needs this to def M_BLKSZ */
#  define _BSD_TYPES  1 /* IRIX 5.0.1 botch in sys/prctl.h */
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
#if defined(IRIX64) && IRIX64 >= 64
static struct sched_param bu_param;
#endif

#ifdef ardent
#  include <thread.h>
#endif

#if defined(n16)
#  include <parallel.h>
#  include <sys/sysadmin.h>
#endif

#include "bio.h"

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

#ifdef CRAY
static struct taskcontrol {
    int tsk_len;
    int tsk_id;
    int tsk_value;
} bu_taskcontrol[MAX_PSW];
#endif


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


int
bu_cpulimit_get(void)
{
#ifdef CRAY
    long old;			/* 64-bit clock counts */
    extern long limit();

    old = limit(C_PROC, 0, L_CPU, -1);
    if (UNLIKELY(old < 0)) {
	perror("bu_cpulimit_get(): CPU limit(get)");
    }
    if (old <= 0)
	return INT_MAX;		/* virtually unlimited */
    return ((old + HZ - 1) / HZ);
#else
    return -1;
#endif
}


void
bu_cpulimit_set(int sec)
{
#ifdef CRAY
    long old;		/* seconds */
    long new;		/* seconds */
    long newtick;	/* 64-bit clock counts */
    extern long limit();

    old = bu_cpulimit_get();
    new = old + sec;
    if (new <= 0 || new >= INT_MAX)
	new = INT_MAX;	/* no limit, for practical purposes */
    newtick = new * HZ;
    if (limit(C_PROC, 0, L_CPU, newtick) < 0) {
	perror("bu_cpulimit_set: CPU limit(set)");
    }
    bu_log("Cray CPU limit changed from %d to %d seconds\n",
	   old, newtick/HZ);

    /* Eliminate any memory limit */
    if (limit(C_PROC, 0, L_MEM, 0) < 0) {
	/* Hopefully, not fatal if memory limits are imposed */
	perror("bu_cpulimit_set: MEM limit(set)");
    }
#endif
    if (sec < 0) sec = 0;
}


int
bu_avail_cpus(void)
{
    int ncpu = -1;

#ifdef PARALLEL

#  ifdef SGI_4D
    /* XXX LAB 04 June 2002
     *
     * The call prctl(PR_MAXPPROCS) is supposed to indicate the number
     * of processors this process can use.  Unfortuantely, this
     * returns 0 when running under a CPU set.  A bug report has been
     * filed with SGI.
     *
     * The sysmp(MP_NPROCS) call returns the number of physically
     * configured processors.  This will have to suffice until SGI
     * comes up with a fix.
     */
#    ifdef HAVE_SYSMP
    if (ncpu < 0) {
	ncpu = sysmp(MP_NPROCS);
    }
#    elif defined(HAVE_PRCTL)
    if (ncpu < 0) {
	ncpu = (int)prctl(PR_MAXPPROCS);
    }
#    endif
#  endif /* SGI_4D */


#  ifdef alliant
    if (ncpu < 0) {
	long memsize, ipnum, cenum, detnum, attnum;

#    if !defined(i860)
	/* FX/8 */
	lib_syscfg(&memsize, &ipnum, &cenum, &detnum, &attnum);
#    else
	/* FX/2800 */
	attnum = 28;
#    endif /* i860 */
	ncpu = attnum;		/* # of CEs attached to parallel Complex */
    }
#  endif /* alliant */


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


#  if defined(n16)
    if (ncpu < 0) {
	if ((ncpu = sysadmin(SADMIN_NUMCPUS, 0)) < 0) {
	    perror("sysadmin");
	}
    }
#  endif /* n16 */


#  ifdef __FreeBSD__
    if (ncpu < 0) {
	int maxproc;
	size_t len;
	len = 4;
	if (sysctlbyname("hw.ncpu", &maxproc, &len, NULL, 0) == -1) {
	    ncpu = 1;
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
	    ncpu = 1;
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
	    ncpu = 1;
	}
    }
#endif

#if defined(_SC_NPROC_ONLN)
    if (ncpu < 0) {
	ncpu = sysconf(_SC_NPROC_ONLN);
	if (ncpu < 0) {
	    perror("Unable to get the number of available CPUs");
	    ncpu = 1;
	}
    }
#endif

#if defined(_SC_CRAY_NCPU)
    /* cray */
    if (ncpu < 0) {
	ncpu = sysconf(_SC_CRAY_NCPU);
	if (ncpu < 0) {
	    perror("Unable to get the number of available CPUs");
	    ncpu = 1;
	}
    }
#  endif


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

	ncpu = 0;

	fp = fopen (CPUINFO_FILE, "r");

	if (fp == NULL) {
	    ncpu = 1;
	    perror (CPUINFO_FILE);
	} else {
	    while (bu_fgets(buf, 80, fp) != NULL) {
		if (strncmp (buf, "processor", 9) == 0) {
		    ++ ncpu;
		}
	    }
	    fclose (fp);

	    if (ncpu <= 0) {
		ncpu = 1;
	    }
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


fastf_t
bu_get_load_average(void)
{
    double load = -1.0;

    bu_log("DEPRECATED: bu_get_load_average is deprecated and will be removed in a future release.\n");

    return load;
}


#ifndef _WIN32
#  define PUBLIC_CPUS1 "/var/tmp/public_cpus"
#  define PUBLIC_CPUS2 "/usr/tmp/public_cpus"
#endif
int
bu_get_public_cpus(void)
{
    int avail_cpus = bu_avail_cpus();
#ifndef _WIN32
    int public_cpus = 1;
    FILE *fp;

    if ((fp = fopen(PUBLIC_CPUS1, "rb")) != NULL
	|| (fp = fopen(PUBLIC_CPUS2, "rb")) != NULL)
    {
	int ret;
	ret = fscanf(fp, "%d", &public_cpus);
	if (ret != 1)
	    public_cpus = 1;
	fclose(fp);
	if (public_cpus < 0) public_cpus = avail_cpus + public_cpus;
	if (public_cpus > avail_cpus) public_cpus = avail_cpus;
	return public_cpus;
    }

    bu_file_delete(PUBLIC_CPUS1);
    bu_file_delete(PUBLIC_CPUS2);
    if ((fp = fopen(PUBLIC_CPUS1, "wb")) != NULL ||
	(fp = fopen(PUBLIC_CPUS2, "wb")) != NULL)
    {
	fprintf(fp, "%d\n", avail_cpus);
	bu_fchmod(fp, 0666);
	fclose(fp);
    }
#endif
    return avail_cpus;
}


int
bu_set_realtime(void)
{
#	if defined(IRIX64) && IRIX64 >= 64
    {
	int policy;

	if ((policy = sched_getscheduler(0)) >= 0) {
	    if (policy == SCHED_RR || policy == SCHED_FIFO)
		return 1;
	}

	sched_getparam(0, &bu_param);

	if (sched_setscheduler(0, SCHED_RR /* policy */, &bu_param) >= 0) {
	    return 1; /* realtime */
	}
	/* Fall through to return 0 */
    }
#	endif
    return 0;
}


/**********************************************************************/

/*
 * Cray is known to wander among various pids, perhaps others.
 */
#if defined(unix) || defined(__unix)
#  define CHECK_PIDS 1
#endif


#ifdef PARALLEL

/* bu_worker_tbl_not_empty and bu_kill_workers are only used by the sgi arch */
#  ifdef SGI_4D


HIDDEN int
_bu_worker_tbl_not_empty(int tbl[MAX_PSW])
{
    register int i;
    register int children=0;

    for (i=1; i < MAX_PSW; ++i)
	if (tbl[i]) children++;

    return children;
}


HIDDEN void
_bu_kill_workers(int tbl[MAX_PSW])
{
    register int i;

    for (i=1; i < MAX_PSW; ++i) {
	if (tbl[i]) {
	    if (kill(tbl[i], 9)) {
		perror("_bu_kill_workers(): SIGKILL to child process");
	    }
	    else {
		bu_log("_bu_kill_workers(): child pid %d killed\n", tbl[i]);
	    }
	}
    }

    memset((char *)tbl, 0, sizeof(tbl));
}
#  endif   /* end check if sgi_4d defined */

/* non-published global */
extern int bu_pid_of_initiating_thread;

static int _bu_nthreads_started = 0;	/* # threads started */
static int _bu_nthreads_finished = 0;	/* # threads properly finished */
static genptr_t _bu_parallel_arg;	/* User's arg to his threads */
static void (*_bu_parallel_func)(int, genptr_t);	/* user function to run in parallel */


/**
 * B U _ P A R A L L E L _ I N T E R F A C E
 *
 * Interface layer between bu_parallel and the user's function.
 * Necessary so that we can provide unique thread numbers as a
 * parameter to the user's function, and to decrement the global
 * counter when the user's function returns to us (as opposed to
 * dumping core or longjmp'ing too far).
 *
 * Note that not all architectures can pass an argument
 * (e.g. the pointer to the user's function), so we depend on
 * using a global variable to communicate this.
 * This is no problem, since only one copy of bu_parallel()
 * may be active at any one time.
 */
HIDDEN void
_bu_parallel_interface(void)
{
    register int cpu;		/* our CPU (thread) number */

    bu_semaphore_acquire(BU_SEM_SYSCALL);
    cpu = _bu_nthreads_started++;
    bu_semaphore_release(BU_SEM_SYSCALL);

    (*_bu_parallel_func)(cpu, _bu_parallel_arg);

    bu_semaphore_acquire(BU_SEM_SYSCALL);
    _bu_nthreads_finished++;
    bu_semaphore_release(BU_SEM_SYSCALL);

#  if defined(SGI_4D) || defined(IRIX)
    /*
     * On an SGI, a process/thread created with the "sproc" syscall
     * has all of its file descriptors closed when it "returns" to
     * sproc.  Since this trashes file descriptors which may still be
     * in use by other processes, we avoid ever returning to sproc.
     */
    if (cpu) _exit(0);
#  endif /* SGI */
}
#endif /* PARALLEL */

#ifdef SGI_4D
/**
 * B U _ P R _ F I L E
 *
 * SGI-specific.  Formatted printing of stdio's FILE struct.
 */
HIDDEN void
bu_pr_FILE(char *title, FILE *fp)
{
    bu_log("FILE structure '%s', at x%x:\n", title, fp);
    bu_log(" _cnt = x%x\n", fp->_cnt);
    bu_log(" _ptr = x%x\n", fp->_ptr);
    bu_log(" _base = x%x\n", fp->_base);
    bu_log(" _file = x%x\n", fp->_file);
    bu_printb(" _flag ", fp->_flag & 0xFF,
	      "\010\010_IORW\7_100\6_IOERR\5_IOEOF\4_IOMYBUF\3_004\2_IOWRT\1_IOREAD");
    bu_log("\n");
}
#endif


void
bu_parallel(void (*func)(int, genptr_t), int ncpu, genptr_t arg)
{
#ifndef PARALLEL

    bu_log("bu_parallel(%d., %p):  Not compiled for PARALLEL machine, running single-threaded\n", ncpu, arg);
    /* do the work anyways */
    (*func)(0, arg);

#else
    int avail_cpus = 1;

#  if defined(alliant) && !defined(i860) && !__STDC__
    register int d7;        /* known to be in d7 */
    register int d6 = ncpu; /* known to be in d6 */
#  endif
    int x;

#  if defined(SGI_4D) || defined(CRAY)
    int new;
#  endif

#  ifdef sgi
    long stdin_pos;
    FILE stdin_save;
    int worker_pid_tbl[MAX_PSW] = {0};
#  endif

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

#  ifdef sgi
    memset(worker_pid_tbl, 0, MAX_PSW * sizeof(int));
#  endif

    if (UNLIKELY(bu_debug & BU_DEBUG_PARALLEL))
	bu_log("bu_parallel(%d, %p)\n", ncpu, arg);

    if (UNLIKELY(bu_pid_of_initiating_thread))
	bu_bomb("bu_parallel() called from within parallel section\n");

    bu_pid_of_initiating_thread = bu_process_id();

    if (ncpu > MAX_PSW) {
	bu_log("WARNING: bu_parallel() ncpu(%d) > MAX_PSW(%d), adjusting ncpu\n", ncpu, MAX_PSW);
	ncpu = MAX_PSW;
    }
    _bu_nthreads_started = 0;
    _bu_nthreads_finished = 0;
    _bu_parallel_func = func;
    _bu_parallel_arg = arg;

    /* if we're in debug mode, allow additional cpus */
    if (!(bu_debug & BU_DEBUG_PARALLEL)) {
	avail_cpus = bu_avail_cpus();
	if (ncpu > avail_cpus) {
	    bu_log("%d cpus requested, but only %d available\n", ncpu, avail_cpus);
	    ncpu = avail_cpus;
	}
    }

#  ifdef HEP
    _bu_nthreads_started = 1;
    _bu_nthreads_finished = 1;
    for (x=1; x<ncpu; x++) {
	/* This is more expensive when GEMINUS>1 */
	Dcreate(_bu_parallel_interface);
    }
    (*func)(0, arg);	/* avoid wasting this task */
#  endif /* HEP */

#  ifdef CRAY
    _bu_nthreads_started = 1;
    _bu_nthreads_finished = 1;
    /* Create any extra worker tasks */
    for (x=1; x<ncpu; x++) {
	bu_taskcontrol[x].tsk_len = 3;
	bu_taskcontrol[x].tsk_value = x;
	TSKSTART(&bu_taskcontrol[x], _bu_parallel_interface);
    }
    (*func)(0, arg);	/* avoid wasting this task */

    /* Wait for them to finish */
    for (x=1; x<ncpu; x++) {
	TSKWAIT(&bu_taskcontrol[x]);
    }
    /* There needs to be some way to kill the tfork()'ed processes here */
#  endif

#  if defined(alliant) && !defined(i860)
#	if defined(__STDC__)	/* fxc defines it == 0 !! */
#	undef __STDC__
#	define __STDC__ 2

    /* Calls _bu_parallel_interface in parallel "ncpu" times */
    concurrent_call(CNCALL_COUNT|CNCALL_NO_QUIT, _bu_parallel_interface, ncpu);

#	else
    {
	asm("	movl d6, d0");
	asm("	subql #1, d0");
	asm("	cstart d0");
	asm("super_loop:");
	_bu_parallel_interface();		/* d7 has current index, like magic */
	asm("	crepeat super_loop");
    }
#	endif
#  endif

#  if defined(alliant) && defined(i860)
#pragma loop cncall
    for (x=0; x<ncpu; x++) {
	_bu_parallel_interface();
    }
#  endif

#  if defined(convex) || defined(__convex__)
    /*$dir force_parallel */
    for (x=0; x<ncpu; x++) {
	_bu_parallel_interface();
    }
#  endif /* convex */

#  ifdef ardent
    /* The stack size parameter is pure guesswork */
    parstack(_bu_parallel_interface, 1024*1024, ncpu);
#  endif /* ardent */

#  ifdef SGI_4D
    stdin_pos = ftell(stdin);
    stdin_save = *(stdin);		/* struct copy */
    _bu_nthreads_started = 1;
    _bu_nthreads_finished = 1;

    /* Note:  it may be beneficial to call prctl(PR_SETEXITSIG); */
    /* prctl(PR_TERMCHILD) could help when parent dies.  But SIGHUP??? hmmm */
    for (x = 1; x < ncpu; x++) {
	/*
	 * Start a share-group process, sharing ALL resources.  This
	 * direct sys-call can be used because none of the
	 * task-management services of, eg, taskcreate() are needed.
	 */
#    if defined(IRIX) && IRIX <= 4
	/* Stack size per proc comes from RLIMIT_STACK (typ 64MBytes). */
	new = sproc(_bu_parallel_interface, PR_SALL, 0);
#    else
	/* State maximum stack size.  Be generous, as this mainly
	 * costs address space.  RAM is allocated only to those pages
	 * used.  On the other hand, don't be too generous, because
	 * each proc needs this much space on, e.g. a 64 processor
	 * system.  Don't go quite for an even number of megabytes, in
	 * the hopes of creating a small 32k "buffer zone" to catch
	 * stack overflows.
	 */
	new = sprocsp((void (*)(void *, size_t))_bu_parallel_interface,
		      PR_SALL, 0, NULL,
#      if defined(IRIX64)
		      64*1024*1024 - 32*1024
#      else
		      4*1024*1024 - 32*1024
#      endif
	    );
#    endif
	if (new < 0) {
	    perror("sproc");
	    bu_log("ERROR bu_parallel(): sproc(x%x, x%x)=%d failed on processor %d\n",
		   _bu_parallel_interface, PR_SALL,
		   new, x);
	    bu_log("sbrk(0)=%p\n", sbrk(0));
	    bu_bomb("bu_parallel() failure");
	} else {
	    worker_pid_tbl[x] = new;
	}

    }
    (*func)(0, arg);	/* don't waste this thread */
    {
	int pid;
	int pstat;
	int children;

	/*
	 * Make sure all children are done.
	 */
	while (children=_bu_worker_tbl_not_empty(worker_pid_tbl)) {
	    pstat = 0;
	    if ((pid = wait(&pstat)) < 0) {
		perror("bu_parallel() wait()");
		_bu_kill_workers(worker_pid_tbl);
		bu_bomb("parallelism error");
	    } else if (pid == 0) {
		bu_log("bu_parallel() wait() == 0 with %d children remaining\n", children);
		_bu_kill_workers(worker_pid_tbl);
		bu_bomb("Missing worker");
	    } else {
		if ((pstat & 0xFF) != 0) {
		    bu_log("***ERROR: bu_parallel() worker %d exited with status x%x!\n", pid, pstat);
		    /* XXX How to cope with this;  can't back out work that was lost at this level. */
#    ifdef IRIX
		    if (WIFEXITED(pstat))
			bu_log ("Child terminated normally with status %d 0x%0x\n",
				WEXITSTATUS(pstat));

		    if (WIFSIGNALED(pstat)) {
			bu_log("child terminated on signal %d %0x\n", WTERMSIG(pstat));
			if (pstat & 0200)
			    bu_log("core dumped\n");
			else
			    bu_log("No core dump\n");
		    }
		    if (WIFSTOPPED(pstat))
			bu_log("child is stopped on signal %d 0x%x\n", WSTOPSIG(pstat));

		    if ((pstat & 0177777) == 0177777)
			bu_log("child has continued\n");

#    endif
		    _bu_kill_workers(worker_pid_tbl);
		    bu_bomb("A worker blew out");
		}
		/* remove pid from worker_pid_tbl */
		for (x=1; x < ncpu; x++)
		    if (worker_pid_tbl[x] == pid) {
			worker_pid_tbl[x] = 0;
			break;
		    }

		if (x >= ncpu) {
		    bu_log("WARNING: bu_parallel(): wait() returned non-child process, pid %d\n", pid);
		}
	    }
	}
    }
    if (UNLIKELY(ftell(stdin) != stdin_pos)) {
	/*
	 * Gross SGI bug: when a thread is finished, it returns to the
	 * stack frame created by sproc(), which just calls exit(0),
	 * resulting in all STDIO file buffers being fflush()ed.  This
	 * zaps the stdin position, and may wreak additional havoc.
	 *
	 * Exists in IRIX 3.3.1, Irix 4.0.5,
	 * should be fixed in a later release.  Maybe.
	 */
	bu_log("\nWarning:  stdin file pointer has been corrupted by SGI multi-processor bug!\n");
	if (UNLIKELY(bu_debug & BU_DEBUG_PARALLEL)) {
	    bu_log("Original position was x%x, now position is x%x!\n", stdin_pos, ftell(stdin));
	    bu_pr_FILE("saved stdin", &stdin_save);
	    bu_pr_FILE("current stdin", stdin);
	}
	fseek(stdin, stdin_pos, SEEK_SET);
	if (UNLIKELY(ftell(stdin) != stdin_pos)) {
	    bu_log("WARNING: fseek() did not recover proper position.\n");
	} else {
	    bu_log("It was fixed by fseek()\n");
	}
    }
#  endif /* sgi */

#  if defined(n16)
    /* The shared memory size requirement is sheer guesswork */
    /* The stack size is also guesswork */
    if (task_init(8*1024*1024, ncpu, _bu_parallel_interface, 128*1024, 0) < 0)
	perror("bu_parallel()/task_init()");
#  endif

    /*
     * multithreading support for SunOS 5.X / Solaris 2.x
     */
#  if defined(SUNOS) && SUNOS >= 52

    thread = 0;
    nthreadc = 0;

    /* Give the thread system a hint... */
    if (ncpu > concurrency) {
	if (thr_setconcurrency(ncpu)) {
	    fprintf(stderr, "ERROR parallel.c/bu_parallel(): thr_setconcurrency(%d) failed\n",
		    ncpu);
	    bu_log("ERROR parallel.c/bu_parallel(): thr_setconcurrency(%d) failed\n",
		   ncpu);
	    /* Not much to do, lump it */
	} else {
	    concurrency = ncpu;
	}
    }

    /* Create the threads */
    for (x = 0; x < ncpu; x++) {

	if (thr_create(0, 0, (void *(*)(void *))_bu_parallel_interface, 0, 0, &thread)) {
	    fprintf(stderr, "ERROR: bu_parallel: thr_create(0x0, 0x0, 0x%x, 0x0, 0, 0x%x) failed on processor %d\n",
		    _bu_parallel_interface, &thread, x);
	    bu_log("ERROR: bu_parallel: thr_create(0x0, 0x0, 0x%x, 0x0, 0, 0x%x) failed on processor %d\n",
		   _bu_parallel_interface, &thread, x);
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
	    fprintf(stderr, "thr_join()");
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

#  if defined(HAVE_PTHREAD_H) && !defined(sgi)

    thread = 0;
    nthreadc = 0;

    /* XXX How to advise thread library that we need 'ncpu' processors? */

    /* Create the threads */
    for (x = 0; x < ncpu; x++) {
	pthread_attr_t attrs;
	pthread_attr_init(&attrs);
	pthread_attr_setstacksize(&attrs, 10*1024*1024);

	if (pthread_create(&thread, &attrs,
			   (void *(*)(void *))_bu_parallel_interface, NULL)) {
	    fprintf(stderr, "ERROR: bu_parallel: thr_create(0x0, 0x0, 0x%lx, 0x0, 0, 0x%lx) failed on processor %d\n",
		    (unsigned long int)_bu_parallel_interface, (unsigned long int)&thread, x);
	    bu_log("ERROR: bu_parallel: thr_create(0x0, 0x0, 0x%lx, 0x0, 0, %p) failed on processor %d\n",
		   (unsigned long int)_bu_parallel_interface, (void *)&thread, x);
	    /* Not much to do, lump it */
	} else {
	    if (UNLIKELY(bu_debug & BU_DEBUG_PARALLEL)) {
		bu_log("bu_parallel(): created thread: (thread: %p) (loop:%d) (nthreadc:%d)\n",
		       (void*)thread, x, nthreadc);
	    }

	    thread_tbl[nthreadc] = thread;
	    nthreadc++;
	}
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
	    fprintf(stderr, "pthread_join(thread_tbl[%d]=%p) ret=%d\n", x, (void *)thread_tbl[x], ret);
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

    /*
     * Ensure that all the threads are REALLY finished.  On some
     * systems, if threads core dump, the rest of the gang keeps
     * going, so this can actually happen (sigh).
     */
    if (UNLIKELY(_bu_nthreads_finished != _bu_nthreads_started)) {
	bu_log("*** ERROR bu_parallel(%d): %d workers did not finish!\n\n",
	       ncpu, ncpu - _bu_nthreads_finished);
    }
    if (UNLIKELY(_bu_nthreads_started != ncpu)) {
	bu_log("bu_parallel() NOTICE:  only %d workers started, expected %d\n",
	       _bu_nthreads_started, ncpu);
    }

    if (UNLIKELY(bu_debug & BU_DEBUG_PARALLEL))
	bu_log("bu_parallel(%d) complete, now serial\n", ncpu);

#  ifdef CHECK_PIDS
    /*
     * At this point, all multi-tasking activity should have ceased,
     * and we should be just a single UNIX process with our original
     * PID and open file table (kernel struct u).  If not, then any
     * output may be written into the wrong file.
     */
    x = bu_process_id();
    if (UNLIKELY(bu_pid_of_initiating_thread != x)) {
	bu_log("WARNING: bu_parallel():  PID of initiating thread changed from %d to %d, open file table may be botched!\n",
	       bu_pid_of_initiating_thread, x);
    }
#  endif
    bu_pid_of_initiating_thread = 0;	/* No threads any more */

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
