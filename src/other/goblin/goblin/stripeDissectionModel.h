
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, July 2009
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file stripeDissectionModel.h
/// \brief Class interface of a stripe dissection model


#include "orthogonalGrid.h"
#include "sparseDigraph.h"

#ifndef _STRIPE_DISSECTION_MODEL_H_
#define _STRIPE_DISSECTION_MODEL_H_


/// \addtogroup groupOrthogonalDrawing
/// @{

/// \brief  A digraph model for shape-preserving post-optimization of orthogonal grid representations
///
/// This is a digraph model which dissects a given orthogonal drawing into either
/// horizonal or vertical stripes. The stripes form the node set of this digraph with
/// special nodes s,t at opposed sides of the drawing, and st-flows represent the
/// metric of prospective drawings of the original graph. Maximizing the flow value
/// means to minimize the width or height of the drawing. A zero arc flow is allowed
/// only if the corresponding original edge segemnt may be contracted.
class stripeDissectionModel : public sparseDiGraph
{
private:

    abstractMixedGraph &G; ///< The target graph object
    orthogonalGrid GM;     ///< A representational object for the start orthogonal drawing of the target graph

    TDim movingDirection; ///< The index of layout point coordinates which are optimized

public:

    /// \param _G                The target graph object
    /// \param _movingDirection  The index of lyout point coordinates which are optimized
    /// \param preserveShape     If true, all bend nodes are preserved, only edge segment lengths are optimized
    ///
    /// The constructor method sets up the incidence structure, and an initial flow corresponding
    /// to the initial drawing of the target graph.
    stripeDissectionModel(abstractMixedGraph& _G,TDim _movingDirection,bool preserveShape) throw();

    /// \brief  Update the drawing of the original graph in terms of an st-flow of the stripe dissection model
    void UpdateDrawing() throw();

};

/// @}

#endif

