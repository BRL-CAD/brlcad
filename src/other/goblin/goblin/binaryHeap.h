
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, December 1999
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   binaryHeap.h
/// \brief  #binaryHeap class interface

#ifndef _BINARY_HEAP_H_
#define _BINARY_HEAP_H_

#include "goblinQueue.h"


/// \addtogroup groupPriorityQueues
/// @{

/// \brief Standard implementation of a priority queue using a binary tree

template <class TItem,class TKey>
class binaryHeap : public goblinQueue<TItem,TKey>
{
private:

    TItem*          v;
    TItem*          index;
    TKey*           key;

    TItem           maxIndex;
    TItem           n;
    TItem           UITEM_MAX() const throw() {return TItem(-1);};

public:

    binaryHeap(TItem nn,goblinController &thisContext) throw();
    ~binaryHeap() throw();

    void            Init() throw();
    unsigned long   Size() const throw();
    unsigned long   Allocated() const throw();

    void            Display(TItem i) const throw(ERRange);
    char*           Display() const throw();

private:

    void            UpHeap(TItem w) throw(ERRange);
    void            DownHeap(TItem w) throw(ERRange);

public:

    void            Insert(TItem w,TKey alpha) throw(ERRange,ERRejected);
    TItem           Delete() throw(ERRejected);
    void            Delete(TItem w) throw(ERRange);
    TKey            Key(TItem w) const throw(ERRange);
    void            ChangeKey(TItem w,TKey alpha) throw(ERRange);
    TItem           Peek() const throw(ERRejected);
    bool            Empty() const throw() {return (maxIndex==0);};
    TItem           Cardinality() const throw() {return maxIndex;};

};

/// @}

#endif
