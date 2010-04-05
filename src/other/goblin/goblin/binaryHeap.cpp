
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, December 1998
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   binaryHeap.cpp
/// \brief  #binaryHeap class implementation

#include "binaryHeap.h"
#include "treeView.h"


template <class TItem,class TKey>
binaryHeap<TItem,TKey>::binaryHeap(TItem _n,goblinController &thisContext)
    throw() : managedObject(thisContext)
{
    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerPrioQ] -> Enable();

    #endif

    n = _n;
//    UITEM_MAX = n+1;

    v = new TItem[n+1];
    index = new TItem[n];
    key = new TKey[n];

    maxIndex = 0;

    for (TItem v=0;v<n;v++) index[v] = UITEM_MAX();

    this -> LogEntry(LOG_MEM,"...Binary heap instanciated");

    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerPrioQ] -> Disable();

    #endif
}


template <class TItem,class TKey>
void binaryHeap<TItem,TKey>::Init() throw()
{
    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerPrioQ] -> Enable();

    #endif

    if (maxIndex*100<n)
    {
        while (!Empty()) Delete();
    }
    else
    {
        for (TItem v=0;v<n;v++) index[v] = UITEM_MAX();

        maxIndex = 0;
    }

    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerPrioQ] -> Disable();

    #endif
}


template <class TItem,class TKey>
unsigned long binaryHeap<TItem,TKey>::Size() const throw()
{
    return
          sizeof(binaryHeap<TItem,TKey>)
        + managedObject::Allocated()
        + binaryHeap::Allocated();
}


template <class TItem,class TKey>
unsigned long binaryHeap<TItem,TKey>::Allocated() const throw()
{
    return
          (2*n+1)*sizeof(TItem)
        + n*sizeof(TKey);
}


static THandle LH = NoHandle;

template <class TItem,class TKey>
char* binaryHeap<TItem,TKey>::Display() const throw()
{
    if (this->CT.displayMode>0)
    {
        if (maxIndex==0) return NULL;

        goblinTreeView G(n,this->CT);
        G.InitPredecessors();
        G.InitNodeColours();

        for (TItem i=1;i<=maxIndex;i++)
        {
            G.SetNodeColour(v[i],0);
            G.SetDist(v[i],TFloat(key[v[i]]));
            if (i>1)

            {
                TArc a = G.InsertArc(TNode(v[i/2]),TNode(v[i]));
                G.SetPred(TNode(v[i]),2*a);
            }
        }

        G.Layout_PredecessorTree();
        G.Display();
    }
    else
    {
        this -> LogEntry(MSG_TRACE,"Binary heap");
        LH = this->LogStart(MSG_TRACE2,"    ");

        if (maxIndex>0)
        {
            Display(1);
            this -> LogEnd(LH);
        }
        else this -> LogEnd(LH,"---");
    }

    return NULL;
}


template <class TItem,class TKey>
void binaryHeap<TItem,TKey>::Display(TItem i) const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (i>maxIndex || i==0) NoSuchIndex("Display",i);

    if (i!=index[v[i]])
    {
        sprintf(this->CT.logBuffer,"Inconsistent index: %lu",static_cast<unsigned long>(i));
        InternalError1("Display");
    }

    #endif

    TItem left = 2*i;
    TItem right = left+1;

    sprintf(this->CT.logBuffer,"%lu[%g]",
        static_cast<unsigned long>(v[i]),static_cast<double>(key[v[i]]));
    this -> LogAppend(LH,this->CT.logBuffer);

    if (left<=maxIndex)
    {
        this -> LogAppend(LH," (");
        Display(left);
        this -> LogAppend(LH," ");

        if (right<=maxIndex) Display(right);
        else this -> LogAppend(LH,"*");

        this -> LogAppend(LH,")");
    }
}


template <class TItem,class TKey>
binaryHeap<TItem,TKey>::~binaryHeap() throw()
{
    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerPrioQ] -> Enable();

    #endif

    delete[] v;
    delete[] index;
    delete[] key;

    this -> LogEntry(LOG_MEM,"...Binary heap disallocated");

    #if defined(_TRACING_)

    if (this->CT.traceData) Display();

    #endif
}


