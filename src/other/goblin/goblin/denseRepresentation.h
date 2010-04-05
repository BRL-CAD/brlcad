
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, September 1999
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file denseRepresentation.h
/// \brief #denseRepresentation class interface

#ifndef _DENSE_REPRESENTATION_H_
#define _DENSE_REPRESENTATION_H_

#include "graphRepresentation.h"
#include "hashTable.h"


/// \brief  The representational objects from which dense graph objects are composed

class denseRepresentation : public graphRepresentation
{
protected:

    goblinHashTable<TArc,TFloat> *sub;

public:

    denseRepresentation(const abstractMixedGraph& _G,TOption options=0) throw();
    ~denseRepresentation() throw();

    TArc            InsertArc(TArc a,TCap _u,TFloat _c,TCap _l) throw(ERRange,ERRejected);

    unsigned long   Size() const throw();
    unsigned long   Allocated() const throw();


    // *************************************************************** //
    //           Subgraph Management                                   //
    // *************************************************************** //

    void    NewSubgraph(TArc card) throw(ERRejected);
    void    NewSubgraph() throw(ERRejected) {NewSubgraph(nMax*2);};
    void    ReleaseSubgraph() throw();

    TFloat  Sub(TArc a) const throw(ERRange);
    void    SetSub(TArc a,TFloat multiplicity) throw(ERRange,ERRejected);
    void    SetSubRelative(TArc a,TFloat lambda) throw(ERRange,ERRejected);

};


#endif
