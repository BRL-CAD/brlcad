
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, February 2004
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   branchMaxCut.h
/// \brief  #branchMaxCut class interface

#ifndef _BRANCH_MAX_CUT_H_
#define _BRANCH_MAX_CUT_H_

#include "branchScheme.h"
#include "abstractMixedGraph.h"


/// \addtogroup maxCut
/// @{

/// \brief  Branch & bound implementation for maximum edge cuts

class branchMaxCut : public branchNode<TNode,TFloat>
{
private:

    char* chi;
    TFloat* leftWeight;
    TFloat* rightWeight;

    TNode source;
    TNode target;

public:

    TFloat totalWeight;
    TFloat selectedWeight;
    TFloat dismissedWeight;

    abstractMixedGraph &G;
    TNode currentVar;

    branchMaxCut(abstractMixedGraph&,TNode = NoNode,TNode = NoNode) throw();
    branchMaxCut(branchMaxCut&) throw();
    ~branchMaxCut() throw();

    unsigned long   Size() const throw();
    unsigned long   Allocated() const throw();

    TFloat          SumWeight(TNode) throw();   // Estimation for the heuristic relevance of a node
    TFloat          MinWeight(TNode) throw();   // Minimum Profit if the node is coloured now
    TFloat          MaxWeight(TNode) throw();   // Maximum Profit if the node is coloured now

    TNode           SelectVariable() throw();         // Select variable to branch with
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

    void            SaveSolution() throw() {};    // Solution is saved by LocalSearch()
    void            ReallySaveSolution() throw(); // Copy the current solution to the original instance
    TFloat          LocalSearch() throw();        // Post-Optimization for improved feasible solutions

};

/// @}

#endif

//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, February 2004
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   branchMaxCut.h
/// \brief  #branchMaxCut class interface

#ifndef _BRANCH_MAX_CUT_H_
#define _BRANCH_MAX_CUT_H_

#include "branchScheme.h"
#include "abstractMixedGraph.h"


/// \addtogroup maxCut
/// @{

/// \brief  Branch & bound implementation for maximum edge cuts

class branchMaxCut : public branchNode<TNode,TFloat>
{
private:

    char* chi;
    TFloat* leftWeight;
    TFloat* rightWeight;

    TNode source;
    TNode target;

public:

    TFloat totalWeight;
    TFloat selectedWeight;
    TFloat dismissedWeight;

    abstractMixedGraph &G;
    TNode currentVar;

    branchMaxCut(abstractMixedGraph&,TNode = NoNode,TNode = NoNode) throw();
    branchMaxCut(branchMaxCut&) throw();
    ~branchMaxCut() throw();

    unsigned long   Size() const throw();
    unsigned long   Allocated() const throw();

    TFloat          SumWeight(TNode) throw();   // Estimation for the heuristic relevance of a node
    TFloat          MinWeight(TNode) throw();   // Minimum Profit if the node is coloured now
    TFloat          MaxWeight(TNode) throw();   // Maximum Profit if the node is coloured now

    TNode           SelectVariable() throw();         // Select variable to branch with
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

    void            SaveSolution() throw() {};    // Solution is saved by LocalSearch()
    void            ReallySaveSolution() throw(); // Copy the current solution to the original instance
    TFloat          LocalSearch() throw();        // Post-Optimization for improved feasible solutions

};

/// @}

#endif
