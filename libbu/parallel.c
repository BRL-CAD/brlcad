/*
 *			P A R A L L E L . C
 *
 *  Machine-specific routines for parallel processing.
 *  Primarily calling functions in multiple threads on multiple CPUs.
 *
 *  Author -
 *	Michael John Muuss
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Status -
 *	Public Domain, Distribution Unlimited.
 */
#ifndef lint
static char RCSparallel[] = "@(#)$Header$ (ARL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <ctype.h>
#include <math.h>
#include "machine.h"
#include "externs.h"
#include "bu.h"

#ifdef CRAY
# include <sys/category.h>
# include <sys/resource.h>
# include <sys/types.h>
# ifdef CRAY1
#  include <sys/machd.h>	/* For HZ */
# endif
#endif

#ifdef CRAY2
#undef MAXINT
# include <sys/param.h>
#endif

#ifdef HEP
# include <synch.h>
# undef stderr
# define stderr stdout
#endif /* HEP */

#if defined(alliant) && !defined(i860)
/* Alliant FX/8 */
# include <cncall.h>
#endif

#if (defined(sgi) && defined(mips)) || (defined(__sgi) && defined(__mips))
# define SGI_4D	1
# define _SGI_SOURCE	1	/* IRIX 5.0.1 needs this to def M_BLKSZ */
# define _BSD_TYPES	1	/* IRIX 5.0.1 botch in sys/prctl.h */
#if ( IRIX == 6 )
typedef __uint64_t k_sigset_t;  /* signal set type */
#endif
# include <sys/types.h>
# include <ulocks.h>
/* ulocks.h #include's <limits.h> and <malloc.h> */
/* ulocks.h #include's <task.h> for getpid stuff */
/* task.h #include's <sys/prctl.h> */
# include <malloc.h>
/* <malloc.h> #include's <stddef.h> */

#include <sys/wait.h>

#endif /* SGI_4D */

/* XXX Probably need to set _SGI_MP_SOURCE in machine.h */

#ifdef ardent
#	include <thread.h>
#endif

#if defined(n16)
#	include <parallel.h>
#	include <sys/sysadmin.h>
#endif

/*
 * multithreading support for SunOS 5.X / Solaris 2.x
 */
#if SUNOS >= 52
#	include <sys/unistd.h>
#	include <thread.h>
#	include <synch.h>
#endif	/* SUNOS */

#ifdef CRAY
struct taskcontrol {
	int	tsk_len;
	int	tsk_id;
	int	tsk_value;
} bu_taskcontrol[MAX_PSW];
#endif

/*
 *			B U _ N I C E _ S E T
 *
 *  Without knowing what the current UNIX "nice" value is,
 *  change to a new absolute "nice" value.
 *  (The system routine makes a relative change).
 */
void
bu_nice_set(newnice)
int	newnice;
{
	int opri, npri, chg;
	int bias;

#ifdef BSD
#define	PRIO_PROCESS	0	/* From /usr/include/sys/resource.h */
	opri = getpriority( PRIO_PROCESS, 0 );
	setpriority( PRIO_PROCESS, 0, newnice );
	npri = getpriority( PRIO_PROCESS, 0 );
#else
	/* " nice adds the value of incr to the nice value of the process" */
	/* "The default nice value is 20" */
	/* "Upon completion, nice returns the new nice value minus 20" */
	bias = 0;
	opri = nice(0) - bias;
	chg = newnice - opri;
	(void)nice(chg);
	npri = nice(0) - bias;
	if( npri != newnice )  bu_log("bu_nice_set() SysV error:  wanted nice %d! check bias=%d\n", newnice, bias );
#endif
	if( bu_debug ) bu_log("bu_nice_set() Priority changed from %d to %d\n", opri, npri);
}

/*
 *			B U _ C P U L I M I T _ G E T
 *
 *  Return the current CPU limit, in seconds.
 *  Zero or negative return indicates that limits are not in effect.
 */
