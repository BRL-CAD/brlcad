
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, September 1998
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   basicHeap.cpp
/// \brief  #basicHeap class implementation

#include "basicHeap.h"


template <class TItem,class TKey>
basicHeap<TItem,TKey>::basicHeap(TItem nn,goblinController &thisContext)
    throw() : managedObject(thisContext)
{
    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerPrioQ] -> Enable();

    #endif

    n = nn;
    v = new TItem[n];
    key = new TKey[n];
    maxIndex = 0;

    this -> LogEntry(LOG_MEM,"...Priority queue allocated");

    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerPrioQ] -> Disable();

    #endif
}


template <class TItem,class TKey>
void basicHeap<TItem,TKey>::Init() throw()
{
    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerPrioQ] -> Enable();

    #endif

    maxIndex = 0;

    this -> LogEntry(LOG_MEM,"...Priority queue instanciated");

    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerPrioQ] -> Disable();

    #endif
}


template <class TItem,class TKey>
unsigned long basicHeap<TItem,TKey>::Size() const throw()
{
    return
          sizeof(basicHeap<TItem,TKey>)
        + managedObject::Allocated()
        + basicHeap::Allocated();
}


template <class TItem,class TKey>
unsigned long basicHeap<TItem,TKey>::Allocated() const throw()
{
    return
          n*sizeof(TItem)
        + n*sizeof(TKey);
}


template <class TItem,class TKey>
char* basicHeap<TItem,TKey>::Display() const throw()
{
    this -> LogEntry(MSG_TRACE,"Priority queue");

    if (Empty()) this->LogEntry(MSG_TRACE2,"    ---");
    else
    {
        THandle LH = this->LogStart(MSG_TRACE2,"    ");

        for (TItem i=0;i<maxIndex;i++)
        {
            sprintf(this->CT.logBuffer,"%lu[%g]",
                static_cast<unsigned long>(v[i]),key[v[i]]);
            this -> LogAppend(LH,this->CT.logBuffer);

            if (i<maxIndex-1)
            {
                this -> LogAppend(LH,", ");

                if (i%10==9)
                {
                    this -> LogEnd(LH);
                    LH = this->LogStart(MSG_TRACE2,"   ");
                }
            }
        }

        this -> LogEnd(LH);
    }

    return NULL;
}


template <class TItem,class TKey>
basicHeap<TItem,TKey>::~basicHeap() throw()
{
    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerPrioQ] -> Enable();

    #endif

    delete[] v;
    delete[] key;

    this -> LogEntry(LOG_MEM,"...Priority queue disallocated");

    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerPrioQ] -> Disable();

    #endif
}


template <class TItem,class TKey>
void basicHeap<TItem,TKey>::Insert(TItem w,TKey alpha) throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (w>=n) NoSuchItem("Insert",w);

    if (maxIndex>=n)
        this -> Error(ERR_REJECTED,"Insert","Buffer is full");

    #endif

    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerPrioQ] -> Enable();

    #endif

    TItem step = maxIndex;
    TItem j = 0;
    while (step>1)
    {
        TItem nextStep = step>>1;

        if (key[v[j+nextStep]]>alpha)
        {
            j+=nextStep;
            step = step-nextStep;
        }
        else step = nextStep;
    }

    if ((step>0) && (key[v[j]]>alpha)) j++;

    memmove(v+j+1,v+j,(unsigned long)(maxIndex-j)*sizeof(TItem));
    v[j] = w;
    key[w] = alpha;
    maxIndex++;

    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerPrioQ] -> Disable();

    #endif
}


template <class TItem,class TKey>
void basicHeap<TItem,TKey>::Delete(TItem w) throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (w>=n) NoSuchItem("Delete",w);

    #endif

    TKey alpha = key[w];

    #if defined(_FAILSAVE_)

    if (alpha==InfFloat)
    {
        sprintf(this->CT.logBuffer,"Not a member: %lu",
            static_cast<unsigned long>(w));
        Error(ERR_REJECTED,"Delete",this->CT.logBuffer);
    }

    #endif

    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerPrioQ] -> Enable();

    #endif

    TItem step = maxIndex;
    TItem j = 0;

    while (step>1)
    {
        TItem nextStep = step>>1;

        if (key[v[j+nextStep]]>alpha)
        {
            j+=nextStep;
            step = step-nextStep;
        }
        else step = nextStep;
    }

    while (v[j]!=w) j++;

    memmove(v+j,v+j+1,(unsigned long)(maxIndex-j-1)*sizeof(TItem));
    key[w] = InfFloat;
    maxIndex--;

    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerPrioQ] -> Disable();

    #endif
}


