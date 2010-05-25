
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, December 1998
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   denseDigraph.h
/// \brief  #denseDiGraph class interface

#if !defined(_DENSE_DIGRAPH_H_)

#define _DENSE_DIGRAPH_H_

#include "abstractDigraph.h"
#include "denseRepresentation.h"


/// \brief A class for directed graphs represented with arc capacity (adjacency) matrices

class denseDiGraph : public abstractDiGraph
{
friend class iDenseDiGraph;
protected:

    denseRepresentation X;

public:

    /// \brief  Default constructor for dense digraphs
    ///
    /// \param _n       The initial number of nodes
    /// \param options  Either 0 or OPT_COMPLETE
    /// \param _CT      The context to which this graph object is attached
    denseDiGraph(TNode _n,TOption options=0,
        goblinController& _CT=goblinDefaultContext) throw();

    /// \brief  File constructor for dense digraphs
    ///
    /// \param fileName  The source file name
    /// \param _CT       The context to which this graph object is attached
    denseDiGraph(const char* fileName,
        goblinController& _CT=goblinDefaultContext) throw(ERFile,ERParse);

    /// \brief  Copy constructor for dense digraphs
    ///
    /// \param G  The original digraph object
    denseDiGraph(abstractDiGraph& G) throw();
    ~denseDiGraph() throw();

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

/// \brief  Complete digraphs where arc length represent a shortest directed path
///
/// Distance graphs are complete digraphs which are obtained from a mixed graph
/// graph by setting the length labels according to the shortest directed path
/// lengths in the original mixed graph

class distanceGraph : public denseDiGraph
{
public:

    distanceGraph(abstractMixedGraph &G) throw(ERRejected);

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

/// \file   denseDigraph.h
/// \brief  #denseDiGraph class interface

#if !defined(_DENSE_DIGRAPH_H_)

#define _DENSE_DIGRAPH_H_

#include "abstractDigraph.h"
#include "denseRepresentation.h"


/// \brief A class for directed graphs represented with arc capacity (adjacency) matrices

class denseDiGraph : public abstractDiGraph
{
friend class iDenseDiGraph;
protected:

    denseRepresentation X;

public:

    /// \brief  Default constructor for dense digraphs
    ///
    /// \param _n       The initial number of nodes
    /// \param options  Either 0 or OPT_COMPLETE
    /// \param _CT      The context to which this graph object is attached
    denseDiGraph(TNode _n,TOption options=0,
        goblinController& _CT=goblinDefaultContext) throw();

    /// \brief  File constructor for dense digraphs
    ///
    /// \param fileName  The source file name
    /// \param _CT       The context to which this graph object is attached
    denseDiGraph(const char* fileName,
        goblinController& _CT=goblinDefaultContext) throw(ERFile,ERParse);

    /// \brief  Copy constructor for dense digraphs
    ///
    /// \param G  The original digraph object
    denseDiGraph(abstractDiGraph& G) throw();
    ~denseDiGraph() throw();

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

/// \brief  Complete digraphs where arc length represent a shortest directed path
///
/// Distance graphs are complete digraphs which are obtained from a mixed graph
/// graph by setting the length labels according to the shortest directed path
/// lengths in the original mixed graph

class distanceGraph : public denseDiGraph
{
public:

    distanceGraph(abstractMixedGraph &G) throw(ERRejected);

};

/// @}


#endif
