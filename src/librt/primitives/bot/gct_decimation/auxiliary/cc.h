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


#include "common.h"

#include "cpuconfig.h"


#ifndef ADDRESS
#define ADDRESS(p,o) ((void *)(((char *)p)+(o)))
#endif

#ifndef ADDRESSDIFF
#define ADDRESSDIFF(a,b) (((char *)a)-((char *)b))
#endif


/****/


uint32_t ccHash32Data(void *data, int size);
uint32_t ccHash32Array32(uint32_t *data, int count);
uint32_t ccHash32Array64(uint64_t *data, int count);


static inline uint32_t ccHash32Int64Inline(uint64_t i)
{
    uint32_t hash;
    hash = i & 0xFFFF;
    hash = ((hash << 16) ^ hash) ^ (((uint32_t)(i >> 16) & 0xFFFF) << 11);
    hash += (hash >> 11) + ((uint32_t)(i >> 32) & 0xFFFF);
    hash = ((hash << 16) ^ hash) ^ (uint32_t)((i & 0xFFFF000000000000LL) >> 37);
    hash += hash >> 11;
    hash ^= hash << 3;
    hash += hash >> 5;
    hash ^= hash << 4;
    hash += hash >> 17;
    hash ^= hash << 25;
    hash += hash >> 6;
    return hash;
}

/****/


typedef struct {
    uint32_t a;
    uint32_t b;
    uint32_t c;
    uint32_t d;
} ccQuickRandState32;

static inline uint32_t ccQuickRand32(ccQuickRandState32 *randstate)
{
    uint32_t e;
    e = randstate->a - ((randstate->b << 27) | (randstate->b >> (32 - 27)));
    randstate->a = randstate->b ^ ((randstate->c << 17) | (randstate->c >> (32 - 17)));
    randstate->b = randstate->c + randstate->d;
    randstate->c = randstate->d + e;
    randstate->d = e + randstate->a;
    return randstate->d;
}

static inline void ccQuickRand32Seed(ccQuickRandState32 *randstate, uint32_t seed)
{
    uint32_t i;
    randstate->a = 0xf1ea5eed;
    randstate->b = seed;
    randstate->c = seed;
    randstate->d = seed;

    for (i = 0 ; i < 20 ; i++)
	ccQuickRand32(randstate);
}


/****/


uint8_t ccLog2Int8(uint8_t i);
uint16_t ccLog2Int16(uint16_t i);
uint32_t ccLog2Int32(uint32_t i);
uint64_t ccLog2Int64(uint64_t i);

/****/


static inline uint32_t ccPow2Round32(uint32_t v)
{
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;
    return v;
}


/****/


void ccStrLowCopy(char *dst, char *src, int length);

int ccStrCmpEqual(char *s0, char *s1);

char *ccStrCmpWord(char *str, char *word);

char *ccStrCmpSeq(char *str, char *seq, int seqlength);

char *ccStrMatchSeq(char *str, char *seq, int seqlength);

char *ccStrSeqCmpSeq(char *s1, char *s2, int s1length, int s2length);

int ccStrWordCmpWord(char *s1, char *s2);

char *ccStrLowCmpSeq(char *str, char *seq, int seqlength);

char *ccStrFind(char *str, char *seq, int seqlength);

char *ccStrFindWord(char *str, char *word, int wordlength);

int ccStrWordLength(char *str);

int ccSeqFindChar(char *seq, int seqlen, char c);

char *ccSeqFindStr(char *seq, int seqlen, char *str);

char *ccStrParam(char *str, int *paramlen, int *skiplen);

char *ccStrNextWord(char *str);

char *ccStrSkipWord(char *str);

char *ccStrNextWordSameLine(char *str);

char *ccStrNextParam(char *str);

char *ccStrNextLine(char *str);

char *ccStrPassLine(char *str);

int ccStrParseInt(char *str, int64_t *retint);

int ccSeqParseInt(char *seq, int seqlength, int64_t *retint);

int ccStrParseFloat(char *str, float *retfloat);


/****/


static inline uint32_t ccAlignInt32(uint32_t i)
{
    i--;
    i |= i >> 1;
    i |= i >> 2;
    i |= i >> 4;
    i |= i >> 8;
    i |= i >> 16;
    return i + 1;
}

static inline uintptr_t ccAlignIntPtr(uintptr_t i)
{
    i--;
    i |= i >> 1;
    i |= i >> 2;
    i |= i >> 4;
    i |= i >> 8;
    i |= i >> 16;
#if CPUCONF_INTPTR_BITS > 32
    i |= i >> 32;
#endif
    return i + 1;
}


/****/


int ccMergeSort(void **src, void **tmp, int count, int (*sortfunc)(void *t0, void *t1));
int ccMergeSortContext(void **src, void **tmp, int count, int (*sortfunc)(void *t0, void *t1, void *context), void *context);
