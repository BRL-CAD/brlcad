
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, January 2005
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   branchMIP.h
/// \brief  #branchMIP class interface

#ifndef _BRANCH_MIP_H_
#define _BRANCH_MIP_H_

#include "branchScheme.h"
#include "ilpWrapper.h"


/// \addtogroup mixedInteger
/// @{

/// \brief  Branch & bound implementation for mixed integer programming

class branchMIP : public branchNode<TVar,TFloat>
{
public:

    mipInstance& X;        // The original problem
    mipInstance* Y;        // Cloned from the parent branch node

    branchMIP(mipInstance&) throw();
    branchMIP(branchMIP&) throw();
    ~branchMIP() throw();

    unsigned long   Size() const throw();
    unsigned long   Allocated() const throw();

    TVar            SelectVariable() throw();      // Select variable to branch with
    TBranchDir      DirectionConstructive(TVar i) throw(ERRange);
    TBranchDir      DirectionExhaustive(TVar i) throw(ERRange);
                        // Select branching direction

    branchNode<TVar,TFloat>* Clone() throw();      // Generate a copy of the branch node
    void            Raise(TVar i) throw(ERRange);    // Raise lower variable bound
    void            Lower(TVar i) throw(ERRange);    // Lower upper variable bound
    TFloat          SolveRelaxation() throw();     // Returns the objective value of the relaxed
                                                   // subproblem and a corresponding solution
    TObjectSense    ObjectSense() const throw();
    TFloat          Infeasibility() const throw();
    bool            Feasible() throw();            // Check for integrality
    void            SaveSolution() throw();        // Copy the current solution to the original instance

};

/// @}

#endif
