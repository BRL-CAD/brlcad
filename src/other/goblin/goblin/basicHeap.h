
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, September 1998
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   basicHeap.h
/// \brief  #basicHeap class interface

#ifndef _BASIC_HEAP_H_
#define _BASIC_HEAP_H_

#include "goblinQueue.h"


/// \addtogroup groupPriorityQueues
/// @{

/// \brief Naive implementation of a priority queue

template <class TItem,class TKey>
class basicHeap : public goblinQueue<TItem,TKey>
{
private:

    TItem*          v;
    TKey*           key;
    TItem           maxIndex;
    TItem           n;

public:

    basicHeap(TItem nn,goblinController &thisContext) throw();
    ~basicHeap() throw();

    void            Init() throw();
    unsigned long   Size() const throw();
    unsigned long   Allocated() const throw();
    char*           Display() const throw();

    void            Insert(TItem w,TKey alpha) throw(ERRange,ERRejected);
    void            Delete(TItem w) throw(ERRange,ERRejected);
    TKey            Key(TItem w) const throw(ERRange);
    void            ChangeKey(TItem w,TKey alpha) throw(ERRange,ERRejected);
    TItem           Delete() throw(ERRejected);
    TItem           Peek() const throw(ERRejected);
    bool            Empty() const throw() {return (maxIndex==0);};
    TItem           Cardinality() const throw() {return maxIndex;};

};

/// @}

#endif

//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, September 1998
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   basicHeap.h
/// \brief  #basicHeap class interface

#ifndef _BASIC_HEAP_H_
#define _BASIC_HEAP_H_

#include "goblinQueue.h"


/// \addtogroup groupPriorityQueues
/// @{

/// \brief Naive implementation of a priority queue

template <class TItem,class TKey>
class basicHeap : public goblinQueue<TItem,TKey>
{
private:

    TItem*          v;
    TKey*           key;
    TItem           maxIndex;
    TItem           n;

public:

    basicHeap(TItem nn,goblinController &thisContext) throw();
    ~basicHeap() throw();

    void            Init() throw();
    unsigned long   Size() const throw();
    unsigned long   Allocated() const throw();
    char*           Display() const throw();

    void            Insert(TItem w,TKey alpha) throw(ERRange,ERRejected);
    void            Delete(TItem w) throw(ERRange,ERRejected);
    TKey            Key(TItem w) const throw(ERRange);
    void            ChangeKey(TItem w,TKey alpha) throw(ERRange,ERRejected);
    TItem           Delete() throw(ERRejected);
    TItem           Peek() const throw(ERRejected);
    bool            Empty() const throw() {return (maxIndex==0);};
    TItem           Cardinality() const throw() {return maxIndex;};

};

/// @}

#endif
