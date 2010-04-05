
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, December 1998
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   abstractDigraph.h
/// \brief  #abstractDiGraph class interface

#ifndef _ABSTRACT_DIGRAPH_H_
#define _ABSTRACT_DIGRAPH_H_

#include "abstractMixedGraph.h"


/// \brief The base class for all kinds of directed graph objects

class abstractDiGraph : public abstractMixedGraph
{
friend class abstractMixedGraph;  // For the max-flow methods

public:

    abstractDiGraph(TNode _n=0,TArc _m=0) throw();
    virtual ~abstractDiGraph() throw();

    unsigned long  Allocated() const throw();

    bool  IsDirected() const throw()
    {
        return true;
    };

    TFloat  NodeCap(TNode v) throw(ERRange)
    {
        if (DegIn(v)<DegOut(v)) return DegIn(v); else return DegOut(v);
    };


    /// \addtogroup flowLowLevel
    /// @{

    /// \brief  Return a flow value
    ///
    /// \param a  An arc index
    /// \return   The flow on this arc (the subgraph multiplicity)
    virtual TFloat  Flow(TArc a) const throw(ERRange);

    /// \brief  Increase or decrease the flow value of the arc a by an amount Lambda
    ///
    /// \param a       An arc index
    /// \param lambda  An amount of flow
    virtual void  Push(TArc a,TFloat lambda) throw(ERRange,ERRejected);

    /// \brief  Update the degree labels after a push operation
    ///
    /// \param a       An arc index
    /// \param lambda  An amount of flow
    void  AdjustDivergence(TArc a,TFloat lambda) throw(ERRange,ERRejected);

    /// \brief  Compute the residual path capacity
    ///
    /// \param pred[]  An array of predecessor arcs assigned with the graph nodes
    /// \param source  The path start node
    /// \param target  The path end node
    /// \return        The minimal residual capacity on the described path
    TFloat  FindCap(TArc* pred,TNode source,TNode target) throw(ERRange,ERRejected);

    /// \brief  Augment along a given path
    ///
    /// \param pred[]  An array of predecessor arcs assigned with the graph nodes
    /// \param source  The path start node
    /// \param target  The path end node
    /// \param lambda  An amount of flow
    void  Augment(TArc* pred,TNode source,TNode target,TFloat lambda) throw(ERRange,ERRejected);

    /// \brief  Checks wether the flow labels define an st-flow and, if so, returns the flow value
    ///
    /// \param source  The path start node
    /// \param target  The path end node
    /// \return        The flow value, that is the divergence at the target node
    TFloat  FlowValue(TNode source,TNode target) throw(ERRange,ERCheck);

    /// \brief  Complementary slackness optimality test
    ///
    /// \retval true  Both the flow and the node potentials are optimal
    ///
    /// Checks wether the flow labels and the node potentials satisfy the
    /// complementary slackness conditions, that is, for every arc, either
    /// the residual capacity is zero, or the reduced length is non-negative.
    ///
    /// This procedure does not verify that the flow is feasible!
    bool  Compatible() throw();

    /// \brief  Computes a min-cost flow "dual" objective value from the node potentials
    ///
    /// \return  The dual objective value
    ///
    /// This applies to arbitrary node potentials and gives a lower bound on the minimal flow objective
    TFloat  MCF_DualObjective() throw();

    /// @}


    /// \addtogroup treePacking
    /// @{

    /// \brief  Determine a maximum arborescence packing
    ///
    /// \param root  The root node index in the range [0,1,..,n-1]
    ///
    /// If the arc capacity bounds are all one, this determines a maximum number
    /// of disjoint spanning arborescences rooted at r, and this number matches
    /// the minimum cut capacity returned by StrongEdgeConnectivity(r).
    ///
    /// In the general setting with non-trivial arc capacities, the constructed
    /// arborescences are not edge disjoint and hence cannot be completely saved
    /// to the edge colours. The particular spanning trees can be obtained either
    /// by the predecessor labels of the trace objects (set traceLevel>=3), or by
    /// calling TreePKGInit() and TreePKGStripTree() directly (and then using the
    /// predecessor trees of the manipulated digraph).
    TCap  TreePacking(TNode root) throw(ERRange);

    /// \brief  Generate a digraph copy to be manipulated during the tree packing
    /// \brief  method and determine the total tree multiplicity
    ///
    /// \param totalMulti  The multiplicity of all arborescences to be determined
    /// \param root        The root node index in range [0,1,..,n-1]
    /// \return            Pointer to a digraph to be manipulated by TreePKGStripTree()
    abstractDiGraph*  TreePKGInit(TCap* totalMulti,TNode root) throw();

