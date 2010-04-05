
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, February 2001
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   branchScheme.h
/// \brief  Class interfaces for the universal branch & bound solver

#ifndef _BRANCH_SCHEME_H_
#define _BRANCH_SCHEME_H_

#include "moduleGuard.h"
#include "sparseDigraph.h"


template <class TItem,class TObj>
class branchScheme;

/// \addtogroup branchAndBound
/// @{

/// \brief  Base class for branch and bound subproblems

template <class TItem,class TObj>
class branchNode : public managedObject
{
protected:

    TItem      n;          // Total number of (integer) variables
    TItem      unfixed;    // Number of unfixed (integer) variables

    TObj        objective;  // Objective value of the relaxation
    bool        solved;     // Has relaxation been solved yet?

    /// \brief  Solve the relaxed subproblem according
    /// 
    /// \return  The objective value of this relaxed subproblem
    virtual TObj    SolveRelaxation() throw() = 0;

public:

    branchScheme<TItem,TObj>* scheme;   // Scheme
    branchNode<TItem,TObj>*   succNode; // Successor node in the list of active nodes
    TNode           index;      // Node index in the branch tree
    TItem          depth;      // Bound on the number of non-zeros

    branchNode(TItem nn,goblinController& thisContext,
            branchScheme<TItem,TObj>* = NULL) throw();
    ~branchNode() throw();

    unsigned long   Allocated() const throw();

    TObj            Objective() throw();
    virtual bool    Feasible() throw();
    TItem           Unfixed() throw() {return unfixed;};

    /// \brief  Selection of a variable to be restricted next
    ///
    /// \return  The branch variable index
    virtual TItem SelectVariable() throw() = 0;

    /// \brief  Specifier for branching directions
    enum TBranchDir {
        LOWER_FIRST = 0, ///< When restricting a variable, consider the lower interval first
        RAISE_FIRST = 1  ///< When restricting a variable, consider the upper interval first
    };

    /// \brief  Preferred search direction before feasibility has been achieved
    virtual TBranchDir DirectionConstructive(TItem i) throw(ERRange) = 0;

    /// \brief  Preferred search direction after feasibility has been achieved
    virtual TBranchDir DirectionExhaustive(TItem i) throw(ERRange) = 0;

    /// \brief  Generate a copy of this branch node
    ///
    /// \return  A pointer to the copy
    virtual branchNode<TItem,TObj>*  Clone() throw() = 0;

    /// \brief  Raise a lower variable bound
    ///
    /// This operation applies after solving a relaxation and duplicating
    /// the branch node. It sets a lower variable bound to the least integral
    /// upper bound on the current variable value.
    ///
    /// The operation may restrict further variables, namely, if it is
    /// obvoious that no solutions for the master problem are lost.
    ///
    /// \param i  The index of the variable to be restricted
    virtual void Raise(TItem i) throw(ERRange) = 0;

    /// \brief  Lower an upper variable bound
    ///
    /// This operation applies after solving a relaxation and duplicating
    /// the branch node. It sets an upper variable bound to the greatest integral
    /// lower bound on the current variable value.
    ///
    /// The operation may restrict further variables, namely, if it is
    /// obvoious that no solutions for the master problem are lost.
    ///
    /// \param i  The index of the restricted variable
    virtual void Lower(TItem i) throw(ERRange) = 0;

    /// \brief  Decide between maximization and minimization problems
    virtual TObjectSense ObjectSense() const throw() = 0;

    /// \brief  Symbolic value for infeasible subproblems
    virtual TObj Infeasibility() const throw() = 0;

    /// \brief  Copy the current solution to the original problem instance
    virtual void SaveSolution() throw() = 0;

    /// \brief  Apply a probelm dependent local search method
    ///
    /// \return  The (possibly improved) objective value
    virtual TObj LocalSearch() throw();

};



#ifdef _TRACING_


/// \brief  Visualization of the branched subproblems

class branchTree : public sparseDiGraph
{
public:

    branchTree(goblinController &) throw();

};


#endif // _TRACING_


/// \brief  Implementation of a universal branch and bound strategy

template <class TItem,class TObj>
class branchScheme : public managedObject
{
private:

    /// \brief  Branch node classification
    enum nodeType {
        PROCESSED = 0, ///< The branch node has been evaluated and split into two subproblems
        QUEUED    = 1, ///< The branch node has been explored but not evaluated yet
        SAVED     = 2, ///< The found relaxed solution is feasible for the original problem and has been saved
        CUTOFF    = 3  ///< The branch node has been evaluated and cannot contribute an optimal solution
    };

    branchNode<TItem,TObj>* firstActive;

    sparseDiGraph* Tree; ///< Branch tree (for tracing)
    THandle  LH;         ///< Handle for compound log entries

protected:

    void Optimize() throw();
    bool Inspect(branchNode<TItem,TObj>*) throw();
    branchNode<TItem,TObj>* SelectActiveNode() throw();
    void QueueExploredNode(branchNode<TItem,TObj>*) throw();
    void StripQueue() throw();

public:

    moduleGuard M;

    unsigned long  nActive;     ///< Number of queued subproblems
    unsigned long  maxActive;   ///< Maximum number of queued subproblems
    unsigned long  nIterations; ///< Total number of evaluated subproblems
    unsigned long  nDFS;        ///< Number of dfs steps since the last bfs or improvement
    unsigned long  depth;       ///< Bound on the number of non-zeros (only a copy)

    bool  feasible; ///< Flag indicating if problem feasibility has been reached yet

    /// \brief  The intended result of applying a branch scheme
    enum TSearchLevel {
        SEARCH_FEASIBLE   = 0, ///< Only a single feasible solution is produced
        SEARCH_CONSTRUCT  = 1, ///< Start with some depth first search steps to obtain good solutions
        SEARCH_EXHAUSTIVE = 2  ///< Pure best first search since a good solution is already known
    };

    TSearchLevel level;

    TFloat  sign;           ///< Object sense imported from the root node
    TObj    savedObjective;
    TObj    bestBound;

    branchScheme(branchNode<TItem,TObj>*,TObj,TModule module,TSearchLevel = SEARCH_EXHAUSTIVE) throw();
    ~branchScheme() throw();

    /// \brief  The currently applied search strategy
    enum TSearchState {
        INITIAL_DFS    = 0, ///< Apply depth first search during the first iterations
        CONSTRUCT_BFS  = 1, ///< Apply best first search in waves
        CONSTRUCT_DFS  = 2, ///< Apply depth first search in waves
        EXHAUSTIVE_BFS = 3, ///< Apply best first search when memory is running low
        EXHAUSTIVE_DFS = 4  ///< Apply depth first search when memory is almost exhausted
    };

    /// \brief  Query the current search state
    TSearchState SearchState() throw();

    unsigned long   Size() const throw();
    unsigned long   Allocated() const throw();

};

/// @}

#endif // _BRANCH_SCHEME_H_
