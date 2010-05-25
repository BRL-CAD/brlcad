
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, July 2000
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   hashTable.cpp
/// \brief  #goblinHashTable class implementation

#include "hashTable.h"


template <class TItem,class TKey>
goblinHashTable<TItem,TKey>::goblinHashTable(TItem rr,TItem nn,TKey alpha,
    goblinController &thisContext) throw() : managedObject(thisContext)
{
    #if defined(_TIMERS_)

    CT.globalTimer[TimerHash] -> Enable();

    #endif

    range = rr;
    nMax = nn;
    nHash = 2*nn;
    UNDEFINED = nHash;

    first = new TItem[nHash];
    next = new TItem[nMax];
    index = new TItem[nMax];
    key = new TKey[nMax];

    defaultKey = alpha;

    Init();

    LogEntry(LOG_MEM,"...Hash table instanciated");

    #if defined(_TIMERS_)

    CT.globalTimer[TimerHash] -> Disable();

    #endif
}


template <class TItem,class TKey>
void goblinHashTable<TItem,TKey>::Init() throw()
{
    #if defined(_TIMERS_)

    CT.globalTimer[TimerHash] -> Enable();

    #endif

    for (TItem v=0;v<nHash;v++) first[v] = UNDEFINED;
    for (TItem v=0;v<nMax;v++) next[v] = v+1;

    next[nMax-1] = UNDEFINED;
    free = 0;
    nz = 0;

    #if defined(_TIMERS_)

    CT.globalTimer[TimerHash] -> Disable();

    #endif
}


template <class TItem,class TKey>
unsigned long goblinHashTable<TItem,TKey>::Size() const throw()
{
    return
          sizeof(goblinHashTable<TItem,TKey>)
        + managedObject::Allocated()
        + goblinHashTable::Allocated();
}


template <class TItem,class TKey>
unsigned long goblinHashTable<TItem,TKey>::Allocated() const throw()
{
    return
          (2*nMax+nHash)*sizeof(TItem)
        + nMax*sizeof(TKey);
}


template <class TItem,class TKey>
char* goblinHashTable<TItem,TKey>::Display() const throw()
{
    LogEntry(MSG_TRACE,"Hash table");
    sprintf(CT.logBuffer,"Maximum size: %lu",static_cast<unsigned long>(nMax));
    LogEntry(MSG_TRACE2,CT.logBuffer);
    sprintf(CT.logBuffer,"Default key: %g",static_cast<double>(defaultKey));
    LogEntry(MSG_TRACE2,CT.logBuffer);

    for (TItem w0=0;w0<nHash;w0++)
    {
        TItem x = first[w0];

        if (x!=UNDEFINED)
        {
            sprintf(CT.logBuffer,"Q[%lu]:",static_cast<unsigned long>(w0));
            THandle LH = LogStart(MSG_TRACE2,CT.logBuffer);

            while (x!=UNDEFINED)
            {
                sprintf(CT.logBuffer," (%lu,%g)",
                    static_cast<unsigned long>(index[x]),
                    static_cast<double>(key[x]));
                LogAppend(LH,CT.logBuffer);
                x = next[x];
            }

            LogEnd(LH,"");
        }
    }

    return NULL;
}


template <class TItem,class TKey>
goblinHashTable<TItem,TKey>::~goblinHashTable() throw()
{
    #if defined(_TIMERS_)

    CT.globalTimer[TimerHash] -> Enable();

    #endif

    delete[] first;
    delete[] next;
    delete[] index;
    delete[] key;

    LogEntry(LOG_MEM,"...Hash table disallocated");

    #if defined(_TIMERS_)

    CT.globalTimer[TimerHash] -> Disable();

    #endif
}


template <class TItem,class TKey>
TKey goblinHashTable<TItem,TKey>::Key(TItem w) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (w>=range) NoSuchItem("Key",w);

    #endif

    #if defined(_TIMERS_)

    CT.globalTimer[TimerHash] -> Enable();

    #endif

    TItem x = first[w%nHash];

    while (x!=UNDEFINED && index[x]!=w) x = next[x];

    #if defined(_TIMERS_)

    CT.globalTimer[TimerHash] -> Disable();

    #endif

    if (x==UNDEFINED) return defaultKey;
    else return key[x];
}


