/*                      P A R A L L E L . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
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
/** @addtogroup thread */
/** @{ */
/** @file parallel.c
 *
 *  @brief routines for parallel processing
 *
 *  Machine-specific routines for parallel processing.
 *  Primarily calling functions in multiple threads on multiple CPUs.
 *
 *  @author  Michael John Muuss
 *
 *
 *  @par Source -
 *	The U. S. Army Research Laboratory
 *  @n	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *
 */

#ifndef lint
static const char RCSparallel[] = "@(#)$Header$ (ARL)";
#endif

#include "common.h"

/* XXX header mess needs cleaned up */

#include <stdio.h>

#ifdef HAVE_STDLIB_H
# include <stdlib.h>
#elif defined(HAVE_MALLOC_H)
# include <malloc.h>
#endif

#include <ctype.h>
#include <math.h>
#ifdef HAVE_SIGNAL_H
#  include <signal.h>
#endif
#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif
#include "machine.h"
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
#  define SGI_4D	1
#  define _SGI_SOURCE	1	/* IRIX 5.0.1 needs this to def M_BLKSZ */
#  define _BSD_TYPES	1	/* IRIX 5.0.1 botch in sys/prctl.h */
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

/*
 * multithreading support for SunOS 5.X / Solaris 2.x
 */
#if defined(SUNOS) && SUNOS >= 52
#  include <sys/unistd.h>
#  include <thread.h>
#  include <synch.h>
#  define rt_thread_t	thread_t
#endif	/* SUNOS */

/*
 * multithread support built on POSIX Threads (pthread) library.
 */
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#else
#  ifdef HAVE_SYS_UNISTD_H
#    include <sys/unistd.h>
#  endif
#endif
#ifdef HAVE_PTHREAD_H
#  include <pthread.h>
#  define rt_thread_t	pthread_t
#endif

#ifdef CRAY
struct taskcontrol {
	int	tsk_len;
	int	tsk_id;
	int	tsk_value;
} bu_taskcontrol[MAX_PSW];
#endif

/**
 *			B U _ N I C E _ S E T
 *
 *  Without knowing what the current UNIX "nice" value is,
 *  change to a new absolute "nice" value.
 *  (The system routine makes a relative change).
 */
void
bu_nice_set(int newnice)
{
#ifdef _WIN32
  if (bu_debug)
    bu_log("bu_nice_set() Priority NOT changed\n");

  return;

#else  /* not _WIN32 */
  int opri, npri;

#  ifdef BSD
#    ifndef PRIO_PROCESS  /* necessary for linux */
#      define PRIO_PROCESS  0	/* From /usr/include/sys/resource.h */
#    endif
  opri = getpriority( PRIO_PROCESS, 0 );
  setpriority( PRIO_PROCESS, 0, newnice );
  npri = getpriority( PRIO_PROCESS, 0 );

#  else  /* not BSD */
  int bias, chg;

  /* " nice adds the value of incr to the nice value of the process" */
  /* "The default nice value is 20" */
  /* "Upon completion, nice returns the new nice value minus 20" */
  bias = 0;
  opri = nice(0) - bias;
  chg = newnice - opri;
  (void)nice(chg);
  npri = nice(0) - bias;
  if( npri != newnice )  bu_log("bu_nice_set() SysV error:  wanted nice %d! check bias=%d\n", newnice, bias );
#  endif  /* BSD */

  if( bu_debug ) bu_log("bu_nice_set() Priority changed from %d to %d\n", opri, npri);

#endif  /* _WIN32 */
}


/**
 *			B U _ C P U L I M I T _ G E T
 *
 *  Return the current CPU limit, in seconds.
 *  Zero or negative return indicates that limits are not in effect.
 */
int
bu_cpulimit_get(void)
{
#ifdef CRAY
	long	old;			/* 64-bit clock counts */
	extern long limit();

	if( (old = limit( C_PROC, 0, L_CPU, -1 )) < 0 )  {
		perror("bu_cpulimit_get(): CPU limit(get)");
	}
	if( old <= 0 )
		return(999999);		/* virtually unlimited */
	return( (old + HZ - 1) / HZ );
#else
	return(-1);
#endif
}

/**
 *			B U _ C P U L I M I T _ S E T
 *
 *  Set CPU time limit, in seconds.
 */
