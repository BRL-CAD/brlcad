
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, May 2001
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   branchStable.h
/// \brief  #branchStable class interface

#ifndef _BRANCH_STABLE_H_
#define _BRANCH_STABLE_H_

#include "branchScheme.h"
#include "abstractMixedGraph.h"


/// \addtogroup stableSet
/// @{

/// \brief  Branch & bound implementation for stable (independent) node sets

class branchStable : public branchNode<TNode,TFloat>
{
public:

    abstractMixedGraph &G;
    char* chi;
    TNode currentVar;
    TNode selected;
    THandle H;

    branchStable(abstractMixedGraph&) throw();
    branchStable(branchStable&) throw();
    ~branchStable() throw();

    unsigned long   Size() const throw();
    unsigned long   Allocated() const throw();

    TNode           SelectVariable() throw();         // Select variable to branch with
    TBranchDir      DirectionConstructive(TNode i) throw(ERRange);
    TBranchDir      DirectionExhaustive(TNode i) throw(ERRange);
                        // Select branching direction

    branchNode<TNode,TFloat>* Clone() throw();     // Generate a copy of the branch node
    void            Raise(TNode i) throw(ERRange);   // Raise lower variable bound
    void            Lower(TNode i) throw(ERRange);   // Lower upper variable bound
    TFloat          SolveRelaxation() throw();     // Returns the objective value of the relaxed
                                                   // subproblem and a corresponding solution
    void            SaveSolution() throw();        // Copy the current solution to the original instance

    TObjectSense    ObjectSense() const throw();
    TFloat          Infeasibility() const throw();

};

/// @}

#endif

//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, May 2001
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   branchStable.h
/// \brief  #branchStable class interface

#ifndef _BRANCH_STABLE_H_
#define _BRANCH_STABLE_H_

#include "branchScheme.h"
#include "abstractMixedGraph.h"


/// \addtogroup stableSet
/// @{

/// \brief  Branch & bound implementation for stable (independent) node sets

class branchStable : public branchNode<TNode,TFloat>
{
public:

    abstractMixedGraph &G;
    char* chi;
    TNode currentVar;
    TNode selected;
    THandle H;

    branchStable(abstractMixedGraph&) throw();
    branchStable(branchStable&) throw();
    ~branchStable() throw();

    unsigned long   Size() const throw();
    unsigned long   Allocated() const throw();

    TNode           SelectVariable() throw();         // Select variable to branch with
    TBranchDir      DirectionConstructive(TNode i) throw(ERRange);
    TBranchDir      DirectionExhaustive(TNode i) throw(ERRange);
                        // Select branching direction

    branchNode<TNode,TFloat>* Clone() throw();     // Generate a copy of the branch node
    void            Raise(TNode i) throw(ERRange);   // Raise lower variable bound
    void            Lower(TNode i) throw(ERRange);   // Lower upper variable bound
    TFloat          SolveRelaxation() throw();     // Returns the objective value of the relaxed
                                                   // subproblem and a corresponding solution
    void            SaveSolution() throw();        // Copy the current solution to the original instance

    TObjectSense    ObjectSense() const throw();
    TFloat          Infeasibility() const throw();

};

/// @}

#endif
