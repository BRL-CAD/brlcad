
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, September 2001
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   mixedGraph.h
/// \brief  Class interface to mixed graph objects

#if !defined(_MIXED_GRAPH_H_)
#define _MIXED_GRAPH_H_

#include "abstractMixedGraph.h"
#include "sparseRepresentation.h"
#include "nestedFamily.h"


/// \brief A class for mixed graphs represented with incidence lists

class mixedGraph : public abstractMixedGraph
{
    friend class sparseRepresentation;

protected:

    sparseRepresentation X;

public:

    /// \brief  Default constructor for mixed graphs
    ///
    /// \param _n       The initial number of nodes
    /// \param _CT      The context to which this graph object is attached
    mixedGraph(TNode _n=0,goblinController& _CT=goblinDefaultContext) throw();

    /// \brief  File constructor for mixed graphs
    ///
    /// \param fileName  The source file name
    /// \param _CT       The context to which this graph object is attached
    mixedGraph(const char* fileName,goblinController& _CT=goblinDefaultContext)
        throw(ERFile,ERParse);

    /// \brief  Copy constructor for sparse undirected graphs
    ///
    /// \param G        The original mixed graph object
    /// \param options  A combination of OPT_MAPPINGS and OPT_SUB
    mixedGraph(abstractMixedGraph& G,TOption options=0) throw();

    ~mixedGraph() throw();

    unsigned long   Size() const throw();
    unsigned long   Allocated() const throw();

    #include <sparseInclude.h>

};


/// \addtogroup groupGraphDerivation
/// @{


/// \brief  Line graph
///
/// A line graph has the original graph edges as its node set. Nodes are adjacent
/// if the original edges have an end node in common.

class lineGraph : public mixedGraph
{
public:

    enum TOptLineGraph {
        LG_UNDIRECTED = 0,
        LG_DIRECTED   = 1
    };

    lineGraph(abstractMixedGraph& G,TOption = LG_DIRECTED) throw(ERRejected);

};

/// \brief  Subgraph of a given mixed graph induced by a specified node or arc set
///
/// This graph maps all nodes in a specified index set and all arcs whose both
/// end nodes are mapped. If an arc index set is specified, this further restricts
/// the mapped arc set.

class inducedSubgraph : public mixedGraph
{
public:

    inducedSubgraph(abstractMixedGraph&,const indexSet<TNode>&,
        const TOption = OPT_PARALLELS) throw(ERRejected);
    inducedSubgraph(abstractMixedGraph&,const indexSet<TNode>&,
        const indexSet<TArc>&,const TOption = OPT_PARALLELS)
        throw(ERRejected);

};


/// \brief  Identify all nodes with the same node colour index
///
/// Nodes in the original graph with the same colour index are mapped to a single
/// node, and orginal arcs are mapped only if they connect different colour classes.

class colourContraction : public mixedGraph
{
public:

    colourContraction(abstractMixedGraph&,const TOption = 0) throw();

};


/// \brief  Surface graph of a given mixed graph and a nested family
///
/// This is used for the display of surface graphs which occur in the Edmonds'
/// spanning arborescence method.

class explicitSurfaceGraph : public mixedGraph
{
public:

    explicitSurfaceGraph(abstractMixedGraph&,nestedFamily<TNode>&,
            TFloat*,TArc*) throw();

};


/// \brief  Replacement of edge control points by graph nodes

class explicitSubdivision : public mixedGraph
{
public:

    explicitSubdivision(abstractMixedGraph&,const TOption = 0) throw();

};

/// @}

#endif

//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, September 2001
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   mixedGraph.h
/// \brief  Class interface to mixed graph objects

#if !defined(_MIXED_GRAPH_H_)
#define _MIXED_GRAPH_H_

#include "abstractMixedGraph.h"
#include "sparseRepresentation.h"
#include "nestedFamily.h"


/// \brief A class for mixed graphs represented with incidence lists

class mixedGraph : public abstractMixedGraph
{
    friend class sparseRepresentation;

protected:

    sparseRepresentation X;

public:

    /// \brief  Default constructor for mixed graphs
    ///
    /// \param _n       The initial number of nodes
    /// \param _CT      The context to which this graph object is attached
    mixedGraph(TNode _n=0,goblinController& _CT=goblinDefaultContext) throw();

    /// \brief  File constructor for mixed graphs
    ///
    /// \param fileName  The source file name
    /// \param _CT       The context to which this graph object is attached
    mixedGraph(const char* fileName,goblinController& _CT=goblinDefaultContext)
        throw(ERFile,ERParse);

    /// \brief  Copy constructor for sparse undirected graphs
    ///
    /// \param G        The original mixed graph object
    /// \param options  A combination of OPT_MAPPINGS and OPT_SUB
    mixedGraph(abstractMixedGraph& G,TOption options=0) throw();

    ~mixedGraph() throw();

    unsigned long   Size() const throw();
    unsigned long   Allocated() const throw();

    #include <sparseInclude.h>

};


/// \addtogroup groupGraphDerivation
/// @{


/// \brief  Line graph
///
/// A line graph has the original graph edges as its node set. Nodes are adjacent
/// if the original edges have an end node in common.

class lineGraph : public mixedGraph
{
public:

    enum TOptLineGraph {
        LG_UNDIRECTED = 0,
        LG_DIRECTED   = 1
    };

    lineGraph(abstractMixedGraph& G,TOption = LG_DIRECTED) throw(ERRejected);

};

/// \brief  Subgraph of a given mixed graph induced by a specified node or arc set
///
/// This graph maps all nodes in a specified index set and all arcs whose both
/// end nodes are mapped. If an arc index set is specified, this further restricts
/// the mapped arc set.

class inducedSubgraph : public mixedGraph
{
public:

    inducedSubgraph(abstractMixedGraph&,const indexSet<TNode>&,
        const TOption = OPT_PARALLELS) throw(ERRejected);
    inducedSubgraph(abstractMixedGraph&,const indexSet<TNode>&,
        const indexSet<TArc>&,const TOption = OPT_PARALLELS)
        throw(ERRejected);

};


/// \brief  Identify all nodes with the same node colour index
///
/// Nodes in the original graph with the same colour index are mapped to a single
/// node, and orginal arcs are mapped only if they connect different colour classes.

class colourContraction : public mixedGraph
{
public:

    colourContraction(abstractMixedGraph&,const TOption = 0) throw();

};


/// \brief  Surface graph of a given mixed graph and a nested family
///
/// This is used for the display of surface graphs which occur in the Edmonds'
/// spanning arborescence method.

class explicitSurfaceGraph : public mixedGraph
{
public:

    explicitSurfaceGraph(abstractMixedGraph&,nestedFamily<TNode>&,
            TFloat*,TArc*) throw();

};


/// \brief  Replacement of edge control points by graph nodes

class explicitSubdivision : public mixedGraph
{
public:

    explicitSubdivision(abstractMixedGraph&,const TOption = 0) throw();

};

/// @}

#endif
