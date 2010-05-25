
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, August 2009
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file movingLineModel.h
/// \brief Class interface of a moving line model


#include "orthogonalGrid.h"
#include "sparseDigraph.h"

#ifndef _MOVING_LINE_MODEL_H_
#define _MOVING_LINE_MODEL_H_


/// \addtogroup groupOrthogonalDrawing
/// @{

/// \brief  A digraph model for exhaustive post-optimization of orthogonal grid representations
///
/// This is a digraph model which dissects a given orthogonal drawing into its
/// grid cells. The grid cells form the node set of this digraph. The purpose
/// is to find a closed walk C in this digraph such that moving the right-hand
/// side of C relative to the left-hand side by one unit maintains a feasible
/// drawing. To this end, only special pairs of neighbor cells are connected.
/// Arc lengths are assigned such that a negative length cycle C represents a
/// block move which is an improvement in terms of the total edge span and the
/// number of bend points
class movingLineModel : public sparseDiGraph
{
private:

    abstractMixedGraph &G; ///< The target graph object
    orthogonalGrid GM;     ///< A representational object for the start orthogonal drawing of the target graph

    TMovingDirection movingDirection; ///< The index of layout point coordinates which are optimized

    /// Enumeration values for the block assignment
    enum TBlockAssignment
    {
        BLOCK_UNASSIGNED = 0, ///< The grid cell has not been assigned to the left-hand or right-hand side yet
        BLOCK_LEFT       = 1, ///< The grid cell has been assigned to the left-hand side
        BLOCK_RIGHT      = 2  ///< The grid cell has been assigned to the right-hand side
    };

    goblinHashTable<TIndex,int> blockAssignment; ///< A hash table to store the block assignment

public:

    /// \param _G                The target graph object
    /// \param _movingDirection  The index of lyout point coordinates which are optimized
    ///
    /// The constructor method sets up the incidence structure, and also the arc lengths.
    movingLineModel(abstractMixedGraph& _G,TDim _movingDirection) throw();

    /// \brief  Search for a negative-length cycle and, in occasion, assign all grid cells to either the left or right-hand side
    ///
    /// \retval true  If a negative length cycle has been found
    bool ExtractMovingBlock() throw();

    /// \brief  Modify the drawing of the original graph in terms of a closed walk in the stripe dissection model
    void PerformBlockMove() throw();

};

/// @}

#endif


//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, August 2009
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file movingLineModel.h
/// \brief Class interface of a moving line model


#include "orthogonalGrid.h"
#include "sparseDigraph.h"

#ifndef _MOVING_LINE_MODEL_H_
#define _MOVING_LINE_MODEL_H_


/// \addtogroup groupOrthogonalDrawing
/// @{

/// \brief  A digraph model for exhaustive post-optimization of orthogonal grid representations
///
/// This is a digraph model which dissects a given orthogonal drawing into its
/// grid cells. The grid cells form the node set of this digraph. The purpose
/// is to find a closed walk C in this digraph such that moving the right-hand
/// side of C relative to the left-hand side by one unit maintains a feasible
/// drawing. To this end, only special pairs of neighbor cells are connected.
/// Arc lengths are assigned such that a negative length cycle C represents a
/// block move which is an improvement in terms of the total edge span and the
/// number of bend points
class movingLineModel : public sparseDiGraph
{
private:

    abstractMixedGraph &G; ///< The target graph object
    orthogonalGrid GM;     ///< A representational object for the start orthogonal drawing of the target graph

    TMovingDirection movingDirection; ///< The index of layout point coordinates which are optimized

    /// Enumeration values for the block assignment
    enum TBlockAssignment
    {
        BLOCK_UNASSIGNED = 0, ///< The grid cell has not been assigned to the left-hand or right-hand side yet
        BLOCK_LEFT       = 1, ///< The grid cell has been assigned to the left-hand side
        BLOCK_RIGHT      = 2  ///< The grid cell has been assigned to the right-hand side
    };

    goblinHashTable<TIndex,int> blockAssignment; ///< A hash table to store the block assignment

public:

    /// \param _G                The target graph object
    /// \param _movingDirection  The index of lyout point coordinates which are optimized
    ///
    /// The constructor method sets up the incidence structure, and also the arc lengths.
    movingLineModel(abstractMixedGraph& _G,TDim _movingDirection) throw();

    /// \brief  Search for a negative-length cycle and, in occasion, assign all grid cells to either the left or right-hand side
    ///
    /// \retval true  If a negative length cycle has been found
    bool ExtractMovingBlock() throw();

    /// \brief  Modify the drawing of the original graph in terms of a closed walk in the stripe dissection model
    void PerformBlockMove() throw();

};

/// @}

#endif

