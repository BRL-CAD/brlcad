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


#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#include "cpuconfig.h"
#include "cc.h"
#include "mm.h"
#include "mmbitmap.h"


/* Friendlier to cache on SMP systems */
#define BP_BITMAP_PREWRITE_CHECK


/*
TODO
If we don't have atomic instruction support, we need a much better mutex locking mechanism!!
*/


void mmBitMapInit( mmBitMap *bitmap, size_t entrycount, int initvalue )
{
  size_t mapsize, index;
  long value;
#ifdef MM_ATOMIC_SUPPORT
  mmAtomicL *map;
#else
  long *map;
#endif

  mapsize = ( entrycount + CPUCONF_LONG_BITS - 1 ) >> CPUCONF_LONG_BITSHIFT;
  bitmap->map = malloc( mapsize * sizeof(mmAtomicL) );
  bitmap->mapsize = mapsize;
  bitmap->entrycount = entrycount;

  map = bitmap->map;
  value = ( initvalue & 0x1 ? ~0x0 : 0x0 );
#ifdef MM_ATOMIC_SUPPORT
  for( index = 0 ; index < mapsize ; index++ )
    mmAtomicWriteL( &map[index], value );
#else
  for( index = 0 ; index < mapsize ; index++ )
    map[index] = value;
  mtMutexInit( &bitmap->mutex );
#endif

  return;
}

void mmBitMapReset( mmBitMap *bitmap, int resetvalue )
{
  size_t index;
  long value;
#ifdef MM_ATOMIC_SUPPORT
  mmAtomicL *map;
#else
  long *map;
#endif

  map = bitmap->map;
  value = ( resetvalue & 0x1 ? ~(long)0x0 : (long)0x0 );
#ifdef MM_ATOMIC_SUPPORT
  for( index = 0 ; index < bitmap->mapsize ; index++ )
    mmAtomicWriteL( &map[index], value );
#else
  for( index = 0 ; index < bitmap->mapsize ; index++ )
    map[index] = value;
#endif

  return;
}

void mmBitMapFree( mmBitMap *bitmap )
{
  free( bitmap->map );
  bitmap->map = 0;
  bitmap->mapsize = 0;
#ifndef MM_ATOMIC_SUPPORT
  mtMutexDestroy( &bitmap->mutex );
#endif
  return;
}

int mmBitMapDirectGet( mmBitMap *bitmap, size_t entryindex )
{
  int value;
  size_t index, shift;
  index = entryindex >> CPUCONF_LONG_BITSHIFT;
  shift = entryindex & ( CPUCONF_LONG_BITS - 1 );
#ifdef MM_ATOMIC_SUPPORT
  value = ( MM_ATOMIC_ACCESS_L( &bitmap->map[index] ) >> shift ) & 0x1;
#else
  value = ( bitmap->map[index] >> shift ) & 0x1;
#endif
  return value;
}

void mmBitMapDirectSet( mmBitMap *bitmap, size_t entryindex )
{
  size_t index, shift;
  index = entryindex >> CPUCONF_LONG_BITSHIFT;
  shift = entryindex & ( CPUCONF_LONG_BITS - 1 );
#ifdef MM_ATOMIC_SUPPORT
  MM_ATOMIC_ACCESS_L( &bitmap->map[index] ) |= (long)1 << shift;
#else
  bitmap->map[index] |= (long)1 << shift;
#endif
  return;
}

void mmBitMapDirectClear( mmBitMap *bitmap, size_t entryindex )
{
  size_t index, shift;
  index = entryindex >> CPUCONF_LONG_BITSHIFT;
  shift = entryindex & ( CPUCONF_LONG_BITS - 1 );
#ifdef MM_ATOMIC_SUPPORT
  MM_ATOMIC_ACCESS_L( &bitmap->map[index] ) &= ~( (long)1 << shift );
#else
  bitmap->map[index] &= ~( (long)1 << shift );
#endif
  return;
}

