
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, September 1998
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   staticQueue.cpp
/// \brief  #staticQueue class implementation

#include "staticQueue.h"


template <class TItem,class TKey>
staticQueue<TItem,TKey>::staticQueue(TItem _n,goblinController &thisContext)
    throw() : managedObject(thisContext), indexSet<TItem>(_n,thisContext)
{
    n = _n;
    next = new TItem[n];
    set = NULL;
    first = n;
    master = true;
    length = 0;

    for (TItem i=0;i<n;i++) next[i] = n;

    this -> LogEntry(LOG_MEM,"...Static queue allocated");
}


template <class TItem,class TKey>
staticQueue<TItem,TKey>::staticQueue(staticQueue<TItem,TKey> &Q) throw() :
    managedObject(Q.Context(),1), indexSet<TItem>(Q.n,Q.Context())
{
    n = Q.n;
    next = Q.next;
    first = n;
    master = false;
    length = 0;

    if (!Q.set)
    {
        Q.set = new THandle[n];

        for (TItem i=0;i<n;i++)
        {
            if (next[i]<n) Q.set[i] = Q.Handle();
            else Q.set[i] = NoHandle;
        }
    }

    set = Q.set;

    this -> LogEntry(LOG_MEM,"...Static queue instanciated");
}


template <class TItem,class TKey>
unsigned long staticQueue<TItem,TKey>::Size() const throw()
{
    return
          sizeof(staticQueue<TItem,TKey>)
        + managedObject::Allocated()
        + staticQueue::Allocated();
}


template <class TItem,class TKey>
unsigned long staticQueue<TItem,TKey>::Allocated() const throw()
{
    unsigned long ret = 0;

    if (master) ret += n*sizeof(TItem);
    if (master && set) ret += n*sizeof(THandle);

    return ret;
}


template <class TItem,class TKey>
char* staticQueue<TItem,TKey>::Display() const throw()
{
    this -> LogEntry(MSG_TRACE,"Queue");

    if (Empty()) this -> LogEntry(MSG_TRACE2,"    ---");
    else
    {
        TItem i = first;
        TItem counter = 0;

        THandle LH = this->LogStart(MSG_TRACE2,"   ");

        for (;i!=last;i=next[i])
        {
            if (counter>0 && counter%10==0)
            {
                this -> LogEnd(LH);
                LH = this->LogStart(MSG_TRACE2,"   ");
            }

            sprintf(this->CT.logBuffer,"%lu, ",static_cast<unsigned long>(i));
            this -> LogAppend(LH,this->CT.logBuffer);

            counter++;
        }

        if (counter>0 && counter%10==0)
        {
            this -> LogEnd(LH);
            LH = this->LogStart(MSG_TRACE2,"   ");
        }

        sprintf(this->CT.logBuffer,"%lu (last in)",static_cast<unsigned long>(i));
        this -> LogEnd(LH,this->CT.logBuffer);
    }

    return NULL;
}


template <class TItem,class TKey>
staticQueue<TItem,TKey>::~staticQueue() throw()
{
    if (master)
    {
        delete[] next;
        if (set) delete[] set;
    }
    else
    {
        while (!Empty()) Delete();
    }

    this -> LogEntry(LOG_MEM,"...Static queue disallocated");
}


template <class TItem,class TKey>
void staticQueue<TItem,TKey>::Insert(TItem w,TKey alpha,staticQueue::TOptInsert mode)
    throw(ERRange,ERCheck)
{
    #if defined(_FAILSAVE_)

    if (w>=n) NoSuchItem("Insert",w);

    #endif

    if (next[w]!=n)
    {
        if (mode==INSERT_NO_THROW) return;

        sprintf(this->CT.logBuffer,"%lu is already on the queue",static_cast<unsigned long>(w));
        this -> Error(ERR_CHECK,"Insert",this->CT.logBuffer);
    }

    if (Empty()) last=first=w;
    else
    {
        next[last] = w;
        last = w;
    }

    next[last] = last;
    length++;

    if (set) set[w] = TItem(this->OH);
}


template <class TItem,class TKey>
TItem staticQueue<TItem,TKey>::Delete() throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (Empty()) this -> Error(ERR_REJECTED,"Delete","Queue is empty");

    #endif

    TItem temp = first;
    first = next[first];
    if (temp==first) first=n;
    next[temp] = n;
    length--;
    return temp;
}


