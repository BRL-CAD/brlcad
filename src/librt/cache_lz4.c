/*
   LZ4 - Fast LZ compression algorithm
   Copyright (C) 2011-2016, Yann Collet.

   BSD 2-Clause License (http://www.opensource.org/licenses/bsd-license.php)

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:

       * Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
       * Redistributions in binary form must reproduce the above
   copyright notice, this list of conditions and the following disclaimer
   in the documentation and/or other materials provided with the
   distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   You can contact the author at :
    - LZ4 homepage : http://www.lz4.org
    - LZ4 source repository : https://github.com/lz4/lz4
*/

#include "common.h"

#include <stddef.h>
#include <string.h>

#define brl_LZ4_MEMORY_USAGE 14
#define brl_LZ4_MAX_INPUT_SIZE        0x7E000000   /* 2 113 929 216 bytes */
#define brl_LZ4_COMPRESSBOUND(isize)  ((unsigned)(isize) > (unsigned)brl_LZ4_MAX_INPUT_SIZE ? 0 : (isize) + ((isize)/255) + 16)

int brl_LZ4_compressBound(int isize)  { return brl_LZ4_COMPRESSBOUND(isize); }

/*-************************************
*  Compiler Options
**************************************/
#ifdef _MSC_VER    /* Visual Studio */
#  define FORCE_INLINE static __forceinline
#  include <intrin.h>
#  pragma warning(disable : 4127)        /* disable: C4127: conditional expression is constant */
#  pragma warning(disable : 4293)        /* disable: C4293: too large shift (32-bits) */
#else
#  if defined(__GNUC__) || defined(__clang__)
#    define FORCE_INLINE static inline __attribute__((always_inline))
#  elif defined(__cplusplus) || (defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L) /* C99 */)
#    define FORCE_INLINE static inline
#  else
#    define FORCE_INLINE static
#  endif
#endif  /* _MSC_VER */

#if (defined(__GNUC__) && (__GNUC__ >= 3)) || (defined(__INTEL_COMPILER) && (__INTEL_COMPILER >= 800)) || defined(__clang__)
#  define expect(expr,value)    (__builtin_expect ((expr),(value)) )
#else
#  define expect(expr,value)    (expr)
#endif

#define likely(expr)     expect((expr) != 0, 1)
#define unlikely(expr)   expect((expr) != 0, 0)


/*-************************************
*  Basic Types
**************************************/
#if defined(__cplusplus) || (defined (__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L) /* C99 */)
# include <stdint.h>
  typedef  uint8_t BYTE;
  typedef uint16_t U16;
  typedef uint32_t U32;
  typedef  int32_t S32;
  typedef uint64_t U64;
  typedef uintptr_t uptrval;
#else
  typedef unsigned char       BYTE;
  typedef unsigned short      U16;
  typedef unsigned int        U32;
  typedef   signed int        S32;
  typedef unsigned long long  U64;
  typedef size_t              uptrval;   /* generally true, except OpenVMS-64 */
#endif

#if defined(__x86_64__)
  typedef U64    reg_t;   /* 64-bits in x32 mode */
#else
  typedef size_t reg_t;   /* 32-bits in x32 mode */
#endif

/*-************************************
*  Reading and writing into memory
**************************************/
static unsigned brl_LZ4_isLittleEndian(void)
{
    const union { U32 u; BYTE c[4]; } one = { 1 };   /* don't use static : performance detrimental */
    return one.c[0];
}

static U16 brl_LZ4_read16(const void* memPtr)
{
    U16 val; memcpy(&val, memPtr, sizeof(val)); return val;
}

static U32 brl_LZ4_read32(const void* memPtr)
{
    U32 val; memcpy(&val, memPtr, sizeof(val)); return val;
}

static reg_t brl_LZ4_read_ARCH(const void* memPtr)
{
    reg_t val; memcpy(&val, memPtr, sizeof(val)); return val;
}

static void brl_LZ4_write16(void* memPtr, U16 value)
{
    memcpy(memPtr, &value, sizeof(value));
}

static void brl_LZ4_write32(void* memPtr, U32 value)
{
    memcpy(memPtr, &value, sizeof(value));
}

