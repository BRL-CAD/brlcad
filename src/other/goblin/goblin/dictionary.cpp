
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, May 2003
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   dictionary.cpp
/// \brief  #goblinDictionary class interface

#include "dictionary.h"


template <class TKey>
goblinDictionary<TKey>::goblinDictionary(TIndex nn,TKey alpha,
    goblinController &thisContext) throw() : managedObject(thisContext)
{
    #if defined(_TIMERS_)

    CT.globalTimer[TimerHash] -> Enable();

    #endif

    nMax = nn;
    nHash = 2*nn+1;

    first = new TIndex[nHash];
    next = new TIndex[nMax];
    token = new char*[nMax];
    index = NULL;
    key = new TKey[nMax];

    defaultKey = alpha;

    Init();

    LogEntry(LOG_MEM,"...Dictionary instanciated");

    #if defined(_TIMERS_)

    CT.globalTimer[TimerHash] -> Disable();

    #endif
}


template <class TKey>
void goblinDictionary<TKey>::Init() throw()
{
    #if defined(_TIMERS_)

    CT.globalTimer[TimerHash] -> Enable();

    #endif

    for (TIndex v=0;v<nHash;v++) first[v] = NoIndex;

    for (TIndex v=0;v<nMax;v++) next[v] = v+1;

    next[nMax-1] = NoIndex;
    free = 0;
    nz = 0;

    #if defined(_TIMERS_)

    CT.globalTimer[TimerHash] -> Disable();

    #endif
}


template <class TKey>
unsigned long goblinDictionary<TKey>::Size() const throw()
{
    return
          sizeof(goblinDictionary<TKey>)
        + managedObject::Allocated()
        + goblinDictionary::Allocated();
}


template <class TKey>
unsigned long goblinDictionary<TKey>::Allocated() const throw()
{
    if (index)
      return
          (2*nMax+nHash)*sizeof(TIndex)
        + nMax*sizeof(char)
        + nMax*sizeof(TKey);
    else return
          (nMax+nHash)*sizeof(TIndex)
        + nMax*sizeof(char)
        + nMax*sizeof(TKey);
}


template <class TKey>
char* goblinDictionary<TKey>::Display() const throw()
{
    LogEntry(MSG_TRACE,"Index table");
    sprintf(CT.logBuffer,"Maximum size: %ld",nMax);
    LogEntry(MSG_TRACE2,CT.logBuffer);
    sprintf(CT.logBuffer,"Default key: %g",double(defaultKey));
    LogEntry(MSG_TRACE2,CT.logBuffer);

    for (TIndex w0=0;w0<nHash;w0++)
    {
        TIndex x = first[w0];

        if (x!=NoIndex)
        {
            sprintf(CT.logBuffer,"Q[%ld]:",w0);
            LogEntry(MSG_TRACE2,CT.logBuffer);
        }

        while (x!=NoIndex)
        {
            if (index)
              sprintf(CT.logBuffer,
                " (%s,%ld,%g)",token[x],index[x],double(key[x]));
            else 
              sprintf(CT.logBuffer," (%s,%g)",token[x],double(key[x]));
            LogEntry(MSG_APPEND,CT.logBuffer);
            x = next[x];
        }
    }

    return NULL;
}


template <class TKey>
goblinDictionary<TKey>::~goblinDictionary() throw()
{
    #if defined(_TIMERS_)

    CT.globalTimer[TimerHash] -> Enable();

    #endif

    delete[] first;
    delete[] next;
    delete[] token;
    delete[] key;

    if (index) delete[] index;

    LogEntry(LOG_MEM,"...Dictionary disallocated");

    #if defined(_TIMERS_)

    CT.globalTimer[TimerHash] -> Disable();

    #endif
}


template <class TKey>
unsigned long goblinDictionary<TKey>::HashVal(char* txt,TIndex id) throw()
{
    unsigned long val = 119;
    if (id != NoIndex) val = 73*id;

    for (unsigned long i=0;i<strlen(txt);i++)
        val = val*(val+131+txt[i]*101);

    return val;
}


