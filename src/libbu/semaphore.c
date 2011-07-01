/*                     S E M A P H O R E . C
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

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <math.h>
#include "bu.h"

#ifdef CRAY
# include <sys/category.h>
# include <sys/resource.h>
# include <sys/types.h>
# ifdef CRAY1
#  include <sys/machd.h>	/* For HZ */
# endif
struct bu_semaphores {
    long magic;
    long p;
};
# define DEFINED_BU_SEMAPHORES 1
#endif

#ifdef CRAY2
#undef MAXINT
# include <sys/param.h>
#endif

#if defined(alliant) && !defined(i860)
/* Alliant FX/8 */
# include <cncall.h>
struct bu_semaphores {
    long magic;
    char c;
};
# define DEFINED_BU_SEMAPHORES 1
#endif

#if (defined(sgi) && defined(mips)) || (defined(__sgi) && defined(__mips))
# define SGI_4D 1
# define _SGI_SOURCE 1	/* IRIX 5.0.1 needs this to def M_BLKSZ */
# define _BSD_TYPES 1	/* IRIX 5.0.1 botch in sys/prctl.h */
# include <sys/types.h>
# include <ulocks.h>
/* ulocks.h #include's <limits.h> and <malloc.h> */
/* ulocks.h #include's <task.h> for getpid stuff */
/* task.h #include's <sys/prctl.h> */
# include <malloc.h>
/* <malloc.h> #include's <stddef.h> */

#ifdef HAVE_SYS_WAIT_H
#  include <sys/wait.h>
#endif

static char bu_lockfile[MAXPATHLEN] = {0};

static usptr_t *bu_lockstuff = 0;
extern int _utrace;

struct bu_semaphores {
    long magic;
    ulock_t ltp;
};
# define DEFINED_BU_SEMAPHORES 1
#endif /* SGI_4D */

/* XXX May need to set _SGI_MP_SOURCE */

#ifdef ardent
#	include <thread.h>
struct bu_semaphores {
    long magic;
    char sem;
};
# define DEFINED_BU_SEMAPHORES 1
#endif

#if defined(convex) || defined(__convex__)
struct bu_semaphores {
    long magic;
    long sem;
};
# define DEFINED_BU_SEMAPHORES 1
#endif

#if defined(n16)
#	include <parallel.h>
#	include <sys/sysadmin.h>
struct bu_semaphores {
    long magic;
    char sem;
};
# define DEFINED_BU_SEMAPHORES 1
#endif

#include "bio.h"

/*
 * multithreading support for SunOS 5.X / Solaris 2.x
 */
#if defined(SUNOS) && SUNOS >= 52
#	include <sys/unistd.h>
#	include <thread.h>
#	include <synch.h>
struct bu_semaphores {
    long magic;
    mutex_t mu;
};
# define DEFINED_BU_SEMAPHORES 1
#endif	/* SUNOS */

/*
 * multithread support built on POSIX Threads (pthread) library.
 */
#ifdef HAVE_PTHREAD_H
#	include <pthread.h>
#  if !defined(sgi)
struct bu_semaphores {
    long magic;
    pthread_mutex_t mu;
};
#	define DEFINED_BU_SEMAPHORES 1
#  endif
#endif


#define BU_SEMAPHORE_MAGIC 0x62757365

#if defined(SGI_4D)
HIDDEN void
_bu_semaphore_sgi_init()
{
    FILE *fp;
    /*
     * First time through.
     * Use this opportunity to tune malloc().  It needs it!
     * Default for M_BLKSZ is 8k.
     */
    if (mallopt(M_BLKSZ, 128*1024) != 0) {
	fprintf(stderr, "_bu_semaphore_sgi_init: mallopt() failed\n");
    }

    /* Now, set up the lock arena */
    fp = bu_temp_file(bu_lockfile, MAXPATHLEN);

    if (UNLIKELY(bu_debug & BU_DEBUG_PARALLEL)) {
	if (usconfig(CONF_LOCKTYPE, _USDEBUGPLUS) == -1)
	    perror("usconfig CONF_LOCKTYPE");
    }
    /*
     * Note that libc mp debugging to stderr can be enabled by saying
     * int _utrace=1;
     */

    /* Cause lock file to vanish on exit */
    usconfig(CONF_ARENATYPE, US_SHAREDONLY);

    /* Set maximum number of procs that can share this arena */
    usconfig(CONF_INITUSERS, bu_avail_cpus()+1);

    if (UNLIKELY(bu_debug & BU_DEBUG_PARALLEL)) {
	/* This is a big performance hit, but may find bugs */
	usconfig(CONF_LOCKTYPE, US_DEBUG);
    } else {
	usconfig(CONF_LOCKTYPE, US_NODEBUG);
    }

    /* Initialize arena */
    bu_lockstuff = usinit(bu_lockfile);
    if (bu_lockstuff == 0) {
	perror("usinit");
	fprintf(stderr, "_bu_semaphore_sgi_init: usinit(%s) failed, unable to allocate lock space\n", bu_lockfile);
	bu_bomb("fatal semaphore initialization failure");
    }
}
#endif

