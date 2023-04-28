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
 * Global memory management header.
 */


#ifndef MM_MM_H
#define MM_MM_H


#include "common.h"

#include "mmthread.h"


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


void mmInit(void);


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


#endif
