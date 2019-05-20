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

#include "common.h"
#include <stdlib.h>
#include "bu/tc.h"


int
bu_mtx_init(bu_mtx_t *mtx)
{
#if defined(HAVE_WINDOWS_H)
    mtx->mAlreadyLocked = FALSE;
    mtx->mRecursive = 0;
    mtx->mTimed = 0;
    InitializeCriticalSection(&(mtx->mHandle.cs));
    return bu_thrd_success;
#else
    int ret;
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    ret = pthread_mutex_init(mtx, &attr);
    pthread_mutexattr_destroy(&attr);
    return ret == 0 ? bu_thrd_success : bu_thrd_error;
#endif
}


void
bu_mtx_destroy(bu_mtx_t *mtx)
{
#if defined(HAVE_WINDOWS_H)
    if (!mtx->mTimed) {
	DeleteCriticalSection(&(mtx->mHandle.cs));
    } else {
	CloseHandle(mtx->mHandle.mut);
    }
#else
    pthread_mutex_destroy(mtx);
#endif
}


#if defined(HAVE_WINDOWS_H)
#define _CONDITION_EVENT_ONE 0
#define _CONDITION_EVENT_ALL 1
#endif


int
bu_cnd_init(bu_cnd_t *cond)
{
#if defined(HAVE_WINDOWS_H)
    cond->mWaitersCount = 0;

    /* Init critical section */
    InitializeCriticalSection(&cond->mWaitersCountLock);

    /* Init events */
    cond->mEvents[_CONDITION_EVENT_ONE] = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (cond->mEvents[_CONDITION_EVENT_ONE] == NULL)
    {
	cond->mEvents[_CONDITION_EVENT_ALL] = NULL;
	return bu_thrd_error;
    }
    cond->mEvents[_CONDITION_EVENT_ALL] = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (cond->mEvents[_CONDITION_EVENT_ALL] == NULL)
    {
	CloseHandle(cond->mEvents[_CONDITION_EVENT_ONE]);
	cond->mEvents[_CONDITION_EVENT_ONE] = NULL;
	return bu_thrd_error;
    }

    return bu_thrd_success;
#else
    return pthread_cond_init(cond, NULL) == 0 ? bu_thrd_success : bu_thrd_error;
#endif
}


void
bu_cnd_destroy(bu_cnd_t *cond)
{
#if defined(HAVE_WINDOWS_H)
    if (cond->mEvents[_CONDITION_EVENT_ONE] != NULL)
    {
	CloseHandle(cond->mEvents[_CONDITION_EVENT_ONE]);
    }
    if (cond->mEvents[_CONDITION_EVENT_ALL] != NULL)
    {
	CloseHandle(cond->mEvents[_CONDITION_EVENT_ALL]);
    }
    DeleteCriticalSection(&cond->mWaitersCountLock);
#else
    pthread_cond_destroy(cond);
#endif
}


int
bu_mtx_lock(bu_mtx_t *mtx)
{
#if defined(HAVE_WINDOWS_H)
    if (!mtx->mTimed)
    {
	EnterCriticalSection(&(mtx->mHandle.cs));
    }
    else
    {
	switch (WaitForSingleObject(mtx->mHandle.mut, INFINITE))
	{
	    case WAIT_OBJECT_0:
		break;
	    case WAIT_ABANDONED:
	    default:
		return bu_thrd_error;
	}
    }

    if (!mtx->mRecursive)
    {
	while(mtx->mAlreadyLocked) Sleep(1); /* Simulate deadlock... */
	mtx->mAlreadyLocked = TRUE;
    }
    return bu_thrd_success;
#else
    return pthread_mutex_lock(mtx) == 0 ? bu_thrd_success : bu_thrd_error;
#endif
}


int
bu_mtx_unlock(bu_mtx_t *mtx)
{
#if defined(HAVE_WINDOWS_H)
    mtx->mAlreadyLocked = FALSE;
    if (!mtx->mTimed)
    {
	LeaveCriticalSection(&(mtx->mHandle.cs));
    }
    else
    {
	if (!ReleaseMutex(mtx->mHandle.mut))
	{
	    return bu_thrd_error;
	}
    }
    return bu_thrd_success;
#else
    return pthread_mutex_unlock(mtx) == 0 ? bu_thrd_success : bu_thrd_error;;
#endif
}


int
bu_mtx_trylock(bu_mtx_t *mtx)
{
#if defined(HAVE_WINDOWS_H)
    int ret;

    if (!mtx->mTimed)
    {
	ret = TryEnterCriticalSection(&(mtx->mHandle.cs)) ? bu_thrd_success : bu_thrd_busy;
    }
    else
    {
	ret = (WaitForSingleObject(mtx->mHandle.mut, 0) == WAIT_OBJECT_0) ? bu_thrd_success : bu_thrd_busy;
    }

    if ((!mtx->mRecursive) && (ret == bu_thrd_success))
    {
	if (mtx->mAlreadyLocked)
	{
	    LeaveCriticalSection(&(mtx->mHandle.cs));
	    ret = bu_thrd_busy;
	}
	else
	{
	    mtx->mAlreadyLocked = TRUE;
	}
    }
    return ret;
#else
    return pthread_mutex_trylock(mtx) == 0 ? bu_thrd_success : bu_thrd_error;
#endif
}


