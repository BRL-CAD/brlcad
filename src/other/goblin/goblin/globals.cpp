
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, April 1999
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   globals.cpp
/// \brief  Global constant definitions and some memory management code

#include <limits.h>
#include <stdlib.h>

#include "globals.h"


#ifdef _BIG_NODES_

const TNode     NoNode   = 200000;

#else

const TNode     NoNode   = 50000;

#endif


#ifdef _SMALL_ARCS_

const TArc      NoArc    = 50000;

#else

const TArc      NoArc    = 2000000000;

#endif


const TCap      InfCap   = 1000000000;
const TFloat    InfFloat = 1.0e+50;
const THandle   NoHandle = 2000000000;
const TIndex    NoIndex  = 2000000000;
const TDim      NoCoord  = UCHAR_MAX;
const TVar      NoVar    = 2000000000;
const TRestr    NoRestr  = 2000000000;


void* DefaultValueAsVoidPtr(TBaseType _type) throw()
{
    static const int NoInt = 0;
    static const double NoDouble = 0.0;
    static const char NoOri = 0;
    static const bool False = false;
    static const char* NoPointer = NULL;

    switch (_type)
    {
        case TYPE_NODE_INDEX  : return (void*)(&NoNode);
        case TYPE_ARC_INDEX   : return (void*)(&NoArc);
        case TYPE_FLOAT_VALUE : return (void*)(&InfFloat);
        case TYPE_CAP_VALUE   : return (void*)(&InfCap);
        case TYPE_INDEX       : return (void*)(&NoIndex);
        case TYPE_ORIENTATION : return (void*)(&NoOri);
        case TYPE_INT         : return (void*)(&NoInt);
        case TYPE_DOUBLE      : return (void*)(&NoDouble);
        case TYPE_BOOL        : return (void*)(&False);
        case TYPE_CHAR        : return (void*)(&NoPointer);
        case TYPE_VAR_INDEX   : return (void*)(&NoVar);
        case TYPE_RESTR_INDEX : return (void*)(&NoRestr);
        case TYPE_SPECIAL     : return NULL;
    }

    return NULL;
}


size_t goblinHeapSize   = 0;
size_t goblinMaxSize   = 0;
size_t goblinNFragments = 0;
size_t goblinNAllocs   = 0;
size_t goblinNObjects  = 0;


#ifdef _HEAP_MON_

void* operator new(size_t size) throw (std::bad_alloc)
{
    if (size==0) return static_cast<void*>(NULL);

    if (void* p = malloc(size+sizeof(size_t)))
    {
        goblinHeapSize += size;

        if (goblinHeapSize>goblinMaxSize) goblinMaxSize = goblinHeapSize;

        goblinNFragments++;
        goblinNAllocs++;

        *((size_t*)(p)) = size;
        return (size_t*)(p)+1;
    }

    throw std::bad_alloc();
}


void* operator new[](size_t size) throw (std::bad_alloc)
{
    if (size==0) return static_cast<void*>(NULL);

    if (void* p = malloc(size+sizeof(size_t)))
    {
        goblinHeapSize += size;

        if (goblinHeapSize>goblinMaxSize) goblinMaxSize = goblinHeapSize;

        goblinNFragments++;
        goblinNAllocs++;

        *((size_t*)(p)) = size;
        return (size_t*)(p)+1;
    }

    throw std::bad_alloc();
}


void* operator new(size_t size,const std::nothrow_t& type) throw ()
{
    if (size==0) return static_cast<void*>(NULL);

    if (void* p = malloc(size+sizeof(size_t)))
    {
        goblinHeapSize += size;

        if (goblinHeapSize>goblinMaxSize) goblinMaxSize = goblinHeapSize;

        goblinNFragments++;
        goblinNAllocs++;

        *((size_t*)(p)) = size;

        return (size_t*)(p)+1;
    }

    return NULL;
}


void* operator new[](size_t size,const std::nothrow_t& type) throw ()
{
    if (size==0) return static_cast<void*>(NULL);

    if (void* p = malloc(size+sizeof(size_t)))
    {
        goblinHeapSize += size;

        if (goblinHeapSize>goblinMaxSize) goblinMaxSize = goblinHeapSize;

        goblinNFragments++;
        goblinNAllocs++;
        *((size_t*)(p)) = size;

        return (size_t*)(p)+1;
    }

    return static_cast<void*>(NULL);
}