int
bu_cpulimit_get()
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

/*
 *			B U _ C P U L I M I T _ S E T
 *
 *  Set CPU time limit, in seconds.
 */
/* ARGSUSED */
void
bu_cpulimit_set(sec)
int	sec;
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
}

/*
 *			B U _ A V A I L _ C P U S
 *
 *  Return the maximum number of physical CPUs that are considered to be
 *  available to this process now.
 */
int
bu_avail_cpus()
{
	int	ret = 1;

#ifdef SGI_4D
	ret = (int)prctl(PR_MAXPPROCS);
#	define	RT_AVAIL_CPUS
#endif

#ifdef alliant
	long	memsize, ipnum, cenum, detnum, attnum;

#if !defined(i860)
	/* FX/8 */
	lib_syscfg( &memsize, &ipnum, &cenum, &detnum, &attnum );
#else
	/* FX/2800 */
	attnum = 28;
#endif
	ret = attnum;		/* # of CEs attached to parallel Complex */
#	define	RT_AVAIL_CPUS
#endif

#if defined(SUNOS)
	if ((ret = sysconf(_SC_NPROCESSORS_ONLN)) == -1) {
		ret = 1;
		perror("sysconf");
	}
#       define  RT_AVAIL_CPUS
#endif	/* defined(SUNOS) */

#if defined(n16)
	if( (ret = sysadmin( SADMIN_NUMCPUS, 0 )) < 0 )
		perror("sysadmin");
#	define	RT_AVAIL_CPUS
#endif

#ifndef RT_AVAIL_CPUS
	ret = DEFAULT_PSW;
#endif
	return( ret );
}

/**********************************************************************/

#if defined(unix) || defined(__unix)
	/*
	 * Cray is known to wander among various pids, perhaps others.
	 */
#	define	CHECK_PIDS	1
#endif

/*
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

/*
 *			B U _ K I L L _ W O R K E R S
 */
static void
bu_kill_workers(tbl)
int tbl[MAX_PSW];
{
	register int i;
	
	for (i=1 ; i < MAX_PSW ; ++i)
	    if ( tbl[i] )
	    	if( kill(tbl[i], 9) )
			perror("bu_kill_workers(): SIGKILL to child process");
		else
			bu_log("bu_kill_workers(): child pid %d killed\n", tbl[i]);

	bzero( (char *)tbl, sizeof(tbl) );
}

#ifdef SGI_4D
/*
 *			B U _ S G I _ F U N C
 *
 *	On an SGI, a process/thread created with the "sproc" syscall has
 *	all of it's file descriptors closed when it "returns" to sproc.
 *	Since this trashes file descriptors which may still be in use by
 *	other processes, we avoid ever returning to sproc.
 *
 *	Rather than calling sproc with the worker function, it is called with
 *	bu_sgi_func().  When "func" returns, calling _exit() gets the process
 *	killed without going through the file-descriptor bashing
 */
#if IRIX <= 4
static void
bu_sgi_func( arg )
void	*arg;
{
	void	(*func)() = (void (*)())arg;

	(*func)();

	_exit(0);
}
#else
static void
bu_sgi_func( arg, stksize )
void	*arg;
size_t	stksize;
{
	void	(*func)() = (void (*)())arg;

	(*func)();

	_exit(0);
}
#endif

/*
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

extern int	bu_pid_of_initiating_thread;

/*
 *			B U _ P A R A L L E L
 *
 *  Create 'ncpu' copies of function 'func' all running in parallel,
 *  with private stack areas.  Locking and work dispatching are
 *  handled by 'func' using a "self-dispatching" paradigm.
 *  No parameters are passed to 'func', because not all machines
 *  can arrange for that.
 *
 *  This function will not return control until all invocations
 *  of the subroutine are finished.  The caller might want to double-check
 *  this, using cooperation with 'func'.
 *
 *  Don't use registers in this function (bu_parallel).  At least on the Alliant,
 *  register context is NOT preserved when exiting the parallel mode,
 *  because the serial portion resumes on some arbitrary processor,
 *  not necessarily the one that serial execution started on.
 *  The registers are not shared.
 */
