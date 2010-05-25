
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, May 2000
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   fibonacciHeap.cpp
/// \brief  #fibonacciHeap class implementation

#include "fibonacciHeap.h"
#include "treeView.h"


template <class TItem,class TKey>
fibonacciHeap<TItem,TKey>::fibonacciHeap(TItem nn,goblinController &thisContext)
    throw() : managedObject(thisContext)
{
    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerPrioQ] -> Enable();

    #endif

    n = nn;
    k = nn;
    UNDEFINED = n;

    pred = new TItem[n];
    first = new TItem[n];
    next = new TItem[n];
    nextLink = new TItem[n];
    previous = new TItem[n];
    rank = new TItem[n];
    status = new TStatus[n];
    bucket = new TItem[k];
    key = new TKey[n];

    card = 0;
    minimal = UNDEFINED;
    firstLink = UNDEFINED;

    for (TItem v=0;v<n;v++) status[v] = NOT_QUEUED;
    for (TItem v=0;v<k;v++) bucket[v] = UNDEFINED;
    for (TItem v=0;v<n;v++) nextLink[v] = UNDEFINED;

    this -> LogEntry(LOG_MEM,"...Fibonacci heap instanciated");

    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerPrioQ] -> Disable();

    #endif
}


template <class TItem,class TKey>
void fibonacciHeap<TItem,TKey>::Init() throw()
{
    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerPrioQ] -> Enable();

    #endif

    if (card*100<n)
    {
        while (!Empty()) Delete();
    }
    else
    {
        for (TItem v=0;v<n;v++) status[v] = NOT_QUEUED;
        for (TItem v=0;v<k;v++) bucket[v] = UNDEFINED;
        for (TItem v=0;v<n;v++) nextLink[v] = UNDEFINED;

        card = 0;
        minimal = UNDEFINED;
        firstLink = UNDEFINED;
    }

    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerPrioQ] -> Disable();

    #endif
}


template <class TItem,class TKey>
unsigned long fibonacciHeap<TItem,TKey>::Size() const throw()
{
    return
          sizeof(fibonacciHeap<TItem,TKey>)
        + managedObject::Allocated()
        + fibonacciHeap::Allocated();
}


template <class TItem,class TKey>
unsigned long fibonacciHeap<TItem,TKey>::Allocated() const throw()
{
    return
          (6*n+k)*sizeof(TItem)
        + n*sizeof(char)
        + n*sizeof(TKey);
}


static THandle LH = NoHandle;

template <class TItem,class TKey>
char* fibonacciHeap<TItem,TKey>::Display() const throw()
{
    if (this->CT.displayMode>0)
    {
        if (card==0) return NULL;

        goblinTreeView G(n,this->CT);
        G.InitPredecessors();

        for (TItem i=0;i<n;i++)
        {
            if (status[i]==NOT_QUEUED) G.SetNodeColour(i,NoNode);
            else
            {
                G.SetNodeColour(i,TNode(status[i]));
                G.SetDist(i,TFloat(key[i]));

                TItem x = first[i];

                while (x!=UNDEFINED)
                {
                    #if defined(_FAILSAVE_)

                    if (status[x]!=UNMARKED_CHILD && status[x]!=MARKED_CHILD)
                    {
                        sprintf(this->CT.logBuffer,"Inconsistent item: %lu",
                            static_cast<unsigned long>(x));
                        InternalError1("Display");
                    }

                    #endif

                    TArc a = G.InsertArc(TNode(i),TNode(x));
                    G.SetPred(TNode(x),2*a);
                    x = next[x];
                }
            }
        }

        G.Layout_PredecessorTree();
        G.Display();
    }
    else
    {
        this -> LogEntry(MSG_TRACE,"Fibonacci heap");
        LH = this->LogStart(MSG_TRACE2,"    ");

        if (card>0)
        {
            for (TItem i=0;i<k;i++)
            {
                if (bucket[i]!=UNDEFINED)
                {
                    #if defined(_FAILSAVE_)

                    if (status[bucket[i]]!=ROOT_NODE)
                    {
                        sprintf(this->CT.logBuffer,"Inconsistent bucket: %lu",
                            static_cast<unsigned long>(i));
                        InternalError1("Display");
                    }

                    #endif

                    this -> LogAppend(LH,"  ");
                    Display(bucket[i]);
                }
            }

            this -> LogEnd(LH);
        }
        else this -> LogEnd(LH,"---");
    }

    return NULL;
}


