
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, October 2008
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file indexSet.cpp
/// \brief Generic indet set implementation


#include "indexSet.h"


template <class TItem>
indexSet<TItem>::indexSet(TItem _maxIndex,goblinController &_context)
    throw() : managedObject(_context), maxIndex(_maxIndex)
{
}


template <class TItem>
indexSet<TItem>::~indexSet() throw()
{
}


template <class TItem>
char* indexSet<TItem>::Display() const throw()
{
    this -> LogEntry(MSG_TRACE,"Index set");

    THandle LH = this->LogStart(MSG_TRACE2,"  {");
    TItem counter = 0;

    for (TItem i=0;i<maxIndex;i++)
    {
        if (!IsMember(i)) continue;

        if (counter==0)
        {
            sprintf(this->CT.logBuffer,"%lu",static_cast<unsigned long>(i));
            this -> LogAppend(LH,this->CT.logBuffer);
        }
        else if (counter%10==0)
        {
            this -> LogEnd(LH,",");
            sprintf(this->CT.logBuffer,"   %lu",static_cast<unsigned long>(i));
            LH = this->LogStart(MSG_TRACE2,this->CT.logBuffer);
        }
        else
        {
            sprintf(this->CT.logBuffer,", %lu",static_cast<unsigned long>(i));
            this -> LogAppend(LH,this->CT.logBuffer);
        }

        counter++;
    }

    this -> LogEnd(LH,"}");

    return NULL;
}


template <class TItem>
TItem indexSet<TItem>::First() const throw()
{
    for (TItem i=0;i<maxIndex;i++)
    {
        if (IsMember(i)) return i;
    }

    return maxIndex;
}


template <class TItem>
TItem indexSet<TItem>::Successor(const TItem i) const throw(ERRange)
{
    for (TItem j=i+1;j<maxIndex;j++)
    {
        if (IsMember(j)) return j;
    }

    return maxIndex;
}


template <class TItem>
singletonIndex<TItem>::singletonIndex(TItem _i,TItem _n,goblinController &_context)
    throw() : managedObject(_context),indexSet<TItem>(_n,_context), containedIndex(_i)
{
}


template <class TItem>
singletonIndex<TItem>::~singletonIndex() throw()
{
}


template <class TItem>
unsigned long singletonIndex<TItem>::Size() const throw()
{
    return sizeof(singletonIndex<TItem>) + managedObject::Allocated();
}

template <class TItem>
bool singletonIndex<TItem>::IsMember(TItem i) const throw(ERRange)
{
    return (i==containedIndex);
}


template <class TItem>
TItem singletonIndex<TItem>::First() const throw()
{
    return this->containedIndex;
}


template <class TItem>
TItem singletonIndex<TItem>::Successor(const TItem i) const throw(ERRange)
{
    return this->maxIndex;
}


template class singletonIndex<unsigned short>;
template class singletonIndex<unsigned long>;


template <class TItem>
fullIndex<TItem>::fullIndex(TItem _n,goblinController &_context)
    throw() : managedObject(_context),indexSet<TItem>(_n,_context)
{
}


template <class TItem>
fullIndex<TItem>::~fullIndex() throw()
{
}


template <class TItem>
unsigned long fullIndex<TItem>::Size() const throw()
{
    return sizeof(fullIndex<TItem>) + managedObject::Allocated();
}


template <class TItem>
bool fullIndex<TItem>::IsMember(TItem i) const throw()
{
    return true;
}


template <class TItem>
TItem fullIndex<TItem>::First() const throw()
{
    return 0;
}


template <class TItem>
TItem fullIndex<TItem>::Successor(const TItem i) const throw(ERRange)
{
    return i+1;
}


template class fullIndex<unsigned short>;
template class fullIndex<unsigned long>;


template <class TItem>
voidIndex<TItem>::voidIndex(TItem _n,goblinController &_context) throw() :
    managedObject(_context),indexSet<TItem>(_n,_context)
{
}


template <class TItem>
voidIndex<TItem>::~voidIndex() throw()
{
}


template <class TItem>
unsigned long voidIndex<TItem>::Size() const throw()
{
    return sizeof(voidIndex<TItem>) + managedObject::Allocated();
}


template <class TItem>
bool voidIndex<TItem>::IsMember(TItem i) const throw()
{
    return false;
}


template <class TItem>
TItem voidIndex<TItem>::First() const throw()
{
    return this->maxIndex;
}


template <class TItem>
TItem voidIndex<TItem>::Successor(const TItem i) const throw(ERRange)
{
    return this->maxIndex;
}


template class voidIndex<unsigned short>;
template class voidIndex<unsigned long>;


