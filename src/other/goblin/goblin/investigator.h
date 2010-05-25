
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, December 1998
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   investigator.h
/// \brief  #investigator class interface

#ifndef _ITERATOR_H_
#define _ITERATOR_H_

#include "managedObject.h"


/// \addtogroup groupInvestigators
/// @{

/// \brief  The base class for all graph investigators

class investigator : public virtual managedObject
{
public:

    virtual unsigned long Size() const throw() = 0;

    /// \brief  Reinitialize a managed graph search
    ///
    /// This restarts the graph search. That is, the iterators of all incidence
    /// lists are positioned the First() arc. Practically, all arcs are marked unvisited.
    virtual void Reset() throw() = 0;

    /// \brief  Reinitialize a particular incidence list
    ///
    /// \param v  A node index ranged [0,1,..,n-1]
    ///
    /// Position the iterator for the incidence list of node v to the First(v)
    /// arc. Practically, all arcs with start node v are marked unvisited.
    virtual void Reset(TNode v) throw(ERRange) = 0;

    /// \brief  Read an arc from a node incidence list and mark it as visited
    ///
    /// \param v  A node index ranged [0,1,..,n-1]
    /// \return   An arc index ranged [0,1,..,2*mAct-1]
    ///
    /// This returns the currently indexed arc a in the incidence list of node
    /// v, and then positions the hidden iterator for this incidence list to
    /// the arc Right(a,v). If all arcs with start node v have been visited,
    /// an ERRejected exception is raised.
    virtual TArc Read(TNode v) throw(ERRange,ERRejected) = 0;

    /// \brief  Return the currently indexed arc in a node incidence list
    ///
    /// \param v  A node index ranged [0,1,..,n-1]
    /// \return   An arc index ranged [0,1,..,2*mAct-1]
    ///
    /// For the investigator indexed by H, this returns the currently indexed
    /// arc in the incidence list of node v. The indexed arc does not change,
    /// that is, this arc is visited once more. If all arcs with start node
    /// v have been visited, an ERRejected exception is raised.
    virtual TArc Peek(TNode v) throw(ERRange,ERRejected) = 0;

    /// \brief  Check for unvisited arcs in a node incidence list
    ///
    /// \param v      A node index ranged [0,1,..,n-1]
    /// \retval true  There are unvisited arcs in the incidence list of v
    virtual bool Active(TNode v) const throw(ERRange) = 0;

};

/// @}


#endif


//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, December 1998
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   investigator.h
/// \brief  #investigator class interface

#ifndef _ITERATOR_H_
#define _ITERATOR_H_

#include "managedObject.h"


/// \addtogroup groupInvestigators
/// @{

/// \brief  The base class for all graph investigators

class investigator : public virtual managedObject
{
public:

    virtual unsigned long Size() const throw() = 0;

    /// \brief  Reinitialize a managed graph search
    ///
    /// This restarts the graph search. That is, the iterators of all incidence
    /// lists are positioned the First() arc. Practically, all arcs are marked unvisited.
    virtual void Reset() throw() = 0;

    /// \brief  Reinitialize a particular incidence list
    ///
    /// \param v  A node index ranged [0,1,..,n-1]
    ///
    /// Position the iterator for the incidence list of node v to the First(v)
    /// arc. Practically, all arcs with start node v are marked unvisited.
    virtual void Reset(TNode v) throw(ERRange) = 0;

    /// \brief  Read an arc from a node incidence list and mark it as visited
    ///
    /// \param v  A node index ranged [0,1,..,n-1]
    /// \return   An arc index ranged [0,1,..,2*mAct-1]
    ///
    /// This returns the currently indexed arc a in the incidence list of node
    /// v, and then positions the hidden iterator for this incidence list to
    /// the arc Right(a,v). If all arcs with start node v have been visited,
    /// an ERRejected exception is raised.
    virtual TArc Read(TNode v) throw(ERRange,ERRejected) = 0;

    /// \brief  Return the currently indexed arc in a node incidence list
    ///
    /// \param v  A node index ranged [0,1,..,n-1]
    /// \return   An arc index ranged [0,1,..,2*mAct-1]
    ///
    /// For the investigator indexed by H, this returns the currently indexed
    /// arc in the incidence list of node v. The indexed arc does not change,
    /// that is, this arc is visited once more. If all arcs with start node
    /// v have been visited, an ERRejected exception is raised.
    virtual TArc Peek(TNode v) throw(ERRange,ERRejected) = 0;

    /// \brief  Check for unvisited arcs in a node incidence list
    ///
    /// \param v      A node index ranged [0,1,..,n-1]
    /// \retval true  There are unvisited arcs in the incidence list of v
    virtual bool Active(TNode v) const throw(ERRange) = 0;

};

/// @}


#endif