/* ARGSUSED */
void
bu_cpulimit_set(int sec)
{
#ifdef CRAY
	long	old;		/* seconds */
	long	new;		/* seconds */
	long	newtick;	/* 64-bit clock counts */
	extern long limit();

	old = bu_cpulimit_get();
	new = old + sec;
	if( new <= 0 || new > 999999 )
		new = 999999;	/* no limit, for practical purposes */
	newtick = new * HZ;
	if( limit( C_PROC, 0, L_CPU, newtick ) < 0 )  {
		perror("bu_cpulimit_set: CPU limit(set)");
	}
	bu_log("Cray CPU limit changed from %d to %d seconds\n",
		old, newtick/HZ );

	/* Eliminate any memory limit */
	if( limit( C_PROC, 0, L_MEM, 0 ) < 0 )  {
		/* Hopefully, not fatal if memory limits are imposed */
		perror("bu_cpulimit_set: MEM limit(set)");
	}
#endif
	if (sec < 0) sec = 0;
}


/**
 *			B U _ A V A I L _ C P U S
 *
 *  Return the maximum number of physical CPUs that are considered to be
 *  available to this process now.
 */
int
bu_avail_cpus(void)
{
	int ncpu = -1;


#if defined(_SC_NPROCESSORS_ONLN)
	/* SUNOS and linux */
	ncpu = sysconf(_SC_NPROCESSORS_ONLN);
	if (ncpu < 0) {
		perror("Unable to get the number of available CPUs");
		ncpu = 1;
	}
	goto DONE_NCPU;
#elif defined(_SC_NPROC_ONLN)
	ncpu = sysconf(_SC_NPROC_ONLN);
	if (ncpu < 0) {
		perror("Unable to get the number of available CPUs");
		ncpu = 1;
	}
	goto DONE_NCPU;
#elif defined(_SC_CRAY_NCPU)
	/* cray */
	ncpu = sysconf(_SC_CRAY_NCPU);
	if (ncpu < 0) {
		perror("Unable to get the number of available CPUs");
		ncpu = 1;
	}
	goto DONE_NCPU;
#endif


#ifdef SGI_4D
	/* XXX LAB 04 June 2002
	 * The call prctl(PR_MAXPPROCS) is supposed to indicate the number
	 * of processors this process can use.  Unfortuantely, this returns
	 * 0 when running under a CPU set.  A bug report has been filed with
	 * SGI.
	 *
	 * The sysmp(MP_NPROCS) call returns the number of physically
	 * configured processors.  This will have to suffice until SGI
	 * comes up with a fix.
	 */
#  ifdef HAVE_SYSMP
	ncpu = sysmp(MP_NPROCS);
#  elif defined(HAVE_PRCTL)
	ncpu = (int)prctl(PR_MAXPPROCS);
#  endif
	goto DONE_NCPU;
#endif /* SGI_4D */


#ifdef alliant
	{
	  long	memsize, ipnum, cenum, detnum, attnum;

#  if !defined(i860)
	  /* FX/8 */
	  lib_syscfg( &memsize, &ipnum, &cenum, &detnum, &attnum );
#  else
	  /* FX/2800 */
	  attnum = 28;
#  endif /* i860 */
	  ncpu = attnum;		/* # of CEs attached to parallel Complex */
	  goto DONE_NCPU;
	}
#endif /* alliant */


#if defined(__sp3__)
	{
	  int status;
	  int cmd;
	  int parmlen;
	  struct var p;

	  cmd = SYS_GETPARMS;
	  parmlen = sizeof(struct var);
	  if ( sysconfig(cmd, &p, parmlen) != 0 ) {
	    bu_bomb("bu_parallel(): sysconfig error for sp3");
	  }
	  ncpu = p.v_ncpus;
	  goto DONE_NCPU;
	}
#endif	/* __sp3__ */


#if defined(n16)
	if( (ncpu = sysadmin( SADMIN_NUMCPUS, 0 )) < 0 )
	  perror("sysadmin");
	goto DONE_NCPU;
#endif /* n16 */


#ifdef __FreeBSD__
	{
	  int maxproc;
	  size_t len;
	  len = 4;
	  if (sysctlbyname("hw.ncpu", &maxproc, &len, NULL, 0) == -1) {
	    ncpu = 1;
	    perror("sysctlbyname");
	  } else {
	    ncpu = maxproc;
	  }
	  goto DONE_NCPU;
	}
#endif


#if defined(__ppc__)
	{
	  int mib[2], maxproc;
	  size_t len;

	  mib[0] = CTL_HW;
	  mib[1] = HW_NCPU;
	  len = sizeof(maxproc);
	  if (sysctl(mib, 2, &maxproc, &len, NULL, 0) == -1) {
	    ncpu = 1;
	    perror("sysctl");
	  } else {
	    ncpu = maxproc; /* should be able to get sysctl to return maxproc */
	  }
	  goto DONE_NCPU;
	}
#endif /* __ppc__ */


#if defined(HAVE_GET_NPROCS)
	ncpu = get_nprocs(); /* GNU extension from sys/sysinfo.h */
	goto DONE_NCPU;
#endif


#if defined(linux) && 0
	{
	  /* old retired linux method */
	  /*
	   * Ultra-kludgey way to determine the number of cpus in a
	   * linux box--count the number of processor entries in
	   * /proc/cpuinfo!
	   */

#	define CPUINFO_FILE "/proc/cpuinfo"
	  FILE *fp;
	  char buf[128];

	  ncpu = 0;

	  fp = fopen (CPUINFO_FILE,"r");

	  if (fp == NULL) {
	    ncpu = 1;
	    perror (CPUINFO_FILE);
	  } else {
	    while (bu_fgets(buf, 80, fp) != NULL) {
	      if (strncmp (buf, "processor",9) == 0) {
		++ ncpu;
	      }
	    }
	    fclose (fp);

	    if (ncpu <= 0) {
	      ncpu = 1;
	    }
	  }
	  goto DONE_NCPU;
	}
#endif

#if defined(_WIN32)
	/* Windows */
	{
	    SYSTEM_INFO sysinfo;

	    GetSystemInfo(&sysinfo);
	    ncpu = (int)sysinfo.dwNumberOfProcessors;
	    goto DONE_NCPU;
	}
#endif

DONE_NCPU:  ; /* allows debug and final validity check */


#if defined(HAVE_PTHREAD_H)
	/* if they have threading and we could not detect properly, claim two */
	if (ncpu < 0) {
		ncpu = 2;
	}
#endif

	if (bu_debug & BU_DEBUG_PARALLEL) {
		/* do not use bu_log() here, this can get called before semaphores are initialized */
		fprintf( stderr, "bu_avail_cpus: counted %d cpus.\n", ncpu);
	}

	if (ncpu > 0) {
		return ncpu;
	}

	return( DEFAULT_PSW );
}