int mmBitMapDirectMaskGet( mmBitMap *bitmap, size_t entryindex, long mask )
{
  int value;
  size_t index, shift;
  index = entryindex >> CPUCONF_LONG_BITSHIFT;
  shift = entryindex & ( CPUCONF_LONG_BITS - 1 );
#ifdef MM_ATOMIC_SUPPORT
  value = ( MM_ATOMIC_ACCESS_L( &bitmap->map[index] ) >> shift ) & mask;
#else
  value = ( bitmap->map[index] >> shift ) & mask;
#endif
  return value;
}

void mmBitMapDirectMaskSet( mmBitMap *bitmap, size_t entryindex, long value, long mask )
{
  size_t index, shift;
  index = entryindex >> CPUCONF_LONG_BITSHIFT;
  shift = entryindex & ( CPUCONF_LONG_BITS - 1 );
#ifdef MM_ATOMIC_SUPPORT
  MM_ATOMIC_ACCESS_L( &bitmap->map[index] ) = ( MM_ATOMIC_ACCESS_L( &bitmap->map[index] ) & ~( mask << shift ) ) | ( value << shift );
#else
  bitmap->map[index] = ( bitmap->map[index] & ~( mask << shift ) ) | ( value << shift );
#endif
  return;
}

int mmBitMapGet( mmBitMap *bitmap, size_t entryindex )
{
  int value;
  size_t index, shift;
  index = entryindex >> CPUCONF_LONG_BITSHIFT;
  shift = entryindex & ( CPUCONF_LONG_BITS - 1 );
#ifdef MM_ATOMIC_SUPPORT
  value = ( mmAtomicReadL( &bitmap->map[index] ) >> shift ) & 0x1;
#else
  mtMutexLock( &bitmap->mutex );
  value = ( bitmap->map[index] >> shift ) & 0x1;
  mtMutexUnlock( &bitmap->mutex );
#endif
  return value;
}

void mmBitMapSet( mmBitMap *bitmap, size_t entryindex )
{
  size_t index, shift;
  index = entryindex >> CPUCONF_LONG_BITSHIFT;
  shift = entryindex & ( CPUCONF_LONG_BITS - 1 );
#ifdef MM_ATOMIC_SUPPORT
 #ifdef BP_BITMAP_PREWRITE_CHECK
  if( !( mmAtomicReadL( &bitmap->map[index] ) & ( (long)1 << shift ) ) )
    mmAtomicOrL( &bitmap->map[index], (long)1 << shift );
 #else
  mmAtomicOrL( &bitmap->map[index], (long)1 << shift );
 #endif
#else
  mtMutexLock( &bitmap->mutex );
  bitmap->map[index] |= (long)1 << shift;
  mtMutexUnlock( &bitmap->mutex );
#endif
  return;
}

void mmBitMapClear( mmBitMap *bitmap, size_t entryindex )
{
  size_t index, shift;
  index = entryindex >> CPUCONF_LONG_BITSHIFT;
  shift = entryindex & ( CPUCONF_LONG_BITS - 1 );
#ifdef MM_ATOMIC_SUPPORT
 #ifdef BP_BITMAP_PREWRITE_CHECK
  if( mmAtomicReadL( &bitmap->map[index] ) & ( (long)1 << shift ) )
    mmAtomicAndL( &bitmap->map[index], ~( (long)1 << shift ) );
 #else
  mmAtomicAndL( &bitmap->map[index], ~( (long)1 << shift ) );
 #endif
#else
  mtMutexLock( &bitmap->mutex );
  bitmap->map[index] &= ~( (long)1 << shift );
  mtMutexUnlock( &bitmap->mutex );
#endif
  return;
}

int mmBitMapMaskGet( mmBitMap *bitmap, size_t entryindex, long mask )
{
  int value;
  size_t index, shift;
  index = entryindex >> CPUCONF_LONG_BITSHIFT;
  shift = entryindex & ( CPUCONF_LONG_BITS - 1 );
#ifdef MM_ATOMIC_SUPPORT
  value = ( mmAtomicReadL( &bitmap->map[index] ) >> shift ) & mask;
#else
  mtMutexLock( &bitmap->mutex );
  value = ( bitmap->map[index] >> shift ) & mask;
  mtMutexUnlock( &bitmap->mutex );
#endif
  return value;
}

