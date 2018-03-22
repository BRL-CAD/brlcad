/*                        T I E N E T . H
 * BRL-CAD
 *
 * Copyright (c) 2008-2018 United States Government as represented by
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

#ifndef ADRT_TIENET_H
#define ADRT_TIENET_H

#include "common.h"

#include "bu/malloc.h"

#define	TN_MASTER_PORT		1980
#define	TN_SLAVE_PORT		1981

#define	TN_OP_PREP		0x0010
#define	TN_OP_REQWORK		0x0011
#define	TN_OP_SENDWORK		0x0012
#define	TN_OP_RESULT		0x0013
#define	TN_OP_COMPLETE		0x0014
#define	TN_OP_SHUTDOWN		0x0015
#define TN_OP_OKAY		0x0016
#define	TN_OP_MESSAGE		0x0017

#if defined(HAVE_PTHREAD_H)
#  include <pthread.h>
#endif
#if defined(HAVE_WINDOWS_H)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#    define __UNDEF_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#  ifdef __UNDEF_LEAN_AND_MEAN
#    undef WIN32_LEAN_AND_MEAN
#    undef __UNDEF_LEAN_AND_MEAN
#  endif
#  include <process.h>
#  include <sys/timeb.h>
#endif

/************************************************************************/
/* Bits from tinycthread for portable threads
 *
 * Copyright (c) 2012 Marcus Geelnard
 * Copyright (c) 2013-2016 Evan Nemerson
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:

 *  1. The origin of this software must not be misrepresented; you must not
 *  claim that you wrote the original software. If you use this software
 *  in a product, an acknowledgment in the product documentation would be
 *  appreciated but is not required.
 *
 *  2. Altered source versions must be plainly marked as such, and must not be
 *  misrepresented as being the original software.
 *
 *  3. This notice may not be removed or altered from any source
 *  distribution.
 */

#define thrd_error    0 /**< The requested operation failed */
#define thrd_success  1 /**< The requested operation succeeded */
#define thrd_timedout 2 /**< The time specified in the call was reached without acquiring the requested resource */
#define thrd_busy     3 /**< The requested operation failed because a tesource requested by a test and return function is already in use */
#define thrd_nomem    4 /**< The requested operation failed because it was unable to allocate memory */

typedef int (*thrd_start_t)(void *arg);
#if defined(HAVE_WINDOWS_H)
typedef HANDLE thrd_t;
#else
typedef pthread_t thrd_t;
#endif

/* Mutex */
#if defined(HAVE_WINDOWS_H)
typedef struct {
    union {
	CRITICAL_SECTION cs;      /* Critical section handle (used for non-timed mutexes) */
	HANDLE mut;               /* Mutex handle (used for timed mutex) */
    } mHandle;                  /* Mutex handle */
    int mAlreadyLocked;         /* TRUE if the mutex is already locked */
    int mRecursive;             /* TRUE if the mutex is recursive */
    int mTimed;                 /* TRUE if the mutex is timed */
} mtx_t;
#else
typedef pthread_mutex_t mtx_t;
#endif

/* Condition variable */
#if defined(HAVE_WINDOWS_H)
typedef struct {
  HANDLE mEvents[2];                  /* Signal and broadcast event HANDLEs. */
  unsigned int mWaitersCount;         /* Count of the number of waiters. */
  CRITICAL_SECTION mWaitersCountLock; /* Serialize access to mWaitersCount. */
} cnd_t;
#else
typedef pthread_cond_t cnd_t;
#endif

extern int thrd_create(thrd_t *thr, thrd_start_t func, void *arg);
extern int thrd_join(thrd_t thr, int *res);
extern int mtx_init(mtx_t *mtx);
extern int mtx_lock(mtx_t *mtx);
extern int mtx_unlock(mtx_t *mtx);

/************************************************************************/


#define TIENET_BUFFER_INIT(_b) { \
	_b.data = NULL; \
	_b.size = 0; \
	_b.ind = 0; }

#define TIENET_BUFFER_FREE(_b) bu_free(_b.data, "tienet buffer");

#define TIENET_BUFFER_SIZE(_b, _s) { \
	if ((size_t)_s > (size_t)_b.size) { \
	    _b.data = (uint8_t *)bu_realloc(_b.data, (size_t)_s, "tienet buffer size");	\
	    _b.size = (uint32_t)_s; \
	} }

typedef struct tienet_buffer_s {
    uint8_t *data;
    uint32_t size;
    uint32_t ind;
} tienet_buffer_t;


typedef struct tienet_sem_s {
    int val;
    mtx_t mut;
    cnd_t cond;
} tienet_sem_t;


int tienet_send(int socket, void* data, size_t size);
int tienet_recv(int socket, void* data, size_t size);

void tienet_sem_init(tienet_sem_t *sem, int val);
void tienet_sem_free(tienet_sem_t *sem);
void tienet_sem_post(tienet_sem_t *sem);
void tienet_sem_wait(tienet_sem_t *sem);

#endif

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