/**
 *			B U _ G E T _ L O A D _ A V E R A G E
 *
 *  A generally portable method for obtaining the 1-minute load average.
 *  Vendor-specific methods which don't involve a fork/exec sequence
 *  would be preferable.
 *  Alas, very very few systems put the load average in /proc,
 *  most still grunge the avenrun[3] array out of /dev/kmem,
 *  which requires special privleges to open.
 */
fastf_t
bu_get_load_average(void)
{
	double	load = -1.0;
#ifndef _WIN32
	FILE	*fp;

	fp = popen("PATH=/bin:/usr/bin:/usr/ucb:/usr/bsd; export PATH; uptime|sed -e 's/.*average: //' -e 's/,.*//' ", "r");
	if( !fp )
		return -1.0;

	fscanf( fp, "%lf", &load );
	fclose(fp);

	while( wait(NULL) != -1 )  ;	/* NIL */
#endif
	return load;
}

/**
 *			B U _ G E T _ P U B L I C _ C P U S
 *
 *  A general mechanism for non-privleged users of a server system to control
 *  how many processors of their server get consumed by multi-thread
 *  cruncher processes, by leaving a world-writable file.
 *
 *  If the number in the file is negative, it means "all but that many."
 *
 *  Returns the number of processors presently available for "public" use.
 */
#ifndef _WIN32
#  define PUBLIC_CPUS1	"/var/tmp/public_cpus"
#  define PUBLIC_CPUS2	"/usr/tmp/public_cpus"
#endif
int
bu_get_public_cpus(void)
{
	int	avail_cpus = bu_avail_cpus();
#ifndef _WIN32
	int	public_cpus = 1;
	FILE	*fp;

	if( (fp = fopen(PUBLIC_CPUS1, "r")) != NULL ||
	    (fp = fopen(PUBLIC_CPUS2, "r")) != NULL
	)  {
		(void)fscanf( fp, "%d", &public_cpus );
		fclose(fp);
		if( public_cpus < 0 )  public_cpus = avail_cpus + public_cpus;
		if( public_cpus > avail_cpus )  public_cpus = avail_cpus;
		return public_cpus;
	}

	(void)unlink(PUBLIC_CPUS1);
	(void)unlink(PUBLIC_CPUS2);
	if( (fp = fopen(PUBLIC_CPUS1, "w")) != NULL ||
	    (fp = fopen(PUBLIC_CPUS2, "w")) != NULL
	)  {
		fprintf(fp, "%d\n", avail_cpus);
		fclose(fp);
		(void)chmod(PUBLIC_CPUS1, 0666);
		(void)chmod(PUBLIC_CPUS2, 0666);
	}
#endif
	return avail_cpus;
}