int
bu_cnd_signal(bu_cnd_t *cond)
{
#if defined(HAVE_WINDOWS_H)
    int haveWaiters;

    /* Are there any waiters? */
    EnterCriticalSection(&cond->mWaitersCountLock);
    haveWaiters = (cond->mWaitersCount > 0);
    LeaveCriticalSection(&cond->mWaitersCountLock);

    /* If we have any waiting threads, send them a signal */
    if(haveWaiters)
    {
	if (SetEvent(cond->mEvents[_CONDITION_EVENT_ONE]) == 0)
	{
	    return bu_thrd_error;
	}
    }

    return bu_thrd_success;
#else
    return pthread_cond_signal(cond) == 0 ? bu_thrd_success : bu_thrd_error;
#endif
}

#if defined(HAVE_WINDOWS_H)
static int _bu_cnd_timedwait_win32(bu_cnd_t *cond, bu_mtx_t *mtx, DWORD timeout)
{
    DWORD result;
    int lastWaiter;

    /* Increment number of waiters */
    EnterCriticalSection(&cond->mWaitersCountLock);
    ++ cond->mWaitersCount;
    LeaveCriticalSection(&cond->mWaitersCountLock);

    /* Release the mutex while waiting for the condition (will decrease
       the number of waiters when done)... */
    bu_mtx_unlock(mtx);

    /* Wait for either event to become signaled due to cnd_signal() or
       cnd_broadcast() being called */
    result = WaitForMultipleObjects(2, cond->mEvents, FALSE, timeout);
    if (result == WAIT_TIMEOUT)
    {
	/* The mutex is locked again before the function returns, even if an error occurred */
	bu_mtx_lock(mtx);
	return bu_thrd_timedout;
    }
    else if (result == WAIT_FAILED)
    {
	/* The mutex is locked again before the function returns, even if an error occurred */
	bu_mtx_lock(mtx);
	return bu_thrd_error;
    }

    /* Check if we are the last waiter */
    EnterCriticalSection(&cond->mWaitersCountLock);
    -- cond->mWaitersCount;
    lastWaiter = (result == (WAIT_OBJECT_0 + _CONDITION_EVENT_ALL)) &&
	(cond->mWaitersCount == 0);
    LeaveCriticalSection(&cond->mWaitersCountLock);

    /* If we are the last waiter to be notified to stop waiting, reset the event */
    if (lastWaiter)
    {
	if (ResetEvent(cond->mEvents[_CONDITION_EVENT_ALL]) == 0)
	{
	    /* The mutex is locked again before the function returns, even if an error occurred */
	    bu_mtx_lock(mtx);
	    return bu_thrd_error;
	}
    }

    /* Re-acquire the mutex */
    bu_mtx_lock(mtx);

    return bu_thrd_success;
}
#endif


int
bu_cnd_wait(bu_cnd_t *cond, bu_mtx_t *mtx)
{
#if defined(HAVE_WINDOWS_H)
    return _bu_cnd_timedwait_win32(cond, mtx, INFINITE);
#else
    return pthread_cond_wait(cond, mtx) == 0 ? bu_thrd_success : bu_thrd_error;
#endif
}


#if defined(HAVE_WINDOWS_H)

typedef DWORD tss_t;
typedef void(*tss_dtor_t)(void *val);
#define _Thread_local __declspec(thread)
#define TSS_DTOR_ITERATIONS (4)

struct TinyCThreadTSSData {
    void* value;
    tss_t key;
    struct TinyCThreadTSSData* next;
};

static tss_dtor_t _tinycthread_tss_dtors[1088] = { NULL, };

static _Thread_local struct TinyCThreadTSSData* _tinycthread_tss_head = NULL;
static _Thread_local struct TinyCThreadTSSData* _tinycthread_tss_tail = NULL;


static void
_tinycthread_tss_cleanup(void)
{
    struct TinyCThreadTSSData* data;
    int iteration;
    unsigned int again = 1;
    void* value;

    for (iteration = 0; iteration < TSS_DTOR_ITERATIONS && again > 0; iteration++)
    {
	again = 0;
	for (data = _tinycthread_tss_head; data != NULL; data = data->next)
	{
	    if (data->value != NULL)
	    {
		value = data->value;
		data->value = NULL;

		if (_tinycthread_tss_dtors[data->key] != NULL)
		{
		    again = 1;
		    _tinycthread_tss_dtors[data->key](value);
		}
	    }
	}
    }

    while (_tinycthread_tss_head != NULL) {
	data = _tinycthread_tss_head->next;
	free(_tinycthread_tss_head);
	_tinycthread_tss_head = data;
    }
    _tinycthread_tss_head = NULL;
    _tinycthread_tss_tail = NULL;
}


