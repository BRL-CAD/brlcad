
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, January 2000
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   nestedFamily.cpp
/// \brief  #nestedFamily class implementation

#include "nestedFamily.h"
#include "treeView.h"


template <class TItem>
nestedFamily<TItem>::nestedFamily(TItem nn,TItem mm,
    goblinController &thisContext) throw() : managedObject(thisContext)
{
    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerUnionFind] -> Enable();

    #endif

    n = nn;
    m = mm;
    compress = this->CT.methDSU;
    UNDEFINED = n+m;

    B = new TItem[n+m];
    depth = new TItem[n+m];
    set = new TItem[n+m];
    canonical = new TItem[m];
    first = new TItem[m];
    next = new TItem[n+m];

    Init();

    this -> LogEntry(LOG_MEM,"...Shrinking family allocated");

    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerUnionFind] -> Disable();

    #endif
}


template <class TItem>
void nestedFamily<TItem>::Init() throw()
{
    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerUnionFind] -> Enable();

    #endif

    for (TNode v=0;v<n+m;v++) B[v]=UNDEFINED;
    for (TNode v=0;v<n;v++) Bud(v);

    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerUnionFind] -> Disable();

    #endif
}


template <class TItem>
unsigned long nestedFamily<TItem>::Size() const throw()
{
    return
          sizeof(nestedFamily<TItem>)
        + managedObject::Allocated()
        + nestedFamily::Allocated();
}


template <class TItem>
unsigned long nestedFamily<TItem>::Allocated() const throw()
{
    return (4*n+6*m)*(int)sizeof(TItem);
}


template <class TItem>
nestedFamily<TItem>::~nestedFamily() throw()
{
    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerUnionFind] -> Enable();

    #endif

    delete[] B;
    delete[] depth;
    delete[] set;
    delete[] canonical;
    delete[] first;
    delete[] next;

    this -> LogEntry(LOG_MEM,"...Shrinking family disallocated");

    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerUnionFind] -> Disable();

    #endif
}


static THandle LH = NoHandle;
 
template <class TItem>
char* nestedFamily<TItem>::Display() const throw()
{
    if (this->CT.displayMode>0)
    {
        goblinTreeView G(n+m,this->CT);
        G.InitPredecessors();

        bool voidStructure = true;

        for (TItem i=0;i<n+m;i++)
        {
            if (B[i]==UNDEFINED || (i<n && B[i]==i))
            {
                G.SetNodeColour(i,NoNode);
            }
            else
            {
                G.SetNodeColour(i,TNode(depth[i]));
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
        this -> LogEntry(MSG_TRACE,"Shrinking family");

        for (TNode v=0;v<n+m;v++)
            if (B[v]!=UNDEFINED && Top(v))
            {
                LH = this->LogStart(MSG_TRACE2,"   ");
                Display(v);
                this -> LogEnd(LH);
            }
    }

    return NULL;
}


template <class TItem>
void nestedFamily<TItem>::Display(TItem v) const throw(ERRange)
{
    if (v<n)
    {
        sprintf(this->CT.logBuffer,"%lu",static_cast<unsigned long>(v));
        this -> LogAppend(LH,this->CT.logBuffer);
    }
    else
    {
        sprintf(this->CT.logBuffer,"(%lu",static_cast<unsigned long>(v));
        this -> LogAppend(LH,this->CT.logBuffer);

        TNode u = UNDEFINED;
        TNode w = first[v-n];

        while (u!=w)
        {
            u = w;
            this -> LogAppend(LH," ");
            Display(u);
            w = next[w];
        }

        this -> LogAppend(LH,")");
    }
}


template <class TItem>
void nestedFamily<TItem>::Bud(TItem v) throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (v>=n) NoSuchItem("Bud",v);

    if (B[v]!=UNDEFINED)
    {
        sprintf(this->CT.logBuffer,"Already present: %lu",static_cast<unsigned long>(v));
        Error(ERR_REJECTED,"Bud",this->CT.logBuffer);
    }

    #endif

    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerUnionFind] -> Enable();

    #endif

    B[v] = v;
    depth[v] = 1;
    next[v] = UNDEFINED;
    set[v] = v;

    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerUnionFind] -> Disable();

    #endif
}


