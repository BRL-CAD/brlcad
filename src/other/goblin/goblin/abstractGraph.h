
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, December 1998
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   abstractGraph.h
/// \brief  #abstractGraph class interface

#ifndef _ABSTRACT_GRAPH_H_
#define _ABSTRACT_GRAPH_H_

#include "abstractMixedGraph.h"


/// \brief The base class for all kinds of undirected graph objects

class abstractGraph : public abstractMixedGraph
{
friend class abstractMixedGraph;
friend class branchSymmTSP;

public:

    abstractGraph(TNode _n=0,TArc _m=0) throw();
    virtual ~abstractGraph() throw();

    unsigned long   Allocated() const throw();


    /// \addtogroup classifications
    /// @{

    bool  IsUndirected() const throw();

    /// @}



    /// \addtogroup steiner
    /// @{

protected:

    /// \brief  Compute a Steiner tree by the Mehlhorn heuristic
    ///
    /// \param Terminals  An index set of terminal nodes in the range [0,1,..,n]
    /// \param root       A node in the range [0,1,..,n]
    /// \return           The length of the found Steiner tree
    ///
    /// This uses a discrete Voronoi diagram which is obtained by simultaneous
    /// search for shortest path trees from all terminal nodes. The found partial
    /// trees are shrunk, and on the resulting graph a minimum spanning tree is
    /// computed. This tree maps back to a Steiner tree in the original graph.
    ///
    /// The procedure requires non-negative edge length. It is 1-approximative,
    /// that is, the constructed tree is twice as long as an optimal Steiner tree
    /// in the worst case.
    TFloat  STT_Heuristic(const indexSet<TNode>& Terminals,TNode root) throw(ERRange);

    // Documented in the class abstractMixedGraph
    TFloat  STT_Enumerate(const indexSet<TNode>& Terminals,TNode root) throw(ERRange);

    /// @}


    /// \addtogroup tsp
    /// @{

protected:

    // Documented in the class abstractMixedGraph
    virtual TFloat  TSP_Heuristic(THeurTSP method,TNode root) throw(ERRange,ERRejected);

    /// \brief  Compute a tour by the Christofides Heuristic
    ///
    /// \param r  The tree root node or NoNode
    /// \return   The length of the constructed tour
    ///
    /// This works similar as TSP_HeuristicTree() in the sense that a spanning
    /// tree is extended to an Eulerian graph. Here, a minimum 1-matching of
    /// the nodes with odd degree in the tree are added. From the resulting graph,
    /// an arbitrary Eulerian walk is determined. This is reduced to a Hamiltonian
    /// cycle by travelling around the Eulerian walk and skipping all nodes which
    /// have already been visited.
    ///
    /// The procedure is intended for metric undirected graphs, and then is
    /// 1/2-approximative, that is, the constructed tour is 3/2 times as long as an
    /// optimal tour in the worst case. The procedure works for all other kinds
    /// of complete graphs, but does not produce good tours in the general setting.
    ///
    /// If no root node is specified, the procedure is repeated for all possible
    /// root nodes.
    ///
    /// If CT.methLocal==LOCAL_OPTIMIZE, TSP_LocalSearch() is called for postprocessing.
    TFloat  TSP_HeuristicChristofides(TNode r=NoNode) throw(ERRange);

    /// \brief  Refine a given tour by a 2 arc exchange step
    ///
    /// \param pred   An array of predecessor arcs, describing an Hamiltonian cycle
    /// \param limit  The treshold for the acceptable change of tour length
    /// \retval true  If the tour has been changed
    ///
    /// This takes a given tour and searches for a pair of edges a1=uv, a2=xy which
    /// can be profitably replaced by the edges ux and vy. At most one replacement
    /// is done!
    bool  TSP_2Exchange(TArc* pred,TFloat limit=0) throw(ERRejected);

    // Documented in the class abstractMixedGraph
    TFloat  TSP_SubOpt1Tree(TRelaxTSP method,TNode root,TFloat& bestUpper,
                        bool branchAndBound) throw(ERRange);

