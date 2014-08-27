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




#if ( defined(CPUCONF_ARCH_IA32) || defined(CPUCONF_ARCH_AMD64) ) && defined(__GNUC__) && !defined(MM_ATOMIC_SUPPORT)


#define MM_ATOMIC_SUPPORT


#ifdef CPUCONF_ARCH_AMD64
 #define MM_ATOMIC_64_BITS_SUPPORT
#endif


typedef struct { volatile int8_t value; } mmAtomic8;
typedef struct { volatile int16_t value; } mmAtomic16;
typedef struct { volatile int32_t value; } mmAtomic32;
#ifdef MM_ATOMIC_64_BITS_SUPPORT
typedef struct { volatile int64_t value; } mmAtomic64;
#endif


////


/*

Architecture Memory Ordering

Memory model for x86 and amd64
--- Aligned stores can not be partially seen by loads
--- Loads can NOT be reordered after loads
--- Loads can NOT be reordered after stores
--- Stores can NOT be reordered after stores
-X- Stores CAN be reordered after loads
--- Atomic instructions are NOT reordered with loads
--- Atomic instructions are NOT reordered with stores
--- Dependent loads can NOT be reordered

*/


/* Memory model configuration for x86/amd64 */
// #define CPUCONF_LOAD_REODERING_AFTER_LOAD
// #define CPUCONF_LOAD_REODERING_AFTER_STORE
#define CPUCONF_STORE_REODERING_AFTER_LOAD
// #define CPUCONF_STORE_REODERING_AFTER_STORE
// #define CPUCONF_ATOMIC_REODERING_WITH_LOAD
// #define CPUCONF_ATOMIC_REODERING_WITH_STORE
// #define CPUCONF_DEPENDENT_REODERING


////


/* Do nothing, prevent compiler reordering */
static inline void mmBarrier()
{
  __asm__ __volatile__( "":::"memory" );
  return;
}

/* All previous loads must complete before future loads */
static inline void mmReadBarrier()
{
#ifdef CPUCONF_CAP_SSE2
  __asm__ __volatile__( "lfence":::"memory" );
#else
  __asm__ __volatile__( "lock ; addl $0,(%%esp)":::"memory" );
#endif
  return;
}

/* All previous stores must complete before future stores */
static inline void mmWriteBarrier()
{
  /* x86 and AMD64 never reorder stores : the sfence instruction is useless unless chatting with devices on MMIO */
  __asm__ __volatile__( "":::"memory" );
  return;
}

/* All previous loads/stores must complete before future loads/stores */
static inline void mmFullBarrier()
{
#ifdef CPUCONF_CAP_SSE2
  __asm__ __volatile__( "mfence":::"memory" );
#else
  __asm__ __volatile__( "lock ; addl $0,(%%esp)":::"memory" );
#endif
  return;
}


////


/* Direct access to the atomic variables, for use when the caller knows no atomicity is needed */
#define MM_ATOMIC_ACCESS_8(v) ((v)->value)
#define MM_ATOMIC_ACCESS_16(v) ((v)->value)
#define MM_ATOMIC_ACCESS_32(v) ((v)->value)
#ifdef MM_ATOMIC_64_BITS_SUPPORT
 #define MM_ATOMIC_ACCESS_64(v) ((v)->value)
#endif


////


/*
mmAtomicRead*()
Atomically read the value
*/
static inline int8_t mmAtomicRead8( mmAtomic8 *v )
{
  mmBarrier();
  return v->value;
}

static inline int16_t mmAtomicRead16( mmAtomic16 *v )
{
  mmBarrier();
  return v->value;
}

static inline int32_t mmAtomicRead32( mmAtomic32 *v )
{
  mmBarrier();
  return v->value;
}

#ifdef MM_ATOMIC_64_BITS_SUPPORT
static inline int64_t mmAtomicRead64( mmAtomic64 *v )
{
  mmBarrier();
  return v->value;
}
#endif


////


/*
mmAtomicWrite*()
Atomically write the value
*/
static inline void mmAtomicWrite8( mmAtomic8 *v, int8_t i )
{
  mmBarrier();
  v->value = i;
  return;
}

static inline void mmAtomicWrite16( mmAtomic16 *v, int16_t i )
{
  mmBarrier();
  v->value = i;
  return;
}

static inline void mmAtomicWrite32( mmAtomic32 *v, int32_t i )
{
  mmBarrier();
  v->value = i;
  return;
}

#ifdef MM_ATOMIC_64_BITS_SUPPORT
static inline void mmAtomicWrite64( mmAtomic64 *v, int64_t i )
{
  mmBarrier();
  v->value = i;
  return;
}
#endif


////


/*
mmAtomicBarrierWrite*()
Atomically write the value and act as a full memory barrier
*/
static inline void mmAtomicBarrierWrite8( mmAtomic8 *v, int8_t i )
{
  __asm__ __volatile__(
    "lock ; xchgb %0,%1"
    :"=q"(i)
    :"m"(v->value), "0"(i) :"memory" );
  return;
}

static inline void mmAtomicBarrierWrite16( mmAtomic16 *v, int16_t i )
{
  __asm__ __volatile__(
    "lock ; xchgw %0,%1"
    :"=q"(i)
    :"m"(v->value), "0"(i) :"memory" );
  return;
}

static inline void mmAtomicBarrierWrite32( mmAtomic32 *v, int32_t i )
{
  __asm__ __volatile__(
    "lock ; xchgl %0,%1"
    :"=q"(i)
    :"m"(v->value), "0"(i) :"memory" );
  return;
}

#ifdef MM_ATOMIC_64_BITS_SUPPORT
static inline void mmAtomicBarrierWrite64( mmAtomic64 *v, int64_t i )
{
  __asm__ __volatile__(
    "lock ; xchgq %0,%1"
    :"=q"(i)
    :"m"(v->value), "0"(i) :"memory" );
  return;
}
#endif


////


static inline void mmAtomicAdd8( mmAtomic8 *v, int8_t i )
{
  __asm__ __volatile__(
    "lock ; addb %1,%0"
    :"=m"(v->value)
    :"ir"(i), "m"(v->value) :"memory" );
}

static inline void mmAtomicAdd16( mmAtomic16 *v, int16_t i )
{
  __asm__ __volatile__(
    "lock ; addw %1,%0"
    :"=m"(v->value)
    :"ir"(i), "m"(v->value) :"memory" );
}

static inline void mmAtomicAdd32( mmAtomic32 *v, int32_t i )
{
  __asm__ __volatile__(
    "lock ; addl %1,%0"
    :"=m"(v->value)
    :"ir"(i), "m"(v->value) :"memory" );
}

#ifdef MM_ATOMIC_64_BITS_SUPPORT
static inline void mmAtomicAdd64( mmAtomic64 *v, int64_t i )
{
  __asm__ __volatile__(
    "lock ; addq %1,%0"
    :"=m"(v->value)
    :"ir"(i), "m"(v->value) :"memory" );
}
#endif


////


static inline void mmAtomicSub8( mmAtomic8 *v, int8_t i )
{
  __asm__ __volatile__(
    "lock ; subb %1,%0"
    :"=m"(v->value)
    :"ir"(i), "m"(v->value) :"memory" );
}

static inline void mmAtomicSub16( mmAtomic16 *v, int16_t i )
{
  __asm__ __volatile__(
    "lock ; subw %1,%0"
    :"=m"(v->value)
    :"ir"(i), "m"(v->value) :"memory" );
}

static inline void mmAtomicSub32( mmAtomic32 *v, int32_t i )
{
  __asm__ __volatile__(
    "lock ; subl %1,%0"
    :"=m"(v->value)
    :"ir"(i), "m"(v->value) :"memory" );
}

#ifdef MM_ATOMIC_64_BITS_SUPPORT
static inline void mmAtomicSub64( mmAtomic64 *v, int64_t i )
{
  __asm__ __volatile__(
    "lock ; subq %1,%0"
    :"=m"(v->value)
    :"ir"(i), "m"(v->value) :"memory" );
}
#endif


////


static inline int mmAtomicAddTestZero8( mmAtomic8 *v, int8_t i )
{
  unsigned char c;
  __asm__ __volatile__(
    "lock ; addb %2,%0 ; setz %1"
    :"=m"(v->value), "=qm"(c)
    :"ir"(i), "m"(v->value) :"memory" );
  return c;
}

static inline int mmAtomicAddTestZero16( mmAtomic16 *v, int16_t i )
{
  unsigned char c;
  __asm__ __volatile__(
    "lock ; addw %2,%0 ; setz %1"
    :"=m"(v->value), "=qm"(c)
    :"ir"(i), "m"(v->value) :"memory" );
  return c;
}

static inline int mmAtomicAddTestZero32( mmAtomic32 *v, int32_t i )
{
  unsigned char c;
  __asm__ __volatile__(
    "lock ; addl %2,%0 ; setz %1"
    :"=m"(v->value), "=qm"(c)
    :"ir"(i), "m"(v->value) :"memory" );
  return c;
}

#ifdef MM_ATOMIC_64_BITS_SUPPORT
static inline int mmAtomicAddTestZero64( mmAtomic64 *v, int64_t i )
{
  unsigned char c;
  __asm__ __volatile__(
    "lock ; addq %2,%0 ; setz %1"
    :"=m"(v->value), "=qm"(c)
    :"ir"(i), "m"(v->value) :"memory" );
  return c;
}
#endif


