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

/* Note - this is a temporary measure for portable compilation of some BRL-CAD
 * components, and will likely be replaced by standards and/or other API at
 * some future date.
 *
 * This header should be considered as INTERNAL ONLY - not public API. */

#ifndef _BU_TCM_H
#define _BU_TCM_H

#include "common.h"

#if !defined(BRLCADBUILD)
#  error "Warning: included bu/tc.h (compile-time API) without BRLCADBUILD defined"
#endif
#if !defined(HAVE_CONFIG_H)
#  error "Warning: included bu/tc.h (compile-time API) without HAVE_CONFIG_H defined"
#endif

#include "bu/defines.h"

#include "bio.h" /* For windows.h */

__BEGIN_DECLS

#if defined(HAVE_PTHREAD_H)
#  include <pthread.h>
#endif
#if defined(HAVE_WINDOWS_H)
#  include <process.h>
#  include <sys/timeb.h>
#endif

#define bu_thrd_error    0 /**< The requested operation failed */
#define bu_thrd_success  1 /**< The requested operation succeeded */
#define bu_thrd_timedout 2 /**< The time specified in the call was reached without acquiring the requested resource */
#define bu_thrd_busy     3 /**< The requested operation failed because a tesource requested by a test and return function is already in use */
#define bu_thrd_nomem    4 /**< The requested operation failed because it was unable to allocate memory */

typedef int (*bu_thrd_start_t)(void *arg);
#if defined(HAVE_WINDOWS_H)
typedef HANDLE bu_thrd_t;
#else
typedef pthread_t bu_thrd_t;
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
} bu_mtx_t;
#else
typedef pthread_mutex_t bu_mtx_t;
#endif

/* Condition variable */
#if defined(HAVE_WINDOWS_H)
typedef struct {
    HANDLE mEvents[2];                  /* Signal and broadcast event HANDLEs. */
    unsigned int mWaitersCount;         /* Count of the number of waiters. */
    CRITICAL_SECTION mWaitersCountLock; /* Serialize access to mWaitersCount. */
} bu_cnd_t;
#else
typedef pthread_cond_t bu_cnd_t;
#endif

BU_EXPORT extern int bu_thrd_create(bu_thrd_t *thr, bu_thrd_start_t func, void *arg);
BU_EXPORT extern int bu_thrd_join(bu_thrd_t thr, int *res);
BU_EXPORT extern int bu_mtx_init(bu_mtx_t *mtx);
BU_EXPORT extern int bu_mtx_lock(bu_mtx_t *mtx);
BU_EXPORT extern int bu_mtx_unlock(bu_mtx_t *mtx);
BU_EXPORT extern void bu_mtx_destroy(bu_mtx_t *mtx);
BU_EXPORT extern int bu_cnd_init(bu_cnd_t *cond);
BU_EXPORT extern void bu_cnd_destroy(bu_cnd_t *cond);
BU_EXPORT extern int bu_cnd_wait(bu_cnd_t *cond, bu_mtx_t *mtx);
BU_EXPORT extern int bu_cnd_broadcast(bu_cnd_t *cond);
BU_EXPORT extern int bu_cnd_signal(bu_cnd_t *cond);
BU_EXPORT extern int bu_mtx_trylock(bu_mtx_t *mtx);


__END_DECLS

#endif /* _BU_TCM_H */

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
