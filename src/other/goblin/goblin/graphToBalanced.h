
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, February 1999
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   graphToBalanced.h
/// \brief  #graphToBalanced class interface

#ifndef _GRAPH_TO_BALANCED_H_
#define _GRAPH_TO_BALANCED_H_

#include "abstractBalanced.h"
#include "abstractGraph.h"


/// \brief Balanced network flow reduction of generalized matching problems

class graphToBalanced : public abstractBalancedFNW
{

protected:

    abstractGraph & G;        ///< The original graph

    TNode           n0;       ///< Number of nodes in the original network
    TArc            m0;       ///< Number of arcs in the original network

    TNode           s1;       ///< Source node, s1 == DefaultSourceNode()
    TNode           t1;       ///< Sink node, t1 == DefaultTargetNode()
    TNode           s2;       ///< Additional source node
    TNode           t2;       ///< Additional target node
    TArc            ret1;     ///< Return arc with end nodes t1 and s1
    TArc            ret2;     ///< Return arc with end nodes t2 and s2
    TArc            art1;     ///< Artifical arc with end nodes s1 and t2
    TArc            art2;     ///< Artifical arc with end nodes s2 and t1

    TFloat*         flow;     ///< Array of flow values
    TFloat*         deg;      ///< Array of node degrees in the original graph
    TCap            ccap;     ///< Node capacity constant
    TCap*           cap;      ///< Array of (upper-lower) node capacities
    TCap*           lower;    ///< Array of lower node capacities

    TCap            sumLower; ///< sum of lower degree bounds
    TCap            sumUpper; ///< sum of upper degree bounds

    TFloat          minLength; ///< Minimum edge length of the original graph

public:

    graphToBalanced(abstractGraph &GC,TCap* pLower,TCap *pCap = NULL) throw();
    graphToBalanced(abstractGraph &GC,TCap cCap) throw();
    graphToBalanced(abstractGraph &GC) throw();
    ~graphToBalanced() throw();

    void            Init() throw();
    unsigned long   Size() const throw();
    unsigned long   Allocated() const throw();

    bool            Perfect() const throw();

    void            Symmetrize() throw();
    void            Relax() throw();

    /// \brief  Export the Gallai-Edmonds Decomposition to the original graph
    void            ExportDecomposition() throw();

    TNode           DefaultSourceNode() const throw() {return s1;};
    TNode           DefaultTargetNode() const throw() {return t1;};

    TNode           StartNode(TArc a) const throw(ERRange);
    TNode           EndNode(TArc a) const throw(ERRange);

    TArc            First(TNode v) const throw(ERRange);
    TArc            Right(TArc a,TNode v) const throw(ERRange,ERRejected);

    TCap            Demand(TNode v) const throw(ERRange);
    bool            CDemand() const throw() {return false;};
    TCap            LCap(TArc a) const throw(ERRange);
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
/*
    TIndex          OriginalOfNode(TNode v) throw(ERRange);
    TIndex          OriginalOfArc(TArc a) throw(ERRange);
*/
    bool            HiddenNode(TNode v) const throw(ERRange);
    bool            HiddenArc(TArc a) const throw(ERRange);

    TFloat          Flow(TArc a) const throw(ERRange);
    TFloat          BalFlow(TArc a) const throw(ERRange,ERRejected);
    void            Push(TArc a,TFloat lambda) throw(ERRange,ERRejected);
    void            BalPush(TArc a,TFloat lambda) throw(ERRange,ERRejected);

    TFloat          Sub(TArc a) const throw(ERRange) {return Flow(a);};

    TFloat          CancelOdd() throw();

};


#endif
