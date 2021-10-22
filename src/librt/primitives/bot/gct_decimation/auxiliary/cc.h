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


#ifndef MM_CC_H
#define MM_CC_H


#include "common.h"


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

    for (i = 0; i < 20; i++)
	ccQuickRand32(randstate);
}


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


static inline unsigned ccLog2Int(unsigned value)
{
    unsigned result = 0;

    while (value /= 2)
	++result;

    return result;
}

#endif
