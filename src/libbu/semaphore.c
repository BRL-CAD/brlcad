/*                     S E M A P H O R E . C
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
#include <string.h>
#include <ctype.h>
#include <math.h>

#include "bio.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/parallel.h"

/*
 * multithreading support for SunOS 5.X / Solaris 2.x
 */
#if defined(SUNOS) && SUNOS >= 52
#	include <sys/unistd.h>
#	include <thread.h>
#	include <synch.h>
#	define SEMAPHORE_INIT DEFAULEMUTEX
struct bu_semaphores {
    uint32_t magic;
    mutex_t mu;
};
#	define DEFINED_BU_SEMAPHORES 1

/*
 * multithread support built on POSIX Threads (pthread) library.
 */
#elif defined(HAVE_PTHREAD_H)
#	include <pthread.h>
#	define SEMAPHORE_INIT PTHREAD_MUTEX_INITIALIZER
struct bu_semaphores {
    uint32_t magic;
    pthread_mutex_t mu;
};
#	define DEFINED_BU_SEMAPHORES 1

/*
 * multithread support based on the Windows kernel.
 */
#elif defined(_WIN32) && !defined(__CYGWIN__)
#	define SEMAPHORE_INIT {0}
struct bu_semaphores {
    uint32_t magic;
    CRITICAL_SECTION mu;
};
#	define DEFINED_BU_SEMAPHORES 1
#endif

#define SEMAPHORE_MAGIC 0x62757365
#define SEMAPHORE_MAX 1024


#if defined(PARALLEL) || defined(DEFINED_BU_SEMAPHORES)
static unsigned int bu_nsemaphores = 0;
static struct bu_semaphores bu_semaphores[SEMAPHORE_MAX] = {{0, SEMAPHORE_INIT}};
#endif