template <class TItem>
TItem nestedFamily<TItem>::MakeSet() throw(ERRejected)
{
    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerUnionFind] -> Enable();

    #endif

    for (TNode u=n;u<n+m;u++)
    {
        if (B[u]==UNDEFINED)
        {
            B[u] = u;
            depth[u] = 1;
            first[u-n] = UNDEFINED;
            next[u] = UNDEFINED;
            set[u] = u;
            canonical[u-n] = UNDEFINED;

            #if defined(_TIMERS_)

            this -> CT.globalTimer[TimerUnionFind] -> Disable();

            #endif

            return u;
        }
    }

    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerUnionFind] -> Disable();

    #endif

    #if defined(_FAILSAVE_)

    this -> Error(ERR_REJECTED,"MakeSet","No more sets available");

    #endif

    throw ERRejected();
}


template <class TItem>
void nestedFamily<TItem>::Merge(TItem s,TItem v) throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (s<n || s>=n+m)
    {
        sprintf(this->CT.logBuffer,"Not a set: %lu",static_cast<unsigned long>(s));
        Error(ERR_RANGE,"Merge",this->CT.logBuffer);
    }

    if (canonical[s-n]!=UNDEFINED)
    {
        sprintf(this->CT.logBuffer,"Set is already fixed: %lu",static_cast<unsigned long>(s));
        Error(ERR_REJECTED,"Merge",this->CT.logBuffer);
    }

    if (v>=n && canonical[v-n]==UNDEFINED)
    {
        sprintf(this->CT.logBuffer,"Item has not been fixed: %lu",static_cast<unsigned long>(v));
        Error(ERR_REJECTED,"Merge",this->CT.logBuffer);
    }

    if (!Top(v))
    {
        sprintf(this->CT.logBuffer,"Item is already shrunk: %lu",static_cast<unsigned long>(v));
        Error(ERR_REJECTED,"Merge",this->CT.logBuffer);
    }

    #endif

    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerUnionFind] -> Enable();

    #endif

    TNode w = Find(v);
    TNode u = Find(s);

    if (depth[u]<depth[w])
    {
        B[u] = w;
        set[w] = s;
    }
    else
    {
        B[w] = u;
        set[u] = s;

        if (depth[w]==depth[u]) depth[u]++;
    }

    if (first[s-n]!=UNDEFINED)
    {
        next[v] = first[s-n];
        first[s-n] = v;
    }
    else
    {
        first[s-n] = v;
        next[v] = v;
    }

    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerUnionFind] -> Disable();

    #endif
}


template <class TItem>
void nestedFamily<TItem>::FixSet(TItem s) throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (s<n || s>=n+m)
    {
        sprintf(this->CT.logBuffer,"Not a set: %lu",static_cast<unsigned long>(s));
        Error(ERR_RANGE,"FixSet",this->CT.logBuffer);
    }

    if (canonical[s-n]!=UNDEFINED)
    {
        sprintf(this->CT.logBuffer,"Set is already fixed: %lu",static_cast<unsigned long>(s));
        Error(ERR_REJECTED,"FixSet",this->CT.logBuffer);
    }

    if (first[s-n]==UNDEFINED)
    {
        sprintf(this->CT.logBuffer,"Set is empty: %lu",static_cast<unsigned long>(s));
        Error(ERR_REJECTED,"FixSet",this->CT.logBuffer);
    }

    #endif

    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerUnionFind] -> Enable();

    #endif

    TNode u = Find(s);
    canonical[s-n] = u;

    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerUnionFind] -> Disable();

    #endif

    if (this->CT.traceData) Display();
}


