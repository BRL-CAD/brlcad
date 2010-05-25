
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, September 1998
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   staticStack.cpp
/// \brief  #staticStack class implementation

#include "staticStack.h"


template <class TItem,class TKey>
staticStack<TItem,TKey>::staticStack(TItem nn,goblinController &thisContext)
    throw() : managedObject(thisContext), indexSet<TItem>(nn,thisContext)
{
    n = nn;
    prev = new TItem[n];
    set = NULL;
    bottom = top = n;
    master = true;
    depth = 0;

    for (TItem i=0;i<n;i++) prev[i] = n;

    this -> LogEntry(LOG_MEM,"...Static stack instanciated");
}


template <class TItem,class TKey>
staticStack<TItem,TKey>::staticStack(staticStack<TItem,TKey> &S) throw() :
    managedObject(S.Context(),1), indexSet<TItem>(S.n,S.Context())
{
    n = S.n;
    prev = S.prev;
    bottom = top = n;
    master = false;
    depth = 0;

    if (!S.set)
    {
        S.set = new THandle[n];

        for (TItem i=0;i<n;i++)
        {
            if (prev[i]<n || i==S.bottom) S.set[i] = S.Handle();
            else S.set[i] = NoHandle;
        }
    }

    set = S.set;

    this -> LogEntry(LOG_MEM,"...Static stack instanciated");
}


template <class TItem,class TKey>
unsigned long staticStack<TItem,TKey>::Size() const throw()
{
    return
          sizeof(staticStack<TItem,TKey>)
        + managedObject::Allocated()
        + staticStack::Allocated();
}


template <class TItem,class TKey>
unsigned long staticStack<TItem,TKey>::Allocated() const throw()
{
    unsigned long ret = 0;

    if (master) ret += n*sizeof(TItem);
    if (master && set) ret += n*sizeof(THandle);

    return ret;
}


template <class TItem,class TKey>
char* staticStack<TItem,TKey>::Display() const throw()
{
    this -> LogEntry(MSG_TRACE,"Stack");

    if (Empty()) this -> LogEntry(MSG_TRACE2,"    ---");
    else
    {
        TItem i = top;
        TItem counter = 0;

        THandle LH = this->LogStart(MSG_TRACE2,"   ");

        for (;prev[i]!=n;i=prev[i])
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

        sprintf(this->CT.logBuffer,"%lu (bottom)",static_cast<unsigned long>(i));
        this -> LogEnd(LH,this->CT.logBuffer);
    }

    return NULL;
}


template <class TItem,class TKey>
staticStack<TItem,TKey>::~staticStack() throw()
{
    if (master)
    {
        delete[] prev;
        if (set) delete[] set;
    }
    else
    {
        while (!Empty()) Delete();
    }

    this -> LogEntry(LOG_MEM,"...Static stack disallocated");
}


template <class TItem,class TKey>
void staticStack<TItem,TKey>::Insert(TItem w,TKey alpha,staticStack::TOptInsert mode)
    throw(ERRange,ERCheck)
{
    #if defined(_FAILSAVE_)

    if (w>=n) NoSuchItem("Insert",w);

    #endif

    if (prev[w]!=n || w==bottom)
    {
        if (mode==INSERT_NO_THROW) return;

        sprintf(this->CT.logBuffer,"%lu is already on the stack",static_cast<unsigned long>(w));
        this -> Error(ERR_CHECK,"Insert",this->CT.logBuffer);
    }

    prev[w] = top;
    top = w;
    depth++;

    if (depth==1) bottom = w;

    if (set) set[w] = TItem(this->OH);
}


template <class TItem,class TKey>
void staticStack<TItem,TKey>::Insert(TItem w,staticStack::TOptInsert mode)
    throw(ERRange,ERCheck)
{
    Insert(w,0,mode);
}


template <class TItem,class TKey>
void staticStack<TItem,TKey>::Insert(TItem w,TKey alpha)
    throw(ERRange,ERCheck)
{
    Insert(w,alpha,INSERT_TWICE_THROW);
}