void
bu_parallel( func, ncpu )
void	(*func)();
int	ncpu;
{
#if defined(PARALLEL)

#if defined(alliant) && !defined(i860) && !__STDC__
	register int d7;	/* known to be in d7 */
	register int d6 = ncpu;	/* known to be in d6 */
#endif
	int	x;
	int	new;
#ifdef sgi
	long	stdin_pos;
	FILE	stdin_save;
	int	worker_pid_tbl[MAX_PSW];

	bzero(worker_pid_tbl, sizeof(worker_pid_tbl) );
#endif

/*
 * multithreading support for SunOS 5.X / Solaris 2.x
 */
#if SUNOS >= 52
	int		nthreadc;
	int		nthreade;
	static int	concurrency = 0; /* Max concurrency we have set */
	thread_t	thread;
	thread_t	thread_tbl[MAX_PSW];
	int		i;
#endif	/* SUNOS */

	if( bu_debug & BU_DEBUG_PARALLEL )
		bu_log("bu_parallel(0x%x, %d)\n", func, ncpu );

	bu_pid_of_initiating_thread = getpid();

	if (ncpu > MAX_PSW) {
		bu_log("WARNING: bu_parallel() ncpu(%d) > MAX_PSW(%d), adjusting ncpu\n", ncpu, MAX_PSW);
		ncpu = MAX_PSW;
	}

#ifdef HEP
	for( x=1; x<ncpu; x++ )  {
		/* This is more expensive when GEMINUS>1 */
		Dcreate( *func );
	}
	(*func)();	/* avoid wasting this task */
#endif /* HEP */

#ifdef CRAY
#if 0
	/* Try to give up processors as soon as they are un needed */
	new = 0;
	TSKTUNE( "DBRELEAS", &new );
#endif

	/* Create any extra worker tasks */
	for( x=1; x<ncpu; x++ ) {
		bu_taskcontrol[x].tsk_len = 3;
		bu_taskcontrol[x].tsk_value = x;
		TSKSTART( &bu_taskcontrol[x], func );
	}
	(*func)();	/* avoid wasting this task */

	/* There needs to be some way to kill the tfork()'ed processes here */
	/* Wait for them to finish */
	for( x=1; x<ncpu; x++ )  {
		TSKWAIT( &bu_taskcontrol[x] );
	}
#endif

#if defined(alliant) && !defined(i860)
#	if defined(__STDC__)	/* fxc defines it == 0 !! */
#	undef __STDC__
#	define __STDC__	2

	/* Calls func in parallel "ncpu" times */
	concurrent_call(CNCALL_COUNT|CNCALL_NO_QUIT, func, ncpu);

#	else
	{
		asm("	movl		d6,d0");
		asm("	subql		#1,d0");
		asm("	cstart		d0");
		asm("super_loop:");
		(*func)();		/* d7 has current index, like magic */
		asm("	crepeat		super_loop");
	}
#	endif
#endif

#if defined(alliant) && defined(i860)
        #pragma loop cncall
        for( x=0; x<ncpu; x++) {
                (*func)();
        }
#endif

#if defined(convex) || defined(__convex__)
	/*$dir force_parallel */
	for( x=0; x<ncpu; x++ )  {
		(*func)();
	}
#endif /* convex */

#ifdef ardent
	/* The stack size parameter is pure guesswork */
	parstack( func, 1024*1024, ncpu );
#endif /* ardent */

#ifdef SGI_4D
	stdin_pos = ftell(stdin);
	stdin_save = *(stdin);		/* struct copy */

	/* Note:  it may be beneficial to call prctl(PR_SETEXITSIG); */
	/* prctl(PR_TERMCHILD) could help when parent dies.  But SIGHUP??? hmmm */
	for( x = 1; x < ncpu; x++)  {
		/*
		 *  Start a share-group process, sharing ALL resources.
		 *  This direct sys-call can be used because none of the
		 *  task-management services of, eg, taskcreate() are needed.
		 */
#if IRIX <= 4
		/*  Stack size per proc comes from RLIMIT_STACK (64MBytes). */
		new = sproc( bu_sgi_func, PR_SALL, func );
#else
		new = sprocsp( bu_sgi_func, PR_SALL, func, NULL, 4*1024*1024 );
#endif
		if( new < 0 )  {
			perror("sproc");
			bu_log("ERROR parallel.c/bu_parallel(): sproc(x%x, x%x, )=%d failed on processor %d\n",
				func, PR_SALL,
				new, x );
			bu_log("sbrk(0)=x%x\n", sbrk(0) );
			bu_bomb("bu_parallel() failure");
		} else {
			worker_pid_tbl[x] = new;
		}
		
	}
	(*func)();
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
#ifdef IRIX
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

#endif
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
#if 0
		bu_log("Original position was x%x, now position is x%x!\n", stdin_pos, ftell(stdin) );
		bu_pr_FILE("saved stdin", &stdin_save);
		bu_pr_FILE("current stdin", stdin);
#endif
		fseek(stdin, stdin_pos, SEEK_SET);
		if( ftell(stdin) != stdin_pos )  {
			bu_log("WARNING: fseek() did not recover proper position.\n");
		} else {
			bu_log("It was fixed by fseek()\n");
		}
	}