template <class TItem,class TKey>
void fibonacciHeap<TItem,TKey>::Display(TItem v) const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (v>=n || status[v]==NOT_QUEUED) NoSuchItem("Display",v);

    #endif

    sprintf(this->CT.logBuffer,"%lu[%g]",
        static_cast<unsigned long>(v),static_cast<double>(key[v]));
    this -> LogAppend(LH,this->CT.logBuffer);

    TItem x = first[v];

    if (x!=UNDEFINED)
    {
        this -> LogAppend(LH," (");

        while (x!=UNDEFINED)
        {
            #if defined(_FAILSAVE_)

            if (status[x]!=UNMARKED_CHILD && status[x]!=MARKED_CHILD)
            {
                sprintf(this->CT.logBuffer,"Inconsistent item: %lu",
                    static_cast<unsigned long>(x));
                InternalError1("Display");
            }

            #endif

            Display(x);
            x = next[x];

            if (x!=UNDEFINED) this -> LogAppend(LH," ");
        }

        this -> LogAppend(LH,")");
    }
}


template <class TItem,class TKey>
fibonacciHeap<TItem,TKey>::~fibonacciHeap() throw()
{
    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerPrioQ] -> Enable();

    #endif

    delete[] pred;
    delete[] first;
    delete[] next;
    delete[] previous;
    delete[] rank;
    delete[] status;
    delete[] bucket;
    delete[] nextLink;
    delete[] key;

    this -> LogEntry(LOG_MEM,"...Fibonacci heap disallocated");

    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerPrioQ] -> Disable();

    #endif
}


template <class TItem,class TKey>
bool fibonacciHeap<TItem,TKey>::IsMember(TItem w)
    const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (w>=n) NoSuchItem("Insert",w);

    #endif

    return (status[w]!=NOT_QUEUED);
}


template <class TItem,class TKey>
void fibonacciHeap<TItem,TKey>::Insert(TItem w,TKey alpha)
    throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (w>=n) NoSuchItem("Insert",w);

    if (status[w]!=NOT_QUEUED)
    {
        sprintf(this->CT.logBuffer,"Already on queue: %lu",
            static_cast<unsigned long>(w));
        Error(ERR_REJECTED,"Insert",this->CT.logBuffer);
    }

    #endif

    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerPrioQ] -> Enable();

    #endif

    first[w] = UNDEFINED;
    rank[w] = 0;
    status[w] = ROOT_NODE;
    key[w] = alpha;
    nextLink[w] = UNDEFINED;
    card++;
    Push(w);

    if (minimal==UNDEFINED || alpha<key[minimal]) minimal = w;

    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerPrioQ] -> Disable();

    #endif

    #if defined(_TRACING_)

    if (this->CT.traceData) Display();

    #endif
}


// Add w to the list of root nodes

template <class TItem,class TKey>
void fibonacciHeap<TItem,TKey>::Push(TItem w) throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (w>=n || status[w]!=ROOT_NODE)
    {
        sprintf(this->CT.logBuffer,"Not a root: %lu",
            static_cast<unsigned long>(w));
        this -> Error(ERR_REJECTED,"Push",this->CT.logBuffer);
    }

    #endif

    nextLink[w] = firstLink;
    firstLink = w;
}


// Consolidate the buckets and recompute the global minimum

template <class TItem,class TKey>
void fibonacciHeap<TItem,TKey>::Restore() throw()
{
    for (TItem i=0;i<k;i++) bucket[i] = UNDEFINED;

    minimal = UNDEFINED;

    TItem x = firstLink;

    while (x!=UNDEFINED)
    {
        if (status[x]==ROOT_NODE)
        {
            if (bucket[rank[x]]==UNDEFINED) bucket[rank[x]] = x;
            else Link(x,bucket[rank[x]]);
        }
        x = nextLink[x];
    }

    x = firstLink;
    TItem y = UNDEFINED;

    while (x!=UNDEFINED)
    {
        if (status[x]!=ROOT_NODE)
        {
            if (y==UNDEFINED) firstLink = nextLink[x];
            else nextLink[y] = nextLink[x];
        }
        else
        {
            y = x;

            if (minimal==UNDEFINED || key[x]<key[minimal]) minimal = x;
        }

        x = nextLink[x];
    }
}


