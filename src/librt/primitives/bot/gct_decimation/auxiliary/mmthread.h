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

#include <stddef.h>
#ifdef HAVE_SYS_SELECT_H
#  include <sys/select.h>
#endif
#include <time.h>

#include "bu/tc.h"

#include "bu/exit.h"
#include "mmatomic.h"


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


#else


static inline int mtMutexTryLock(bu_mtx_t *mutex)
{
    switch (bu_mtx_trylock(mutex)) {
	case bu_thrd_success:
	    return 1;

	case bu_thrd_busy:
	    return 0;

	default:
	    bu_bomb("bu_mtx_trylock() failed");
	    return -1; /* silence warnings */
    };
}


#ifdef HAVE_PTHREAD_H
#include <pthread.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#endif

#if defined(_POSIX_SPIN_LOCKS) && _POSIX_SPIN_LOCKS > 0


typedef struct {
    pthread_spinlock_t pspinlock;
} mtSpin;


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


typedef bu_mtx_t mtSpin;


static inline void mtSpinInit(mtSpin *spin)
{
    if (bu_mtx_init(spin) == bu_thrd_error)
	bu_bomb("bu_mtx_init() failed");
}


static inline void mtSpinDestroy(mtSpin *spin)
{
    bu_mtx_destroy(spin);
}

static inline void mtSpinLock(mtSpin *spin)
{
    if (bu_mtx_lock(spin) == bu_thrd_error)
	bu_bomb("bu_mtx_lock() failed");
}


static inline void mtSpinUnlock(mtSpin *spin)
{
    if (bu_mtx_unlock(spin) == bu_thrd_error)
	bu_bomb("bu_mtx_unlock() failed");
}


#endif
#endif
#endif