    // Documented in the class abstractMixedGraph
    TFloat  TSP_BranchAndBound(TRelaxTSP method,int nCandidates,
                        TNode root,TFloat upperBound) throw();

    /// @}


    /// \addtogroup matching
    /// @{

public:

    /// \brief  Determine a maximum b-matching or f-factor
    ///
    /// \retval true  A perfect matching has been found
    ///
    /// The desired node degrees are defined implicitly by the Demand() values.
    virtual bool  MaximumMatching() throw();

    /// \brief  Determine a maximum k-factor
    ///
    /// \param cDeg   The desired constant node degree
    /// \retval true  A perfect matching has been found
    virtual bool  MaximumMatching(TCap cDeg) throw();

    /// \brief  Determine a minimum defect (b,c)-matching
    ///
    /// \param pLower  A pointer to an array of lower degree bounds
    /// \param pDeg    A pointer to an array of upper degree bounds or NULL
    /// \retval true   A perfect matching has been found
    virtual bool  MaximumMatching(TCap *pLower,TCap *pDeg = NULL) throw();

    /// \brief  Determine a minimum-cost perfect b-matching or f-factor
    ///
    /// \retval true  A perfect matching has been found
    ///
    /// The desired node degrees are defined implicitly by the Demand() values.
    /// Other than the overloaded methods which take explicit degree information,
    /// this allows for the candidate graph heuristic if the graph is dense and
    /// if methCandidates >= 0.
    ///
    virtual bool  MinCMatching() throw();

    /// \brief  Determine a minimum-cost perfect k-factor
    ///
    /// \param cDeg   The desired constant node degree
    /// \retval true  A perfect matching has been found
    virtual bool  MinCMatching(TCap cDeg) throw();

    /// \brief  Determine a minimum-cost perfect (b,c)-matching
    ///
    /// \param pLower  A pointer to an array of lower degree bounds
    /// \param pDeg    A pointer to an array of upper degree bounds or NULL
    /// \retval true   A perfect matching has been found
    virtual bool  MinCMatching(TCap *pLower,TCap *pDeg = NULL) throw();

private:

    /// \brief  Determine a minimum-cost perfect (b,c)-matching by a candidate heuristic
    ///
    /// \retval true   A perfect matching has been found
    ///
    /// This calls PMHeuristicsRandom() several times and takes the join of the
    /// found matchings as a candidate graph. If methSolve = 0, an optimal matching
    /// of the candidate graph is provided. Otherwise, a min-cost flow on the
    /// candidate graph is used as the starting solution on the entire graph.
    bool  PMHeuristicsCandidates() throw();

protected:

    /// \brief  Determine a minimum-cost perfect (b,c)-matching by a random heuristic
    ///
    /// \return  The weight of the constructed matching
    ///
    /// This step-by-step chooses random nodes and add adjacent arcs increasing
    /// by their lenght.
    TFloat  PMHeuristicsRandom() throw();

public:

    /// \brief  Provide a minimum length edge cover by the edge colour register
    ///
    /// \return  The total edge length
    TFloat  MinCEdgeCover() throw(ERRejected);


protected:

    /// \brief  Compute a T-join for a given node set T
    ///
    /// \param Terminals  A set of terminal nodes. Must have even cardinality
    ///
    /// This is the working procedure for all kinds of T-join solvers.
    /// It requires non-negative edge lengths.
    void  ComputeTJoin(const indexSet<TNode>& Terminals) throw(ERRejected);

public:

    /// \brief  Compute a T-join for a given node set T
    ///
    /// \param Terminals  A set of terminal nodes. Must have even cardinality
    ///
    /// Other than ComputeTJoin(), this can deal with negative length arcs.
    void  MinCTJoin(const indexSet<TNode>& Terminals) throw();

    /// \brief  Compute a shortest path connecting a given node pair
    ///
    /// \param s      The start node of the path to be generated
    /// \param t      The end node of the path to be generated
    /// \retval true  A shortest st-path has been found
    ///
    /// Other than general shortest path methods applied to undirected graphs,
    /// this can deal with negative length arcs. The found path is provided by
    /// the subgraph labels.
    bool  SPX_TJoin(TNode s,TNode t) throw(ERRange);

