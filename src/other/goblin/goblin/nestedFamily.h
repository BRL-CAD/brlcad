
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, January 2000
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   nestedFamily.h
/// \brief  #nestedFamily class interface

#ifndef _NESTED_FAMILY_H_
#define _NESTED_FAMILY_H_

#include "managedObject.h"


/// \addtogroup setFamilies
/// @{

/// \brief  Implements the process of merging disjoint sets and splitting sets into the former components

template <class TItem>
class nestedFamily : public virtual managedObject
{
private:

    /// \brief  Number of real elements which are in the index range [0,1,..,n-1]
    TItem           n;

    /// \brief  Maximum number of sets, i.e. virtual elements
    TItem           m;

    /// \brief  Allow for path compression
    bool            compress;

    /// \brief  
    TItem           UNDEFINED;

    /// \brief  Pointer towards the canonical element of a toplevel set
    TItem*          B;

    /// \brief  If path compression is disabled, the maximum depth of the
    /// \brief  subtree defined by the B[] labels. Otherwise, the rank
    TItem*          depth;

    /// \brief  For canonical elements of toplevel sets, the actual set index
    TItem*          set;

    /// \brief  For fixed sets, the canonical element
    TItem*          canonical;

    /// \brief  First element of a set
    TItem*          first;

    /// \brief  Defines a linked list on the elements of a given set v, starting at first[v].
    TItem*          next;

public:

    nestedFamily(TItem nn,TItem mm,
        goblinController &thisContext = goblinDefaultContext) throw();
    ~nestedFamily() throw();

    void            Init() throw();
    unsigned long   Size() const throw();
    unsigned long   Allocated() const throw();

    /// \brief  Display the set family nested into a given element
    void Display(TItem v) const throw(ERRange);

    /// \brief  Display the complete nested set family
    char* Display() const throw();

    /// \brief  Prepares a real element for merge operations
    void Bud(TItem v) throw(ERRange,ERRejected);

    /// \brief  Prepares a new toplevel set, returning its index
    TItem MakeSet() throw(ERRejected);

    /// \brief  Merges a toplevel element into a set which is not fixed yet
    void Merge(TItem s,TItem v) throw(ERRange,ERRejected);

    /// \brief  Finalize the merging elements into a toplevel set and assign a canonical element
    void FixSet(TItem s) throw(ERRange,ERRejected);

    /// \brief  Decides wether v is a toplevel element
    bool Top(TItem v) const throw(ERRange);

    /// \brief  Returns the toplevel set containing a given item
    TItem Set(TItem v) const throw(ERRange);


    /// \brief  Public interface of the array first[]
    TItem First(TItem v) const throw(ERRange);

    /// \brief  Public interface of the array next[]
    TItem Next(TItem v) const throw(ERRange,ERRejected);

    /// \brief  Destroy a toplevel set and, if path compression is enabled, re-arrange B-labels
    void Split(TItem v) throw(ERRange,ERRejected);

    /// \brief  Like #Split(), but reversible.
    void Block(TItem v) const throw(ERRange,ERRejected);

    /// \brief  Inverse operation of #Block()
    void UnBlock(TItem v) const throw(ERRange,ERRejected);

private:

    /// \brief  Reset the B-labels after splitting a set
    void Adjust(TItem s,TItem b) const throw(ERRange);

    /// \brief  Compute the canonical element of a set and ajust B[v] if path compression is enabled
    TItem Find(TItem v) const throw(ERRange);

};

/// @}

#endif