#endif /* sgi */

#if defined(n16)
	/* The shared memory size requirement is sheer guesswork */
	/* The stack size is also guesswork */
	if( task_init( 8*1024*1024, ncpu, func, 128*1024, 0 ) < 0 )
		perror("bu_parallel()/task_init()");
#endif

	/*
	 * multithreading support for SunOS 5.X / Solaris 2.x
	 */
#if SUNOS >= 52

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

		if (thr_create(0, 0, (void *(*)(void *))func, 0, 0, &thread)) {
			fprintf(stderr, "ERROR parallel.c/bu_parallel(): thr_create(0x0, 0x0, 0x%x, 0x0, 0, 0x%x) failed on processor %d\n",
				func, &thread, x);
			bu_log("ERROR parallel.c/bu_parallel(): thr_create(0x0, 0x0, 0x%x, 0x0, 0, 0x%x) failed on processor %d\n",
				func, &thread, x);
			/* Not much to do, lump it */
		} else {
			if( bu_debug & BU_DEBUG_PARALLEL )
				bu_log("bu_parallel(): created thread: (thread: %d) (loop:%d) (nthreadc:%d)\n",
				       thread, x, nthreadc);

			thread_tbl[nthreadc] = thread;
			nthreadc++;
		}
	}

	if( bu_debug & BU_DEBUG_PARALLEL )
		for (i = 0; i < nthreadc; i++)
			bu_log("bu_parallel(): thread_tbl[%d] = %d\n",
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

		if (thr_join((thread_t)0, &thread, NULL)) {
			/* badness happened */
			fprintf(stderr, "thr_join()");
		}

		/* Check to see if this is one the threads we created */
		for (i = 0; i < nthreadc; i++) {
			if (thread_tbl[i] == thread) {
				thread_tbl[i] = -1;
				nthreade++;
				break;
			}
		}

		if ((thread_tbl[i] != -1) && i < nthreadc) {
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
#endif	/* SUNOS */

	if( bu_debug & BU_DEBUG_PARALLEL )
		bu_log("bu_parallel() complete, now serial\n");

#ifdef CHECK_PIDS
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
#endif
	bu_pid_of_initiating_thread = 0;	/* No threads any more */
#else	/* PARALLEL */
	bu_log("bu_parallel( x%x, %d. ):  Not compiled for PARALLEL machine\n",
		func, ncpu );
#endif	/* PARALLEL */
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
