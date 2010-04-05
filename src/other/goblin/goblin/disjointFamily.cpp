
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, October 1998
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   disjointFamily.cpp
/// \brief  #disjointFamily class implementation

#include "disjointFamily.h"
#include "treeView.h"


template <class TItem>
disjointFamily<TItem>::disjointFamily(TItem nn,goblinController &thisContext)
    throw() : managedObject(thisContext)
{
    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerUnionFind] -> Enable();

    #endif

    n = nn;
    UNDEFINED = n;

    B = new TItem[n];
    rank = new TItem[n];
    Init();

    this -> LogEntry(LOG_MEM,"...Disjoint set family allocated");

    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerUnionFind] -> Disable();

    #endif
}


template <class TItem>
void disjointFamily<TItem>::Init() throw()
{
    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerUnionFind] -> Enable();

    #endif

    for (TNode v=0;v<n;v++) B[v]=UNDEFINED;

    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerUnionFind] -> Disable();

    #endif
}


template <class TItem>
unsigned long disjointFamily<TItem>::Size() const throw()
{
    return
          sizeof(disjointFamily<TItem>)
        + managedObject::Allocated()
        + disjointFamily::Allocated();
}


template <class TItem>
unsigned long disjointFamily<TItem>::Allocated() const throw()
{
    return 2*n*(int)sizeof(TItem);
}


template <class TItem>
char* disjointFamily<TItem>::Display() const throw()
{
    if (this->CT.displayMode>0)
    {
        goblinTreeView G(n,this->CT);
        G.InitPredecessors();

        bool voidStructure = true;
        for (TItem i=0;i<n;i++)
        {
            if (B[i]==UNDEFINED) G.SetNodeColour(i,NoNode);
            else
            {
                G.SetNodeColour(i,TNode(rank[i]));
                G.SetDist(i,TFloat(i));
                voidStructure = false;

                if (B[i]!=i)
                {
                    TArc a = G.InsertArc(TNode(B[i]),TNode(i));
                    G.SetPred(TNode(i),2*a);
                }
            }
        }

        if (!voidStructure)
        {
            G.Layout_PredecessorTree();
            G.Display();
        }
    }
    else
    {
        this -> LogEntry(MSG_TRACE,"Disjoint set family");
        THandle LH = this->LogStart(MSG_TRACE2,"  ");
        TNode counter = 0;

        for (TNode v=0;v<n;v++)
            if (B[v]!=UNDEFINED)
            {
                if ((++counter)%10==0)
                {
                    this -> LogEnd(LH);
                    LH = this->LogStart(MSG_TRACE2,"  ");
                }
                sprintf(this->CT.logBuffer," %lu->%lu",
                    static_cast<unsigned long>(v),static_cast<unsigned long>(B[v]));
                this -> LogAppend(LH,this->CT.logBuffer);
                counter++;
            }

        this -> LogEnd(LH);
    }

    return NULL;
}


template <class TItem>
void disjointFamily<TItem>::Bud(TItem v) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (this->CT.logWarn && v>=n) NoSuchItem("Find",v);

    #endif

    B[v] = v;
    rank[v] = 1;
}


template <class TItem>
void disjointFamily<TItem>::Merge(TItem u,TItem v) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (u>=n || B[u]==UNDEFINED) NoSuchItem("Find",u);

    if (v>=n || B[v]==UNDEFINED) NoSuchItem("Find",v);

    #endif

    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerUnionFind] -> Enable();

    #endif

    u = Find(u);
    v = Find(v);

    if (rank[u]<rank[v]) B[u] = v;
    else
    {
        B[v] = u;
        if ((rank[v]==rank[u])&&(u!=v)) rank[u]++;
    }

    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerUnionFind] -> Disable();

    #endif

    if (this->CT.traceData) Display();
}


template <class TItem>
TItem disjointFamily<TItem>::Find(TItem v) const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (this->CT.logWarn && (v>=n || B[v]==UNDEFINED)) NoSuchItem("Find",v);

    #endif

    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerUnionFind] -> Enable();

    #endif

    TNode w = v;

    if (B[v]!=v) w = Find(B[v]);

    if (this->CT.methDSU==1 && B[v]!=w)
    {
        B[v] = w;
        if (this->CT.traceData) Display();
    }

    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerUnionFind] -> Disable();

    #endif

    return w;
}


template <class TItem>
disjointFamily<TItem>::~disjointFamily() throw()
{
    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerUnionFind] -> Enable();

    #endif

    delete[] B;
    delete[] rank;

    this -> LogEntry(LOG_MEM,"...Disjoint set family disallocated");

    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerUnionFind] -> Disable();

    #endif
}


template class disjointFamily<unsigned short>;
template class disjointFamily<unsigned long>;