void mmBitMapMaskSet( mmBitMap *bitmap, size_t entryindex, long value, long mask )
{
  size_t index, shift;
  long oldvalue, newvalue;
  index = entryindex >> CPUCONF_LONG_BITSHIFT;
  shift = entryindex & ( CPUCONF_LONG_BITS - 1 );
#ifdef MM_ATOMIC_SUPPORT
  for( ; ; )
  {
    oldvalue = mmAtomicReadL( &bitmap->map[index] );
    newvalue = ( oldvalue & ~( mask << shift ) ) | ( value << shift );
    if( mmAtomicCmpReplaceL( &bitmap->map[index], oldvalue, newvalue ) )
      break;
  }
#else
  mtMutexLock( &bitmap->mutex );
  bitmap->map[index] = ( bitmap->map[index] & ~( mask << shift ) ) | ( value << shift );
  mtMutexUnlock( &bitmap->mutex );
#endif
  return;
}

/* TODO: Yeah... That code was written in one go, maybe I should test if it's working fine, just in case? */
int mmBitMapFindSet( mmBitMap *bitmap, size_t entryindex, size_t entryindexlast, size_t *retentryindex )
{
  unsigned long value;
  size_t index, shift, shiftbase, shiftmax, indexlast, shiftlast;

  indexlast = entryindexlast >> CPUCONF_LONG_BITSHIFT;
  shiftlast = entryindexlast & ( CPUCONF_LONG_BITS - 1 );

  /* Leading bits search */
  index = entryindex >> CPUCONF_LONG_BITSHIFT;
  shiftbase = entryindex & ( CPUCONF_LONG_BITS - 1 );
#ifdef MM_ATOMIC_SUPPORT
  value = mmAtomicReadL( &bitmap->map[index] );
#else
  mtMutexLock( &bitmap->mutex );
  value = bitmap->map[index];
#endif
  if( value != (unsigned long)0x0 )
  {
    shiftmax = CPUCONF_LONG_BITS;
    if( ( index == indexlast ) && ( shiftlast > shiftbase ) )
      shiftmax = shiftlast;
    value >>= shiftbase;
    for( shift = shiftbase ; shift < shiftmax ; shift++, value >>= 1 )
    {
      if( !( value & 0x1 ) )
        continue;
      entryindex = ( index << CPUCONF_LONG_BITSHIFT ) | shift;
      if( entryindex >= bitmap->entrycount )
        break;
#ifndef MM_ATOMIC_SUPPORT
      mtMutexUnlock( &bitmap->mutex );
#endif
      *retentryindex = entryindex;
      return 1;
    }
  }
  if( ( index == indexlast ) && ( shiftlast > shiftbase ) )
  {
#ifndef MM_ATOMIC_SUPPORT
    mtMutexUnlock( &bitmap->mutex );
#endif
    return 0;
  }

  /* Main search */
  for( ; ; )
  {
    index = ( index + 1 ) % bitmap->mapsize;
    if( index == indexlast )
      break;
#ifdef MM_ATOMIC_SUPPORT
    value = mmAtomicReadL( &bitmap->map[index] );
#else
    value = bitmap->map[index];
#endif
    if( value != (unsigned long)0x0 )
    {
      for( shift = 0 ; ; shift++, value >>= 1 )
      {
        if( !( value & 0x1 ) )
          continue;
        entryindex = ( index << CPUCONF_LONG_BITSHIFT ) | shift;
        if( entryindex >= bitmap->entrycount )
          break;
#ifndef MM_ATOMIC_SUPPORT
        mtMutexUnlock( &bitmap->mutex );
#endif
        *retentryindex = entryindex;
        return 1;
      }
    }
  }

  /* Trailing bits search */
  shiftlast = entryindexlast & ( CPUCONF_LONG_BITS - 1 );
#ifdef MM_ATOMIC_SUPPORT
  value = mmAtomicReadL( &bitmap->map[index] );
#else
  value = bitmap->map[index];
#endif
  if( value != (unsigned long)0x0 )
  {
    for( shift = 0 ; shift < shiftlast ; shift++, value >>= 1 )
    {
      if( !( value & 0x1 ) )
        continue;
      entryindex = ( index << CPUCONF_LONG_BITSHIFT ) | shift;
      if( entryindex >= bitmap->entrycount )
        break;
#ifndef MM_ATOMIC_SUPPORT
      mtMutexUnlock( &bitmap->mutex );
#endif
      if( ( index == indexlast ) && ( shift >= shiftlast ) )
        return 0;
      *retentryindex = entryindex;
      return 1;
    }
  }
#ifndef MM_ATOMIC_SUPPORT
  mtMutexUnlock( &bitmap->mutex );
#endif

  return 0;
}