    /// @}


    /// \addtogroup eulerPostman
    /// @{

public:

    // Documented in the class abstractMixedGraph
    void  ChinesePostman(bool adjustUCap) throw(ERRejected);

    /// @}


    /// \addtogroup maxCut
    /// @{

protected:

    /// \brief  Approximate maximum weight edge cut
    ///
    /// \param method  A #TMethMaxCut value specifying the applied method
    /// \param s       A left-hand node index or NoNode
    /// \param t       A right-hand node index or NoNode
    /// \return        The weight of the constructed edge cut
    ///
    /// Depending on methMaxCut, this either calls MXC_HeuristicGRASP(),
    /// MXC_HeuristicTree() or MXC_DualTJoin().
    TFloat MXC_Heuristic(TMethMaxCut method,TNode s = NoNode,TNode t = NoNode)
                throw(ERRange,ERRejected);

    /// \brief  Approximate maximum weight edge cut by using a maximum spanning tree
    ///
    /// \param s  A left-hand node index or NoNode
    /// \param t  A right-hand node index or NoNode
    /// \return   The weight of the constructed edge cut
    ///
    /// This sets the bipartition according to a maximum weight spanning tree
    /// and the root node s. In particular, the graph must be connected.
    /// If methLocal==LOCAL_OPTIMIZE, MXC_LocalSearch() is called for postprocessing.
    TFloat MXC_HeuristicTree(TNode s = NoNode,TNode t = NoNode) throw(ERRange,ERRejected);

    /// \brief  Exact solution of the undirected planar maximum weight edge cut problem
    ///
    /// \param s  A left-hand node index or NoNode
    /// \return   The weight of the constructed edge cut
    ///
    /// This determines a T-join in the undirected dual graph.
    ///
    /// The procedure is restricted to non-negative edge lengths!
    TFloat MXC_DualTJoin(TNode s = NoNode) throw(ERRange,ERRejected);

    /// @}

};


#endif


//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, December 1998
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   abstractGraph.h
/// \brief  #abstractGraph class interface

#ifndef _ABSTRACT_GRAPH_H_
#define _ABSTRACT_GRAPH_H_

#include "abstractMixedGraph.h"


/// \brief The base class for all kinds of undirected graph objects

class abstractGraph : public abstractMixedGraph
{
friend class abstractMixedGraph;
friend class branchSymmTSP;

public:

    abstractGraph(TNode _n=0,TArc _m=0) throw();
    virtual ~abstractGraph() throw();

    unsigned long   Allocated() const throw();


    /// \addtogroup classifications
    /// @{

    bool  IsUndirected() const throw();

    /// @}



    /// \addtogroup steiner
    /// @{

protected:

    /// \brief  Compute a Steiner tree by the Mehlhorn heuristic
    ///
    /// \param Terminals  An index set of terminal nodes in the range [0,1,..,n]
    /// \param root       A node in the range [0,1,..,n]
    /// \return           The length of the found Steiner tree
    ///
    /// This uses a discrete Voronoi diagram which is obtained by simultaneous
    /// search for shortest path trees from all terminal nodes. The found partial
    /// trees are shrunk, and on the resulting graph a minimum spanning tree is
    /// computed. This tree maps back to a Steiner tree in the original graph.
    ///
    /// The procedure requires non-negative edge length. It is 1-approximative,
    /// that is, the constructed tree is twice as long as an optimal Steiner tree
    /// in the worst case.
    TFloat  STT_Heuristic(const indexSet<TNode>& Terminals,TNode root) throw(ERRange);

    // Documented in the class abstractMixedGraph
    TFloat  STT_Enumerate(const indexSet<TNode>& Terminals,TNode root) throw(ERRange);

    /// @}


    /// \addtogroup tsp
    /// @{

protected:

    // Documented in the class abstractMixedGraph
    virtual TFloat  TSP_Heuristic(THeurTSP method,TNode root) throw(ERRange,ERRejected);