////


static inline int mmAtomicSubTestZero8( mmAtomic8 *v, int8_t i )
{
  unsigned char c;
  __asm__ __volatile__(
    "lock ; subb %2,%0 ; setz %1"
    :"=m"(v->value), "=qm"(c)
    :"ir"(i), "m"(v->value) :"memory" );
  return c;
}

static inline int mmAtomicSubTestZero16( mmAtomic16 *v, int16_t i )
{
  unsigned char c;
  __asm__ __volatile__(
    "lock ; subw %2,%0 ; setz %1"
    :"=m"(v->value), "=qm"(c)
    :"ir"(i), "m"(v->value) :"memory" );
  return c;
}

static inline int mmAtomicSubTestZero32( mmAtomic32 *v, int32_t i )
{
  unsigned char c;
  __asm__ __volatile__(
    "lock ; subl %2,%0 ; setz %1"
    :"=m"(v->value), "=qm"(c)
    :"ir"(i), "m"(v->value) :"memory" );
  return c;
}

#ifdef MM_ATOMIC_64_BITS_SUPPORT
static inline int mmAtomicSubTestZero64( mmAtomic64 *v, int64_t i )
{
  unsigned char c;
  __asm__ __volatile__(
    "lock ; subq %2,%0 ; setz %1"
    :"=m"(v->value), "=qm"(c)
    :"ir"(i), "m"(v->value) :"memory" );
  return c;
}
#endif


////


static inline void mmAtomicInc8( mmAtomic8 *v )
{
  __asm__ __volatile__(
    "lock ; incb %0"
    :"=m"(v->value)
    :"m"(v->value) :"memory" );
}

static inline void mmAtomicInc16( mmAtomic16 *v )
{
  __asm__ __volatile__(
    "lock ; incw %0"
    :"=m"(v->value)
    :"m"(v->value) :"memory" );
}

static inline void mmAtomicInc32( mmAtomic32 *v )
{
  __asm__ __volatile__(
    "lock ; incl %0"
    :"=m"(v->value)
    :"m"(v->value) :"memory" );
}

#ifdef MM_ATOMIC_64_BITS_SUPPORT
static inline void mmAtomicInc64( mmAtomic64 *v )
{
  __asm__ __volatile__(
    "lock ; incq %0"
    :"=m"(v->value)
    :"m"(v->value) :"memory" );
}
#endif


////


static inline void mmAtomicDec8( mmAtomic8 *v )
{
  __asm__ __volatile__(
    "lock ; decl %0"
    :"=m"(v->value)
    :"m"(v->value) :"memory" );
}

static inline void mmAtomicDec16( mmAtomic16 *v )
{
  __asm__ __volatile__(
    "lock ; decl %0"
    :"=m"(v->value)
    :"m"(v->value) :"memory" );
}

static inline void mmAtomicDec32( mmAtomic32 *v )
{
  __asm__ __volatile__(
    "lock ; decl %0"
    :"=m"(v->value)
    :"m"(v->value) :"memory" );
}

#ifdef MM_ATOMIC_64_BITS_SUPPORT
static inline void mmAtomicDec64( mmAtomic64 *v )
{
  __asm__ __volatile__(
    "lock ; decq %0"
    :"=m"(v->value)
    :"m"(v->value) :"memory" );
}
#endif


////


static inline int mmAtomicIncTestZero8( mmAtomic8 *v )
{
  unsigned char c;
  __asm__ __volatile__(
    "lock ; incb %0 ; setz %1"
    :"=m"(v->value), "=qm"(c)
    :"m"(v->value) :"memory" );
  return c != 0;
}

static inline int mmAtomicIncTestZero16( mmAtomic16 *v )
{
  unsigned char c;
  __asm__ __volatile__(
    "lock ; incw %0 ; setz %1"
    :"=m"(v->value), "=qm"(c)
    :"m"(v->value) :"memory" );
  return c != 0;
}

static inline int mmAtomicIncTestZero32( mmAtomic32 *v )
{
  unsigned char c;
  __asm__ __volatile__(
    "lock ; incl %0 ; setz %1"
    :"=m"(v->value), "=qm"(c)
    :"m"(v->value) :"memory" );
  return c != 0;
}

#ifdef MM_ATOMIC_64_BITS_SUPPORT
static inline int mmAtomicIncTestZero64( mmAtomic64 *v )
{
  unsigned char c;
  __asm__ __volatile__(
    "lock ; incq %0 ; setz %1"
    :"=m"(v->value), "=qm"(c)
    :"m"(v->value) :"memory" );
  return c != 0;
}
#endif


////


static inline int mmAtomicDecTestZero8( mmAtomic8 *v )
{
  unsigned char c;
  __asm__ __volatile__(
    "lock ; decb %0 ; setz %1"
    :"=m"(v->value), "=qm"(c)
    :"m"(v->value) :"memory" );
  return c != 0;
}

static inline int mmAtomicDecTestZero16( mmAtomic16 *v )
{
  unsigned char c;
  __asm__ __volatile__(
    "lock ; decw %0 ; setz %1"
    :"=m"(v->value), "=qm"(c)
    :"m"(v->value) :"memory" );
  return c != 0;
}

static inline int mmAtomicDecTestZero32( mmAtomic32 *v )
{
  unsigned char c;
  __asm__ __volatile__(
    "lock ; decl %0 ; setz %1"
    :"=m"(v->value), "=qm"(c)
    :"m"(v->value) :"memory" );
  return c != 0;
}

#ifdef MM_ATOMIC_64_BITS_SUPPORT
static inline int mmAtomicDecTestZero64( mmAtomic64 *v )
{
  unsigned char c;
  __asm__ __volatile__(
    "lock ; decq %0 ; setz %1"
    :"=m"(v->value), "=qm"(c)
    :"m"(v->value) :"memory" );
  return c != 0;
}
#endif


////


static inline int mmAtomicAddTestNegative8( mmAtomic8 *v, int8_t i )
{
  unsigned char c;
  __asm__ __volatile__(
    "lock ; addb %2,%0 ; sets %1"
    :"=m"(v->value), "=qm"(c)
    :"ir"(i), "m"(v->value) :"memory" );
  return c;
}

static inline int mmAtomicAddTestNegative16( mmAtomic16 *v, int16_t i )
{
  unsigned char c;
  __asm__ __volatile__(
    "lock ; addw %2,%0 ; sets %1"
    :"=m"(v->value), "=qm"(c)
    :"ir"(i), "m"(v->value) :"memory" );
  return c;
}

static inline int mmAtomicAddTestNegative32( mmAtomic32 *v, int32_t i )
{
  unsigned char c;
  __asm__ __volatile__(
    "lock ; addl %2,%0 ; sets %1"
    :"=m"(v->value), "=qm"(c)
    :"ir"(i), "m"(v->value) :"memory" );
  return c;
}

#ifdef MM_ATOMIC_64_BITS_SUPPORT
static inline int mmAtomicAddTestNegative64( mmAtomic64 *v, int64_t i )
{
  unsigned char c;
  __asm__ __volatile__(
    "lock ; addq %2,%0 ; sets %1"
    :"=m"(v->value), "=qm"(c)
    :"ir"(i), "m"(v->value) :"memory" );
  return c;
}
#endif


////


static inline int mmAtomicSubTestNegative8( mmAtomic8 *v, int8_t i )
{
  unsigned char c;
  __asm__ __volatile__(
    "lock ; subb %2,%0 ; sets %1"
    :"=m"(v->value), "=qm"(c)
    :"ir"(i), "m"(v->value) :"memory" );
  return c;
}

static inline int mmAtomicSubTestNegative16( mmAtomic16 *v, int16_t i )
{
  unsigned char c;
  __asm__ __volatile__(
    "lock ; subw %2,%0 ; sets %1"
    :"=m"(v->value), "=qm"(c)
    :"ir"(i), "m"(v->value) :"memory" );
  return c;
}

static inline int mmAtomicSubTestNegative32( mmAtomic32 *v, int32_t i )
{
  unsigned char c;
  __asm__ __volatile__(
    "lock ; subl %2,%0 ; sets %1"
    :"=m"(v->value), "=qm"(c)
    :"ir"(i), "m"(v->value) :"memory" );
  return c;
}

#ifdef MM_ATOMIC_64_BITS_SUPPORT
static inline int mmAtomicSubTestNegative64( mmAtomic64 *v, int64_t i )
{
  unsigned char c;
  __asm__ __volatile__(
    "lock ; subq %2,%0 ; sets %1"
    :"=m"(v->value), "=qm"(c)
    :"ir"(i), "m"(v->value) :"memory" );
  return c;
}
#endif



////////////////



static inline void mmAtomicAnd8( mmAtomic8 *v, int8_t i )
{
  __asm__ __volatile__(
    "lock ; andb %1,%0"
    :"=m"(v->value)
    :"ir"(i), "m"(v->value) :"memory" );
}

static inline void mmAtomicAnd16( mmAtomic16 *v, int16_t i )
{
  __asm__ __volatile__(
    "lock ; andw %1,%0"
    :"=m"(v->value)
    :"ir"(i), "m"(v->value) :"memory" );
}

static inline void mmAtomicAnd32( mmAtomic32 *v, int32_t i )
{
  __asm__ __volatile__(
    "lock ; andl %1,%0"
    :"=m"(v->value)
    :"ir"(i), "m"(v->value) :"memory" );
}