#if defined(convex) || defined(__convex__)
HIDDEN void
_bu_convex_acquire(p)
    register long *p;
{
    asm("getlck:");
    asm("	tas	@0(ap)");	/* try to set the lock */
    asm("	jbra.f	getlck");	/* loop until successful */
}
#endif /* convex */

#if defined(PARALLEL) || defined(DEFINED_BU_SEMAPHORES)
static unsigned int bu_nsemaphores = 0;
static struct bu_semaphores *bu_semaphores = (struct bu_semaphores *)NULL;
#endif


void
bu_semaphore_init(unsigned int nsemaphores)
{
    unsigned int i;

    if (nsemaphores <= 0)
	nsemaphores = i = 1;

#if !defined(PARALLEL) && !defined(DEFINED_BU_SEMAPHORES)
    return;					/* No support on this hardware */
#else

    if (bu_nsemaphores != 0)  return;	/* Already called */
    bu_semaphores = (struct bu_semaphores *)calloc(nsemaphores, sizeof(struct bu_semaphores));
    if (UNLIKELY(!bu_semaphores)) {
	fprintf(stderr, "bu_semaphore_init(): could not allocate space for %d semaphores of len %ld\n",
		nsemaphores, (long)sizeof(struct bu_semaphores));
	exit(2); /* cannot call bu_exit() here */
    }

    /*
     * Begin vendor-specific initialization sections.
     */

#	if defined(alliant)
    for (i=0; i < nsemaphores; i++) {
	bu_semaphores[i].magic = BU_SEMAPHORE_MAGIC;
	(void) initialize_lock(&bu_semaphores[i].c);
    }
#	endif

#	ifdef ardent
    for (i=0; i < nsemaphores; i++) {
	bu_semaphores[i].magic = BU_SEMAPHORE_MAGIC;
	bu_semaphores[i].sem = 1;	/* mark as released */
    }
#	endif

#	if defined(convex) || defined(__convex__)
    for (i=0; i < nsemaphores; i++) {
	bu_semaphores[i].magic = BU_SEMAPHORE_MAGIC;
	bu_semaphores[i].sem = 0;	/* mark as released */
    }
#	endif

#	ifdef CRAY
    for (i=0; i < nsemaphores; i++) {
	bu_semaphores[i].magic = BU_SEMAPHORE_MAGIC;
	LOCKASGN(&bu_semaphores[i].p);
    }
#	endif /* CRAY */

#	if defined(n16)
    /*
     * Encore MultiMax.
     * While the manual suggests that one should use spin_create()
     * to aquire a new control structure for spin locking, it turns
     * out that the library support for that simply malloc()s a 1-byte
     * area to contain the lock, and sets it to PAR_UNLOCKED.
     */
    for (i=0; i < nsemaphores; i++) {
	bu_semaphores[i].magic = BU_SEMAPHORE_MAGIC;
	bu_semaphores[i].sem = PAR_UNLOCKED;
    }
#	endif

#	ifdef SGI_4D
    _bu_semaphore_sgi_init();
    for (i=0; i < nsemaphores; i++) {
	bu_semaphores[i].magic = BU_SEMAPHORE_MAGIC;
	if ((bu_semaphores[i].ltp = usnewlock(bu_lockstuff)) == NULL) {
	    perror("usnewlock");
	    fprintf(stderr, "bu_semaphore_init: usnewlock() failed, unable to allocate lock [%d]\n", i);
	    bu_bomb("fatal semaphore initialization failure");
	}
    }
#	endif

#	ifdef SUNOS
    for (i=0; i < nsemaphores; i++) {
	bu_semaphores[i].magic = BU_SEMAPHORE_MAGIC;
	if (mutex_init(&bu_semaphores[i].mu, USYNC_THREAD, NULL)) {
	    fprintf(stderr, "bu_semaphore_init(): mutex_init() failed on [%d]\n", i);
	    bu_bomb("fatal semaphore acquisition failure");
	}

    }
#	endif
#	if defined(HAVE_PTHREAD_H) && !defined(sgi)
    for (i=0; i < nsemaphores; i++) {
	bu_semaphores[i].magic = BU_SEMAPHORE_MAGIC;
	if (pthread_mutex_init(&bu_semaphores[i].mu,  NULL)) {
	    fprintf(stderr, "bu_semaphore_init(): pthread_mutex_init() failed on [%d]\n", i);
	    bu_bomb("fatal semaphore acquisition failure");
	}
    }
#	endif

    /*
     * This should be last thing done before returning, so that
     * any subroutines called (e.g. bu_calloc()) won't think that
     * parallel operation has begun yet, and do acquire/release.
     */
    bu_nsemaphores = nsemaphores;
#endif	/* PARALLEL */
}


void
bu_semaphore_reinit(unsigned int nsemaphores)
{
    if (nsemaphores <= 0)
	nsemaphores = 1;

#if !defined(PARALLEL) && !defined(DEFINED_BU_SEMAPHORES)
    return;					/* No support on this hardware */
#else

    if (bu_nsemaphores != 0) {
	free((void *)bu_semaphores);
	bu_semaphores = (struct bu_semaphores *)0;
	bu_nsemaphores = 0;
    }

    bu_semaphore_init(nsemaphores);
#endif	/* PARALLEL */
}