template <class TKey>
TKey goblinDictionary<TKey>::Key(char* txt,TIndex id) throw()
{
    TIndex x = first[HashVal(txt,id)%nHash];

    if (index)
        while (x!=NoIndex && (strcmp(token[x],txt)!=0 || index[x]!=id))
            x = next[x];
    else while (x!=NoIndex && strcmp(token[x],txt)!=0) x = next[x];

    #if defined(_TIMERS_)

    CT.globalTimer[TimerHash] -> Disable();

    #endif

    if (x==NoIndex) return defaultKey;
    else return key[x];
}


template <class TKey>
void goblinDictionary<TKey>::ChangeKey(char* txt,TKey alpha,TIndex id,TOwnership tp)
    throw(ERRejected)
{
    if (Key(txt)==alpha) return;

    #if defined(_TIMERS_)

    CT.globalTimer[TimerHash] -> Enable();

    #endif

    unsigned long w = HashVal(txt,id);

    TIndex w0 = w%nHash;

    if (alpha==defaultKey)
    {
        TIndex x = first[w0];
        TIndex y = NoIndex;

        while (x!=NoIndex && (strcmp(token[x],txt)!=0 ||
               (index!=NULL && index[x]!=id)))
        {
            y = x;
            x = next[x];
        }

        if (x!=NoIndex)
        {
            delete[] token[x];

            if (y==NoIndex) first[w0] = next[x];
            else next[y] = next[x];

            next[x] = free;
            free = x;
        }

        nz--;
    }
    else
    {
        TIndex x = first[w0];

        if (index)
            while (x!=NoIndex && (strcmp(token[x],txt)!=0 || index[x]!=id))
                x = next[x];
        else while (x!=NoIndex && strcmp(token[x],txt)!=0) x = next[x];

        if (x!=NoIndex) key[x] = alpha;
        else
        {
            if (free==NoIndex)
            {
                TIndex* savedFirst = first;
                TIndex* savedNext = next;
                TKey* savedKey = key;
                char** savedToken = token;
                TIndex* savedIndex = index;

                first = new TIndex[2*nHash];
                next = new TIndex[2*nMax];
                token = new char*[2*nMax];
                if (savedIndex) index = new TIndex[2*nMax];
                key = new TKey[2*nMax];

                nMax = 2*nMax;
                nHash = 2*nMax+1;

                Init();

                for (TIndex x=0;x<nMax+1;x++) // nMax+1 = oldNHash
                {
                    TIndex y = savedFirst[x];
                    while (y!=NoIndex)
                    {
                        if (savedIndex)
                            ChangeKey(savedToken[y],savedIndex[y],savedKey[y]);
                        else ChangeKey(savedToken[y],NoIndex,savedKey[y]);
                        y = savedNext[y];
                    }
                }

                delete[] savedFirst;
                delete[] savedNext;
                delete[] savedToken;
                if (savedIndex) delete[] savedIndex;
                delete[] savedKey;

                LogEntry(LOG_MEM,"...Dictionary rescaled");

                w0 = w%nHash;
            }

            TIndex y = free;
            free = next[free];

            if (tp==OWNED_BY_SENDER)
            {
                token[y] = new char[strlen(txt)+1];
                sprintf(token[y],"%s",txt);
            }
            else token[y] = txt;

            if (index) index[y] = id;
            else if (id!=NoIndex)
            {
                index = new TIndex[nMax];

                for (TIndex i=0;i<nMax;i++) index[i] = NoIndex;

                index[y] = id;
            }

            key[y] = alpha;

            TIndex x = first[w0];
            first[w0] = y;
            next[y] = x;

            nz++;
        }
    }

    #if defined(_TIMERS_)

    CT.globalTimer[TimerHash] -> Disable();

    #endif
}


template class goblinDictionary<TIndex>;