#ifdef MM_ATOMIC_64_BITS_SUPPORT
static inline void mmAtomicAnd64( mmAtomic64 *v, int64_t i )
{
  __asm__ __volatile__(
    "lock ; andq %1,%0"
    :"=m"(v->value)
    :"ir"(i), "m"(v->value) :"memory" );
}
#endif


////


static inline int mmAtomicAndTestZero8( mmAtomic8 *v, int8_t i )
{
  unsigned char c;
  __asm__ __volatile__(
    "lock ; andb %2,%0 ; setz %1"
    :"=m"(v->value), "=qm"(c)
    :"ir"(i), "m"(v->value) :"memory" );
  return c;
}

static inline int mmAtomicAndTestZero16( mmAtomic16 *v, int16_t i )
{
  unsigned char c;
  __asm__ __volatile__(
    "lock ; andw %2,%0 ; setz %1"
    :"=m"(v->value), "=qm"(c)
    :"ir"(i), "m"(v->value) :"memory" );
  return c;
}

static inline int mmAtomicAndTestZero32( mmAtomic32 *v, int32_t i )
{
  unsigned char c;
  __asm__ __volatile__(
    "lock ; andl %2,%0 ; setz %1"
    :"=m"(v->value), "=qm"(c)
    :"ir"(i), "m"(v->value) :"memory" );
  return c;
}

#ifdef MM_ATOMIC_64_BITS_SUPPORT
static inline int mmAtomicAndTestZero64( mmAtomic64 *v, int64_t i )
{
  unsigned char c;
  __asm__ __volatile__(
    "lock ; andq %2,%0 ; setz %1"
    :"=m"(v->value), "=qm"(c)
    :"ir"(i), "m"(v->value) :"memory" );
  return c;
}
#endif


////


static inline void mmAtomicOr8( mmAtomic8 *v, int8_t i )
{
  __asm__ __volatile__(
    "lock ; orb %1,%0"
    :"=m"(v->value)
    :"ir"(i), "m"(v->value) :"memory" );
}

static inline void mmAtomicOr16( mmAtomic16 *v, int16_t i )
{
  __asm__ __volatile__(
    "lock ; orw %1,%0"
    :"=m"(v->value)
    :"ir"(i), "m"(v->value) :"memory" );
}

static inline void mmAtomicOr32( mmAtomic32 *v, int32_t i )
{
  __asm__ __volatile__(
    "lock ; orl %1,%0"
    :"=m"(v->value)
    :"ir"(i), "m"(v->value) :"memory" );
}

#ifdef MM_ATOMIC_64_BITS_SUPPORT
static inline void mmAtomicOr64( mmAtomic64 *v, int64_t i )
{
  __asm__ __volatile__(
    "lock ; orq %1,%0"
    :"=m"(v->value)
    :"ir"(i), "m"(v->value) :"memory" );
}
#endif


////


static inline int mmAtomicOrTestZero8( mmAtomic8 *v, int8_t i )
{
  unsigned char c;
  __asm__ __volatile__(
    "lock ; orb %2,%0 ; setz %1"
    :"=m"(v->value), "=qm"(c)
    :"ir"(i), "m"(v->value) :"memory" );
  return c;
}

static inline int mmAtomicOrTestZero16( mmAtomic16 *v, int16_t i )
{
  unsigned char c;
  __asm__ __volatile__(
    "lock ; orw %2,%0 ; setz %1"
    :"=m"(v->value), "=qm"(c)
    :"ir"(i), "m"(v->value) :"memory" );
  return c;
}

static inline int mmAtomicOrTestZero32( mmAtomic32 *v, int32_t i )
{
  unsigned char c;
  __asm__ __volatile__(
    "lock ; orl %2,%0 ; setz %1"
    :"=m"(v->value), "=qm"(c)
    :"ir"(i), "m"(v->value) :"memory" );
  return c;
}

#ifdef MM_ATOMIC_64_BITS_SUPPORT
static inline int mmAtomicOrTestZero64( mmAtomic64 *v, int64_t i )
{
  unsigned char c;
  __asm__ __volatile__(
    "lock ; orq %2,%0 ; setz %1"
    :"=m"(v->value), "=qm"(c)
    :"ir"(i), "m"(v->value) :"memory" );
  return c;
}
#endif


////


static inline void mmAtomicXor8( mmAtomic8 *v, int8_t i )
{
  __asm__ __volatile__(
    "lock ; xorb %1,%0"
    :"=m"(v->value)
    :"ir"(i), "m"(v->value) :"memory" );
}

static inline void mmAtomicXor16( mmAtomic16 *v, int16_t i )
{
  __asm__ __volatile__(
    "lock ; xorw %1,%0"
    :"=m"(v->value)
    :"ir"(i), "m"(v->value) :"memory" );
}

static inline void mmAtomicXor32( mmAtomic32 *v, int32_t i )
{
  __asm__ __volatile__(
    "lock ; xorl %1,%0"
    :"=m"(v->value)
    :"ir"(i), "m"(v->value) :"memory" );
}

#ifdef MM_ATOMIC_64_BITS_SUPPORT
static inline void mmAtomicXor64( mmAtomic64 *v, int64_t i )
{
  __asm__ __volatile__(
    "lock ; xorq %1,%0"
    :"=m"(v->value)
    :"ir"(i), "m"(v->value) :"memory" );
}
#endif


////


static inline int mmAtomicXorTestZero8( mmAtomic8 *v, int8_t i )
{
  unsigned char c;
  __asm__ __volatile__(
    "lock ; xorb %2,%0 ; setz %1"
    :"=m"(v->value), "=qm"(c)
    :"ir"(i), "m"(v->value) :"memory" );
  return c;
}

static inline int mmAtomicXorTestZero16( mmAtomic16 *v, int16_t i )
{
  unsigned char c;
  __asm__ __volatile__(
    "lock ; xorw %2,%0 ; setz %1"
    :"=m"(v->value), "=qm"(c)
    :"ir"(i), "m"(v->value) :"memory" );
  return c;
}

static inline int mmAtomicXorTestZero32( mmAtomic32 *v, int32_t i )
{
  unsigned char c;
  __asm__ __volatile__(
    "lock ; xorl %2,%0 ; setz %1"
    :"=m"(v->value), "=qm"(c)
    :"ir"(i), "m"(v->value) :"memory" );
  return c;
}

#ifdef MM_ATOMIC_64_BITS_SUPPORT
static inline int mmAtomicXorTestZero64( mmAtomic64 *v, int64_t i )
{
  unsigned char c;
  __asm__ __volatile__(
    "lock ; xorq %2,%0 ; setz %1"
    :"=m"(v->value), "=qm"(c)
    :"ir"(i), "m"(v->value) :"memory" );
  return c;
}
#endif



////////////////



static inline int8_t mmAtomicXchg8( mmAtomic8 *v, int8_t i )
{
  __asm__ __volatile__(
    "lock ; xchgb %0,%1"
    :"=q"(i)
    :"m"(v->value), "0"(i) :"memory" );
  return i;
}

static inline int16_t mmAtomicXchg16( mmAtomic16 *v, int16_t i )
{
  __asm__ __volatile__(
    "lock ; xchgw %0,%1"
    :"=q"(i)
    :"m"(v->value), "0"(i) :"memory" );
  return i;
}

static inline int32_t mmAtomicXchg32( mmAtomic32 *v, int32_t i )
{
  __asm__ __volatile__(
    "lock ; xchgl %0,%1"
    :"=q"(i)
    :"m"(v->value), "0"(i) :"memory" );
  return i;
}

#ifdef MM_ATOMIC_64_BITS_SUPPORT
static inline int64_t mmAtomicXchg64( mmAtomic64 *v, int64_t i )
{
  __asm__ __volatile__(
    "lock ; xchgq %0,%1"
    :"=q"(i)
    :"m"(v->value), "0"(i) :"memory" );
  return i;
}
#endif


////


static inline int8_t mmAtomicCmpXchg8( mmAtomic8 *v, int8_t old, int8_t new )
{
  int8_t prev;
  __asm__ __volatile__(
    "lock ; cmpxchgb %1,%2"
    :"=a"(prev)
    :"r"(new), "m"(v->value), "a"(old) :"memory" );
  return prev;
}

static inline int16_t mmAtomicCmpXchg16( mmAtomic16 *v, int16_t old, int16_t new )
{
  int16_t prev;
  __asm__ __volatile__(
    "lock ; cmpxchgw %1,%2"
    :"=a"(prev)
    :"r"(new), "m"(v->value), "a"(old) :"memory" );
  return prev;
}

static inline int32_t mmAtomicCmpXchg32( mmAtomic32 *v, int32_t old, int32_t new )
{
  int32_t prev;
  __asm__ __volatile__(
    "lock ; cmpxchgl %1,%2"
    :"=a"(prev)
    :"r"(new), "m"(v->value), "a"(old) :"memory" );
  return prev;
}

#ifdef MM_ATOMIC_64_BITS_SUPPORT
static inline int64_t mmAtomicCmpXchg64( mmAtomic64 *v, int64_t old, int64_t new )
{
  int64_t prev;
  __asm__ __volatile__(
    "lock ; cmpxchgq %1,%2"
    :"=a"(prev)
    :"r"(new), "m"(v->value), "a"(old) :"memory" );
  return prev;
}
#endif



