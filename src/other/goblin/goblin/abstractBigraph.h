
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, December 1998
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   abstractBigraph.h
/// \brief  #abstractBiGraph class interface

#ifndef _ABSTRACT_BIGRAPH_H_
#define _ABSTRACT_BIGRAPH_H_

#include "abstractGraph.h"


/// \brief The base class for all kinds of bipartite graph objects

class abstractBiGraph : public abstractGraph
{
protected:

    TNode   n1;     ///< Number of left-hand nodes
    TNode   n2;     ///< Number of right-hand nodes

public:

    abstractBiGraph(TNode _n1=0,TNode _n2=0,TArc _m=0) throw(ERRange);
    virtual ~abstractBiGraph() throw();

    unsigned long   Allocated() const throw();
    void            CheckLimits() throw(ERRange);

    /// \addtogroup classifications
    /// @{

    bool  IsBipartite() const throw();

    /// @}


    /// \addtogroup objectDimensions
    /// @{

    /// \brief  Query the number of left-hand graph nodes
    ///
    /// \return  The number of left-hand graph nodes
    inline TNode  N1() const throw();

    /// \brief  Query the number of right-hand graph nodes
    ///
    /// \return  The number of right-hand graph nodes
    inline TNode  N2() const throw();

    /// @}

    void  RandomArcs(TArc _m) throw(ERRejected);

    virtual void  ReadNNodes(goblinImport& F) throw(ERParse);

    bool  MaximumAssignment() throw();
    bool  MaximumAssignment(TCap cDeg) throw();
    bool  MaximumAssignment(TCap *pLower,TCap *pDeg = NULL) throw();

    bool  MinCAssignment() throw();
    bool  MinCAssignment(TCap cDeg) throw();
    bool  MinCAssignment(TCap *pLower,TCap *pDeg = NULL) throw();

private:

    bool  PMHeuristicsCandidates() throw();

public:

    bool  MaximumMatching() throw()
                        {return MaximumAssignment();};
    bool  MaximumMatching(TCap cDeg) throw()
                        {return MaximumAssignment(cDeg);};
    bool  MaximumMatching(TCap *pLower,TCap *pDeg = NULL) throw()
                        {return MaximumAssignment(pLower,pDeg);};

    bool  MinCMatching() throw()
                        {return MinCAssignment();};
    bool  MinCMatching(TCap cDeg) throw()
                        {return MinCAssignment(cDeg);};
    bool  MinCMatching(TCap *pLower,TCap *pDeg = NULL) throw()
                        {return MinCAssignment(pLower,pDeg);};

    TNode  StableSet() throw();
    TNode  NodeColouring(TNode k = NoNode) throw();

};


inline TNode abstractBiGraph::N1() const throw()
{
    return n1;
}


inline TNode abstractBiGraph::N2() const throw()
{
    return n2;
}


/// \addtogroup groupIndexSets
/// @{

/// \brief  Index sets representing all left-hand nodes of a bipartite graph
///
/// Objects specify all bigraph nodes 0,1,..,n1-1

class leftHandNodes : public virtual indexSet<TNode>
{
private:

    const abstractBiGraph& G;

public:

    leftHandNodes(const abstractBiGraph& _G) throw();
    ~leftHandNodes() throw();

    unsigned long  Size() const throw();

    bool  IsMember(const TNode i) const throw(ERRange);
    TNode  First() const throw();
    TNode  Successor(const TNode i) const throw(ERRange);

};


/// \brief  Index sets representing all right-hand nodes of a bipartite graph
///
/// Objects specify all bigraph nodes n1,n1+1,..,n-1

class rightHandNodes : public virtual indexSet<TNode>
{
private:

    const abstractBiGraph& G;

public:

    rightHandNodes(const abstractBiGraph& _G) throw();
    ~rightHandNodes() throw();

    unsigned long  Size() const throw();

