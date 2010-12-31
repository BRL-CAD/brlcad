/*
 * restrack.c --
 *
 *     This file contains wrappers for functions that dynamically allocate
 *     and deallocate resources (for example ckalloc() and ckfree()). The
 *     purpose of this is to provide a built-in system for debugging
 *     problems with dynamic resource allocation and buffer-overruns.
 *
 *     Currently, only heap memory is managed, but others (colors, fonts,
 *     pixmaps, Tcl_Obj, etc.) are to be added later.
 *
 *     Heap memory alloc/free wrappers: 
 *         Rt_Alloc()
 *         Rt_Realloc()
 *         Rt_Free()
 *
 *     Other externally available functions:
 *         Rt_AllocCommand()
 *             This implements the [::tkhtml::htmlalloc] command. See 
 *             below for details.
 *
 *         HtmlHeapDebug()
 *             This implements the [::tkhtml::heapdebug] command. See
 *             below for details.
 *
 *     No tkhtml code outside of this file should call ckalloc() and 
 *     friends directly.
 *
 *     This code is not thread safe. It's only for debugging.
 *
 *-------------------------------------------------------------------------
 *
 * Copyright (c) 2005 Dan Kennedy.
 * All rights reserved.
 *
 * This Open Source project was made possible through the financial support
 * of Eolas Technologies Inc.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Eolas Technologies Inc. nor the names of its
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */
static const char rcsid[] = "$Id: restrack.c,v 1.13 2007/12/12 04:50:29 danielk1977 Exp $";

#ifdef HTML_RES_DEBUG
#define RES_DEBUG
#endif

#include "tcl.h"
#include "tk.h"

#include <stdio.h>

#ifdef RES_DEBUG 
  #include <execinfo.h> 
#endif

#include <string.h>
#include <assert.h>

#ifndef NDEBUG 

#define MAX(x,y) ((x)>(y)?(x):(y))
#define MIN(x,y) ((x)<(y)?(x):(y))

#define RES_ALLOC  0
#define RES_OBJREF 1
#define RES_GC     2
#define RES_PIXMAP 3
#define RES_XCOLOR 4

static const char *aResNames[] = {
    "memory allocation",                 /* RES_ALLOC */
    "tcl object reference",              /* RES_OBJREF */
    "GC",                                /* RES_GC */
    "pixmap",                            /* RES_PIXMAP */
    "xcolor",                            /* RES_XCOLOR */
    0
};
static int aResCounts[] = {0, 0, 0, 0, 0};

/*
 * If RES_DEBUG is defined and glibc is in use, then a little extra
 * accounting is enabled.
 *
 * The interface to the accounting system is:
 *
 *     ResAlloc()      - Note the allocation of a resource.
 *     ResFree()       - Note the deallocation of a resource.
 *     ResDump()       - Print a catalogue of all currently allocated
 *                       resources to stdout.
 *
 * Each resource is identified by two ClientData variables, one to identify
 * the type of resource and another to identify the unique resource
 * instance. Collectively, the two ClientData values make up a
 * "resource-id". The global hash table, aOutstanding, contains a mapping
 * between resource-id and a ResRecord structure instance for every
 * currently allocated resource:
 *
 *      (<res-type> <res-ptr>)    ->    ResRecord
 *
 * There can be more than one reference to a single resource, so reference
 * counted resources (Tcl_Obj* for example) can be used with this system.
 */

/*
 * Each ResRecord structure stores info for a currently allocated resource.
 * The information stored is the number of references to the resource, and
 * the backtraces of each of the call-chains that reserved a reference.
 * A "backtrace" is obtained from the non-standard backtrace() function
 * implemented in glibc. 
 *
 * There may be more backtraces than outstanding references. This is
 * because when a reference is returned via ResFree(), it is not possible
 * to tell which of the backtraces to remove. For example, if the sequence
 * of calls is:
 *
 *     Rt_IncrRefCount();
 *     Rt_IncrRefCount();
 *     Rt_DecrRefCount();
 *
 * the ResRecord structure has nRef==1, nStack==2 and aStack pointing to an
 * array of size 2.
 */
typedef struct ResRecord ResRecord;
struct ResRecord {
    int nRef;            /* Current number of outstanding references */
    int nStack;          /* Number of stored stack-dumps */
    int **aStack;        /* Array of stored stack-dumps */
};

/* 
 * Global hash table of currently outstanding resource references.
 */
#if defined(RES_DEBUG) && defined(__GLIBC__)
static Tcl_HashTable aOutstanding;
#endif

