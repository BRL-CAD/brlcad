/* *****************************************************************************
 *
 * Copyright (c) 2014 Alexis Naveros. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are
 * permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 * *****************************************************************************
 */
/**
 * @file
 *
 * Threading interface, POSIX implementation.
 */


#ifndef MM_THREAD_H
#define MM_THREAD_H


#include "common.h"

#include "mmatomic.h"

#include <pthread.h>
#include <sys/time.h>
#include <unistd.h>

#if _POSIX_SPIN_LOCKS > 0
#define MT_SPIN_LOCK_SUPPORT
#endif


typedef struct mtThread mtThread;

struct mtThread {
    pthread_t pthread;
};


#define MT_MUTEX_INITIALIZER { PTHREAD_MUTEX_INITIALIZER }


static inline void mtThreadCreate(mtThread *thread, void *(*threadmain)(void *value), void *value)
{
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    pthread_create(&thread->pthread, &attr, threadmain, value);
    pthread_attr_destroy(&attr);
}

static inline void mtThreadJoin(mtThread *thread)
{
    void *ret;
    pthread_join(thread->pthread, &ret);
}


typedef struct mtMutex mtMutex;

struct mtMutex {
    pthread_mutex_t pmutex;
};

static inline void mtMutexInit(mtMutex *mutex)
{
    pthread_mutex_init(&mutex->pmutex, 0);
}

static inline void mtMutexDestroy(mtMutex *mutex)
{
    pthread_mutex_destroy(&mutex->pmutex);
}

static inline void mtMutexLock(mtMutex *mutex)
{
    pthread_mutex_lock(&mutex->pmutex);
}

static inline void mtMutexUnlock(mtMutex *mutex)
{
    pthread_mutex_unlock(&mutex->pmutex);
}

static inline int mtMutexTryLock(mtMutex *mutex)
{
    return !(pthread_mutex_trylock(&mutex->pmutex));
}


/****/


typedef struct mtSignal mtSignal;

struct mtSignal {
    pthread_cond_t pcond;
};


static inline void mtSignalInit(mtSignal *signal)
{
    pthread_cond_init(&signal->pcond, 0);
}


static inline void mtSignalDestroy(mtSignal *signal)
{
    pthread_cond_destroy(&signal->pcond);
}


static inline void mtSignalBroadcast(mtSignal *signal)
{
    pthread_cond_broadcast(&signal->pcond);
}


static inline void mtSignalWait(mtSignal *signal, mtMutex *mutex)
{
    pthread_cond_wait(&signal->pcond, &mutex->pmutex);
}


static inline void mtSignalWaitTimeout(mtSignal *signal, mtMutex *mutex, long milliseconds)
{
    uint64_t microsecs;
    struct timespec ts;
    struct timeval tp;
    gettimeofday(&tp, NULL);
    ts.tv_sec  = tp.tv_sec + (milliseconds / 1000);
    microsecs = tp.tv_usec + ((milliseconds % 1000) * 1000);

    if (microsecs >= 1000000) {
	ts.tv_sec++;
	microsecs -= 1000000;
    }

    ts.tv_nsec = microsecs * 1000;
    pthread_cond_timedwait(&signal->pcond, &mutex->pmutex, &ts);
    return;
}


/****/


#ifdef MM_ATOMIC_SUPPORT


typedef struct mtSpin mtSpin;


struct mtSpin {
    mmAtomic32 atomicspin;
};


static inline void mtSpinInit(mtSpin *spin)
{
    mmAtomicWrite32(&spin->atomicspin, 0x0);
}


static inline void mtSpinDestroy(mtSpin *UNUSED(spin))
{}


static inline void mtSpinLock(mtSpin *spin)
{
    mmAtomicSpin32(&spin->atomicspin, 0x0, 0x1);
}


static inline void mtSpinUnlock(mtSpin *spin)
{
    mmAtomicWrite32(&spin->atomicspin, 0x0);
}


#elif defined(MT_SPIN_LOCK_SUPPORT)


typedef struct mtSpin mtSpin;


struct mtSpin {
    pthread_spinlock_t pspinlock;
};


static inline void mtSpinInit(mtSpin *spin)
{
    pthread_spin_init(&spin->pspinlock, PTHREAD_PROCESS_PRIVATE);
}


static inline void mtSpinDestroy(mtSpin *spin)
{
    pthread_spin_destroy(&spin->pspinlock);
}

static inline void mtSpinLock(mtSpin *spin)
{
    pthread_spin_lock(&spin->pspinlock);
}


static inline void mtSpinUnlock(mtSpin *spin)
{
    pthread_spin_unlock(&spin->pspinlock);
}


#else


typedef struct mtMutex mtSpin;


#define mtSpinInit(a) mtMutexInit(a)
#define mtSpinDestroy(a) mtMutexDestroy(a)
#define mtSpinLock(a) mtMutexLock(a)
#define mtSpinUnlock(a) mtMutexUnlock(a)


#endif


#endif
