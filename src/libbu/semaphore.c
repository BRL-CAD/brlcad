/*                     S E M A P H O R E . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2021 United States Government as represented by
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
#include <errno.h>

#include "bio.h"
#include "bu/malloc.h"
#include "bu/parallel.h"
#include "bu/exit.h"

static void
sem_bomb(int eno) {
    switch (eno) {
	case EINVAL:
	    bu_bomb("fatal semaphore acquisition failure EINVAL");
	    break;
	case EBUSY:
	    bu_bomb("fatal semaphore acquisition failure EBUSY");
	    break;
	case EAGAIN:
	    bu_bomb("fatal semaphore acquisition failure EAGAIN");
	    break;
	case EDEADLK:
	    bu_bomb("fatal semaphore acquisition failure EDEADLK");
	    break;
	case EPERM:
	    bu_bomb("fatal semaphore acquisition failure EPERM");
	    break;
	case EOWNERDEAD:
	    bu_bomb("fatal semaphore acquisition failure EOWNERDEAD");
	    break;
	default:
	    bu_bomb("fatal semaphore acquisition failure");
    }
}


/*
 * multithreading support for SunOS 5.X / Solaris 2.x
 */
#if defined(SUNOS) && SUNOS >= 52
#  include <sys/unistd.h>
#  include <thread.h>
#  include <synch.h>
#  define SEMAPHORE_INIT DEFAULTMUTEX
struct bu_semaphores {
    uint32_t magic;
    mutex_t mu;
};

static mutext_t bu_init_lock = SEMAPHORE_INIT;
#  define DEFINED_BU_SEMAPHORES 1

/*
 * multithread support built on POSIX Threads (pthread) library.
 */
#elif defined(HAVE_PTHREAD_H)
#  include <pthread.h>
#  define SEMAPHORE_INIT PTHREAD_MUTEX_INITIALIZER
struct bu_semaphores {
    uint32_t magic;
    pthread_mutex_t mu;
};

static pthread_mutex_t bu_init_lock = SEMAPHORE_INIT;
#  define DEFINED_BU_SEMAPHORES 1

/*
 * multithread support based on the Windows kernel.
 */
#elif defined(_WIN32) && !defined(__CYGWIN__)
#  define SEMAPHORE_INIT {0}
struct bu_semaphores {
    uint32_t magic;
    CRITICAL_SECTION mu;
};

static LONG bu_init_lock = 0;
#  define DEFINED_BU_SEMAPHORES 1
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

#  if defined(SUNOS)
    if (mutex_lock(&bu_init_lock)) {
	fprintf(stderr, "bu_semaphore_acquire(): mutex_lock() failed on init lock\n");
	bu_bomb("fatal semaphore acquisition failure");
    }
    for (i=bu_nsemaphores; i < nsemaphores; i++) {
	memset(&bu_semaphores[i], 0, sizeof(struct bu_semaphores));
	bu_semaphores[i].magic = SEMAPHORE_MAGIC;
	if (mutex_init(&bu_semaphores[i].mu, USYNC_THREAD, NULL)) {
	    fprintf(stderr, "bu_semaphore_init(): mutex_init() failed on [%d] of [%d]\n", i+1, nsemaphores - bu_nsemaphores);
	    bu_bomb("fatal semaphore acquisition failure");
	}
    }
    bu_nsemaphores = nsemaphores;
    if (mutex_unlock(&bu_init_lock)) {
	fprintf(stderr, "bu_semaphore_acquire(): mutex_unlock() failed on init lock\n");
	bu_bomb("fatal semaphore acquisition failure");
    }

