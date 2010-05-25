
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, November 2007
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   sequentialQueue.h
/// \brief  #sequentialQueueAdapter class interface

#ifndef _SEQUENTIAL_QUEUE_H_
#define _SEQUENTIAL_QUEUE_H_

#include "goblinQueue.h"
#include <deque>


/// \addtogroup groupSequentialContainers
/// @{

/// \brief Implements a sequential queue adapter for STL deques

template <class TItem,class TKey = TFloat>
class sequentialQueueAdapter : public goblinQueue<TItem,TKey>
{
private:

    deque<TItem>  DQ;

public:

    sequentialQueueAdapter(TItem _n,goblinController &thisContext) throw() :
        managedObject(thisContext), DQ(0) {};
    ~sequentialQueueAdapter() throw() {};

    unsigned long   Size() const throw() {return 0;};
    unsigned long   Allocated() const throw() {return 0;};
    char*           Display() const throw() {return NULL;};

    /// \brief  Extract the embeded STL deque object
    ///
    /// \return  A reference of the embeded STL deque object
    deque<TItem>& GetDeque() {return DQ;};

    // Implementation of the goblinQueue interface

    void  Insert(TItem w,TKey alpha = 0) throw(ERRange) {DQ.push_back(w);};
    TItem  Delete() throw(ERRejected)
    {
        TItem ret = DQ.front();
        DQ.pop_front();
        return ret;
    };
    TItem  Peek() const throw(ERRejected) {return DQ.front();};
    bool  Empty() const throw() {return DQ.empty();};
    TItem  Cardinality() const throw() {return DQ.size();};

};

/// @}

#endif

//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, November 2007
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   sequentialQueue.h
/// \brief  #sequentialQueueAdapter class interface

#ifndef _SEQUENTIAL_QUEUE_H_
#define _SEQUENTIAL_QUEUE_H_

#include "goblinQueue.h"
#include <deque>


/// \addtogroup groupSequentialContainers
/// @{

/// \brief Implements a sequential queue adapter for STL deques

template <class TItem,class TKey = TFloat>
class sequentialQueueAdapter : public goblinQueue<TItem,TKey>
{
private:

    deque<TItem>  DQ;

public:

    sequentialQueueAdapter(TItem _n,goblinController &thisContext) throw() :
        managedObject(thisContext), DQ(0) {};
    ~sequentialQueueAdapter() throw() {};

    unsigned long   Size() const throw() {return 0;};
    unsigned long   Allocated() const throw() {return 0;};
    char*           Display() const throw() {return NULL;};

    /// \brief  Extract the embeded STL deque object
    ///
    /// \return  A reference of the embeded STL deque object
    deque<TItem>& GetDeque() {return DQ;};

    // Implementation of the goblinQueue interface

    void  Insert(TItem w,TKey alpha = 0) throw(ERRange) {DQ.push_back(w);};
    TItem  Delete() throw(ERRejected)
    {
        TItem ret = DQ.front();
        DQ.pop_front();
        return ret;
    };
    TItem  Peek() const throw(ERRejected) {return DQ.front();};
    bool  Empty() const throw() {return DQ.empty();};
    TItem  Cardinality() const throw() {return DQ.size();};

};

/// @}

#endif