/*
 *---------------------------------------------------------------------------
 *
 * ResAlloc --
 *
 *     Add an entry to aOutstanding for the resource identified by (v1, v2).
 *     Or, if the resource already exists, increment it's ref-count and
 *     add a new stack-trace.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     See above.
 *
 *---------------------------------------------------------------------------
 */
static void 
ResAlloc(v1, v2)
    ClientData v1;
    ClientData v2;
{
#if defined(RES_DEBUG) && defined(__GLIBC__)
    int key[2];
    int newentry;
    Tcl_HashEntry *pEntry;
    int *aFrame;
    ResRecord *pRec;

    static int init = 0;
    if (!init) {
        Tcl_InitHashTable(&aOutstanding, 2);
        init = 1;
    }

    key[0] = (int)v1;
    key[1] = (int)v2;

    pEntry = Tcl_CreateHashEntry(&aOutstanding, (const char *)key, &newentry);
    if (newentry) {
        pRec = (ResRecord *)ckalloc(sizeof(ResRecord));
        memset(pRec, 0, sizeof(ResRecord));
        Tcl_SetHashValue(pEntry, pRec);
    } else {
        pRec = Tcl_GetHashValue(pEntry);
    }

    aFrame = (int *)ckalloc(sizeof(int) * 30);
    backtrace((void *)aFrame, 29);
    aFrame[29] = 0;
    pRec->nRef++;
    pRec->nStack++;
    pRec->aStack = (int **)ckrealloc(
            (char *)pRec->aStack, 
            sizeof(int *) * pRec->nStack
    );
    pRec->aStack[pRec->nStack - 1] = aFrame;
#endif

    aResCounts[(int)((size_t) v1)]++;
}

/*
 *---------------------------------------------------------------------------
 *
 * ResFree --
 *
 *     Decrement the reference count of the resource identified by (v1, v2).
 *     If the ref-count reaches 0, remove the entry from the aOutstanding
 *     hash table.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     See above.
 *
 *---------------------------------------------------------------------------
 */
static void 
ResFree(v1, v2)
    ClientData v1;
    ClientData v2;
{
#if defined(RES_DEBUG) && defined(__GLIBC__)
    int key[2];
    Tcl_HashEntry *pEntry;
    ResRecord *pRec;

    key[0] = (int)v1;
    key[1] = (int)v2;

    pEntry = Tcl_FindHashEntry(&aOutstanding, (const char *)key);
    assert(pEntry);

    pRec = (ResRecord *)Tcl_GetHashValue(pEntry);
    pRec->nRef--;

    if (pRec->nRef == 0) {
        int i;
        ResRecord *pRec = (ResRecord *)Tcl_GetHashValue(pEntry);

        for (i = 0; i < pRec->nStack; i++) {
            ckfree((char *)pRec->aStack[i]);
        }
        ckfree((char *)pRec->aStack);
        ckfree((char *)pRec);

        Tcl_DeleteHashEntry(pEntry);
    }
#endif

    aResCounts[(int)((size_t) v1)]--;
}

/*
 *---------------------------------------------------------------------------
 *
 * ResDump --
 *
 *     Print the current contents of the global hash table aOutstanding to
 *     stdout. 
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static void 
ResDump()
{
#if defined(RES_DEBUG) && defined(__GLIBC__)
    Tcl_HashEntry *pEntry;
    Tcl_HashSearch search;
    for (
        pEntry = Tcl_FirstHashEntry(&aOutstanding, &search);
        pEntry;
        pEntry = Tcl_NextHashEntry(&search)
    ) {
        int *aKey = (int *)Tcl_GetHashKey(&aOutstanding, pEntry);
        ResRecord *pRec = (ResRecord *)Tcl_GetHashValue(pEntry);
        int i;
        printf("RESOURCE %x %x ", aKey[0], aKey[1]);
        for (i = 0; i < pRec->nStack; i++) {
            int j;
            printf("{");
            for (j = 0; pRec->aStack[i][j]; j++) {
                printf("%x%s", pRec->aStack[i][j], pRec->aStack[i][j+1]?" ":"");
            }
            printf("} ");
        }
        printf("\n");
    }
#endif
}

/*
 *---------------------------------------------------------------------------
 * End of ResTrack code.
 *---------------------------------------------------------------------------
 */

