
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, July 2005
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file indexSet.h
/// \brief Interface to index sets and some inline class implementations


#ifndef _INDEX_SET_H_
#define _INDEX_SET_H_

#include "managedObject.h"


/// \addtogroup groupIndexSets
/// @{

/// \brief Interface class for index set objects

template <class TItem>
class indexSet : public virtual managedObject
{
public:

    TItem maxIndex;

public:

    indexSet(TItem _maxIndex,goblinController &_context = goblinDefaultContext) throw();
    virtual ~indexSet() throw();

    virtual unsigned long Size() const throw() = 0;

    char* Display() const throw();

    /// \brief  Test for indet set membership
    ///
    /// \param i      An index to test for indet set membership
    /// \retval true  The indexed element is contained by this set
    virtual bool IsMember(TItem i) const throw(ERRange) = 0;

    /// \brief  Query the smallest index in an index set
    ///
    /// \return  The smallest index contained by this set
    virtual TItem First() const throw();

    /// \brief  Enumeration of the indices contained in a set
    ///
    /// \return  The least upper bound index  contained by this set
    virtual TItem Successor(const TItem i) const throw(ERRange);

};


/// \brief Index set representing a single index in an unlimited range

template <class TItem>
class singletonIndex : public indexSet<TItem>
{
private:

    TItem containedIndex;

public:

    singletonIndex(TItem _i,TItem _n,goblinController &_context = goblinDefaultContext) throw();
    ~singletonIndex() throw();

    unsigned long  Size() const throw();

    bool   IsMember(TItem i) const throw(ERRange);
    TItem  First() const throw();
    TItem  Successor(const TItem i) const throw(ERRange);

};


/// \brief Index set representing all indices in an unlimited range

template <class TItem>
class fullIndex : public indexSet<TItem>
{
public:

    fullIndex(TItem _n,goblinController &_context = goblinDefaultContext) throw();
    ~fullIndex() throw();

    unsigned long  Size() const throw();

    bool   IsMember(TItem i) const throw();
    TItem  First() const throw();
    TItem  Successor(const TItem i) const throw(ERRange);

};


/// \brief Index set representing no indices in an unlimited range

template <class TItem>
class voidIndex : public indexSet<TItem>
{
public:

    voidIndex(TItem _n,goblinController &_context = goblinDefaultContext) throw();
    ~voidIndex() throw();

    unsigned long  Size() const throw();

    bool   IsMember(TItem i) const throw();
    TItem  First() const throw();
    TItem  Successor(const TItem i) const throw(ERRange);

};


/// \brief Index set representing the set union of two index sets

template <class TItem>
class indexSetUnion : public indexSet<TItem>
{
private:

    indexSet<TItem>& set1;
    indexSet<TItem>& set2;

public:

    indexSetUnion(indexSet<TItem> &_set1,indexSet<TItem> &_set2)  throw();
    ~indexSetUnion() throw();

    unsigned long Size() const throw();

    bool IsMember(TItem i) const throw(ERRange);

};


/// \brief Index set representing the cut of two index sets

template <class TItem>
class indexSetCut : public indexSet<TItem>
{
private:

    indexSet<TItem>& set1;
    indexSet<TItem>& set2;

public:

    indexSetCut(indexSet<TItem> &_set1,indexSet<TItem> &_set2)  throw();
    ~indexSetCut() throw();

    unsigned long Size() const throw();

    bool IsMember(TItem i) const throw(ERRange);

};


/// \brief Index set representing the difference of two index sets

template <class TItem>
class indexSetMinus : public indexSet<TItem>
{
private:

    indexSet<TItem>& set1;
    indexSet<TItem>& set2;

public:

    indexSetMinus(indexSet<TItem> &_set1,indexSet<TItem> &_set2)  throw();
    ~indexSetMinus() throw();

    unsigned long Size() const throw();

    bool IsMember(TItem i) const throw(ERRange);

};


/// \brief Index set representing the symmetric difference of two index sets

template <class TItem>
class indexSetDifference : public indexSet<TItem>
{
private:

    indexSet<TItem>& set1;
    indexSet<TItem>& set2;

public:

    indexSetDifference(indexSet<TItem> &_set1,indexSet<TItem> &_set2)  throw();
    ~indexSetDifference() throw();

    unsigned long Size() const throw();

    bool IsMember(TItem i) const throw(ERRange);

};


/// \brief Index set representing the complement of another index sets

template <class TItem>
class indexSetComplement : public indexSet<TItem>
{
private:

    indexSet<TItem>& set1;

public:

    indexSetComplement(indexSet<TItem> &_set1)  throw();
    ~indexSetComplement() throw();

    unsigned long Size() const throw();

    bool IsMember(TItem i) const throw(ERRange);

};

/// @}


#endif
