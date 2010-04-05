
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, January 2000
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   balancedDigraph.h
/// \brief  #balancedFNW and #splitGraph class interfaces

#ifndef _BALANCED_DIGRAPH_H_
#define _BALANCED_DIGRAPH_H_

#include "abstractBalanced.h"
#include "sparseRepresentation.h"


/// \brief  Balanced (skew-symmetric) directed graphs represented with incidence lists

class balancedFNW : public abstractBalancedFNW
{
    friend class sparseRepresentation;

protected:

    sparseRepresentation X;

public:

    /// \brief  Default constructor for balanced digraphs
    ///
    /// \param _n       The initial number of nodes
    /// \param _CT      The context to which this graph object is attached
    balancedFNW(TNode _n,goblinController& _CT=goblinDefaultContext) throw();

    /// \brief  File constructor for balanced digraphs
    ///
    /// \param fileName  The source file name
    /// \param _CT       The context to which this graph object is attached
    balancedFNW(const char* fileName,goblinController& _CT=goblinDefaultContext)
        throw(ERFile,ERParse);

    ~balancedFNW() throw();

    unsigned long   Size() const throw();
    unsigned long   Allocated() const throw();

    void            Symmetrize() throw();
    void            Relax() throw();

    #include <sparseInclude.h>

    TFloat          Length(TArc a) const throw(ERRange);
    TFloat          BalFlow(TArc a) const throw(ERRange,ERRejected)
                        {return X.Sub(a);};
    void            BalPush(TArc a,TFloat lambda) throw(ERRange)
                        {Push(a,lambda); Push(a^2,lambda);};

};


/// \brief  Balanced digraphs obtained by symmetrization of ordinary flow networks

class splitGraph : public balancedFNW
{
public:

    splitGraph(const abstractDiGraph &G) throw();

    TNode DefaultSourceNode() const throw() {return n-1;};
    TNode DefaultTargetNode() const throw() {return n-2;};

};

#endif
