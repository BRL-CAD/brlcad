
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, May 2003
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   dictionary.h
/// \brief  #goblinDictionary class interface

#ifndef _DICTIONARY_H_
#define _DICTIONARY_H_

#include "managedObject.h"


/// \brief  Map labels ot indices by using hash table lookup

template <class TKey>
class goblinDictionary : public managedObject
{
private:

    TIndex*         first;
    TIndex*         next;
    char**          token;
    TIndex*         index;
    TKey*           key;

    TIndex          nHash;
    TIndex          nMax;
    TKey            defaultKey;
    TIndex          free;
    TIndex          nz;

public:

    goblinDictionary(TIndex,TKey,goblinController&) throw();
    ~goblinDictionary() throw();

    void            Init() throw();
    unsigned long   Size() const throw();
    unsigned long   Allocated() const throw();

    char*           Display() const throw();

    TIndex          NMax() throw() {return nMax;};
    TIndex          NZ() throw() {return nz;};

private:

    unsigned long   HashVal(char*,TIndex = NoIndex) throw();

public:

    TKey            Key(char*,TIndex = NoIndex) throw();
    void            ChangeKey(char*,TKey,TIndex = NoIndex,
                        TOwnership = OWNED_BY_RECEIVER) throw(ERRejected);

};


#endif
