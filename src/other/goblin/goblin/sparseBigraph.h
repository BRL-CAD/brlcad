
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, December 1998
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   sparseBigraph.h
/// \brief  Class interface to sparse bipartite graph objects

#if !defined(_SPARSE_BIGRAPH_H_)
#define _SPARSE_BIGRAPH_H_

#include "abstractBigraph.h"
#include "sparseRepresentation.h"


/// \brief A class for bipartite graphs represented with incidence lists

class sparseBiGraph : public abstractBiGraph
{
protected:

    sparseRepresentation X;

public:

    /// \brief  Default constructor for sparse bigraphs
    ///
    /// \param _n1      The initial number of left-hand nodes
    /// \param _n2      The initial number of right-hand nodes
    /// \param _CT      The context to which this graph object is attached
    sparseBiGraph(TNode _n1=0,TNode _n2=0,goblinController& _CT=goblinDefaultContext) throw();

    /// \brief  File constructor for sparse bigraphs
    ///
    /// \param fileName  The source file name
    /// \param _CT       The context to which this graph object is attached
    sparseBiGraph(const char* fileName,goblinController& _CT=goblinDefaultContext)
        throw(ERFile,ERParse);

    /// \brief  Copy constructor for sparse bigraphs
    ///
    /// \param G        The original bigraph object
    /// \param options  A combination of OPT_CLONE, OPT_PARALLELS, OPT_MAPPINGS and OPT_SUB
    sparseBiGraph(abstractBiGraph& G,TOption options=0) throw();

    ~sparseBiGraph() throw();

    unsigned long   Size() const throw();
    unsigned long   Allocated() const throw();

    /// \brief  Move a node from one partition to another
    ///
    /// \param u  A node index ranged [0,1,..,n-1]
    ///
    /// This moves u from the left-hand side to the right-hand side or vice
    /// versa. It is required that u is an isolated node.
    TNode   SwapNode(TNode u) throw(ERRange,ERRejected);

    #include <sparseInclude.h>

};


/// \addtogroup groupGraphDerivation
/// @{

/// \brief  Subgraph of a given mixed graph induced by two disjoint node index sets
///
/// This graph maps all nodes in the two specified index sets and all arcs connecting
/// these node sets.

class inducedBigraph : public sparseBiGraph
{
public:

    inducedBigraph(abstractMixedGraph&,const indexSet<TNode>&,
        const indexSet<TNode>&,const TOption = OPT_PARALLELS) throw(ERRejected);

};

/// @}

#endif

//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, December 1998
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   sparseBigraph.h
/// \brief  Class interface to sparse bipartite graph objects

#if !defined(_SPARSE_BIGRAPH_H_)
#define _SPARSE_BIGRAPH_H_

#include "abstractBigraph.h"
#include "sparseRepresentation.h"


/// \brief A class for bipartite graphs represented with incidence lists

class sparseBiGraph : public abstractBiGraph
{
protected:

    sparseRepresentation X;

public:

    /// \brief  Default constructor for sparse bigraphs
    ///
    /// \param _n1      The initial number of left-hand nodes
    /// \param _n2      The initial number of right-hand nodes
    /// \param _CT      The context to which this graph object is attached
    sparseBiGraph(TNode _n1=0,TNode _n2=0,goblinController& _CT=goblinDefaultContext) throw();

    /// \brief  File constructor for sparse bigraphs
    ///
    /// \param fileName  The source file name
    /// \param _CT       The context to which this graph object is attached
    sparseBiGraph(const char* fileName,goblinController& _CT=goblinDefaultContext)
        throw(ERFile,ERParse);

    /// \brief  Copy constructor for sparse bigraphs
    ///
    /// \param G        The original bigraph object
    /// \param options  A combination of OPT_CLONE, OPT_PARALLELS, OPT_MAPPINGS and OPT_SUB
    sparseBiGraph(abstractBiGraph& G,TOption options=0) throw();

    ~sparseBiGraph() throw();

    unsigned long   Size() const throw();
    unsigned long   Allocated() const throw();

    /// \brief  Move a node from one partition to another
    ///
    /// \param u  A node index ranged [0,1,..,n-1]
    ///
    /// This moves u from the left-hand side to the right-hand side or vice
    /// versa. It is required that u is an isolated node.
    TNode   SwapNode(TNode u) throw(ERRange,ERRejected);

    #include <sparseInclude.h>

};


/// \addtogroup groupGraphDerivation
/// @{

/// \brief  Subgraph of a given mixed graph induced by two disjoint node index sets
///
/// This graph maps all nodes in the two specified index sets and all arcs connecting
/// these node sets.

class inducedBigraph : public sparseBiGraph
{
public:

    inducedBigraph(abstractMixedGraph&,const indexSet<TNode>&,
        const indexSet<TNode>&,const TOption = OPT_PARALLELS) throw(ERRejected);

};

/// @}

#endif