/**
 *			B U _ S E T _ R E A L T I M E
 *
 *  If possible, mark this process for real-time scheduler priority.
 *  Will often need root privs to succeed.
 *
 *  Returns -
 *	1	realtime priority obtained
 *	0	running with non-realtime scheduler behavior
 */
int
bu_set_realtime(void)
{
#	if defined(IRIX64) && IRIX64 >= 64
	{
		int	policy;

		if( (policy = sched_getscheduler(0)) >= 0 )  {
			if( policy == SCHED_RR || policy == SCHED_FIFO )
				return 1;
		}

		sched_getparam( 0, &bu_param );

		if ( sched_setscheduler( 0,
			SCHED_RR,		/* policy */
			&bu_param
		    ) >= 0 )  {
			return 1;		/* realtime */
		}
		/* Fall through to return 0 */
	}
#	endif
	return 0;
}

/**********************************************************************/

#if defined(unix) || defined(__unix)
	/*
	 * Cray is known to wander among various pids, perhaps others.
	 */
#	define	CHECK_PIDS	1
#endif

#if defined(PARALLEL)

/* bu_worker_tbl_not_empty and bu_kill_workers are only used by the sgi arch */
#  ifdef SGI_4D

/**
 *			B U _ W O R K E R _ T B L _ N O T _ E M P T Y
 */
static int
bu_worker_tbl_not_empty(tbl)
int tbl[MAX_PSW];
{
	register int i;
	register int children=0;

	for (i=1 ; i < MAX_PSW ; ++i)
		if (tbl[i]) children++;

	return(children);
}

/**
 *			B U _ K I L L _ W O R K E R S
 */
static void
bu_kill_workers(tbl)
int tbl[MAX_PSW];
{
  register int i;

  for (i=1 ; i < MAX_PSW ; ++i) {
    if ( tbl[i] ) {
      if( kill(tbl[i], 9) ) {
	perror("bu_kill_workers(): SIGKILL to child process");
      }
      else {
	bu_log("bu_kill_workers(): child pid %d killed\n", tbl[i]);
      }
    }
  }

  bzero( (char *)tbl, sizeof(tbl) );
}
#  endif   /* end check if sgi_4d defined */

extern int	bu_pid_of_initiating_thread;	/* From ispar.c */

static int	bu_nthreads_started = 0;	/* # threads started */
static int	bu_nthreads_finished = 0;	/* # threads properly finished */
static void	(*bu_parallel_func) BU_ARGS((int,genptr_t));	/* user function to run in parallel */
static genptr_t	bu_parallel_arg;		/* User's arg to his threads */

/**
 *			B U _ P A R A L L E L _ I N T E R F A C E
 *
 *  Interface layer between bu_parallel and the user's function.
 *  Necessary so that we can provide unique thread numbers as a
 *  parameter to the user's function, and to decrement the global
 *  counter when the user's function returns to us (as opposed to
 *  dumping core or longjmp'ing too far).
 *
 *  Note that not all architectures can pass an argument
 *  (e.g. the pointer to the user's function), so we depend on
 *  using a global variable to communicate this.
 *  This is no problem, since only one copy of bu_parallel()
 *  may be active at any one time.
 */
static void
bu_parallel_interface(void)
{
	register int	cpu;		/* our CPU (thread) number */

#if 0
#ifdef HAVE_PTHREAD_H
	{
		pthread_t	pt;
		pt = pthread_self();
		fprintf(stderr,"bu_parallel_interface, Thread ID = 0x%x\n", (unsigned int)pt);
	}
#endif
#endif
	bu_semaphore_acquire( BU_SEM_SYSCALL );
	cpu = bu_nthreads_started++;
	bu_semaphore_release( BU_SEM_SYSCALL );

	(*bu_parallel_func)(cpu, bu_parallel_arg);

	bu_semaphore_acquire( BU_SEM_SYSCALL );
	bu_nthreads_finished++;
	bu_semaphore_release( BU_SEM_SYSCALL );

#	if defined(SGI_4D) || defined(IRIX)
	/*
	 *  On an SGI, a process/thread created with the "sproc" syscall has
	 *  all of it's file descriptors closed when it "returns" to sproc.
	 *  Since this trashes file descriptors which may still be in use by
	 *  other processes, we avoid ever returning to sproc.
	 */
	if(cpu) _exit(0);
#	endif /* SGI */
}
#endif /* PARALLEL */

