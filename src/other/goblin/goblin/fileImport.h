
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, December 1998
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   fileImport.h
/// \brief  #goblinImport class interface

#ifndef _FILE_IMPORT_H_
#define _FILE_IMPORT_H_

#include "goblinController.h"


/// \addtogroup groupObjectImport
/// @{

/// \brief  Class for reading data objects from file

class goblinImport : public goblinRootObject
{
private:

    char            buffer[256];
    char            currentLevel;
    bool            head;
    bool            tail;
    bool            complain;
    ifstream        impFile;
    unsigned long   length;
    goblinController& CT;

    void*           tuple;

    TNode           n;
    TArc            m;
    TNode           ni;

public:

    goblinImport(const char*,goblinController& = goblinDefaultContext) throw(ERFile);
    ~goblinImport() throw(ERParse);

    char*           Display() const throw() {return NULL;};
    const char*     Label() const throw() {return NULL;};
    unsigned long   Size() const throw() {return 0;};
    ifstream&       Stream() throw() {return impFile;};

    char*           Scan(char* = NULL,TOwnership = OWNED_BY_RECEIVER) throw(ERParse);
    bool            Seek(char* token) throw(ERParse);
    bool            Head() throw();
    bool            Tail() throw();
    bool            Eof() throw();

    TNode*          GetTNodeTuple(unsigned long) throw(ERParse);
    TArc*           GetTArcTuple(unsigned long) throw(ERParse);
    TCap*           GetTCapTuple(unsigned long) throw(ERParse);
    TFloat*         GetTFloatTuple(unsigned long) throw(ERParse);
    TIndex*         GetTIndexTuple(unsigned long) throw(ERParse);
    char*           GetCharTuple(unsigned long) throw(ERParse);

    void            ReadConfiguration() throw(ERParse);

    bool            Constant() throw();
    unsigned long   Length() throw();
    void            DontComplain() throw();

    void            SetDimensions(TNode nn,TArc mm,TNode nni) throw()
                        {n = nn; m = mm; ni = nni;};
    size_t          AllocateTuple(TBaseType,TArrayDim)
                        throw(ERRejected);
    void            ReadTupleValues(TBaseType,size_t,void* = NULL)
                        throw(ERParse);

    template <typename TEntry> TEntry* GetTuple() throw();

    template <typename TToken> TToken ReadTuple(
        const TPoolTable listOfParameters[],TToken endToken,TToken undefToken) throw();
};


template <typename TEntry> inline TEntry* goblinImport::GetTuple() throw()
{
    void* localTuple = tuple;
    tuple = NULL;
    return static_cast<TEntry*>(localTuple);
}


template <typename TToken> inline TToken goblinImport::ReadTuple(
    const TPoolTable listOfParameters[],
    TToken endToken,TToken undefToken) throw()
{
    char* label = Scan();

    if (strcmp(label,"")==0)
    {
        return endToken;
    }


    TToken thisToken = TToken(0);

    while (strcmp(label,listOfParameters[thisToken].tokenLabel)!=0)
    {
        thisToken = TToken(int(thisToken)+1);

        if (thisToken==endToken)
        {
            while (!tail) label = Scan();

            return undefToken;
        }
    }


    size_t reqLength = AllocateTuple(listOfParameters[thisToken].arrayType,
        listOfParameters[thisToken].arrayDim);

    ReadTupleValues(listOfParameters[thisToken].arrayType,reqLength);

    return thisToken;
}


/// @}

#endif
