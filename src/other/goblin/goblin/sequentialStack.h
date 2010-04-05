
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, November 2007
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   sequentialStack.h
/// \brief  #sequentialStackAdapter class interface

#ifndef _SEQUENTIAL_STACK_H_
#define _SEQUENTIAL_STACK_H_

#include "goblinQueue.h"
#include <vector>


/// \addtogroup groupSequentialContainers
/// @{

/// \brief Implements a sequential stack adapter for STL vectors

template <class TItem,class TKey = TFloat>
class sequentialStackAdapter : public goblinQueue<TItem,TKey>
{
private:

    vector<TItem>  DQ;

public:

    sequentialStackAdapter(TItem _n,goblinController &thisContext) throw() :
        managedObject(thisContext), DQ(_n) {DQ.erase(DQ.begin(),DQ.end());};
    ~sequentialStackAdapter() throw() {};

    unsigned long   Size() const throw() {return 0;};
    unsigned long   Allocated() const throw() {return 0;};
    char*           Display() const throw() {return NULL;};

    /// \brief  Extract the embeded STL vector object
    ///
    /// \return  A reference of the embeded STL vector object
    deque<TItem>& GetVector() {return DQ;};

    // Implementation of the goblinQueue interface

    void  Insert(TItem w,TKey alpha = 0) throw(ERRange) {DQ.push_back(w);};
    TItem  Delete() throw(ERRejected)
    {
        TItem ret = DQ.back();
        DQ.pop_back();
        return ret;
    };
    TItem  Peek() const throw(ERRejected) {return DQ.back();};
    bool  Empty() const throw() {return DQ.empty();};
    TItem  Cardinality() const throw() {return DQ.size();};

};

/// @}

#endif