void* GoblinRealloc(void* p,size_t size) throw (std::bad_alloc)
{
    if (!p)
    {
        if (size==0) return NULL;

        if (void* p = malloc(size+sizeof(size_t)))
        {
            goblinHeapSize += size;

            if (goblinHeapSize>goblinMaxSize) goblinMaxSize = goblinHeapSize;

            goblinNFragments++;
            goblinNAllocs++;

            *((size_t*)(p)) = size;
            return (size_t*)(p)+1;
        }

        throw std::bad_alloc();
    }
    else
    {
        size_t old_size = *((size_t*)(p)-1);

        if (size==old_size) return p;

        goblinHeapSize += size-old_size;

        if (size==0)
        {
            goblinNFragments--;
            free((size_t*)(p)-1);
            return NULL;
        }

        if (size_t* q = (size_t*)malloc(size+sizeof(size_t)))
        {
            if (goblinHeapSize>goblinMaxSize) goblinMaxSize = goblinHeapSize;

            goblinNAllocs++;

            *q = size;

            if (size>old_size)
                memcpy(q+1,p,old_size);
            else memcpy(q+1,p,size);

            free((size_t*)(p)-1);

            return q+1;
        }

        throw std::bad_alloc();
    }

    throw std::bad_alloc();
}


void operator delete(void* p) throw ()
{
    if (!p) return;

    size_t size = *((size_t*)(p)-1);
    goblinHeapSize -= size;
    goblinNFragments--;

    free((size_t*)(p)-1);
}


void operator delete[](void* p) throw ()
{
    if (!p) return;

    size_t size = *((size_t*)(p)-1);

    goblinHeapSize -= size;
    goblinNFragments--;

    free((size_t*)(p)-1);
}




void operator delete(void* p,size_t) throw ()
{
    if (!p) return;

    size_t size = *((size_t*)(p)-1);
    goblinHeapSize -= size;
    goblinNFragments--;

    free((size_t*)(p)-1);
}


void operator delete[](void* p,size_t) throw ()
{
    if (!p) return;

    size_t size = *((size_t*)(p)-1);

    goblinHeapSize -= size;
    goblinNFragments--;

    free((size_t*)(p)-1);
}


void operator delete(void *p,const std::nothrow_t&) throw ()
{
    if (!p) return;

    size_t size = *((size_t*)(p)-1);

    goblinHeapSize -= size;
    goblinNFragments--;

    free((size_t*)(p)-1);
}


void operator delete[](void *p,const std::nothrow_t&) throw ()
{
    if (!p) return;

    size_t size = *((size_t*)(p)-1);

    goblinHeapSize -= size;
    goblinNFragments--;

    free((size_t*)(p)-1);
}

#else

void* GoblinRealloc(void* p,size_t size) throw (std::bad_alloc)
{
    return realloc(p,size);
}

#endif


THandle NewObjectHandle()
    throw()
{
    // Handles are globally unique, not context dependent
    static THandle nextObjectHandle = 1;

    if (nextObjectHandle+1==NoHandle) nextObjectHandle = 0;

    return nextObjectHandle++;
}

//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, April 1999
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   globals.cpp
/// \brief  Global constant definitions and some memory management code

#include <limits.h>
#include <stdlib.h>

#include "globals.h"


#ifdef _BIG_NODES_

const TNode     NoNode   = 200000;

#else

const TNode     NoNode   = 50000;

#endif


#ifdef _SMALL_ARCS_

const TArc      NoArc    = 50000;

#else

const TArc      NoArc    = 2000000000;

#endif


const TCap      InfCap   = 1000000000;
const TFloat    InfFloat = 1.0e+50;
const THandle   NoHandle = 2000000000;
const TIndex    NoIndex  = 2000000000;
const TDim      NoCoord  = UCHAR_MAX;
const TVar      NoVar    = 2000000000;
const TRestr    NoRestr  = 2000000000;


void* DefaultValueAsVoidPtr(TBaseType _type) throw()
{
    static const int NoInt = 0;
    static const double NoDouble = 0.0;
    static const char NoOri = 0;
    static const bool False = false;
    static const char* NoPointer = NULL;

    switch (_type)
    {
        case TYPE_NODE_INDEX  : return (void*)(&NoNode);
        case TYPE_ARC_INDEX   : return (void*)(&NoArc);
        case TYPE_FLOAT_VALUE : return (void*)(&InfFloat);
        case TYPE_CAP_VALUE   : return (void*)(&InfCap);
        case TYPE_INDEX       : return (void*)(&NoIndex);
        case TYPE_ORIENTATION : return (void*)(&NoOri);
        case TYPE_INT         : return (void*)(&NoInt);
        case TYPE_DOUBLE      : return (void*)(&NoDouble);
        case TYPE_BOOL        : return (void*)(&False);
        case TYPE_CHAR        : return (void*)(&NoPointer);
        case TYPE_VAR_INDEX   : return (void*)(&NoVar);
        case TYPE_RESTR_INDEX : return (void*)(&NoRestr);
        case TYPE_SPECIAL     : return NULL;
    }

    return NULL;
}


size_t goblinHeapSize   = 0;
size_t goblinMaxSize   = 0;
size_t goblinNFragments = 0;
size_t goblinNAllocs   = 0;
size_t goblinNObjects  = 0;


#ifdef _HEAP_MON_