template <class TItem>
bool nestedFamily<TItem>::Top(TItem v) const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (v>=n+m) NoSuchItem("Top",v);

    #endif

    if (B[v]==UNDEFINED)
    {
        #if defined(_FAILSAVE_)

        if (this->CT.logMeth>1 && this->CT.logWarn)
        {
            sprintf(this->CT.logBuffer,"No such item: %lu",static_cast<unsigned long>(v));
            Error(MSG_WARN,"Top",this->CT.logBuffer);
        }

        #endif

        return true;
    }

    return (next[v]==UNDEFINED);
}


template <class TItem>
TItem nestedFamily<TItem>::Find(TItem v) const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (v>=n+m) NoSuchItem("Find",v);

    #endif

    if (B[v]==UNDEFINED)
    {
        #if defined(_FAILSAVE_)

        if (this->CT.logMeth>1 && this->CT.logWarn)
        {
            sprintf(this->CT.logBuffer,"No such item: %lu",static_cast<unsigned long>(v));
            Error(MSG_WARN,"Find",this->CT.logBuffer);
        }

        #endif

        return UNDEFINED;
    }

    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerUnionFind] -> Enable();

    #endif

    TNode w = v;

    if (B[v]!=v) w = Find(B[v]);
    if (compress) B[v] = w;

    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerUnionFind] -> Disable();

    #endif

    return w;
}


template <class TItem>
TItem nestedFamily<TItem>::Set(TItem v) const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (v>=n+m) NoSuchItem("Set",v);

    #endif

    if (B[v]==UNDEFINED)
    {
        #if defined(_FAILSAVE_)

        if (this->CT.logMeth>1 && this->CT.logWarn)
        {
            sprintf(this->CT.logBuffer,"No such item: %lu",static_cast<unsigned long>(v));
            Error(MSG_WARN,"Set",this->CT.logBuffer);
        }

        #endif

        return UNDEFINED;
    }

    return set[Find(v)];
}


template <class TItem>
TItem nestedFamily<TItem>::First(TItem v) const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (v>=n+m || v<n)
    {
        sprintf(this->CT.logBuffer,"Not a set: %lu",static_cast<unsigned long>(v));
        Error(ERR_RANGE,"First",this->CT.logBuffer);
    }

    #endif

    if (B[v]==UNDEFINED)
    {
        #if defined(_FAILSAVE_)

        if (this->CT.logMeth>1 && this->CT.logWarn)
        {
            sprintf(this->CT.logBuffer,"No such item: %lu",static_cast<unsigned long>(v));
            Error(MSG_WARN,"First",this->CT.logBuffer);
        }

        #endif

        return UNDEFINED;
    }

    if (B[v]==UNDEFINED)
    {
        #if defined(_FAILSAVE_)

        if (this->CT.logMeth>1 && this->CT.logWarn)
        {
            sprintf(this->CT.logBuffer,"Empty set: %lu",static_cast<unsigned long>(v));
            Error(MSG_WARN,"First",this->CT.logBuffer);
        }

        #endif

        return UNDEFINED;
    }

    return first[v-n];
}


template <class TItem>
TItem nestedFamily<TItem>::Next(TItem v) const throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (v>=n+m) NoSuchItem("Next",v);

    if (B[v]==UNDEFINED)
    {
        sprintf(this->CT.logBuffer,"No such item: %lu",static_cast<unsigned long>(v));
        Error(ERR_REJECTED,"Next",this->CT.logBuffer);
    }

    if (next[v]==UNDEFINED)
    {
        sprintf(this->CT.logBuffer,"Toplevel item: %lu",static_cast<unsigned long>(v));
        Error(ERR_REJECTED,"Next",this->CT.logBuffer);
    }

    #endif

    return next[v];
}