static U16 brl_LZ4_readLE16(const void* memPtr)
{
    if (brl_LZ4_isLittleEndian()) {
        return brl_LZ4_read16(memPtr);
    } else {
        const BYTE* p = (const BYTE*)memPtr;
        return (U16)((U16)p[0] + (p[1]<<8));
    }
}

static void brl_LZ4_writeLE16(void* memPtr, U16 value)
{
    if (brl_LZ4_isLittleEndian()) {
        brl_LZ4_write16(memPtr, value);
    } else {
        BYTE* p = (BYTE*)memPtr;
        p[0] = (BYTE) value;
        p[1] = (BYTE)(value>>8);
    }
}

static void brl_LZ4_copy8(void* dst, const void* src)
{
    memcpy(dst,src,8);
}

/* customized variant of memcpy, which can overwrite up to 8 bytes beyond dstEnd */
static void brl_LZ4_wildCopy(void* dstPtr, const void* srcPtr, void* dstEnd)
{
    BYTE* d = (BYTE*)dstPtr;
    const BYTE* s = (const BYTE*)srcPtr;
    BYTE* const e = (BYTE*)dstEnd;

    do { brl_LZ4_copy8(d,s); d+=8; s+=8; } while (d<e);
}


/*-************************************
*  Common Constants
**************************************/
#define MINMATCH 4

#define WILDCOPYLENGTH 8
#define LASTLITERALS 5
#define MFLIMIT (WILDCOPYLENGTH+MINMATCH)
static const int brl_LZ4_minLength = (MFLIMIT+1);

#define KB *(1 <<10)
#define MB *(1 <<20)
#define GB *(1U<<30)

#define MAXD_LOG 16
#define MAX_DISTANCE ((1 << MAXD_LOG) - 1)

#define ML_BITS  4
#define ML_MASK  ((1U<<ML_BITS)-1)
#define RUN_BITS (8-ML_BITS)
#define RUN_MASK ((1U<<RUN_BITS)-1)


/*-************************************
*  Common Utils
**************************************/
#define brl_LZ4_STATIC_ASSERT(c)    { enum { brl_LZ4_static_assert = 1/(int)(!!(c)) }; }   /* use only *after* variable declarations */


/*-************************************
*  Common functions
**************************************/
static unsigned brl_LZ4_NbCommonBytes (register reg_t val)
{
    if (brl_LZ4_isLittleEndian()) {
        if (sizeof(val)==8) {
#       if defined(_MSC_VER) && defined(_WIN64) && !defined(brl_LZ4_FORCE_SW_BITCOUNT)
            unsigned long r = 0;
            _BitScanForward64( &r, (U64)val );
            return (int)(r>>3);
#       elif (defined(__clang__) || (defined(__GNUC__) && (__GNUC__>=3))) && !defined(brl_LZ4_FORCE_SW_BITCOUNT)
            return (__builtin_ctzll((U64)val) >> 3);
#       else
            static const int DeBruijnBytePos[64] = { 0, 0, 0, 0, 0, 1, 1, 2, 0, 3, 1, 3, 1, 4, 2, 7, 0, 2, 3, 6, 1, 5, 3, 5, 1, 3, 4, 4, 2, 5, 6, 7, 7, 0, 1, 2, 3, 3, 4, 6, 2, 6, 5, 5, 3, 4, 5, 6, 7, 1, 2, 4, 6, 4, 4, 5, 7, 2, 6, 5, 7, 6, 7, 7 };
            return DeBruijnBytePos[((U64)((val & -(long long)val) * 0x0218A392CDABBD3FULL)) >> 58];
#       endif
        } else /* 32 bits */ {
#       if defined(_MSC_VER) && !defined(brl_LZ4_FORCE_SW_BITCOUNT)
            unsigned long r;
            _BitScanForward( &r, (U32)val );
            return (int)(r>>3);
#       elif (defined(__clang__) || (defined(__GNUC__) && (__GNUC__>=3))) && !defined(brl_LZ4_FORCE_SW_BITCOUNT)
            return (__builtin_ctz((U32)val) >> 3);
#       else
            static const int DeBruijnBytePos[32] = { 0, 0, 3, 0, 3, 1, 3, 0, 3, 2, 2, 1, 3, 2, 0, 1, 3, 3, 1, 2, 2, 2, 2, 0, 3, 1, 2, 0, 1, 0, 1, 1 };
            return DeBruijnBytePos[((U32)((val & -(S32)val) * 0x077CB531U)) >> 27];
#       endif
        }
    } else   /* Big Endian CPU */ {
        if (sizeof(val)==8) {
#       if defined(_MSC_VER) && defined(_WIN64) && !defined(brl_LZ4_FORCE_SW_BITCOUNT)
            unsigned long r = 0;
            _BitScanReverse64( &r, val );
            return (unsigned)(r>>3);
#       elif (defined(__clang__) || (defined(__GNUC__) && (__GNUC__>=3))) && !defined(brl_LZ4_FORCE_SW_BITCOUNT)
            return (__builtin_clzll((U64)val) >> 3);
#       else
            unsigned r;
            if (!(val>>32)) { r=4; } else { r=0; val>>=32; }
            if (!(val>>16)) { r+=2; val>>=8; } else { val>>=24; }
            r += (!val);
            return r;
#       endif
        } else /* 32 bits */ {
#       if defined(_MSC_VER) && !defined(brl_LZ4_FORCE_SW_BITCOUNT)
            unsigned long r = 0;
            _BitScanReverse( &r, (unsigned long)val );
            return (unsigned)(r>>3);
#       elif (defined(__clang__) || (defined(__GNUC__) && (__GNUC__>=3))) && !defined(brl_LZ4_FORCE_SW_BITCOUNT)
            return (__builtin_clz((U32)val) >> 3);
#       else
            unsigned r;
            if (!(val>>16)) { r=2; val>>=8; } else { r=0; val>>=24; }
            r += (!val);
            return r;
#       endif
        }
    }
}

