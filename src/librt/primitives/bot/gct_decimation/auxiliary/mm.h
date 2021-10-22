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
 * GCT utility header.
 */


#ifndef MM_H
#define MM_H

#include "common.h"

#include <stddef.h>
#ifdef HAVE_SYS_SELECT_H
#  include <sys/select.h>
#endif
#include <time.h>

#include "bu/tc.h"
#include "bu/exit.h"

#if (defined(__i386__) || defined(__x86_64__)) && defined(__GNUC__)
#define MM_ATOMIC_SUPPORT


#define MM_ATOMIC_ACCESS_L(v) ((v)->value)
#define mmAtomicCmpReplace32(v,old,vnew) (mmAtomicCmpXchg32(v,old,vnew)==(old))


#ifdef __x86_64__
#  define mmAtomicP mmAtomic64
#  define mmAtomicWriteP(v,i) mmAtomicWrite64(v,(int64_t)i)
#elif defined(__i386__)
#  define mmAtomicP mmAtomic32
#  define mmAtomicWriteP(v,i) mmAtomicWrite32(v,(int32_t)i)
#else
#  error should not reach here
#endif


typedef struct {
    volatile int32_t value;
} mmAtomic32;


static inline int32_t mmAtomicRead32(mmAtomic32 *v)
{
    __asm__ __volatile__("":::"memory");
    return v->value;
}


static inline void mmAtomicWrite32(mmAtomic32 *v, int32_t i)
{
    __asm__ __volatile__("":::"memory");
    v->value = i;
}


static inline void mmAtomicAnd32(mmAtomic32 *v, int32_t i)
{
    __asm__ __volatile__(
	    "lock; andl %1,%0"
	    :"=m"(v->value)
	    :"ir"(i), "m"(v->value) :"memory");
}


static inline void mmAtomicOr32(mmAtomic32 *v, int32_t i)
{
    __asm__ __volatile__(
	    "lock; orl %1,%0"
	    :"=m"(v->value)
	    :"ir"(i), "m"(v->value) :"memory");
}


static inline void mmAtomicAdd32(mmAtomic32 *v, int32_t i)
{
    __asm__ __volatile__(
	    "lock; addl %1,%0"
	    :"=m"(v->value)
	    :"ir"(i), "m"(v->value) :"memory");
}


static inline int32_t mmAtomicCmpXchg32(mmAtomic32 *v, int32_t old, int32_t vnew)
{
    int32_t prev;
    __asm__ __volatile__(
	    "lock; cmpxchgl %1,%2"
	    :"=a"(prev)
	    :"r"(vnew), "m"(v->value), "a"(old) :"memory");
    return prev;
}


static inline int32_t mmAtomicAddRead32(mmAtomic32 *v, int32_t add)
{
    int32_t i;

    do {
	i = mmAtomicRead32(v);
    } while (mmAtomicCmpXchg32(v, i, i + add) != i);

    return i + add;
}


static inline void mmAtomicSpin32(mmAtomic32 *v, int32_t old, int32_t vnew)
{
    for (; mmAtomicCmpXchg32(v, old, vnew) != old;) {
	for (; mmAtomicRead32(v) != old;)
	    __asm__ __volatile__("rep; nop":::"memory");
    }
}


static inline void mmAtomicSpinWaitEq32(mmAtomic32 *v, int32_t i)
{
    __asm__ __volatile__(
	    "jmp 1f\n"
	    ".p2align 6\n"
	    "2:\n"
	    "rep; nop\n"
	    "1:\n"
	    "cmpl %1,%0\n"
	    "jnz 2b\n"
	    ".p2align 4\n"
	    "3:\n"
	    :
	    :"m"(v->value), "r"(i) :"memory");
}


typedef struct {
    mmAtomic32 v;
} mmAtomicLock32;


static inline void mmAtomicLockInit32(mmAtomicLock32 *v)
{
    mmAtomicWrite32(&v->v, 0);
}


static inline int mmAtomicAddTestNegative32(mmAtomic32 *v, int32_t i)
{
    unsigned char c;
    __asm__ __volatile__(
	    "lock; addl %2,%0; sets %1"
	    :"=m"(v->value), "=qm"(c)
	    :"ir"(i), "m"(v->value) :"memory");
    return c;
}


static inline int mmAtomicLockTryRead32(mmAtomicLock32 *v, int spincount)
{
    do {
	if (mmAtomicRead32(&v->v) < 0)
	    __asm__ __volatile__("rep; nop":::"memory");
	else {
	    if (!(mmAtomicAddTestNegative32(&v->v, 1)))
		return 1;

	    mmAtomicAdd32(&v->v, -1);
	}
    } while (--spincount);

    return 0;
}