template <class TItem,class TKey>
TItem staticStack<TItem,TKey>::Delete() throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (Empty()) this -> Error(ERR_REJECTED,"Delete","Queue is empty");

    #endif

    TItem temp = top;
    top = prev[top];
    prev[temp] = n;
    depth--;

    if (depth==0) bottom = n;

    return temp;
}


template <class TItem,class TKey>
void staticStack<TItem,TKey>::ChangeKey(TItem w,TKey alpha) throw(ERRange,ERRejected)
{
}


template <class TItem,class TKey>
TItem staticStack<TItem,TKey>::Peek() const throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (Empty()) this -> Error(ERR_REJECTED,"Peek","Queue is empty");

    #endif

    return top;
}


template <class TItem,class TKey>
bool staticStack<TItem,TKey>::Empty() const throw()
{
    return (top==n);
}


template <class TItem,class TKey>
void staticStack<TItem,TKey>::Init() throw()
{
    while (!Empty()) Delete();
}


template <class TItem,class TKey>
TItem staticStack<TItem,TKey>::Cardinality() const throw()
{
    return depth;
}


template <class TItem,class TKey>
bool staticStack<TItem,TKey>::IsMember(TItem i) const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (i>=n) NoSuchItem("IsMember",i);

    #endif

    if (i==bottom) return true;

    if (set && prev[i]!=n) return (set[i]==this->OH);

    return (prev[i]!=n);
}


template <class TItem,class TKey>
TItem staticStack<TItem,TKey>::First() const throw()
{
    if (Empty()) return n;

    return top;
}


template <class TItem,class TKey>
TItem staticStack<TItem,TKey>::Successor(TItem i) const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (i>=n) NoSuchItem("Successor",i);

    #endif

    if (i==bottom) return n;

    return prev[i];
}


template class staticStack<unsigned short,TFloat>;
template class staticStack<unsigned long,TFloat>;

//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, September 1998
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   staticStack.cpp
/// \brief  #staticStack class implementation

#include "staticStack.h"


template <class TItem,class TKey>
staticStack<TItem,TKey>::staticStack(TItem nn,goblinController &thisContext)
    throw() : managedObject(thisContext), indexSet<TItem>(nn,thisContext)
{
    n = nn;
    prev = new TItem[n];
    set = NULL;
    bottom = top = n;
    master = true;
    depth = 0;

    for (TItem i=0;i<n;i++) prev[i] = n;

    this -> LogEntry(LOG_MEM,"...Static stack instanciated");
}


template <class TItem,class TKey>
staticStack<TItem,TKey>::staticStack(staticStack<TItem,TKey> &S) throw() :
    managedObject(S.Context(),1), indexSet<TItem>(S.n,S.Context())
{
    n = S.n;
    prev = S.prev;
    bottom = top = n;
    master = false;
    depth = 0;

    if (!S.set)
    {
        S.set = new THandle[n];

        for (TItem i=0;i<n;i++)
        {
            if (prev[i]<n || i==S.bottom) S.set[i] = S.Handle();
            else S.set[i] = NoHandle;
        }
    }

    set = S.set;

    this -> LogEntry(LOG_MEM,"...Static stack instanciated");
}


template <class TItem,class TKey>
unsigned long staticStack<TItem,TKey>::Size() const throw()
{
    return
          sizeof(staticStack<TItem,TKey>)
        + managedObject::Allocated()
        + staticStack::Allocated();
}


template <class TItem,class TKey>
unsigned long staticStack<TItem,TKey>::Allocated() const throw()
{
    unsigned long ret = 0;

    if (master) ret += n*sizeof(TItem);
    if (master && set) ret += n*sizeof(THandle);

    return ret;
}