template <class TItem>
indexSetUnion<TItem>::indexSetUnion(indexSet<TItem> &_set1,indexSet<TItem> &_set2) throw() :
    managedObject(_set1.Context()),
    indexSet<TItem>(
        (_set1.maxIndex > _set2.maxIndex) ? _set1.maxIndex : _set2.maxIndex,_set1.Context()),
    set1(_set1), set2(_set2)
{
}


template <class TItem>
indexSetUnion<TItem>::~indexSetUnion() throw()
{
}


template <class TItem>
unsigned long indexSetUnion<TItem>::Size() const throw()
{
    return sizeof(indexSetUnion<TItem>) + managedObject::Allocated();
}


template <class TItem>
bool indexSetUnion<TItem>::IsMember(TItem i) const throw(ERRange)
{
    return (set1.IsMember(i) || set2.IsMember(i));
}


template class indexSetUnion<unsigned short>;
template class indexSetUnion<unsigned long>;


template <class TItem>
indexSetCut<TItem>::indexSetCut(indexSet<TItem> &_set1,indexSet<TItem> &_set2)  throw() :
    managedObject(_set1.Context()),
    indexSet<TItem>(
        (_set1.maxIndex > _set2.maxIndex) ? _set1.maxIndex : _set2.maxIndex,_set1.Context()),
    set1(_set1), set2(_set2)
{
}


template <class TItem>
indexSetCut<TItem>::~indexSetCut() throw()
{
}


template <class TItem>
unsigned long indexSetCut<TItem>::Size() const throw()
{
    return sizeof(indexSetCut<TItem>) + managedObject::Allocated();
}


template <class TItem>
bool indexSetCut<TItem>::IsMember(TItem i) const throw(ERRange)
{
    return (set1.IsMember(i) && set2.IsMember(i));
}


template class indexSetCut<unsigned short>;
template class indexSetCut<unsigned long>;


template <class TItem>
indexSetMinus<TItem>::indexSetMinus(indexSet<TItem> &_set1,indexSet<TItem> &_set2)  throw() :
    managedObject(_set1.Context()),
    indexSet<TItem>(
        (_set1.maxIndex > _set2.maxIndex) ? _set1.maxIndex : _set2.maxIndex,_set1.Context()),
    set1(_set1), set2(_set2)
{
}


template <class TItem>
indexSetMinus<TItem>::~indexSetMinus() throw()
{
}


template <class TItem>
unsigned long indexSetMinus<TItem>::Size() const throw()
{
    return sizeof(indexSetMinus<TItem>) + managedObject::Allocated();
}


template <class TItem>
bool indexSetMinus<TItem>::IsMember(TItem i) const throw(ERRange)
{
    return (set1.IsMember(i) && !set2.IsMember(i));
}


template class indexSetMinus<unsigned short>;
template class indexSetMinus<unsigned long>;


template <class TItem>
indexSetDifference<TItem>::indexSetDifference(indexSet<TItem> &_set1,indexSet<TItem> &_set2)  throw() :
    managedObject(_set1.Context()),
    indexSet<TItem>(
        (_set1.maxIndex > _set2.maxIndex) ? _set1.maxIndex : _set2.maxIndex,_set1.Context()),
    set1(_set1), set2(_set2)
{
}


template <class TItem>
indexSetDifference<TItem>::~indexSetDifference() throw()
{
}


template <class TItem>
unsigned long indexSetDifference<TItem>::Size() const throw()
{
    return sizeof(indexSetDifference<TItem>) + managedObject::Allocated();
}


template <class TItem>
bool indexSetDifference<TItem>::IsMember(TItem i) const throw(ERRange)
{
    return (set1.IsMember(i) ^ set2.IsMember(i));
}


template class indexSetDifference<unsigned short>;
template class indexSetDifference<unsigned long>;


template <class TItem>
indexSetComplement<TItem>::indexSetComplement(indexSet<TItem> &_set1)  throw() :
    managedObject(_set1.Context()),
    indexSet<TItem>(_set1.maxIndex,_set1.Context()),
    set1(_set1)
{
}


template <class TItem>
indexSetComplement<TItem>::~indexSetComplement() throw()
{
}


template <class TItem>
unsigned long indexSetComplement<TItem>::Size() const throw()
{
    return sizeof(indexSetComplement<TItem>) + managedObject::Allocated();
}


template <class TItem>
bool indexSetComplement<TItem>::IsMember(TItem i) const throw(ERRange)
{
    return !set1.IsMember(i);
}


template class indexSetComplement<unsigned short>;
template class indexSetComplement<unsigned long>;