template <class TItem,class TKey>
void staticQueue<TItem,TKey>::Insert(TItem w,staticQueue::TOptInsert mode)
    throw(ERRange,ERCheck)
{
    Insert(w,0,mode);
}


template <class TItem,class TKey>
void staticQueue<TItem,TKey>::Insert(TItem w,TKey alpha)
    throw(ERRange,ERCheck)
{
    Insert(w,alpha,INSERT_TWICE_THROW);
}


template <class TItem,class TKey>
void staticQueue<TItem,TKey>::ChangeKey(TItem w,TKey alpha) throw(ERRange,ERRejected)
{
}


template <class TItem,class TKey>
TItem staticQueue<TItem,TKey>::Peek() const throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (Empty()) this -> Error(ERR_REJECTED,"Peek","Queue is empty");

    #endif

    return first;
}


template <class TItem,class TKey>
bool staticQueue<TItem,TKey>::Empty() const throw()
{
    return (first==n);
}


template <class TItem,class TKey>
void staticQueue<TItem,TKey>::Init() throw()
{
    while (!Empty()) Delete();
}


template <class TItem,class TKey>
TItem staticQueue<TItem,TKey>::Cardinality() const throw()
{
    return length;
}


template <class TItem,class TKey>
bool staticQueue<TItem,TKey>::IsMember(TItem i) const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (i>=n) NoSuchItem("IsMember",i);

    #endif

    if (set && next[i]!=n) return (set[i]==this->OH);

    return (next[i]!=n);
}


template <class TItem,class TKey>
TItem staticQueue<TItem,TKey>::First() const throw()
{
    if (Empty()) return n;

    return first;
}


template <class TItem,class TKey>
TItem staticQueue<TItem,TKey>::Successor(TItem i) const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (i>=n) NoSuchItem("Successor",i);

    #endif

    if (i==last) return n;

    return next[i];
}


template class staticQueue<unsigned short,TFloat>;
template class staticQueue<unsigned long,TFloat>;

//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, September 1998
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   staticQueue.cpp
/// \brief  #staticQueue class implementation

#include "staticQueue.h"


template <class TItem,class TKey>
staticQueue<TItem,TKey>::staticQueue(TItem _n,goblinController &thisContext)
    throw() : managedObject(thisContext), indexSet<TItem>(_n,thisContext)
{
    n = _n;
    next = new TItem[n];
    set = NULL;
    first = n;
    master = true;
    length = 0;

    for (TItem i=0;i<n;i++) next[i] = n;

    this -> LogEntry(LOG_MEM,"...Static queue allocated");
}


template <class TItem,class TKey>
staticQueue<TItem,TKey>::staticQueue(staticQueue<TItem,TKey> &Q) throw() :
    managedObject(Q.Context(),1), indexSet<TItem>(Q.n,Q.Context())
{
    n = Q.n;
    next = Q.next;
    first = n;
    master = false;
    length = 0;

    if (!Q.set)
    {
        Q.set = new THandle[n];

        for (TItem i=0;i<n;i++)
        {
            if (next[i]<n) Q.set[i] = Q.Handle();
            else Q.set[i] = NoHandle;
        }
    }

    set = Q.set;

    this -> LogEntry(LOG_MEM,"...Static queue instanciated");
}


template <class TItem,class TKey>
unsigned long staticQueue<TItem,TKey>::Size() const throw()
{
    return
          sizeof(staticQueue<TItem,TKey>)
        + managedObject::Allocated()
        + staticQueue::Allocated();
}


template <class TItem,class TKey>
unsigned long staticQueue<TItem,TKey>::Allocated() const throw()
{
    unsigned long ret = 0;

    if (master) ret += n*sizeof(TItem);
    if (master && set) ret += n*sizeof(THandle);

    return ret;
}


