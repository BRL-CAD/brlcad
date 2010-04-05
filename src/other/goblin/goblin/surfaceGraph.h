
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, February 2000
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   surfaceGraph.h
/// \brief  #surfaceGraph class interface

#ifndef _SURFACE_GRAPH_H_
#define _SURFACE_GRAPH_H_

#include "abstractBalanced.h"
#include "nestedFamily.h"
#include "dynamicStack.h"


/// \brief  Encapsulates the primal-dual code for weighted balanced network flows

class surfaceGraph : public abstractBalancedFNW
{
friend class iSurfaceGraph;
protected:

    abstractBalancedFNW &   G;  // The original network
    nestedFamily<TNode>     S;  // The blossoms

    TNode       n0; // Number of nodes in the original network (real nodes)
    TNode       nr; // Number of real node pairs
    TNode       nv; // Number of virtual node pairs (blossoms)
        // n0==nr*2, n1==nr+nv

    TFloat*     piG;        // potentials in the original network G
    TFloat*     pi;         // potentials in this surface graph
    TFloat*     modlength;  // Explicit modified length labels
    TArc*       bprop;      // Current blossom props
    bool        real;
        // Switches arc incidences between surface graph and original network

public:

    surfaceGraph(abstractBalancedFNW &GC) throw();
    ~surfaceGraph() throw();

    void            Init() throw();
    unsigned long   Size() const throw();
    unsigned long   Allocated() const throw();
    investigator*   NewInvestigator() const throw();

    TNode           StartNode(TArc a) const throw(ERRange);
    TNode           EndNode(TArc a) const throw(ERRange);

    TArc            First(TNode v) const throw(ERRange,ERRejected);
    TArc            Right(TArc a,TNode v) const throw(ERRange,ERRejected);

    TCap            LCap(TArc a) const throw(ERRange);
    bool            CLCap() const throw() {return G.CLCap();};
    TCap            UCap(TArc a) const throw(ERRange);
    bool            CUCap() const throw() {return G.CUCap();};
    TFloat          Length(TArc a) const throw(ERRange);
    bool            CLength() const throw() {return G.CLength();};
    bool            Blocking(TArc a) const throw() {return a&1;};

    TFloat          C(TNode v,TDim i) const throw(ERRange);
    TFloat          CMax(TDim i) const throw(ERRange) {return G.CMax(i);};
    TDim            Dim() const throw() {return G.Dim();};
    bool            HiddenNode(TNode v) const throw(ERRange);
    bool            HiddenArc(TArc a) const throw(ERRange);

    TFloat          Flow(TArc a) const throw(ERRange);
    TFloat          BalFlow(TArc a) const throw(ERRange,ERRejected);
    void            Push(TArc a,TFloat lambda) throw(ERRange,ERRejected);
    void            BalPush(TArc a,TFloat lambda) throw(ERRange,ERRejected);

    TFloat          Sub(TArc a) const throw(ERRange) {return BalFlow(a);};

    void            Symmetrize() throw() {G.Symmetrize();};
    void            Relax() throw() {G.Relax();};

    // --------------------------------------------------------------
    //            Handling of Blossoms and Fragments
    // --------------------------------------------------------------

    TNode           MakeBlossom(TNode b,TArc a) throw(ERRange);
        // Initializes a new blossom and returns the corresponding node index
    bool            Top(TNode v) const throw(ERRange);
        // Decides wether v is in the surface graph or shrunk into a blossom

    // --------------------------------------------------------------
    //            Handling of Modified length labels
    // --------------------------------------------------------------

    TFloat          ModLength(TArc a) const throw(ERRange);
        // Returns the label encoded into modlength
    TFloat          RModLength(TArc a) const throw(ERRange);
        // Computes the modified length label recursively
    TFloat          RedLength(const TFloat* pi,TArc a) const throw(ERRange) {return ModLength(a);};
    void            ShiftPotential(TNode v,TFloat epsilon) throw(ERRange);
        // Increases pi[v] and decreases pi[v^1] by an amount epsilon
    void            ShiftModLength(TArc a,TFloat epsilon) throw(ERRange);
        // Increases ModLength(a) and ModLength(a^2), and decreases
        // ModLength(a^1) and ModLength(a^3) by an amount epsilon

