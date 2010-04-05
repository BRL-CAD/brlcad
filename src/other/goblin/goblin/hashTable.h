
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, July 2000
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   hashTable.h
/// \brief  #goblinHashTable class interface

#ifndef _HASH_TABLE_H_
#define _HASH_TABLE_H_

#ifndef _MANAGED_OBJECT_H_
#include "managedObject.h"
#endif


/// \brief A data structure to store small subsets in a large index range

template <class TItem,class TKey>
class goblinHashTable : public managedObject
{
private:

    TItem*          first;
    TItem*          next;
    TItem*          index;
    TKey*           key;

    TItem           range;
    TItem           nHash;
    TItem           nMax;
    TItem           UNDEFINED;
    TKey            defaultKey;
    TItem           free;
    TItem           nz;

public:

    goblinHashTable(TItem rr,TItem nn,TKey alpha,
        goblinController& thisContext) throw();
    ~goblinHashTable() throw();

    void            Init() throw();
    unsigned long   Size() const throw();
    unsigned long   Allocated() const throw();

    char*           Display() const throw();

    TItem           NMax() throw() {return nMax;};
    TItem           NZ() throw() {return nz;};

    TKey            Key(TItem w) throw(ERRange);
    void            ChangeKey(TItem w,TKey alpha) throw(ERRange,ERRejected);

};


#endif
