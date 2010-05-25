
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, December 1998
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   denseGraph.h
/// \brief  #denseGraph class interface

#if !defined(_DENSE_GRAPH_H_)

#define _DENSE_GRAPH_H_

#include "abstractGraph.h"
#include "denseRepresentation.h"


/// \brief A class for undirected graphs represented with arc capacity (adjacency) matrices

class denseGraph : public abstractGraph
{
    friend class iDenseGraph;

protected:

    denseRepresentation X;

public:

    /// \brief  Default constructor for dense undirected graphs
    ///
    /// \param _n       The initial number of nodes
    /// \param options  Either 0 or OPT_COMPLETE
    /// \param _CT      The context to which this graph object is attached
    denseGraph(TNode _n,TOption options=0,
        goblinController& _CT=goblinDefaultContext) throw();

    /// \brief  File constructor for dense undirected graphs
    ///
    /// \param fileName  The source file name
    /// \param _CT       The context to which this graph object is attached
    denseGraph(const char* fileName,goblinController& _CT=goblinDefaultContext)
        throw(ERFile,ERParse);

    /// \brief  Copy constructor for dense undirected graphs
    ///
    /// \param G  The original graph object
    denseGraph(abstractGraph& G) throw();

    ~denseGraph() throw();

    void            ReadNNodes(goblinImport& F) throw(ERParse);

    unsigned long   Size() const throw();
    unsigned long   Allocated() const throw();

    TArc            Adjacency(TNode u,TNode v,TMethAdjacency method = ADJ_MATRIX) const throw(ERRange);
    TNode           StartNode(TArc a) const throw(ERRange);
    TNode           EndNode(TArc a) const throw(ERRange);

    TArc            First(TNode v) const throw(ERRange);
    TArc            Right(TArc a,TNode v = NoNode) const throw(ERRange);

    #include <denseInclude.h>

    /// \addtogroup tsp
    /// @{

protected:

    // Documented in the class abstractMixedGraph
    TFloat  TSP_Heuristic(THeurTSP method,TNode root) throw(ERRange,ERRejected);

    /// @}

};


/// \addtogroup groupDistanceGraphs
/// @{

/// \brief  Complete graphs where edge length represent a shortest path
///
/// Metric graphs are complete graphs which respect the triangle inequality
/// d(uw) <= d(uv) + d(vw). They are obtained from another (possibly sparse)
/// graph by setting the length labels according to the shortest path lengths
/// in the original graph

class metricGraph : public denseGraph
{
public:

    metricGraph(abstractGraph &G) throw(ERRejected);

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

/// \file   denseGraph.h
/// \brief  #denseGraph class interface

#if !defined(_DENSE_GRAPH_H_)

#define _DENSE_GRAPH_H_

#include "abstractGraph.h"
#include "denseRepresentation.h"


/// \brief A class for undirected graphs represented with arc capacity (adjacency) matrices

class denseGraph : public abstractGraph
{
    friend class iDenseGraph;

protected:

    denseRepresentation X;

public:

    /// \brief  Default constructor for dense undirected graphs
    ///
    /// \param _n       The initial number of nodes
    /// \param options  Either 0 or OPT_COMPLETE
    /// \param _CT      The context to which this graph object is attached
    denseGraph(TNode _n,TOption options=0,
        goblinController& _CT=goblinDefaultContext) throw();

    /// \brief  File constructor for dense undirected graphs
    ///
    /// \param fileName  The source file name
    /// \param _CT       The context to which this graph object is attached
    denseGraph(const char* fileName,goblinController& _CT=goblinDefaultContext)
        throw(ERFile,ERParse);

    /// \brief  Copy constructor for dense undirected graphs
    ///
    /// \param G  The original graph object
    denseGraph(abstractGraph& G) throw();

    ~denseGraph() throw();

    void            ReadNNodes(goblinImport& F) throw(ERParse);

    unsigned long   Size() const throw();
    unsigned long   Allocated() const throw();

    TArc            Adjacency(TNode u,TNode v,TMethAdjacency method = ADJ_MATRIX) const throw(ERRange);
    TNode           StartNode(TArc a) const throw(ERRange);
    TNode           EndNode(TArc a) const throw(ERRange);

    TArc            First(TNode v) const throw(ERRange);
    TArc            Right(TArc a,TNode v = NoNode) const throw(ERRange);

    #include <denseInclude.h>

    /// \addtogroup tsp
    /// @{

protected:

    // Documented in the class abstractMixedGraph
    TFloat  TSP_Heuristic(THeurTSP method,TNode root) throw(ERRange,ERRejected);

    /// @}

};


/// \addtogroup groupDistanceGraphs
/// @{

/// \brief  Complete graphs where edge length represent a shortest path
///
/// Metric graphs are complete graphs which respect the triangle inequality
/// d(uw) <= d(uv) + d(vw). They are obtained from another (possibly sparse)
/// graph by setting the length labels according to the shortest path lengths
/// in the original graph

class metricGraph : public denseGraph
{
public:

    metricGraph(abstractGraph &G) throw(ERRejected);

};

/// @}

#endif
