
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, September 1998
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   dynamicStack.h
/// \brief  #dynamicStack class interface

#ifndef _DYNAMIC_STACK_H_
#define _DYNAMIC_STACK_H_

#include "goblinQueue.h"


/// \addtogroup groupNodeBasedContainers
/// @{

/// \brief Implements a node based stack

template <class TItem,class TKey = TFloat>
class dynamicStack : public goblinQueue<TItem,TKey>
{
private:

    struct stackMember
    {
        TItem           index;
        stackMember*    prev;
    };

    stackMember*    top;
    TItem           n;
    TItem           depth;

public:

    dynamicStack(TItem nn,goblinController &thisContext) throw();
    ~dynamicStack() throw();

    unsigned long   Size() const throw();
    unsigned long   Allocated() const throw();
    char*           Display() const throw();

    void            Insert(TItem w,TKey alpha = 0) throw(ERRange);
    void            Delete(TItem w) throw(ERRange);
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
//  Initially written by Christian Fremuth-Paeger, September 1998
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   dynamicStack.h
/// \brief  #dynamicStack class interface

#ifndef _DYNAMIC_STACK_H_
#define _DYNAMIC_STACK_H_

#include "goblinQueue.h"


/// \addtogroup groupNodeBasedContainers
/// @{

/// \brief Implements a node based stack

template <class TItem,class TKey = TFloat>
class dynamicStack : public goblinQueue<TItem,TKey>
{
private:

    struct stackMember
    {
        TItem           index;
        stackMember*    prev;
    };

    stackMember*    top;
    TItem           n;
    TItem           depth;

public:

    dynamicStack(TItem nn,goblinController &thisContext) throw();
    ~dynamicStack() throw();

    unsigned long   Size() const throw();
    unsigned long   Allocated() const throw();
    char*           Display() const throw();

    void            Insert(TItem w,TKey alpha = 0) throw(ERRange);
    void            Delete(TItem w) throw(ERRange);
    TItem           Delete() throw(ERRejected);
    void            ChangeKey(TItem w,TKey alpha) throw(ERRange,ERRejected);
    TItem           Peek() const throw(ERRejected);
    bool            Empty() const throw();
    void            Init() throw();
    TItem           Cardinality() const throw();

};

/// @}

#endif
