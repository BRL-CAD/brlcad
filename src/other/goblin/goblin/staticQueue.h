
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, September 1998
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   staticQueue.h
/// \brief  #staticQueue class interface

#ifndef _STATIC_QUEUE_H_
#define _STATIC_QUEUE_H_

#include "goblinQueue.h"
#include "indexSet.h"


/// \addtogroup groupFixedIndexRangeContainers
/// @{

/// \brief Implements a fixed index range FIFO queue

template <class TItem,class TKey = TFloat>
class staticQueue : public goblinQueue<TItem,TKey>, public indexSet<TItem>
{
private:

    TItem*          next;
    THandle*        set;
    TItem           first;
    TItem           last;
    TItem           n;
    TItem           length;
    bool            master;

public:

    staticQueue(TItem nn,goblinController &thisContext) throw();
    staticQueue(staticQueue<TItem,TKey> &Q) throw();
    ~staticQueue() throw();

    unsigned long   Size() const throw();
    unsigned long   Allocated() const throw();
    char*           Display() const throw();

    /// \brief  Strategy for repeated queue insertions
    enum TOptInsert {
        INSERT_NO_THROW = 0,    ///< Do not complain when inserting elements twice
        INSERT_TWICE_THROW = 1  ///< Complain if an element is inserted twice
    };

    void  Insert(TItem w,TKey alpha,staticQueue::TOptInsert mode) throw(ERRange,ERCheck);
    void  Insert(TItem w,staticQueue::TOptInsert mode) throw(ERRange,ERCheck);

    // Implementation of the goblinQueue interface

    void            Insert(TItem w,TKey alpha = 0) throw(ERRange,ERCheck);

    TItem           Delete() throw(ERRejected);
    void            ChangeKey(TItem w,TKey alpha) throw(ERRange,ERRejected);
    TItem           Peek() const throw(ERRejected);
    bool            Empty() const throw();
    void            Init() throw();
    TItem           Cardinality() const throw();

    // Implementation of the indexSet interface

    bool            IsMember(TItem i) const throw(ERRange);
    TItem           First() const throw();
    TItem           Successor(const TItem i) const throw(ERRange);

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

/// \file   staticQueue.h
/// \brief  #staticQueue class interface

#ifndef _STATIC_QUEUE_H_
#define _STATIC_QUEUE_H_

#include "goblinQueue.h"
#include "indexSet.h"


/// \addtogroup groupFixedIndexRangeContainers
/// @{

/// \brief Implements a fixed index range FIFO queue

template <class TItem,class TKey = TFloat>
class staticQueue : public goblinQueue<TItem,TKey>, public indexSet<TItem>
{
private:

    TItem*          next;
    THandle*        set;
    TItem           first;
    TItem           last;
    TItem           n;
    TItem           length;
    bool            master;

public:

    staticQueue(TItem nn,goblinController &thisContext) throw();
    staticQueue(staticQueue<TItem,TKey> &Q) throw();
    ~staticQueue() throw();

    unsigned long   Size() const throw();
    unsigned long   Allocated() const throw();
    char*           Display() const throw();

    /// \brief  Strategy for repeated queue insertions
    enum TOptInsert {
        INSERT_NO_THROW = 0,    ///< Do not complain when inserting elements twice
        INSERT_TWICE_THROW = 1  ///< Complain if an element is inserted twice
    };

    void  Insert(TItem w,TKey alpha,staticQueue::TOptInsert mode) throw(ERRange,ERCheck);
    void  Insert(TItem w,staticQueue::TOptInsert mode) throw(ERRange,ERCheck);

    // Implementation of the goblinQueue interface

    void            Insert(TItem w,TKey alpha = 0) throw(ERRange,ERCheck);

    TItem           Delete() throw(ERRejected);
    void            ChangeKey(TItem w,TKey alpha) throw(ERRange,ERRejected);
    TItem           Peek() const throw(ERRejected);
    bool            Empty() const throw();
    void            Init() throw();
    TItem           Cardinality() const throw();

    // Implementation of the indexSet interface

    bool            IsMember(TItem i) const throw(ERRange);
    TItem           First() const throw();
    TItem           Successor(const TItem i) const throw(ERRange);

};

/// @}

#endif