//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, October 2008
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file indexSet.cpp
/// \brief Generic indet set implementation


#include "indexSet.h"


template <class TItem>
indexSet<TItem>::indexSet(TItem _maxIndex,goblinController &_context)
    throw() : managedObject(_context), maxIndex(_maxIndex)
{
}


template <class TItem>
indexSet<TItem>::~indexSet() throw()
{
}


template <class TItem>
char* indexSet<TItem>::Display() const throw()
{
    this -> LogEntry(MSG_TRACE,"Index set");

    THandle LH = this->LogStart(MSG_TRACE2,"  {");
    TItem counter = 0;

    for (TItem i=0;i<maxIndex;i++)
    {
        if (!IsMember(i)) continue;

        if (counter==0)
        {
            sprintf(this->CT.logBuffer,"%lu",static_cast<unsigned long>(i));
            this -> LogAppend(LH,this->CT.logBuffer);
        }
        else if (counter%10==0)
        {
            this -> LogEnd(LH,",");
            sprintf(this->CT.logBuffer,"   %lu",static_cast<unsigned long>(i));
            LH = this->LogStart(MSG_TRACE2,this->CT.logBuffer);
        }
        else
        {
            sprintf(this->CT.logBuffer,", %lu",static_cast<unsigned long>(i));
            this -> LogAppend(LH,this->CT.logBuffer);
        }

        counter++;
    }

    this -> LogEnd(LH,"}");

    return NULL;
}


template <class TItem>
TItem indexSet<TItem>::First() const throw()
{
    for (TItem i=0;i<maxIndex;i++)
    {
        if (IsMember(i)) return i;
    }

    return maxIndex;
}


template <class TItem>
TItem indexSet<TItem>::Successor(const TItem i) const throw(ERRange)
{
    for (TItem j=i+1;j<maxIndex;j++)
    {
        if (IsMember(j)) return j;
    }

    return maxIndex;
}


template <class TItem>
singletonIndex<TItem>::singletonIndex(TItem _i,TItem _n,goblinController &_context)
    throw() : managedObject(_context),indexSet<TItem>(_n,_context), containedIndex(_i)
{
}


template <class TItem>
singletonIndex<TItem>::~singletonIndex() throw()
{
}


template <class TItem>
unsigned long singletonIndex<TItem>::Size() const throw()
{
    return sizeof(singletonIndex<TItem>) + managedObject::Allocated();
}

template <class TItem>
bool singletonIndex<TItem>::IsMember(TItem i) const throw(ERRange)
{
    return (i==containedIndex);
}


template <class TItem>
TItem singletonIndex<TItem>::First() const throw()
{
    return this->containedIndex;
}


template <class TItem>
TItem singletonIndex<TItem>::Successor(const TItem i) const throw(ERRange)
{
    return this->maxIndex;
}


template class singletonIndex<unsigned short>;
template class singletonIndex<unsigned long>;


template <class TItem>
fullIndex<TItem>::fullIndex(TItem _n,goblinController &_context)
    throw() : managedObject(_context),indexSet<TItem>(_n,_context)
{
}


template <class TItem>
fullIndex<TItem>::~fullIndex() throw()
{
}


template <class TItem>
unsigned long fullIndex<TItem>::Size() const throw()
{
    return sizeof(fullIndex<TItem>) + managedObject::Allocated();
}


template <class TItem>
bool fullIndex<TItem>::IsMember(TItem i) const throw()
{
    return true;
}


template <class TItem>
TItem fullIndex<TItem>::First() const throw()
{
    return 0;
}


template <class TItem>
TItem fullIndex<TItem>::Successor(const TItem i) const throw(ERRange)
{
    return i+1;
}


template class fullIndex<unsigned short>;
template class fullIndex<unsigned long>;


template <class TItem>
voidIndex<TItem>::voidIndex(TItem _n,goblinController &_context) throw() :
    managedObject(_context),indexSet<TItem>(_n,_context)
{
}


template <class TItem>
voidIndex<TItem>::~voidIndex() throw()
{
}


template <class TItem>
unsigned long voidIndex<TItem>::Size() const throw()
{
    return sizeof(voidIndex<TItem>) + managedObject::Allocated();
}


template <class TItem>
bool voidIndex<TItem>::IsMember(TItem i) const throw()
{
    return false;
}


template <class TItem>
TItem voidIndex<TItem>::First() const throw()
{
    return this->maxIndex;
}


template <class TItem>
TItem voidIndex<TItem>::Successor(const TItem i) const throw(ERRange)
{
    return this->maxIndex;
}


template class voidIndex<unsigned short>;
template class voidIndex<unsigned long>;