    bool            Compatible() throw();
        // Verifies the complementary slackness conditions
    void            CheckDual() throw();
        // Checks if the explicit modified length labels coincide
        // with the labels returned by RModLength(..)

    // --------------------------------------------------------------
    //         Generic Shrinking and Expansion Operations
    // --------------------------------------------------------------

    TArc FindSupport(TFloat* dist,TNode s,TArc a,dynamicStack<TNode> &Support) throw();
        // Tracks back on the prop-labels from the start nodes of
        // the petals a and a^2. Determines the blossom base b and a path
        // from b to b^1 which is returned by the Q-labels. The return value
        // is the first arc on this path.
        // The nodes forming the blossom are pushed onto the stack Support.
    void Traverse(TArc* pred,TArc aIn,TArc aOut) throw();
        // If the end node v of aIn is real, pred[v] = aIn is set.
        // Otherwise, if v is a blossom, it is expanded.
        // The end nodes of aIn and aOut must be the same!
    void Expand(TArc* pred,TArc aIn,TArc aOut) throw();
        // Tries to find a path through a blossom, starting at the
        // end node of aIn, and ending at the start node of aOut.
        // Uses the Q-labels, returns the path by the P-labels.
    TCap ExpandAndAugment(TArc aIn,TArc aOut) throw();
        // Calls Expand(aIn,aOut), determines the path capacity,
        // and finally augments on aIn, aOut, and the intermediate path.

    // --------------------------------------------------------------
    //           Primal-Dual Algorithms and Subroutines
    // --------------------------------------------------------------

    TFloat          ComputeEpsilon(TFloat* dist) throw();
        // Determines the multiplier for the restricted primal by which
        // the node potentials are iterated in the main procedure
    void PrimalDual0(TNode s) throw(ERRange);
        // Node oriented version with O(nm) operations per augmentation
    void Explore(TFloat* dist,goblinQueue<TArc,TFloat> &Q,investigator &I,TNode v) throw();
        // Puts all incidences of node v onto the queue Q
    TFloat ComputeEpsilon1(TFloat* dist) throw();
        // Determines the multiplier for the restricted primal by which
        // the node potentials are iterated in the main procedure
    void PrimalDual1(TNode s) throw(ERRange);
        // Arc oriented version with O(nm) operations per augmentation

};



/// \brief  Investigators for #surfaceGraph objects

class iSurfaceGraph : public investigator
{
private:

    const abstractBalancedFNW &G;     // The original network
    const surfaceGraph &N;            // The surface graph
    const nestedFamily<TNode> &S;     // The blossoms

    TNode       n;  // Number of nodes in the surface graph
    TNode       n0; // Number of nodes in the original network (real nodes)
    TNode       nr; // Number of real node pairs (n0==2*nr)
    TNode       nv; // Number of virtual node pairs (blossoms)
    TNode       n1; // Number of node pairs (n1==nr+nv)
    TArc        m;  // Number of arcs (which coincides in G and N)

    THandle     Handle;     // Investigator handle in the original network
    TNode *     current;    // Currently investigated element of a blossom

public:

    iSurfaceGraph(const surfaceGraph &GC) throw();
    ~iSurfaceGraph() throw();

    unsigned long   Size() const throw();
    unsigned long   Allocated() const throw();

    void            Reset() throw();
    void            Reset(TNode v) throw(ERRange);
    TArc            Read(TNode v) throw(ERRange,ERRejected);
    TArc            ReadBlossom(TNode v,TArc thisProp) throw(ERRange);
    TArc            Peek(TNode v) throw(ERRange,ERRejected);
    bool            Active(TNode v) const throw(ERRange);
    bool            ActiveBlossom(TNode v) const throw(ERRange);

};


#endif
