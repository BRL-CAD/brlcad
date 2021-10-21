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
 * Atomic memory operations.
 */


#ifndef MM_ATOMIC_H
#define MM_ATOMIC_H


#include "common.h"


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