template <class TItem,class TKey>
TKey basicHeap<TItem,TKey>::Key(TItem w) const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (w>=n) NoSuchItem("Key",w);

    #endif

    return key[w];
}


template <class TItem,class TKey>
void basicHeap<TItem,TKey>::ChangeKey(TItem w,TKey alpha) throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (w>=n) NoSuchItem("ChangeKey",w);

    if (key[w]==InfFloat)
    {
        sprintf(this->CT.logBuffer,"Not a member: %lu",
            static_cast<unsigned long>(w));
        Error(ERR_REJECTED,"ChangeKey",this->CT.logBuffer);
    }

    #endif

    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerPrioQ] -> Enable();

    #endif

    Delete(w);
    Insert(w,alpha);

    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerPrioQ] -> Disable();

    #endif
}


template <class TItem,class TKey>
TItem basicHeap<TItem,TKey>::Delete() throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (Empty()) this -> Error(ERR_REJECTED,"Delete","Queue is empty");

    #endif

    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerPrioQ] -> Enable();

    #endif

    TItem w = v[--maxIndex];
    key[w] = InfFloat;

    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerPrioQ] -> Disable();

    #endif

    return w;
}


template <class TItem,class TKey>
TItem basicHeap<TItem,TKey>::Peek() const throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (Empty()) this -> Error(ERR_REJECTED,"Peek","Queue is empty");

    #endif

    return v[maxIndex-1];
}


template class basicHeap<TIndex,TFloat>;

#ifndef _BIG_NODES_

template class basicHeap<TNode,TFloat>;

#endif

//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, September 1998
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   basicHeap.cpp
/// \brief  #basicHeap class implementation

#include "basicHeap.h"


template <class TItem,class TKey>
basicHeap<TItem,TKey>::basicHeap(TItem nn,goblinController &thisContext)
    throw() : managedObject(thisContext)
{
    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerPrioQ] -> Enable();

    #endif

    n = nn;
    v = new TItem[n];
    key = new TKey[n];
    maxIndex = 0;

    this -> LogEntry(LOG_MEM,"...Priority queue allocated");

    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerPrioQ] -> Disable();

    #endif
}


template <class TItem,class TKey>
void basicHeap<TItem,TKey>::Init() throw()
{
    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerPrioQ] -> Enable();

    #endif

    maxIndex = 0;

    this -> LogEntry(LOG_MEM,"...Priority queue instanciated");

    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerPrioQ] -> Disable();

    #endif
}


template <class TItem,class TKey>
unsigned long basicHeap<TItem,TKey>::Size() const throw()
{
    return
          sizeof(basicHeap<TItem,TKey>)
        + managedObject::Allocated()
        + basicHeap::Allocated();
}


template <class TItem,class TKey>
unsigned long basicHeap<TItem,TKey>::Allocated() const throw()
{
    return
          n*sizeof(TItem)
        + n*sizeof(TKey);
}


template <class TItem,class TKey>
char* basicHeap<TItem,TKey>::Display() const throw()
{
    this -> LogEntry(MSG_TRACE,"Priority queue");

    if (Empty()) this->LogEntry(MSG_TRACE2,"    ---");
    else
    {
        THandle LH = this->LogStart(MSG_TRACE2,"    ");

        for (TItem i=0;i<maxIndex;i++)
        {
            sprintf(this->CT.logBuffer,"%lu[%g]",
                static_cast<unsigned long>(v[i]),key[v[i]]);
            this -> LogAppend(LH,this->CT.logBuffer);

            if (i<maxIndex-1)
            {
                this -> LogAppend(LH,", ");

                if (i%10==9)
                {
                    this -> LogEnd(LH);
                    LH = this->LogStart(MSG_TRACE2,"   ");
                }
            }
        }

        this -> LogEnd(LH);
    }

    return NULL;
}


