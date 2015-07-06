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


#if defined(__GNUC__) && (__GNUC__ == 5 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 3)) && !defined(__clang__)
#  pragma GCC diagnostic ignored "-Wunused-function"
#endif
#if defined(__clang__)
#  pragma clang diagnostic ignored "-Wunused-function"
#endif


#include "common.h"

#include "mmbitmap.h"

#include "cpuconfig.h"

#include <stdlib.h>


void mmBitMapInit(mmBitMap *bitmap, size_t entrycount, int initvalue)
{
    size_t mapsize, vindex;
    long value;
#ifdef MM_ATOMIC_SUPPORT
    mmAtomicL *map;
#else
    long *map;
#endif

    mapsize = (entrycount + CPUCONF_LONG_BITS - 1) >> CPUCONF_LONG_BITSHIFT;
    bitmap->map = (mmAtomic64 *)malloc(mapsize * sizeof(mmAtomicL));
    bitmap->mapsize = mapsize;
    bitmap->entrycount = entrycount;

    map = bitmap->map;
    value = (initvalue & 0x1 ? ~0x0 : 0x0);
#ifdef MM_ATOMIC_SUPPORT

    for (vindex = 0 ; vindex < mapsize ; vindex++)
	mmAtomicWriteL(&map[vindex], value);

#else

    for (vindex = 0 ; vindex < mapsize ; vindex++)
	map[vindex] = value;

    mtMutexInit(&bitmap->mutex);
#endif

    return;
}


void mmBitMapFree(mmBitMap *bitmap)
{
    free(bitmap->map);
    bitmap->map = 0;
    bitmap->mapsize = 0;
#ifndef MM_ATOMIC_SUPPORT
    mtMutexDestroy(&bitmap->mutex);
#endif
    return;
}


int mmBitMapDirectGet(mmBitMap *bitmap, size_t entryindex)
{
    int value;
    size_t vindex, shift;
    vindex = entryindex >> CPUCONF_LONG_BITSHIFT;
    shift = entryindex & (CPUCONF_LONG_BITS - 1);
#ifdef MM_ATOMIC_SUPPORT
    value = (MM_ATOMIC_ACCESS_L(&bitmap->map[vindex]) >> shift) & 0x1;
#else
    value = (bitmap->map[vindex] >> shift) & 0x1;
#endif
    return value;
}

void mmBitMapDirectSet(mmBitMap *bitmap, size_t entryindex)
{
    size_t vindex, shift;
    vindex = entryindex >> CPUCONF_LONG_BITSHIFT;
    shift = entryindex & (CPUCONF_LONG_BITS - 1);
#ifdef MM_ATOMIC_SUPPORT
    MM_ATOMIC_ACCESS_L(&bitmap->map[vindex]) |= (long)1 << shift;
#else
    bitmap->map[vindex] |= (long)1 << shift;
#endif
    return;
}