void
bu_semaphore_init(unsigned int nsemaphores)
{
#if !defined(PARALLEL) && !defined(DEFINED_BU_SEMAPHORES)
    if (nsemaphores) /* quellage */
	return;
    return; /* No support on this hardware */
#else

    unsigned int i;

    if (nsemaphores < 1)
	nsemaphores = 1;
    else if (nsemaphores <= bu_nsemaphores)
	return;	/* Already initialized */
    else if (UNLIKELY(nsemaphores > SEMAPHORE_MAX)) {
	fprintf(stderr, "bu_semaphore_init(): could not initialize %d semaphores, max is %d\n",
		nsemaphores, SEMAPHORE_MAX);
	exit(2); /* cannot call bu_exit() here */
    }

    /*
     * Begin vendor-specific initialization sections.
     */

#	if defined(SUNOS)
    for (i=bu_nsemaphores; i < nsemaphores; i++) {
	bu_semaphores[i].magic = SEMAPHORE_MAGIC;
	memset(&bu_semaphores[i].mu, 0, sizeof(struct bu_semaphores));
	if (mutex_init(&bu_semaphores[i].mu, USYNC_THREAD, NULL)) {
	    fprintf(stderr, "bu_semaphore_init(): mutex_init() failed on [%d] of [%d]\n", i+1, nsemaphores - bu_nsemaphores);
	    bu_bomb("fatal semaphore acquisition failure");
	}
    }
#	elif defined(HAVE_PTHREAD_H)
    for (i=bu_nsemaphores; i < nsemaphores; i++) {
	bu_semaphores[i].magic = SEMAPHORE_MAGIC;
	memset(&bu_semaphores[i].mu, 0, sizeof(struct bu_semaphores));
	if (pthread_mutex_init(&bu_semaphores[i].mu,  NULL)) {
	    fprintf(stderr, "bu_semaphore_init(): pthread_mutex_init() failed on [%d] of [%d]\n", i+1, nsemaphores - bu_nsemaphores);
	    bu_bomb("fatal semaphore acquisition failure");
	}
    }
#	elif defined(_WIN32) && !defined(__CYGWIN__)
    for (i=bu_nsemaphores; i < nsemaphores; i++) {
	bu_semaphores[i].magic = SEMAPHORE_MAGIC;
	memset(&bu_semaphores[i].mu, 0, sizeof(struct bu_semaphores));
	/* This cannot fail except for very low memory situations in XP. */
	InitializeCriticalSection(&bu_semaphores[i].mu);
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
bu_semaphore_free(void)
{

#if !defined(PARALLEL) && !defined(DEFINED_BU_SEMAPHORES)
    return;					/* No support on this hardware */
#else

    unsigned int i;

    /* Close out the mutexes already created. */
#	if defined(SUNOS)
    for (i = 0; i < bu_nsemaphores; i++) {
	if (mutex_destroy(&bu_semaphores[i].mu)) {
	    fprintf(stderr, "bu_semaphore_free(): mutex_destroy() failed on [%d] of [%d]\n", i+1, bu_nsemaphores);
	}
    }

#	elif defined(HAVE_PTHREAD_H)
    for (i = 0; i < bu_nsemaphores; i++) {
	if (pthread_mutex_destroy(&bu_semaphores[i].mu)) {
	    fprintf(stderr, "bu_semaphore_free(): pthread_mutex_destroy() failed on [%d] of [%d]\n", i+1, bu_nsemaphores);
	}
    }

#	elif defined(_WIN32) && !defined(__CYGWIN__)
    for (i = 0; i < bu_nsemaphores; i++) {
	DeleteCriticalSection(&bu_semaphores[i].mu);
    }
#	endif

    bu_nsemaphores = 0;
#endif	/* PARALLEL */
}


void
bu_semaphore_acquire(unsigned int i)
{
#if !defined(PARALLEL) && !defined(DEFINED_BU_SEMAPHORES)
    if (i) /* quellage */
	return;
    return;					/* No support on this hardware */
#else

    /* ensure we have this semaphore */
    bu_semaphore_init(i+1);
    BU_CKMAG(&bu_semaphores[i], SEMAPHORE_MAGIC, "bu_semaphore");

    /*
     * Begin vendor-specific initialization sections.
     */

#	ifdef SUNOS
    if (mutex_lock(&bu_semaphores[i].mu)) {
	fprintf(stderr, "bu_semaphore_acquire(): mutex_lock() failed on [%d]\n", i);
	bu_bomb("fatal semaphore acquisition failure");
    }
#	endif
#	if defined(HAVE_PTHREAD_H)
    if (pthread_mutex_lock(&bu_semaphores[i].mu)) {
	fprintf(stderr, "bu_semaphore_acquire(): pthread_mutex_lock() failed on [%d]\n", i);
	bu_bomb("fatal semaphore acquisition failure");
    }
#	endif
#	if defined(_WIN32) && !defined(__CYGWIN__)
    /* This only fails if the timeout exceeds 30 days. */
    EnterCriticalSection(&bu_semaphores[i].mu);
#	endif

#endif
}


void
bu_semaphore_release(unsigned int i)
{
#if !defined(PARALLEL) && !defined(DEFINED_BU_SEMAPHORES)
    if (i) /* quellage */
	return;
    return;					/* No support on this hardware */
#else

    /* ensure we have this semaphore */
    bu_semaphore_init(i+1);
    BU_CKMAG(&bu_semaphores[i], SEMAPHORE_MAGIC, "bu_semaphore");

    /*
     * Begin vendor-specific initialization sections.
     */

#	ifdef SUNOS
    if (mutex_unlock(&bu_semaphores[i].mu)) {
	fprintf(stderr, "bu_semaphore_acquire(): mutex_unlock() failed on [%d]\n", i);
	bu_bomb("fatal semaphore acquisition failure");
    }
#	endif
#	if defined(HAVE_PTHREAD_H)
    if (pthread_mutex_unlock(&bu_semaphores[i].mu)) {
	fprintf(stderr, "bu_semaphore_acquire(): pthread_mutex_unlock() failed on [%d]\n", i);
	bu_bomb("fatal semaphore acquisition failure");
    }
#	endif

#	if defined(_WIN32) && !defined(__CYGWIN__)
    LeaveCriticalSection(&bu_semaphores[i].mu);
#	endif
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