template <class TItem,class TKey>
void goblinHashTable<TItem,TKey>::ChangeKey(TItem w,TKey alpha)
    throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (w>=range) NoSuchItem("ChangeKey",w);

    #endif

    if (Key(w)==alpha) return;

    #if defined(_TIMERS_)

    CT.globalTimer[TimerHash] -> Enable();

    #endif

    TItem w0 = w%nHash;

    if (alpha==defaultKey)
    {
        TItem x = first[w0];
        TItem y = UNDEFINED;

        while (x!=UNDEFINED && index[x]!=w)
        {
            y = x;
            x = next[x];
        }

        if (x!=UNDEFINED)
        {
            if (y==UNDEFINED) first[w0] = next[x];
            else next[y] = next[x];

            next[x] = free;
            free = x;
        }

        nz--;
    }
    else
    {
        TItem x = first[w0];

        while (x!=UNDEFINED && index[x]!=w) x = next[x];

        if (x!=UNDEFINED) key[x] = alpha;
        else
        {
            if (free==UNDEFINED)
            {
                TItem* savedFirst = first;
                TItem* savedNext = next;
                TKey* savedKey = key;
                TItem* savedIndex = index;

                first = new TItem[2*nHash];
                next = new TItem[2*nMax];
                index = new TItem[2*nMax];
                key = new TKey[2*nMax];

                nMax = 2*nMax;
                TItem OLD_UNDEFINED = UNDEFINED;
                UNDEFINED = nHash = 2*nHash;

                Init();

                for (TItem x=0;x<nMax;x++) // nMax = oldNHash
                {
                    TItem y = savedFirst[x];
                    while (y!=OLD_UNDEFINED)
                    {
                        ChangeKey(savedIndex[y],savedKey[y]);
                        y = savedNext[y];
                    }
                }

                delete[] savedFirst;
                delete[] savedNext;
                delete[] savedIndex;
                delete[] savedKey;

                LogEntry(LOG_MEM,"...Hash table rescaled");

                w0 = w%nHash;
            }

            TItem y = free;
            free = next[free];

            index[y] = w;
            key[y] = alpha;

            TItem x = first[w0];
            first[w0] = y;
            next[y] = x;

            nz++;
        }
    }

    #if defined(_TIMERS_)

    CT.globalTimer[TimerHash] -> Disable();

    #endif
}


template class goblinHashTable<TArc,TArc>;

template class goblinHashTable<TArc,TFloat>;

#ifdef _SMALL_ARCS_

template class goblinHashTable<TIndex,TFloat>;

template class goblinHashTable<THandle,long unsigned>;

#endif

template class goblinHashTable<TIndex,int>;

//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, July 2000
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   hashTable.cpp
/// \brief  #goblinHashTable class implementation

#include "hashTable.h"


template <class TItem,class TKey>
goblinHashTable<TItem,TKey>::goblinHashTable(TItem rr,TItem nn,TKey alpha,
    goblinController &thisContext) throw() : managedObject(thisContext)
{
    #if defined(_TIMERS_)

    CT.globalTimer[TimerHash] -> Enable();

    #endif

    range = rr;
    nMax = nn;
    nHash = 2*nn;
    UNDEFINED = nHash;

    first = new TItem[nHash];
    next = new TItem[nMax];
    index = new TItem[nMax];
    key = new TKey[nMax];

    defaultKey = alpha;

    Init();

    LogEntry(LOG_MEM,"...Hash table instanciated");

    #if defined(_TIMERS_)

    CT.globalTimer[TimerHash] -> Disable();

    #endif
}


template <class TItem,class TKey>
void goblinHashTable<TItem,TKey>::Init() throw()
{
    #if defined(_TIMERS_)

    CT.globalTimer[TimerHash] -> Enable();

    #endif

    for (TItem v=0;v<nHash;v++) first[v] = UNDEFINED;
    for (TItem v=0;v<nMax;v++) next[v] = v+1;

    next[nMax-1] = UNDEFINED;
    free = 0;
    nz = 0;

    #if defined(_TIMERS_)

    CT.globalTimer[TimerHash] -> Disable();

    #endif
}


template <class TItem,class TKey>
unsigned long goblinHashTable<TItem,TKey>::Size() const throw()
{
    return
          sizeof(goblinHashTable<TItem,TKey>)
        + managedObject::Allocated()
        + goblinHashTable::Allocated();
}


template <class TItem,class TKey>
unsigned long goblinHashTable<TItem,TKey>::Allocated() const throw()
{
    return
          (2*nMax+nHash)*sizeof(TItem)
        + nMax*sizeof(TKey);
}


