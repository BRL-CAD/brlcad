
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, May 2000
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   fibonacciHeap.h
/// \brief  #fibonacciHeap class interface

#ifndef _FIBONACCI_HEAP_H_
#define _FIBONACCI_HEAP_H_

#include "goblinQueue.h"


/// \addtogroup groupPriorityQueues
/// @{

/// \brief Implementation of a priority queue with long-term almost linear running times

template <class TItem,class TKey>
class fibonacciHeap : public goblinQueue<TItem,TKey>
{
private:

    enum TStatus {
        UNMARKED_CHILD = 0,
        MARKED_CHILD = 1,
        ROOT_NODE = 2,
        NOT_QUEUED = 3};

    TItem*          pred;
    TItem*          first;
    TItem*          next;
    TItem*          previous;
    TItem*          rank;
    TStatus*        status;
    TItem*          bucket;
    TItem*          nextLink;
    TKey*           key;

    TItem           card;
    TItem           n;
    TItem           k;
    TItem           minimal;
    TItem           firstLink;
    TItem           UNDEFINED;

public:

    fibonacciHeap(TItem nn,goblinController &thisContext) throw();
    ~fibonacciHeap() throw();

    unsigned long   Size() const throw();
    unsigned long   Allocated() const throw();

    void            Init() throw();
    void            Display(TItem i) const throw(ERRange);
    char*           Display() const throw();

private:

    void            Link(TItem v,TItem w) throw(ERRange,ERRejected);
    void            Push(TItem w) throw(ERRejected);
    void            Restore() throw();
    void            Cut(TItem w) throw(ERRange,ERRejected);

public:

    void            Insert(TItem w,TKey alpha) throw(ERRange,ERRejected);
    TItem           Delete() throw(ERRejected);
    void            Delete(TItem w) throw(ERRange);
    TKey            Key(TItem w) const throw(ERRange);
    void            ChangeKey(TItem w,TKey alpha) throw(ERRange);
    TItem           Peek() const throw(ERRejected);
    bool            IsMember(TItem w) const throw(ERRange);
    bool            Empty() const throw() {return (card==0);};
    TItem           Cardinality() const throw() {return card;};

};

/// @}

#endif