    bool  IsMember(const TNode i) const throw(ERRange);
    TNode  First() const throw();
    TNode  Successor(const TNode i) const throw(ERRange);

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

/// \file   abstractBigraph.h
/// \brief  #abstractBiGraph class interface

#ifndef _ABSTRACT_BIGRAPH_H_
#define _ABSTRACT_BIGRAPH_H_

#include "abstractGraph.h"


/// \brief The base class for all kinds of bipartite graph objects

class abstractBiGraph : public abstractGraph
{
protected:

    TNode   n1;     ///< Number of left-hand nodes
    TNode   n2;     ///< Number of right-hand nodes

public:

    abstractBiGraph(TNode _n1=0,TNode _n2=0,TArc _m=0) throw(ERRange);
    virtual ~abstractBiGraph() throw();

    unsigned long   Allocated() const throw();
    void            CheckLimits() throw(ERRange);

    /// \addtogroup classifications
    /// @{

    bool  IsBipartite() const throw();

    /// @}


    /// \addtogroup objectDimensions
    /// @{

    /// \brief  Query the number of left-hand graph nodes
    ///
    /// \return  The number of left-hand graph nodes
    inline TNode  N1() const throw();

    /// \brief  Query the number of right-hand graph nodes
    ///
    /// \return  The number of right-hand graph nodes
    inline TNode  N2() const throw();

    /// @}

    void  RandomArcs(TArc _m) throw(ERRejected);

    virtual void  ReadNNodes(goblinImport& F) throw(ERParse);

    bool  MaximumAssignment() throw();
    bool  MaximumAssignment(TCap cDeg) throw();
    bool  MaximumAssignment(TCap *pLower,TCap *pDeg = NULL) throw();

    bool  MinCAssignment() throw();
    bool  MinCAssignment(TCap cDeg) throw();
    bool  MinCAssignment(TCap *pLower,TCap *pDeg = NULL) throw();

private:

    bool  PMHeuristicsCandidates() throw();

public:

    bool  MaximumMatching() throw()
                        {return MaximumAssignment();};
    bool  MaximumMatching(TCap cDeg) throw()
                        {return MaximumAssignment(cDeg);};
    bool  MaximumMatching(TCap *pLower,TCap *pDeg = NULL) throw()
                        {return MaximumAssignment(pLower,pDeg);};

    bool  MinCMatching() throw()
                        {return MinCAssignment();};
    bool  MinCMatching(TCap cDeg) throw()
                        {return MinCAssignment(cDeg);};
    bool  MinCMatching(TCap *pLower,TCap *pDeg = NULL) throw()
                        {return MinCAssignment(pLower,pDeg);};

    TNode  StableSet() throw();
    TNode  NodeColouring(TNode k = NoNode) throw();

};


inline TNode abstractBiGraph::N1() const throw()
{
    return n1;
}


inline TNode abstractBiGraph::N2() const throw()
{
    return n2;
}


/// \addtogroup groupIndexSets
/// @{

/// \brief  Index sets representing all left-hand nodes of a bipartite graph
///
/// Objects specify all bigraph nodes 0,1,..,n1-1

class leftHandNodes : public virtual indexSet<TNode>
{
private:

    const abstractBiGraph& G;

public:

    leftHandNodes(const abstractBiGraph& _G) throw();
    ~leftHandNodes() throw();

    unsigned long  Size() const throw();

    bool  IsMember(const TNode i) const throw(ERRange);
    TNode  First() const throw();
    TNode  Successor(const TNode i) const throw(ERRange);

};


/// \brief  Index sets representing all right-hand nodes of a bipartite graph
///
/// Objects specify all bigraph nodes n1,n1+1,..,n-1

class rightHandNodes : public virtual indexSet<TNode>
{
private:

    const abstractBiGraph& G;

public:

    rightHandNodes(const abstractBiGraph& _G) throw();
    ~rightHandNodes() throw();

    unsigned long  Size() const throw();

    bool  IsMember(const TNode i) const throw(ERRange);
    TNode  First() const throw();
    TNode  Successor(const TNode i) const throw(ERRange);

};

/// @}


#endif