static void NTAPI
_tinycthread_tss_callback(PVOID h, DWORD dwReason, PVOID pv)
{
    (void)h;
    (void)pv;

    if (_tinycthread_tss_head != NULL && (dwReason == DLL_THREAD_DETACH || dwReason == DLL_PROCESS_DETACH))
    {
	_tinycthread_tss_cleanup();
    }
}

#if defined(_MSC_VER)
#ifdef _M_X64
#pragma const_seg(".CRT$XLB")
#else
#pragma data_seg(".CRT$XLB")
#endif
PIMAGE_TLS_CALLBACK p_thread_callback = _tinycthread_tss_callback;
#ifdef _M_X64
#pragma data_seg()
#else
#pragma const_seg()
#endif
#endif
#endif /* defined(HAVE_WINDOWS_H) */

typedef struct {
    bu_thrd_start_t mFunction; /**< Pointer to the function to be executed. */
    void * mArg;            /**< Function argument for the thread function. */
} _bu_thread_start_info;

/* Thread wrapper function. */
#if defined(HAVE_WINDOWS_H)
static DWORD WINAPI _bu_thrd_wrapper_function(LPVOID aArg)
#else
static void * _bu_thrd_wrapper_function(void * aArg)
#endif
{
    bu_thrd_start_t fun;
    void *arg;
    int  res;

    /* Get thread startup information */
    _bu_thread_start_info *ti = (_bu_thread_start_info *) aArg;
    fun = ti->mFunction;
    arg = ti->mArg;

    /* The thread is responsible for freeing the startup information */
    free((void *)ti);

    /* Call the actual client thread function */
    res = fun(arg);

#if defined(HAVE_WINDOWS_H)
    if (_tinycthread_tss_head != NULL)
    {
	_tinycthread_tss_cleanup();
    }

    return (DWORD)res;
#else
    return (void*)(intptr_t)res;
#endif
}

int bu_thrd_create(bu_thrd_t *thr, bu_thrd_start_t func, void *arg)
{
    /* Fill out the thread startup information (passed to the thread wrapper,
     * which will eventually free it) */
    _bu_thread_start_info* ti = (_bu_thread_start_info*)malloc(sizeof(_bu_thread_start_info));
    if (ti == NULL)
    {
	return bu_thrd_nomem;
    }
    ti->mFunction = func;
    ti->mArg = arg;

    /* Create the thread */
#if defined(HAVE_WINDOWS_H)
    *thr = CreateThread(NULL, 0, _bu_thrd_wrapper_function, (LPVOID) ti, 0, NULL);
#else
    if(pthread_create(thr, NULL, _bu_thrd_wrapper_function, (void *)ti) != 0)
    {
	*thr = 0;
    }
#endif
    /* Did we fail to create the thread? */
    if(!*thr)
    {
	free((void *)ti);
	return bu_thrd_error;
    }

    return bu_thrd_success;

}

int bu_thrd_join(bu_thrd_t thr, int *res)
{
#if defined(HAVE_WINDOWS_H)
    DWORD dwRes;

    if (WaitForSingleObject(thr, INFINITE) == WAIT_FAILED)
    {
	return bu_thrd_error;
    }
    if (res != NULL)
    {
	if (GetExitCodeThread(thr, &dwRes) != 0)
	{
	    *res = (int) dwRes;
	}
	else
	{
	    return bu_thrd_error;
	}
    }
    CloseHandle(thr);
#else
    void *pres;
    if (pthread_join(thr, &pres) != 0)
    {
	return bu_thrd_error;
    }
    if (res != NULL)
    {
	*res = (int)(intptr_t)pres;
    }
#endif
    return bu_thrd_success;
}

int bu_cnd_broadcast(bu_cnd_t *cond)
{
#if defined(HAVE_WINDOWS_H)
    int haveWaiters;

    /* Are there any waiters? */
    EnterCriticalSection(&cond->mWaitersCountLock);
    haveWaiters = (cond->mWaitersCount > 0);
    LeaveCriticalSection(&cond->mWaitersCountLock);

    /* If we have any waiting threads, send them a signal */
    if(haveWaiters)
    {
	if (SetEvent(cond->mEvents[_CONDITION_EVENT_ALL]) == 0)
	{
	    return bu_thrd_error;
	}
    }

    return bu_thrd_success;
#else
    return pthread_cond_broadcast(cond) == 0 ? bu_thrd_success : bu_thrd_error;
#endif
}



/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
