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

#ifndef _RT_TCM_H
#define _RT_TCM_H

#include "common.h"

#if defined(HAVE_PTHREAD_H)
#  include <pthread.h>
#endif
#if defined(HAVE_WINDOWS_H)
  #include <process.h>
  #include <sys/timeb.h>
#endif

#define thrd_error    0 /**< The requested operation failed */
#define thrd_success  1 /**< The requested operation succeeded */
#define thrd_timedout 2 /**< The time specified in the call was reached without acquiring the requested resource */
#define thrd_busy     3 /**< The requested operation failed because a tesource requested by a test and return function is already in use */
#define thrd_nomem    4 /**< The requested operation failed because it was unable to allocate memory */

typedef int (*rt_thrd_start_t)(void *arg);
#if defined(HAVE_WINDOWS_H)
typedef HANDLE rt_thrd_t;
#else
typedef pthread_t rt_thrd_t;
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
} rt_mtx_t;
#else
typedef pthread_mutex_t rt_mtx_t;
#endif

/* Condition variable */
#if defined(HAVE_WINDOWS_H)
typedef struct {
  HANDLE mEvents[2];                  /* Signal and broadcast event HANDLEs. */
  unsigned int mWaitersCount;         /* Count of the number of waiters. */
  CRITICAL_SECTION mWaitersCountLock; /* Serialize access to mWaitersCount. */
} rt_cnd_t;
#else
typedef pthread_cond_t rt_cnd_t;
#endif

extern int rt_thrd_create(rt_thrd_t *thr, rt_thrd_start_t func, void *arg);
extern int rt_thrd_join(rt_thrd_t thr, int *res);
extern int rt_mtx_init(rt_mtx_t *mtx);
extern int rt_mtx_lock(rt_mtx_t *mtx);
extern int rt_mtx_unlock(rt_mtx_t *mtx);
extern void rt_mtx_destroy(rt_mtx_t *mtx);
extern int rt_cnd_init(rt_cnd_t *cond);
extern void rt_cnd_destroy(rt_cnd_t *cond);
extern int rt_cnd_wait(rt_cnd_t *cond, rt_mtx_t *mtx);
extern int rt_cnd_broadcast(rt_cnd_t *cond);

#endif /* _RT_TCM_H */

/************************************************************************/
/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
