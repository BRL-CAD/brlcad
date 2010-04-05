
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, November 2000
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   branchColour.h
/// \brief  #branchColour class interface

#ifndef _BRANCH_COLOUR_H_
#define _BRANCH_COLOUR_H_

#include "branchScheme.h"
#include "abstractMixedGraph.h"
#include "staticStack.h"


/// \addtogroup colouring
/// @{

/// \brief  Branch & bound implementation for node colouring

class branchColour : public branchNode<TNode,TFloat>
{
private:

    abstractMixedGraph& G;
        // The original graph

    bool master; // Distinguish the master problem

    TNode   n;
    TArc    m;
        // Dimensions of the original graph

    TNode   nActive;
    TNode   nDominated;
    TNode   nColoured;
        // Number of active, dominated and coloured nodes

    TNode                       kMax;
        // The maximum node degree (constant).

    TNode                       k;
        // The desired number of colours. Bounded by kMax+1,
        // and initialized to kMax.

    TNode                       DOMINATED;
        // Colour which indicates that a node is dominated. That is,
        // the node has small degree, and every partial colouring which
        // covers the undominated nodes can be completed without conflicts.
        // Effectively, dominated nodes are deleted.

    TNode*                      colour;
        // Node colours

    bool*                       active;
        // True for the nodes which are neither coloured nor dominated

    TNode**                     neighbours;
        // For each colour c in 0,1,..,kMax-1 and each node v,
        // the number of neighbours of v which are coloured with c.

    TNode*                      conflicts;
        // For each node v, the number of colours which are occupied by a
        // neighbour of v plus the number of uncoloured neighbours of v.

    investigator*               I;
        // An investigator object which is used by Set and Reduce operations.

    staticStack<TNode>*  Dominated;
        // A stack which is used to colour the dominated nodes
        // in the correct order.

    bool                        exhaustive;
        // This controls the selection of branch nodes and colours.
        // If false, the smallest possible colour is used for branching.
        // This is intended to generate many dominated nodes and solutions.
        // quickly. If true, a complete enumeration is optimized to generate
        // a small number of branch nodes.

public:

    branchColour(abstractMixedGraph &,TNode,char = 0) throw();
    branchColour(branchColour &) throw();
    ~branchColour() throw();

    unsigned long   Size() const throw();
    unsigned long   Allocated() const throw();

    void            Show() throw();

private:

    void            SetColour(TNode v,TNode c) throw(ERRange,ERRejected);
    void            Reduce(TNode w = NoNode) throw(ERRange,ERRejected);

public: 

    bool            Complete() throw(ERRejected);
    void            PlanarComplete() throw(ERCheck);

    TNode           SelectVariable() throw();    // Select variable to branch with
    TBranchDir      DirectionConstructive(TNode i) throw(ERRange);
    TBranchDir      DirectionExhaustive(TNode i) throw(ERRange);
                        // Select branching direction

    branchNode<TNode,TFloat>* Clone() throw();     // Generate a copy of the branch node
    void            Raise(TNode i) throw(ERRange);   // Raise lower variable bound
    void            Lower(TNode i) throw(ERRange);   // Lower upper variable bound
    TFloat          SolveRelaxation() throw();     // Returns the objective value of the relaxed
                                                   // subproblem and a corresponding solution
    TObjectSense    ObjectSense() const throw();
    TFloat          Infeasibility() const throw();

    void            SaveSolution() throw();      // Copy the current solution to the original instance

};

/// @}

#endif