/*
 * Two hash tables to maintain a summary of the currently outstanding
 * calls to HtmlAlloc() (used to measure approximate heap usage). They
 * are manipulated exclusively by the following functions:
 *
 *     * initMallocHash()
 *     * insertMallocHash()
 *     * freeMallocHash()
 *
 * Each call to Rt_Alloc() or Rt_Realloc() is passed a "topic" argument
 * (a string). Usually, the topic is the name of the structure being
 * allocated (i.e. "HtmlComputedValues"), but can be anything.
 *
 * The aMalloc table is a mapping from topic name to the total number
 * of bytes currently allocated specifying that topic. i.e:
 *
 *     "HtmlComputedValues" -> 4068
 *     "HtmlNode" -> 13456
 *     ...
 *
 * The aAllocationType is a mapping from each pointer returned by 
 * Rt_Alloc() or Rt_Realloc() to a pointer to the hash entry corresponding 
 * to that topic in the aMalloc table.
 */
static Tcl_HashTable aMalloc;
static Tcl_HashTable aAllocationType;

/*
 *---------------------------------------------------------------------------
 *
 * initMallocHash --
 *
 *     Initialise the global aMalloc and aAllocationType hash tables.
 *     This function is a no-op if they have already been initialised.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     May initialise aMalloc and aAllocationType.
 *
 *---------------------------------------------------------------------------
 */