template <class TItem,class TKey>
char* staticStack<TItem,TKey>::Display() const throw()
{
    this -> LogEntry(MSG_TRACE,"Stack");

    if (Empty()) this -> LogEntry(MSG_TRACE2,"    ---");
    else
    {
        TItem i = top;
        TItem counter = 0;

        THandle LH = this->LogStart(MSG_TRACE2,"   ");

        for (;prev[i]!=n;i=prev[i])
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

        sprintf(this->CT.logBuffer,"%lu (bottom)",static_cast<unsigned long>(i));
        this -> LogEnd(LH,this->CT.logBuffer);
    }

    return NULL;
}


template <class TItem,class TKey>
staticStack<TItem,TKey>::~staticStack() throw()
{
    if (master)
    {
        delete[] prev;
        if (set) delete[] set;
    }
    else
    {
        while (!Empty()) Delete();
    }

    this -> LogEntry(LOG_MEM,"...Static stack disallocated");
}


template <class TItem,class TKey>
void staticStack<TItem,TKey>::Insert(TItem w,TKey alpha,staticStack::TOptInsert mode)
    throw(ERRange,ERCheck)
{
    #if defined(_FAILSAVE_)

    if (w>=n) NoSuchItem("Insert",w);

    #endif

    if (prev[w]!=n || w==bottom)
    {
        if (mode==INSERT_NO_THROW) return;

        sprintf(this->CT.logBuffer,"%lu is already on the stack",static_cast<unsigned long>(w));
        this -> Error(ERR_CHECK,"Insert",this->CT.logBuffer);
    }

    prev[w] = top;
    top = w;
    depth++;

    if (depth==1) bottom = w;

    if (set) set[w] = TItem(this->OH);
}


template <class TItem,class TKey>
void staticStack<TItem,TKey>::Insert(TItem w,staticStack::TOptInsert mode)
    throw(ERRange,ERCheck)
{
    Insert(w,0,mode);
}


template <class TItem,class TKey>
void staticStack<TItem,TKey>::Insert(TItem w,TKey alpha)
    throw(ERRange,ERCheck)
{
    Insert(w,alpha,INSERT_TWICE_THROW);
}


template <class TItem,class TKey>
TItem staticStack<TItem,TKey>::Delete() throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (Empty()) this -> Error(ERR_REJECTED,"Delete","Queue is empty");

    #endif

    TItem temp = top;
    top = prev[top];
    prev[temp] = n;
    depth--;

    if (depth==0) bottom = n;

    return temp;
}


template <class TItem,class TKey>
void staticStack<TItem,TKey>::ChangeKey(TItem w,TKey alpha) throw(ERRange,ERRejected)
{
}


template <class TItem,class TKey>
TItem staticStack<TItem,TKey>::Peek() const throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (Empty()) this -> Error(ERR_REJECTED,"Peek","Queue is empty");

    #endif

    return top;
}


template <class TItem,class TKey>
bool staticStack<TItem,TKey>::Empty() const throw()
{
    return (top==n);
}


template <class TItem,class TKey>
void staticStack<TItem,TKey>::Init() throw()
{
    while (!Empty()) Delete();
}


template <class TItem,class TKey>
TItem staticStack<TItem,TKey>::Cardinality() const throw()
{
    return depth;
}


template <class TItem,class TKey>
bool staticStack<TItem,TKey>::IsMember(TItem i) const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (i>=n) NoSuchItem("IsMember",i);

    #endif

    if (i==bottom) return true;

    if (set && prev[i]!=n) return (set[i]==this->OH);

    return (prev[i]!=n);
}


template <class TItem,class TKey>
TItem staticStack<TItem,TKey>::First() const throw()
{
    if (Empty()) return n;

    return top;
}


template <class TItem,class TKey>
TItem staticStack<TItem,TKey>::Successor(TItem i) const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (i>=n) NoSuchItem("Successor",i);

    #endif

    if (i==bottom) return n;

    return prev[i];
}


template class staticStack<unsigned short,TFloat>;
template class staticStack<unsigned long,TFloat>;
