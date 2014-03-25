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
#include <stdio.h>
#include <ctype.h>
#include <math.h>

#include "bio.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/parallel.h"


#if defined(_WIN32) && !defined(__CYGWIN__)
struct bu_semaphores {
    uint32_t magic;
    HANDLE m;
};
# define DEFINED_BU_SEMAPHORES 1
#endif

/*
 * multithreading support for SunOS 5.X / Solaris 2.x
 */
#if defined(SUNOS) && SUNOS >= 52
#	include <sys/unistd.h>
#	include <thread.h>
#	include <synch.h>
struct bu_semaphores {
    uint32_t magic;
    mutex_t mu;
};
# define DEFINED_BU_SEMAPHORES 1
#endif	/* SUNOS */

/*
 * multithread support built on POSIX Threads (pthread) library.
 */
#ifdef HAVE_PTHREAD_H
#	include <pthread.h>
struct bu_semaphores {
    uint32_t magic;
    pthread_mutex_t mu;
};
#	define DEFINED_BU_SEMAPHORES 1
#endif


#define BU_SEMAPHORE_MAGIC 0x62757365


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

#	ifdef SUNOS
    for (i=0; i < nsemaphores; i++) {
	bu_semaphores[i].magic = BU_SEMAPHORE_MAGIC;
	if (mutex_init(&bu_semaphores[i].mu, USYNC_THREAD, NULL)) {
	    fprintf(stderr, "bu_semaphore_init(): mutex_init() failed on [%d]\n", i);
	    bu_bomb("fatal semaphore acquisition failure");
	}

    }
#	endif
#	if defined(HAVE_PTHREAD_H)
    for (i=0; i < nsemaphores; i++) {
	bu_semaphores[i].magic = BU_SEMAPHORE_MAGIC;
	if (pthread_mutex_init(&bu_semaphores[i].mu,  NULL)) {
	    fprintf(stderr, "bu_semaphore_init(): pthread_mutex_init() failed on [%d]\n", i);
	    bu_bomb("fatal semaphore acquisition failure");
	}
    }
#	endif

#	if defined(_WIN32) && !defined(__CYGWIN__)
    for (i=0; i < nsemaphores; i++) {
	bu_semaphores[i].magic = BU_SEMAPHORE_MAGIC;
	bu_semaphores[i].m = CreateMutex(NULL, FALSE, NULL);
	if (bu_semaphores[i].m == NULL) {
	    fprintf(stderr, "bu_semaphore_init(): CreateMutex() failed on [%d]\n", i);
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
bu_semaphore_free() {
    if (bu_semaphores) {
	free(bu_semaphores);
	bu_semaphores = (struct bu_semaphores *)NULL;
	bu_nsemaphores = 0;
    }
}


void
bu_semaphore_reinit(unsigned int nsemaphores)
{
    if (nsemaphores <= 0)
	nsemaphores = 1;

#if !defined(PARALLEL) && !defined(DEFINED_BU_SEMAPHORES)
    return;					/* No support on this hardware */
#else

    bu_semaphore_free();
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
    if (WaitForSingleObject(bu_semaphores[i].m, INFINITE) == WAIT_FAILED) {
	fprintf(stderr, "bu_semaphore_acquire(): WaitForSingleObject() failed on [%d]\n", i);
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
    if (!ReleaseMutex(bu_semaphores[i].m)) {
	fprintf(stderr, "bu_semaphore_acquire(): ReleaseMutex() failed on [%d]\n", i);
	bu_bomb("fatal semaphore acquisition failure");
    }
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