void* operator new(size_t size) throw (std::bad_alloc)
{
    if (size==0) return static_cast<void*>(NULL);

    if (void* p = malloc(size+sizeof(size_t)))
    {
        goblinHeapSize += size;

        if (goblinHeapSize>goblinMaxSize) goblinMaxSize = goblinHeapSize;

        goblinNFragments++;
        goblinNAllocs++;

        *((size_t*)(p)) = size;
        return (size_t*)(p)+1;
    }

    throw std::bad_alloc();
}


void* operator new[](size_t size) throw (std::bad_alloc)
{
    if (size==0) return static_cast<void*>(NULL);

    if (void* p = malloc(size+sizeof(size_t)))
    {
        goblinHeapSize += size;

        if (goblinHeapSize>goblinMaxSize) goblinMaxSize = goblinHeapSize;

        goblinNFragments++;
        goblinNAllocs++;

        *((size_t*)(p)) = size;
        return (size_t*)(p)+1;
    }

    throw std::bad_alloc();
}


void* operator new(size_t size,const std::nothrow_t& type) throw ()
{
    if (size==0) return static_cast<void*>(NULL);

    if (void* p = malloc(size+sizeof(size_t)))
    {
        goblinHeapSize += size;

        if (goblinHeapSize>goblinMaxSize) goblinMaxSize = goblinHeapSize;

        goblinNFragments++;
        goblinNAllocs++;

        *((size_t*)(p)) = size;

        return (size_t*)(p)+1;
    }

    return NULL;
}


void* operator new[](size_t size,const std::nothrow_t& type) throw ()
{
    if (size==0) return static_cast<void*>(NULL);

    if (void* p = malloc(size+sizeof(size_t)))
    {
        goblinHeapSize += size;

        if (goblinHeapSize>goblinMaxSize) goblinMaxSize = goblinHeapSize;

        goblinNFragments++;
        goblinNAllocs++;
        *((size_t*)(p)) = size;

        return (size_t*)(p)+1;
    }

    return static_cast<void*>(NULL);
}


void* GoblinRealloc(void* p,size_t size) throw (std::bad_alloc)
{
    if (!p)
    {
        if (size==0) return NULL;

        if (void* p = malloc(size+sizeof(size_t)))
        {
            goblinHeapSize += size;

            if (goblinHeapSize>goblinMaxSize) goblinMaxSize = goblinHeapSize;

            goblinNFragments++;
            goblinNAllocs++;

            *((size_t*)(p)) = size;
            return (size_t*)(p)+1;
        }

        throw std::bad_alloc();
    }
    else
    {
        size_t old_size = *((size_t*)(p)-1);

        if (size==old_size) return p;

        goblinHeapSize += size-old_size;

        if (size==0)
        {
            goblinNFragments--;
            free((size_t*)(p)-1);
            return NULL;
        }

        if (size_t* q = (size_t*)malloc(size+sizeof(size_t)))
        {
            if (goblinHeapSize>goblinMaxSize) goblinMaxSize = goblinHeapSize;

            goblinNAllocs++;

            *q = size;

            if (size>old_size)
                memcpy(q+1,p,old_size);
            else memcpy(q+1,p,size);

            free((size_t*)(p)-1);

            return q+1;
        }

        throw std::bad_alloc();
    }

    throw std::bad_alloc();
}


void operator delete(void* p) throw ()
{
    if (!p) return;

    size_t size = *((size_t*)(p)-1);
    goblinHeapSize -= size;
    goblinNFragments--;

    free((size_t*)(p)-1);
}


void operator delete[](void* p) throw ()
{
    if (!p) return;

    size_t size = *((size_t*)(p)-1);

    goblinHeapSize -= size;
    goblinNFragments--;

    free((size_t*)(p)-1);
}




void operator delete(void* p,size_t) throw ()
{
    if (!p) return;

    size_t size = *((size_t*)(p)-1);
    goblinHeapSize -= size;
    goblinNFragments--;

    free((size_t*)(p)-1);
}


void operator delete[](void* p,size_t) throw ()
{
    if (!p) return;

    size_t size = *((size_t*)(p)-1);

    goblinHeapSize -= size;
    goblinNFragments--;

    free((size_t*)(p)-1);
}


void operator delete(void *p,const std::nothrow_t&) throw ()
{
    if (!p) return;

    size_t size = *((size_t*)(p)-1);

    goblinHeapSize -= size;
    goblinNFragments--;

    free((size_t*)(p)-1);
}


void operator delete[](void *p,const std::nothrow_t&) throw ()
{
    if (!p) return;

    size_t size = *((size_t*)(p)-1);

    goblinHeapSize -= size;
    goblinNFragments--;

    free((size_t*)(p)-1);
}

#else

void* GoblinRealloc(void* p,size_t size) throw (std::bad_alloc)
{
    return realloc(p,size);
}

#endif


THandle NewObjectHandle()
    throw()
{
    // Handles are globally unique, not context dependent
    static THandle nextObjectHandle = 1;

    if (nextObjectHandle+1==NoHandle) nextObjectHandle = 0;

    return nextObjectHandle++;
}
