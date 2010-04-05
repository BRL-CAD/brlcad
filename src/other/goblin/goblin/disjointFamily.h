
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, October 1998
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   disjointFamily.h
/// \brief  #disjointFamily class interface

#ifndef _DISJOINT_FAMILY_H_
#define _DISJOINT_FAMILY_H_

#include "managedObject.h"


/// \addtogroup setFamilies
/// @{

/// \brief  Efficient implementation of the disjoint set union process

template <class TItem>
class disjointFamily : public virtual managedObject
{
private:

    TItem*          B;
    TItem*          rank;
    TItem           n;
    TItem           UNDEFINED;

public:

    disjointFamily(TItem _n,goblinController& _CT) throw();
    ~disjointFamily() throw();

    /// \brief  Initialize the disjoint node set structure
    ///
    /// After this operation, it is Find(v)==UNDEFINED for every element v.
    void            Init() throw();

    unsigned long   Size() const throw();
    unsigned long   Allocated() const throw();
    char*           Display() const throw();

    /// \brief  Start a single element set
    ///
    /// \param v  An index ranged [0,1,..,n-1]
    ///
    /// After this operation, it is Find(v)==v.
    void Bud(TItem v) throw(ERRange);

    /// \brief  Merge two disjoint sets represented by arbitrary elements
    ///
    /// \param u  An index ranged [0,1,..,n-1]
    /// \param v  An index ranged [0,1,..,n-1]
    ///
    /// After this operation, it is Find(u)==Find(v).
    void Merge(TItem u,TItem v) throw(ERRange);

    /// \brief  Retrieve the set containing a given element
    ///
    /// \param v  An index ranged [0,1,..,n-1]
    /// \return   An index ranged [0,1,..,n-1] or UNDEFINED
    ///
    /// This returns the index of an arbitrary but fixed element in the same set
    /// as v, the so-called canonical element in this set. Only when sets are
    /// merged, the canonical element changes.
    ///
    /// In order to check if two elements x,y are in the same set, it is sufficient
    /// to evaluate Find(x)==Find(y) and Find(u)!=UNDEFINED. If the context
    /// parameter methDSU is set, Find() does not only lookup the canonical element,
    /// but also performs some path compression to reduce the future lookup times.
    TItem Find(TItem v) const throw(ERRange);

};

/// @}

#endif