#define MM_ATOMIC_LOCK32_WRITE (-((int32_t)0x10000000))
static inline int mmAtomicLockTryWrite32(mmAtomicLock32 *v, int spincount)
{
    do {
	if (mmAtomicRead32(&v->v))
	    __asm__ __volatile__("rep; nop":::"memory");
	else {
	    if (!(mmAtomicCmpXchg32(&v->v, 0, MM_ATOMIC_LOCK32_WRITE)))
		return 1;
	}
    } while (--spincount);

    return 0;
}


static inline void mmAtomicLockDoneRead32(mmAtomicLock32 *v)
{
    mmAtomicAdd32(&v->v, -1);
}


static inline void mmAtomicLockDoneWrite32(mmAtomicLock32 *v)
{
    mmAtomicAdd32(&v->v, -MM_ATOMIC_LOCK32_WRITE);
}


#ifdef __x86_64__


typedef struct {
    volatile int64_t value;
} mmAtomic64;


static inline int64_t mmAtomicRead64(mmAtomic64 *v)
{
    __asm__ __volatile__("":::"memory");
    return v->value;
}


static inline void mmAtomicWrite64(mmAtomic64 *v, int64_t i)
{
    __asm__ __volatile__("":::"memory");
    v->value = i;
}

#endif


#endif



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

#define ADDRESS(p,o) ((void *)(((char *)p)+(o)))

#define MM_CPU_COUNT_MAXIMUM (1024)
#define MM_NODE_COUNT_MAXIMUM (256)

#if defined(__GNUC__) && defined(CPUCONF_CACHE_LINE_SIZE)
#define MM_CACHE_ALIGN __attribute__((aligned(CPUCONF_CACHE_LINE_SIZE)))
#define MM_NOINLINE __attribute__((noinline))
#else
#define MM_CACHE_ALIGN
#define MM_NOINLINE
#endif


#if defined(__linux__) || defined(__gnu_linux__) || defined(__linux) || defined(__linux)
#define MM_LINUX (1)
#define MM_UNIX (1)
#elif defined(__APPLE__)
#define MM_OSX (1)
#define MM_UNIX (1)
#elif defined(__unix__) || defined(__unix) || defined(unix)
#define MM_UNIX (1)
#elif defined(_WIN64) || defined(__WIN64__) || defined(WIN64)
#define MM_WIN64 (1)
#define MM_WIN32 (1)
#define MM_WINDOWS (1)
#elif defined(_WIN32) || defined(__WIN32__) || defined(WIN32)
#define MM_WIN32 (1)
#define MM_WINDOWS (1)
#endif

#if defined(__MINGW64__) && __MINGW64__
#define MM_MINGW32 (1)
#define MM_MINGW64 (1)
#elif defined(__MINGW32__) && __MINGW32__
#define MM_MINGW32 (1)
#endif


typedef struct {
    void *blocklist;
    void *freelist;
    size_t chunksize;
    int chunkperblock;
    int alignment;
    size_t allocsize;
    int keepfreecount;
    int chunkfreecount;
    void *treeroot;
    void *(*relayalloc)(void *head, size_t bytes);
    void (*relayfree)(void *head, void *v, size_t bytes);
    void *relayvalue;
    mtSpin spinlock;
} mmBlockHead;


typedef struct {
    void **prev;
    void *next;
} mmListNode;


typedef struct {
    int numaflag;
    int pagesize;
    int nodecount;
    int cpunode[MM_CPU_COUNT_MAXIMUM];
    int64_t nodesize[MM_NODE_COUNT_MAXIMUM];
    int64_t sysmemory;
} mmContext;

extern mmContext mmcontext;


void mmInit();


void *mmAlignAlloc(size_t bytes, intptr_t align);
void mmAlignFree(void *v);

void *mmBlockAlloc(mmBlockHead *head);
void mmBlockRelease(mmBlockHead *head, void *v);
void mmBlockFreeAll(mmBlockHead *head);
void mmBlockNodeInit(mmBlockHead *head, int nodeindex, size_t chunksize, int chunkperblock, int keepfreecount, int alignment);
void mmBlockInit(mmBlockHead *head, size_t chunksize, int chunkperblock, int keepfreecount, int alignment);
void *mmNodeAlloc(int nodeindex, size_t size);
void mmNodeFree(int nodeindex, void *v, size_t size);

#ifndef MM_ATOMIC_SUPPORT
void mmBlockProcessList(mmBlockHead *head, void *userpointer, int (*processchunk)(void *chunk, void *userpointer));
#endif


void mmListAdd(void **list, void *item, intptr_t offset);
void mmListRemove(void *item, intptr_t offset);

void mmThreadBindToCpu(int cpuindex);
int mmCpuGetNode(int cpuindex);

#endif /* MM_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