int mmBitMapFindClear( mmBitMap *bitmap, size_t entryindex, size_t entryindexlast, size_t *retentryindex )
{
  unsigned long value;
  size_t index, shift, shiftbase, shiftmax, indexlast, shiftlast;

  indexlast = entryindexlast >> CPUCONF_LONG_BITSHIFT;
  shiftlast = entryindexlast & ( CPUCONF_LONG_BITS - 1 );

  /* Leading bits search */
  index = entryindex >> CPUCONF_LONG_BITSHIFT;
  shiftbase = entryindex & ( CPUCONF_LONG_BITS - 1 );
#ifdef MM_ATOMIC_SUPPORT
  value = mmAtomicReadL( &bitmap->map[index] );
#else
  mtMutexLock( &bitmap->mutex );
  value = bitmap->map[index];
#endif
  if( value != ~(unsigned long)0x0 )
  {
    shiftmax = CPUCONF_LONG_BITS;
    if( ( index == indexlast ) && ( shiftlast > shiftbase ) )
      shiftmax = shiftlast;
    value >>= shiftbase;
    for( shift = shiftbase ; shift < shiftmax ; shift++, value >>= 1 )
    {
      if( value & 0x1 )
        continue;
      entryindex = ( index << CPUCONF_LONG_BITSHIFT ) | shift;
      if( entryindex >= bitmap->entrycount )
        break;
#ifndef MM_ATOMIC_SUPPORT
      mtMutexUnlock( &bitmap->mutex );
#endif
      *retentryindex = entryindex;
      return 1;
    }
  }
  if( ( index == indexlast ) && ( shiftlast > shiftbase ) )
  {
#ifndef MM_ATOMIC_SUPPORT
    mtMutexUnlock( &bitmap->mutex );
#endif
    return 0;
  }

  /* Main search */
  for( ; ; )
  {
    index = ( index + 1 ) % bitmap->mapsize;
    if( index == indexlast )
      break;
#ifdef MM_ATOMIC_SUPPORT
    value = mmAtomicReadL( &bitmap->map[index] );
#else
    value = bitmap->map[index];
#endif
    if( value != ~(unsigned long)0x0 )
    {
      for( shift = 0 ; ; shift++, value >>= 1 )
      {
        if( value & 0x1 )
          continue;
        entryindex = ( index << CPUCONF_LONG_BITSHIFT ) | shift;
        if( entryindex >= bitmap->entrycount )
          break;
#ifndef MM_ATOMIC_SUPPORT
        mtMutexUnlock( &bitmap->mutex );
#endif
        *retentryindex = entryindex;
        return 1;
      }
    }
  }

  /* Trailing bits search */
  shiftlast = entryindexlast & ( CPUCONF_LONG_BITS - 1 );
#ifdef MM_ATOMIC_SUPPORT
  value = mmAtomicReadL( &bitmap->map[index] );
#else
  value = bitmap->map[index];
#endif
  if( value != ~(unsigned long)0x0 )
  {
    for( shift = 0 ; shift < shiftlast ; shift++, value >>= 1 )
    {
      if( value & 0x1 )
        continue;
      entryindex = ( index << CPUCONF_LONG_BITSHIFT ) | shift;
      if( entryindex >= bitmap->entrycount )
        break;
#ifndef MM_ATOMIC_SUPPORT
      mtMutexUnlock( &bitmap->mutex );
#endif
      if( ( index == indexlast ) && ( shift >= shiftlast ) )
        return 0;
      *retentryindex = entryindex;
      return 1;
    }
  }
#ifndef MM_ATOMIC_SUPPORT
  mtMutexUnlock( &bitmap->mutex );
#endif

  return 0;
}