////////////////



static inline void mmAtomicPause()
{
  __asm__ __volatile__(
    "rep ; nop"
    :
    ::"memory" );
  return;
}


static inline void mmAtomicSpinWaitEq8( mmAtomic8 *v, int8_t i )
{
  __asm__ __volatile__(
    "jmp 1f\n"
    ".p2align 6\n"
    "2:\n"
    "rep ; nop\n"
    "1:\n"
    "cmpb %1,%0\n"
    "jnz 2b\n"
    ".p2align 4\n"
    "3:\n"
    :
    :"m"(v->value), "r"(i) :"memory" );
  return;
}


static inline void mmAtomicSpinWaitEq16( mmAtomic16 *v, int16_t i )
{
  __asm__ __volatile__(
    "jmp 1f\n"
    ".p2align 6\n"
    "2:\n"
    "rep ; nop\n"
    "1:\n"
    "cmpw %1,%0\n"
    "jnz 2b\n"
    ".p2align 4\n"
    "3:\n"
    :
    :"m"(v->value), "r"(i) :"memory" );
  return;
}


static inline void mmAtomicSpinWaitEq32( mmAtomic32 *v, int32_t i )
{
  __asm__ __volatile__(
    "jmp 1f\n"
    ".p2align 6\n"
    "2:\n"
    "rep ; nop\n"
    "1:\n"
    "cmpl %1,%0\n"
    "jnz 2b\n"
    ".p2align 4\n"
    "3:\n"
    :
    :"m"(v->value), "r"(i) :"memory" );
  return;
}


#ifdef MM_ATOMIC_64_BITS_SUPPORT
static inline void mmAtomicSpinWaitEq64( mmAtomic64 *v, int64_t i )
{
  __asm__ __volatile__(
    "jmp 1f\n"
    ".p2align 6\n"
    "2:\n"
    "rep ; nop\n"
    "1:\n"
    "cmpq %1,%0\n"
    "jnz 2b\n"
    ".p2align 4\n"
    "3:\n"
    :
    :"m"(v->value), "r"(i) :"memory" );
  return;
}
#endif


static inline int32_t mmAtomicSpinWaitEq8Count( mmAtomic8 *v, int8_t i, int32_t spinmaxcount )
{
  __asm__ __volatile__(
    "jmp 1f\n"
    ".p2align 6\n"
    "2:\n"
    "subl $1,%0\n"
    "jz 3f\n"
    "rep ; nop\n"
    "1:\n"
    "cmpb %2,%1\n"
    "jnz 2b\n"
    ".p2align 4\n"
    "3:\n"
    :"=q"(spinmaxcount)
    :"m"(v->value), "r"(i), "0"(spinmaxcount) :"memory" );
  return spinmaxcount;
}


static inline int32_t mmAtomicSpinWaitEq16Count( mmAtomic16 *v, int16_t i, int32_t spinmaxcount )
{
  __asm__ __volatile__(
    "jmp 1f\n"
    ".p2align 6\n"
    "2:\n"
    "subl $1,%0\n"
    "jz 3f\n"
    "rep ; nop\n"
    "1:\n"
    "cmpw %2,%1\n"
    "jnz 2b\n"
    ".p2align 4\n"
    "3:\n"
    :"=q"(spinmaxcount)
    :"m"(v->value), "r"(i), "0"(spinmaxcount) :"memory" );
  return spinmaxcount;
}


static inline int32_t mmAtomicSpinWaitEq32Count( mmAtomic32 *v, int32_t i, int32_t spinmaxcount )
{
  __asm__ __volatile__(
    "jmp 1f\n"
    ".p2align 6\n"
    "2:\n"
    "subl $1,%0\n"
    "jz 3f\n"
    "rep ; nop\n"
    "1:\n"
    "cmpl %2,%1\n"
    "jnz 2b\n"
    ".p2align 4\n"
    "3:\n"
    :"=q"(spinmaxcount)
    :"m"(v->value), "r"(i), "0"(spinmaxcount) :"memory" );
  return spinmaxcount;
}


#ifdef MM_ATOMIC_64_BITS_SUPPORT
static inline int32_t mmAtomicSpinWaitEq64Count( mmAtomic64 *v, int64_t i, int32_t spinmaxcount )
{
  __asm__ __volatile__(
    "jmp 1f\n"
    ".p2align 6\n"
    "2:\n"
    "subl $1,%0\n"
    "jz 3f\n"
    "rep ; nop\n"
    "1:\n"
    "cmpq %2,%1\n"
    "jnz 2b\n"
    ".p2align 4\n"
    "3:\n"
    :"=q"(spinmaxcount)
    :"m"(v->value), "r"(i), "0"(spinmaxcount) :"memory" );
  return spinmaxcount;
}
#endif


static inline void mmAtomicSpinWaitNeq8( mmAtomic8 *v, int8_t i )
{
  __asm__ __volatile__(
    "jmp 1f\n"
    ".p2align 6\n"
    "2:\n"
    "rep ; nop\n"
    "1:\n"
    "cmpb %1,%0\n"
    "jz 2b\n"
    ".p2align 4\n"
    "3:\n"
    :
    :"m"(v->value), "r"(i) :"memory" );
  return;
}


static inline void mmAtomicSpinWaitNeq16( mmAtomic16 *v, int16_t i )
{
  __asm__ __volatile__(
    "jmp 1f\n"
    ".p2align 6\n"
    "2:\n"
    "rep ; nop\n"
    "1:\n"
    "cmpw %1,%0\n"
    "jz 2b\n"
    ".p2align 4\n"
    "3:\n"
    :
    :"m"(v->value), "r"(i) :"memory" );
  return;
}


static inline void mmAtomicSpinWaitNeq32( mmAtomic32 *v, int32_t i )
{
  __asm__ __volatile__(
    "jmp 1f\n"
    ".p2align 6\n"
    "2:\n"
    "rep ; nop\n"
    "1:\n"
    "cmpl %1,%0\n"
    "jz 2b\n"
    ".p2align 4\n"
    "3:\n"
    :
    :"m"(v->value), "r"(i) :"memory" );
  return;
}


#ifdef MM_ATOMIC_64_BITS_SUPPORT
static inline void mmAtomicSpinWaitNeq64( mmAtomic64 *v, int64_t i )
{
  __asm__ __volatile__(
    "jmp 1f\n"
    ".p2align 6\n"
    "2:\n"
    "rep ; nop\n"
    "1:\n"
    "cmpq %1,%0\n"
    "jz 2b\n"
    ".p2align 4\n"
    "3:\n"
    :
    :"m"(v->value), "r"(i) :"memory" );
  return;
}
#endif


static inline int32_t mmAtomicSpinWaitNeq8Count( mmAtomic8 *v, int8_t i, int32_t spinmaxcount )
{
  __asm__ __volatile__(
    "jmp 1f\n"
    ".p2align 6\n"
    "2:\n"
    "subl $1,%0\n"
    "jz 3f\n"
    "rep ; nop\n"
    "1:\n"
    "cmpb %2,%1\n"
    "jz 2b\n"
    ".p2align 4\n"
    "3:\n"
    :"=q"(spinmaxcount)
    :"m"(v->value), "r"(i), "0"(spinmaxcount) :"memory" );
  return spinmaxcount;
}


static inline int32_t mmAtomicSpinWaitNeq16Count( mmAtomic16 *v, int16_t i, int32_t spinmaxcount )
{
  __asm__ __volatile__(
    "jmp 1f\n"
    ".p2align 6\n"
    "2:\n"
    "subl $1,%0\n"
    "jz 3f\n"
    "rep ; nop\n"
    "1:\n"
    "cmpw %2,%1\n"
    "jz 2b\n"
    ".p2align 4\n"
    "3:\n"
    :"=q"(spinmaxcount)
    :"m"(v->value), "r"(i), "0"(spinmaxcount) :"memory" );
  return spinmaxcount;
}


static inline int32_t mmAtomicSpinWaitNeq32Count( mmAtomic32 *v, int32_t i, int32_t spinmaxcount )
{
  __asm__ __volatile__(
    "jmp 1f\n"
    ".p2align 6\n"
    "2:\n"
    "subl $1,%0\n"
    "jz 3f\n"
    "rep ; nop\n"
    "1:\n"
    "cmpl %2,%1\n"
    "jz 2b\n"
    ".p2align 4\n"
    "3:\n"
    :"=q"(spinmaxcount)
    :"m"(v->value), "r"(i), "0"(spinmaxcount) :"memory" );
  return spinmaxcount;
}


#ifdef MM_ATOMIC_64_BITS_SUPPORT
static inline int32_t mmAtomicSpinWaitNeq64Count( mmAtomic64 *v, int64_t i, int32_t spinmaxcount )
{
  __asm__ __volatile__(
    "jmp 1f\n"
    ".p2align 6\n"
    "2:\n"
    "subl $1,%0\n"
    "jz 3f\n"
    "rep ; nop\n"
    "1:\n"
    "cmpq %2,%1\n"
    "jz 2b\n"
    ".p2align 4\n"
    "3:\n"
    :"=q"(spinmaxcount)
    :"m"(v->value), "r"(i), "0"(spinmaxcount) :"memory" );
  return spinmaxcount;
}
#endif


////


