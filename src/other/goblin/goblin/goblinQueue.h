
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, December 1998
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   goblinQueue.h
/// \brief  #goblinQueue class interface

#ifndef _QUEUE_H_
#define _QUEUE_H_

#ifndef _MANAGED_OBJECT_H_
#include "managedObject.h"
#endif


/// \addtogroup groupContainers
/// @{


/// \brief Interface class for container objects

template <class TItem,class TKey>
class goblinQueue : public virtual managedObject
{
public:

    virtual unsigned long   Size() const throw() = 0;

    /// \brief  Insert an index into the queue
    ///
    /// \param w      The index to be inserted
    /// \param alpha  The priority of the inserted index
    virtual void   Insert(TItem w,TKey alpha) throw(ERRange,ERRejected,ERCheck) = 0;

    /// \brief  Delete an element from the queue
    ///
    /// \return   The index of the deleted element
    virtual TItem  Delete() throw(ERRejected) = 0;

    /// \brief  Insert an index into the queue
    ///
    /// \param w      An element index
    /// \param alpha  A new priority for this element
    virtual void   ChangeKey(TItem w,TKey alpha) throw(ERRange,ERRejected) = 0;

    /// \brief  Query what is coming next on the queue
    ///
    /// \return   The index of the element to be deleted next
    virtual TItem  Peek() const throw(ERRejected) = 0;

    /// \brief  Check if the queue is empty
    ///
    /// \retval true  The queue is empty
    virtual bool   Empty() const throw() = 0;

    /// \brief  Delete all elements from the queue efficently
    virtual void   Init() throw() = 0;

    /// \brief  Query the current queue cardinality
    ///
    /// \return   The queue cardinality
    virtual TItem  Cardinality() const throw() = 0;

};

/// @}

#endif