#define STEPSIZE sizeof(reg_t)
static unsigned brl_LZ4_count(const BYTE* pIn, const BYTE* pMatch, const BYTE* pInLimit)
{
    const BYTE* const pStart = pIn;

    while (likely(pIn<pInLimit-(STEPSIZE-1))) {
        reg_t const diff = brl_LZ4_read_ARCH(pMatch) ^ brl_LZ4_read_ARCH(pIn);
        if (!diff) { pIn+=STEPSIZE; pMatch+=STEPSIZE; continue; }
        pIn += brl_LZ4_NbCommonBytes(diff);
        return (unsigned)(pIn - pStart);
    }

    if ((STEPSIZE==8) && (pIn<(pInLimit-3)) && (brl_LZ4_read32(pMatch) == brl_LZ4_read32(pIn))) { pIn+=4; pMatch+=4; }
    if ((pIn<(pInLimit-1)) && (brl_LZ4_read16(pMatch) == brl_LZ4_read16(pIn))) { pIn+=2; pMatch+=2; }
    if ((pIn<pInLimit) && (*pMatch == *pIn)) pIn++;
    return (unsigned)(pIn - pStart);
}


/*-************************************
*  Local Constants
**************************************/
static const int brl_LZ4_64Klimit = ((64 KB) + (MFLIMIT-1));
static const U32 brl_LZ4_skipTrigger = 6;  /* Increase this value ==> compression run slower on incompressible data */


/*-************************************
*  Local Structures and types
**************************************/
typedef enum { notLimited = 0, limitedOutput = 1 } limitedOutput_directive;
typedef enum { byPtr, byU32, byU16 } tableType_t;

typedef enum { noDict = 0, withPrefix64k, usingExtDict } dict_directive;
typedef enum { noDictIssue = 0, dictSmall } dictIssue_directive;

typedef enum { endOnOutputSize = 0, endOnInputSize = 1 } endCondition_directive;
typedef enum { full = 0, partial = 1 } earlyEnd_directive;

/*-******************************
*  Compression functions
********************************/
#define brl_LZ4_MEMORY_USAGE 14
#define brl_LZ4_HASHLOG   (brl_LZ4_MEMORY_USAGE-2)
static U32 brl_LZ4_hash4(U32 sequence, tableType_t const tableType)
{
    if (tableType == byU16)
        return ((sequence * 2654435761U) >> ((MINMATCH*8)-(brl_LZ4_HASHLOG+1)));
    else
        return ((sequence * 2654435761U) >> ((MINMATCH*8)-brl_LZ4_HASHLOG));
}

