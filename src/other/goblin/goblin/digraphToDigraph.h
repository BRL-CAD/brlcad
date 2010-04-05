
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, December 1998
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   digraphToDigraph.h
/// \brief  #digraphToDigraph class interface

#ifndef _DIGRAPH_TO_DIGRAPH_H_
#define _DIGRAPH_TO_DIGRAPH_H_

#include "abstractDigraph.h"


/// \brief  Digraphs obtained as a simplification of network flow problems
///
/// Any objects in this class results from another digraph and an according
/// fixed pseudo-flow. By that transformation, all lower capacity bounds
/// and node imbalances are eliminated. Circulation and b-flow problems map
/// to a single source / target flow problem.

class digraphToDigraph : public abstractDiGraph
{
protected:

    abstractDiGraph &G;   ///< The original network

    TNode           n0;         ///< Number of nodes in the original network
    TArc            m0;         ///< Number of arcs in the original network

    TNode           s1;         ///< Source node, s1 == DefaultSourceNode()
    TNode           t1;         ///< Target node, t1 == DefaultTargetNode()
    TArc            ret1;       ///< Return arc with end nodes t1 and s1

    TFloat*         flow;       ///< Array of flow values on the artifical arcs
    TCap*           cap;        ///< Array of capacities of the artifical arcs
    TCap            sumLower;   ///< Sum of original lower capacity bounds

    TFloat*         piG;        ///< Pointer to the original node potentials

public:

    digraphToDigraph(abstractDiGraph &PG,TFloat *pp= NULL) throw(ERRejected);
    ~digraphToDigraph() throw();

    unsigned long   Size() const throw();
    unsigned long   Allocated() const throw();

    bool            Perfect() const throw();

    TNode           DefaultSourceNode() const throw() {return s1;};
    TNode           DefaultTargetNode() const throw() {return t1;};

    TNode           StartNode(TArc a) const throw(ERRange);
    TNode           EndNode(TArc a) const throw(ERRange);

    TArc            First(TNode v) const throw(ERRange);
    TArc            Right(TArc a,TNode v) const throw(ERRange,ERRejected);

    TCap            Demand(TNode v) const throw(ERRange);
    bool            CDemand() const throw() {return false;};
    TCap            UCap(TArc a) const throw(ERRange);
    bool            CUCap() const throw() {return false;};
    TCap            LCap(TArc a) const throw(ERRange);
    TCap            MaxLCap() const throw() {return G.MaxLCap();};
    bool            CLCap() const throw() {return G.CLCap() & (G.MaxLCap()==0);};
    TFloat          Length(TArc a) const throw(ERRange);
    TFloat          MaxLength() const throw() {return G.MaxLength();}; 
    bool            CLength() const throw() {return G.CLength() & (G.MaxLength()==0);};
    bool            Blocking(TArc a) const throw() {return a&1;};

    TFloat          C(TNode v,TDim i) const throw(ERRange);
    TFloat          CMax(TDim i) const throw(ERRange);
    TDim            Dim() const throw() {return G.Dim();};
    bool            HiddenNode(TNode v) const throw(ERRange);

    TFloat          Flow(TArc a) const throw(ERRange);
    void            Push(TArc a,TFloat lambda) throw(ERRange,ERRejected);

    TFloat          Sub(TArc a) const throw(ERRange) {return Flow(a);};

};


#endif