template <class TItem,class TKey>
char* staticQueue<TItem,TKey>::Display() const throw()
{
    this -> LogEntry(MSG_TRACE,"Queue");

    if (Empty()) this -> LogEntry(MSG_TRACE2,"    ---");
    else
    {
        TItem i = first;
        TItem counter = 0;

        THandle LH = this->LogStart(MSG_TRACE2,"   ");

        for (;i!=last;i=next[i])
        {
            if (counter>0 && counter%10==0)
            {
                this -> LogEnd(LH);
                LH = this->LogStart(MSG_TRACE2,"   ");
            }

            sprintf(this->CT.logBuffer,"%lu, ",static_cast<unsigned long>(i));
            this -> LogAppend(LH,this->CT.logBuffer);

            counter++;
        }

        if (counter>0 && counter%10==0)
        {
            this -> LogEnd(LH);
            LH = this->LogStart(MSG_TRACE2,"   ");
        }

        sprintf(this->CT.logBuffer,"%lu (last in)",static_cast<unsigned long>(i));
        this -> LogEnd(LH,this->CT.logBuffer);
    }

    return NULL;
}


template <class TItem,class TKey>
staticQueue<TItem,TKey>::~staticQueue() throw()
{
    if (master)
    {
        delete[] next;
        if (set) delete[] set;
    }
    else
    {
        while (!Empty()) Delete();
    }

    this -> LogEntry(LOG_MEM,"...Static queue disallocated");
}


template <class TItem,class TKey>
void staticQueue<TItem,TKey>::Insert(TItem w,TKey alpha,staticQueue::TOptInsert mode)
    throw(ERRange,ERCheck)
{
    #if defined(_FAILSAVE_)

    if (w>=n) NoSuchItem("Insert",w);

    #endif

    if (next[w]!=n)
    {
        if (mode==INSERT_NO_THROW) return;

        sprintf(this->CT.logBuffer,"%lu is already on the queue",static_cast<unsigned long>(w));
        this -> Error(ERR_CHECK,"Insert",this->CT.logBuffer);
    }

    if (Empty()) last=first=w;
    else
    {
        next[last] = w;
        last = w;
    }

    next[last] = last;
    length++;

    if (set) set[w] = TItem(this->OH);
}


template <class TItem,class TKey>
TItem staticQueue<TItem,TKey>::Delete() throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (Empty()) this -> Error(ERR_REJECTED,"Delete","Queue is empty");

    #endif

    TItem temp = first;
    first = next[first];
    if (temp==first) first=n;
    next[temp] = n;
    length--;
    return temp;
}


template <class TItem,class TKey>
void staticQueue<TItem,TKey>::Insert(TItem w,staticQueue::TOptInsert mode)
    throw(ERRange,ERCheck)
{
    Insert(w,0,mode);
}


template <class TItem,class TKey>
void staticQueue<TItem,TKey>::Insert(TItem w,TKey alpha)
    throw(ERRange,ERCheck)
{
    Insert(w,alpha,INSERT_TWICE_THROW);
}


template <class TItem,class TKey>
void staticQueue<TItem,TKey>::ChangeKey(TItem w,TKey alpha) throw(ERRange,ERRejected)
{
}


template <class TItem,class TKey>
TItem staticQueue<TItem,TKey>::Peek() const throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (Empty()) this -> Error(ERR_REJECTED,"Peek","Queue is empty");

    #endif

    return first;
}


template <class TItem,class TKey>
bool staticQueue<TItem,TKey>::Empty() const throw()
{
    return (first==n);
}


template <class TItem,class TKey>
void staticQueue<TItem,TKey>::Init() throw()
{
    while (!Empty()) Delete();
}


template <class TItem,class TKey>
TItem staticQueue<TItem,TKey>::Cardinality() const throw()
{
    return length;
}


template <class TItem,class TKey>
bool staticQueue<TItem,TKey>::IsMember(TItem i) const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (i>=n) NoSuchItem("IsMember",i);

    #endif

    if (set && next[i]!=n) return (set[i]==this->OH);

    return (next[i]!=n);
}


template <class TItem,class TKey>
TItem staticQueue<TItem,TKey>::First() const throw()
{
    if (Empty()) return n;

    return first;
}


template <class TItem,class TKey>
TItem staticQueue<TItem,TKey>::Successor(TItem i) const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (i>=n) NoSuchItem("Successor",i);

    #endif

    if (i==last) return n;

    return next[i];
}


template class staticQueue<unsigned short,TFloat>;
template class staticQueue<unsigned long,TFloat>;