static U32 brl_LZ4_hash5(U64 sequence, tableType_t const tableType)
{
    static const U64 prime5bytes = 889523592379ULL;
    static const U64 prime8bytes = 11400714785074694791ULL;
    const U32 hashLog = (tableType == byU16) ? brl_LZ4_HASHLOG+1 : brl_LZ4_HASHLOG;
    if (brl_LZ4_isLittleEndian())
        return (U32)(((sequence << 24) * prime5bytes) >> (64 - hashLog));
    else
        return (U32)(((sequence >> 24) * prime8bytes) >> (64 - hashLog));
}

FORCE_INLINE U32 brl_LZ4_hashPosition(const void* const p, tableType_t const tableType)
{
    if ((sizeof(reg_t)==8) && (tableType != byU16)) return brl_LZ4_hash5(brl_LZ4_read_ARCH(p), tableType);
    return brl_LZ4_hash4(brl_LZ4_read32(p), tableType);
}

static void brl_LZ4_putPositionOnHash(const BYTE* p, U32 h, void* tableBase, tableType_t const tableType, const BYTE* srcBase)
{
    switch (tableType)
    {
    case byPtr: { const BYTE** hashTable = (const BYTE**)tableBase; hashTable[h] = p; return; }
    case byU32: { U32* hashTable = (U32*) tableBase; hashTable[h] = (U32)(p-srcBase); return; }
    case byU16: { U16* hashTable = (U16*) tableBase; hashTable[h] = (U16)(p-srcBase); return; }
    }
}

FORCE_INLINE void brl_LZ4_putPosition(const BYTE* p, void* tableBase, tableType_t tableType, const BYTE* srcBase)
{
    U32 const h = brl_LZ4_hashPosition(p, tableType);
    brl_LZ4_putPositionOnHash(p, h, tableBase, tableType, srcBase);
}

static const BYTE* brl_LZ4_getPositionOnHash(U32 h, void* tableBase, tableType_t tableType, const BYTE* srcBase)
{
    if (tableType == byPtr) { const BYTE** hashTable = (const BYTE**) tableBase; return hashTable[h]; }
    if (tableType == byU32) { const U32* const hashTable = (U32*) tableBase; return hashTable[h] + srcBase; }
    { const U16* const hashTable = (U16*) tableBase; return hashTable[h] + srcBase; }   /* default, to ensure a return */
}

FORCE_INLINE const BYTE* brl_LZ4_getPosition(const BYTE* p, void* tableBase, tableType_t tableType, const BYTE* srcBase)
{
    U32 const h = brl_LZ4_hashPosition(p, tableType);
    return brl_LZ4_getPositionOnHash(h, tableBase, tableType, srcBase);
}

#define brl_LZ4_HASH_SIZE_U32 (1 << brl_LZ4_HASHLOG)
typedef struct {
    uint32_t hashTable[brl_LZ4_HASH_SIZE_U32];
    uint32_t currentOffset;
    uint32_t initCheck;
    const uint8_t* dictionary;
    uint8_t* bufferStart;   /* obsolete, used for slideInputBuffer */
    uint32_t dictSize;
} brl_LZ4_stream_t_internal;

/** brl_LZ4_compress_generic() :
    inlined, to ensure branches are decided at compilation time */