// Merge two trees with respective roots v and w which have equal rank.
// If the bucket (rank+1) is occupied, link is called recursively.

template <class TItem,class TKey>
void fibonacciHeap<TItem,TKey>::Link(TItem v,TItem w) throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (v>=n) NoSuchItem("Link",v);

    if (status[v]!=ROOT_NODE)
    {
        sprintf(this->CT.logBuffer,"Not a root: %lu",
            static_cast<unsigned long>(v));
        this -> Error(ERR_REJECTED,"Link",this->CT.logBuffer);
    }

    if (w>=n) NoSuchItem("Link",w);

    if (status[w]!=ROOT_NODE)
    {
        sprintf(this->CT.logBuffer,"Not a root: %lu",
            static_cast<unsigned long>(w));
        this -> Error(ERR_REJECTED,"Link",this->CT.logBuffer);
    }

    #endif

    if (key[v]<key[w])
    {
        TItem swap = v;
        v = w;
        w = swap;
    }

    if (v==bucket[rank[v]]) bucket[rank[v]] = UNDEFINED;
    if (w==bucket[rank[w]]) bucket[rank[w]] = UNDEFINED;

    pred[v] = w;
    status[v] = UNMARKED_CHILD;
    previous[v] = UNDEFINED;
    TItem x = first[w];
    first[w] = v;

    if (x!=UNDEFINED)
    {
        previous[x] = v;
        next[v] = x;
    }
    else next[v] = UNDEFINED;

    rank[w]++;

    #if defined(_TRACING_)

    if (this->CT.traceData) Display();

    #endif    

    if (bucket[rank[w]]==UNDEFINED) bucket[rank[w]] = w;
    else Link(w,bucket[rank[w]]);
}


// Split the tree containing v which must have a predecessor x.
// The node v becomes a root.
// If status[x]==MARKED_CHILD, a recursive call Cut(x) occurs.

template <class TItem,class TKey>
void fibonacciHeap<TItem,TKey>::Cut(TItem v) throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (v>=n) NoSuchItem("Cut",v);

    if (status[v]!=UNMARKED_CHILD && status[v]!=MARKED_CHILD)
    {
        sprintf(this->CT.logBuffer,"Cut node is a root: %lu",
            static_cast<unsigned long>(v));
        this -> Error(ERR_REJECTED,"Cut",this->CT.logBuffer);
    }

    #endif

    #if defined(_TRACING_)

    if (this->CT.traceData) Display();

    #endif    

    TItem x = pred[v];
    TItem u = previous[v];
    TItem w = next[v];

    if (u==UNDEFINED) first[x] = w;
    else next[u] = w;

    if (w!=UNDEFINED) previous[w] = u;

    status[v] = ROOT_NODE;
    Push(v);

    if (minimal==UNDEFINED || key[v]<key[minimal]) minimal = v;

    #if defined(_TRACING_)

    if (this->CT.traceData) Display();

    #endif

    if (status[x]==MARKED_CHILD)
    {
        Cut(x);
    }
    else if (status[x]==UNMARKED_CHILD)
    {
        status[x] = MARKED_CHILD;
    }

    rank[x]--;
}


template <class TItem,class TKey>
void fibonacciHeap<TItem,TKey>::Delete(TItem w) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (w>=n || status[w]==NOT_QUEUED) NoSuchItem("Delete",w);

    #endif

    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerPrioQ] -> Enable();

    #endif

    TItem x = first[w];

    while (x!=UNDEFINED)
    {
        TItem y = next[x];
        Cut(x);
        x = y;
    }

    if (status[w]==UNMARKED_CHILD || status[w]==MARKED_CHILD) Cut(w);

    status[w] = NOT_QUEUED;
    card--;
    Restore();

    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerPrioQ] -> Disable();

    #endif

    #if defined(_TRACING_)

    if (this->CT.traceData) Display();

    #endif
}