template <class TItem>
void nestedFamily<TItem>::Adjust(TItem s,TItem b) const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (s>=n+m || s<n)
    {
        sprintf(this->CT.logBuffer,"Not a set: %lu",static_cast<unsigned long>(s));
        Error(ERR_RANGE,"Adjust",this->CT.logBuffer);
    }

    if (b>=n+m) NoSuchItem("Adjust",b);

    #endif

    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerUnionFind] -> Enable();

    #endif

    TNode u = UNDEFINED;
    TNode w = first[s-n];

    while (u!=w)
    {
        u = w;
        B[u] = b;
        if (u>=n) Adjust(u,b);
        w = next[w];
    }

    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerUnionFind] -> Disable();

    #endif
}


template <class TItem>
void nestedFamily<TItem>::Split(TItem s) throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (s>=n+m || s<n || B[s]==UNDEFINED)
    {
        sprintf(this->CT.logBuffer,"Not a set: %lu",static_cast<unsigned long>(s));
        Error(ERR_RANGE,"Split",this->CT.logBuffer);
    }

    if (first[s-n]==UNDEFINED)
    {
        sprintf(this->CT.logBuffer,"Empty set: %lu",static_cast<unsigned long>(s));
        Error(ERR_REJECTED,"Split",this->CT.logBuffer);
    }

    TNode u = Find(s);

    if (set[u]!=s)
    {
        sprintf(this->CT.logBuffer,"Not a toplevel set: %lu",static_cast<unsigned long>(s));
        Error(ERR_REJECTED,"Split",this->CT.logBuffer);
    }

    if (canonical[s-n]==UNDEFINED)
    {
        sprintf(this->CT.logBuffer,"Set has not been fixed: %lu",static_cast<unsigned long>(s));
        Error(ERR_REJECTED,"Split",this->CT.logBuffer);
    }

    #endif

    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerUnionFind] -> Enable();

    #endif

    TNode v = UNDEFINED;
    TNode w = first[s-n];

    while (v!=w)
    {
        v = w;

        if (v<n)
        {
            B[v] = v;
            if (compress) set[v] = v;
        }
        else
        {
            TNode x = canonical[v-n];
            B[v] = B[x] = x;
            if (compress) Adjust(v,x);
            set[x] = v;
        }

        w = next[w];
        next[v] = UNDEFINED;
    }

    B[s] = UNDEFINED;

    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerUnionFind] -> Disable();

    #endif

    if (this->CT.traceData) Display();
}


template <class TItem>
void nestedFamily<TItem>::Block(TItem v) const throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (v>=n+m) NoSuchItem("Block",v);

    if (B[v]==UNDEFINED)
    {
        sprintf(this->CT.logBuffer,"No such item: %lu",static_cast<unsigned long>(v));
        Error(ERR_REJECTED,"Block",this->CT.logBuffer);
    }

    #endif

    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerUnionFind] -> Enable();

    #endif

    TNode u = UNDEFINED;
    TNode w = first[v-n];

    while (u!=w)
    {
        u = w;

        if (u<n)
        {
            B[u] = u;
            if (compress) set[u] = u;
        }
        else
        {
            TNode x = canonical[u-n];
            B[u] = B[x] = x;
            if (compress) Adjust(u,x);
            set[x] = u;
        }

        w = next[w];
    }

    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerUnionFind] -> Disable();

    #endif
}


template <class TItem>
void nestedFamily<TItem>::UnBlock(TItem v) const throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (v>=n+m) NoSuchItem("Block",v);

    if (B[v]==UNDEFINED)
    {
        sprintf(this->CT.logBuffer,"No such item: %lu",static_cast<unsigned long>(v));
        Error(ERR_REJECTED,"UnBlock",this->CT.logBuffer);
    }

    #endif

    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerUnionFind] -> Enable();

    #endif

    TNode u = UNDEFINED;
    TNode w = first[v-n];
    TNode x = canonical[v-n];

    while (u!=w)
    {
        u = w;
        if (u<n) B[u] = x;
        else B[canonical[u-n]] = x;

        w = next[w];
    }

    set[x] = v;

    #if defined(_TIMERS_)

    this -> CT.globalTimer[TimerUnionFind] -> Disable();

    #endif
}


template class nestedFamily<unsigned short>;
template class nestedFamily<unsigned long>;