    /// \brief  Reduce the arc capacities in the digraph copy according to the
    /// \brief  current arborescence and construct another arborescence if one exists
    ///
    /// \param G      Pointer to the digraph to be manipulated
    /// \param multi  The remaining multiplicity
    /// \param root   The root node index in range [0,1,..,n-1]
    /// \return       The capacity of the newly found arborescence
    TCap  TreePKGStripTree(abstractDiGraph* G,TCap* multi,TNode root) throw(ERRange);

    /// @}


    /// \addtogroup minimumMeanCycle
    /// @{

    /// \brief  Compute a minimum mean length value cycle
    ///
    /// \param EligibleArcs    The set of eligible arcs
    /// \param meanValue       A pointer to where the mean length value is written
    /// \return                The index of a node on the found cycle or NoNode
    ///
    /// This determines a cycle such that mean length value of the traversed
    /// arcs is minimal, exports this cycle by the predecessor labels. If the
    /// digraph is acyclic, NoNode is returned.
    TNode  MinimumMeanCycle(const indexSet<TArc>& EligibleArcs,TFloat* meanValue) throw();

    /// @}


    /// \addtogroup groupDirectedAcyclic
    /// @{

    /// \brief  Determine a topological numbering
    ///
    /// \return  A node on a directed cycle, or NoNode if the graph is a DAG
    ///
    /// The ordinal numbers are provided by the node colour register
    TNode  TopSort() throw();

    /// \brief  Determine a critical path
    ///
    /// \return  The end node of the critical path, or NoNode if the graph is not a DAG
    TNode  CriticalPath() throw();

    /// @}


    /// \addtogroup maximumFlow
    /// @{

private:

    TFloat  MXF_EdmondsKarp(TNode,TNode) throw(ERRange);
    TFloat  MXF_CapacityScaling(TNode,TNode) throw(ERRange);
    TFloat  MXF_PushRelabel(TMethMXF,TNode,TNode) throw(ERRange);
    TFloat  MXF_Dinic(TNode,TNode) throw(ERRange);

    /// @}


    /// \addtogroup groupMinCut
    /// @{

private:

    TCap  MCC_HaoOrlin(TMethMCC method,TNode r) throw(ERRange);

    /// @}


    /// \addtogroup minCostFlow
    /// @{

private:

    TFloat  MCF_BusackerGowen(TNode,TNode) throw(ERRejected);
    TFloat  MCF_EdmondsKarp(TNode,TNode) throw(ERRejected);

    TFloat  MCF_CycleCanceling() throw();
    TFloat  MCF_MinMeanCycleCanceling() throw();
    TFloat  MCF_CostScaling(TMethMCF) throw();
    TFloat  MCF_NWSimplex() throw();
    TFloat  MCF_CapacityScaling(bool = true) throw(ERRejected);
    TFloat  MCF_ShortestAugmentingPath() throw(ERRejected)
                        {return MCF_CapacityScaling(false);};

    void  MCF_NWSimplexCancelFree() throw();
    void  MCF_NWSimplexStrongTree() throw();

    /// @}


    /// \addtogroup eulerPostman
    /// @{

public:

    // Documented in the class abstractMixedGraph
    void  ChinesePostman(bool adjustUCap) throw(ERRejected);

    /// @}

};


/// \addtogroup groupIndexSets
/// @{

/// \brief  Index sets representing all supersaturated nodes
///
/// Objects specify all graph nodes with Divergence() > Demand() + delta
/// where the latter is the capacity scaling parameter

class supersaturatedNodes : public virtual indexSet<TNode>
{
private:

    const abstractDiGraph& G;
    const TCap delta;

public:

    supersaturatedNodes(const abstractDiGraph& _G,TCap _delta) throw();
    ~supersaturatedNodes() throw();

    unsigned long Size() const throw();

    bool IsMember(const TNode i) const throw(ERRange);

};


/// \brief  Index sets representing all deficient nodes
///
/// Objects specify all graph nodes with Divergence() < Demand() - delta
/// where the latter is the capacity scaling parameter

class deficientNodes : public virtual indexSet<TNode>
{
private:

    const abstractDiGraph& G;
    const TCap delta;

public:

    deficientNodes(const abstractDiGraph& _G,TCap _delta) throw();
    ~deficientNodes() throw();

    unsigned long Size() const throw();

    bool IsMember(const TNode i) const throw(ERRange);

};


/// \brief Index sets representing residual arcs

class residualArcs : public indexSet<TArc>
{
private:

    const abstractMixedGraph& G;
    TCap delta;

public:

    residualArcs(const abstractMixedGraph& _G,TCap _delta=0.0) throw();
    ~residualArcs() throw();

    unsigned long Size() const throw();

    bool IsMember(const TArc i) const throw(ERRange);

};

/// @}


#endif
