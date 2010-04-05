
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, February 1999
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   abstractBalanced.h
/// \brief  #abstractBalancedFNW class interface

#ifndef _ABSTRACT_BALANCED_H_
#define _ABSTRACT_BALANCED_H_

#include "abstractDigraph.h"


/// \brief The base class for all kinds of balanced (skew-symmetric) digraph objects

class abstractBalancedFNW : public abstractDiGraph
{
friend class balancedToBalanced;
protected:

    TNode   n1;     ///< Number of outer nodes
    TArc*   Q;      ///< An array for encoding of odd cycles
    TArc*   prop;   ///< An array of node props
    TArc*   petal;  ///< An array of node petals
    TNode*  base;   ///< An array of blossom bases

public:

    abstractBalancedFNW(TNode _n1=0,TArc _m1=0) throw();
    virtual ~abstractBalancedFNW() throw();

    unsigned long   Allocated() const throw();

    bool            IsBalanced() const throw() {return true;};

    virtual void    Symmetrize() throw() = 0;
    virtual void    Relax() throw() = 0;

    TNode           N1() const throw() {return n1;};

    TNode           ComplNode(TNode v) throw(ERRange);
    TArc            ComplArc(TArc a) throw(ERRange);

    // *************************************************************** //
    //           Management of Odd Cycles                              //
    // *************************************************************** //

    void            InitCycles() throw();
    void            ReleaseCycles() throw();

    // *************************************************************** //
    //           Management of Blossoms and Blossom Bases              //
    // *************************************************************** //

    void            InitBlossoms() throw();
    void            ReleaseBlossoms() throw();
    TNode           Base(TNode v) throw(ERRange);
    TNode           Pred(TNode v) throw(ERRange);
    void            Bud(TNode v) throw(ERRange);
    void            Shrink(TNode u,TNode v) throw(ERRange);

    void            InitPartition() throw() {InitBlossoms();};
    void            ReleasePartition() throw() {ReleaseBlossoms();};
    void            Merge(TNode u,TNode v) throw(ERRange) {Shrink(u,v);};
    TNode           Find(TNode v) throw(ERRange) {return Base(v);};

    // *************************************************************** //
    //           Management of Props                                   //
    // *************************************************************** //

    void            InitProps() throw();
    void            ReleaseProps() throw();

    // *************************************************************** //
    //           Management of Petals                                  //
    // *************************************************************** //

    void            InitPetals() throw();
    void            ReleasePetals() throw();

    TFloat          FindBalCap(TArc*,TNode u,TNode v) throw(ERRange,ERRejected);
    virtual void    BalPush(TArc a,TFloat lambda) throw(ERRange,ERRejected) = 0;
    virtual TFloat  BalFlow(TArc a) const throw(ERRange,ERRejected) = 0;
    virtual TFloat  BalCap(TArc a) const throw(ERRange,ERRejected);
    void            BalAugment(TArc*,TNode,TNode,TFloat) throw(ERRange,ERRejected);

    void            CancelEven() throw();
    void            MakeIntegral(TArc*,TNode,TNode) throw(ERRange);

    TFloat          MaxBalFlow(TNode s) throw(ERRange);
    TFloat          Anstee(TNode s) throw(ERRange);
    virtual TFloat  CancelOdd() throw();
    TFloat          MicaliVazirani(TNode s,TNode t=NoNode) throw(ERRange);

    TFloat          BNSAndAugment(TNode s) throw(ERRange);
    TFloat          BalancedScaling(TNode s) throw(ERRange);
    bool            BNS(TNode s,TNode t=NoNode) throw(ERRange,ERRejected);
    void            Expand(TNode* dist,TArc*,TNode,TNode) throw(ERRange);
    void            CoExpand(TNode* dist,TArc*,TNode,TNode) throw(ERRange);

    bool            BNSKocayStone(TNode s,TNode t=NoNode) throw(ERRange);
    bool            BNSKamedaMunro(TNode s,TNode t=NoNode) throw(ERRange);
    bool            BNSHeuristicsBF(TNode s,TNode t=NoNode) throw(ERRange);
    bool            BNSMicaliVazirani(TNode s,TNode t=NoNode) throw(ERRange);

    TFloat          MinCBalFlow(TNode s) throw(ERRange,ERRejected);
    TFloat          PrimalDual(TNode s) throw(ERRange,ERRejected);
    TFloat          EnhancedPD(TNode s) throw(ERRange,ERRejected);
    TFloat          CancelPD() throw();

};


#endif