void
bu_semaphore_acquire(unsigned int i)
{
#if !defined(PARALLEL) && !defined(DEFINED_BU_SEMAPHORES)
    i = i; /* quellage */
    return;					/* No support on this hardware */
#else
    if (bu_semaphores == NULL) {
	/* Semaphores not initialized yet.  Must be non-parallel */
	return;
    }

    BU_CKMAG(bu_semaphores, BU_SEMAPHORE_MAGIC, "bu_semaphore");

    if (i >= bu_nsemaphores) {
	fprintf(stderr, "bu_semaphore_acquire(%d): semaphore # exceeds max of %d\n",
		i, bu_nsemaphores - 1);
	bu_bomb("fatal semaphore acquisition failure");
    }

    BU_CKMAG(&bu_semaphores[i], BU_SEMAPHORE_MAGIC, "bu_semaphore");

    /*
     * Begin vendor-specific initialization sections.
     */

#	if defined(alliant)
    (void) lock(&bu_semaphores[i].c);
#	endif

#	ifdef ardent
    {
	register long *p = &bu_semaphores[i].sem;
	while (SYNCH_Adr = p, !SYNCH_Val)  while (!*p);
    }
#	endif

#	if defined(convex) || defined(__convex__)
    _bu_convex_acquire(&bu_semaphores[i].sem);
#	endif

#	ifdef CRAY
    LOCKON(&bu_semaphores[i].p);
#	endif /* CRAY */

#	if defined(n16)
    (void)spin_lock((LOCK *)&bu_semaphores[i].sem);
#	endif

#	ifdef SGI_4D
    uswsetlock(bu_semaphores[i].ltp, 1000);
#	endif

#	ifdef SUNOS
    if (mutex_lock(&bu_semaphores[i].mu)) {
	fprintf(stderr, "bu_semaphore_acquire(): mutex_lock() failed on [%d]\n", i);
	bu_bomb("fatal semaphore acquisition failure");
    }
#	endif
#	if defined(HAVE_PTHREAD_H) && !defined(sgi)
    if (pthread_mutex_lock(&bu_semaphores[i].mu)) {
	fprintf(stderr, "bu_semaphore_acquire(): pthread_mutex_lock() failed on [%d]\n", i);
	bu_bomb("fatal semaphore acquisition failure");
    }
#	endif

#endif
}


void
bu_semaphore_release(unsigned int i)
{
#if !defined(PARALLEL) && !defined(DEFINED_BU_SEMAPHORES)
    i = i; /* quellage */
    return;					/* No support on this hardware */
#else
    if (bu_semaphores == NULL) {
	/* Semaphores not initialized yet.  Must be non-parallel */
	return;
    }

    BU_CKMAG(bu_semaphores, BU_SEMAPHORE_MAGIC, "bu_semaphore");

    if (i >= bu_nsemaphores) {
	fprintf(stderr, "bu_semaphore_release(%d): semaphore # exceeds max of %d\n",
		i, bu_nsemaphores - 1);
	exit(3); /* cannot call bu_exit() here */
    }

    BU_CKMAG(&bu_semaphores[i], BU_SEMAPHORE_MAGIC, "bu_semaphore");

    /*
     * Begin vendor-specific initialization sections.
     */

#	if defined(alliant)
    (void) unlock(&bu_semaphores[i].c);
#	endif

#	ifdef ardent
    bu_semaphores[i].sem = 1;	/* release */
#	endif

#	if defined(convex) || defined(__convex__)
    bu_semaphores[i].sem = 0;	/* release */
#	endif

#	ifdef CRAY
    LOCKOFF(&bu_semaphores[i].p);
#	endif /* CRAY */

#	if defined(n16)
    (void)spin_unlock((LOCK *)&bu_semaphores[i].sem);
#	endif

#	ifdef SGI_4D
    usunsetlock(bu_semaphores[i].ltp);
#	endif

#	ifdef SUNOS
    if (mutex_unlock(&bu_semaphores[i].mu)) {
	fprintf(stderr, "bu_semaphore_acquire(): mutex_unlock() failed on [%d]\n", i);
	bu_bomb("fatal semaphore acquisition failure");
    }
#	endif
#	if defined(HAVE_PTHREAD_H) && !defined (sgi)
    if (pthread_mutex_unlock(&bu_semaphores[i].mu)) {
	fprintf(stderr, "bu_semaphore_acquire(): pthread_mutex_unlock() failed on [%d]\n", i);
	bu_bomb("fatal semaphore acquisition failure");
    }
#	endif
#endif
}

/* XXX need a routine to pair up with _init() to delete the semaphore structures */
#if 0
void
bu_semaphore_free() {
    if (bu_semaphores) {
	free(bu_semaphores);
	bu_semaphores = (struct bu_semaphores *)NULL;
    }
}
#endif


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