#ifdef SGI_4D
/**
 *			B U _ P R _ F I L E
 *
 *  SGI-specific.  Formatted printing of stdio's FILE struct.
 */
void
bu_pr_FILE(title, fp)
char	*title;
FILE	*fp;
{
	bu_log("FILE structure '%s', at x%x:\n", title, fp );
	bu_log(" _cnt = x%x\n", fp->_cnt);
	bu_log(" _ptr = x%x\n", fp->_ptr);
	bu_log(" _base = x%x\n", fp->_base);
	bu_log(" _file = x%x\n", fp->_file);
	bu_printb(" _flag ", fp->_flag & 0xFF,
		"\010\010_IORW\7_100\6_IOERR\5_IOEOF\4_IOMYBUF\3_004\2_IOWRT\1_IOREAD" );
	bu_log("\n");
}
#endif

/**
 *			B U _ P A R A L L E L
 *
 *  Create 'ncpu' copies of function 'func' all running in parallel,
 *  with private stack areas.  Locking and work dispatching are
 *  handled by 'func' using a "self-dispatching" paradigm.
 *
 *  'func' is called with one parameter, it's thread number.
 *  Threads are given increasing numbers, starting with zero.
 *
 *  This function will not return control until all invocations
 *  of the subroutine are finished.
 *
 *  Don't use registers in this function (bu_parallel).  At least on the Alliant,
 *  register context is NOT preserved when exiting the parallel mode,
 *  because the serial portion resumes on some arbitrary processor,
 *  not necessarily the one that serial execution started on.
 *  The registers are not shared.
 */