static inline void mmAtomicSpin8( mmAtomic8 *v, int8_t old, int8_t new )
{
  for( ; mmAtomicCmpXchg8( v, old, new ) != old ; )
  {
    for( ; mmAtomicRead8( v ) != old ; )
      mmAtomicPause();
  }
  return;
}

static inline void mmAtomicSpin16( mmAtomic16 *v, int16_t old, int16_t new )
{
  for( ; mmAtomicCmpXchg16( v, old, new ) != old ; )
  {
    for( ; mmAtomicRead16( v ) != old ; )
      mmAtomicPause();
  }
  return;
}

static inline void mmAtomicSpin32( mmAtomic32 *v, int32_t old, int32_t new )
{
  for( ; mmAtomicCmpXchg32( v, old, new ) != old ; )
  {
    for( ; mmAtomicRead32( v ) != old ; )
      mmAtomicPause();
  }
  return;
}

#ifdef MM_ATOMIC_64_BITS_SUPPORT
static inline void mmAtomicSpin64( mmAtomic64 *v, int64_t old, int64_t new )
{
  for( ; mmAtomicCmpXchg64( v, old, new ) != old ; )
  {
    for( ; mmAtomicRead64( v ) != old ; )
      mmAtomicPause();
  }
  return;
}
#endif


static inline int mmAtomicTrySpin8( mmAtomic8 *v, int8_t old, int8_t new, int spincount )
{
  for( ; mmAtomicCmpXchg8( v, old, new ) != old ; )
  {
    for( ; mmAtomicRead8( v ) != old ; )
    {
      if( !( --spincount ) )
        return 0;
      mmAtomicPause();
    }
  }
  return 1;
}

static inline int mmAtomicTrySpin16( mmAtomic16 *v, int16_t old, int16_t new, int spincount )
{
  for( ; mmAtomicCmpXchg16( v, old, new ) != old ; )
  {
    for( ; mmAtomicRead16( v ) != old ; )
    {
      if( !( --spincount ) )
        return 0;
      mmAtomicPause();
    }
  }
  return 1;
}

static inline int mmAtomicTrySpin32( mmAtomic32 *v, int32_t old, int32_t new, int spincount )
{
  for( ; mmAtomicCmpXchg32( v, old, new ) != old ; )
  {
    for( ; mmAtomicRead32( v ) != old ; )
    {
      if( !( --spincount ) )
        return 0;
      mmAtomicPause();
    }
  }
  return 1;
}

#ifdef MM_ATOMIC_64_BITS_SUPPORT
static inline int mmAtomicTrySpin64( mmAtomic64 *v, int64_t old, int64_t new, int spincount )
{
  for( ; mmAtomicCmpXchg64( v, old, new ) != old ; )
  {
    for( ; mmAtomicRead64( v ) != old ; )
    {
      if( !( --spincount ) )
        return 0;
      mmAtomicPause();
    }
  }
  return 1;
}
#endif


////////////////


static inline int8_t mmAtomicAddRead8( mmAtomic8 *v, int8_t add )
{
  int8_t i;
  do
  {
    i = mmAtomicRead8( v );
  } while( mmAtomicCmpXchg8( v, i, i + add ) != i );
  return i + add;
}

static inline int16_t mmAtomicAddRead16( mmAtomic16 *v, int16_t add )
{
  int16_t i;
  do
  {
    i = mmAtomicRead16( v );
  } while( mmAtomicCmpXchg16( v, i, i + add ) != i );
  return i + add;
}

static inline int32_t mmAtomicAddRead32( mmAtomic32 *v, int32_t add )
{
  int32_t i;
  do
  {
    i = mmAtomicRead32( v );
  } while( mmAtomicCmpXchg32( v, i, i + add ) != i );
  return i + add;
}

#ifdef MM_ATOMIC_64_BITS_SUPPORT
static inline int64_t mmAtomicAddRead64( mmAtomic64 *v, int64_t add )
{
  int64_t i;
  do
  {
    i = mmAtomicRead64( v );
  } while( mmAtomicCmpXchg64( v, i, i + add ) != i );
  return i + add;
}
#endif


////


static inline int8_t mmAtomicReadAdd8( mmAtomic8 *v, int8_t add )
{
  int8_t i;
  do
  {
    i = mmAtomicRead8( v );
  } while( mmAtomicCmpXchg8( v, i, i + add ) != i );
  return i;
}

static inline int16_t mmAtomicReadAdd16( mmAtomic16 *v, int16_t add )
{
  int16_t i;
  do
  {
    i = mmAtomicRead16( v );
  } while( mmAtomicCmpXchg16( v, i, i + add ) != i );
  return i;
}

static inline int32_t mmAtomicReadAdd32( mmAtomic32 *v, int32_t add )
{
  int32_t i;
  do
  {
    i = mmAtomicRead32( v );
  } while( mmAtomicCmpXchg32( v, i, i + add ) != i );
  return i;
}

#ifdef MM_ATOMIC_64_BITS_SUPPORT
static inline int64_t mmAtomicReadAdd64( mmAtomic64 *v, int64_t add )
{
  int64_t i;
  do
  {
    i = mmAtomicRead64( v );
  } while( mmAtomicCmpXchg64( v, i, i + add ) != i );
  return i;
}
#endif


////


static inline int8_t mmAtomicReadAnd8( mmAtomic8 *v, int8_t mask )
{
  int8_t i, j;
  do
  {
    i = mmAtomicRead8( v );
    j = i & mask;
  } while( mmAtomicCmpXchg8( v, i, j ) != i );
  return i;
}

static inline int16_t mmAtomicReadAnd16( mmAtomic16 *v, int16_t mask )
{
  int16_t i, j;
  do
  {
    i = mmAtomicRead16( v );
    j = i & mask;
  } while( mmAtomicCmpXchg16( v, i, j ) != i );
  return i;
}

static inline int32_t mmAtomicReadAnd32( mmAtomic32 *v, int32_t mask )
{
  int32_t i, j;
  do
  {
    i = mmAtomicRead32( v );
    j = i & mask;
  } while( mmAtomicCmpXchg32( v, i, j ) != i );
  return i;
}

#ifdef MM_ATOMIC_64_BITS_SUPPORT
static inline int64_t mmAtomicReadAnd64( mmAtomic64 *v, int64_t mask )
{
  int64_t i, j;
  do
  {
    i = mmAtomicRead64( v );
    j = i & mask;
  } while( mmAtomicCmpXchg64( v, i, j ) != i );
  return i;
}
#endif


////


static inline int8_t mmAtomicReadOr8( mmAtomic8 *v, int8_t mask )
{
  int8_t i, j;
  do
  {
    i = mmAtomicRead8( v );
    j = i | mask;
  } while( mmAtomicCmpXchg8( v, i, j ) != i );
  return i;
}

static inline int16_t mmAtomicReadOr16( mmAtomic16 *v, int16_t mask )
{
  int16_t i, j;
  do
  {
    i = mmAtomicRead16( v );
    j = i | mask;
  } while( mmAtomicCmpXchg16( v, i, j ) != i );
  return i;
}

static inline int32_t mmAtomicReadOr32( mmAtomic32 *v, int32_t mask )
{
  int32_t i, j;
  do
  {
    i = mmAtomicRead32( v );
    j = i | mask;
  } while( mmAtomicCmpXchg32( v, i, j ) != i );
  return i;
}

#ifdef MM_ATOMIC_64_BITS_SUPPORT
static inline int64_t mmAtomicReadOr64( mmAtomic64 *v, int64_t mask )
{
  int64_t i, j;
  do
  {
    i = mmAtomicRead64( v );
    j = i | mask;
  } while( mmAtomicCmpXchg64( v, i, j ) != i );
  return i;
}
#endif


////


static inline int8_t mmAtomicReadIncLoop8( mmAtomic8 *v, int8_t max )
{
  int8_t i, j;
  do
  {
    i = mmAtomicRead8( v );
    j = i + 1;
    if( j >= max )
      j = 0;
  } while( mmAtomicCmpXchg8( v, i, j ) != i );
  return i;
}

static inline int16_t mmAtomicReadIncLoop16( mmAtomic16 *v, int16_t max )
{
  int16_t i, j;
  do
  {
    i = mmAtomicRead16( v );
    j = i + 1;
    if( j >= max )
      j = 0;
  } while( mmAtomicCmpXchg16( v, i, j ) != i );
  return i;
}

static inline int32_t mmAtomicReadIncLoop32( mmAtomic32 *v, int32_t max )
{
  int32_t i, j;
  do
  {
    i = mmAtomicRead32( v );
    j = i + 1;
    if( j >= max )
      j = 0;
  } while( mmAtomicCmpXchg32( v, i, j ) != i );
  return i;
}

#ifdef MM_ATOMIC_64_BITS_SUPPORT
static inline int64_t mmAtomicReadIncLoop64( mmAtomic64 *v, int64_t max )
{
  int64_t i, j;
  do
  {
    i = mmAtomicRead64( v );
    j = i + 1;
    if( j >= max )
      j = 0;
  } while( mmAtomicCmpXchg64( v, i, j ) != i );
  return i;
}
#endif


////


static inline int8_t mmAtomicReadAddLoop8( mmAtomic8 *v, int8_t add, int8_t base, int8_t max )
{
  int8_t i, j;
  do
  {
    i = mmAtomicRead8( v );
    j = i + add;
    if( j >= max )
      j = base;
  } while( mmAtomicCmpXchg8( v, i, j ) != i );
  return i;
}

