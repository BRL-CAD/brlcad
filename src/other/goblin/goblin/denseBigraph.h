
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, December 1998
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   denseBigraph.h
/// \brief  #denseBiGraph class interface

#if !defined(_DENSE_BIGRAPH_H_)

#define _DENSE_BIGRAPH_H_

#include "abstractBigraph.h"
#include "denseRepresentation.h"


/// \brief A class for bipartite graphs represented with arc capacity (adjacency) matrices

class denseBiGraph : public abstractBiGraph
{
    friend class iDenseBiGraph;

protected:

    denseRepresentation X;

public:

    /// \brief  Default constructor for dense bigraphs
    ///
    /// \param _n1      The initial number of left-hand nodes
    /// \param _n2      The initial number of right-hand nodes
    /// \param options  Either 0 or OPT_COMPLETE
    /// \param _CT      The context to which this graph object is attached
    denseBiGraph(TNode _n1,TNode _n2,TOption options=0,
        goblinController& _CT=goblinDefaultContext) throw();

    /// \brief  File constructor for dense bigraphs
    ///
    /// \param fileName  The source file name
    /// \param _CT       The context to which this graph object is attached
    denseBiGraph(const char* fileName,goblinController& _CT=goblinDefaultContext)
        throw(ERFile,ERParse);

    /// \brief  Copy constructor for dense bigraphs
    ///
    /// \param G  The original bigraph object
    denseBiGraph(abstractBiGraph& G) throw();

    ~denseBiGraph() throw();

    void            ReadNNodes(goblinImport& F) throw(ERParse);

    unsigned long   Size() const throw();
    unsigned long   Allocated() const throw();

    TArc            Adjacency(TNode u,TNode v,TMethAdjacency method = ADJ_MATRIX) const throw(ERRange);
    TNode           StartNode(TArc a) const throw(ERRange);
    TNode           EndNode(TArc a) const throw(ERRange);

    TArc            First(TNode v) const throw(ERRange);
    TArc            Right(TArc a,TNode v = NoNode) const throw(ERRange);

    #include <denseInclude.h>

};


#endif

//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, December 1998
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   denseBigraph.h
/// \brief  #denseBiGraph class interface

#if !defined(_DENSE_BIGRAPH_H_)

#define _DENSE_BIGRAPH_H_

#include "abstractBigraph.h"
#include "denseRepresentation.h"


/// \brief A class for bipartite graphs represented with arc capacity (adjacency) matrices

class denseBiGraph : public abstractBiGraph
{
    friend class iDenseBiGraph;

protected:

    denseRepresentation X;

public:

    /// \brief  Default constructor for dense bigraphs
    ///
    /// \param _n1      The initial number of left-hand nodes
    /// \param _n2      The initial number of right-hand nodes
    /// \param options  Either 0 or OPT_COMPLETE
    /// \param _CT      The context to which this graph object is attached
    denseBiGraph(TNode _n1,TNode _n2,TOption options=0,
        goblinController& _CT=goblinDefaultContext) throw();

    /// \brief  File constructor for dense bigraphs
    ///
    /// \param fileName  The source file name
    /// \param _CT       The context to which this graph object is attached
    denseBiGraph(const char* fileName,goblinController& _CT=goblinDefaultContext)
        throw(ERFile,ERParse);

    /// \brief  Copy constructor for dense bigraphs
    ///
    /// \param G  The original bigraph object
    denseBiGraph(abstractBiGraph& G) throw();

    ~denseBiGraph() throw();

    void            ReadNNodes(goblinImport& F) throw(ERParse);

    unsigned long   Size() const throw();
    unsigned long   Allocated() const throw();

    TArc            Adjacency(TNode u,TNode v,TMethAdjacency method = ADJ_MATRIX) const throw(ERRange);
    TNode           StartNode(TArc a) const throw(ERRange);
    TNode           EndNode(TArc a) const throw(ERRange);

    TArc            First(TNode v) const throw(ERRange);
    TArc            Right(TArc a,TNode v = NoNode) const throw(ERRange);

    #include <denseInclude.h>

};


#endif
