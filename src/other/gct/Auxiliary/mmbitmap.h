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


typedef struct
{
  size_t entrycount;
  size_t mapsize;
#ifdef MM_ATOMIC_SUPPORT
  mmAtomicL *map;
#else
  long *map;
  mtMutex mutex;
#endif
} mmBitMap;

void mmBitMapInit( mmBitMap *bitmap, size_t entrycount, int initvalue );
void mmBitMapReset( mmBitMap *bitmap, int resetvalue );
void mmBitMapFree( mmBitMap *bitmap );

int mmBitMapDirectGet( mmBitMap *bitmap, size_t entryindex );
void mmBitMapDirectSet( mmBitMap *bitmap, size_t entryindex );
void mmBitMapDirectClear( mmBitMap *bitmap, size_t entryindex );

int mmBitMapDirectMaskGet( mmBitMap *bitmap, size_t entryindex, long mask );
void mmBitMapDirectMaskSet( mmBitMap *bitmap, size_t entryindex, long value, long mask );

int mmBitMapGet( mmBitMap *bitmap, size_t entryindex );
void mmBitMapSet( mmBitMap *bitmap, size_t entryindex );
void mmBitMapClear( mmBitMap *bitmap, size_t entryindex );

int mmBitMapMaskGet( mmBitMap *bitmap, size_t entryindex, long mask );
void mmBitMapMaskSet( mmBitMap *bitmap, size_t entryindex, long value, long mask );

int mmBitMapFindSet( mmBitMap *bitmap, size_t entryindex, size_t entryindexlast, size_t *retentryindex );
int mmBitMapFindClear( mmBitMap *bitmap, size_t entryindex, size_t entryindexlast, size_t *retentryindex );