static inline int16_t mmAtomicReadAddLoop16( mmAtomic16 *v, int16_t add, int16_t base, int16_t max )
{
  int16_t i, j;
  do
  {
    i = mmAtomicRead16( v );
    j = i + add;
    if( j >= max )
      j = base;
  } while( mmAtomicCmpXchg16( v, i, j ) != i );
  return i;
}

static inline int32_t mmAtomicReadAddLoop32( mmAtomic32 *v, int32_t add, int32_t base, int32_t max )
{
  int32_t i, j;
  do
  {
    i = mmAtomicRead32( v );
    j = i + add;
    if( j >= max )
      j = base;
  } while( mmAtomicCmpXchg32( v, i, j ) != i );
  return i;
}

#ifdef MM_ATOMIC_64_BITS_SUPPORT
static inline int64_t mmAtomicReadAddLoop64( mmAtomic64 *v, int64_t add, int64_t base, int64_t max )
{
  int64_t i, j;
  do
  {
    i = mmAtomicRead64( v );
    j = i + add;
    if( j >= max )
      j = base;
  } while( mmAtomicCmpXchg64( v, i, j ) != i );
  return i;
}
#endif



////////////////




#define mmAtomicCmpReplace8(v,old,new) (mmAtomicCmpXchg8(v,old,new)==(old))
#define mmAtomicCmpReplace16(v,old,new) (mmAtomicCmpXchg16(v,old,new)==(old))
#define mmAtomicCmpReplace32(v,old,new) (mmAtomicCmpXchg32(v,old,new)==(old))
#define mmAtomicCmpReplace64(v,old,new) (mmAtomicCmpXchg64(v,old,new)==(old))


#if CPUCONF_POINTER_BITS == 64
 #define mmAtomicP mmAtomic64
 #define MM_ATOMIC_ACCESS_P(v) (void *)MM_ATOMIC_ACCESS_64(v)
 #define mmAtomicReadP(v) (void *)mmAtomicRead64(v)
 #define mmAtomicWriteP(v,i) mmAtomicWrite64(v,(int64_t)i)
 #define mmAtomicAddP(v,i) mmAtomicAdd64(v,(int64_t)i)
 #define mmAtomicSubP(v,i) mmAtomicSub64(v,(int64_t)i)
 #define mmAtomicAddTestZeroP(v,i) mmAtomicAddTestZero64(v,(int64_t)i)
 #define mmAtomicSubTestZeroP(v,i) mmAtomicSubTestZero64(v,(int64_t)i)
 #define mmAtomicIncP(v) mmAtomicInc64(v)
 #define mmAtomicDecP(v) mmAtomicDec64(v)
 #define mmAtomicIncTestZeroP(v) mmAtomicIncTestZero64(v)
 #define mmAtomicDecTestZeroP(v) mmAtomicDecTestZero64(v)
 #define mmAtomicAddTestNegativeP(v,i) mmAtomicAddTestNegative64(v,(int64_t)i)
 #define mmAtomicSubTestNegativeP(v,i) mmAtomicSubTestNegative64(v,(int64_t)i)
 #define mmAtomicAndP(v,i) mmAtomicAnd64(v,(int64_t)i)
 #define mmAtomicAndTestZeroP(v,i) mmAtomicAndTestZero64(v,(int64_t)i)
 #define mmAtomicOrP(v,i) mmAtomicOr64(v,(int64_t)i)
 #define mmAtomicOrTestZeroP(v,i) mmAtomicOrTestZero64(v,(int64_t)i)
 #define mmAtomicXorP(v,i) mmAtomicXor64(v,(int64_t)i)
 #define mmAtomicXorTestZeroP(v,i) mmAtomicXorTestZero64(v,(int64_t)i)
 #define mmAtomicXchgP(v,i) (void *)mmAtomicXchg64(v,(int64_t)i)
 #define mmAtomicCmpXchgP(v,i,j) (void *)mmAtomicCmpXchg64(v,(int64_t)i,(int64_t)j)
 #define mmAtomicCmpReplaceP(v,i,j) mmAtomicCmpReplace64(v,(int64_t)i,(int64_t)j)
 #define mmAtomicSpinP(v,i,j) (void *)mmAtomicSpin64(v,(int64_t)i,(int64_t)j)
 #define mmAtomicAddReadP(v,i) (void *)mmAtomicAddRead64(v,(int64_t)i)
#elif CPUCONF_POINTER_BITS == 32
 #define mmAtomicP mmAtomic32
 #define MM_ATOMIC_ACCESS_P(v) (void *)MM_ATOMIC_ACCESS_32(v)
 #define mmAtomicReadP(v) (void *)mmAtomicRead32(v)
 #define mmAtomicWriteP(v,i) mmAtomicWrite32(v,(int32_t)i)
 #define mmAtomicAddP(v,i) mmAtomicAdd32(v,(int32_t)i)
 #define mmAtomicSubP(v,i) mmAtomicSub32(v,(int32_t)i)
 #define mmAtomicAddTestZeroP(v,i) mmAtomicAddTestZero32(v,(int32_t)i)
 #define mmAtomicSubTestZeroP(v,i) mmAtomicSubTestZero32(v,(int32_t)i)
 #define mmAtomicIncP(v) mmAtomicInc32(v)
 #define mmAtomicDecP(v) mmAtomicDec32(v)
 #define mmAtomicIncTestZeroP(v) mmAtomicIncTestZero32(v)
 #define mmAtomicDecTestZeroP(v) mmAtomicDecTestZero32(v)
 #define mmAtomicAddTestNegativeP(v,i) mmAtomicAddTestNegative32(v,(int32_t)i)
 #define mmAtomicSubTestNegativeP(v,i) mmAtomicSubTestNegative32(v,(int32_t)i)
 #define mmAtomicAndP(v,i) mmAtomicAnd32(v,(int32_t)i)
 #define mmAtomicAndTestZeroP(v,i) mmAtomicAndTestZero32(v,(int32_t)i)
 #define mmAtomicOrP(v,i) mmAtomicOr32(v,(int32_t)i)
 #define mmAtomicOrTestZeroP(v,i) mmAtomicOrTestZero32(v,(int32_t)i)
 #define mmAtomicXorP(v,i) mmAtomicXor32(v,(int32_t)i)
 #define mmAtomicXorTestZeroP(v,i) mmAtomicXorTestZero32(v,(int32_t)i)
 #define mmAtomicXchgP(v,i) (void *)mmAtomicXchg32(v,(int32_t)i)
 #define mmAtomicCmpXchgP(v,i,j) (void *)mmAtomicCmpXchg32(v,(int32_t)i,(int32_t)j)
 #define mmAtomicCmpReplaceP(v,i,j) mmAtomicCmpReplace32(v,(int32_t)i,(int32_t)j)
 #define mmAtomicSpinP(v,i,j) (void *)mmAtomicSpin32(v,(int32_t)i,(int32_t)j)
 #define mmAtomicAddReadP(v,i) (void *)mmAtomicAddRead32(v,(int32_t)i)
#else
 #error CPUCONF_POINTER_BITS undefined
#endif

#ifdef MM_ATOMIC_64_BITS_SUPPORT
 #define intlarge int64_t
 #define uintlarge uint64_t
 #define mmAtomicL mmAtomic64
 #define MM_ATOMIC_ACCESS_L(v) MM_ATOMIC_ACCESS_64(v)
 #define mmAtomicReadL(v) mmAtomicRead64(v)
 #define mmAtomicWriteL(v,i) mmAtomicWrite64(v,i)
 #define mmAtomicAddL(v,i) mmAtomicAdd64(v,i)
 #define mmAtomicSubL(v,i) mmAtomicSub64(v,i)
 #define mmAtomicAddTestZeroL(v,i) mmAtomicAddTestZero64(v,i)
 #define mmAtomicSubTestZeroL(v,i) mmAtomicSubTestZero64(v,i)
 #define mmAtomicIncL(v) mmAtomicInc64(v)
 #define mmAtomicDecL(v) mmAtomicDec64(v)
 #define mmAtomicIncTestZeroL(v) mmAtomicIncTestZero64(v)
 #define mmAtomicDecTestZeroL(v) mmAtomicDecTestZero64(v)
 #define mmAtomicAddTestNegativeL(v,i) mmAtomicAddTestNegative64(v,i)
 #define mmAtomicSubTestNegativeL(v,i) mmAtomicSubTestNegative64(v,i)
 #define mmAtomicAndL(v,i) mmAtomicAnd64(v,i)
 #define mmAtomicAndTestZeroL(v,i) mmAtomicAndTestZero64(v,i)
 #define mmAtomicOrL(v,i) mmAtomicOr64(v,i)
 #define mmAtomicOrTestZeroL(v,i) mmAtomicOrTestZero64(v,i)
 #define mmAtomicXorL(v,i) mmAtomicXor64(v,i)
 #define mmAtomicXorTestZeroL(v,i) mmAtomicXorTestZero64(v,i)
 #define mmAtomicXchgL(v,i) mmAtomicXchg64(v,i)
 #define mmAtomicCmpXchgL(v,i,j) mmAtomicCmpXchg64(v,i,j)
 #define mmAtomicCmpReplaceL(v,i,j) mmAtomicCmpReplace64(v,i,j)
 #define mmAtomicSpinL(v,i,j) mmAtomicSpin64(v,i,j)
 #define mmAtomicAddReadL(v,i) mmAtomicAddRead64(v,(int64_t)i)