template <class TItem,class TKey>
void binaryHeap<TItem,TKey>::UpHeap(TItem i) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (i>maxIndex || i==0) NoSuchIndex("UpHeap",i);

    #endif

    TItem w = v[i];
    TKey alpha = key[w];

    TItem next;
    while (i>1 && key[v[(next = i>>1)]]>alpha)
    {
        v[i] = v[next];
        index[v[i]] = i;
        i = next;
    }

    v[i] = w;
    index[w] = i;
}


template <class TItem,class TKey>
void binaryHeap<TItem,TKey>::DownHeap(TItem i) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (i>maxIndex || i==0) NoSuchIndex("DownHeap",i);

    #endif

    TItem left = 2*i;
    TItem right = left+1;

    if (left<=maxIndex && key[v[left]]<key[v[i]])
    {
        TItem swap = v[i];
        v[i] = v[left];
        index[v[i]] = i;
        v[left] = swap;
        index[swap] = left;

        DownHeap(left);
    }

    if (right<=maxIndex && key[v[right]]<key[v[i]])
    {
        TItem swap = v[i];
        v[i] = v[right];
        index[v[i]] = i;
        v[right] = swap;
        index[swap] = right;

        DownHeap(right);
    }
}


template <class TItem,class TKey>
void binaryHeap<TItem,TKey>::Insert(TItem w,TKey alpha) throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (w>=n) NoSuchIndex("Insert",w);

    if (maxIndex>=n) this -> Error(ERR_REJECTED,"Insert","Heap overflow");

    #endif

    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerPrioQ] -> Enable();

    #endif

    key[w] = alpha;
    index[w] = (++maxIndex);
    v[maxIndex] = w;

    UpHeap(maxIndex);

    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerPrioQ] -> Disable();

    #endif

    #if defined(_TRACING_)

    if (this->CT.traceData) Display();

    #endif
}


template <class TItem,class TKey>
void binaryHeap<TItem,TKey>::Delete(TItem w) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (w>=n || index[w]==UITEM_MAX()) NoSuchItem("Delete",w);

    #endif

    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerPrioQ] -> Enable();

    #endif

    TItem i = index[w];
    index[w] = UITEM_MAX();
    maxIndex--;

    if (i<=maxIndex)
    {
        v[i] = v[maxIndex+1];
        DownHeap(i);
    }

    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerPrioQ] -> Disable();

    #endif

    #if defined(_TRACING_)

    if (this->CT.traceData) Display();

    #endif
}


template <class TItem,class TKey>
TItem binaryHeap<TItem,TKey>::Delete() throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (maxIndex==0) this -> Error(ERR_REJECTED,"Delete","Heap is empty");

    #endif

    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerPrioQ] -> Enable();

    #endif

    TItem w = v[1];
    index[w] = UITEM_MAX();
    TItem x = v[maxIndex];
    maxIndex--;

    if (maxIndex>0)
    {
        v[1] = x;
        index[x] = 1;
        DownHeap(1);
    }

    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerPrioQ] -> Disable();

    #endif

    #if defined(_TRACING_)

    if (this->CT.traceData) Display();

    #endif

    return w;
}


template <class TItem,class TKey>
TKey binaryHeap<TItem,TKey>::Key(TItem w) const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (w>=n || index[w]==UITEM_MAX()) NoSuchItem("Key",w);

    #endif

    return key[w];
}


template <class TItem,class TKey>
void binaryHeap<TItem,TKey>::ChangeKey(TItem w,TKey alpha) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (w>=n || index[w]==UITEM_MAX()) NoSuchItem("ChangeKey",w);

    #endif

    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerPrioQ] -> Enable();

    #endif

    if (alpha>key[w])
    {
        key[w] = alpha;
        DownHeap(index[w]);
    }
    else
    {
        key[w] = alpha;
        UpHeap(index[w]);
    }

    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerPrioQ] -> Disable();

    #endif

    #if defined(_TRACING_)

    if (this->CT.traceData) Display();

    #endif
}


template <class TItem,class TKey>
TItem binaryHeap<TItem,TKey>::Peek() const throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (maxIndex==0) this -> Error(ERR_REJECTED,"Peek","Heap is empty");

    #endif

    return v[1];
}


template class binaryHeap<TIndex,TFloat>;

#ifndef _BIG_NODES_

template class binaryHeap<TNode,TFloat>;

#endif