template <class TItem,class TKey>
TItem fibonacciHeap<TItem,TKey>::Delete() throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (card==0) this -> Error(ERR_REJECTED,"Delete","Heap is empty");

    #endif

    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerPrioQ] -> Enable();

    #endif

    TItem ret = minimal;
    Delete(minimal);

    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerPrioQ] -> Disable();

    #endif

    return ret;
}


template <class TItem,class TKey>
TKey fibonacciHeap<TItem,TKey>::Key(TItem w) const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (w>=n || status[w]==NOT_QUEUED) NoSuchItem("Key",w);

    #endif

    return key[w];
}


template <class TItem,class TKey>
void fibonacciHeap<TItem,TKey>::ChangeKey(TItem w,TKey alpha) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (w>=n || status[w]==NOT_QUEUED) NoSuchItem("ChangeKey",w);

    #endif

    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerPrioQ] -> Enable();

    #endif

    if (alpha>key[w])
    {
        key[w] = alpha;
        TItem x = first[w];

        while (x!=UNDEFINED)
        {
            TItem y = next[x];
            if (key[x]<key[w]) Cut(x);
            x = y;
        }
    }
    else
    {
        key[w] = alpha;

        if (   status[w]!=ROOT_NODE
            && status[w]!=NOT_QUEUED
            && pred[w]!=UNDEFINED
            && key[pred[w]]>alpha)
        {
            Cut(w);
        }
    }

    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerPrioQ] -> Disable();

    #endif

    #if defined(_TRACING_)

    if (this->CT.traceData) Display();

    #endif
}


template <class TItem,class TKey>
TItem fibonacciHeap<TItem,TKey>::Peek() const throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (card==0) this -> Error(ERR_REJECTED,"PEEK","Heap is empty");

    #endif

    return minimal;
}


template class fibonacciHeap<TIndex,TFloat>;

#ifndef _BIG_NODES_

template class fibonacciHeap<TNode,TFloat>;

#endif

//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, May 2000
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   fibonacciHeap.cpp
/// \brief  #fibonacciHeap class implementation

#include "fibonacciHeap.h"
#include "treeView.h"


template <class TItem,class TKey>
fibonacciHeap<TItem,TKey>::fibonacciHeap(TItem nn,goblinController &thisContext)
    throw() : managedObject(thisContext)
{
    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerPrioQ] -> Enable();

    #endif

    n = nn;
    k = nn;
    UNDEFINED = n;

    pred = new TItem[n];
    first = new TItem[n];
    next = new TItem[n];
    nextLink = new TItem[n];
    previous = new TItem[n];
    rank = new TItem[n];
    status = new TStatus[n];
    bucket = new TItem[k];
    key = new TKey[n];

    card = 0;
    minimal = UNDEFINED;
    firstLink = UNDEFINED;

    for (TItem v=0;v<n;v++) status[v] = NOT_QUEUED;
    for (TItem v=0;v<k;v++) bucket[v] = UNDEFINED;
    for (TItem v=0;v<n;v++) nextLink[v] = UNDEFINED;

    this -> LogEntry(LOG_MEM,"...Fibonacci heap instanciated");

    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerPrioQ] -> Disable();

    #endif
}


template <class TItem,class TKey>
void fibonacciHeap<TItem,TKey>::Init() throw()
{
    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerPrioQ] -> Enable();

    #endif

    if (card*100<n)
    {
        while (!Empty()) Delete();
    }
    else
    {
        for (TItem v=0;v<n;v++) status[v] = NOT_QUEUED;
        for (TItem v=0;v<k;v++) bucket[v] = UNDEFINED;
        for (TItem v=0;v<n;v++) nextLink[v] = UNDEFINED;

        card = 0;
        minimal = UNDEFINED;
        firstLink = UNDEFINED;
    }

    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerPrioQ] -> Disable();

    #endif
}


template <class TItem,class TKey>
unsigned long fibonacciHeap<TItem,TKey>::Size() const throw()
{
    return
          sizeof(fibonacciHeap<TItem,TKey>)
        + managedObject::Allocated()
        + fibonacciHeap::Allocated();
}