#else
 #define intlarge int32_t
 #define uintlarge uint32_t
 #define mmAtomicL mmAtomic32
 #define MM_ATOMIC_ACCESS_L(v) MM_ATOMIC_ACCESS_32(v)
 #define mmAtomicReadL(v) mmAtomicRead32(v)
 #define mmAtomicWriteL(v,i) mmAtomicWrite32(v,i)
 #define mmAtomicAddL(v,i) mmAtomicAdd32(v,i)
 #define mmAtomicSubL(v,i) mmAtomicSub32(v,i)
 #define mmAtomicAddTestZeroL(v,i) mmAtomicAddTestZero32(v,i)
 #define mmAtomicSubTestZeroL(v,i) mmAtomicSubTestZero32(v,i)
 #define mmAtomicIncL(v) mmAtomicInc32(v)
 #define mmAtomicDecL(v) mmAtomicDec32(v)
 #define mmAtomicIncTestZeroL(v) mmAtomicIncTestZero32(v)
 #define mmAtomicDecTestZeroL(v) mmAtomicDecTestZero32(v)
 #define mmAtomicAddTestNegativeL(v,i) mmAtomicAddTestNegative32(v,i)
 #define mmAtomicSubTestNegativeL(v,i) mmAtomicSubTestNegative32(v,i)
 #define mmAtomicAndL(v,i) mmAtomicAnd32(v,i)
 #define mmAtomicAndTestZeroL(v,i) mmAtomicAndTestZero32(v,i)
 #define mmAtomicOrL(v,i) mmAtomicOr32(v,i)
 #define mmAtomicOrTestZeroL(v,i) mmAtomicOrTestZero32(v,i)
 #define mmAtomicXorL(v,i) mmAtomicXor32(v,i)
 #define mmAtomicXorTestZeroL(v,i) mmAtomicXorTestZero32(v,i)
 #define mmAtomicXchgL(v,i) mmAtomicXchg32(v,i)
 #define mmAtomicCmpXchgL(v,i,j) mmAtomicCmpXchg32(v,i,j)
 #define mmAtomicCmpReplaceL(v,i,j) mmAtomicCmpReplace32(v,i,j)
 #define mmAtomicSpinL(v,i,j) mmAtomicSpin32(v,i,j)
 #define mmAtomicAddReadL(v,i) mmAtomicAddRead32(v,(int32_t)i)
#endif



////////////////



typedef struct { mmAtomic8 v; } mmAtomicLock8;

#define MM_ATOMIC_LOCK8_WRITE (-((int8_t)0x7f))

static inline void mmAtomicLockInit8( mmAtomicLock8 *v )
{
  mmAtomicWrite8( &v->v, 0 );
  return;
}

static inline int mmAtomicLockAttemptRead8( mmAtomicLock8 *v )
{
  if( mmAtomicAddTestNegative8( &v->v, 1 ) )
  {
    mmAtomicAdd8( &v->v, -1 );
    return 0;
  }
  return 1;
}

static inline int mmAtomicLockAttemptWrite8( mmAtomicLock8 *v )
{
  if( mmAtomicCmpXchg8( &v->v, 0, MM_ATOMIC_LOCK8_WRITE ) )
    return 0;
  return 1;
}

static inline void mmAtomicLockSpinRead8( mmAtomicLock8 *v )
{
  for( ; ; )
  {
    for( ; mmAtomicRead8( &v->v ) < 0 ; )
      mmAtomicPause();
    if( !( mmAtomicAddTestNegative8( &v->v, 1 ) ) )
      break;
    mmAtomicAdd8( &v->v, -1 );
  }
  return;
}

static inline void mmAtomicLockSpinWrite8( mmAtomicLock8 *v )
{
  for( ; ; )
  {
    for( ; mmAtomicRead8( &v->v ) ; )
      mmAtomicPause();
    if( !( mmAtomicCmpXchg8( &v->v, 0, MM_ATOMIC_LOCK8_WRITE ) ) )
      break;
  }
  return;
}

static inline int mmAtomicLockTryRead8( mmAtomicLock8 *v, int spincount )
{
  do
  {
    if( mmAtomicRead8( &v->v ) < 0 )
      mmAtomicPause();
    else
    {
      if( !( mmAtomicAddTestNegative8( &v->v, 1 ) ) )
        return 1;
      mmAtomicAdd8( &v->v, -1 );
    }
  } while( --spincount );
  return 0;
}

static inline int mmAtomicLockTryWrite8( mmAtomicLock8 *v, int spincount )
{
  do
  {
    if( mmAtomicRead8( &v->v ) )
      mmAtomicPause();
    else
    {
      if( !( mmAtomicCmpXchg8( &v->v, 0, MM_ATOMIC_LOCK8_WRITE ) ) )
        return 1;
    }
  } while( --spincount );
  return 0;
}

static inline void mmAtomicLockDoneRead8( mmAtomicLock8 *v )
{
  mmAtomicAdd8( &v->v, -1 );
  return;
}

static inline void mmAtomicLockDoneWrite8( mmAtomicLock8 *v )
{
  mmAtomicAdd8( &v->v, -MM_ATOMIC_LOCK8_WRITE );
  return;
}


////


typedef struct { mmAtomic16 v; } mmAtomicLock16;

#define MM_ATOMIC_LOCK16_WRITE (-((int16_t)0x7fff))

static inline void mmAtomicLockInit16( mmAtomicLock16 *v )
{
  mmAtomicWrite16( &v->v, 0 );
  return;
}

static inline int mmAtomicLockAttemptRead16( mmAtomicLock16 *v )
{
  if( mmAtomicAddTestNegative16( &v->v, 1 ) )
  {
    mmAtomicAdd16( &v->v, -1 );
    return 0;
  }
  return 1;
}

static inline int mmAtomicLockAttemptWrite16( mmAtomicLock16 *v )
{
  if( mmAtomicCmpXchg16( &v->v, 0, MM_ATOMIC_LOCK16_WRITE ) )
    return 0;
  return 1;
}

static inline void mmAtomicLockSpinRead16( mmAtomicLock16 *v )
{
  for( ; ; )
  {
    for( ; mmAtomicRead16( &v->v ) < 0 ; )
      mmAtomicPause();
    if( !( mmAtomicAddTestNegative16( &v->v, 1 ) ) )
      break;
    mmAtomicAdd16( &v->v, -1 );
  }
  return;
}

static inline void mmAtomicLockSpinWrite16( mmAtomicLock16 *v )
{
  for( ; ; )
  {
    for( ; mmAtomicRead16( &v->v ) ; )
      mmAtomicPause();
    if( !( mmAtomicCmpXchg16( &v->v, 0, MM_ATOMIC_LOCK16_WRITE ) ) )
      break;
  }
  return;
}

static inline int mmAtomicLockTryRead16( mmAtomicLock16 *v, int spincount )
{
  do
  {
    if( mmAtomicRead16( &v->v ) < 0 )
      mmAtomicPause();
    else
    {
      if( !( mmAtomicAddTestNegative16( &v->v, 1 ) ) )
        return 1;
      mmAtomicAdd16( &v->v, -1 );
    }
  } while( --spincount );
  return 0;
}

static inline int mmAtomicLockTryWrite16( mmAtomicLock16 *v, int spincount )
{
  do
  {
    if( mmAtomicRead16( &v->v ) )
      mmAtomicPause();
    else
    {
      if( !( mmAtomicCmpXchg16( &v->v, 0, MM_ATOMIC_LOCK16_WRITE ) ) )
        return 1;
    }
  } while( --spincount );
  return 0;
}

static inline void mmAtomicLockDoneRead16( mmAtomicLock16 *v )
{
  mmAtomicAdd16( &v->v, -1 );
  return;
}

static inline void mmAtomicLockDoneWrite16( mmAtomicLock16 *v )
{
  mmAtomicAdd16( &v->v, -MM_ATOMIC_LOCK16_WRITE );
  return;
}


////


typedef struct { mmAtomic32 v; } mmAtomicLock32;

/*
#define MM_ATOMIC_LOCK32_WRITE (-((int32_t)0x7fffffff))
*/
#define MM_ATOMIC_LOCK32_WRITE (-((int32_t)0x10000000))

static inline void mmAtomicLockInit32( mmAtomicLock32 *v )
{
  mmAtomicWrite32( &v->v, 0 );
  return;
}

static inline int mmAtomicLockAttemptRead32( mmAtomicLock32 *v )
{
  if( mmAtomicAddTestNegative32( &v->v, 1 ) )
  {
    mmAtomicAdd32( &v->v, -1 );
    return 0;
  }
  return 1;
}

static inline int mmAtomicLockAttemptWrite32( mmAtomicLock32 *v )
{
  if( mmAtomicCmpXchg32( &v->v, 0, MM_ATOMIC_LOCK32_WRITE ) )
    return 0;
  return 1;
}

static inline void mmAtomicLockSpinRead32( mmAtomicLock32 *v )
{
  for( ; ; )
  {
    for( ; mmAtomicRead32( &v->v ) < 0 ; )
      mmAtomicPause();
    if( !( mmAtomicAddTestNegative32( &v->v, 1 ) ) )
      break;
    mmAtomicAdd32( &v->v, -1 );
  }
  return;
}