template <class TItem,class TKey>
basicHeap<TItem,TKey>::~basicHeap() throw()
{
    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerPrioQ] -> Enable();

    #endif

    delete[] v;
    delete[] key;

    this -> LogEntry(LOG_MEM,"...Priority queue disallocated");

    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerPrioQ] -> Disable();

    #endif
}


template <class TItem,class TKey>
void basicHeap<TItem,TKey>::Insert(TItem w,TKey alpha) throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (w>=n) NoSuchItem("Insert",w);

    if (maxIndex>=n)
        this -> Error(ERR_REJECTED,"Insert","Buffer is full");

    #endif

    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerPrioQ] -> Enable();

    #endif

    TItem step = maxIndex;
    TItem j = 0;
    while (step>1)
    {
        TItem nextStep = step>>1;

        if (key[v[j+nextStep]]>alpha)
        {
            j+=nextStep;
            step = step-nextStep;
        }
        else step = nextStep;
    }

    if ((step>0) && (key[v[j]]>alpha)) j++;

    memmove(v+j+1,v+j,(unsigned long)(maxIndex-j)*sizeof(TItem));
    v[j] = w;
    key[w] = alpha;
    maxIndex++;

    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerPrioQ] -> Disable();

    #endif
}


template <class TItem,class TKey>
void basicHeap<TItem,TKey>::Delete(TItem w) throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (w>=n) NoSuchItem("Delete",w);

    #endif

    TKey alpha = key[w];

    #if defined(_FAILSAVE_)

    if (alpha==InfFloat)
    {
        sprintf(this->CT.logBuffer,"Not a member: %lu",
            static_cast<unsigned long>(w));
        Error(ERR_REJECTED,"Delete",this->CT.logBuffer);
    }

    #endif

    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerPrioQ] -> Enable();

    #endif

    TItem step = maxIndex;
    TItem j = 0;

    while (step>1)
    {
        TItem nextStep = step>>1;

        if (key[v[j+nextStep]]>alpha)
        {
            j+=nextStep;
            step = step-nextStep;
        }
        else step = nextStep;
    }

    while (v[j]!=w) j++;

    memmove(v+j,v+j+1,(unsigned long)(maxIndex-j-1)*sizeof(TItem));
    key[w] = InfFloat;
    maxIndex--;

    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerPrioQ] -> Disable();

    #endif
}


template <class TItem,class TKey>
TKey basicHeap<TItem,TKey>::Key(TItem w) const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (w>=n) NoSuchItem("Key",w);

    #endif

    return key[w];
}


template <class TItem,class TKey>
void basicHeap<TItem,TKey>::ChangeKey(TItem w,TKey alpha) throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (w>=n) NoSuchItem("ChangeKey",w);

    if (key[w]==InfFloat)
    {
        sprintf(this->CT.logBuffer,"Not a member: %lu",
            static_cast<unsigned long>(w));
        Error(ERR_REJECTED,"ChangeKey",this->CT.logBuffer);
    }

    #endif

    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerPrioQ] -> Enable();

    #endif

    Delete(w);
    Insert(w,alpha);

    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerPrioQ] -> Disable();

    #endif
}


template <class TItem,class TKey>
TItem basicHeap<TItem,TKey>::Delete() throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (Empty()) this -> Error(ERR_REJECTED,"Delete","Queue is empty");

    #endif

    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerPrioQ] -> Enable();

    #endif

    TItem w = v[--maxIndex];
    key[w] = InfFloat;

    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerPrioQ] -> Disable();

    #endif

    return w;
}


template <class TItem,class TKey>
TItem basicHeap<TItem,TKey>::Peek() const throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (Empty()) this -> Error(ERR_REJECTED,"Peek","Queue is empty");

    #endif

    return v[maxIndex-1];
}


template class basicHeap<TIndex,TFloat>;

#ifndef _BIG_NODES_

template class basicHeap<TNode,TFloat>;

#endif