template <class TItem,class TKey>
unsigned long fibonacciHeap<TItem,TKey>::Allocated() const throw()
{
    return
          (6*n+k)*sizeof(TItem)
        + n*sizeof(char)
        + n*sizeof(TKey);
}


static THandle LH = NoHandle;

template <class TItem,class TKey>
char* fibonacciHeap<TItem,TKey>::Display() const throw()
{
    if (this->CT.displayMode>0)
    {
        if (card==0) return NULL;

        goblinTreeView G(n,this->CT);
        G.InitPredecessors();

        for (TItem i=0;i<n;i++)
        {
            if (status[i]==NOT_QUEUED) G.SetNodeColour(i,NoNode);
            else
            {
                G.SetNodeColour(i,TNode(status[i]));
                G.SetDist(i,TFloat(key[i]));

                TItem x = first[i];

                while (x!=UNDEFINED)
                {
                    #if defined(_FAILSAVE_)

                    if (status[x]!=UNMARKED_CHILD && status[x]!=MARKED_CHILD)
                    {
                        sprintf(this->CT.logBuffer,"Inconsistent item: %lu",
                            static_cast<unsigned long>(x));
                        InternalError1("Display");
                    }

                    #endif

                    TArc a = G.InsertArc(TNode(i),TNode(x));
                    G.SetPred(TNode(x),2*a);
                    x = next[x];
                }
            }
        }

        G.Layout_PredecessorTree();
        G.Display();
    }
    else
    {
        this -> LogEntry(MSG_TRACE,"Fibonacci heap");
        LH = this->LogStart(MSG_TRACE2,"    ");

        if (card>0)
        {
            for (TItem i=0;i<k;i++)
            {
                if (bucket[i]!=UNDEFINED)
                {
                    #if defined(_FAILSAVE_)

                    if (status[bucket[i]]!=ROOT_NODE)
                    {
                        sprintf(this->CT.logBuffer,"Inconsistent bucket: %lu",
                            static_cast<unsigned long>(i));
                        InternalError1("Display");
                    }

                    #endif

                    this -> LogAppend(LH,"  ");
                    Display(bucket[i]);
                }
            }

            this -> LogEnd(LH);
        }
        else this -> LogEnd(LH,"---");
    }

    return NULL;
}


template <class TItem,class TKey>
void fibonacciHeap<TItem,TKey>::Display(TItem v) const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (v>=n || status[v]==NOT_QUEUED) NoSuchItem("Display",v);

    #endif

    sprintf(this->CT.logBuffer,"%lu[%g]",
        static_cast<unsigned long>(v),static_cast<double>(key[v]));
    this -> LogAppend(LH,this->CT.logBuffer);

    TItem x = first[v];

    if (x!=UNDEFINED)
    {
        this -> LogAppend(LH," (");

        while (x!=UNDEFINED)
        {
            #if defined(_FAILSAVE_)

            if (status[x]!=UNMARKED_CHILD && status[x]!=MARKED_CHILD)
            {
                sprintf(this->CT.logBuffer,"Inconsistent item: %lu",
                    static_cast<unsigned long>(x));
                InternalError1("Display");
            }

            #endif

            Display(x);
            x = next[x];

            if (x!=UNDEFINED) this -> LogAppend(LH," ");
        }

        this -> LogAppend(LH,")");
    }
}


template <class TItem,class TKey>
fibonacciHeap<TItem,TKey>::~fibonacciHeap() throw()
{
    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerPrioQ] -> Enable();

    #endif

    delete[] pred;
    delete[] first;
    delete[] next;
    delete[] previous;
    delete[] rank;
    delete[] status;
    delete[] bucket;
    delete[] nextLink;
    delete[] key;

    this -> LogEntry(LOG_MEM,"...Fibonacci heap disallocated");

    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerPrioQ] -> Disable();

    #endif
}


template <class TItem,class TKey>
bool fibonacciHeap<TItem,TKey>::IsMember(TItem w)
    const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (w>=n) NoSuchItem("Insert",w);

    #endif

    return (status[w]!=NOT_QUEUED);
}