FORCE_INLINE int brl_LZ4_compress_generic(
                 brl_LZ4_stream_t_internal* const cctx,
                 const char* const source,
                 char* const dest,
                 const int inputSize,
                 const int maxOutputSize,
                 const limitedOutput_directive outputLimited,
                 const tableType_t tableType,
                 const dict_directive dict,
                 const dictIssue_directive dictIssue,
                 const U32 acceleration)
{
    const BYTE* ip = (const BYTE*) source;
    const BYTE* base;
    const BYTE* lowLimit;
    const BYTE* const lowRefLimit = ip - cctx->dictSize;
    const BYTE* const dictionary = cctx->dictionary;
    const BYTE* const dictEnd = dictionary + cctx->dictSize;
    const ptrdiff_t dictDelta = dictEnd - (const BYTE*)source;
    const BYTE* anchor = (const BYTE*) source;
    const BYTE* const iend = ip + inputSize;
    const BYTE* const mflimit = iend - MFLIMIT;
    const BYTE* const matchlimit = iend - LASTLITERALS;

    BYTE* op = (BYTE*) dest;
    BYTE* const olimit = op + maxOutputSize;

    U32 forwardH;

    /* Init conditions */
    if ((U32)inputSize > (U32)brl_LZ4_MAX_INPUT_SIZE) return 0;   /* Unsupported inputSize, too large (or negative) */
    switch(dict)
    {
    case noDict:
    default:
        base = (const BYTE*)source;
        lowLimit = (const BYTE*)source;
        break;
    case withPrefix64k:
        base = (const BYTE*)source - cctx->currentOffset;
        lowLimit = (const BYTE*)source - cctx->dictSize;
        break;
    case usingExtDict:
        base = (const BYTE*)source - cctx->currentOffset;
        lowLimit = (const BYTE*)source;
        break;
    }
    if ((tableType == byU16) && (inputSize>=brl_LZ4_64Klimit)) return 0;   /* Size too large (not within 64K limit) */
    if (inputSize<brl_LZ4_minLength) goto _last_literals;                  /* Input too small, no compression (all literals) */

    /* First Byte */
    brl_LZ4_putPosition(ip, cctx->hashTable, tableType, base);
    ip++; forwardH = brl_LZ4_hashPosition(ip, tableType);

    /* Main Loop */
    for ( ; ; ) {
        ptrdiff_t refDelta = 0;
        const BYTE* match;
        BYTE* token;

        /* Find a match */
        {   const BYTE* forwardIp = ip;
            unsigned step = 1;
            unsigned searchMatchNb = acceleration << brl_LZ4_skipTrigger;
            do {
                U32 const h = forwardH;
                ip = forwardIp;
                forwardIp += step;
                step = (searchMatchNb++ >> brl_LZ4_skipTrigger);

                if (unlikely(forwardIp > mflimit)) goto _last_literals;

                match = brl_LZ4_getPositionOnHash(h, cctx->hashTable, tableType, base);
                if (dict==usingExtDict) {
                    if (match < (const BYTE*)source) {
                        refDelta = dictDelta;
                        lowLimit = dictionary;
                    } else {
                        refDelta = 0;
                        lowLimit = (const BYTE*)source;
                }   }
                forwardH = brl_LZ4_hashPosition(forwardIp, tableType);
                brl_LZ4_putPositionOnHash(ip, h, cctx->hashTable, tableType, base);

            } while ( ((dictIssue==dictSmall) ? (match < lowRefLimit) : 0)
                || ((tableType==byU16) ? 0 : (match + MAX_DISTANCE < ip))
                || (brl_LZ4_read32(match+refDelta) != brl_LZ4_read32(ip)) );
        }

        /* Catch up */
        while (((ip>anchor) & (match+refDelta > lowLimit)) && (unlikely(ip[-1]==match[refDelta-1]))) { ip--; match--; }

        /* Encode Literals */
        {   unsigned const litLength = (unsigned)(ip - anchor);
            token = op++;
            if ((outputLimited) &&  /* Check output buffer overflow */
                (unlikely(op + litLength + (2 + 1 + LASTLITERALS) + (litLength/255) > olimit)))
                return 0;
            if (litLength >= RUN_MASK) {
                int len = (int)litLength-RUN_MASK;
                *token = (RUN_MASK<<ML_BITS);
                for(; len >= 255 ; len-=255) *op++ = 255;
                *op++ = (BYTE)len;
            }
            else *token = (BYTE)(litLength<<ML_BITS);

            /* Copy Literals */
            brl_LZ4_wildCopy(op, anchor, op+litLength);
            op+=litLength;
        }

_next_match:
        /* Encode Offset */
        brl_LZ4_writeLE16(op, (U16)(ip-match)); op+=2;

        /* Encode MatchLength */
        {   unsigned matchCode;

            if ((dict==usingExtDict) && (lowLimit==dictionary)) {
                const BYTE* limit;
                match += refDelta;
                limit = ip + (dictEnd-match);
                if (limit > matchlimit) limit = matchlimit;
                matchCode = brl_LZ4_count(ip+MINMATCH, match+MINMATCH, limit);
                ip += MINMATCH + matchCode;
                if (ip==limit) {
                    unsigned const more = brl_LZ4_count(ip, (const BYTE*)source, matchlimit);
                    matchCode += more;
                    ip += more;
                }
            } else {
                matchCode = brl_LZ4_count(ip+MINMATCH, match+MINMATCH, matchlimit);
                ip += MINMATCH + matchCode;
            }

            if ( outputLimited &&    /* Check output buffer overflow */
                (unlikely(op + (1 + LASTLITERALS) + (matchCode>>8) > olimit)) )
                return 0;
            if (matchCode >= ML_MASK) {
                *token += ML_MASK;
                matchCode -= ML_MASK;
                brl_LZ4_write32(op, 0xFFFFFFFF);
                while (matchCode >= 4*255) op+=4, brl_LZ4_write32(op, 0xFFFFFFFF), matchCode -= 4*255;
                op += matchCode / 255;
                *op++ = (BYTE)(matchCode % 255);
            } else
                *token += (BYTE)(matchCode);
        }

        anchor = ip;

        /* Test end of chunk */
        if (ip > mflimit) break;

        /* Fill table */
        brl_LZ4_putPosition(ip-2, cctx->hashTable, tableType, base);

        /* Test next position */
        match = brl_LZ4_getPosition(ip, cctx->hashTable, tableType, base);
        if (dict==usingExtDict) {
            if (match < (const BYTE*)source) {
                refDelta = dictDelta;
                lowLimit = dictionary;
            } else {
                refDelta = 0;
                lowLimit = (const BYTE*)source;
        }   }
        brl_LZ4_putPosition(ip, cctx->hashTable, tableType, base);
        if ( ((dictIssue==dictSmall) ? (match>=lowRefLimit) : 1)
            && (match+MAX_DISTANCE>=ip)
            && (brl_LZ4_read32(match+refDelta)==brl_LZ4_read32(ip)) )
        { token=op++; *token=0; goto _next_match; }

        /* Prepare next loop */
        forwardH = brl_LZ4_hashPosition(++ip, tableType);
    }

_last_literals:
    /* Encode Last Literals */
    {   size_t const lastRun = (size_t)(iend - anchor);
        if ( (outputLimited) &&  /* Check output buffer overflow */
            ((op - (BYTE*)dest) + lastRun + 1 + ((lastRun+255-RUN_MASK)/255) > (U32)maxOutputSize) )
            return 0;
        if (lastRun >= RUN_MASK) {
            size_t accumulator = lastRun - RUN_MASK;
            *op++ = RUN_MASK << ML_BITS;
            for(; accumulator >= 255 ; accumulator-=255) *op++ = 255;
            *op++ = (BYTE) accumulator;
        } else {
            *op++ = (BYTE)(lastRun<<ML_BITS);
        }
        memcpy(op, anchor, lastRun);
        op += lastRun;
    }

    /* End */
    return (int) (((char*)op)-dest);
}

