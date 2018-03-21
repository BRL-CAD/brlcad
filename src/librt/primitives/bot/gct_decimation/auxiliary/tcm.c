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
#include "tcm.h"
#include "bu/malloc.h"

int rt_mtx_init(rt_mtx_t *mtx)
{
#if defined(HAVE_WINDOWS_H)
    mtx->mAlreadyLocked = FALSE;
    mtx->mRecursive = type & mtx_recursive;
    mtx->mTimed = type & rt_mtx_timed;
    if (!mtx->mTimed)
    {
	InitializeCriticalSection(&(mtx->mHandle.cs));
    }
    else
    {
	mtx->mHandle.mut = CreateMutex(NULL, FALSE, NULL);
	if (mtx->mHandle.mut == NULL)
	{
	    return thrd_error;
	}
    }
    return thrd_success;
#else
    int ret;
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    ret = pthread_mutex_init(mtx, &attr);
    pthread_mutexattr_destroy(&attr);
    return ret == 0 ? thrd_success : thrd_error;
#endif
}

void rt_mtx_destroy(rt_mtx_t *mtx)
{
#if defined(HAVE_WINDOWS_H)
  if (!mtx->mTimed)
  {
    DeleteCriticalSection(&(mtx->mHandle.cs));
  }
  else
  {
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

int rt_cnd_init(rt_cnd_t *cond)
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
    return thrd_error;
  }
  cond->mEvents[_CONDITION_EVENT_ALL] = CreateEvent(NULL, TRUE, FALSE, NULL);
  if (cond->mEvents[_CONDITION_EVENT_ALL] == NULL)
  {
    CloseHandle(cond->mEvents[_CONDITION_EVENT_ONE]);
    cond->mEvents[_CONDITION_EVENT_ONE] = NULL;
    return thrd_error;
  }

  return thrd_success;
#else
  return pthread_cond_init(cond, NULL) == 0 ? thrd_success : thrd_error;
#endif
}

void rt_cnd_destroy(rt_cnd_t *cond)
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

int rt_mtx_lock(rt_mtx_t *mtx)
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
        return thrd_error;
    }
  }

  if (!mtx->mRecursive)
  {
    while(mtx->mAlreadyLocked) Sleep(1); /* Simulate deadlock... */
    mtx->mAlreadyLocked = TRUE;
  }
  return thrd_success;
#else
  return pthread_mutex_lock(mtx) == 0 ? thrd_success : thrd_error;
#endif
}

int rt_mtx_unlock(rt_mtx_t *mtx)
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
      return thrd_error;
    }
  }
  return thrd_success;
#else
  return pthread_mutex_unlock(mtx) == 0 ? thrd_success : thrd_error;;
#endif
}

int rt_cnd_signal(rt_cnd_t *cond)
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
      return thrd_error;
    }
  }

  return thrd_success;
#else
  return pthread_cond_signal(cond) == 0 ? thrd_success : thrd_error;
#endif
}

#if defined(HAVE_WINDOWS_H)
static int _rt_rt_cnd_timedwait_win32(rt_cnd_t *cond, rt_mtx_t *mtx, DWORD timeout)
{
  DWORD result;
  int lastWaiter;

  /* Increment number of waiters */
  EnterCriticalSection(&cond->mWaitersCountLock);
  ++ cond->mWaitersCount;
  LeaveCriticalSection(&cond->mWaitersCountLock);

  /* Release the mutex while waiting for the condition (will decrease
     the number of waiters when done)... */
  mtx_unlock(mtx);

  /* Wait for either event to become signaled due to cnd_signal() or
     cnd_broadcast() being called */
  result = WaitForMultipleObjects(2, cond->mEvents, FALSE, timeout);
  if (result == WAIT_TIMEOUT)
  {
    /* The mutex is locked again before the function returns, even if an error occurred */
    mtx_lock(mtx);
    return thrd_timedout;
  }
  else if (result == WAIT_FAILED)
  {
    /* The mutex is locked again before the function returns, even if an error occurred */
    mtx_lock(mtx);
    return thrd_error;
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
      mtx_lock(mtx);
      return thrd_error;
    }
  }

  /* Re-acquire the mutex */
  mtx_lock(mtx);

  return thrd_success;
}
#endif

int rt_cnd_wait(rt_cnd_t *cond, rt_mtx_t *mtx)
{
#if defined(HAVE_WINDOWS_H)
  return _rt_rt_cnd_timedwait_win32(cond, mtx, INFINITE);
#else
  return pthread_cond_wait(cond, mtx) == 0 ? thrd_success : thrd_error;
#endif
}

typedef struct {
    rt_thrd_start_t mFunction; /**< Pointer to the function to be executed. */
    void * mArg;            /**< Function argument for the thread function. */
} _rt_thread_start_info;

/* Thread wrapper function. */
#if defined(HAVE_WINDOWS_H)
static DWORD WINAPI _rt_thrd_wrapper_function(LPVOID aArg)
#else
static void * _rt_thrd_wrapper_function(void * aArg)
#endif
{
    rt_thrd_start_t fun;
    void *arg;
    int  res;

    /* Get thread startup information */
    _rt_thread_start_info *ti = (_rt_thread_start_info *) aArg;
    fun = ti->mFunction;
    arg = ti->mArg;

    /* The thread is responsible for freeing the startup information */
    bu_free((void *)ti, "thread info");

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

int rt_thrd_create(rt_thrd_t *thr, rt_thrd_start_t func, void *arg)
{
      /* Fill out the thread startup information (passed to the thread wrapper,
       *      which will eventually free it) */
      _rt_thread_start_info* ti = (_rt_thread_start_info*)bu_malloc(sizeof(_rt_thread_start_info), "thread info");
        if (ti == NULL)
	      {
		      return thrd_nomem;
		        }
	  ti->mFunction = func;
	    ti->mArg = arg;

	      /* Create the thread */
#if defined(HAVE_WINDOWS_H)
	    *thr = CreateThread(NULL, 0, _rt_thrd_wrapper_function, (LPVOID) ti, 0, NULL);
#else
	    if(pthread_create(thr, NULL, _rt_thrd_wrapper_function, (void *)ti) != 0)
	    {
		*thr = 0;
	    }
#endif
	    /* Did we fail to create the thread? */
	    if(!*thr)
	    {
		bu_free(ti, "thread info");
		return thrd_error;
	    }

	    return thrd_success;

}

int rt_thrd_join(rt_thrd_t thr, int *res)
{
#if defined(HAVE_WINDOWS_H)
    DWORD dwRes;

    if (WaitForSingleObject(thr, INFINITE) == WAIT_FAILED)
    {
	return thrd_error;
    }
    if (res != NULL)
    {
	if (GetExitCodeThread(thr, &dwRes) != 0)
	{
	    *res = (int) dwRes;
	}
	else
	{
	    return thrd_error;
	}
    }
    CloseHandle(thr);
#else
    void *pres;
    if (pthread_join(thr, &pres) != 0)
    {
	return thrd_error;
    }
    if (res != NULL)
    {
	*res = (int)(intptr_t)pres;
    }
#endif
    return thrd_success;
}

int rt_cnd_broadcast(rt_cnd_t *cond)
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
	    return thrd_error;
	}
    }

    return thrd_success;
#else
    return pthread_cond_broadcast(cond) == 0 ? thrd_success : thrd_error;
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