template <class TItem,class TKey>
void fibonacciHeap<TItem,TKey>::Insert(TItem w,TKey alpha)
    throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (w>=n) NoSuchItem("Insert",w);

    if (status[w]!=NOT_QUEUED)
    {
        sprintf(this->CT.logBuffer,"Already on queue: %lu",
            static_cast<unsigned long>(w));
        Error(ERR_REJECTED,"Insert",this->CT.logBuffer);
    }

    #endif

    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerPrioQ] -> Enable();

    #endif

    first[w] = UNDEFINED;
    rank[w] = 0;
    status[w] = ROOT_NODE;
    key[w] = alpha;
    nextLink[w] = UNDEFINED;
    card++;
    Push(w);

    if (minimal==UNDEFINED || alpha<key[minimal]) minimal = w;

    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerPrioQ] -> Disable();

    #endif

    #if defined(_TRACING_)

    if (this->CT.traceData) Display();

    #endif
}


// Add w to the list of root nodes

template <class TItem,class TKey>
void fibonacciHeap<TItem,TKey>::Push(TItem w) throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (w>=n || status[w]!=ROOT_NODE)
    {
        sprintf(this->CT.logBuffer,"Not a root: %lu",
            static_cast<unsigned long>(w));
        this -> Error(ERR_REJECTED,"Push",this->CT.logBuffer);
    }

    #endif

    nextLink[w] = firstLink;
    firstLink = w;
}


// Consolidate the buckets and recompute the global minimum

template <class TItem,class TKey>
void fibonacciHeap<TItem,TKey>::Restore() throw()
{
    for (TItem i=0;i<k;i++) bucket[i] = UNDEFINED;

    minimal = UNDEFINED;

    TItem x = firstLink;

    while (x!=UNDEFINED)
    {
        if (status[x]==ROOT_NODE)
        {
            if (bucket[rank[x]]==UNDEFINED) bucket[rank[x]] = x;
            else Link(x,bucket[rank[x]]);
        }
        x = nextLink[x];
    }

    x = firstLink;
    TItem y = UNDEFINED;

    while (x!=UNDEFINED)
    {
        if (status[x]!=ROOT_NODE)
        {
            if (y==UNDEFINED) firstLink = nextLink[x];
            else nextLink[y] = nextLink[x];
        }
        else
        {
            y = x;

            if (minimal==UNDEFINED || key[x]<key[minimal]) minimal = x;
        }

        x = nextLink[x];
    }
}


// Merge two trees with respective roots v and w which have equal rank.
// If the bucket (rank+1) is occupied, link is called recursively.

template <class TItem,class TKey>
void fibonacciHeap<TItem,TKey>::Link(TItem v,TItem w) throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (v>=n) NoSuchItem("Link",v);

    if (status[v]!=ROOT_NODE)
    {
        sprintf(this->CT.logBuffer,"Not a root: %lu",
            static_cast<unsigned long>(v));
        this -> Error(ERR_REJECTED,"Link",this->CT.logBuffer);
    }

    if (w>=n) NoSuchItem("Link",w);

    if (status[w]!=ROOT_NODE)
    {
        sprintf(this->CT.logBuffer,"Not a root: %lu",
            static_cast<unsigned long>(w));
        this -> Error(ERR_REJECTED,"Link",this->CT.logBuffer);
    }

    #endif

    if (key[v]<key[w])
    {
        TItem swap = v;
        v = w;
        w = swap;
    }

    if (v==bucket[rank[v]]) bucket[rank[v]] = UNDEFINED;
    if (w==bucket[rank[w]]) bucket[rank[w]] = UNDEFINED;

    pred[v] = w;
    status[v] = UNMARKED_CHILD;
    previous[v] = UNDEFINED;
    TItem x = first[w];
    first[w] = v;

    if (x!=UNDEFINED)
    {
        previous[x] = v;
        next[v] = x;
    }
    else next[v] = UNDEFINED;

    rank[w]++;

    #if defined(_TRACING_)

    if (this->CT.traceData) Display();

    #endif    

    if (bucket[rank[w]]==UNDEFINED) bucket[rank[w]] = w;
    else Link(w,bucket[rank[w]]);
}


