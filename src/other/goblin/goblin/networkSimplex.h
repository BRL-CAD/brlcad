
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, March 2004
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   networkSimplex.h
/// \brief  #networkSimplex class interface

#ifndef _NETWORK_SIMPLEX_H_
#define _NETWORK_SIMPLEX_H_

#include "abstractMixedGraph.h"


/// \brief  Encapsulates auxiliary data structures and procedures of the network simplex method

class networkSimplex : public managedObject
{
private:

    abstractDiGraph &G; // Related graph Object

    TNode   n;          // Number of graph nodes
    TArc    m;          // Number of graph arcs

    TFloat* piG;        // Shortcut to the graphs' node potentials
    TArc*   pred;       // Shortcut to the graphs' predecessor arcs 

    TArc    r;          // Number of candidate lists
    TArc    k;          // Size of candidate lists
    TArc    j;          // Threshold size of hotList
    TArc    l;          // Current length of hotList

    TArc    nextList;   // Index of candidate list which is considered next

    TArc*   hotList;
    TArc*   swapList;

    TNode*  thread;     // Follow up the nodes in the tree defined by P
    TNode*  depth;      // Distance from the root in the tree solution

public:

    networkSimplex(abstractDiGraph&) throw();
    ~networkSimplex() throw();

    unsigned long   Size() const throw() {return 0;};

    TArc            PivotArc() throw();
    TArc            PartialPricing() throw();
    TArc            MultiplePartialPricing() throw();
    TArc            DantzigPricing() throw();
    TArc            FirstEligiblePricing() throw();

    void            InitThreadIndex() throw();
    void            ComputePotentials() throw();
    TNode           UpdateThread(TNode,TNode,TNode) throw();
    bool            PivotOperation(TArc) throw();

};


#endif