void
bu_parallel( func, ncpu, arg )
void		(*func) BU_ARGS((int, genptr_t));
int		ncpu;
genptr_t	arg;
{
#if defined(PARALLEL)
	int	avail_cpus;

#  if defined(alliant) && !defined(i860) && !__STDC__
	register int d7;	/* known to be in d7 */
	register int d6 = ncpu;	/* known to be in d6 */
#  endif
	int	x;

#  if defined(SGI_4D) || defined(CRAY)
	int	new;
#  endif

#  ifdef sgi
	long	stdin_pos;
	FILE	stdin_save;
	int	worker_pid_tbl[MAX_PSW];
#  endif

/*
 * multithreading support for SunOS 5.X / Solaris 2.x
 */
#  if defined(SUNOS) && SUNOS >= 52
	static int	concurrency = 0; /* Max concurrency we have set */
#  endif
#  if (defined(SUNOS) && SUNOS >= 52) || defined(HAVE_PTHREAD_H)
	int		nthreadc;
	int		nthreade;
	rt_thread_t	thread;
	rt_thread_t	thread_tbl[MAX_PSW];
	int		i;
#  endif	/* SUNOS */

#  ifdef sgi
	bzero(worker_pid_tbl, sizeof(worker_pid_tbl) );
#  endif

	if( bu_debug & BU_DEBUG_PARALLEL )
		bu_log("bu_parallel(0x%lx, %d, x%lx)\n", (long)func, ncpu, (long)arg );

	if( bu_pid_of_initiating_thread )
		bu_bomb("bu_parallel() called from within parallel section\n");

	bu_pid_of_initiating_thread = getpid();

	if (ncpu > MAX_PSW) {
		bu_log("WARNING: bu_parallel() ncpu(%d) > MAX_PSW(%d), adjusting ncpu\n", ncpu, MAX_PSW);
		ncpu = MAX_PSW;
	}
	bu_nthreads_started = 0;
	bu_nthreads_finished = 0;
	bu_parallel_func = func;
	bu_parallel_arg = arg;
	avail_cpus = bu_avail_cpus();
	if( ncpu > avail_cpus ) {
		bu_log( "%d cpus requested, but only %d available\n", ncpu, avail_cpus );
		ncpu = avail_cpus;
	}


#  ifdef HEP
	bu_nthreads_started = 1;
	bu_nthreads_finished = 1;
	for( x=1; x<ncpu; x++ )  {
		/* This is more expensive when GEMINUS>1 */
		Dcreate( bu_parallel_interface );
	}
	(*func)(0,arg);	/* avoid wasting this task */
#  endif /* HEP */

#  ifdef CRAY
#    if 0
	/* Try to give up processors as soon as they are un needed */
	new = 0;
	TSKTUNE( "DBRELEAS", &new );
#    endif

	bu_nthreads_started = 1;
	bu_nthreads_finished = 1;
	/* Create any extra worker tasks */
	for( x=1; x<ncpu; x++ ) {
		bu_taskcontrol[x].tsk_len = 3;
		bu_taskcontrol[x].tsk_value = x;
		TSKSTART( &bu_taskcontrol[x], bu_parallel_interface );
	}
	(*func)(0,arg);	/* avoid wasting this task */

	/* Wait for them to finish */
	for( x=1; x<ncpu; x++ )  {
		TSKWAIT( &bu_taskcontrol[x] );
	}
	/* There needs to be some way to kill the tfork()'ed processes here */
#  endif

#  if defined(alliant) && !defined(i860)
#	if defined(__STDC__)	/* fxc defines it == 0 !! */
#	undef __STDC__
#	define __STDC__	2

	/* Calls bu_parallel_interface in parallel "ncpu" times */
	concurrent_call(CNCALL_COUNT|CNCALL_NO_QUIT, bu_parallel_interface, ncpu);

#	else
	{
		asm("	movl		d6,d0");
		asm("	subql		#1,d0");
		asm("	cstart		d0");
		asm("super_loop:");
		bu_parallel_interface();		/* d7 has current index, like magic */
		asm("	crepeat		super_loop");
	}
#	endif
#  endif

#  if defined(alliant) && defined(i860)
	#pragma loop cncall
	for( x=0; x<ncpu; x++) {
		bu_parallel_interface();
	}
#  endif

#  if defined(convex) || defined(__convex__)
	/*$dir force_parallel */
	for( x=0; x<ncpu; x++ )  {
		bu_parallel_interface();
	}
#  endif /* convex */

#  ifdef ardent
	/* The stack size parameter is pure guesswork */
	parstack( bu_parallel_interface, 1024*1024, ncpu );
#  endif /* ardent */

#  ifdef SGI_4D
	stdin_pos = ftell(stdin);
	stdin_save = *(stdin);		/* struct copy */
	bu_nthreads_started = 1;
	bu_nthreads_finished = 1;

	/* Note:  it may be beneficial to call prctl(PR_SETEXITSIG); */
	/* prctl(PR_TERMCHILD) could help when parent dies.  But SIGHUP??? hmmm */
	for( x = 1; x < ncpu; x++)  {
		/*
		 *  Start a share-group process, sharing ALL resources.
		 *  This direct sys-call can be used because none of the
		 *  task-management services of, eg, taskcreate() are needed.
		 */
#    if defined(IRIX) && IRIX <= 4
		/*  Stack size per proc comes from RLIMIT_STACK (typ 64MBytes). */
		new = sproc( bu_parallel_interface, PR_SALL, 0 );
#    else
		/* State maximum stack size.
		 * Be generous, as this mainly costs address space.
		 * RAM is allocated only to those pages used.
		 * On the other hand, don't be too generous, because each
		 * proc needs this much space on, e.g. a 64 processor system.
		 * Don't go quite for an even number of megabytes,
		 * in the hopes of creating a small 32k "buffer zone"
		 * to catch stack overflows.
		 */
		new = sprocsp( (void (*)(void *, size_t))bu_parallel_interface,
			PR_SALL, 0, NULL,
#			if defined(IRIX64)
				64*1024*1024 - 32*1024
#			else
				4*1024*1024 - 32*1024
#			endif
			);
#    endif
		if( new < 0 )  {
			perror("sproc");
			bu_log("ERROR bu_parallel(): sproc(x%x, x%x, )=%d failed on processor %d\n",
				bu_parallel_interface, PR_SALL,
				new, x );
			bu_log("sbrk(0)=x%x\n", sbrk(0) );
			bu_bomb("bu_parallel() failure");
		} else {
			worker_pid_tbl[x] = new;
		}

	}
	(*func)(0,arg);	/* don't waste this thread */
	{
		int	pid;
		int	pstat;
		int	children;

		/*
		 * Make sure all children are done.
		 */
		while ( children=bu_worker_tbl_not_empty(worker_pid_tbl) ) {
			pstat = 0;
			if ( (pid = wait(&pstat)) < 0) {
				perror("bu_parallel() wait()");
				bu_kill_workers(worker_pid_tbl);
				bu_bomb("parallelism error");
			} else if (pid == 0) {
				bu_log("bu_parallel() wait() == 0 with %d children remaining\n", children);
				bu_kill_workers(worker_pid_tbl);
				bu_bomb("Missing worker");
			} else {
				if( (pstat & 0xFF) != 0 )  {
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

	if ( (pstat & 0177777) == 0177777 )
		bu_log("child has continued\n");

#    endif
					bu_kill_workers(worker_pid_tbl);
					bu_bomb("A worker blew out");
				}
				/* remove pid from worker_pid_tbl */
				for (x=1 ; x < ncpu ; x++)
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
	if( ftell(stdin) != stdin_pos )  {
		/*
		 *  Gross SGI bug:  when a thread is finished, it returns
		 *  to the stack frame created by sproc(), which
		 *  just calls exit(0), resulting in all STDIO file buffers
		 *  being fflush()ed.  This zaps the stdin position, and
		 *  may wreak additional havoc.
		 *  Exists in IRIX 3.3.1, Irix 4.0.5,
		 *  should be fixed in a later release.  Maybe.
		 */
		bu_log("\nWarning:  stdin file pointer has been corrupted by SGI multi-processor bug!\n");
		if( bu_debug & BU_DEBUG_PARALLEL )  {
			bu_log("Original position was x%x, now position is x%x!\n", stdin_pos, ftell(stdin) );
			bu_pr_FILE("saved stdin", &stdin_save);
			bu_pr_FILE("current stdin", stdin);
		}
		fseek(stdin, stdin_pos, SEEK_SET);
		if( ftell(stdin) != stdin_pos )  {
			bu_log("WARNING: fseek() did not recover proper position.\n");
		} else {
			bu_log("It was fixed by fseek()\n");
		}
	}
#  endif /* sgi */

#  if defined(n16)
	/* The shared memory size requirement is sheer guesswork */
	/* The stack size is also guesswork */
	if( task_init( 8*1024*1024, ncpu, bu_parallel_interface, 128*1024, 0 ) < 0 )
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
	for (x = 0; x < ncpu; x++)  {

		if (thr_create(0, 0, (void *(*)(void *))bu_parallel_interface, 0, 0, &thread)) {
			fprintf(stderr, "ERROR parallel.c/bu_parallel(): thr_create(0x0, 0x0, 0x%x, 0x0, 0, 0x%x) failed on processor %d\n",
				bu_parallel_interface, &thread, x);
			bu_log("ERROR parallel.c/bu_parallel(): thr_create(0x0, 0x0, 0x%x, 0x0, 0, 0x%x) failed on processor %d\n",
				bu_parallel_interface, &thread, x);
			/* Not much to do, lump it */
		} else {
			if( bu_debug & BU_DEBUG_PARALLEL )
				bu_log("bu_parallel(): created thread: (thread: 0x%x) (loop:%d) (nthreadc:%d)\n",
				       thread, x, nthreadc);

			thread_tbl[nthreadc] = thread;
			nthreadc++;
		}
	}

	if( bu_debug & BU_DEBUG_PARALLEL )
		for (i = 0; i < nthreadc; i++)
			bu_log("bu_parallel(): thread_tbl[%d] = 0x%x\n",
			       i, thread_tbl[i]);

	/*
	 * Wait for completion of all threads.  We don't wait for
	 * threads in order.  We wait for any old thread but we keep
	 * track of how many have returned and whether it is one that we
	 * started
	 */
	thread = 0;
	nthreade = 0;
	for (x = 0; x < nthreadc; x++)  {
		if( bu_debug & BU_DEBUG_PARALLEL )
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

		if( bu_debug & BU_DEBUG_PARALLEL )
			bu_log("bu_parallel(): thread completed: (thread: %d)\t(loop:%d) (nthreadc:%d) (nthreade:%d)\n",
			       thread, x, nthreadc, nthreade);
	}

	if( bu_debug & BU_DEBUG_PARALLEL )
		bu_log("bu_parallel(): %d threads created.  %d threads exited.\n",
		       nthreadc, nthreade);
#  endif	/* SUNOS */

#  if defined(HAVE_PTHREAD_H) && !defined(sgi)

	thread = 0;
	nthreadc = 0;

	/* XXX How to advise thread library that we need 'ncpu' processors? */

	/* Create the threads */
	for (x = 0; x < ncpu; x++)  {
		pthread_attr_t attrs;
		pthread_attr_init(&attrs);
		pthread_attr_setstacksize(&attrs,10*1024*1024);

		if (pthread_create(&thread, &attrs,
		    (void *(*)(void *))bu_parallel_interface, NULL)) {
			fprintf(stderr, "ERROR parallel.c/bu_parallel(): thr_create(0x0, 0x0, 0x%lx, 0x0, 0, 0x%lx) failed on processor %d\n",
				(unsigned long int)bu_parallel_interface, (unsigned long int)&thread, x);
			bu_log("ERROR parallel.c/bu_parallel(): thr_create(0x0, 0x0, 0x%x, 0x0, 0, 0x%x) failed on processor %d\n",
				bu_parallel_interface, &thread, x);
			/* Not much to do, lump it */
		} else {
			if( bu_debug & BU_DEBUG_PARALLEL ) {
				bu_log("bu_parallel(): created thread: (thread: %d) (loop:%d) (nthreadc:%d)\n",
				       thread, x, nthreadc);
			}

			thread_tbl[nthreadc] = thread;
			nthreadc++;
		}
	}


	if( bu_debug & BU_DEBUG_PARALLEL ) {
		for (i = 0; i < nthreadc; i++) {
			bu_log("bu_parallel(): thread_tbl[%d] = %d\n",
			       i, thread_tbl[i]);
		}
#    if defined(HAVE_RAISE) && defined(SIGINFO)
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
	for (x = 0; x < nthreadc; x++)  {
		int ret;

		if( bu_debug & BU_DEBUG_PARALLEL )
			bu_log("bu_parallel(): waiting for thread x%x to complete:\t(loop:%d) (nthreadc:%d) (nthreade:%d)\n",
				thread_tbl[x], x, nthreadc, nthreade);

		if ( (ret = pthread_join(thread_tbl[x], NULL)) != 0) {
			/* badness happened */
			fprintf(stderr, "pthread_join(thread_tbl[%d]=0x%x) ret=%d\n", x, (unsigned int)thread_tbl[x], ret);
		}
		nthreade++;
		thread_tbl[x] = (rt_thread_t)-1;

		if( bu_debug & BU_DEBUG_PARALLEL )
			bu_log("bu_parallel(): thread completed: (thread: %d)\t(loop:%d) (nthreadc:%d) (nthreade:%d)\n",
			       thread, x, nthreadc, nthreade);
	}

	if( bu_debug & BU_DEBUG_PARALLEL )
		bu_log("bu_parallel(): %d threads created.  %d threads exited.\n",
		       nthreadc, nthreade);

#  endif /* end if posix threads */

	/*
	 *  Ensure that all the threads are REALLY finished.
	 *  On some systems, if threads core dump, the rest of
	 *  the gang keeps going, so this can actually happen (sigh).
	 */
	if( bu_nthreads_finished != bu_nthreads_started )  {
		bu_log("*** ERROR bu_parallel(%d): %d workers did not finish!\n\n",
			ncpu, ncpu - bu_nthreads_finished);
	}
	if( bu_nthreads_started != ncpu )  {
		bu_log("bu_parallel() NOTICE:  only %d workers started, expected %d\n",
			bu_nthreads_started, ncpu );
	}

	if( bu_debug & BU_DEBUG_PARALLEL )
		bu_log("bu_parallel(%d) complete, now serial\n", ncpu);

#  ifdef CHECK_PIDS
	/*
	 * At this point, all multi-tasking activity should have ceased,
	 * and we should be just a single UNIX process with our original
	 * PID and open file table (kernel struct u).  If not, then any
	 * output may be written into the wrong file.
	 */
	if( bu_pid_of_initiating_thread != (x=getpid()) )  {
		bu_log("WARNING: bu_parallel():  PID of initiating thread changed from %d to %d, open file table may be botched!\n",
			bu_pid_of_initiating_thread, x );
	}
#  endif
	bu_pid_of_initiating_thread = 0;	/* No threads any more */
#else	/* PARALLEL */
	bu_log("bu_parallel( x%lx, %d., x%lx ):  Not compiled for PARALLEL machine, running single-threaded\n", (long)func, ncpu, (long)arg );
	/* do the work anyways */
	(*func)(0,arg);
#endif	/* PARALLEL */

	return;
}

#if defined(sgi) && !defined(mips)
/* Horrible bug in 3.3.1 and 3.4 and 3.5 -- hypot ruins stack! */
long float
hypot(a,b)
double a,b;
{
	return(sqrt(a*a+b*b));
}
#endif /* sgi */

/** @} */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