// Split the tree containing v which must have a predecessor x.
// The node v becomes a root.
// If status[x]==MARKED_CHILD, a recursive call Cut(x) occurs.

template <class TItem,class TKey>
void fibonacciHeap<TItem,TKey>::Cut(TItem v) throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (v>=n) NoSuchItem("Cut",v);

    if (status[v]!=UNMARKED_CHILD && status[v]!=MARKED_CHILD)
    {
        sprintf(this->CT.logBuffer,"Cut node is a root: %lu",
            static_cast<unsigned long>(v));
        this -> Error(ERR_REJECTED,"Cut",this->CT.logBuffer);
    }

    #endif

    #if defined(_TRACING_)

    if (this->CT.traceData) Display();

    #endif    

    TItem x = pred[v];
    TItem u = previous[v];
    TItem w = next[v];

    if (u==UNDEFINED) first[x] = w;
    else next[u] = w;

    if (w!=UNDEFINED) previous[w] = u;

    status[v] = ROOT_NODE;
    Push(v);

    if (minimal==UNDEFINED || key[v]<key[minimal]) minimal = v;

    #if defined(_TRACING_)

    if (this->CT.traceData) Display();

    #endif

    if (status[x]==MARKED_CHILD)
    {
        Cut(x);
    }
    else if (status[x]==UNMARKED_CHILD)
    {
        status[x] = MARKED_CHILD;
    }

    rank[x]--;
}


template <class TItem,class TKey>
void fibonacciHeap<TItem,TKey>::Delete(TItem w) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (w>=n || status[w]==NOT_QUEUED) NoSuchItem("Delete",w);

    #endif

    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerPrioQ] -> Enable();

    #endif

    TItem x = first[w];

    while (x!=UNDEFINED)
    {
        TItem y = next[x];
        Cut(x);
        x = y;
    }

    if (status[w]==UNMARKED_CHILD || status[w]==MARKED_CHILD) Cut(w);

    status[w] = NOT_QUEUED;
    card--;
    Restore();

    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerPrioQ] -> Disable();

    #endif

    #if defined(_TRACING_)

    if (this->CT.traceData) Display();

    #endif
}


template <class TItem,class TKey>
TItem fibonacciHeap<TItem,TKey>::Delete() throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (card==0) this -> Error(ERR_REJECTED,"Delete","Heap is empty");

    #endif

    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerPrioQ] -> Enable();

    #endif

    TItem ret = minimal;
    Delete(minimal);

    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerPrioQ] -> Disable();

    #endif

    return ret;
}


template <class TItem,class TKey>
TKey fibonacciHeap<TItem,TKey>::Key(TItem w) const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (w>=n || status[w]==NOT_QUEUED) NoSuchItem("Key",w);

    #endif

    return key[w];
}


template <class TItem,class TKey>
void fibonacciHeap<TItem,TKey>::ChangeKey(TItem w,TKey alpha) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (w>=n || status[w]==NOT_QUEUED) NoSuchItem("ChangeKey",w);

    #endif

    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerPrioQ] -> Enable();

    #endif

    if (alpha>key[w])
    {
        key[w] = alpha;
        TItem x = first[w];

        while (x!=UNDEFINED)
        {
            TItem y = next[x];
            if (key[x]<key[w]) Cut(x);
            x = y;
        }
    }
    else
    {
        key[w] = alpha;

        if (   status[w]!=ROOT_NODE
            && status[w]!=NOT_QUEUED
            && pred[w]!=UNDEFINED
            && key[pred[w]]>alpha)
        {
            Cut(w);
        }
    }

    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerPrioQ] -> Disable();

    #endif

    #if defined(_TRACING_)

    if (this->CT.traceData) Display();

    #endif
}


template <class TItem,class TKey>
TItem fibonacciHeap<TItem,TKey>::Peek() const throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (card==0) this -> Error(ERR_REJECTED,"PEEK","Heap is empty");

    #endif

    return minimal;
}


template class fibonacciHeap<TIndex,TFloat>;

#ifndef _BIG_NODES_

template class fibonacciHeap<TNode,TFloat>;

#endif