template <class TItem>
indexSetUnion<TItem>::indexSetUnion(indexSet<TItem> &_set1,indexSet<TItem> &_set2) throw() :
    managedObject(_set1.Context()),
    indexSet<TItem>(
        (_set1.maxIndex > _set2.maxIndex) ? _set1.maxIndex : _set2.maxIndex,_set1.Context()),
    set1(_set1), set2(_set2)
{
}


template <class TItem>
indexSetUnion<TItem>::~indexSetUnion() throw()
{
}


template <class TItem>
unsigned long indexSetUnion<TItem>::Size() const throw()
{
    return sizeof(indexSetUnion<TItem>) + managedObject::Allocated();
}


template <class TItem>
bool indexSetUnion<TItem>::IsMember(TItem i) const throw(ERRange)
{
    return (set1.IsMember(i) || set2.IsMember(i));
}


template class indexSetUnion<unsigned short>;
template class indexSetUnion<unsigned long>;


template <class TItem>
indexSetCut<TItem>::indexSetCut(indexSet<TItem> &_set1,indexSet<TItem> &_set2)  throw() :
    managedObject(_set1.Context()),
    indexSet<TItem>(
        (_set1.maxIndex > _set2.maxIndex) ? _set1.maxIndex : _set2.maxIndex,_set1.Context()),
    set1(_set1), set2(_set2)
{
}


template <class TItem>
indexSetCut<TItem>::~indexSetCut() throw()
{
}


template <class TItem>
unsigned long indexSetCut<TItem>::Size() const throw()
{
    return sizeof(indexSetCut<TItem>) + managedObject::Allocated();
}


template <class TItem>
bool indexSetCut<TItem>::IsMember(TItem i) const throw(ERRange)
{
    return (set1.IsMember(i) && set2.IsMember(i));
}


template class indexSetCut<unsigned short>;
template class indexSetCut<unsigned long>;


template <class TItem>
indexSetMinus<TItem>::indexSetMinus(indexSet<TItem> &_set1,indexSet<TItem> &_set2)  throw() :
    managedObject(_set1.Context()),
    indexSet<TItem>(
        (_set1.maxIndex > _set2.maxIndex) ? _set1.maxIndex : _set2.maxIndex,_set1.Context()),
    set1(_set1), set2(_set2)
{
}


template <class TItem>
indexSetMinus<TItem>::~indexSetMinus() throw()
{
}


template <class TItem>
unsigned long indexSetMinus<TItem>::Size() const throw()
{
    return sizeof(indexSetMinus<TItem>) + managedObject::Allocated();
}


template <class TItem>
bool indexSetMinus<TItem>::IsMember(TItem i) const throw(ERRange)
{
    return (set1.IsMember(i) && !set2.IsMember(i));
}


template class indexSetMinus<unsigned short>;
template class indexSetMinus<unsigned long>;


template <class TItem>
indexSetDifference<TItem>::indexSetDifference(indexSet<TItem> &_set1,indexSet<TItem> &_set2)  throw() :
    managedObject(_set1.Context()),
    indexSet<TItem>(
        (_set1.maxIndex > _set2.maxIndex) ? _set1.maxIndex : _set2.maxIndex,_set1.Context()),
    set1(_set1), set2(_set2)
{
}


template <class TItem>
indexSetDifference<TItem>::~indexSetDifference() throw()
{
}


template <class TItem>
unsigned long indexSetDifference<TItem>::Size() const throw()
{
    return sizeof(indexSetDifference<TItem>) + managedObject::Allocated();
}


template <class TItem>
bool indexSetDifference<TItem>::IsMember(TItem i) const throw(ERRange)
{
    return (set1.IsMember(i) ^ set2.IsMember(i));
}


template class indexSetDifference<unsigned short>;
template class indexSetDifference<unsigned long>;


template <class TItem>
indexSetComplement<TItem>::indexSetComplement(indexSet<TItem> &_set1)  throw() :
    managedObject(_set1.Context()),
    indexSet<TItem>(_set1.maxIndex,_set1.Context()),
    set1(_set1)
{
}


template <class TItem>
indexSetComplement<TItem>::~indexSetComplement() throw()
{
}


template <class TItem>
unsigned long indexSetComplement<TItem>::Size() const throw()
{
    return sizeof(indexSetComplement<TItem>) + managedObject::Allocated();
}


template <class TItem>
bool indexSetComplement<TItem>::IsMember(TItem i) const throw(ERRange)
{
    return !set1.IsMember(i);
}


template class indexSetComplement<unsigned short>;
template class indexSetComplement<unsigned long>;