typedef union brl_LZ4_stream_u brl_LZ4_stream_t;
#define brl_LZ4_STREAMSIZE_U64 ((1 << (brl_LZ4_MEMORY_USAGE-3)) + 4)
#define brl_LZ4_STREAMSIZE     (brl_LZ4_STREAMSIZE_U64 * sizeof(unsigned long long))
union brl_LZ4_stream_u {
    unsigned long long table[brl_LZ4_STREAMSIZE_U64];
    brl_LZ4_stream_t_internal internal_donotuse;
};

int brl_LZ4_compress_default(const char* source, char* dest, int inputSize, int maxOutputSize)
{
    brl_LZ4_stream_t ctx_s;
    brl_LZ4_stream_t_internal* ctx = &ctx_s.internal_donotuse;
    memset(&ctx_s, 0, sizeof(brl_LZ4_stream_t));

    if (maxOutputSize >= brl_LZ4_compressBound(inputSize)) {
        if (inputSize < brl_LZ4_64Klimit)
            return brl_LZ4_compress_generic(ctx, source, dest, inputSize,             0,    notLimited,                        byU16, noDict, noDictIssue, 1);
        else
            return brl_LZ4_compress_generic(ctx, source, dest, inputSize,             0,    notLimited, (sizeof(void*)==8) ? byU32 : byPtr, noDict, noDictIssue, 1);
    } else {
        if (inputSize < brl_LZ4_64Klimit)
            return brl_LZ4_compress_generic(ctx, source, dest, inputSize, maxOutputSize, limitedOutput,                        byU16, noDict, noDictIssue, 1);
        else
            return brl_LZ4_compress_generic(ctx, source, dest, inputSize, maxOutputSize, limitedOutput, (sizeof(void*)==8) ? byU32 : byPtr, noDict, noDictIssue, 1);
    }
}