static inline void mmAtomicLockSpinWrite32( mmAtomicLock32 *v )
{
  for( ; ; )
  {
    for( ; mmAtomicRead32( &v->v ) ; )
      mmAtomicPause();
    if( !( mmAtomicCmpXchg32( &v->v, 0, MM_ATOMIC_LOCK32_WRITE ) ) )
      break;
  }
  return;
}

static inline int mmAtomicLockTryRead32( mmAtomicLock32 *v, int spincount )
{
  do
  {
    if( mmAtomicRead32( &v->v ) < 0 )
      mmAtomicPause();
    else
    {
      if( !( mmAtomicAddTestNegative32( &v->v, 1 ) ) )
        return 1;
      mmAtomicAdd32( &v->v, -1 );
    }
  } while( --spincount );
  return 0;
}

static inline int mmAtomicLockTryWrite32( mmAtomicLock32 *v, int spincount )
{
  do
  {
    if( mmAtomicRead32( &v->v ) )
      mmAtomicPause();
    else
    {
      if( !( mmAtomicCmpXchg32( &v->v, 0, MM_ATOMIC_LOCK32_WRITE ) ) )
        return 1;
    }
  } while( --spincount );
  return 0;
}

static inline void mmAtomicLockDoneRead32( mmAtomicLock32 *v )
{
  mmAtomicAdd32( &v->v, -1 );
  return;
}

static inline void mmAtomicLockDoneWrite32( mmAtomicLock32 *v )
{
  mmAtomicAdd32( &v->v, -MM_ATOMIC_LOCK32_WRITE );
  return;
}


////


#ifdef MM_ATOMIC_64_BITS_SUPPORT

typedef struct { mmAtomic64 v; } mmAtomicLock64;

#define MM_ATOMIC_LOCK64_WRITE (-((int64_t)0x7fffffffffffffff))

static inline void mmAtomicLockInit64( mmAtomicLock64 *v )
{
  mmAtomicWrite64( &v->v, 0 );
  return;
}

static inline int mmAtomicLockAttemptRead64( mmAtomicLock64 *v )
{
  if( mmAtomicAddTestNegative64( &v->v, 1 ) )
  {
    mmAtomicAdd64( &v->v, -1 );
    return 0;
  }
  return 1;
}

static inline int mmAtomicLockAttemptWrite64( mmAtomicLock64 *v )
{
  if( mmAtomicCmpXchg64( &v->v, 0, MM_ATOMIC_LOCK64_WRITE ) )
    return 0;
  return 1;
}

static inline void mmAtomicLockSpinRead64( mmAtomicLock64 *v )
{
  for( ; ; )
  {
    for( ; mmAtomicRead64( &v->v ) < 0 ; )
      mmAtomicPause();
    if( !( mmAtomicAddTestNegative64( &v->v, 1 ) ) )
      break;
    mmAtomicAdd64( &v->v, -1 );
  }
  return;
}

static inline void mmAtomicLockSpinWrite64( mmAtomicLock64 *v )
{
  for( ; ; )
  {
    for( ; mmAtomicRead64( &v->v ) ; )
      mmAtomicPause();
    if( !( mmAtomicCmpXchg64( &v->v, 0, MM_ATOMIC_LOCK64_WRITE ) ) )
      break;
  }
  return;
}

static inline int mmAtomicLockTryRead64( mmAtomicLock64 *v, int spincount )
{
  do
  {
    if( mmAtomicRead64( &v->v ) < 0 )
      mmAtomicPause();
    else
    {
      if( !( mmAtomicAddTestNegative64( &v->v, 1 ) ) )
        return 1;
      mmAtomicAdd64( &v->v, -1 );
    }
  } while( --spincount );
  return 0;
}

static inline int mmAtomicLockTryWrite64( mmAtomicLock64 *v, int spincount )
{
  do
  {
    if( mmAtomicRead64( &v->v ) )
      mmAtomicPause();
    else
    {
      if( !( mmAtomicCmpXchg64( &v->v, 0, MM_ATOMIC_LOCK64_WRITE ) ) )
        return 1;
    }
  } while( --spincount );
  return 0;
}

static inline void mmAtomicLockDoneRead64( mmAtomicLock64 *v )
{
  mmAtomicAdd64( &v->v, -1 );
  return;
}

static inline void mmAtomicLockDoneWrite64( mmAtomicLock64 *v )
{
  mmAtomicAdd64( &v->v, -MM_ATOMIC_LOCK64_WRITE );
  return;
}

#endif



////////////////



#define MM_ATOMIC_LIST_BUSY ((void *)0x1)

typedef struct
{
  mmAtomicP prev; /* mmAtomicP* to &(prev->next) */
  mmAtomicP next; /* void* to next */
  mmAtomic32 status;
} mmAtomicListNode;

#define MM_ATOMIC_LIST_VALID (0x0)
#define MM_ATOMIC_LIST_DELETED (0x1)

typedef struct
{
  mmAtomicP first; /* void* to item */
  mmAtomicP last; /* mmAtomicP* to &(lastitem->next) */
} mmAtomicListDualHead;


void mmAtomicListAdd( mmAtomicP *list, void *item, intptr_t offset );
void mmAtomicListRemove( void *item, intptr_t offset );

static inline void *mmAtomicListFirst( mmAtomicP *head )
{
  void *item;
  for( ; ( item = mmAtomicReadP( head ) ) == MM_ATOMIC_LIST_BUSY ; )
    mmAtomicPause();
  return item;
}

static inline void *mmAtomicListNext( mmAtomicListNode *node )
{
  void *item;
  for( ; ; )
  {
    item = mmAtomicReadP( &node->next );
    if( mmAtomicRead32( &node->status ) == MM_ATOMIC_LIST_DELETED )
      return 0;
    /* At the time we have read node->next, the node has not been deleted yet */
    if( item != MM_ATOMIC_LIST_BUSY )
      break;
    mmAtomicPause();
  }
  return item;
}


void mmAtomicListDualInit( mmAtomicListDualHead *head );
void mmAtomicListDualAddFirst( mmAtomicListDualHead *head, void *item, intptr_t offset );
void mmAtomicListDualAddLast( mmAtomicListDualHead *head, void *item, intptr_t offset );
void mmAtomicListDualRemove( mmAtomicListDualHead *head, void *item, intptr_t offset );

static inline void *mmAtomicListDualFirst( mmAtomicListDualHead *head )
{
  void *item;
  for( ; ( item = mmAtomicReadP( &head->first ) ) == MM_ATOMIC_LIST_BUSY ; )
    mmAtomicPause();
  return item;
}

static inline void *mmAtomicListDualNext( mmAtomicListNode *node )
{
  void *item;
  for( ; ; )
  {
    item = mmAtomicReadP( &node->next );
    if( mmAtomicRead32( &node->status ) == MM_ATOMIC_LIST_DELETED )
      return 0;
    /* At the time we have read node->next, the node has not been deleted yet */
    if( item != MM_ATOMIC_LIST_BUSY )
      break;
    mmAtomicPause();
  }
  return item;
}



////////////////



/*
#define MM_ATOMIC_BARRIER_DEBUG
*/


#define MM_ATOMIC_BARRIER_DELAYED_RESET


typedef struct
{
  int32_t clearcounter;
  int32_t yieldcounter;
} mmAtomicBarrierStat;

typedef struct
{
  mmAtomic32 flag MM_CACHE_ALIGN;
  mmAtomic32 counter MM_CACHE_ALIGN;
  volatile int32_t flagref MM_CACHE_ALIGN;
  void *parent MM_CACHE_ALIGN;
  int32_t resetvalue;
} mmAtomicBarrier;

void mmAtomicBarrierBuild( mmAtomicBarrier *barrier, int childcount, mmAtomicBarrier *parent );
int mmAtomicBarrierWait( mmAtomicBarrier *barrier, int32_t spinwaitcounter, mmAtomicBarrierStat *barrierstat );




////



typedef struct
{
  mmAtomic32 counter MM_CACHE_ALIGN;
  /* Data below remains constant */
  void *parent MM_CACHE_ALIGN;
  int32_t resetvalue;
} mmAtomicCounterNode;

typedef struct
{
  int32_t lockcount;
  int32_t nodecount;
  mmAtomicCounterNode *nodearray;
  mmAtomicCounterNode **locknode;
} mmAtomicCounter;

void mmAtomicCounterInit( mmAtomicCounter *counter, int lockcount, int stagesize );
void mmAtomicCounterDestroy( mmAtomicCounter *counter );
int mmAtomicCounterHit( mmAtomicCounter *counter, int lockindex );



////


/*
Stockpile callbacks for freeing items

*/

typedef struct
{
  mmAtomicP p;


} mmAtomicRcu;

static inline void mmAtomicRcuEnter( mmAtomicRcu *rcu )
{
  /* On archs with messed up memory models like Alpha, we would need some memory barrier here */
  return;
}

static inline void mmAtomicRcuLeave( mmAtomicRcu *rcu )
{
  /* On archs with messed up memory models like Alpha, we would need some memory barrier here */
  return;
}


/*
To reiterate, synchronize_rcu() waits only for ongoing RCU read-side critical sections to complete,
not necessarily for any that begin after synchronize_rcu() is invoked.
*/
static inline void mmAtomicRcuSync()
{

}

static inline void mmAtomicRcuRead()
{


}


static inline void mmAtomicRcuWrite()
{


}



#endif


