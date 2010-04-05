
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, December 1998
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   sparseDigraph.h
/// \brief  Class interface to sparse directed graph objects

#if !defined(_SPARSE_DIGRAPH_H_)
#define _SPARSE_DIGRAPH_H_

#include "abstractDigraph.h"
#include "sparseRepresentation.h"


/// \brief A class for directed graphs represented with incidence lists

class sparseDiGraph : public abstractDiGraph
{
    friend class sparseRepresentation;

private:

    bool mode;

protected:

    sparseRepresentation X;

public:

    /// \brief  Default constructor for sparse digraphs
    ///
    /// \param _n       The initial number of nodes
    /// \param _CT      The context to which this graph object is attached
    /// \param _mode    If true, the graph is not displayed before destruction
    sparseDiGraph(TNode _n=0,goblinController& _CT=goblinDefaultContext,
        bool _mode = false) throw();

    /// \brief  File constructor for sparse digraphs
    ///
    /// \param fileName  The source file name
    /// \param _CT       The context to which this graph object is attached
    sparseDiGraph(const char* fileName,goblinController& _CT=goblinDefaultContext)
        throw(ERFile,ERParse);

    /// \brief  Copy constructor for sparse digraphs
    ///
    /// \param G        The original mixed graph object
    /// \param options  A combination of OPT_CLONE, OPT_PARALLELS, OPT_MAPPINGS and OPT_SUB
    sparseDiGraph(abstractMixedGraph& G,TOption options=0) throw();

    ~sparseDiGraph() throw();

    unsigned long   Size() const throw();
    unsigned long   Allocated() const throw();

    #include <sparseInclude.h>

};


/// \addtogroup groupGraphDerivation
/// @{

/// \brief Complete orientation of a given mixed graph object
///
/// This digraph replaces all undirected edges of the original mixed graph object
/// by an antiparallel arc pair. If the #OPT_REVERSE option is specified in the
/// constructor call, directed arcs are also duplicated.

class completeOrientation : public sparseDiGraph
{
private:

    TArc*   origin;
    char    type;

public:

    completeOrientation(abstractMixedGraph &G,TOption options = 0) throw();
    ~completeOrientation() throw();

    unsigned long   Size() throw();
    unsigned long   Allocated() throw();

    TIndex  OriginalOfArc(TArc a) throw(ERRange);

    /// \brief  Map a subgraph from the original graph to this object
    ///
    /// \param G  A reference of the original graph
    void  MapFlowForward(abstractMixedGraph &G) throw(ERRejected);

    /// \brief  Map a subgraph back to the original graph
    ///
    /// \param G  A reference of the original graph
    void  MapFlowBackward(abstractMixedGraph &G) throw(ERRejected);

};


/// \brief Orientation of a given mixed graph object
///
/// This digraph orientates all arcs and edges of the original mixed graph object
/// so that the start node has a smaller node colour index than the end node. So,
/// if the node colours are pairwise different, a DAG results. If the node colours
/// encode an st-numbering, even a bipolar digraph results.

class inducedOrientation : public sparseDiGraph
{
public:

    inducedOrientation(abstractMixedGraph &G,TOption options = 0) throw(ERRejected);

};


/// \brief Split all nodes of a given mixed graph object
///
/// In this digraph, every node of the original graph is replaced by a node pair
/// and a directed arc connecting these two nodes. The original arcs end up at the mapped
/// start nodes and start at the mapped end nodes. Additionally, the original arcs are
/// oriented by applying the same rules as in the #completeOrientation constructor.

class nodeSplitting : public sparseDiGraph
{

private:

    abstractMixedGraph& G; ///< A reference of the original graph
    bool mapUnderlying;    ///< Corresponds with the MCC_MAP_UNDERLYING constructor option
    bool mapCapacities;    ///< Corresponds with the MCC_MAP_DEMANDS constructor option

public:

    /// \param _G       The original mixed graph
    /// \param options  A combination of TOptNodeSplitting values
    nodeSplitting(abstractMixedGraph& _G,TOption options = 0) throw();

    /// \brief  Map back an edge cut to the original graph
    ///
    /// This sets the node colours of the original graph depending on the colours
    /// of the respective node pair in the node splitting object. In the case of
    /// unit node capacities (not using the MCC_MAP_DEMANDS option), a minimum
    /// cut of the node splitting is transformed to a minimum node cut of the
    /// original graph (such that left-hand nodes and right-hand nodes are
    /// non-adjacent.
    void MapEdgeCut() throw();

};

/// @}


/// \addtogroup groupGraphSeries
/// @{

/// \brief A regular tree

class regularTree : public sparseDiGraph
{
public:

    regularTree(TNode _depth,TNode deg,TNode _n = NoNode,
        goblinController& thisContext = goblinDefaultContext) throw(ERRejected);

};


/// \brief Layered regular digraph
///
/// The (k,b)-butterfly consists of k+1 layers. Each layer consists of b^k nodes,
/// representing the k-digit numbers about an alphabet of cardinality b.
/// Moving from one layer i to the next layer (i+1) means to choose the i-th
/// digit value.
///
/// For any pair of nodes, one node on layer 0 and one node on layer k, there
/// is a unique directed path connecting these nodes.
///
/// The maximal bicliques are K_b,b, and every node is incident with such a biclique.
/// In the standard setting, b equals 2, and the K_2,2 bicliques have motivated
/// the term butterfly.

class butterflyGraph : public sparseDiGraph
{
public:

    /// \param length       The number of digits
    /// \param base         The cardinality of the alphabet
    /// \param thisContext  The context to which this graph object is attached
    butterflyGraph(TNode length,TNode base=2,goblinController& thisContext = goblinDefaultContext) throw();

};


/// \brief Regular digraph with a cyclic layering
///
/// The cyclic (k,b) cyclic butterfly graph can be obtained from the (k,b) butterflyGraph
/// by pairwise identification of the nodes in the two extreme layers.
///
/// Every graph node has b incoming and b outgoing arcs, and so the graph is
/// Eulerian and 2b-regular. The length of all directed cycles is a multiple of k
/// and through every node, there is a unique directed cycle of length k.

class cyclicButterfly : public sparseDiGraph
{
public:

    /// \param length       The number of digits
    /// \param base         The cardinality of the alphabet
    /// \param thisContext  The context to which this graph object is attached
    cyclicButterfly(TNode length,TNode base=2,goblinController& thisContext = goblinDefaultContext) throw();

};

/// @}


/// \addtogroup groupPlanarComposition
/// @{

/// \brief Directed duals of planar bipolar digraphs
///
/// The nodes of the dual graph are the faces in the original graph. Every edge
/// of the original graph is associated with an edge in the dual graph where it
/// connects the nodes mapped from the faces which where originally separated by
/// this arc.
///
/// As a specialization of the undirected dual graph, the arc orientations are
/// set according to a st-numbering given in advance.

class directedDual : public sparseDiGraph
{
public:

    directedDual(abstractMixedGraph &G,TOption options = 0) throw(ERRejected);

};

/// @}


/// \addtogroup groupDagComposition
/// @{

/// \brief The minimal transitive supergraph of a given DAG
///
/// This digraph adds to the original digraph all edges a=uv for which a directed
/// uv-path exists.

class transitiveClosure : public sparseDiGraph
{
public:

    transitiveClosure(abstractDiGraph &G,TOption options = 0) throw(ERRejected);

};


/// \brief The maximal subgraph of a given DAG which does not have transitive edges
///
/// This digraph eliminates from the original digraph all edges a=uv for which a
/// non-trivial directed uv-path can be found.

class intransitiveReduction : public sparseDiGraph
{
public:

    intransitiveReduction(abstractDiGraph &G,TOption options = 0) throw(ERRejected);

};

/// @}


#endif
