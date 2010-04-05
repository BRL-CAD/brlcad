
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, May 2000
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   balancedToBalanced.h
/// \brief  #balancedToBalanced class interface

#ifndef _BALANCED_TO_BALANCED_H_
#define _BALANCED_TO_BALANCED_H_

#include "abstractBalanced.h"


/// \brief  Balanced digraphs obtained as a simplification of balanced network flow problems
///
/// Any object in this class results from another balanced digraph and an according
/// fixed balanced pseudo-flow. By that transformation, lower capacity bounds
/// and node imbalances are eliminated. Circulation and b-flow problems map
/// to single source / target flow problems. This is analogous to the #digraphToDigraph
/// class for ordinary network flow probleme.

class balancedToBalanced : public abstractBalancedFNW
{
protected:

    abstractBalancedFNW & G; ///< Original network

    TNode           n0;      ///< Number of nodes in the original network
    TArc            m0;      ///< Number of arcs in the original network
    TNode           k2;      ///< Number of odd cycles
    TNode           k1;      ///< k1 == k2/2

    TNode           s1;      ///< Source node, s1 == DefaultSourceNode()
    TNode           t1;      ///< Target node, t1 == DefaultTargetNode()
    TNode           s2;      ///< Additional source node
    TNode           t2;      ///< Additional target node
    TArc            ret1;    ///< Return arc with end nodes t1 and s1
    TArc            ret2;    ///< Return arc with end nodes t2 and s2
    TArc            art1;    ///< Artifical arc with end nodes s1 and t2
    TArc            art2;    ///< Artifical arc with end nodes s2 and t1

    TFloat*         flow;    ///< Flow values on the artifical arcs
    TArc*           artifical;
        // artifical[v] connects the original node v to s2 or t2.
        // Only partially defined, 2*k2 arcs in total.
    TNode *         repr;    ///< Canonical elements of the odd cycles

    bool            symm;    ///< Is flow balanced?

public:

    balancedToBalanced(abstractBalancedFNW &GC) throw();
    ~balancedToBalanced() throw();

    unsigned long   Size() const throw();
    unsigned long   Allocated() const throw();

    bool            Perfect() const throw();

    void            Relax() throw();
    void            Symmetrize() throw();

    TNode           DefaultSourceNode() const throw() {return s1;};
    TNode           DefaultTargetNode() const throw() {return t1;};

    TNode           StartNode(TArc a) const throw(ERRange);
    TNode           EndNode(TArc a) const throw(ERRange);

    TArc            First(TNode v) const throw(ERRange);
    TArc            Right(TArc a,TNode v) const throw(ERRange,ERRejected);

    TCap            Demand(TNode v) const throw(ERRange);
    bool            CDemand() const throw() {return false;};
    TCap            LCap(TArc a) const throw(ERRange);
    bool            CLCap() const throw() {return G.CLCap() && (G.MaxLCap()==0);};
    TCap            UCap(TArc a) const throw(ERRange);
    bool            CUCap() const throw() {return false;};
    TFloat          Length(TArc a) const throw(ERRange);
    bool            CLength() const throw()
                        {return G.CLength() && (G.MaxLength()==0);};
    bool            Blocking(TArc a) const throw() {return a&1;};

    TFloat          C(TNode v,TDim i) const throw(ERRange);
    TFloat          CMax(TDim i) const throw(ERRange);
    TDim            Dim() const throw() {return G.Dim();};
    bool            HiddenNode(TNode v) const throw(ERRange);

    TFloat          Flow(TArc a) const throw(ERRange);
    TFloat          BalFlow(TArc a) const throw(ERRange,ERRejected);
    void            Push(TArc a,TFloat lambda) throw(ERRange,ERRejected);
    void            BalPush(TArc a,TFloat lambda) throw(ERRange,ERRejected);

    TFloat          Sub(TArc a) const throw(ERRange) {return BalFlow(a);};

};


#endif
