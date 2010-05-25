
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, September 2001
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   dynamicQueue.h
/// \brief  #dynamicQueue class interface

#ifndef _DYNAMIC_QUEUE_H_
#define _DYNAMIC_QUEUE_H_

#include "goblinQueue.h"


/// \addtogroup groupNodeBasedContainers
/// @{

/// \brief Implements a node based FIFO queue

template <class TItem,class TKey = TFloat>
class dynamicQueue : public goblinQueue<TItem,TKey>
{
private:

    struct queueMember
    {
        TItem           index;
        queueMember*    next;
    };

    queueMember*    first;
    queueMember*    last;
    TItem           n;
    TItem           length;

public:

    dynamicQueue(TItem nn,goblinController &thisContext) throw();
    ~dynamicQueue() throw();

    unsigned long   Size() const throw();
    unsigned long   Allocated() const throw();
    char*           Display() const throw();

    void            Insert(TItem w,TKey alpha = 0) throw(ERRange);
    TItem           Delete() throw(ERRejected);
    void            ChangeKey(TItem w,TKey alpha) throw(ERRange,ERRejected);
    TItem           Peek() const throw(ERRejected);
    bool            Empty() const throw();
    void            Init() throw();
    TItem           Cardinality() const throw();

};

/// @}

#endif

//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, September 2001
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   dynamicQueue.h
/// \brief  #dynamicQueue class interface

#ifndef _DYNAMIC_QUEUE_H_
#define _DYNAMIC_QUEUE_H_

#include "goblinQueue.h"


/// \addtogroup groupNodeBasedContainers
/// @{

/// \brief Implements a node based FIFO queue

template <class TItem,class TKey = TFloat>
class dynamicQueue : public goblinQueue<TItem,TKey>
{
private:

    struct queueMember
    {
        TItem           index;
        queueMember*    next;
    };

    queueMember*    first;
    queueMember*    last;
    TItem           n;
    TItem           length;

public:

    dynamicQueue(TItem nn,goblinController &thisContext) throw();
    ~dynamicQueue() throw();

    unsigned long   Size() const throw();
    unsigned long   Allocated() const throw();
    char*           Display() const throw();

    void            Insert(TItem w,TKey alpha = 0) throw(ERRange);
    TItem           Delete() throw(ERRejected);
    void            ChangeKey(TItem w,TKey alpha) throw(ERRange,ERRejected);
    TItem           Peek() const throw(ERRejected);
    bool            Empty() const throw();
    void            Init() throw();
    TItem           Cardinality() const throw();

};

/// @}

#endif