#  elif defined(HAVE_PTHREAD_H)
    int ret = pthread_mutex_lock(&bu_init_lock);
    if (ret) {
	fprintf(stderr, "bu_semaphore_acquire(): pthread_mutex_lock() failed on init lock\n");
	sem_bomb(ret);
    }
    for (i=bu_nsemaphores; i < nsemaphores; i++) {
	memset(&bu_semaphores[i], 0, sizeof(struct bu_semaphores));
	bu_semaphores[i].magic = SEMAPHORE_MAGIC;
	ret = pthread_mutex_init(&bu_semaphores[i].mu,  NULL);
	if (ret) {
	    fprintf(stderr, "bu_semaphore_init(): pthread_mutex_init() failed on [%d] of [%d]\n", i+1, nsemaphores - bu_nsemaphores);
	    sem_bomb(ret);
	}
    }
    bu_nsemaphores = nsemaphores;
    ret = pthread_mutex_unlock(&bu_init_lock);
    if (ret) {
	fprintf(stderr, "bu_semaphore_acquire(): pthread_mutex_unlock() failed on init lock\n");
	sem_bomb(ret);
    }

#  elif defined(_WIN32) && !defined(__CYGWIN__)
    while (InterlockedCompareExchange(&bu_init_lock, 1, 0)) {
	/* someone else has the lock, spin-wait */
    }
    /* lock acquired */
    for (i=bu_nsemaphores; i < nsemaphores; i++) {
	memset(&bu_semaphores[i], 0, sizeof(struct bu_semaphores));
	bu_semaphores[i].magic = SEMAPHORE_MAGIC;
	InitializeCriticalSection(&bu_semaphores[i].mu);
    }
    bu_nsemaphores = nsemaphores;
    /* release lock */
    InterlockedExchange(&bu_init_lock, 0);
#  endif
#endif	/* PARALLEL */
}


void
bu_semaphore_free(void)
{
    unsigned int i;
    extern void semaphore_clear(void);
    semaphore_clear();

#if !defined(PARALLEL) && !defined(DEFINED_BU_SEMAPHORES)
    return;					/* No support on this hardware */
#else

    /* Close out the mutexes already created. */
#  if defined(SUNOS)
    for (i = 0; i < bu_nsemaphores; i++) {
	if (mutex_destroy(&bu_semaphores[i].mu)) {
	    fprintf(stderr, "bu_semaphore_free(): mutex_destroy() failed on [%d] of [%d]\n", i+1, bu_nsemaphores);
	}
    }

#  elif defined(HAVE_PTHREAD_H)
    for (i = 0; i < bu_nsemaphores; i++) {
	if (pthread_mutex_destroy(&bu_semaphores[i].mu)) {
	    fprintf(stderr, "bu_semaphore_free(): pthread_mutex_destroy() failed on [%d] of [%d]\n", i+1, bu_nsemaphores);
	}
    }

#  elif defined(_WIN32) && !defined(__CYGWIN__)
    for (i = 0; i < bu_nsemaphores; i++) {
	DeleteCriticalSection(&bu_semaphores[i].mu);
    }
#  endif

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

#  ifdef SUNOS
    if (mutex_lock(&bu_semaphores[i].mu)) {
	fprintf(stderr, "bu_semaphore_acquire(): mutex_lock() failed on [%d]\n", i);
	bu_bomb("fatal semaphore acquisition failure");
    }
#  endif

#  if defined(HAVE_PTHREAD_H)
    int ret = pthread_mutex_lock(&bu_semaphores[i].mu);
    if (ret) {
	fprintf(stderr, "bu_semaphore_acquire(): pthread_mutex_lock() failed on [%d]\n", i);
	sem_bomb(ret);
    }
#  endif

#  if defined(_WIN32) && !defined(__CYGWIN__)
    /* This only fails if the timeout exceeds 30 days. */
    EnterCriticalSection(&bu_semaphores[i].mu);
#  endif

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

#  ifdef SUNOS
    if (mutex_unlock(&bu_semaphores[i].mu)) {
	fprintf(stderr, "bu_semaphore_acquire(): mutex_unlock() failed on [%d]\n", i);
	bu_bomb("fatal semaphore acquisition failure");
    }
#  endif

#  if defined(HAVE_PTHREAD_H)
    int ret = pthread_mutex_unlock(&bu_semaphores[i].mu);
    if (ret) {
	fprintf(stderr, "bu_semaphore_acquire(): pthread_mutex_unlock() failed on [%d]\n", i);
	sem_bomb(ret);
    }
#  endif

#  if defined(_WIN32) && !defined(__CYGWIN__)
    LeaveCriticalSection(&bu_semaphores[i].mu);
#  endif

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
