
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, December 1998
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   bigraphToDigraph.h
/// \brief  #bigraphToDigraph class interface

#ifndef _BIGRAPH_TO_DIGRAPH_H_
#define _BIGRAPH_TO_DIGRAPH_H_

#include "abstractDigraph.h"
#include "abstractBigraph.h"


/// \brief Network flow reduction of bipartite matching problems

class bigraphToDigraph : public abstractDiGraph
{

protected:

    abstractBiGraph &G;       ///< The original graph

    TNode           n0;       ///< Number of nodes in the original network
    TNode           n1;       ///< Number of outer nodes in the original network
    TNode           n2;       ///< Number of inner nodes in the original network
    TArc            m0;       ///< Number of arcs in the original network

    TNode           s1;       ///< Source node, s1 == DefaultSourceNode()
    TNode           t1;       ///< Target node, t1 == DefaultTargetNode()
    TNode           s2;       ///< Additional source node
    TNode           t2;       ///< Additional target node
    TArc            ret1;     ///< Return arc with end nodes t1 and s1
    TArc            ret2;     ///< Return arc with end nodes t2 and s2
    TArc            art1;     ///< Artifical arc with end nodes s1 and t2
    TArc            art2;     ///< Artifical arc with end nodes s2 and t1

    TFloat*         dgl;      ///< Array of node degrees in the original graph
    TCap            ccap;     ///< Node capacity constant
    TCap*           cap;      ///< Array of (upper-lower) node capacities
    TCap*           lower;    ///< Array of lower node capacities

    TCap            sumLower1; ///< sum of lower degree bounds on the outer nodes
    TCap            sumLower2; ///< sum of lower degree bounds on the inner nodes
    TCap            sumUpper;  ///< sum of upper degree bounds

    TFloat          minLength; ///< Minimum edge length of the original graph

public:

    bigraphToDigraph(abstractBiGraph &GC,TCap *pLower,TCap *pCap = NULL) throw();
    bigraphToDigraph(abstractBiGraph &GC,TCap cCap) throw();
    bigraphToDigraph(abstractBiGraph &GC) throw();
    ~bigraphToDigraph() throw();

    void            Init() throw();
    void            SetDegrees() throw();
    unsigned long   Size() const throw();
    unsigned long   Allocated() const throw();

    bool            Perfect() const throw();

    TNode           DefaultSourceNode() const throw() {return s1;};
    TNode           DefaultTargetNode() const throw() {return t1;};

    TNode           StartNode(TArc a) const throw(ERRange);
    TNode           EndNode(TArc a) const throw(ERRange);

    TArc            First(TNode v) const throw(ERRange);
    TArc            Right(TArc a,TNode v) const throw(ERRange);

    TCap            Demand(TNode v) const throw(ERRange);
    bool            CDemand() const throw() {return false;};
    TCap            LCap(TArc a) const throw() {return 0;};
    bool            CLCap() const throw() {return true;};
    TCap            UCap(TArc a) const throw(ERRange);
    bool            CUCap() const throw() {return false;};
    TFloat          Length(TArc a) const throw(ERRange);
    bool            CLength() const throw()
                        {return G.CLength() & (G.MaxLength()==0);};
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