/*-*****************************
*  Decompression functions
*******************************/
/*! brl_LZ4_decompress_generic() :
 *  This generic decompression function cover all use cases.
 *  It shall be instantiated several times, using different sets of directives
 *  Note that it is important this generic function is really inlined,
 *  in order to remove useless branches during compilation optimization.
 */
FORCE_INLINE int brl_LZ4_decompress_generic(
                 const char* const source,
                 char* const dest,
                 int inputSize,
                 int outputSize,         /* If endOnInput==endOnInputSize, this value is the max size of Output Buffer. */

                 int endOnInput,         /* endOnOutputSize, endOnInputSize */
                 int partialDecoding,    /* full, partial */
                 int targetOutputSize,   /* only used if partialDecoding==partial */
                 int dict,               /* noDict, withPrefix64k, usingExtDict */
                 const BYTE* const lowPrefix,  /* == dest when no prefix */
                 const BYTE* const dictStart,  /* only if dict==usingExtDict */
                 const size_t dictSize         /* note : = 0 if noDict */
                 )
{
    if (source == NULL || outputSize < 0)
        return -1;

    /* Local Variables */
    const BYTE* ip = (const BYTE*) source;
    const BYTE* const iend = ip + inputSize;

    BYTE* op = (BYTE*) dest;
    BYTE* const oend = op + outputSize;
    BYTE* cpy;
    BYTE* oexit = op + targetOutputSize;
    const BYTE* const lowLimit = lowPrefix - dictSize;

    const BYTE* const dictEnd = (const BYTE*)dictStart + dictSize;
    const unsigned dec32table[] = {0, 1, 2, 1, 4, 4, 4, 4};
    const int dec64table[] = {0, 0, 0, -1, 0, 1, 2, 3};

    const int safeDecode = (endOnInput==endOnInputSize);
    const int checkOffset = ((safeDecode) && (dictSize < (int)(64 KB)));


    /* Special cases */
    if ((partialDecoding) && (oexit > oend-MFLIMIT)) oexit = oend-MFLIMIT;                        /* targetOutputSize too high => decode everything */
    if ((endOnInput) && (unlikely(outputSize==0))) return ((inputSize==1) && (*ip==0)) ? 0 : -1;  /* Empty output buffer */
    if ((!endOnInput) && (unlikely(outputSize==0))) return (*ip==0?1:-1);

    /* Main Loop : decode sequences */
    while (1) {
        size_t length;
        const BYTE* match;
        size_t offset;

        /* get literal length */
        unsigned const token = *ip++;
        if ((length=(token>>ML_BITS)) == RUN_MASK) {
            unsigned s;
            do {
                s = *ip++;
                length += s;
            } while ( likely(endOnInput ? ip<iend-RUN_MASK : 1) & (s==255) );
            if ((safeDecode) && unlikely((uptrval)(op)+length<(uptrval)(op))) goto _output_error;   /* overflow detection */
            if ((safeDecode) && unlikely((uptrval)(ip)+length<(uptrval)(ip))) goto _output_error;   /* overflow detection */
        }

        /* copy literals */
        cpy = op+length;
        if ( ((endOnInput) && ((cpy>(partialDecoding?oexit:oend-MFLIMIT)) || (ip+length>iend-(2+1+LASTLITERALS))) )
            || ((!endOnInput) && (cpy>oend-WILDCOPYLENGTH)) )
        {
            if (partialDecoding) {
                if (cpy > oend) goto _output_error;                           /* Error : write attempt beyond end of output buffer */
                if ((endOnInput) && (ip+length > iend)) goto _output_error;   /* Error : read attempt beyond end of input buffer */
            } else {
                if ((!endOnInput) && (cpy != oend)) goto _output_error;       /* Error : block decoding must stop exactly there */
                if ((endOnInput) && ((ip+length != iend) || (cpy > oend))) goto _output_error;   /* Error : input must be consumed */
            }
            memcpy(op, ip, length);
            ip += length;
            op += length;
            break;     /* Necessarily EOF, due to parsing restrictions */
        }
        brl_LZ4_wildCopy(op, ip, cpy);
        ip += length; op = cpy;

        /* get offset */
        offset = brl_LZ4_readLE16(ip); ip+=2;
        match = op - offset;
        if ((checkOffset) && (unlikely(match < lowLimit))) goto _output_error;   /* Error : offset outside buffers */
        brl_LZ4_write32(op, (U32)offset);   /* costs ~1%; silence an msan warning when offset==0 */

        /* get matchlength */
        length = token & ML_MASK;
        if (length == ML_MASK) {
            unsigned s;
            do {
                s = *ip++;
                if ((endOnInput) && (ip > iend-LASTLITERALS)) goto _output_error;
                length += s;
            } while (s==255);
            if ((safeDecode) && unlikely((uptrval)(op)+length<(uptrval)op)) goto _output_error;   /* overflow detection */
        }
        length += MINMATCH;

        /* check external dictionary */
        if ((dict==usingExtDict) && (match < lowPrefix)) {
            if (unlikely(op+length > oend-LASTLITERALS)) goto _output_error;   /* doesn't respect parsing restriction */

            if (length <= (size_t)(lowPrefix-match)) {
                /* match can be copied as a single segment from external dictionary */
                memmove(op, dictEnd - (lowPrefix-match), length);
                op += length;
            } else {
                /* match encompass external dictionary and current block */
                size_t const copySize = (size_t)(lowPrefix-match);
                size_t const restSize = length - copySize;
                memcpy(op, dictEnd - copySize, copySize);
                op += copySize;
                if (restSize > (size_t)(op-lowPrefix)) {  /* overlap copy */
                    BYTE* const endOfMatch = op + restSize;
                    const BYTE* copyFrom = lowPrefix;
                    while (op < endOfMatch) *op++ = *copyFrom++;
                } else {
                    memcpy(op, lowPrefix, restSize);
                    op += restSize;
            }   }
            continue;
        }

        /* copy match within block */
        cpy = op + length;
        if (unlikely(offset<8)) {
            const int dec64 = dec64table[offset];
            op[0] = match[0];
            op[1] = match[1];
            op[2] = match[2];
            op[3] = match[3];
            match += dec32table[offset];
            memcpy(op+4, match, 4);
            match -= dec64;
        } else { brl_LZ4_copy8(op, match); match+=8; }
        op += 8;

        if (unlikely(cpy>oend-12)) {
            BYTE* const oCopyLimit = oend-(WILDCOPYLENGTH-1);
            if (cpy > oend-LASTLITERALS) goto _output_error;    /* Error : last LASTLITERALS bytes must be literals (uncompressed) */
            if (op < oCopyLimit) {
                brl_LZ4_wildCopy(op, match, oCopyLimit);
                match += oCopyLimit - op;
                op = oCopyLimit;
            }
            while (op<cpy) *op++ = *match++;
        } else {
            brl_LZ4_copy8(op, match);
            if (length>16) brl_LZ4_wildCopy(op+8, match+8, cpy);
        }
        op=cpy;   /* correction */
    }

    /* end of decoding */
    if (endOnInput)
       return (int) (((char*)op)-dest);     /* Nb of output bytes decoded */
    else
       return (int) (((const char*)ip)-source);   /* Nb of input bytes read */

    /* Overflow error detected */
_output_error:
    return (int) (-(((const char*)ip)-source))-1;
}

int brl_LZ4_decompress_fast(const char* source, char* dest, int originalSize)
{
    return brl_LZ4_decompress_generic(source, dest, 0, originalSize, endOnOutputSize, full, 0, withPrefix64k, (BYTE*)(dest - 64 KB), NULL, 64 KB);
}