template <class TItem,class TKey>
char* goblinHashTable<TItem,TKey>::Display() const throw()
{
    LogEntry(MSG_TRACE,"Hash table");
    sprintf(CT.logBuffer,"Maximum size: %lu",static_cast<unsigned long>(nMax));
    LogEntry(MSG_TRACE2,CT.logBuffer);
    sprintf(CT.logBuffer,"Default key: %g",static_cast<double>(defaultKey));
    LogEntry(MSG_TRACE2,CT.logBuffer);

    for (TItem w0=0;w0<nHash;w0++)
    {
        TItem x = first[w0];

        if (x!=UNDEFINED)
        {
            sprintf(CT.logBuffer,"Q[%lu]:",static_cast<unsigned long>(w0));
            THandle LH = LogStart(MSG_TRACE2,CT.logBuffer);

            while (x!=UNDEFINED)
            {
                sprintf(CT.logBuffer," (%lu,%g)",
                    static_cast<unsigned long>(index[x]),
                    static_cast<double>(key[x]));
                LogAppend(LH,CT.logBuffer);
                x = next[x];
            }

            LogEnd(LH,"");
        }
    }

    return NULL;
}


template <class TItem,class TKey>
goblinHashTable<TItem,TKey>::~goblinHashTable() throw()
{
    #if defined(_TIMERS_)

    CT.globalTimer[TimerHash] -> Enable();

    #endif

    delete[] first;
    delete[] next;
    delete[] index;
    delete[] key;

    LogEntry(LOG_MEM,"...Hash table disallocated");

    #if defined(_TIMERS_)

    CT.globalTimer[TimerHash] -> Disable();

    #endif
}


template <class TItem,class TKey>
TKey goblinHashTable<TItem,TKey>::Key(TItem w) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (w>=range) NoSuchItem("Key",w);

    #endif

    #if defined(_TIMERS_)

    CT.globalTimer[TimerHash] -> Enable();

    #endif

    TItem x = first[w%nHash];

    while (x!=UNDEFINED && index[x]!=w) x = next[x];

    #if defined(_TIMERS_)

    CT.globalTimer[TimerHash] -> Disable();

    #endif

    if (x==UNDEFINED) return defaultKey;
    else return key[x];
}


template <class TItem,class TKey>
void goblinHashTable<TItem,TKey>::ChangeKey(TItem w,TKey alpha)
    throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (w>=range) NoSuchItem("ChangeKey",w);

    #endif

    if (Key(w)==alpha) return;

    #if defined(_TIMERS_)

    CT.globalTimer[TimerHash] -> Enable();

    #endif

    TItem w0 = w%nHash;

    if (alpha==defaultKey)
    {
        TItem x = first[w0];
        TItem y = UNDEFINED;

        while (x!=UNDEFINED && index[x]!=w)
        {
            y = x;
            x = next[x];
        }

        if (x!=UNDEFINED)
        {
            if (y==UNDEFINED) first[w0] = next[x];
            else next[y] = next[x];

            next[x] = free;
            free = x;
        }

        nz--;
    }
    else
    {
        TItem x = first[w0];

        while (x!=UNDEFINED && index[x]!=w) x = next[x];

        if (x!=UNDEFINED) key[x] = alpha;
        else
        {
            if (free==UNDEFINED)
            {
                TItem* savedFirst = first;
                TItem* savedNext = next;
                TKey* savedKey = key;
                TItem* savedIndex = index;

                first = new TItem[2*nHash];
                next = new TItem[2*nMax];
                index = new TItem[2*nMax];
                key = new TKey[2*nMax];

                nMax = 2*nMax;
                TItem OLD_UNDEFINED = UNDEFINED;
                UNDEFINED = nHash = 2*nHash;

                Init();

                for (TItem x=0;x<nMax;x++) // nMax = oldNHash
                {
                    TItem y = savedFirst[x];
                    while (y!=OLD_UNDEFINED)
                    {
                        ChangeKey(savedIndex[y],savedKey[y]);
                        y = savedNext[y];
                    }
                }

                delete[] savedFirst;
                delete[] savedNext;
                delete[] savedIndex;
                delete[] savedKey;

                LogEntry(LOG_MEM,"...Hash table rescaled");

                w0 = w%nHash;
            }

            TItem y = free;
            free = next[free];

            index[y] = w;
            key[y] = alpha;

            TItem x = first[w0];
            first[w0] = y;
            next[y] = x;

            nz++;
        }
    }

    #if defined(_TIMERS_)

    CT.globalTimer[TimerHash] -> Disable();

    #endif
}


template class goblinHashTable<TArc,TArc>;

template class goblinHashTable<TArc,TFloat>;

#ifdef _SMALL_ARCS_

template class goblinHashTable<TIndex,TFloat>;

template class goblinHashTable<THandle,long unsigned>;

#endif

template class goblinHashTable<TIndex,int>;