static void 
initMallocHash() {
    static int init = 0;
    if (!init) {
        Tcl_InitHashTable(&aMalloc, TCL_STRING_KEYS);
        Tcl_InitHashTable(&aAllocationType, TCL_ONE_WORD_KEYS);
        init = 1;
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * insertMallocHash --
 *
 *     Insert an entry into the aMalloc/aAllocationType database.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     May initialise aMalloc and aAllocationType. May insert an entry into
 *     aMalloc. Always inserts an entry into aAllocationType.
 *
 *---------------------------------------------------------------------------
 */
static void
insertMallocHash(zTopic, p, nBytes) 
    const char *zTopic;    /* Topic for allocation */
    char *p;               /* Pointer just allocated by Rt_Alloc()/Realloc() */
    int nBytes;            /* Number of bytes allocated at p */
{
    int *aData;
    int isNewEntry;
    Tcl_HashEntry *pEntry;
    Tcl_HashEntry *pEntry2;

    initMallocHash();

    pEntry = Tcl_CreateHashEntry(&aMalloc, zTopic, &isNewEntry);
    if (isNewEntry) {
        aData = (int *)ckalloc(sizeof(int) * 2);
        aData[0] = 1; 
        aData[1] = nBytes;
        Tcl_SetHashValue(pEntry, aData);
    } else {
        aData = Tcl_GetHashValue(pEntry);
        aData[0] += 1;
        aData[1] += nBytes;
    }

    pEntry2 = Tcl_CreateHashEntry(&aAllocationType, p, &isNewEntry);
    Tcl_SetHashValue(pEntry2, pEntry);
}

/*
 *---------------------------------------------------------------------------
 *
 * freeMallocHash --
 *
 *     Remove an entry from the aMalloc/aAllocationType database. If
 *     the supplied pointer is not in the database, an assert() will 
 *     fail.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     Removes an entry from aAllocationType hash table.
 *
 *---------------------------------------------------------------------------
 */
static void
freeMallocHash(p, nBytes) 
    char *p;              /* Pointer to remove from db */
    int nBytes;           /* Number of bytes (previously) allocated at p */
{
    int *aData;
    Tcl_HashEntry *pEntryAllocationType;
    Tcl_HashEntry *pEntryMalloc;

    initMallocHash();

    pEntryAllocationType = Tcl_FindHashEntry(&aAllocationType, p);
    assert(pEntryAllocationType);
    pEntryMalloc = (Tcl_HashEntry *)Tcl_GetHashValue(pEntryAllocationType);

    assert(pEntryMalloc);
    aData = Tcl_GetHashValue(pEntryMalloc);
    aData[0] -= 1;
    aData[1] -= nBytes;
    assert((aData[0] == 0 && aData[1] == 0) || (aData[0] > 0 && aData[1] >= 0));

    if (aData[0] == 0) {
        Tcl_DeleteHashEntry(pEntryMalloc);
        ckfree((char *)aData);
    }
    Tcl_DeleteHashEntry(pEntryAllocationType);
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlHeapDebug --
 *
 *         ::tkhtml::heapdebug
 *
 *     This Tcl command reports on the currently outstanding heap memory
 *     allocations made by the Html widget code. A Tcl list is returned
 *     containing a single entry for each allocation "topic" used by
 *     the html widget (see above). Each list entry is itself a list
 *     of length three, as follows:
 *
 *         [list TOPIC N-ALLOCATIONS N-BYTES]
 *
 *     i.e.:
 *         
 *         [list HtmlComputedValues 4 544]
 * 
 *     indicates that 4 HtmlComputedValues structures are allocated for
 *     a total of 544 bytes.
 *
 * Results:
 *     Always TCL_OK.
 *
 * Side effects:
 *     Populates the tcl interpreter with a result.
 *
 *---------------------------------------------------------------------------
 */
int 
HtmlHeapDebug(clientData, interp, objc, objv)
    ClientData clientData;
    Tcl_Interp *interp; 
    int objc;
    Tcl_Obj * const objv[];
{
    Tcl_Obj *pRet = Tcl_NewObj();
    Tcl_HashEntry *pEntry;
    Tcl_HashSearch search;
    for (
        pEntry = Tcl_FirstHashEntry(&aMalloc, &search);
        pEntry;
        pEntry = Tcl_NextHashEntry(&search)
    ) {
        const char *zTopic = (const char *)Tcl_GetHashKey(&aMalloc, pEntry);
        int *aData = (int *)Tcl_GetHashValue(pEntry);

        Tcl_Obj *pObj = Tcl_NewObj();
        Tcl_ListObjAppendElement(interp, pObj, Tcl_NewStringObj(zTopic, -1));
        Tcl_ListObjAppendElement(interp, pObj, Tcl_NewIntObj(aData[0]));
        Tcl_ListObjAppendElement(interp, pObj, Tcl_NewIntObj(aData[1]));
        Tcl_ListObjAppendElement(interp, pRet, pObj);
    }

    Tcl_SetObjResult(interp, pRet);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * Rt_AllocCommand --
 *
 *         ::tkhtml::htmlalloc
 *
 *     This Tcl command is only available if NDEBUG is not defined. It
 *     returns a Tcl key-value list (suitable for passing to [array set])
 *     containing the names of the managed resource types and the number
 *     of outstanding allocations. i.e:
 *
 *         [list "memory allocation" 345 "tcl object reference" 0 ....]
 * 
 *     Note: At this stage all except "memory allocation" will be 0.
 *
 *     This function also invokes ResDump(). So if RES_DEBUG is defined at
 *     compile time and glibc is in use some data may be dumped to stdout.
 *
 * Results:
 *     Always TCL_OK.
 *
 * Side effects:
 *     Populates the tcl interpreter with a result. Invokes ResDump().
 *
 *---------------------------------------------------------------------------
 */
int 
Rt_AllocCommand(clientData, interp, objc, objv)
  ClientData clientData;
  Tcl_Interp *interp; 
  int objc;
  Tcl_Obj * const objv[];
{
    int i;
    Tcl_Obj *pRet;

    pRet = Tcl_NewObj();
    for (i = 0; aResNames[i]; i++) {
        Tcl_Obj *pName = Tcl_NewStringObj(aResNames[i],-1);
        Tcl_ListObjAppendElement(interp, pRet, pName);
        Tcl_ListObjAppendElement(interp, pRet, Tcl_NewIntObj(aResCounts[i]));
    }
    Tcl_SetObjResult(interp, pRet);

    ResDump();
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * Rt_Alloc --
 *
 *     A wrapper around ckalloc() for use by code outside of this file.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
char *
Rt_Alloc(zTopic, n)
    const char *zTopic;
    int n;
{
    int nAlloc = n + 4 * sizeof(int);
    int *z = (int *)ckalloc(nAlloc);
    char *zRet = (char *)&z[2];
    z[0] = 0xFED00FED;
    z[1] = n;
    z[3 + n / sizeof(int)] = 0xBAD00BAD;

    ResAlloc(RES_ALLOC, z);
    insertMallocHash(zTopic ? zTopic : "UNSPECIFIED", zRet, n);

    memset(zRet, 0x55, n);
    return zRet;
}

/*
 *---------------------------------------------------------------------------
 *
 * Rt_Free --
 *
 *     A wrapper around ckfree() for use by code outside of this file.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
void 
Rt_Free(p)
    char *p;
{
    if (p) {
        int *z = (int *)p;
        int n = z[-1];
        assert(z[-2] == 0xFED00FED);
        assert(z[1 + n / sizeof(int)] == 0xBAD00BAD);
        memset(z, 0x55, n);
        ckfree((char *)&z[-2]);
        ResFree(RES_ALLOC, &z[-2]);
        freeMallocHash((char *) z, n);
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * Rt_Realloc --
 *
 *     A wrapper around ckrealloc() for use by code outside of this file.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
char * 
Rt_Realloc(zTopic, p, n)
    const char *zTopic;
    char *p;
    int n;
{
    char *pRet = Rt_Alloc(zTopic, n);
    if (p) {
        int current_sz = ((int *)p)[-1];
        memcpy(pRet, p, MIN(current_sz, n));
        Rt_Free((char *)p);
    }
    return pRet;
}

#endif