    /// \brief  Compute a tour by the Christofides Heuristic
    ///
    /// \param r  The tree root node or NoNode
    /// \return   The length of the constructed tour
    ///
    /// This works similar as TSP_HeuristicTree() in the sense that a spanning
    /// tree is extended to an Eulerian graph. Here, a minimum 1-matching of
    /// the nodes with odd degree in the tree are added. From the resulting graph,
    /// an arbitrary Eulerian walk is determined. This is reduced to a Hamiltonian
    /// cycle by travelling around the Eulerian walk and skipping all nodes which
    /// have already been visited.
    ///
    /// The procedure is intended for metric undirected graphs, and then is
    /// 1/2-approximative, that is, the constructed tour is 3/2 times as long as an
    /// optimal tour in the worst case. The procedure works for all other kinds
    /// of complete graphs, but does not produce good tours in the general setting.
    ///
    /// If no root node is specified, the procedure is repeated for all possible
    /// root nodes.
    ///
    /// If CT.methLocal==LOCAL_OPTIMIZE, TSP_LocalSearch() is called for postprocessing.
    TFloat  TSP_HeuristicChristofides(TNode r=NoNode) throw(ERRange);

    /// \brief  Refine a given tour by a 2 arc exchange step
    ///
    /// \param pred   An array of predecessor arcs, describing an Hamiltonian cycle
    /// \param limit  The treshold for the acceptable change of tour length
    /// \retval true  If the tour has been changed
    ///
    /// This takes a given tour and searches for a pair of edges a1=uv, a2=xy which
    /// can be profitably replaced by the edges ux and vy. At most one replacement
    /// is done!
    bool  TSP_2Exchange(TArc* pred,TFloat limit=0) throw(ERRejected);

    // Documented in the class abstractMixedGraph
    TFloat  TSP_SubOpt1Tree(TRelaxTSP method,TNode root,TFloat& bestUpper,
                        bool branchAndBound) throw(ERRange);

    // Documented in the class abstractMixedGraph
    TFloat  TSP_BranchAndBound(TRelaxTSP method,int nCandidates,
                        TNode root,TFloat upperBound) throw();

    /// @}


    /// \addtogroup matching
    /// @{

public:

    /// \brief  Determine a maximum b-matching or f-factor
    ///
    /// \retval true  A perfect matching has been found
    ///
    /// The desired node degrees are defined implicitly by the Demand() values.
    virtual bool  MaximumMatching() throw();

    /// \brief  Determine a maximum k-factor
    ///
    /// \param cDeg   The desired constant node degree
    /// \retval true  A perfect matching has been found
    virtual bool  MaximumMatching(TCap cDeg) throw();

    /// \brief  Determine a minimum defect (b,c)-matching
    ///
    /// \param pLower  A pointer to an array of lower degree bounds
    /// \param pDeg    A pointer to an array of upper degree bounds or NULL
    /// \retval true   A perfect matching has been found
    virtual bool  MaximumMatching(TCap *pLower,TCap *pDeg = NULL) throw();

    /// \brief  Determine a minimum-cost perfect b-matching or f-factor
    ///
    /// \retval true  A perfect matching has been found
    ///
    /// The desired node degrees are defined implicitly by the Demand() values.
    /// Other than the overloaded methods which take explicit degree information,
    /// this allows for the candidate graph heuristic if the graph is dense and
    /// if methCandidates >= 0.
    ///
    virtual bool  MinCMatching() throw();

    /// \brief  Determine a minimum-cost perfect k-factor
    ///
    /// \param cDeg   The desired constant node degree
    /// \retval true  A perfect matching has been found
    virtual bool  MinCMatching(TCap cDeg) throw();

    /// \brief  Determine a minimum-cost perfect (b,c)-matching
    ///
    /// \param pLower  A pointer to an array of lower degree bounds
    /// \param pDeg    A pointer to an array of upper degree bounds or NULL
    /// \retval true   A perfect matching has been found
    virtual bool  MinCMatching(TCap *pLower,TCap *pDeg = NULL) throw();

private:

    /// \brief  Determine a minimum-cost perfect (b,c)-matching by a candidate heuristic
    ///
    /// \retval true   A perfect matching has been found
    ///
    /// This calls PMHeuristicsRandom() several times and takes the join of the
    /// found matchings as a candidate graph. If methSolve = 0, an optimal matching
    /// of the candidate graph is provided. Otherwise, a min-cost flow on the
    /// candidate graph is used as the starting solution on the entire graph.
    bool  PMHeuristicsCandidates() throw();

protected:

    /// \brief  Determine a minimum-cost perfect (b,c)-matching by a random heuristic
    ///
    /// \return  The weight of the constructed matching
    ///
    /// This step-by-step chooses random nodes and add adjacent arcs increasing
    /// by their lenght.
    TFloat  PMHeuristicsRandom() throw();

public:

    /// \brief  Provide a minimum length edge cover by the edge colour register
    ///
    /// \return  The total edge length
    TFloat  MinCEdgeCover() throw(ERRejected);


protected:

    /// \brief  Compute a T-join for a given node set T
    ///
    /// \param Terminals  A set of terminal nodes. Must have even cardinality
    ///
    /// This is the working procedure for all kinds of T-join solvers.
    /// It requires non-negative edge lengths.
    void  ComputeTJoin(const indexSet<TNode>& Terminals) throw(ERRejected);

public:

    /// \brief  Compute a T-join for a given node set T
    ///
    /// \param Terminals  A set of terminal nodes. Must have even cardinality
    ///
    /// Other than ComputeTJoin(), this can deal with negative length arcs.
    void  MinCTJoin(const indexSet<TNode>& Terminals) throw();

    /// \brief  Compute a shortest path connecting a given node pair
    ///
    /// \param s      The start node of the path to be generated
    /// \param t      The end node of the path to be generated
    /// \retval true  A shortest st-path has been found
    ///
    /// Other than general shortest path methods applied to undirected graphs,
    /// this can deal with negative length arcs. The found path is provided by
    /// the subgraph labels.
    bool  SPX_TJoin(TNode s,TNode t) throw(ERRange);

    /// @}


    /// \addtogroup eulerPostman
    /// @{

public:

    // Documented in the class abstractMixedGraph
    void  ChinesePostman(bool adjustUCap) throw(ERRejected);

    /// @}


    /// \addtogroup maxCut
    /// @{

protected:

    /// \brief  Approximate maximum weight edge cut
    ///
    /// \param method  A #TMethMaxCut value specifying the applied method
    /// \param s       A left-hand node index or NoNode
    /// \param t       A right-hand node index or NoNode
    /// \return        The weight of the constructed edge cut
    ///
    /// Depending on methMaxCut, this either calls MXC_HeuristicGRASP(),
    /// MXC_HeuristicTree() or MXC_DualTJoin().
    TFloat MXC_Heuristic(TMethMaxCut method,TNode s = NoNode,TNode t = NoNode)
                throw(ERRange,ERRejected);

    /// \brief  Approximate maximum weight edge cut by using a maximum spanning tree
    ///
    /// \param s  A left-hand node index or NoNode
    /// \param t  A right-hand node index or NoNode
    /// \return   The weight of the constructed edge cut
    ///
    /// This sets the bipartition according to a maximum weight spanning tree
    /// and the root node s. In particular, the graph must be connected.
    /// If methLocal==LOCAL_OPTIMIZE, MXC_LocalSearch() is called for postprocessing.
    TFloat MXC_HeuristicTree(TNode s = NoNode,TNode t = NoNode) throw(ERRange,ERRejected);

    /// \brief  Exact solution of the undirected planar maximum weight edge cut problem
    ///
    /// \param s  A left-hand node index or NoNode
    /// \return   The weight of the constructed edge cut
    ///
    /// This determines a T-join in the undirected dual graph.
    ///
    /// The procedure is restricted to non-negative edge lengths!
    TFloat MXC_DualTJoin(TNode s = NoNode) throw(ERRange,ERRejected);

    /// @}

};


#endif

