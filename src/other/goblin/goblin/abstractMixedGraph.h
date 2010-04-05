
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, December 1999
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   abstractMixedGraph.h
/// \brief  #abstractMixedGraph class interface

#ifndef _ABSTRACT_MIXED_GRAPH_H_
#define _ABSTRACT_MIXED_GRAPH_H_

#include "investigator.h"
#include "goblinQueue.h"
#include "disjointFamily.h"
#include "hashTable.h"
#include "indexSet.h"

#include "attributePool.h"

class graphRepresentation;


/// \brief The base class for all kinds of graph objects
///
/// This class declares the interfaces to any graph representation, layout data
/// and registers (which save computational results). Based on these interfaces,
/// the #abstractMixedGraph class implements nearly all optimization and
/// layout algorithms.
///
/// The graph nodes are indexed 0,1,..,#N()-1, the arcs are indexed 0,1,..,2*#M()-1
/// or 0,1,..,#M()-1 depending on whether arc orientations are important or not.
/// In the former case (which is the regular one), the indices 2*i and 2*i+1 refer
/// to the two directions of the same edge.
class abstractMixedGraph : public virtual managedObject
{
friend class surfaceGraph;
friend class sparseRepresentation;
friend class denseRepresentation;
friend class graphRepresentation;
friend class branchAsyTSP;
friend class branchSymmTSP;

private:

    mutable THandle                             itCounter;
    mutable investigator**                      pInvestigator;
    mutable THandle                             LH,RH;
        // Data structures for investigator access

    mutable goblinHashTable<TArc,TArc>*  adj; ///< Hash table for node adjacencies

protected:

    disjointFamily<TNode>*  partition; ///< Partition of the node set (connected components, colourings)

    goblinQueue<TNode,TFloat>*  nHeap; ///< Cached priority queue for the graph nodes

    /// \brief  Create a priority queue to store graph node indices in the range [0,1,..,n-1]
    ///
    /// \return  A pointer to the new heap object
    ///
    /// The concrete heap type depends on the context parameter methPQ
    goblinQueue<TNode,TFloat>* NewNodeHeap() throw(ERRejected);

    /// \brief  Create a priority queue to store arc indices in the range [0,1,..,m-1]
    ///
    /// \return  A pointer to the new heap object
    ///
    /// The concrete heap type depends on the context parameter methPQ
    goblinQueue<TArc,TFloat>*  NewArcHeap() throw(ERRejected);

    TFloat*  sDeg;    ///< Explicit degree labels, counting all undirected edges
    TFloat*  sDegIn;  ///< Explicit degree labels, counting all backward arcs
    TFloat*  sDegOut; ///< Explicit degree labels, counting all forward arcs

    TNode  n;  ///< Number of nodes
    TNode  ni; ///< Number of Interpolation Points
    TArc   m;  ///< Number of arcs (counting arc directions only oncce)

    TNode*  face; ///< Array of dual incidences (faces left hand of an arc)

    TNode  defaultSourceNode; ///< Default source node
    TNode  defaultTargetNode; ///< Default target node
    TNode  defaultRootNode;   ///< Default root node

    /// \brief  An attribute pool representing all potential solutions
    attributePool  registers;

public:

    abstractMixedGraph(TNode = 0,TArc = 0) throw();
    virtual ~abstractMixedGraph() throw();

    void            CheckLimits() throw(ERRejected);
    unsigned long   Allocated() const throw();


    /// \addtogroup classifications
    /// @{

    bool  IsGraphObject() const throw();

    /// \brief  Query if this graph object is bipartite
    ///
    /// \retval true  This graph is bipartite
    virtual bool  IsBipartite() const throw();

    /// \brief  Query if this graph object is directed
    ///
    /// \retval true  This graph does not not have undirected edges
    virtual bool  IsDirected() const throw();

    /// \brief  Query if this graph object is undirected
    ///
    /// \retval true  This graph does not not have directed arcs
    virtual bool  IsUndirected() const throw();

    /// \brief  Query if this a sparsely represented graph object
    ///
    /// \retval true  This graph is represented by incidence lists
    virtual bool  IsSparse() const throw();

    /// \brief  Query if this a densely represented graph object
    ///
    /// \retval true  This graph is represented by an adjacency matrix
    virtual bool  IsDense() const throw();

    /// \brief  Query if this a represented graph object
    ///
    /// \retval true  This graph is represented
    inline bool  IsRepresented() const throw();

    /// \brief  Query if this a balanced graph object
    ///
    /// \retval true  This graph is balanced (skew-symmetric)
    virtual bool  IsBalanced() const throw();

    /// \brief  Query if this an embedded planar graph
    ///
    /// \retval true  This graph stores dual incidence information
    inline bool  IsEmbedded() const throw();

    /// @}


    /// \addtogroup manipSkeleton
    /// @{

    /// \brief  Insert an arc with given end nodes and with default or random attribute values
    ///
    /// \param u  The start node index ranged [0,1,..,n-1]
    /// \param v  The start node index ranged [0,1,..,n-1]
    /// \return   The index of the new arc ranged [0,1,..,m-1]
    ///
    /// In sparsely represented graphs, this procedure adds an arc connecting
    /// u and v and inserts it into to the incidence lists of these node,
    /// adjusts the dimensions of all attributes. and return the value of M()
    /// in advance.
    ///
    /// In densely represented graphs, the procedure only increases
    /// the capacity of an existing arc and returns that arc's index.
    ///
    /// The new arc is assigned with default or random attribute values,
    /// depending on the context parameter randLength, randUCap and randLCap.
    TArc  InsertArc(TNode u,TNode v) throw(ERRange,ERRejected);

    /// \brief  Insert an arc with given end nodes and given representational arc labels
    ///
    /// \param u  The start node index ranged [0,1,..,n-1]
    /// \param v  The start node index ranged [0,1,..,n-1]
    /// \param cc  An upper capacity bound
    /// \param ll  A length label
    /// \param bb  A lower capacity bound
    /// \return   The index of the new arc ranged [0,1,..,m-1]
    ///
    /// In sparsely represented graphs, this procedure adds an arc connecting
    /// u and v and inserts it into to the incidence lists of these node,
    /// adjusts the dimensions of all attributes. and return the value of M()
    /// in advance.
    ///
    /// In densely represented graphs, the procedure only increases
    /// the capacity of an existing arc and returns that arc's index.
    TArc  InsertArc(TNode u,TNode v,TCap cc,TFloat ll,TCap bb=0) throw(ERRange,ERRejected);

    /// \brief  Insert a node
    ///
    /// \return   The index of the new arc: The value of N() in advance
    TNode  InsertNode() throw(ERRange,ERRejected);

    /// \brief  Instant deletion of a node
    ///
    /// \param v  A graph node index ranged [0,1,..,n-1]
    ///
    /// For sparsely represented graphs, this may corrupt the indices of other nodes and arcs!
    void  DeleteNode(TNode v) throw(ERRange,ERRejected);

    /// @}


    /// \addtogroup groupRandomManipulation
    /// @{

    /// \brief  Extend a graph by random arcs
    ///
    /// \param _m  The number of arcs to be added
    virtual void  RandomArcs(TArc _m) throw(ERRejected);

    /// \brief  Extend a graph by a random Euler cycle
    ///
    /// \param _m  The number of arcs to be added
    void  RandEulerian(TArc _m) throw(ERRejected);

    /// \brief  Extend a graph to a degree regular graph
    ///
    /// \param k  The desired node degree
    virtual void  RandRegular(TArc k) throw(ERRejected);

    /// @}


    /// \addtogroup groupMergeGraphs
    /// @{

    /// \brief  Merge display modes
    enum TMergeLayoutMode {
        MERGE_ALIGN_RIGHT = 0, ///< Merged graph is displayed right-hand of the current graph
        MERGE_ALIGN_BELOW = 1, ///< Merged graph is displayed below of the current graph
        MERGE_OVERLAY     = 2  ///< Merged graph is overlayed with the current graph
    };

    /// \brief  Disjoint merge of another graph into this object
    ///
    /// \param G                The graph to be merged
    /// \param mergeLayoutMode  The display position of the merged graph, relative to the current graph
    ///
    /// The copy of G is node disjoint with the former destination graph. That is,
    /// the resulting graph has at least two connected components.
    void  AddGraphByNodes(abstractMixedGraph& G,
        TMergeLayoutMode mergeLayoutMode = MERGE_OVERLAY) throw(ERRejected);

    /// \brief  Fill the appropriate faces with copies of another graph
    ///
    /// \param G  The graph to be merged
    ///
    /// This copies G several times into this object, namely for every face of
    /// this object which has the same length as the exterior face of G. This
    /// graph must provide a planar representation, and G must provide an
    /// exterior arc. It is not required that G is planar, but only that an
    /// orbit of exterior arcs is given. In the resulting graph, this exterior
    /// orbit of G is identified with the faces of this graph, and the interior
    /// of G subdivides the faces of this graph. If G is planar (represented),
    /// so is the merge result.
    ///
    /// The procedure also determines a geometric representation, if both this
    /// graph and G provide 2D or 3D straight line drawings. Supposed that all
    /// relevant faces are regular polygones, G is rotated and scaled as expected.
    /// Then in the 2D setting, a plane drawing results. In the 3D setting,
    /// Observe that a convex drawing results only in special circumstances.
    void  FacetIdentification(abstractMixedGraph& G) throw(ERRejected);

    /// @}


    /// \addtogroup objectDimensions
    /// @{

    /// \brief  Query the number of graph nodes
    ///
    /// \return  The number of graph nodes
    inline TNode  N() const throw();

    /// \brief  Query the number of display nodes
    ///
    /// \return  The number of display nodes, including the graph nodes
    inline TNode  L() const throw();

    /// \brief  Query the number of artificial nodes
    ///
    /// \return  The number of display nodes, graph nodes excluded
    inline TNode  NI() const throw();

    /// \brief  Query the number of graph edges
    ///
    /// \return  The number of graph edges
    inline TArc  M() const throw();

    /// \brief  Query the number of dual graph nodes
    ///
    /// \return  The number of dual graph nodes
    ///
    /// This is useful for planar graphs only
    inline TNode  ND() const throw();

    virtual size_t  SizeInfo(TArrayDim arrayDim,TSizeInfo size) const throw();

    /// @}


    /// \addtogroup groupAttributes
    /// @{

    /// \brief  Access the representational object
    ///
    /// \return  A pointer to the representational object or NULL
    virtual graphRepresentation*  Representation() throw();

    /// \brief  Non-modifying access the representational object
    ///
    /// \return  A const pointer to the representational object or NULL
    virtual const graphRepresentation*  Representation() const throw();

    /// \brief  Access the representational data pool
    ///
    /// \return  A pointer to the representational data pool object or NULL
    virtual attributePool*  RepresentationalData() const throw();

    /// \brief  Access the geometry data pool
    ///
    /// \return  A pointer to the geometry data pool object or NULL
    virtual attributePool*  Geometry() const throw();

    /// \brief  Access the layout data pool
    ///
    /// \return  A pointer to the layout data pool object or NULL
    virtual attributePool*  LayoutData() const throw();

    /// \brief  Inherit all layout pool data from another graph object
    ///
    /// \param G  The graph object to copy from
    ///
    /// If G or this graph does not own a layout pool, the procedure returns
    /// without any effect.
    void ImportLayoutData(const abstractMixedGraph& G) throw();

    /// \brief  Extract a layout parameter from the layout data pool
    ///
    /// \param tokenStr  A token label
    /// \param valueStr  A character string buffer by which the parameter value is returned
    /// \retval false    If no parameter value has been found in the pool, and no default value is known
    ///
    /// If no parameter value is stored in the pool, a default value is substituted.
    /// The latter value might depend on the current layout model.
    bool GetLayoutParameter(const char* tokenStr,char* valueStr) const throw();

    /// \brief  Extract a layout parameter from the layout data pool
    ///
    /// \param token   A token symbol
    /// \param value   The variable by which the parameter value is returned
    /// \param model   The active layout model
    /// \retval false  If no parameter value has been found in the pool, and no default value is known
    ///
    /// If no parameter value is stored in the pool, a default value is substituted.
    /// The latter value might depend on the current layout model.
    template <typename T> inline
        bool GetLayoutParameter(TOptLayoutTokens token,T& value,TLayoutModel model = LAYOUT_DEFAULT) const throw();

private:

    bool GetLayoutParameterImpl(TOptLayoutTokens token,int& value,TLayoutModel model) const throw();
    bool GetLayoutParameterImpl(TOptLayoutTokens token,double& value,TLayoutModel model) const throw();
    bool GetLayoutParameterImpl(TOptLayoutTokens token,char*& value,TLayoutModel model) const throw();

public:

    /// \brief  Assign a layout parameter
    ///
    /// \param tokenStr  A token label
    /// \param valueStr  The parameter value to be assigned
    /// \retval false    If no parameter value has been assigned
    bool SetLayoutParameter(const char* tokenStr,const char* valueStr) throw();

    /// \brief  Assign a layout parameter
    ///
    /// \param token   A token symbol
    /// \param value   The parameter value to be assigned
    /// \retval false  If no parameter value has been assigned
    template <typename T> inline
        bool SetLayoutParameter(TOptLayoutTokens token,const T value) throw();

    /// \brief  Assign a layout parameter
    ///
    /// \param token   A token symbol
    /// \param value   The variable by which the parameter value is returned
    /// \param model   The active layout model
    /// \retval false  If no parameter value has been assigned
    template <typename T> inline
        bool SetLayoutParameter(TOptLayoutTokens token,const T value,TLayoutModel model) throw();

private:

    bool SetLayoutParameterImpl(TOptLayoutTokens token,const int value,TLayoutModel model) throw();
    bool SetLayoutParameterImpl(TOptLayoutTokens token,const double value,TLayoutModel model) throw();
    bool SetLayoutParameterImpl(TOptLayoutTokens token,const char* value,TLayoutModel model) throw();

public:

    /// \brief  Snychronize a method spacing parameter with the layout pool
    ///
    /// \param token    The token label of either node, bend or fine spacing
    /// \param spacing  A variable to be snychronized with the value in pool
    ///
    /// If spacing is less or equal zero (undefined), replace it with the value
    /// in pool. Otherwise overwrite the layout pool attribute value
    void SyncSpacingParameters(TOptLayoutTokens token,TFloat& spacing) throw();

    /// \brief  Access the register pool
    ///
    /// \return  A pointer to the register pool object
    inline attributePool*  Registers() const throw();

    /// @}


    /// \addtogroup specialEntities
    /// @{

    /// \brief  Retrieve the default source node
    ///
    /// \return  The index of the default source node
    virtual TNode  DefaultSourceNode() const throw();

    /// \brief  Retrieve the default target node
    ///
    /// \return  The index of the default target node
    virtual TNode  DefaultTargetNode() const throw();

    /// \brief  Retrieve the default root node
    ///
    /// \return  The index of the default root node
    TNode  DefaultRootNode() const throw();

    /// \brief  Assign a default source node
    ///
    /// \param v  The index of the default source node ranged [0,1,..,n-1]
    void  SetSourceNode(TNode v) throw(ERRange);

    /// \brief  Assign a default target node
    ///
    /// \param v  The index of the default target node ranged [0,1,..,n-1]
    void  SetTargetNode(TNode v) throw(ERRange);

    /// \brief  Assign a default root node
    ///
    /// \param v  The index of the default root node ranged [0,1,..,n-1]
    void  SetRootNode(TNode v) throw(ERRange);

    /// @}


    /// \addtogroup queryIncidences
    /// @{

public:

    /// \brief  Query the start node of a given arc
    ///
    /// \param a  An arc index ranged [0,1,..,2*mAct-1]
    /// \return   The index of the start node
    virtual TNode  StartNode(TArc a) const throw(ERRange) = 0;

    /// \brief  Query the end node of a given arc
    ///
    /// \param a  An arc index ranged [0,1,..,2*mAct-1]
    /// \return   The index of the end node
    virtual TNode  EndNode(TArc a) const throw(ERRange);

    /// \brief  Query the successor of a given arc in the incidence list of its start node
    ///
    /// \param a  An arc index ranged [0,1,..,2*mAct-1]
    /// \param v  The index of the start node of a (only to reduce lookup times)
    /// \return   The index of the right-hand incident arc
    virtual TArc  Right(TArc a,TNode v) const throw(ERRange,ERRejected) = 0;

    /// \brief  Query the predecessor of a given arc in the incidence list of its start node
    ///
    /// Other than the right-hand incidence, left-hand incidences are generated
    /// only on demand. The initialization is O(mAct).
    ///
    /// \param a  An arc index ranged [0,1,..,2*mAct-1]
    /// \return   The index of the left-hand incident arc
    virtual TArc  Left(TArc a) const throw(ERRange,ERRejected);

    /// \brief  Retrieve an arc with a given start node
    ///
    /// The First(v) incidence is somewhat arbitrary, since icidence lists are
    /// cyclic. But when an arc is inserted, it is inserted into the incidence
    /// lists right after the First() incidence. If the graph is planely represented,
    /// and v is an exterior node, the first two incidences are usually exterior
    /// arcs, and inserting an arc with two exterior end nodes preserves the
    /// planar representation.
    ///
    /// \param v  A node index ranged [0,1,..,nAct-1]
    /// \return   The index of an outgoing arc
    virtual TArc  First(TNode v) const throw(ERRange,ERRejected) = 0;

    /// @}


    /// \addtogroup queryReprAttributes
    /// @{

public:

    /// \brief  Retrieve a node demand
    ///
    /// \param v  A node index ranged [0,1,..,n-1]
    /// \return   The demand of node v
    virtual TCap  Demand(TNode v) const throw(ERRange);

    /// \brief  Retrieve the maximum node demand
    ///
    /// \return   The maximum demand among all graph nodes
    TCap  MaxDemand() const throw();

    /// \brief  Check if the node demands are all the same
    ///
    /// \retval true  All node demands coincide
    virtual bool  CDemand() const throw();

    /// \brief  Retrieve an upper capacity bound
    ///
    /// \param a  An arc index ranged [0,1,..,2m-1]
    /// \return   The upper capacity bound of arc a
    virtual TCap  UCap(TArc a) const throw(ERRange);

    /// \brief  Retrieve the maximum upper capacity bound
    ///
    /// \return   The maximum upper capacity bound among all graph arcs
    TCap  MaxUCap() const throw();

    /// \brief  Check if the upper capacity bounds are all the same
    ///
    /// \retval true  All upper capacity bounds coincide
    virtual bool  CUCap() const throw();

    /// \brief  Retrieve a lower capacity bound
    ///
    /// \param a  An arc index ranged [0,1,..,2m-1]
    /// \return   The lower capacity bound of arc a
    virtual TCap  LCap(TArc a) const throw(ERRange);

    /// \brief  Retrieve the maximum lower capacity bound
    ///
    /// \return   The maximum lower capacity bound among all graph arcs
    virtual TCap  MaxLCap() const throw();

    /// \brief  Check if the lower capacity bounds are all the same
    ///
    /// \retval true  All lower capacity bounds coincide
    virtual bool  CLCap() const throw();

    /// \brief  Retrieve an arc length
    ///
    /// \param a  An arc index ranged [0,1,..,2m-1]
    /// \return   The length of arc a
    virtual TFloat  Length(TArc a) const throw(ERRange);

    /// \brief  Retrieve the maximum arc length
    ///
    /// \return   The maximum arc length among all graph arcs
    TFloat  MaxLength() const throw();

    /// \brief  Check if the arc length labels are all the same
    ///
    /// \retval true  All arc length labels coincide
    virtual bool  CLength() const throw();

    /// @}


    /// \addtogroup arcOrientations
    /// @{

    /// \brief  Retrieve an arc orientation
    ///
    /// \param a   An arc index ranged [0,1,..,2m-1]
    /// \retval 0  This edge is undirected
    /// \retval 1  This edge is directed
    virtual char  Orientation(TArc a) const throw(ERRange);

    /// \brief  Check if the graph has directed arcs
    ///
    /// \retval 0  All arcs are undirected
    /// \retval 1  Some arcs are directed
    char  Orientation() const throw();

    /// \brief  Check if the arc orientations are all the same
    ///
    /// \retval true  Either all arcs are directed or undirected
    virtual bool  COrientation() const throw();

    /// \brief  Retrieve a reverse arc index
    ///
    /// \param a   An arc index ranged [0,1,..,2m-1]
    /// \return    The reverse arc a^1
    TArc  Reverse(TArc a) const throw(ERRange);

    /// \brief  Sort out backward arcs
    ///
    /// \param a      The index of the considered arc ranged [0,1,..,2m-1]
    /// \retval true  The arc is the reverse of a directed arc
    virtual bool  Blocking(TArc a) const throw(ERRange);

    /// @}


    /// \addtogroup nodeCoordinates
    /// @{

public:

    /// \brief  Retrieve a node coordinate value
    ///
    /// \param v  A node index ranged [0,1,..,n+ni-1]
    /// \param i  A coordinate index
    /// \return   The i-th coordinate value of node v
    virtual TFloat  C(TNode v,TDim i) const throw(ERRange);

    /// \brief  Set a node coordinate value
    ///
    /// \param v    A node index ranged [0,1,..,n+ni-1]
    /// \param i    A coordinate index
    /// \param pos  The desired coordinate value for node v
    void  SetC(TNode v,TDim i,TFloat pos) throw(ERRange,ERRejected);

    /// \brief  Retrieve the minimum coordinate value for a given coordinate index
    ///
    /// \param i  A coordinate index
    /// \return   The minimum i-th coordinate value among all nodes
    TFloat  CMin(TDim i) const throw(ERRange);

    /// \brief  Retrieve the maximum coordinate value for a given coordinate index
    ///
    /// \param i  A coordinate index
    /// \return   The maximum i-th coordinate value among all nodes
    TFloat  CMax(TDim i) const throw(ERRange);

    /// \brief  Retrieve the coordinate representational dimension
    ///
    /// \return  The coordinate representational dimension
    virtual TDim  Dim() const throw();

    /// \brief  Arc length metric types
    enum TMetricType {
        METRIC_DISABLED = 0,  ///< Apply the arc length attribute values
        METRIC_MANHATTAN = 1, ///< Apply the Manhattan metric
        METRIC_EUCLIDIAN = 2, ///< Apply the Euclidian metric
        METRIC_MAXIMUM = 3,   ///< Apply the maximum coordinate metric
        METRIC_SPHERIC = 4    ///< Apply an earth speric metric
    };

    /// \brief  Retrieve the type of arc length metric
    ///
    /// \return  The metric type
    TMetricType  MetricType() const throw();

    /// @}


    /// \addtogroup arcOrientations
    /// @{

    /// \brief  Retrieve a reduced arc length
    ///
    /// \param pi  A pointer to an array of node potentials
    /// \param a   An arc index ranged [0,1,..,2m-1]
    /// \return    The reduced length of arc a
    ///
    /// This used by several optimization codes to derive benefit from dual
    /// solutions (the node potentials) which have been computed in advance.
    ///
    /// For undirected edges: RedLength(u,v) =  RedLength(v,u) = Length(u,v) + pi[u] + pi[v]
    ///
    /// For directed arcs:    RedLength(u,v) = -RedLength(v,u) = Length(u,v) + pi[u] - pi[v]
    virtual TFloat  RedLength(const TFloat* pi,TArc a) const throw(ERRange);

    /// @}


    /// \addtogroup groupArcRouting
    /// @{

public:

    /// \brief  Retrieve the display anchor point of a giiven node
    ///
    /// \param v      A node index ranged [0,1,..,n-1]
    /// \retval true  The anchor point index ranged [0,1,..,n+ni-1]
    ///
    /// This returns a layout point with absolute coordinates. With this point,
    /// the node label and all incident arcs are aligned.
    /// For the time being, it is NodeAnchorPoint(v)==v.
    TNode  NodeAnchorPoint(TNode v) const throw(ERRange);

    /// \brief  Retrieve the label display anchor point of a given arc
    ///
    /// \param a      An arc index ranged [0,1,..,2m-1]
    /// \retval true  The anchor point index ranged [n,n+1,..,n+ni-1]
    TNode  ArcLabelAnchor(TArc a) const throw(ERRange);

    /// \brief  Check if any arc label display anchor points exist
    ///
    /// \retval true  No arc label anchor points are allocated
    bool  NoArcLabelAnchors() const throw();

    /// \brief  Retrieve the port node or the first control point of a given arc
    ///
    /// \param a  An arc index ranged [0,1,..,2m-1]
    /// \return   A control point index ranged [n,n+1,..,n+ni-1]
    ///
    /// This respects the arc orientations. That is, for an odd index a,
    /// the final control point is returned instead of the first one.
    TNode  PortNode(TArc a) const throw(ERRange);

    /// \brief  Retrieve the follow-up point of a given layout point
    ///
    /// \param v      A node index ranged [0,1,..,n+ni-1]
    /// \retval true  The follow-up node index ranged [n,n+1,..,n+ni-1]
    TNode  ThreadSuccessor(TNode v) const throw(ERRange);

    /// \brief  Check if any follow-up points exist
    ///
    /// \retval true  No thread points are allocated
    bool  NoThreadSuccessors() const throw();

    /// @}


    /// \addtogroup groupLayoutModel
    /// @{

public:

    /// \brief  Retrieve the current layout model
    ///
    /// \return  The current layout model
    TLayoutModel  LayoutModel() const throw();

    /// \brief  Formally assign a layout model
    ///
    /// \param model  The intended layout model
    void  SetLayoutModel(TLayoutModel model) throw();

    /// @}


    /// \addtogroup groupLayoutFilter
    /// @{

public:

    /// \brief  Check wether a given graph node shall be visualized or not
    ///
    /// \param v      A node index ranged [0,1,..,n-1]
    /// \retval true  This node is not visualized
    ///
    /// If the node v is not visualized, none of the incident arcs are visualized
    virtual bool  HiddenNode(TNode v) const throw(ERRange);

    /// \brief  Check wether a given arc shall be visualized or not
    ///
    /// \param a      An arc index ranged [0,1,..,2m-1]
    /// \retval true  This arc is not visualized
    virtual bool  HiddenArc(TArc a) const throw(ERRange);

    /// \brief  Modify a node visibilty state
    ///
    /// \param v        A node index ranged [0,1,..,n-1]
    /// \param visible  If true, the node is visible hereafter
    ///
    /// If the node v is not visualized, none of the incident arcs are visualized
    void  SetNodeVisibility(TNode v,bool visible) throw(ERRange);

    /// \brief  Modify an arc visibilty state
    ///
    /// \param a        An arc index ranged [0,1,..,2m-1]
    /// \param visible  If true, the node is visible hereafter
    void  SetArcVisibility(TArc a,bool visible) throw(ERRange);

    /// @}


    /// \addtogroup nodeAdjacencies
    /// @{

public:

    /// \brief  Strategy for the generation of none-node adjacency matrices
    enum TMethAdjacency {
        ADJ_SEARCH = 0, ///< If no adjacency matrix exists, let Adjacency() search the node incidence list
        ADJ_MATRIX = 1  ///< If no adjacency matrix exists, let Adjacency() generate it
    };

    /// \brief  Retrieve an arc connecting two given nodes
    ///
    /// \param  u       A node index in the range [0,1..,n-1]
    /// \param  v       A node index in the range [0,1..,n-1]
    /// \param  method  A TMethAdjacency strategy option
    /// \return         The index of any arc connecting u and v, or NoArc
    ///
    /// This function logically implements a node-node adjacency matrix.
    /// In the default implementation, such a matrix is physically generated
    /// or, if ADJ_SEARCH is passed, the incidence list of u is searched.
    ///
    /// All implementations prefer non-blocking arcs and, if there is more than
    /// one candidate arc even then, return the arc with the least possible index.
    virtual TArc  Adjacency(TNode u,TNode v,TMethAdjacency method = ADJ_MATRIX) const throw(ERRange);

    /// \brief  Update a node adjacency reference
    ///
    /// \param  u   A node index in the range [0,1..,n-1]
    /// \param  v   A node index in the range [0,1..,n-1]
    /// \param  a   An arc index in the range [0,1..,2*m-1]
    ///
    /// Update a potential node-node adjacency matrix which might have been
    /// materialized due to previous Adjacency() calls. In particular, set
    /// the arc index to be returned for the node pair u,v.
    void  MarkAdjacency(TNode u,TNode v,TArc a) throw(ERRange,ERRejected);

    /// \brief  Delete the node adjacency references
    ///
    /// Delete a potential node-node adjacency matrix which have been
    /// materialized due to previous Adjacency() calls.
    void  ReleaseAdjacencies() throw();

    /// @}


    /// \addtogroup groupMapIndices
    /// @{

public:

    /// \brief Map back a node index to the original data object
    ///
    /// \param  v   A node index in the range [0,1..,n-1]
    /// \return     The index of the original entity
    ///
    /// When graph objects are composed from other objects, nodes usually refer
    /// to entities in the original object. These entities are most likely but
    /// not necessarily nodes. By conventition, if the referenced entities are
    /// arcs, the returned indices also encode the original arc orientations.
    ///
    /// To have those references, graph objects must be generated with the
    /// OPT_MAPPINGS option and must not have been deleted in advance. The type
    /// of referenced entities is application dependant.
    virtual TIndex  OriginalOfNode(TNode v) const throw(ERRange);

    /// \brief Map back an arc index to the original data object
    ///
    /// \param  a   An arc index in the range [0,1..,2*m-1]
    /// \return     The index of the original entity
    ///
    /// When graph objects are composed from other objects, nodes usually refer
    /// to entities in the original object. These entities are most likely but
    /// not necessarily nodes.
    ///
    /// To have those references, graph objects must be generated with the
    /// OPT_MAPPINGS option. The type of referenced entities is application
    /// dependant. By conventition, if the referenced entities are arcs, the
    /// returned indices encode the original arc orientations. The passed
    /// are index also includes the orientation bit, but it is required that
    /// OriginalOfArc(a)==OriginalOfArc(a^1).
    virtual TIndex  OriginalOfArc(TArc a) const throw(ERRange);

    /// \brief  Irreversibly delete node index references
    ///
    /// Delete the node index references which have been materialized due to
    /// the OPT_MAPPINGS option passed to the constructor of this object.
    void  ReleaseNodeMapping() throw();

    /// \brief  Irreversibly delete arc index references
    ///
    /// Delete the arc index references which have been materialized due to
    /// the OPT_MAPPINGS option passed to the constructor of this object.
    void  ReleaseArcMapping() throw();

    /// @}


    /// \addtogroup groupLayoutBounding
    /// @{

public:

    /// \brief  Fit the current drawing into the first orthant
    ///
    /// This shifts all nodes by the same amount such that the coordinate values
    /// become non-negative, and a zero value is achieved for every coordinate
    void  Layout_AlignWithOrigin() throw(ERRejected);

    /// \brief  Fit the current drawing into a specified bounding interval
    ///
    /// \param i     A component of the current drawing
    /// \param cMin  The value to which the lower interval limit is mapped
    /// \param cMax  The value to which the upper interval limit is mapped
    ///
    /// This performs an affine transformation of all graph node and layout
    /// point positions, changing only a specific coordinate of each point.
    /// It is feasible to specify cMin > cMax. In that case, the drawing
    /// is mirrored about the orthogonal hyperplane.
    void  Layout_TransformCoordinate(TDim i,TFloat cMin,TFloat cMax) throw(ERRejected);

    /// \brief  Retrieve a bounding interval
    ///
    /// \param i     A component of the current drawing
    /// \param cMin  Returns the lower bound
    /// \param cMax  Returns the upper bound
    ///
    /// For the given component, the bounding interval is determined. If the bounding
    /// box has been frozen in advance, the saved values are returned. Otherwise, the
    /// following default rule applies: For grid layout models, the tight interval of
    /// control point coordinates is determined, and enlarged by the bend spacing value.
    /// Otherwise, the tight interval of graph node coordinates is determined (no layout
    /// points are considered!), and enlarged by the node spacing value.
    void  Layout_GetBoundingInterval(TDim i,TFloat& cMin,TFloat& cMax) const throw();

    /// \brief  Freeze the current bounding box
    ///
    /// This freezes the current bounding box. A bounding box saved in advance
    /// is not changed. If the bounding box was computed dynamically before,
    /// the display does not change immediately by this operation. But later node
    /// insertions, deletions or movements do not have impact on the placement of
    /// unchanged nodes.
    void  Layout_FreezeBoundingBox() throw();

    /// \brief  Freeze the default bounding box
    ///
    /// This freezes the default bounding box. A bounding box saved in advance
    /// is lost. If the bounding box was computed dynamically before, the display
    /// does not change immediately by this operation. But later node insertions,
    /// deletions or movements do not have impact on the placement of unchanged
    /// nodes.
    void  Layout_DefaultBoundingBox() throw();

    /// \brief  Release an explicit bounding box definition
    ///
    /// This deletes all bounding box data. Subsequently, drawings apply the dynamic
    /// bounding box computation implemented in Layout_GetBoundingInterval().
    void  Layout_ReleaseBoundingBox() throw();

    /// @}


    /// \addtogroup groupLayoutModel
    /// @{

public:


    /// \brief  Translate a drawing to another layout model
    ///
    /// \param model  The intended layout model
    ///
    /// This assigns a new layout model and translates the drawing so that it
    /// formally conforms with the new model. The rules are as follows:
    /// - If the old and the new layout model are the same, only some of the
    ///   display parameters are set, depending on the concrete model
    /// - If neither the old nor the new model is LAYOUT_STRAIGHT_2DIM and
    ///   the new model is not a generalization of the old one, there is an
    ///   implicit intermediate conversion to the LAYOUT_STRAIGHT_2DIM model.
    /// - If the new model is LAYOUT_STRAIGHT_2DIM, all shape and bend points
    ///   are eliminated.
    /// - If the new model does not require differently, Layout_ArcRouting()
    ///   is called to handle loops andparallel arcs.
    void  Layout_ConvertModel(TLayoutModel model) throw();

    /// @}


    /// \addtogroup groupLayoutCircular
    /// @{

public:

    /// \brief  Draw all nodes on the boundary of a circle
    ///
    /// \param spacing  The minimum node distance
    ///
    /// This arranges the graph nodes on the boundary of a circle. The radius
    /// of this circle is such that the distance of two adjacent nodes becomes
    /// spacing or, if spacing==0, the nodeSpacing configuration parameter value.
    ///
    /// With decreasing preference, the clockwise order of nodes is defined by
    /// one of the following properties:
    /// - An outerplanar representation
    /// - The predecessor labels
    /// - The node colours
    /// - The node indices
    void  Layout_Circular(TFloat spacing=0.0) throw(ERRejected);

    /// \brief  Circular drawing based on given predecessor arcs
    ///
    /// \param spacing  The minimum node distance
    ///
    /// This arranges the graph nodes on the boundary of a circle. The radius
    /// of this circle is such that the distance of two adjacent nodes becomes
    /// spacing or, if spacing==0, the nodeSpacing configuration parameter value.
    ///
    /// The clockwise order of nodes is such that Hamiltonian cycles and
    /// 2-factors are exposed. If the predecessor labels do not encode a
    /// 2-factor,  this procedure still puts the predecessor arcs on the
    /// exterior by using a greedy strategy.
    void  Layout_CircularByPredecessors(TFloat spacing=0.0) throw(ERRejected);

    /// \brief  Circular drawing based on a node colouring
    ///
    /// \param spacing  The minimum node distance or 0
    ///
    /// This arranges the graph nodes on the boundary of a circle. The radius
    /// of this circle is such that the distance of two adjacent nodes becomes
    /// spacing or, if spacing==0, the nodeSpacing configuration parameter value.
    ///
    /// The clockwise order of nodes is with increasing colour indices and, for
    /// equal colour indices, with increasing node indices.
    void  Layout_CircularByColours(TFloat spacing=0.0) throw(ERRejected);

    /// \brief  Circular drawing of outerplanar graphs
    ///
    /// \param spacing  The minimum node distance or 0
    ///
    /// This arranges the graph nodes on the boundary of a circle. The radius
    /// of this circle is such that the distance of two adjacent nodes becomes
    /// spacing or, if spacing==0, the nodeSpacing configuration parameter value.
    ///
    /// An outerplanar representation must be set in advance. If so, the procedure
    /// displays precisely the given combinatorial embedding:
    /// 
    bool  Layout_Outerplanar(TFloat spacing=0.0) throw(ERRejected);

    /// \brief  Multi-circular drawing of outerplanar graphs
    ///
    /// \param spacing  The minimum node distance or 0
    ///
    /// This arranges the nodes of any interior face on the boundary of a circle.
    /// The radius of this circle is such that the distance of two adjacent nodes
    /// becomes spacing or, if spacing==0, the nodeSpacing configuration parameter
    /// value.
    ///
    /// An outerplanar representation must be set in advance. If so, the procedure
    /// displays precisely the given combinatorial embedding:
    ///
    /// It is not verified that a plane drawing results. In fact, this is true
    /// only in very special cases. The procedure can be used to produce paper
    /// models of 3D polyhedra, after separating some faces according to a
    /// spanning tree.
    void  Layout_Equilateral(TFloat spacing=0.0) throw(ERRejected);

private:

    /// \brief  Circular drawing based on an explicit node ordering
    ///
    /// \param spacing  The minimum node distance or 0
    /// \param index    A pointer to an array of node indices or NULL
    ///
    /// This arranges the graph nodes on the boundary of a circle. The radius
    /// of this circle is such that the distance of two adjacent nodes becomes
    /// spacing or, if spacing==0, the nodeSpacing configuration parameter value.
    /// The clockwise order of node indices is index[0],index[1],..,index[n-1]
    /// or, if index==NULL, 0,1,..,n-1.
    void  Layout_AssignCircularCoordinates(TFloat spacing=0.0,TNode* index=NULL) throw(ERRejected);

    /// @}


    /// \addtogroup groupOrthogonalDrawing
    /// @{

public:

    /// \brief  Alternative methods for orthogonal drawing
    enum TMethOrthogonal {
        ORTHO_DEFAULT = -1,          ///< Apply the default method set in #goblinController::methOrthogonal
        ORTHO_EXPLICIT = 0,          ///< 1-bent Kandinsky drawing where the arc routing is entirely determined by the original arc orientations
        ORTHO_EULER = 1,             ///< 1-bent Kandinsky drawing where the arc routing is given by the orientation of an Eulerian cycle
        ORTHO_DEG4 = 2,              ///< Draw graph with maximum node degree 4 by small nodes
        ORTHO_4PLANAR = 3,           ///< Draw planar graph with maximum node degree 4 by small nodes
        ORTHO_VISIBILITY_RAW = 4,    ///< 1-visibility representation
        ORTHO_VISIBILITY_TRIM = 5,   ///< 1-visibility representation with minimum width vertices
        ORTHO_VISIBILITY_GIOTTO = 6  ///< Shrink the nodes of a 1-visibility representation to the minimum width which is admissible in the Giotto model
    };

    /// \brief Draw a graph in the 1-bent Kandinsky layout model
    ///
    /// \param method   Either #ORTHO_EXPLICIT, #ORTHO_EULER or #ORTHO_DEFAULT
    /// \param spacing  The minimum distance between two nodes
    ///
    /// This methods draws the graph such that all nodes are squares of equal
    /// size and are placed in a coarse grid which is a multiple of spacing.
    /// The arcs are drawn by horizontal and vertical segments with at most
    /// one bend. If #ORTHO_EULER is used, the number of arcs which are
    /// attached horizontally to a particular node is (almost) equalized
    /// with the number of vertically attached arcs. If #ORTHO_EXPLICIT is
    /// used, all arcs are leaving vertically and entering horizontally with
    /// respect to the original arc orientations.
    void  Layout_Kandinsky(TMethOrthogonal method = ORTHO_DEFAULT,TFloat spacing = 0.0) throw(ERRejected);

private:

    /// \brief For the 1-bent Kandinsky methods: Set the node positions and the
    ///        node sizes, so that arcs can be attached at the specified side
    ///
    /// \param orientation  Virtual orientation so that arcs start at a vertical
    ///                     port and end at an horizontal port
    /// \return             The achieved node size
    ///
    /// This method is called with a finalized sketch. It determines the port sides
    /// for every arc and the maximum number of ports attached to a single node side.
    TArc  Layout_KandinskyScaleNodes(const char* orientation) throw();

    /// \brief For Kandinsky methods: Place a single control point and two ports for each arc
    ///
    /// \param orientation  Virtual orientation so that arcs start at a vertical
    ///                     port and end at an horizontal port
    ///
    /// This forms the final step in the Kandinsky layout process. Two port nodes,
    /// a arc label alignment node and, potentially, a control point are allocated and
    /// positioned as required. The arcs attached to a particular node side do not
    /// cross each other, but arcs attached to different sides of the same node
    /// may cross each other (if the sktech is poor).
    void  Layout_KandinskyRouteArcs(const char* orientation) throw();

    /// \brief  The four sides of a rectangular node in an orthogonal drawing
    enum TPortSide {
        PORT_NORTH = 0, ///< Attach arc to the y minus side of its end node
        PORT_EAST  = 1, ///< Attach arc to the x plus side of its end node
        PORT_SOUTH = 2, ///< Attach arc to the y plus side of its end node
        PORT_WEST  = 3  ///< Attach arc to the x minus side of its end node
    };

    /// \brief For Kandinsky methods: Determine the port side of a given arc
    ///
    /// \param a            The index of the considered arc
    /// \param orientation  Virtual orientation so that arcs start at a vertical
    ///                     port and end at an horizontal port. 
    /// \return             The side of the start node of the arc a to which the
    ///                     arc will be attached
    ///
    /// In the 1-bent Kandinsky model, the arc routing depends on a virtual arc
    /// orientation and the relative position of the arc end nodes. If both end
    /// nodes are in the same row or column, no control point is used and the port
    /// sides are obvious. If not, the virtual orientation tells which node is
    /// left vertically, and which node is entered horizontally.
    TPortSide  Layout_KandinskyPortSide(TArc a,const char* orientation) throw(ERRange);

    /// \brief For the 1-bent Kandinsky methods: Reduce the sketch grid size
    /// \brief before drawing the arcs
    ///
    /// \param orientation  Virtual orientation so that arcs start at a vertical
    ///                     port and end at an horizontal port
    /// \param planar       Input drawing is without edge crossings
    /// \return             True, if the sketch grid size could be reduced
    ///
    /// Sweeps over the sketch again and again, searching for neighbouring
    /// grid lines which can be merged. No edge crossings are introduced.
    /// Adjacent nodes are placed in the same grid lines only if the routing
    /// of the connecting arc is obvious.
    ///
    /// The procedure works for planar but also for general graphs. It does
    /// not introduce edge crossings, it does not increase the required
    /// node size and, for each particular arc, it does not increase the
    /// number of control points.
    bool  Layout_KandinskyCompaction(char* orientation,bool planar) throw();

    /// \brief For the 1-bent Kandinsky methods: Reduce the number of edge
    /// \brief crossings by succesive reversion of arc orientations
    ///
    /// \param orientation  Virtual orientation so that arcs start at a vertical
    ///                     port and end at an horizontal port
    /// \return             The final number of edge crossings
    /// 
    /// This procedure requires nodes in general position (nodes do not share
    /// grid lines). It modifies the virtual arc orientations as long as
    /// reverting a single arc decreases the number of crossiings.
    TArc  Layout_KandinskyCrossingMinimization(char* orientation) throw();

    /// \brief For 1-bent Kandinsky methods:
    /// \brief Check if a node can be shifted to the grid line of another node
    ///
    /// \param orientation  Virtual orientation so that arcs start at a vertical
    ///                     port and end at an horizontal port
    /// \param i            A coordinate index (0 or 1)
    /// \param w            The index of the node to be moved
    /// \param v            Another node index such that C(v,i^1) > C(w,i^1)
    /// \return             True, if w could be shifted to the grid line of v
    ///
    /// This method checks if taking C(w,i):=C(v,i) introduces edge crossings of
    /// the nodes incident with w and v. Moving w is also prohibited if w and v
    /// are adjacent, and both nodes have further arcs attached to the the
    /// respective side.
    bool  Layout_KandinskySeparableNodes(const char* orientation,TDim i,TNode v,TNode w) throw();

    /// \brief For 1-bent Kandinsky methods:
    /// \brief Inspect two consecutive grid lines and shift segments from one line to the other
    ///
    /// \param orientation     Virtual orientation so that arcs start at a vertical
    ///                        port and end at an horizontal port
    /// \param i               A coordinate index (0 or 1)
    /// \param line            A two-dimensional array to store the node indices of two grid lines
    /// \param firstLine       The buffer index of the grid line back in the sweeping direction
    /// \param mergeWholeLine  If true, only complete lines may be shifted
    /// \return                True, if the drawing could be simplified
    ///
    /// Helper procedure for #Layout_KandinskyCompaction(). It determines consecutive
    /// edge segments which are parallel to both grid lines and occasionally moves
    /// them to the other grid line.
    bool  Layout_KandinskyShiftChain(const char* orientation,
                TDim i,TNode** line,TNode firstLine,bool mergeWholeLine) throw();

    /// \brief For 1-bent Kandinsky methods:
    /// \brief Move nodes on a grid line so that ports are equally distributed
    ///
    /// \param orientation     Virtual orientation so that arcs start at a vertical
    ///                        port and end at an horizontal port
    /// \return                True, if the drawing could be simplified
    ///
    /// THis operation manipulates the orientation[] of unbent arcs
    bool  Layout_KandinskyShiftNodes(char* orientation) throw();

    /// \brief For 1-bent Kandinsky methods:
    /// \brief Inspect two consecutive grid lines and flip virtual arc orientations
    ///
    /// \param orientation     Virtual orientation so that arcs start at a vertical
    ///                        port and end at an horizontal port
    /// \param i               A coordinate index (0 or 1)
    /// \param line            A two-dimensional array to store the node indices of two grid lines
    /// \param firstLine       The buffer index of the grid line back in the sweeping direction
    /// \return                True, if the drawing could be improved
    ///
    /// Helper procedure for #Layout_KandinskyCompaction(). It manipulates #orientation[]
    /// so that ports are assigned more evenly.
    bool  Layout_KandinskyRefineRouting(char* orientation,TDim i,TNode** line,TNode firstLine) throw();

public:

    /// \brief Orthogonal drawing of graphs with maximum node degree 4
    ///
    /// This method supports two variants of orthogonal drawing: If the graph
    /// is planely represented, the resulting drawing is also plane. Non-planar
    /// graphs can be also drawn. But in either case, the graph must be biconnected
    void  Layout_OrthogonalDeg4(TMethOrthogonal method = ORTHO_4PLANAR,TFloat spacing = 0.0) throw(ERRejected);

    /// \brief  Post-optimization levels for orthogonal drawing
    enum TMethOrthoRefine {
        ORTHO_REFINE_DEFAULT  = -1, ///< Apply the default method set in #goblinController::methOrthoRefine
        ORTHO_REFINE_NONE     =  0, ///< No post-minimization
        ORTHO_REFINE_PRESERVE =  1, ///< Area reduction by means of flow minimization. Preserve all bend nodes
        ORTHO_REFINE_FLOW     =  2, ///< Area reduction by means of flow minimization. Admit bend node reduction
        ORTHO_REFINE_SWEEP    =  3, ///< Sweep line method
        ORTHO_REFINE_MOVE     =  4  ///< Moving line shortest path search
    };

    /// \brief  Entry point for all orthogonal layout compaction codes
    ///
    /// \param maxSearchLevel  The requested post-optimization level
    /// \return                True, if the drawing could be simplified
    ///
    /// This implements a general post-optimization scheme calling the concrete
    /// post-optimization procedures. The scheme starts at the least possible
    /// optimization level, which is given by the stripe dissection method
    /// \ref abstractMixedGraph::Layout_OrthoFlowCompaction(). If no improvement
    /// is possible, the optmization level is increased - until maxSearchLevel
    /// has been reached. After any improvements, the procedure returns to the
    /// ORTHO_REFINE_FLOW level.
    bool  Layout_OrthoCompaction(TMethOrthoRefine maxSearchLevel = ORTHO_REFINE_DEFAULT) throw(ERRejected);

    /// Moving directions in orthogonal drawing post-optimization methods
    enum TMovingDirection {
        MOVE_X_MINUS = 0, ///< Move to the left
        MOVE_Y_MINUS = 1, ///< Move up
        MOVE_X_PLUS  = 2, ///< Move to the right
        MOVE_Y_PLUS  = 3  ///< Move down
    };

private:

    /// \brief Compaction method based on a network flow model
    ///
    /// \param movingDirection  Either 0 or 1, the coordinate direction in which layout points may be shifted
    /// \param preserveShape    If true, all edge segments are kept intact, only lengths may change
    /// \return                 True, if the drawing could be simplified
    ///
    /// This dissects the drawing into either horizonal or vertical stripes,
    /// generates a dual layered digraph with the stripes as the node set,
    /// an st-flow representing the metric of the current drawing, and then
    /// minimizes the flow value. The final flow is used
    bool  Layout_OrthoFlowCompaction(TDim movingDirection,bool preserveShape=true) throw(ERRejected);

    /// \brief Compaction method based on grid line sweeping
    ///
    /// \param sweepingDirection  The coordinate direction in which layout points may be shifted
    /// \return                   True, if the drawing could be simplified
    ///
    /// This method applies to general graphs. It sweeps either horizontally or
    /// vertically over the drawing, inspecting two consecutive grid lines.
    /// If possible, both grid lines are merged. Otherwise, line segments may be
    /// shifted to reduce the total edge length and the number of control points
    bool  Layout_OrthoSmallLineSweep(TMovingDirection sweepingDirection) throw(ERRejected);

    /// \brief Compaction method based on global search on the dual grid representation
    ///
    /// \param movingDirection  Either 0 or 1, the coordinate direction in which layout points may be shifted
    /// \return                 True, if the drawing could be simplified
    ///
    /// This method generates an explicit grid representation for the current
    /// drawing and, from that, a dual digraph which nodes are the grid cells.
    /// In the dual digraph, negative length cycles are determined which are moving
    /// lines in the original drawing. The interior of the cycle is moved, contracting
    /// edge segments in one direction and tearing line segemnts in the opposite
    /// direction. In the two other directions, no edge segemnts may be crossed by
    /// the moving line.
    ///
    /// There are two search levels implemented: If possible, moving line are
    /// generated which cross the full grid and hence provide area reductions.
    /// If not, a more exhaustive search is performed for sake of saving bends
    /// and reducing the edge span.
    ///
    /// As the negative cycle search is really inperformant, the procedure is
    /// applied only to dual representations with 5000 arcs or less.
    bool  Layout_OrthoSmallBlockMove(TDim movingDirection) throw(ERRejected);

    /// \brief Assign arcs to the four sides of a node
    ///
    /// \param pointingDirection  A coordinate direction pointing to
    /// \param v                  The index of the node of interest
    /// \param windrose           An array to store 4 arc indices
    /// \param arcRef             For every control point, the index of the covering arc
    /// \param prevBend           For every control point, the index of the preceding control point
    ///
    /// Helper procedure for #Layout_OrthoGetWindrose(). It fills the windrose[4]
    /// array with the indices of arcs attached to the respective node sides
    void  Layout_OrthoGetWindrose(TDim pointingDirection,TNode v,TNode windrose[4],
                TArc* arcRef,TNode* prevBend) throw();

    /// \brief Inspect two consecutive grid lines and shift segments connecting both lines
    ///
    /// \param sweepingDirection  The sweeping direction
    /// \param line               A two-dimensional array to store the (bend) node indices of two grid lines
    /// \param firstLine          The buffer index of the grid line back in the sweeping direction
    /// \param arcRef             For every control point, the index of the covering arc
    /// \param prevBend           For every control point, the index of the preceding control point
    /// \return                   True, if the drawing could be simplified
    ///
    /// Helper procedure for #Layout_OrthoSmallLineSweep(). It tries to find an
    /// edge segment connecting a graph node v in one of the inspected grid lines
    /// with a control point b in the other inspected grid line. If possible, this
    /// segment is shifted to cover other control points. In a special case, v is
    /// moved to the other grid line.
    bool  Layout_OrthoShiftChord(TMovingDirection sweepingDirection,TNode** line,TNode firstLine,
                TArc* arcRef,TNode* prevBend) throw();

    /// \brief Inspect two consecutive grid lines and shift segments from one line to the other
    ///
    /// \param sweepingDirection  The sweeping direction
    /// \param line               A two-dimensional array to store the (bend) node indices of two grid lines
    /// \param firstLine          The buffer index of the grid line back in the sweeping direction
    /// \param arcRef             For every control point, the index of the covering arc
    /// \param prevBend           For every control point, the index of the preceding control point
    /// \param mergeWholeLine     If true, only complete lines may be shifted
    /// \return                   True, if the drawing could be simplified
    ///
    /// Helper procedure for #Layout_OrthoSmallLineSweep(). It determines consecutive
    /// edge segments which are parallel to both grid lines and occasionally moves
    /// them to the other grid line
    bool  Layout_OrthoShiftChain(TMovingDirection sweepingDirection,TNode** line,TNode firstLine,
                TArc* arcRef,TNode* prevBend,bool mergeWholeLine) throw();

    /// \brief Compaction method based on local inspection of graph nodes
    ///
    /// \return  True, if the drawing could be simplified
    ///
    /// This inspects the graph nodes one-by-one, and its incident edges. Depending
    /// on the node degree, some basic simplifications are performed.
    bool  Layout_OrthoSmallNodeCompaction() throw(ERRejected);

public:

    void  Layout_OrthoAlignPorts(TFloat spacing,TFloat padding) throw(ERRejected);

    /// @}

    /// \addtogroup groupOrthogonalPlanar
    /// @{

public:

    /// \brief Draw a planar graph in the visibility representation model
    ///
    /// \param method   Either #ORTHO_VISIBILITY_TRIM, #ORTHO_VISIBILITY_RAW or #ORTHO_VISIBILITY_GIOTTO
    /// \param spacing  The minimum distance between two nodes / parallel edge segments
    ///
    /// This methods draws planar graphs such that all nodes are horizontal
    /// line segments, and all edges are vertical line segments. If
    /// #ORTHO_VISIBILITY_TRIM is specified, the length of the node segments is
    /// just as necessary to cover the adjacent edges. If #ORTHO_VISIBILITY_RAW
    /// is specified, the lengths of the horizontal node segments is as provided
    /// by the core algorithm (sometimes overshooting at the right-hand side).
    ///
    /// If #ORTHO_VISIBILITY_GIOTTO is specified, actually no visibility
    /// representation results, but the node segments are shrunk such that
    /// edges can enter a node at its left or right hand side.
    /// In the special case of 3-planar graphs, one obtains small nodes.
    void  Layout_VisibilityRepresentation(
                TMethOrthogonal method = ORTHO_VISIBILITY_TRIM,TFloat spacing = 0.0) throw(ERRejected);

private:

    /// \brief Draw a 2-connected planar graph in the visibility representation model
    ///
    /// \param spacing     The minimum distance between two nodes / parallel edge segments
    /// \param alignPorts  If true, all control points are aligned like port nodes
    ///
    /// This methods draws 2-connected planar graphs such that all nodes are
    /// horizontal line segments, and all edges are vertical line segments.
    /// This is the core implementation for #Layout_VisibilityRepresentation(),
    /// where node segments can can be larger than required by the adjacent
    /// edges.
    void  Layout_Visibility2Connected(TFloat spacing = 0.0,bool alignPorts = true) throw(ERRejected);

    /// @}

    /// \addtogroup groupOrthogonalTree
    /// @{

public:

    /// \brief Draw a rooted tree in the one-bent Kandinsky model
    ///
    /// \param root     The node in the upper left corner
    /// \param spacing  The minimum distance between two nodes
    void  Layout_KandinskyTree(TNode root = NoNode,TFloat spacing = 0.0) throw(ERRejected);

    /// \brief Draw a binary tree bend-free in the small node orthogonal model
    ///
    /// \param root     The node in the upper left corner
    /// \param spacing  The minimum distance between two nodes
    /// \retval true    If the predecessor arcs form a binary tree
    ///
    /// This produces so-called HV-drawing of binary trees, where is every node
    /// is placed atop and/or left-hand of its descendants. That is, subtree root
    /// nodes are displayed in the upper left corner of the tight subtree bounding box.
    ///
    /// The root node is defined by the predecessor labels if those exist, and
    /// then must the unique node without a predecessor arc. If no predecessor
    /// labels are at hand, a BFS search from the given root node is performed.
    /// If this is undefined, a degree 2 node is determined or, if non-existing,
    /// a degree 1 node, and used as the BFS root. The tree is exported by the
    /// predecessor labels then.
    bool  Layout_HorizontalVerticalTree(TNode root = NoNode,TFloat spacing = 0.0) throw(ERRejected);

    /// @}


    /// \addtogroup groupForceDirected
    /// @{

public:

    /// \brief  Alternative methods for force directed drawing
    enum TOptFDP {
        FDP_DEFAULT = -1,       ///< Apply the default method set in #goblinController::methFDP
        FDP_GEM = 0,            ///< Apply the GEM method
        FDP_SPRING = 1,         ///< Apply the spring embedder method
        FDP_RESTRICTED = 2,     ///< Apply the restricted GEM method
        FDP_LAYERED = 4,        ///< Apply the 1D force directed method
        FDP_LAYERED_RESTR = 6   ///< Apply the restricted 1D force directed method
    };

    /// \brief Refine a given graph drawing according to a model of forces
    ///
    /// \param method   A #TOptFDP value specifying the applied method
    /// \param spacing  The minimum node distance or 0
    ///
    /// This refines a given straight-line drawing by modelling the nodes as
    /// loaded particles repelling each other, and the edges as elastic bands
    /// keeping its end nodes close to each other. Node are moved according to
    /// the unbalanced forces in the current drawing. Depending on the concrete
    /// method, there can be additional restrictions on the node movements.
    void  Layout_ForceDirected(TOptFDP method=FDP_DEFAULT,TFloat spacing=0.0) throw(ERRejected);

private:

    /// \brief Refine a given straight-line drawing according to the spring force model
    ///
    /// \param spacing  The minimum node distance or 0
    ///
    /// This refines a given straight-line drawing by modelling the nodes as
    /// loaded particles repelling each other, and the edges as springs keeping
    /// its end nodes close to each other. Node are moved simultaneously by
    /// Newton iterations which requires inversion of Jacobian matrix. By that,
    /// this procedure is slow and memory consuming.
    void  Layout_SpringEmbedder(TFloat spacing=0.0) throw(ERRejected);

    /// \brief Refine a given straight-line drawing according to a model of forces
    ///
    /// \param method   Either #FDP_GEM or #FDP_RESTRICTED
    /// \param spacing  The minimum node distance or 0
    ///
    /// This refines a given straight-line drawing by modelling the nodes as
    /// loaded particles repelling each other, and the edges as elastic bands
    /// keeping its end nodes close to each other. Node are moved one by one
    /// according to unbalanced forces in the current drawing.
    ///
    /// If #FDP_RESTRICTED is specified, nodes are prevented from crossing edges.
    /// That is, this procedure maintains a given (almost) planar drawing.
    void  Layout_GEMDrawing(TOptFDP method=FDP_GEM,TFloat spacing=0.0) throw(ERRejected);

    /// \brief Refine a given layered drawing according to a model of forces
    ///
    /// \param method   Either #FDP_GEM or #FDP_RESTRICTED
    /// \param spacing  The minimum node distance or 0
    ///
    /// This refines a given layered drawing by modelling the nodes as loaded
    /// particles repelling each other, and the edge segments as elastic bands
    /// keeping its end nodes atop of each other. Node are moved horizontally
    /// one by one according to unbalanced forces in the current drawing.
    ///
    /// If #FDP_LAYERED_RESTR is specified, nodes are prevented from crossing
    /// other (bend) nodes in the same grid line. That is, this procedure
    /// maintains a given (almost) upward-planar drawing.
    void  Layout_LayeredFDP(TOptFDP method=FDP_LAYERED,TFloat spacing=0.0) throw(ERRejected);

    /// @}


    /// \addtogroup groupLayeredDrawing
    /// @{

public:

    /// \brief  Alternative rules for the horizontal node placement in layered tree drawing
    enum TOptAlign {
        ALIGN_OPTIMIZE = 0, ///< Apply the horizontal node placement code for general layered drawing
        ALIGN_LEFT = 1,     ///< Align nodes with the left-most child node
        ALIGN_CENTER = 2,   ///< Center nodes atop of its child node
        ALIGN_FDP = 3,      ///< As #ALIGN_CENTER, but refine by force directed placement
        ALIGN_RIGHT = 4     ///< Align nodes with the right-most child node
    };

    /// \brief Draw a rooted tree of predecessor arcs
    ///
    /// \param mode  A #TOptAlign value specifying the horizontal aligment rule
    /// \param dx    The minimum horizontal node distance or 0
    /// \param dy    The minimum vertical node distance or 0
    ///
    /// This draws the rooted tree which is provided by the predecessor labels.
    /// Non-tree arcs are also displayed, but not considered before the layout
    /// is fixed.
    ///
    /// Parent nodes are displayed atop of its child nodes, child nodes are
    /// displayed on the same level, and the order of child nodes from left to
    /// right reproduces the order in the incidence list of the father node.
    void  Layout_PredecessorTree(TOptAlign mode=ALIGN_OPTIMIZE,TFloat dx=0.0,TFloat dy=0.0) throw(ERRejected);

    /// \brief  Options for the layered drawing method
    ///
    /// LAYERED_FEEDBACK, LAYERED_EDGE_SPAN, LAYERED_COLOURS denote vertical node
    /// placement rules. LAYERED_FDP, LAYERED_ALIGN and LAYERED_SWEEP are vertical
    /// rules.
    enum TOptLayered {
        LAYERED_DEFAULT = -1,    ///< The hard-coded default set of rules (LAYERED_VERTICAL | LAYERED_HORIZONTAL)
        LAYERED_FEEDBACK = 1,    ///< Determine an implicit edge orientation and
                                 ///  save it to the edge colour register. If the
                                 ///  graph is undirected, this calls
                                 ///  ImplicitSTOrientation() with the default
                                 ///  source and target node. Otherwise,
                                 ///  FeedbackArcSet() is called
        LAYERED_EDGE_SPAN = 2,   ///< Call Layout_EdgeSpanMinimalLayering() to
                                 ///  assign layers to all graph nodes
        LAYERED_VERTICAL = 3,    ///< The default set of rules for vertical node placement
        LAYERED_COLOURS = 4,     ///< Fix all node with finite colour index to the equal
                                 ///  layer and call Layout_EdgeSpanMinimalLayering() to
                                 ///  assign layers to the remaining graph nodes
        LAYERED_FDP = 8,         ///< Call Layout_LayeredFDP() to improve the horizontal node placement
                                 ///  This overrides LAYERED_ALIGN
        LAYERED_ALIGN = 16,      ///< Call Layout_SetHorizontalCoordinates() to adjust nodes horizontally
        LAYERED_SWEEP = 32,      ///< Call Layout_SweepLayerByLayer() to reduce edge crossing
        LAYERED_HORIZONTAL = 48  ///< The default set of rules for horizontal node placement
    };

    /// \brief  Layered drawing of general graphs
    ///
    /// \param method  A combination of #TOptLayered options
    /// \param dx      The minimum horizontal node distance or 0
    /// \param dy      The minimum vertical node distance or 0
    ///
    /// This draws the graph in a grid such that the nodes in the same horizontal
    /// grid line form independent sets, edges have one control point on every
    /// crossed grid line and run top-down due to some implicit orientation.
    ///
    /// Four phases can be distinguished: In the first phase, the edges are
    /// oriented. In the second phase, the graph nodes are assigned to layers
    /// (horizontal grid lines). Then the edges are subdivided to obtain a
    /// proper layering. In the final phase, nodes are arranged in their layers
    /// with the aim of minimizing edge lengths and crossings.
    ///
    /// Depending on the specified method, only some of these phases are performed.
    /// The default rule will produce a legal drawing in any case. After that,
    /// special rules can be used for post-optimization.
    void  Layout_Layered(int method=LAYERED_DEFAULT,TFloat dx=0.0,TFloat dy=0.0) throw(ERRejected);

private:

    /// \brief  Assign vertical coordinates based on implicit arc orientations
    ///
    /// \param dy          The minimum vertical node distance
    /// \param nodeColour  An array of node colours or NULL
    ///
    /// This requires implicit arc orientations by the edge colour register.
    /// Based on this, nodes are assigned to layers such that edges run top-down
    /// and the total edge span is minimized.
    ///
    /// If nodeColour[] is given, the nodes with finite colour indices are
    /// fixed to the specified layer.
    bool  Layout_EdgeSpanMinimalLayering(TFloat dy,TNode* nodeColour=NULL) throw();

    /// \brief  Assign vertical coordinates based on implicit arc orientations
    ///
    /// \param dx  The minimum horizontal node distance
    /// \param dy  The minimum vertical node distance
    ///
    /// This requires a fixed node layer assignment by the node colour register.
    /// Based on this, nodes are drawn in the respective horizontal grid line.
    void  Layout_LayeringByColours(TFloat dx,TFloat dy) throw(ERRejected);

    /// \brief  Assign vertical coordinates based on implicit arc orientations
    ///
    /// \param dx  The minimum horizontal node distance
    /// \param dy  The minimum vertical node distance
    ///
    /// Starting with the first layer, refine the order of the nodes in the
    /// second layer in order to minimize the number of edge crossings between
    /// these two layers. Proceed from layer to layer with this 1-layer crossing
    /// minimization. When the final layer is reached, sweep back to the first
    /// layer. Repeat the procedure until no further improvement is achieved.
    /// This step is important for readable layouts; but it is also performance
    /// critical.
    void  Layout_SweepLayerByLayer(TFloat dx,TFloat dy) throw(ERRejected);

    /// \brief  Minimize the number of edge crossings between two adjacent horizontal grid lines
    ///
    /// \param fixedLayer     A node layer whose nodes have been moved in advance
    /// \param floatingLayer  The neighboring layer whose nodes can be moved
    ///
    /// This places the nodes in floatingLayer at the mean ordinate values of
    /// the neighbors in fixedLayer, and then calls Layout_CrossingLocalSearch().
    void  Layout_CrossingMinimization(indexSet<TNode>& fixedLayer,indexSet<TNode>& floatingLayer) throw();

    /// \brief  Reduce the number of edge crossings between two adjacent horizontal grid lines by local search
    ///
    /// \param fixedLayer     A node layer whose nodes have been moved in advance
    /// \param floatingLayer  The neighboring layer whose nodes can be moved
    /// \retval true          If the node order has been changed
    ///
    /// This sweeps along the floatingLayer grid line again and again. If this
    /// immediately reduces the number of crossings, two neighboring nodes are
    /// swapped.
    bool  Layout_CrossingLocalSearch(indexSet<TNode>& fixedLayer,indexSet<TNode>& floatingLayer) throw();

    /// \brief  Compute the crossing numbers between all node pairs in a given layer
    ///
    /// \param fixedLayer     The nodes in one grid line whose nodes have been moved in advance
    /// \param floatingLayer  The nodes in a neighboring grid line which shall be moved now
    /// \param cross          A hash table to store the number of edge crossing between node pairs
    ///
    /// For every pair u,v of nodes in floatingLayer, cross->key(u+n*v) denotes
    /// the number of crossings between edges connecting u or v and nodes in
    /// fixedLayer, provided that u would be placed left-hand of v.
    void  Layout_ComputeCrossingNumbers(indexSet<TNode>& fixedLayer,indexSet<TNode>& floatingLayer,
                goblinHashTable<TArc,TFloat>* cross) throw();

    /// \brief  Reassign horizontal coordinates without changing the relative order in the respective grid line
    ///
    /// \param dx  The minimum horizontal node distance
    ///
    /// For each edge segment, this minimizes absolute difference between its
    /// end node horizontal coordinates. Intermediate segments have higher
    /// priority than unbent arcs. That is, practically, at most two segments
    /// of any edge are non-vertical which improves the readability.
    void  Layout_SetHorizontalCoordinates(TFloat dx) throw();

    /// @}


    /// \addtogroup groupInvestigators
    /// @{

public:

    /// \brief  Allocate an investigator object for this graph
    ///
    /// \return  A pointer to the new investigator object
    ///
    /// Graphs work as factories for their own investigators. This is the factory
    /// method, and it is reimplemented in several subclasses.
    virtual investigator*  NewInvestigator() const throw();

    /// \brief  Provide a managed investigator object for this graph
    ///
    /// \return  The handle of an unused investigator object
    ///
    /// This utilizes an internal cache of investigator objects. If no unused
    /// investigator object is found in the cache, a new one is allocated and
    /// added to the cache. In either case, the handle of an investigator in
    /// this cache is returned.
    THandle  Investigate() const throw(ERRejected);

    /// \brief  Obtain direct access to a managed investigator object
    ///
    /// \param H  An investigator handle
    /// \return   A reference of the investigator object with handle H
    investigator &Investigator(THandle H) const throw(ERRejected);

    /// \brief  Reinitialize a managed graph search
    ///
    /// \param H  An investigator handle
    ///
    /// For the investigator indexed by H, this restarts the graph search. That is,
    /// the iterators of all incidence lists are positioned the First() arc.
    /// Practically, all arcs are marked unvisited.
    void  Reset(THandle H) const throw(ERRejected);

    /// \brief  Reinitialize a particular managed incidence list
    ///
    /// \param H  An investigator handle
    /// \param v  A node index ranged [0,1,..,n-1]
    ///
    /// For the investigator indexed by H, position the iterator for the incidence
    /// list of node v to the First(v) arc. Practically, all arcs are marked unvisited.
    void  Reset(THandle H, TNode v) const throw(ERRange,ERRejected);

    /// \brief  Read an arc from a managed node incidence list and mark it as visited
    ///
    /// \param H  An investigator handle
    /// \param v  A node index ranged [0,1,..,n-1]
    /// \return   An arc index ranged [0,1,..,2*mAct-1]
    ///
    /// For the investigator indexed by H, this returns the currently indexed arc
    /// a in the incidence list of node v, and then positions the hidden iterator
    /// for this incidence list to the arc Right(a,v). If all arcs with start
    /// node v have been visited, an ERRejected exception is raised.
    TArc  Read(THandle H, TNode v) const throw(ERRange,ERRejected);

    /// \brief  Return the currently indexed arc in a managed node incidence list
    ///
    /// \param H  An investigator handle
    /// \param v  A node index ranged [0,1,..,n-1]
    /// \return   An arc index ranged [0,1,..,2*mAct-1]
    ///
    /// For the investigator indexed by H, this returns the currently indexed
    /// arc in the incidence list of node v. The indexed arc does not change,
    /// that is, this arc is visited once more. If all arcs with start node
    /// v have been visited, an ERRejected exception is raised.
    TArc  Peek(THandle H, TNode v) const throw(ERRange,ERRejected);

    /// \brief  Check for unvisited arcs in a managed node incidence list
    ///
    /// \param H      An investigator handle
    /// \param v      A node index ranged [0,1,..,n-1]
    /// \retval true  There are unvisited arcs in the incidence list of v,
    ///               in the investigator with handle H
    bool  Active(THandle H, TNode v) const throw(ERRange,ERRejected);

    /// \brief  Conclude a managed graph search
    ///
    /// \param H      An investigator handle
    ///
    /// This concludes a graph search started with Investigate(). The investigator
    /// with handle H is returned to the pool of available investigator objects.
    void  Close(THandle H) const throw(ERRejected);

    /// \brief  Disallocate all managed investigator objects
    void  ReleaseInvestigators() const throw();

    /// \brief  Compress the list of managed investigator objects
    void  StripInvestigators() const throw();

    /// @}


    /// \addtogroup subgraphManagement
    /// @{

public:

    /// \brief Set all subgraph multiplicities to the lower capacity bounds
    ///
    /// This also releases the node degree information
    void  InitSubgraph() throw();

    /// \brief Set the subgraph multiplicity of a given arc
    ///
    /// \param a       An arc index ranged [0,1,..,2*mAct-1]
    /// \param lambda  The desired multiplicity of a
    virtual void  SetSub(TArc a,TFloat lambda) throw(ERRange,ERRejected);

    /// \brief Adjust the subgraph multiplicity of a given arc
    ///
    /// \param a       An arc index ranged [0,1,..,2*mAct-1]
    /// \param lambda  The desired change of the multiplicity of a
    ///
    /// This increases or decreases the multiplicity of the arc a by the amount
    /// of lambda.
    virtual void  SetSubRelative(TArc a,TFloat lambda) throw(ERRange);

    /// \brief  Increase or decrease the flow value of the arc a by an amount Lambda
    ///
    /// \param a       An arc index ranged [0,1,..,2*mAct-1]
    /// \param lambda  An amount of flow
    ///
    /// This increases or decreases the multiplicity of the arc a by the amount
    /// of lambda. Other than SetSubRelative(), the orientation bit of a determines
    /// whether the multiplicity is increased or decreased.
    virtual void  Push(TArc a,TFloat lambda) throw(ERRange,ERRejected);

    /// \brief Retrieve the subgraph multiplicity of a given arc
    ///
    /// \param a      An arc index ranged [0,1,..,2*mAct-1]
    /// \return       The multiplicity of a
    virtual TFloat  Sub(TArc a) const throw(ERRange) = 0;

    /// \brief Retrieve the residual capacity of a given arc
    ///
    /// \param a      An arc index ranged [0,1,..,2*mAct-1]
    /// \return       The residual capacity of a
    ///
    /// This returns the possible change of the multiplicity of the arc a,
    /// depending on the orientation bit of a.
    TFloat  ResCap(TArc a) const throw(ERRange);

    /// \brief Cardinality of the current subgraph
    ///
    /// \return The sum of arc multiplicities
    TCap  Cardinality() const throw();

    /// \brief Total length of the current predecessor tree
    ///
    /// \return The sum of predecessor arc lengths
    TFloat  Length() const throw();

    /// \brief Total weight of the current subgraph
    ///
    /// \return The weighted sum of subgraph edge lengths
    TFloat  Weight() const throw();

    /// \brief Export predecessor arcs to the subgraph multiplicities
    ///
    /// \param v  A node index ranged [0,1,..,n-1] or NoNode
    ///
    /// If v is undefined, the subgraph multiplicity of all predecessor arcs
    /// is set to its upper capacity bounds. If a node v is specified, the
    /// operation is restricted to the predecessor path ending at v.
    void  AddToSubgraph(TNode v = NoNode) throw(ERRange,ERRejected);

    /// @}


    /// \addtogroup nodePartitions
    /// @{

public:

    /// \brief  Initialize the disjoint node set structure
    ///
    /// If not already present, the disjoint node set structure is allocated.
    /// After this operation, it is Find(v)==NoNode for every node v.
    virtual void  InitPartition() throw();

    /// \brief  Start a single node set
    ///
    /// \param v  A node index ranged [0,1,..,n-1]
    ///
    /// After this operation, it is Find(v)==v.
    virtual void  Bud(TNode v) throw(ERRange);

    /// \brief  Merge two disjoint node sets represented by arbitrary members
    ///
    /// \param u  A node index ranged [0,1,..,n-1]
    /// \param v  A node index ranged [0,1,..,n-1]
    ///
    /// After this operation, it is Find(u)==Find(v).
    virtual void  Merge(TNode u,TNode v) throw(ERRange);

    /// \brief  Retrieve the set containing a given node
    ///
    /// \param v  A node index ranged [0,1,..,n-1]
    /// \return   A node index ranged [0,1,..,n-1] or NoNode
    ///
    /// This returns the index of an arbitrary but fixed node in the same set,
    /// the so-called canonical element in this set. Only when sets are merged,
    /// the canonical element changes.
    ///
    /// In order to check if two nodes x,y are in the same set, it is sufficient
    /// to evaluate Find(x)==Find(y) and Find(u)!=NoNode. If the context parameter
    /// methDSU is set, Find() does not only lookup the canonical element, but
    /// also performs some path compression to reduce the future lookup times.
    virtual TNode  Find(TNode v) throw(ERRange);

    /// \brief  Release the disjoint set structure
    ///
    /// After this operation, it is Find(v)==v for every node v, and the
    /// disjoint node set structure is deallocated.
    virtual void  ReleasePartition() throw();

    /// @}


    /// \addtogroup distanceLabels
    /// @{

public:

    /// \brief  Initialize the distance labels
    ///
    /// \param def  A default value
    /// \return     A pointer to the array of distance labels
    ///
    /// If not already present, the distance label register is allocated.
    /// In any case, all distance labels are set to a default value.
    TFloat*  InitDistanceLabels(TFloat def = InfFloat) throw();

    /// \brief  Get access to the distance labels
    ///
    /// \return  A pointer to the array of distance labels or NULL
    ///
    /// Other than InitDistanceLabels() and RawDistanceLabels(), this operation
    /// does not allocate distance labels if those do not exist.
    TFloat*  GetDistanceLabels() const throw();

    /// \brief  Ensure existence of the distance labels
    ///
    /// \return  A pointer to the array of distance labels
    ///
    /// If not already present, the distance label register is allocated. Other
    /// than InitDistanceLabels(), this does not initialize the array element values.
    TFloat*  RawDistanceLabels() throw();

    /// \brief  Retrieve the distance label of a given node
    ///
    /// \param v   A node index in the range [0,1..,n-1]
    /// \return    The distance label of node v
    ///
    /// This operation is little efficient if it is called for several
    /// nodes due to the attribute lookup operations which occur.
    virtual TFloat  Dist(TNode v) const throw(ERRange);

    /// \brief  Assign a distance label to a given node
    ///
    /// \param v         A node index in the range [0,1..,n-1]
    /// \param thisDist  The desired distance value for v
    ///
    /// If not already present and if thisDist != InfFloat, the distance label
    /// register is allocated. In any case, InfFloat is assigned
    /// as the distance label of v.
    ///
    /// This operation is little efficient if it is called for several nodes
    /// due to the attribute lookup operations which occur.
    void  SetDist(TNode v,TFloat thisDist) throw(ERRange);

    /// \brief  Release the distance labels from memory
    ///
    /// This implicitly sets all distance labels to InfFloat.
    void  ReleaseLabels() throw();

    /// @}


    /// \addtogroup nodePotentials
    /// @{

public:

    /// \brief  Initialize the node potentials
    ///
    /// \param def  A default value
    /// \return     A pointer to the array of node potentials
    ///
    /// If not already present, the node potential register is allocated.
    /// In any case, all node potentials are set to a default value.
    TFloat*  InitPotentials(TFloat def = 0) throw();

    /// \brief  Get access to the node potentials
    ///
    /// \return  A pointer to the array of node potentials or NULL
    ///
    /// Other than InitPotentials() and RawPotentials(), this operation
    /// does not allocate node potentials if those do not exist.
    TFloat*  GetPotentials() const throw();

    /// \brief  Ensure existence of the node potentials
    ///
    /// \return  A pointer to the array of node potentials
    ///
    /// If not already present, the node potential register is allocated. Other
    /// than InitNodePotentials(), this does not initialize the array element values.
    TFloat*  RawPotentials() throw();

    /// \brief  Retrieve the potential of a given node
    ///
    /// \param v   A node index in the range [0,1..,n-1]
    /// \return    The potential of node v
    ///
    /// This operation is little efficient if it is called for several
    /// nodes due to the attribute lookup operations which occur.
    TFloat  Pi(TNode v) const throw(ERRange);

    /// \brief  Assign a potential to a given node
    ///
    /// \param v       A node index in the range [0,1..,n-1]
    /// \param thisPi  The desired potential value for v
    ///
    /// If not already present and if thisPi != 0, the node potential
    /// register is allocated. In any case, thisPi is assigned
    /// as the potential of v.
    ///
    /// This operation is little efficient if it is called for several nodes
    /// due to the attribute lookup operations which occur.
    void  SetPotential(TNode v,TFloat thisPi) throw(ERRange);

    /// \brief  Change the potential of a given node by a given amount
    ///
    /// \param v        A node index in the range [0,1..,n-1]
    /// \param epsilon  The amount by which the potential of v changes
    ///
    /// This operation is little efficient if it is called for several nodes
    /// due to the attribute lookup operations which occur.
    void  PushPotential(TNode v,TFloat epsilon) throw(ERRange);

    /// \brief  Adjust the node potentials with the result of a shortest path solver
    ///
    /// \param cutValue  The maximum distance value
    ///
    /// This increases each node potential by the distance label, provided that
    /// the distance label is less than the cutValue. This procedure is used to
    /// determine optimal node potentials for a given optimal flow.
    void  UpdatePotentials(TFloat cutValue) throw(ERRejected);

    /// \brief  Release the node potentials from memory
    ///
    /// This implicitly sets all node potentials to zero.
    void  ReleasePotentials() throw();

    /// @}


    /// \addtogroup nodeColours
    /// @{

public:

    /// \brief  Initialize the node colours
    ///
    /// \param def  A default value
    /// \return     A pointer to the array of node colours
    ///
    /// If not already present, the node colour register is allocated.
    /// In any case, all node colours are set to a default value.
    TNode*  InitNodeColours(TNode def = NoNode) throw();

    /// \brief  Get access to the node colours
    ///
    /// \return  A pointer to the array of node colours or NULL
    ///
    /// Other than InitNodeColours() and RawNodeColours(), this operation
    /// does not allocate node colours if those do not exist.
    TNode*  GetNodeColours() const throw();

    /// \brief  Ensure existence of the node colours
    ///
    /// \return  A pointer to the array of node colours
    ///
    /// If not already present, the node colour register is allocated. Other
    /// than InitNodeColours(), this does not initialize the array element values.
    TNode*  RawNodeColours() throw();

    /// \brief  Assign the node colours with a random permutation
    ///
    /// \return  A pointer to the array of node colours
    ///
    /// If not already present, the node colour register is allocated. Then, a
    /// random permutation of the sequence 0,1,..,n-1 is assigned.
    TNode*  RandomNodeOrder() throw();

    /// \brief  Retrieve the colour of a given node
    ///
    /// \param v   A node index in the range [0,1..,n-1]
    /// \return    A colour index ranged [0,1,..,n-1] or NoNode
    ///
    /// This operation is little efficient if it is called for several
    /// nodes due to the attribute lookup operations which occur.
    virtual TNode  NodeColour(TNode v) const throw(ERRange);

    /// \brief  Assign a colour to a given node
    ///
    /// \param v           A node index in the range [0,1..,n-1]
    /// \param thisColour  A colour index ranged [0,1,..,n-1] or NoNode
    ///
    /// If not already present and if thisColour != NoNode, the node colour
    /// register is allocated. In any case, thisColour is assigned
    /// as the colour of v.
    ///
    /// This operation is little efficient if it is called for several nodes
    /// due to the attribute lookup operations which occur.
    void  SetNodeColour(TNode v,TNode thisColour) throw(ERRange);

    /// \brief  Release the node colours from memory
    ///
    /// This implicitly sets all node colours to NoNode.
    void  ReleaseNodeColours() throw();

    /// \brief  Extract an edge cut from the distance labels and save it to the node colours
    ///
    /// This separates the nodes with finite distance labels from the nodes with
    /// distance label InfFloat.
    void  ExtractCut() throw(ERRejected);

    /// \brief  Extract a bipartition from the distance labels and save it to the node colours
    ///
    /// This separates the nodes with even distance labels (including InfFloat)
    /// from the nodes with odd distance label InfFloat.
    void  ExtractBipartition() throw(ERRejected);

    /// \brief  Convert a partition of the node set from the disjoint set family to the node colours
    ///
    /// This assigns colours to the nodes such that nodes u, v with
    /// Find(u)==Find(v) are equally coloured.
    void  ExtractColours() throw(ERRejected);

    /// @}


    /// \addtogroup edgeColours
    /// @{

public:

    /// \brief  Initialize the edge colours
    ///
    /// \param def  A default value
    /// \return     A pointer to the array of edge colours
    ///
    /// If not already present, the edge colour register is allocated.
    /// In any case, all edge colours are set to a default value.
    TArc*  InitEdgeColours(TArc def = NoArc) throw();

    /// \brief  Get access to the edge colours
    ///
    /// \return  A pointer to the array of edge colours or NULL
    ///
    /// Other than InitEdgeColours() and RawEdgeColours(), this operation
    /// does not allocate edge colours if those do not exist.
    TArc*  GetEdgeColours() const throw();

    /// \brief  Ensure existence of the edge colours
    ///
    /// \return  A pointer to the array of edge colours
    ///
    /// If not already present, the edge colour register is allocated. Other
    /// than InitEdgeColours(), this does not initialize the array element values.
    TArc*  RawEdgeColours() throw();

    /// \brief  Retrieve a colour of a given edge
    ///
    /// \param a   An arc index in the range [0,1..,2*m-1]
    /// \return    A colour index ranged [0,1,..,m-1] or NoArc
    ///
    /// This operation is little efficient if it is called for several edges
    /// due to the attribute lookup operations which occur.
    TArc  EdgeColour(TArc a) const throw(ERRange);

    /// \brief  Assign a colour to a given edge
    ///
    /// \param a           An arc index in the range [0,1..,2*m-1]
    /// \param thisColour  An arc index ranged [0,1,..,m-1] or NoArc
    ///
    /// If not already present and if thisColour != NoArc, the edge colour
    /// register is allocated. In any case, thisColour is assigned as the
    /// colour of a. Note that the colours of reverse arcs a and (a^1) are
    /// the same.
    ///
    /// This operation is little efficient if it is called for several nodes
    /// due to the attribute lookup operations which occur.
    void  SetEdgeColour(TArc a,TArc thisColour) throw(ERRange);

    /// \brief  Release the edge colours from memory
    ///
    /// This implicitly sets all edge colours to NoArc.
    void  ReleaseEdgeColours() throw();

    /// @}


    /// \addtogroup planarRepresentation
    /// @{

public:

    /// \brief  Method options for ExtractEmbedding()
    enum TOptExtractEmbedding {
        PLANEXT_DEFAULT = 0, ///< Assign a left-hand face index to every arc
        PLANEXT_GROW    = 1, ///< Manipulate the incidence order to achieve as many exterior nodes as possible
        PLANEXT_DUAL    = 2, ///< Generate the dual graph explicitly
        PLANEXT_CONNECT = 3  ///< Minimal augmentation to a planar connected graph
    };

    /// \brief Check if the current node incidence ordering constitutes a planar representation
    ///
    /// \param option   Specifies additional operations on the found dual representation
    /// \param pArg     Occasionally, computational results are exported by this pointer
    /// \retval NoNode  The current node incidence ordering does not encode a planar representation
    /// \return         The number of faces (as in the Euler formula, but only if the graph is connected)
    ///
    /// In the case of option=PLANEXT_DEFAULT, the left-hand face indices can be
    /// exported to a TNode[m-n+2] array. In the case of option=PLANEXT_DUAL, a
    /// pointer to an empty graph must be passed, which is then filled with the
    /// dual incidence structure. The other options manipulate the original graph.
    TNode  ExtractEmbedding(TOptExtractEmbedding option = PLANEXT_DEFAULT,void* pArg = NULL) throw(ERRejected);

protected:

    /// \brief  Assign an exterior arc in the attribute pool
    ///
    /// \param a   An arc index in the range [0,1..,2m-1] or NoArc
    void  SetExteriorArc(TArc a) throw(ERRange,ERRejected);

public:

    /// \brief  Adjust the First() incidences according to an exterior arc
    ///
    /// \param a   An arc index in the range [0,1..,2m-1] or NoArc
    ///
    /// If the arc a is defined, loop around the left-hand face of a and adjust
    /// the First() incidences so that these arcs defined the counter-clockwise
    /// cycle
    void  MarkExteriorFace(TArc a) throw(ERRange,ERRejected);

    /// \brief  Retrieve the left-hand face index of a given arc
    ///
    /// \param a   An arc index in the range [0,1..,2m-1]
    /// \return    A node index in the range [0,1..,n-1] or NoNode
    TNode  Face(TArc a) throw(ERRange);

    /// \brief  Retrieve an exterior arc
    ///
    /// \return  An arc index in the range [0,1,..,2m-1] or NoArc
    TArc  ExteriorArc() const throw();

    /// \brief  Retrieve the left-hand face index of a given arc
    ///
    /// \param v         A node index in the range [0,1..,n-1]
    /// \param thisFace  A face index in the range [0,1..,ND()-1] or NoNode
    /// \retval true     If thisFace is exterior and v is on thisFace
    ///
    /// If thisFace==NoNode, the procedure indeed checks whether v is an exterior
    /// node. This works correctly if MarkExteriorFace() has been called in advance.
    ///
    /// If thisFace!=NoNode, the check runs on this face. And the First()
    /// indicces must be set accordingly.
    bool  ExteriorNode(TNode v,TNode thisFace=NoNode) const throw(ERRange);

    /// \brief  Release the mapping of arcs to left-hand face indices
    void  ReleaseEmbedding() throw();

    /// @}


    /// \addtogroup predecessorLabels
    /// @{

public:

    /// \brief  Initialize the node predecessor labels
    ///
    /// \return  A pointer to the array of predecessor labels
    ///
    /// If not already present, the node predecessor label data structure is
    /// allocated. In any case, all predecessor labels are set to NoArc.
    TArc*  InitPredecessors() throw();

    /// \brief  Get access to the node predecessor labels
    ///
    /// \return  A pointer to the array of predecessor labels or NULL
    ///
    /// Other than InitPredecessors() and RawPredecessors(), this operation
    /// does not allocate predecessor labels if those do not exist.
    TArc*  GetPredecessors() const throw();

    /// \brief  Ensure existence of the node predecessor labels
    ///
    /// \return  A pointer to the array of predecessor labels
    ///
    /// If not already present, the node predecessor label data structure is
    /// allocated. Other than InitPredecessors(), this does not initialize
    /// the array element values.
    TArc*  RawPredecessors() throw();

    /// \brief  Retrieve a predecessor label of a given node
    ///
    /// \param v   A node index in the range [0,1..,n-1]
    /// \return    An arc index ranged [0,1,..,2*mAct-1] or NoArc
    ///
    /// This operation is little efficient if it is called for several
    /// nodes due to the attribute lookup operations which occur.
    TArc  Pred(TNode v) const throw(ERRange);

    /// \brief  Assign a predecessor label to a given node
    ///
    /// \param v         A node index in the range [0,1..,n-1]
    /// \param thisPred  An arc index ranged [0,1,..,2*mAct-1] or NoArc
    ///
    /// If not already present and if thisPred != NoArc, the node predecessor
    /// label data structure is allocated. In any case, thisPred is assigned
    /// as the predecessor label of v.
    ///
    /// This operation is little efficient if it is called for several nodes
    /// due to the attribute lookup operations which occur.
    void  SetPred(TNode v,TArc thisPred) throw(ERRange,ERRejected);

    /// \brief  Release the node predecessor labels from memory
    ///
    /// This implicitly sets all node predecessor labels to NoArc.
    void  ReleasePredecessors() throw();

    /// \brief  Check if the current subgraph forms a forest and convert it to the predecessor labels
    ///
    /// \return  The number of connected subgraph components or NoNode
    ///
    /// This examines all subgraph arcs. If the subgraph contains cycles,
    /// NoNode is returned. Otherwise, for every subgraph component a rooted
    /// tree is exported to the predecessor labels.
    TNode  ExtractTrees() throw();

    /// \brief  Convert a subgraph path to the predecessor labels
    ///
    /// \param u   A node index in the range [0,1..,n-1]
    /// \param v   A node index in the range [0,1..,n-1]
    /// \return    The uv-path length or NoNode
    ///
    /// This searches the subgraph component of the node u which is assumed to
    /// form a uv-path or, if u==v, a cycle. In the regular case, the predecessor
    /// labels are partially assigned with a subgraph path or cycle, directed from
    /// u to v. If the sugraph component is not a simple path, the procedure does
    /// or does not return prematurely with NoNode.
    TNode  ExtractPath(TNode u,TNode v) throw();

    /// \brief  Check if the current subgraph forms a 2-factor and convert it to the predecessor labels
    ///
    /// \return  The number of cycles or NoNode
    ///
    /// This examines all subgraph arcs. All nodes must be incident with exactly
    /// two subgraph edges. If not, NoNode is returned. Otherwise ExtractPath()
    /// is called once for every cycle to do the actual conversion.
    TNode  ExtractCycles() throw();


    /// \brief  Check if the current subgraph forms a 1-matching and convert it to the predecessor labels
    ///
    /// \return  The matching cardinality or NoNode
    ///
    /// This examines the subgraph. If adjacent subgraph edges are found, NoNode
    /// is returned. Otherwise, for every subgraph edge, two antiparallel
    /// predecessor arca are generated.
    TNode  Extract1Matching() throw();


    /// \brief  Extract an edge cover from a 1-matching subgraph and save it to the predecessor labels
    ///
    /// \return  The edge cover cardinality or NoNode
    ///
    /// This examines the subgraph. If adjacent subgraph edges are found, NoNode
    /// is returned. Otherwise, for every subgraph edge, two antiparallel
    /// predecessor arca are generated. For the unmatched nodes, arbitrary
    /// predecessor arcs are assigned or, if there are isolated nodes, NoNode
    /// is returned.
    TNode  ExtractEdgeCover() throw();

    /// @}


    /// \addtogroup degreeLabels
    /// @{

public:

    /// \brief  Initialize the undirected degree labels
    ///
    /// For every node, this sums up the subgraph (!) multiplicities of all
    /// incident undirected edges. Directed arcs are not counted. The results
    /// are available by the method Deg().
    void  InitDegrees() throw();

    /// \brief  Initialize the directed degree labels
    ///
    /// For every node, this sums up the subgraph (!) multiplicities of all
    /// incident directed arcs. Undirected edges are not counted. The results
    /// are available by the methods DegIn() and DegOut() where DegIn()
    /// [DegOut()] counts the arcs which are blocking in the leaving [entering]
    /// direction.
    void  InitDegInOut() throw();

    /// \brief  Retrieve a subgraph node degree
    ///
    /// \param v  A node index in the range [0,1..,n-1]
    /// \return   The sum of subgraph multiplicities, counting all incident undirected edges
    TFloat  Deg(TNode v) throw(ERRange);

    /// \brief  Retrieve a subgraph node in-degree
    ///
    /// \param v  A node index in the range [0,1..,n-1]
    /// \return   The sum of subgraph multiplicities, counting all arcs entering v
    TFloat  DegIn(TNode v) throw(ERRange);

    /// \brief  Retrieve a subgraph node out-degree
    ///
    /// \param v  A node index in the range [0,1..,n-1]
    /// \return   The sum of subgraph multiplicities, counting all arcs leaving v
    TFloat  DegOut(TNode v) throw(ERRange);

    /// \brief  Retrieve a subgraph node imbalance
    ///
    /// \param v  A node index in the range [0,1..,n-1]
    /// \return   DegIn(v)-DegOut(v)
    inline TFloat  Divergence(TNode v) throw(ERRange);

    /// \brief  Deallocate all node degree labels (directed and undirected)
    void  ReleaseDegrees() throw();

protected:

    /// \brief  Update the degree labels when a subgraph multiplicity changes
    ///
    /// \param a       An node index in the range [0,1..,2m-1]
    /// \param lambda  The amount of shift
    ///
    /// This is internally called whenever the subgraph multiplicity of a changes,
    /// including the case of deleting the edge. If the multiplicity decreases by
    /// an amount lambda, the call AdjustDegrees(a,-lambda) updates the undirected
    /// or the directed degree labels of both end nodes, whatever applies.

    void  AdjustDegrees(TArc a,TFloat lambda) throw(ERRange);

    /// @}


    /// \addtogroup groupObjectExport
    /// @{

public:

    /// \brief  Write this graph object to a file in a native format
    ///
    /// \param fileName  The destination file name
    virtual void  Write(const char* fileName) const throw(ERFile);

    // This method is documented in class goblinRootObject
    void  WriteSpecial(goblinExport& F,const attributePool& pool,TPoolEnum token) const throw();

    virtual void  WriteIncidences(goblinExport* F) const throw();

    /// \brief  Write the upper capacity bounds to file
    ///
    /// \param F  An output file stream
    void  WriteUCap(goblinExport* F) const throw();

    /// \brief  Write the lower capacity bounds to file
    ///
    /// \param F  An output file stream
    void  WriteLCap(goblinExport* F) const throw();

    /// \brief  Write the arc length labsls to file
    ///
    /// \param F  An output file stream
    void  WriteLength(goblinExport* F) const throw();

    /// \brief  Write the node demands to file
    ///
    /// \param F  An output file stream
    void  WriteDemand(goblinExport* F) const throw();

    /// \brief  Write the arc orientations to file
    ///
    /// \param F  An output file stream
    void  WriteOrientation(goblinExport* F) const throw();

    /// \brief  Write all geometry pool data to file
    ///
    /// \param F  An output file stream
    void  WriteGeometry(goblinExport* F) const throw();

    /// \brief  Write all layout pool data to file
    ///
    /// \param F  An output file stream
    void  WriteLayout(goblinExport* F) const throw();

    /// \brief  Write all registers to file
    ///
    /// \param F  An output file stream
    void  WriteRegisters(goblinExport* F) const throw();

    /// \brief  Write a single register to file
    ///
    /// \param F      An output file stream
    /// \param token  A TOptRegTokens value (register pool index)
    void  WriteRegister(goblinExport& F,TPoolEnum token) const throw(ERRange);

    /// \brief  Write the subgraph multiplicities to file
    ///
    /// \param F  An output file stream
    void  WriteSubgraph(goblinExport& F) const throw();

    /// @}


    /// \addtogroup objectImport
    /// @{

public:

    // This method is documented in class goblinRootObject
    void  ReadSpecial(goblinImport& F,attributePool& pool,TPoolEnum token) throw(ERParse);

protected:

    /// \brief  Read a complete graph object from file
    ///
    /// \param F  An input file stream
    ///
    /// This reads anything from the file up to the class name which has been
    /// read in advance.
    void  ReadAllData(goblinImport& F) throw(ERParse);

    /// \brief  Read the number of nodes from file
    ///
    /// \param F  An input file stream
    ///
    /// This reads the number of graph nodes, the number of layout points and,
    /// if the graph is bipartite, the number of left-hand nodes. This operation
    /// must occur prior to all attribute read attempts.
    virtual void  ReadNNodes(goblinImport& F) throw(ERParse);

    /// \brief  Read the number of arcs from file
    ///
    /// \param F  An input file stream
    ///
    /// This operation must occur prior to all attribute read attempts.
    void  ReadNArcs(goblinImport& F) throw(ERParse);

    /// \brief  Read all representational data from file
    ///
    /// \param F  An input file stream
    void  ReadRepresentation(goblinImport& F) throw(ERParse);

    /// \brief  Read all geometry pool data from file
    ///
    /// \param F  An input file stream
    void  ReadGeometry(goblinImport& F) throw(ERParse);

    /// \brief  Read all registers from file
    ///
    /// \param F  An input file stream
    void  ReadRegisters(goblinImport& F) throw(ERParse);

    /// \brief  Read the subgraph multiplicities from file
    ///
    /// \param F  An input file stream
    void  ReadSubgraph(goblinImport& F) throw(ERParse);

    /// \brief  Read all layout pool data from file
    ///
    /// \param F  An input file stream
    void  ReadLayoutData(goblinImport& F) throw(ERParse);

    /// @}


    /// \addtogroup shortestPath
    /// @{

public:

    /// \brief  Alternative methods for shortest path solver
    enum TMethSPX {
        SPX_DEFAULT  = -1,  ///< Apply the default method set in #goblinController::methSPX
        SPX_FIFO     =  0,  ///< Apply an improved label correcting method
        SPX_DIJKSTRA =  1,  ///< Apply the Dijkstra label setting method
        SPX_BELLMAN  =  2,  ///< Apply the Bellman label correcting method
        SPX_BFS      =  3,  ///< Apply breadth first search
        SPX_DAG      =  4,  ///< Apply a method for directed acyclic graphs
        SPX_TJOIN    =  5   ///< Apply the T-join method for undirected graphs
    };

    /// \brief  Options for implicit modification of the arc length labels
    enum TOptSPX {
        SPX_ORIGINAL = 0,   ///< Apply the original length labels
        SPX_REDUCED  = 1    ///< Apply the reduced length labels
    };

    /// \brief  Compute a shortest path or a shortest path tree by using the default method
    ///
    /// \param source The node where the search tree is rooted at
    /// \param target An optional target node
    /// \retval true  The target node could be reached
    ///
    /// Depending on the selected algorithm and whether a target node is specified,
    /// this method generates a shortest path between two nodes or a tree rooted
    /// at a given node.
    bool  ShortestPath(TNode source,TNode target = NoNode) throw(ERRange,ERRejected);

    /// \brief  Compute a shortest path or a shortest path tree using a specified method
    ///
    /// \param method          A #TMethSPX value specifying the applied method
    /// \param characteristic  A #TOptSPX value for modifying the searched graph
    /// \param EligibleArcs    An index set of eligible arcs
    /// \param source          The node where the search tree is rooted at
    /// \param target          An optional target node
    /// \retval true           The target node could be reached
    ///
    /// Depending on the selected algorithm and whether a target node is specified,
    /// this method generates a shortest path between two nodes or a tree rooted
    /// at a given node.
    bool  ShortestPath(TMethSPX method,TOptSPX characteristic,const indexSet<TArc>& EligibleArcs,
                        TNode source,TNode target = NoNode) throw(ERRange,ERRejected);

    /// \brief  Determine the discrete Voronoi regions
    ///
    /// \param Terminals  The index set of terminal nodes
    /// \return           The number of terminal node
    ///
    /// This requires a connected graph with non-negative edge lengths. The node
    /// set is partitioned such that every node set contains a single terminal
    /// node. The other nodes a put into the set of the terminal, from which the
    /// minimum distance is achieved (with some arbitrary tie-braking).
    /// The partition data structure is exported, and predecessor labels encoding
    /// a minimum distance path from a terminal node.
    TNode  VoronoiRegions(const indexSet<TNode>& Terminals) throw(ERRejected);

    /// \brief  Solve Bellman's equations or return a negative length cycle
    ///
    /// \param characteristic  A #TOptSPX value
    /// \param EligibleArcs    An index set of eligible arcs
    /// \param source          An optional source node
    /// \param epsilon         A constant to be added on the length of each arc
    /// \return                A node on the negative-length cycle or NoNode
    ///
    /// If the graph contains a negative length eligible cycle, such a cycle is
    /// exported by the predecessor labels, and one of the nodes on this cycle
    /// is returned. If s is defined, the negative length cycle must consist
    /// of nodes which can be reached from s.
    ///
    /// If no such cycle is found, for every node u (reachable from s), Bellman's equation
    ///    Dist(u) == min{Dist(v)+Length(v,u) : vu is an eligible arc}
    /// is solved. Supposed that the graph is strongly connected, one degree of
    /// freedom is left. If s is defined, this is fixed by setting Dist(s) := 0,
    /// and a predecessor tree rooted at s is returned.
    TNode  NegativeCycle(TOptSPX characteristic,const indexSet<TArc>& EligibleArcs,
            TNode source=NoNode,TFloat epsilon=0) throw(ERRange);

    /// \brief  Perform a breadth first search
    ///
    /// \param EligibleArcs  An index set of eligible arcs
    /// \param S             An index set of source nodes
    /// \param T             An index set of target nodes
    /// \return              The first-reached target node index or NoNode
    ///
    /// This determines a predecessor tree or forest - in case of multiple source
    /// nodes in S. If T is an empty set, every node is reached from an arbitrary
    /// source node on a path with a minimum number of eligible arcs. But the first
    /// time when a target node in T is reached, the procedure exits prematurely.
    TNode  BFS(const indexSet<TArc>& EligibleArcs,const indexSet<TNode>& S,const indexSet<TNode>& T) throw();

protected:

    /// \brief  Perform the Dijstra shortest-path search
    ///
    /// \param characteristic  A #TOptSPX value
    /// \param EligibleArcs    An index set of eligible arcs
    /// \param S               An index set of source nodes
    /// \param T               An index set of target nodes
    /// \return                The first-reached target node index or NoNode
    ///
    /// This is an implementation of the famous Dikstra algorithm which has been
    /// somewhat modified to allow for multiple source nodes. Non-negative arc
    /// lengths are required. The first time when a target node is reached, this
    /// node is reached by a shortest path, and the procedure is exited prematurely.
    TNode  SPX_Dijkstra(TOptSPX characteristic,const indexSet<TArc>& EligibleArcs,
                const indexSet<TNode>& S,const indexSet<TNode>& T) throw(ERRange,ERRejected);

    /// \brief  Perform the FIFO label correcting shortest-path search
    ///
    /// \param characteristic  A #TOptSPX value
    /// \param EligibleArcs    An index set of eligible arcs
    /// \param s               A source node
    /// \param t               A target node
    /// \retval true           If t is reachable from s
    ///
    /// This is only a wrapper for NegativeCycle().
    bool  SPX_FIFOLabelCorrecting(TOptSPX characteristic,const indexSet<TArc>& EligibleArcs,
                TNode s,TNode t=NoNode) throw(ERRange,ERCheck);

    /// \brief  Perform the Bellman-Ford label correcting shortest-path search
    ///
    /// \param characteristic  A #TOptSPX value
    /// \param EligibleArcs    An index set of eligible arcs
    /// \param s               A source node
    /// \param t               A target node
    /// \retval true           If t is reachable from s
    ///
    /// This implements the original Bellman-Ford label correcting algorithm.
    /// The procedure is similar but less efficient than SPX_FIFOLabelCorrecting().
    bool  SPX_BellmanFord(TOptSPX characteristic,const indexSet<TArc>& EligibleArcs,TNode s,TNode t=NoNode)
                throw(ERRange,ERCheck);

    /// @}


    /// \addtogroup groupDirectedAcyclic
    /// @{

public:

    /// \brief  Alternative options for the DAG search procedure
    enum TOptDAGSearch {
        DAG_TOPSORT  = 0,  ///< Determine a topological numbering
        DAG_CRITICAL = 1,  ///< Determine a forest of critical paths
        DAG_SPTREE   = 2   ///< Determine a shortest path tree or forest
    };

    /// \brief  Perform a DAG search
    ///
    /// \param opt           The goal of this graph search
    /// \param EligibleArcs  The set of eligible arcs
    /// \param s             For DAG_SPTREE: The source node (optional)
    /// \param t             For DAG_SPTREE: The target node (optional)
    /// \return              A node index ranged [0,1,..,n-1]
    ///
    /// If the graph is a DAG and opt==DAG_TOPSORT or opt==DAG_SPTREE, the
    /// ordinal numbers of a topological numbering are provided by the node
    /// colour register. For opt==DAG_SPTREE and opt==DAG_CRITICAL, the distance
    /// and the predecessor label registers are assigned.
    TNode  DAGSearch(TOptDAGSearch opt,const indexSet<TArc>& EligibleArcs,TNode s=NoNode,TNode t=NoNode) throw(ERRange);

    /// @}


    /// \addtogroup groupComponents
    /// @{

public:

    /// \brief Colour indices used to specify node and edge cuts
    enum {
        CONN_CUT_NODE   = 0, ///< Specifies a cut node
        CONN_LEFT_HAND  = 1, ///< Specifies a left-hand node
        CONN_RIGHT_HAND = 2  ///< Specifies a right-hand node
    };

    /// \brief  Connectivity test
    ///
    /// \retval true   The graph is connected
    ///
    /// Assigns node colours such that equally coloured nodes are in the same
    /// connected component. The predecessor labels encode a DFS tree / forest
    bool  Connected() throw();

    /// \brief  Test for k-connectivity
    ///
    /// \param k      The desired degree of connectivity
    /// \retval true  The graph is k-connected
    virtual bool  Connected(TCap k) throw();

    /// \brief  The formal return of the DFS method CutNodes()
    enum TRetDFS {
        DFS_DISCONNECTED       = 0, ///< The graph is not even 1-connected
        DFS_BICONNECTED        = 1, ///< The graph is biconnected
        DFS_MULTIPLE_BLOCKS    = 2, ///< The graph is connected, but not biconnected
        DFS_ALMOST_BICONNECTED = 3  ///< The graph is not biconnected, but can be made
                                    ///  made biconnected by adding a single edge
    };

    /// \brief  Multiple purpose DFS method, 2-connectivity test
    ///
    /// \param rootArc      The arc considered first in the DFS. If the graph is
    ///                     2-connected, its the only tree arc adjacent with the root node
    /// \param order[]      Pre-ordinal numbers (nodes are numbered increasingly
    ///                     when reached the first time)
    /// \param lowArc[]     For any node v, a non-tree arc connecting a descendent of v
    ///                     with the lowest ancestor of v
    /// \retval true        The graph is 2-connected
    ///
    /// This method exports a maximal predecessor tree / forest. If the graph is not
    /// connected, each connected component is scanned. If the graph is not 2-connected,
    /// the nodes coloured 0 are the cut nodes, and the other colour classes define
    /// the non-trivial 2-blocks. Edge colours are assigned such that equally coloured
    /// edge denote edges in the same 2-connected component. The procedure is capable
    /// to export additional information by the arrays order[] and lowArc[]
    TRetDFS  CutNodes(TArc rootArc = NoArc,TNode* order = NULL,TArc* lowArc = NULL) throw();

    /// \brief  2-edge connectivity test
    ///
    /// \retval true  The graph is 2-edge connected
    ///
    /// Assigns node colours such that equally coloured nodes are in the same
    /// 2-edge connected component. Edge colours and predecessor labels are as
    /// for #CutNodes()
    bool  Biconnected() throw();

    /// \brief  Bipolar numbering and open ear decomposition
    ///
    /// \param source       The node with the minimum ordinal number 0 (optional parameter)
    /// \param target       The node with the maximum ordinal number n-1 (optional parameter)
    /// \param rootArc      If specified, the end node of this arc becomes the target,
    ///                     and the start node becomes the source (optional parameter)
    /// \retval true        The graph could be decomposed
    ///
    /// Exports the ordinal numbers by the node colours register and an open ear
    /// decomposition by the edge colours register. The input graph must be
    /// 2-connected or, at least, adding an arc between the selected source and
    /// target node must make the graph 2-connected
    bool  STNumbering(TArc rootArc = NoArc,TNode source = NoNode,TNode target = NoNode) throw(ERRejected);

    /// \brief  Test for k-edge connectivity
    ///
    /// \param k      The desired degree of connectivity
    /// \retval true  If the graph is k-edge connected
    virtual bool  EdgeConnected(TCap k) throw();

    /// \brief  Test for strong connectivity
    ///
    /// \retval true   If the graph is strongly connected
    ///
    /// Perform a reverse and a forward DFS. Assigns node colours such that
    /// equally coloured nodes are in the same strong component. The predecessor
    /// labels represent a forest spanning the strong components
    bool  StronglyConnected() throw();

    /// \brief  Test for strong k-connectivity
    ///
    /// \param k      The desired degree of connectivity
    /// \retval true  If the graph is strongly k-connected
    virtual bool  StronglyConnected(TCap k) throw();

    /// \brief  Test for strong k-edge connectivity
    ///
    /// \param k      The desired degree of connectivity
    /// \retval true  If the graph is strongly k-edge connected
    virtual bool  StronglyEdgeConnected(TCap k) throw();

    /// @}


    /// \addtogroup groupMinCut
    /// @{

    /// \brief  Options for the nodeSplitting constructor
    enum TOptNodeSplitting {
        MCC_UNIT_CAPACITIES = 0, ///< Assume unit node capacities and infinite arc capacities
        MCC_MAP_DEMANDS     = 1, ///< Instead of taking unit node capacities, apply the node demands
        MCC_MAP_UNDERLYING  = 2  ///< Ignore the arc orientations, do as if all arcs are undirected
    };

    /// \brief  Compute a global minimum node cut
    ///
    /// \param source  A predefined left-hand node or NoNode
    /// \param target  A predefined right-hand node or NoNode
    /// \param mode    If MCC_UNIT_CAPACITIES, assume unit node capacities. If MCC_MAP_DEMANDS, apply the node demands
    /// \return  The connectivity number (the minimum number of separation nodes)
    ///
    /// Provided that all node demands are 1 or that mode==0, this determines a
    /// minimum node cut as in the standard terminology, by splitting the nodes
    /// and computing maximum st-flows for all node pairs. The optimal node cut
    /// is exported by the node colour register where colour indices are
    /// interpreted as follows:
    /// - 0: Cut nodes
    /// - 1: Left-hand nodes
    /// - 2: Right-hand nodes
    TCap  NodeConnectivity(TNode source=NoNode,TNode target=NoNode,TOptNodeSplitting mode=MCC_MAP_DEMANDS) throw();

    /// \brief  Alternative methods for the minium capacity cut solver
    enum TMethMCC {
        MCC_DEFAULT        = -1, ///< Apply the default method set in #goblinController::methMST
        MCC_MAXFLOW        =  0, ///< Apply an iterated nax-flow method
        MCC_PREFLOW_FIFO   =  1, ///< Apply a FIFO push & relabel method
        MCC_PREFLOW_HIGH   =  2, ///< Apply a highest order push & relabel method
        MCC_IDENTIFICATION =  3  ///< Apply a node identification method
    };

    /// \brief  Compute a global minimum edge cut
    ///
    /// \param source  A predefined left-hand node or NoNode
    /// \param target  A predefined right-hand node or NoNode
    /// \return        The minimum cut capacity
    ///
    /// This is a shortcut for EdgeConnectivity(MCC_DEFAULT,source,target)
    TCap  EdgeConnectivity(TNode source=NoNode,TNode target=NoNode) throw(ERRange);

    /// \brief  Compute a global minimum edge cut
    ///
    /// \param method  A #TMethMCC value specifying the applied method
    /// \param source  A predefined left-hand node or NoNode
    /// \param target  A predefined right-hand node or NoNode
    /// \return        The edge connectivity number (the minimum number of separation edges)
    ///
    /// If two nodes are specified, this procedure determines a minimum edge cut by
    /// computing a maximum flow for this node pair. Otherwise, the method parameter
    /// allows to to choose from various methods.
    ///
    /// This determines a minimum edge cut by computing a series of n maximum
    /// st-flows. The optimal edge cut is exported by the node colour register where
    /// colour indices are interpreted as follows:
    /// - 0: Left-hand nodes (including source)
    /// - 1: Right-hand nodes (including target)
    TCap  EdgeConnectivity(TMethMCC method,TNode source=NoNode,TNode target=NoNode) throw(ERRange);

    /// \brief  Compute a global minimum node cut in the directed sense
    ///
    /// \param source  A predefined left-hand node or NoNode
    /// \param target  A predefined right-hand node or NoNode
    /// \param mode    If MCC_UNIT_CAPACITIES, assume unit node capacities. If MCC_MAP_DEMANDS, apply the node demands
    /// \return  The strong connectivity number (the minimum number of separating nodes)
    ///
    /// Provided that all node demands are 1 or that mode==0, this determines a
    /// minimum node cut as in the standard terminology.
    /// by splitting the nodes and computing maximum st-flows for all node pairs.
    /// The optimal node cut is exported by the node colour register where colour
    /// indices are interpreted as follows:
    /// - 0: Cut nodes
    /// - 1: Left-hand nodes
    /// - 2: Right-hand nodes
    /// For undirected graphs, the procedure behaves just as Connectivity().
    TCap  StrongNodeConnectivity(TNode source=NoNode,TNode target=NoNode,TOptNodeSplitting mode=MCC_MAP_DEMANDS) throw();

    /// \brief  Compute a minimum edge cut in the directed sense
    ///
    /// \param source  A predefined left-hand node or NoNode
    /// \param target  A predefined right-hand node or NoNode
    /// \return        The minimum cut capacity
    ///
    /// This is a shortcut for StrongEdgeConnectivity(MCC_DEFAULT,source,target)
    TCap  StrongEdgeConnectivity(TNode source=NoNode,TNode target=NoNode) throw(ERRange);

    /// \brief  Compute a minimum edge cut in the directed sense
    ///
    /// \param method  Either MCC_MAXFLOW, MCC_PREFLOW_HIGH or MCC_PREFLOW_FIFO
    /// \param source  A predefined left-hand node or NoNode
    /// \param target  A predefined right-hand node or NoNode
    /// \return        The minimum st-cut capacity
    ///
    /// This is the public interface to all strong edge connectivity methods.
    /// It calls one of the MCC_StrongEdgeConnectivity() methods, depending on
    /// on the passed node indices.
    TCap  StrongEdgeConnectivity(abstractMixedGraph::TMethMCC method,
            TNode source=NoNode,TNode target=NoNode) throw(ERRange);

private:

    /// \brief  Compute a global minimum edge cut in the directed sense
    ///
    /// \return  The strong edge connectivity number (the minimum number of separation edges)
    ///
    /// This determines a minimum edge cut by computing a series of n maximum
    /// st-flows. The optimal edge cut is exported by the node colour register where
    /// colour indices are interpreted as follows:
    /// - 0: Left-hand nodes
    /// - 1: Right-hand nodes
    /// For undirected graphs, the procedure behaves just as EdgeConnectivity().
    TCap  MCC_StrongEdgeConnectivity() throw();

    /// \brief  Compute a minimum edge cut in the directed sense
    ///
    /// \param method  Either MCC_MAXFLOW, MCC_PREFLOW_HIGH or MCC_PREFLOW_FIFO
    /// \param source  A predefined left-hand node
    /// \return        The minimum cut capacity
    ///
    /// This determines a minimum edge cut with r on the left-hand side. The
    /// optimal edge cut is exported by the node colour register where colour
    /// indices are interpreted as follows:
    /// - 0: Left-hand nodes (including root)
    /// - 1: Right-hand nodes
    TCap  MCC_StrongEdgeConnectivity(abstractMixedGraph::TMethMCC method,TNode source) throw(ERRange);

    /// \brief  Compute a minimum edge cut in the directed sense
    ///
    /// \param source  A predefined left-hand node or NoNode
    /// \param target  A predefined right-hand node or NoNode
    /// \return        The minimum st-cut capacity
    ///
    /// This procedure determines a minimum edge cut by computing a maximum flow
    /// for this node pair. The optimal edge cut is exported by the node colour
    /// register where colour indices are interpreted as follows:
    /// - 0: Left-hand nodes (including source)
    /// - 1: Right-hand nodes (including target)
    TCap  MCC_StrongEdgeConnectivity(TNode source,TNode target) throw(ERRange);

    /// \brief  Compute a legal node ordering
    ///
    /// \param r  The root of search
    /// \param x  The found left-hand node
    /// \param y  The found right-hand node
    /// \return   The minimum xy-cut capacity
    ///
    /// This grows the left-hand side of a cut, starting with r. Like greedy,
    /// the node with the maximum capacity of arcs connecting to the left-hand
    /// side is moved. This node search order is called a legal ordering, and is
    /// exported by the node colour register. The two final nodes in this
    /// ordering are returned by x and y.
    ///
    /// The importance is that the xy-cut capacity is derived without additional
    /// efforts. The procedure is technical analogue of efficient implementations
    /// of the Dijkstra and the Prim algorithms.
    TCap  MCC_LegalOrdering(TNode r,TNode& x,TNode& y) throw(ERRange,ERRejected);

    /// \brief  Compute a global minimum edge cut by node identification
    ///
    /// \return  The edge connectivity number (the capacity in a minimum edge cut)
    ///
    /// This determines the minimum edge xy-cut capacity for an unspecified node
    /// pair x,y by calling MinCutLegalOrdering(). Then these nodes are identified
    /// and the procedure is repeated until only a single node is left.
    ///
    /// The optimal edge cut is exported by the node colour register where
    /// colour indices are interpreted as follows:
    /// - 0: Left-hand nodes
    /// - 1: Right-hand nodes
    TCap  MCC_NodeIdentification() throw(ERRejected);

    /// @}


    /// \addtogroup groupSeriesParallel
    /// @{

public:

    /// \brief Options for series-parallelity methods
    enum TOptSeriesParallel {
        ESP_UNDIRECTED = 0, ///< Ignore the original arc orientations
        ESP_DIRECTED   = 1, ///< Respect the original arc orientations
        ESP_EMBEDDING  = 2, ///< Reorder the node incidence
        ESP_ORIENT     = 4, ///< Set the arc orientations according to the decomposition
        ESP_VISIBILITY = 8, ///< Draw the graph by a 1-visibility representation
        ESP_MINOR      = 16 ///< In the graph is not series-parallel, export a forbidden minor
    };

    /// \brief Handling of edge series-parallel graphs
    ///
    /// \param options          Any bit combination of the #TOptSeriesParallel options
    /// \param s                The source node if the graph can been reduced to a single edge
    /// \param t                The target node if the graph can been reduced to a single edge
    /// \param _T               One can pass a binary decomposition tree here
    /// \retval true            The graph could be decomposed
    ///
    /// Manipulate edge series parallel graphs according to a given decomposition
    /// tree and / or call the edge series parallel recognition method
    bool EdgeSeriesParallelMethod(TOptSeriesParallel options = ESP_EMBEDDING,
             TNode s = NoNode,TNode t = NoNode,abstractMixedGraph* _T = NULL) throw(ERRejected);

    /// \brief Recognition of edge series-parallel graphs
    ///
    /// \param options  Either #ESP_DIRECTED or #ESP_UNDIRECTED
    /// \param s        The source node if the graph can been reduced to a single edge
    /// \param t        The target node if the graph can been reduced to a single edge
    /// \return         A pointer to the decomposition tree or NULL
    ///
    /// This method checks if the graph is edge series parallel and generates an
    /// according binary decomposition tree. The leave nodes of this tree are
    /// the original edges and the node colours encode node types and arc orientations.
    abstractMixedGraph*  ESP_DecompositionTree(TOptSeriesParallel options = ESP_DIRECTED,
                            TNode s = NoNode,TNode t = NoNode) throw();

private:

    /// \brief Construction of subgraphs which prevent from series-parallelity
    ///
    /// \param options  Either #ESP_DIRECTED or #ESP_UNDIRECTED
    /// \param G        The reduced graph after applying the series-parallel production rules
    /// \param T        The respective partial decomposition tree
    ///
    /// If the graph cannot be entirely decmposed by applying the series-parallel
    /// production rules, this method takes the partial decomposition tree T and
    /// the reduced graph G, determines a forbidden subgraph in G and maps it to
    /// the original graph by means of T. The subgraph consists of the edges with
    /// finite edge colours. All edges with the same colour contract into an edge
    /// of the forbidden minor.
    void  ESP_ConstructK4Minor(TOptSeriesParallel options,abstractMixedGraph& G,
            abstractMixedGraph* T) throw ();

    /// @}


    /// \addtogroup planarEmbedding
    /// @{

public:

    /// \brief  Alternative methods for planarity testing and embedding
    enum TMethPlanarity {
        PLANAR_DEFAULT   = -1, ///< Apply the default method set in #goblinController::methPlanarity
        PLANAR_HO_TA     =  0, ///< Apply the Hopcroft / Tarjan method
        PLANAR_DMP       =  1  ///< Apply the Demoulcron / Malgrange / Pertuisset method
    };

    /// \brief Options for planarity methods
    enum TOptPlanarity {
        PLANAR_NO_OPT    =  0, ///< Planarity test and (occasionally) embedding only
        PLANAR_MINOR     =  1  ///< If the graph is non-planar, raise a forbidden minor
    };

    /// \brief Perform a planarity test
    ///
    /// \param method   A TMethPlanarity value specifying the applied method
    /// \param options  If PLANAR_MINOR is specified, if the graph is non-planar,
    ///                 and if the method supports this, a forbidden minor is computed
    ///
    /// This is the public interface to PlanarityMethod(). It applies to all graph
    /// objects since node incidences are not manipulated.
    bool  IsPlanar(TMethPlanarity method = PLANAR_DEFAULT,TOptPlanarity options = PLANAR_MINOR) throw(ERRejected);

private:

    /// \brief Interface to planarity test and embedding methods
    ///
    /// This method dissects the graph into biconnected simple componemts,
    /// for which a concrete planarity testing or even embedding method is
    /// called. Embeddings of the components are occasionally joined to
    /// a planar representation of the original graph
    ///
    /// \param method   A #TMethPlanarity value specifying the applied method
    /// \param options  If PLANAR_MINOR is specified, if the graph is non-planar,
    ///                 and if the #method supports this, a forbidden minor is computed
    /// \param predArc  If this is not NULL, the array exports a node incidence order
    bool  PlanarityMethod(TMethPlanarity method,TOptPlanarity options,TArc* predArc = NULL) throw(ERRejected);

    /// \brief Hopcroft/Tarjan planarity test and embedding
    ///
    /// The famous planarity code which runs in linear time
    ///
    /// \param predArc       If this is not NULL, the array exports a node incidence order
    /// \param extractMinor  If this is true and the graph is non-planar,
    ///                      a K_5 or a K_3_3 minor is computed
    bool  PlanarityHopcroftTarjan(TArc* predArc,bool extractMinor) throw(ERRejected);

    /// \brief Demoucron/Malgrange/Pertuiset planarity test and embedding
    ///
    /// This is the planartiy test of Demoucron, Malgrange, Pertuiset, which is simpler
    /// than the Hopcroft/Tarjan planarity test, but not so fast.
    ///
    /// \param predArc       If this is not NULL, the array exports a node incidence order
    bool  PlanarityDMP64(TArc* predArc) throw(ERRejected);

public:

    /// \brief Refine a planar representation in order to obtain a maximal exterior face
    ///
    /// This method applies to planar embedded graph objects only. Dual incidences
    /// If the graph is implicitly outerplanar,
    void  GrowExteriorFace() throw(ERRejected);

private:

    /// \brief Refine a planar representation in order to obtain a maximal exterior face
    ///
    /// This method applies to planely represented graph objects only. Dual incidences
    /// #face[] must be provided in advance. The procedure sweeps clockwise around
    /// the exterior face. If the two end nodes of an exterior arc form a cutting
    /// pair, this arc is routed internally. The dual incidences are also updated.
    ///
    /// If the graph is outerplanar, a respective embedding results. Otherwise,
    /// a K_4 or K_3_2 minor is exported by the edge colours register.
    ///
    /// \param exteriorArc  An arc whose left-hand face is the initial exterior
    /// \return             The final number of exterior nodes
    TNode  GrowExteriorFace(TArc exteriorArc) throw(ERRejected);

    /// \brief Refine the node incidence list of x so that all incident 2-blocks become exterior
    ///
    /// This procedure also updates the dual incidences #face[] which must be
    /// provided in advance
    ///
    /// \param x         The index of the node under inspection
    /// \param _visited  If this array is supplied, the performance is improved by incremental updates
    /// \retval true     The incidence list of x has been reordered
    bool  MoveInteriorBlocks(TNode x,bool* _visited = NULL) throw(ERRejected);

    /// @}


    /// \addtogroup groupPlanarAugmentation
    /// @{


public:

    /// \brief Add arcs until the graph becomes connected
    ///
    /// This operation runs in linear time and applies to all sparse represented
    /// graph objects. It the graph in advance is planar [embedded], the final
    /// graph is also planar [embedded]. The augmenting edge set is minimal.
    void  PlanarConnectivityAugmentation() throw(ERRejected);

    /// \brief Add arcs to a connected graph so that it becomes biconnected
    ///
    /// This operation runs in linear time and applies to connected planar
    /// embedded graph objects only. The initial planar representation is legally extended
    /// in the final graph. The augmenting edge set is most likely not minimal.
    void  PlanarBiconnectivityAugmentation() throw(ERRejected);

    /// \brief Augment a biconnected planar graph so that the graph becomes triconnected and all faces become triangles
    ///
    /// This operation runs in linear time and applies to biconnected planar
    /// embedded graph objects only. The initial planar representation is legally extended
    /// in the final graph.
    void  Triangulation() throw(ERRejected);

    /// @}


    /// \addtogroup manipIncidences
    /// @{

public:

    /// \brief Produce a random incidence order
    ///
    /// This operation applies to sparse represented graphs only.
    void  RandomizeIncidenceOrder() throw(ERRejected);

    /// \brief Synchronize node incidence order with drawing
    ///
    /// Reorder the node incidences according to the current 2D drawing.
    /// This operation applies to sparse represented graphs only.
    void  IncidenceOrderFromDrawing() throw(ERRejected);

    /// \brief Compute a planar representation
    ///
    /// \param method  A #TMethPlanarity value specifying the applied method
    /// \retval true   The graph is planar
    ///
    /// This calls PlanarityMethod() and, if the graph turns out to be planar,
    /// ReorderIncidences() which orders the node incidences according to a
    /// potential plane drawing.
    /// This operation applies to sparse represented graphs only.
    bool  PlanarizeIncidenceOrder(TMethPlanarity method = PLANAR_DEFAULT) throw(ERRejected);

    /// @}


    /// \addtogroup groupPlanarDrawing
    /// @{

public:

    /// \brief Partition the node set of a triconnected planar graph so that it can be drawn efficiently
    ///
    /// \return          The number of generated components
    /// \param cLeft[]   When drawing the components bottom up, the leftmost contact arc
    /// \param cRight[]  When drawing the components bottom up, the rightmost contact arc
    /// \param vRight[]  The right-hand node in the same component, NoNode for the rightmost node
    ///
    /// This method is a prerequisite for several planar graph drawing methods:
    /// -  (Convex) straight line embedding
    /// -  1-bent orthogonal drawing with high-degree nodes
    /// -  Polyline drawings with ports and 3 bends per edge
    TNode  LMCOrderedPartition(TArc* cLeft,TArc* cRight,TNode* vRight) throw(ERRejected);

    /// \brief Convex grid drawing of triconnected planar graphs
    ///
    /// All nodes are placed in an integer grid, and all edges are displayed by
    /// straight lines without crossings.
    ///
    /// \param aBasis   The arc which is drawn in the bottom line
    /// \param spacing  The coordinate-wise minimum number of grid lines between two nodes
    void  Layout_ConvexDrawing(TArc aBasis = NoArc,TFloat spacing = 0.0) throw(ERRejected);

    /// \brief Straight line grid drawing of planar graphs
    ///
    /// All edges are displayed by straight lines without crossings. If the
    /// force-directed post optimization is disabled, all nodes are placed
    /// in an integer grid.
    ///
    /// \param aBasis   The arc which is drawn in the bottom line
    /// \param spacing  The minimum node distance
    void  Layout_StraightLineDrawing(TArc aBasis = NoArc,TFloat spacing = 0.0) throw(ERRejected);

    /// @}


    /// \addtogroup groupOrthogonalPlanar
    /// @{

    /// \brief 1-Bent orthogonal grid drawing of planar graphs with high degree nodes
    ///
    /// All nodes are placed in a coarse integer grid and drawn with equal size.
    /// All edges are displayed by orthogonal open polygons in a fine grid without crossings.
    ///
    /// \param aBasis   The arc which is routed outermost on the left hand and the bottom side
    /// \param spacing  The minimum node distance
    void  Layout_StaircaseDrawing(TArc aBasis = NoArc,TFloat spacing = 0.0) throw(ERRejected);

    /// \brief 1-Bent orthogonal grid drawing of triconnected planar graphs with high degree nodes
    ///
    /// All nodes are placed in a coarse integer grid and drawn with equal size.
    /// All edges are displayed by orthogonal open polygons in a fine grid without crossings.
    /// The graph must be simple and triconnected.
    ///
    /// \param aBasis   The arc which is routed outermost on the left hand and the bottom side
    /// \param spacing  The minimum node distance
    void  Layout_StaircaseTriconnected(TArc aBasis = NoArc,TFloat spacing = 0.0) throw(ERRejected);

private:

    /// \brief  Core procedure of the staircase drawing algorithm
    ///
    /// This procedure fixes the relative horizontal and vertical node positions. The implicit
    /// arc orientations are assigned so that arcs start with a horizontal segment and end with a
    /// vertical segment. Finally, the minimum node size is determined. The procedure modiifieds
    /// the layout parameters nodeSpacing, bendSpacing, fineSpacing
    ///
    /// \param aBasis         The arc which is routed outermost on the left hand and the bottom side
    /// \param spacing        The minimum node distance
    /// \param orientation[]  An array to save the implicit arc orientations
    void  Layout_StaircaseSketch(TArc aBasis,TFloat spacing,char* orientation) throw(ERRejected);

    /// @}


    /// \addtogroup groupPerfectGraphs
    /// @{

public:

    /// \brief  Complementarity option for perfect graph methods
    enum TOptComplement {
        PERFECT_AS_IS      = 0, ///< Apply method to the addressed graph
        PERFECT_COMPLEMENT = 1  ///< Apply method to the implicitly given complementary graph
    };

    /// \brief  Determine a perfect elimination order and a simplicial node
    ///
    /// This assigns the node colours with a certain node order. If the graph is
    /// chordal, this is a so-called perfect elimination order, and the returned
    /// node is simplicial, that is, its neighbours form a clique.
    ///
    /// \param complementarityMode  Specifies if the method addresses the given or the complementary graph
    /// \return  The index of the node which comes first  in the order (which has node colour 0)
    TNode  PerfectEliminationOrder(TOptComplement complementarityMode = PERFECT_AS_IS) throw();

    /// \brief  Perform a chordality test
    ///
    /// \param complementarityMode  Specifies if the method addresses the given or the complementary graph
    /// \retval true  This graph is chordal, that is, a perfect elimnation order has been found
    bool  IsChordal(TOptComplement complementarityMode = PERFECT_AS_IS) throw();

    /// @}


    /// \addtogroup spanningTree
    /// @{

public:

    /// \brief  Alternative methods for the spanning tree solver
    enum TMethMST {
        MST_DEFAULT = -1,   ///< Apply the default method set in #goblinController::methMST
        MST_PRIM    =  0,   ///< Apply the Prim method
        MST_PRIM2   =  1,   ///< Apply the enhanced Prim method
        MST_KRUSKAL =  2,   ///< Apply the Kruskal greedy method
        MST_EDMONDS =  3    ///< Apply the Edmonds arborescence method
    };

    /// \brief  Modifiers for the objective of spanning tree optimization
    enum TOptMST {
        MST_PLAIN             =  0, ///< Run solver in the default mode
        MST_ONE_CYCLE         =  1, ///< Determine a subgraph with exactly one cycle, exposing the root node
        MST_UNDIRECTED        =  2, ///< Ignore the arc orientations
        MST_DIRECTED          =  4, ///< Take care of the arc orientations
        MST_REDUCED           =  8, ///< Take care of the node potentials, optimize w.r.t. the reduced length labels
        MST_ONE_CYCLE_REDUCED =  9, ///< Combination of MST_ONE_CYCLE and MST_ONE_CYCLE_REDUCED
        MST_MAX               = 16  ///< Maximize the total edge length instead of minimizing it
    };

    /// \brief  Compute a minimum spanning tree or a related subgraph structure
    ///
    /// \param root            The intended root node
    /// \return                The total arc length of the derived subgraph
    ///
    /// This is the public interface to all spanning tree methods.
    TFloat  MinTree(TNode root = NoNode) throw(ERRange,ERRejected);

    /// \brief  Compute a minimum spanning tree or a related subgraph structure
    ///
    /// \param method          A #TMethMST value specifying the applied method
    /// \param characteristic  A bit combination of #TOptMST values
    /// \param root            The root node of the arborescence
    /// \return                The total arc length of the derived subgraph
    ///
    /// This is the public interface to all spanning tree methods. By specifying
    /// methods and characteristics, the properties of the subgraph can be varied
    TFloat  MinTree(TMethMST method,TOptMST characteristic,TNode root = NoNode)
        throw(ERRange,ERRejected);

    /// \brief  Check if the current subgraph encodes a spanning tree and save it to the predecessor labels
    ///
    /// \param root            The root node of the arborescence
    /// \param characteristic  Valid options are #MST_ONE_CYCLE and #MST_DIRECTED
    /// \retval true           The current subgraph could be validated
    bool  ExtractTree(TNode root,TOptMST characteristic = MST_PLAIN) throw(ERRejected);

    /// \brief  Check if the current subgraph encodes a spanning tree and save it to an array
    ///
    /// \param pred            The array where predecessor labels are stored
    /// \param root            The desired root node
    /// \param characteristic  Valid options are #MST_ONE_CYCLE and #MST_DIRECTED
    /// \retval true           The current subgraph could be validated
    bool  ExtractTree(TArc* const pred,TNode root,TOptMST characteristic = MST_PLAIN)
        throw(ERRejected);

protected:

    /// \brief  Prims minimum spanning tree method
    ///
    /// \param method          Either #MST_PRIM or #MST_PRIM2
    /// \param characteristic  A bit combination of #TOptMST modifiers
    /// \param root            The root node of the arborescence
    /// \return                The total arc length of the derived subgraph
    ///
    /// This method saves the spanning tree to the predecessor labels.
    TFloat  MST_Prim(TMethMST method,TOptMST characteristic,TNode root = NoNode)
         throw(ERRange,ERRejected);

    /// \brief  Edmonds' minimum spanning arborescence method
    ///
    /// \param characteristic  A bit combination of #TOptMST modifiers
    /// \param root            The root node of the arborescence
    /// \return                The total arc length of the derived subgraph
    ///
    /// This method saves the spanning tree to the predecessor labels. Other
    /// than the Prim and the Kruskal method, arc directions are considered.
    TFloat  MST_Edmonds(TOptMST characteristic,TNode root = NoNode)
        throw(ERRange,ERRejected);

    /// \brief  Kruskals minimum spanning tree method
    ///
    /// \param characteristic  A bit combination of #TOptMST modifiers
    /// \param root            The root node of the arborescence
    /// \return                The total arc length of the derived subgraph
    ///
    /// This method saves the spanning tree to the subgraph data structure. If
    /// the graph is disconnected, the connected components are saved to the
    /// partition data structure.
    TFloat  MST_Kruskal(TOptMST characteristic,TNode root = NoNode) throw(ERRange,ERRejected);

private:

    /// \brief  Abstraction of arc length labels, called by spanning tree methods
    ///
    /// \param characteristic  Either #MST_PLAIN or #MST_REDUCED
    /// \param potential       An array of node potentials
    /// \param arcIndex        The index of the considered arc
    /// \return                Either the length or reduced length label of the specified arc
    TFloat  MST_Length(TOptMST characteristic,TFloat* potential,TArc arcIndex) throw(ERRange);

    /// @}


    /// \addtogroup steiner
    /// @{

public:

    /// \brief  Determine a minimum length discrete Steiner tree
    ///
    /// \param Terminals  An index set of terminal nodes in the range [0,1,..,n]
    /// \param root       A node in the range [0,1,..,n]
    /// \return           The length of the found Steiner tree
    TFloat  SteinerTree(const indexSet<TNode>& Terminals,TNode root=NoNode) throw(ERRange,ERRejected);

protected:

    /// \brief  Turn a given spanning tree into a Steiner tree by successively deleting all Steiner nodes
    ///
    /// \param Terminals  An index set of terminal nodes in the range [0,1,..,n]
    /// \param pred       An array of predecessor arcs
    /// \return           The sum of lengths of the deleted arcs
    ///
    /// This turns a spanning tree given by the predecessor arcs in pred[] into
    /// a Steiner tree by successively deleting all Steiner nodes which are leaves.
    TFloat  STT_TrimLeaves(const indexSet<TNode>& Terminals,TArc* pred) throw();

    /// \brief  Steiner tree construction heuristic
    ///
    /// \param Terminals  An index set of terminal nodes in the range [0,1,..,n]
    /// \param root       A node in the range [0,1,..,n]
    /// \return           The length of the found Steiner tree
    ///
    /// This simply determines a minimum spanning tree and then calls STT_TrimLeaves().
    virtual TFloat  STT_Heuristic(const indexSet<TNode>& Terminals,TNode root) throw(ERRange);

    /// \brief  Brute force enumeration of discrete Steiner trees
    ///
    /// \param Terminals  An index set of terminal nodes in the range [0,1,..,n]
    /// \param root       A node in the range [0,1,..,n]
    /// \return           The length of the found Steiner tree
    ///
    /// This iterates over all Steiner node subsets and computes a spanning tree
    /// of the complementary subgraph. Only the applied spanning tree method
    /// differs between undirected and general graphs.
    virtual TFloat  STT_Enumerate(const indexSet<TNode>& Terminals,TNode root) throw(ERRange);

    /// @}


    /// \addtogroup maximumFlow
    /// @{

public:

    /// \brief  Alternative methods for the maximum st-flow solver
    enum TMethMXF {
        MXF_DEFAULT       = -1,   ///< Apply the default method set in #goblinController::methMXF
        MXF_SAP           =  0,   ///< Apply the shortest path method
        MXF_DINIC         =  1,   ///< Apply the Dinic blocking flow method
        MXF_PREFLOW_FIFO  =  2,   ///< Apply the FIFO push / relabel method
        MXF_PREFLOW_HIGH  =  3,   ///< Apply the highest label push / relabel method
        MXF_PREFLOW_SCALE =  4,   ///< Apply the excess scaling push / relabel method
        MXF_SAP_SCALE     =  5    ///< Apply a shortest path method with scaled capacities
    };

    /// \brief  Compute a maximum st-flow by using a default method
    ///
    /// \param source   A node whose flow excess is minimized
    /// \param target   A node whose flow excess is maximized
    /// \return         The flow augmentation value
    ///
    /// The method determines a subgraph in which all nodes but source and target
    /// are balanced, and the flow excess of the source node is minimal. The
    /// distance labels on return encode a minimum st-cut.
    ///
    /// Depending on the applied method, it may be required that the lower
    /// capacity bounds are all zero, or that the subgraph at start is already
    /// a feasible st-flow whose value does not exeed the demand of target.
    ///
    /// Note that the returned value is the difference of the final and the inital
    /// flow value. This works as the absolute flow of the computed flow only if
    /// the solver has been started from scratch, with a zero flow.
    TFloat  MaxFlow(TNode source,TNode target) throw(ERRange,ERRejected);

    /// \brief  Compute a maximum st-flow by using the specified method
    ///
    /// \param method   A #TMethMXF value specifying the applied method
    /// \param source   A node whose flow excess is minimized
    /// \param target   A node whose flow excess is maximized
    /// \return         The value of the final st-flow
    ///
    /// This is the public interface to all maximum st-flow methods. The result
    /// is a subgraph in which all nodes but source and target are balanced,
    /// and the flow excess of the source node is minimal. The distance labels
    /// on return encode a minimum st-cut.
    ///
    /// Depending on the applied method, it may be required that the lower
    /// capacity bounds are all zero, or that the subgraph at start is already
    /// a feasible st-flow whose value does not exeed the demand of target.
    TFloat  MaxFlow(TMethMXF method,TNode source,TNode target) throw(ERRange,ERRejected);

    /// \brief  Construct a feasible b-flow
    ///
    /// \retval true           The graph admits a feasible b-flow
    ///
    /// This method determines a b-flow by using the maximum flow solver.
    /// The solution is returned by the subgraph data structure.
    bool  AdmissibleBFlow() throw();

    /// @}


    /// \addtogroup minCostFlow
    /// @{

public:

    /// \brief  Alternative methods for the minimum cost st-flow solver
    enum TMethMCFST {
        MCF_ST_DEFAULT  = -1,   ///< Apply the default method set in #goblinController::methMCFST
        MCF_ST_DIJKSTRA =  0,   ///< Apply the Dijkstra shortest path method
        MCF_ST_SAP      =  1,   ///< Apply a label setting shortest path method
        MCF_ST_BFLOW    =  2    ///< Solve by transformation to a b-flow problem
    };

    /// \brief  Compute a minimum cost st-flow by using the default method
    ///
    /// \param source   A node whose flow excess is minimized
    /// \param target   A node whose flow excess is maximized
    /// \return         The weight of the final st-flow or InfFloat
    ///
    /// This method returns a subgraph in which all nodes but source and target
    /// are balanced, and the flow excess of the source node is minimal or
    /// the target node is balanced. The optimal node potentials are exported
    /// for sake of post-optimization.
    ///
    /// It is required that the subgraph at start is a feasible st-flow whose
    /// value does not exeed the demand of the target node. The node demands
    /// must resolve
    ///
    /// The initial node potentials are considered. If reduced cost optimality
    /// is violated, the potentials are adjusted so that the search for shortest
    /// augmenting paths can run with non-negative arc lengths.
    TFloat  MinCostSTFlow(TNode source,TNode target) throw(ERRange,ERRejected);

    /// \brief  Compute a minimum cost st-flow by using the specified method
    ///
    /// \param method   A #TMethMCFST value specifying the applied method
    /// \param source   A node whose flow excess is minimized
    /// \param target   A node whose flow excess is maximized
    /// \return         The weight of the final st-flow or InfFloat
    ///
    /// This is the public interface to all minimum cost st-flow methods. The
    /// result is a subgraph in which all nodes but source and target
    /// are balanced, and the flow excess of the source node s is minimal or
    /// the target node is balanced. The optimal node potentials are exported
    /// for sake of post-optimization.
    ///
    /// It is required that the subgraph at start is a feasible st-flow whose
    /// value does not exeed the demand of the target node. The node demands
    /// must resolve
    ///
    /// The initial node potentials are considered. If reduced cost optimality
    /// is violated, the potentials are adjusted so that the search for shortest
    /// augmenting paths can run with non-negative arc lengths.
    TFloat  MinCostSTFlow(TMethMCFST method,TNode source,TNode target) throw(ERRange,ERRejected);


    /// \brief  Alternative methods for the minimum cost b-flow solver
    enum TMethMCF {
        MCF_BF_DEFAULT = -1,    ///< Apply the default method set in #goblinController::methMCF
        MCF_BF_CYCLE   =  0,    ///< Apply the Klein cycle canceling method
        MCF_BF_COST    =  1,    ///< Apply the Goldberg / Tarjan cost scaling method
        MCF_BF_TIGHT   =  2,    ///< Apply the Goldberg / Tarjan cost scaling method with eps-tightening
        MCF_BF_MEAN    =  3,    ///< Apply minimum mean cycle canceling method
        MCF_BF_SAP     =  4,    ///< Solve by transformation to an st-flow problem
        MCF_BF_SIMPLEX =  5,    ///< Apply the network simplex algorithm
        MCF_BF_LINEAR  =  6,    ///< Solve by transformation to a linear program
        MCF_BF_CAPA    =  7,    ///< Apply a capacity scaling method
        MCF_BF_PHASE1  =  8     ///< Stop after an admissible b-flow has been found
    };

    /// \brief  Compute a minimum cost st-flow by using the specified method
    ///
    /// \param method   A #TMethMCF value specifying the applied method
    /// \return         The weight of the final b-flow or InfFloat
    ///
    /// This is the public interface to all minimum cost b-flow methods. The
    /// result is a subgraph in which all nodes are balanced, that is, all node
    /// demands are satisfied. The optimal node potentials are exported
    /// for sake of post-optimization.
    ///
    /// The initial subgraph and node potentials are considered.
    TFloat  MinCostBFlow(TMethMCF method = MCF_BF_DEFAULT) throw(ERRejected);

    /// @}


    /// \addtogroup stableSet
    /// @{

public:

    /// \brief  Compute a maximum independent node set
    ///
    /// \return  The achieved stable set cardinality
    virtual TNode  StableSet() throw();

    /// \brief  Compute a maximum cardinality clique
    ///
    /// \return  The achieved clique cardinality
    TNode  Clique() throw();

    /// \brief  Compute a minimum vertex cover
    ///
    /// \return  The achieved vertex cover cardinality
    TNode  VertexCover() throw();

    /// @}


    /// \addtogroup colouring
    /// @{

public:

    /// \brief  Partition the node set into independent sets
    ///
    /// \param k  The accepted number of independent sets
    /// \return   The achieved number of independent sets
    virtual TNode  NodeColouring(TNode k=NoNode) throw();

private:

    /// \brief  Partition the node set of a planar graph into five independent sets
    ///
    /// \return  The achieved number of independent sets
    TNode  PlanarColouring() throw(ERRejected);

    /// \brief  Try to improve an existing node colouring by local search
    ///
    /// \return  The achieved number of independent sets
    TNode  NCLocalSearch() throw(ERRejected);

public:

    /// \brief  Modify a given colouring by alternating two colours in a Kempe component
    ///
    /// \param nodeColour  A pointer to an arry of node colours
    /// \param r           A node which is recoloured
    /// \param x           A node which should not be recoloured
    /// \retval true       r and x are in different Kempe components
    /// 
    /// This takes the colours of r and x and flips the colours in the Kempe
    /// component (the connected component spanned by both colours) of r.
    bool  NCKempeExchange(TNode* nodeColour,TNode r,TNode x) throw(ERRange,ERRejected);

    /// \brief  Partition the node set into cliques
    ///
    /// \param k  The accepted number of cliques
    /// \return   The achieved number of cliques
    TNode  CliqueCover(TNode k=NoNode) throw();

    /// \brief  Partition the edge set into independent sets
    ///
    /// \param k  The accepted number of independent sets
    /// \return   The achieved number of independent sets
    TArc  EdgeColouring(TArc k=NoNode) throw(ERRange);

    /// @}


    /// \addtogroup feedbackSets
    /// @{

public:

    /// \brief  Compute a minimum feedback arc set
    ///
    /// \return  The achieved arc set capacity
    ///
    /// The feedback arc set is provided by the edge colour register. If the
    /// graph is directed, the arcs with colour index 1 have to be reverted in
    /// order to obtain an acyclic digraph. Undirected edges are coloured 2 or
    /// 3, depending on the implicit orientation.
    TCap  FeedbackArcSet() throw();

    /// \brief  Extract implicit top-down arc orientations from the current drawing
    ///
    /// The arc orientations are provided by the edge colour register. If the
    /// graph is directed, the arcs with colour index 1 have to be reverted in
    /// order to obtain the top-down orientation. Undirected edges are coloured
    /// 2 or 3, depending on the implicit orientation.
    void  ImplicitOrientationFromDrawing() throw();

    /// \brief  Determine an st-numbering and according implicit arc orientations
    ///
    /// \param s      An optional source node or NoNode
    /// \param t      An optional target node or NoNode
    /// \retval true  The graph is st-orientable
    ///
    /// The arc orientations are provided by the edge colour register. If the
    /// graph is directed, the arcs with colour index 1 have to be reverted in
    /// order to obtain the st-orientation. Undirected edges are coloured 2 or
    /// 3, depending on the implicit orientation.
    bool  ImplicitSTOrientation(TNode s=NoNode, TNode t=NoNode) throw(ERRejected);

    /// @}


    /// \addtogroup eulerPostman
    /// @{

public:

    /// \brief  Compute a maximum-weight Eulerian subgraph
    ///
    /// \param adjustUCap  If true: Increase the arc capacities to a minimum-weight Eulerian supergraph
    virtual void  ChinesePostman(bool adjustUCap) throw(ERRejected);

    /// \brief  Compute an Euler cycle
    ///
    /// \param pred   An TArc[m] array to store the edge predecessors
    /// \retval true  The graph is Eulerian
    ///
    /// This procedure checks if the graph is Eulerian and, occasionally,
    /// determines an Eulerian closed walk. If a pred[] array is passed, this
    /// is filled with the predecessors ranged [0,1,..,2m-1] for each edge.
    /// Otherwise, for every arc, its ordinal number in the walk is stored
    /// by the edge colour register.
    ///
    /// In order to save the walk by such fixed length arrays, it is required
    /// that the upper capacity bounds are all one. If higher arc capacities
    /// occur (as for the ChinesePostman() application), it is recommended to
    /// call ExplicitParallels() first.
    bool  EulerianCycle(TArc* pred = NULL) throw(ERRejected);

    /// @}


    /// \addtogroup tsp
    /// @{

public:

    /// \brief  Alternative TSP construction heuristics
    enum THeurTSP {
        TSP_HEUR_DEFAULT      = -1,  ///< Apply the default method set in #goblinController::methHeurTSP
        TSP_HEUR_RANDOM       =  0,  ///< Apply the random starting heuristic
        TSP_HEUR_FARTHEST     =  1,  ///< Apply the farthest insertion heuristic
        TSP_HEUR_TREE         =  2,  ///< Apply the tree approximation method
        TSP_HEUR_CHRISTOFIDES =  3,  ///< Apply the Christofides approximation method
        TSP_HEUR_NEAREST      =  4   ///< Apply the nearest insertion heuristic
    };

    /// \brief  Levels of TSP lower bounding
    enum TRelaxTSP {
        TSP_RELAX_DEFAULT     = -1,  ///< Apply the default method set in #goblinController::methRelaxTSP1
                                     ///  respectively #goblinController::methRelaxTSP2
        TSP_RELAX_NULL        = -2,  ///< Do not apply any bounding procedure
        TSP_RELAX_1TREE       =  0,  ///< Single one-cycle tree relaxation
        TSP_RELAX_FAST        =  1,  ///< Fast subgradient optimization
        TSP_RELAX_SUBOPT      =  2   ///< Thorough subgradient optimization
    };

    /// \brief  Run the TSP solver with default parameters
    ///
    /// \param root  A node index in the range [0,1,..,n-1] or NoNode
    /// \return      The length of the constructed tour or InfFloat
    TFloat  TSP(TNode root=NoNode) throw(ERRange,ERRejected);

    /// \brief  Solve a travelling salesman instance
    ///
    /// \param methHeur    A #THeurTSP value specifying the applied construction heuristic
    /// \param methRelax1  A #TRelaxTSP value specifying the applied initial bounding procedure
    /// \param methRelax2  A #TRelaxTSP value specifying the applied branch&bound procedure
    /// \param root        A node index in the range [0,1,..,n-1] or NoNode
    /// \return            The length of the constructed tour or InfFloat
    ///
    /// This searches for a Hamiltonian cycle of minimum length, and exports
    /// the constructed tour by the predecessor labels. Depending on the general
    /// optimization level, the following operations take place:
    /// - Check if there is a tour given in advance
    /// - Check if the one-cycle tree rooted at r exists
    /// - Apply a construction heuristic by calling TSP_Heuristic()
    /// - Apply the lower bounding procedure TSP_SubOpt1Tree()
    /// - Call TSP_BranchAndBound() for an optimality proof or candiadate graph search
    /// Each of these steps preserves or improves the predecessor labels and the
    /// node potentials given in advance. By that, the solver can be called
    /// iteratively with different parameters.
    TFloat  TSP(THeurTSP methHeur,TRelaxTSP methRelax1,TRelaxTSP methRelax2,TNode root=NoNode)
                throw(ERRange,ERRejected);

protected:

    /// \brief  Compute a tour from a random Hamiltonian cycle
    ///
    /// \param method  A #THeurTSP value specifying the applied method
    /// \param root    A node index in the range [0,1,..,n-1] or NoNode
    /// \return        The length of the constructed tour or InfFloat
    ///
    /// This applies some construction heuristic, depending on the method value.
    ///
    /// If the graph is neither a complete undirected graph nor a complete
    /// digraph, the construction heuristic is applied to the metric closure,
    /// and the tour is mapped back to the original graph. This might fail,
    /// however.
    virtual TFloat  TSP_Heuristic(abstractMixedGraph::THeurTSP method,TNode root) throw(ERRange,ERRejected);

    /// \brief  Compute a tour from a random Hamiltonian cycle
    ///
    /// This sets a random premutation of the node set and then calls
    /// TSP_LocalSearch() for postprocessing. When using this procedure, it is
    /// reasonable to call it several times.
    TFloat  TSP_HeuristicRandom() throw(ERRejected);

    /// \brief  Compute a tour by farthest insertion
    ///
    /// \param method  Either #TSP_HEUR_FARTHEST or #TSP_HEUR_NEAREST
    /// \param r       The node on the initial subtour or NoNode
    /// \return        The length of the constructed tour
    ///
    /// This starts with a single node as the initial subtour, and successively
    /// inserts the node which is most far away from the current subtour, between
    /// two nodes where it causes the least change of subtour length.
    ///
    /// The procedure does not strictly require a complete graph. But at every
    /// stage, at least one node feasible for insertion must exist. And this is
    /// in particular false for bigraphs.
    ///
    /// If no root node is specified, the procedure is repeated for all possible
    /// root nodes.
    ///
    /// If CT.methLocal==#LOCAL_OPTIMIZE, TSP_LocalSearch() is called for postprocessing.
    TFloat  TSP_HeuristicInsert(THeurTSP method,TNode r=NoNode) throw(ERRange,ERRejected);

    /// \brief  Compute a tour from a minimum one-cycle tree
    ///
    /// \param r  The tree root node or NoNode
    /// \return   The length of the constructed tour
    ///
    /// This starts with a minimum one-cycle tree. By duplicating the predecessor
    /// edges, an Eulerian walk is given. This walk is reduced to a Hamiltonian
    /// cycle as follows: The unique predecessor cycle serves as the initial
    /// subtour (including r). One tracks back from every leave node x until the
    /// current subtour is reached (say at a node y). Let z denote the successor
    /// of y on the current subtour. Then the travelled yx-path plus the arc xz
    /// is added to the subtour in replace of the arc yz.
    ///
    /// The procedure is intended for metric undirected graphs, and then is
    /// 1-approximative, that is, the constructed tour is twice as long as an
    /// optimal tour in the worst case. The procedure works for all other kinds
    /// of complete graphs or digraphs, but does not produce good tours in the
    /// general setting.
    ///
    /// If no root node is specified, the procedure is repeated for all possible
    /// root nodes.
    ///
    /// If CT.methLocal==#LOCAL_OPTIMIZE, TSP_LocalSearch() is called for postprocessing.
    TFloat  TSP_HeuristicTree(TNode r=NoNode) throw(ERRange,ERRejected);

    /// \brief  Refine a given tour by local optimization
    ///
    /// \param pred  An array of predecessor arcs, describing an Hamiltonian cycle
    /// \return      The length of the final tour
    ///
    /// This takes a given tour and applies the procedures TSP_NodeExchange() and
    /// TSP_2Exchange() until no further improvement is achieved.
    TFloat  TSP_LocalSearch(TArc* pred) throw(ERRejected);

    /// \brief  Refine a given tour by a node exchange step
    ///
    /// \param pred   An array of predecessor arcs, describing an Hamiltonian cycle
    /// \param limit  The treshold for the acceptable change of tour length
    /// \retval true  If the tour has been changed
    ///
    /// This takes a given tour and searches for a node x can be profitably removed
    /// from the tour and reinserted at another place. At most one node is shifted!
    bool  TSP_NodeExchange(TArc* pred,TFloat limit=0) throw(ERRejected);

    /// \brief  Lower bounding procedure for the travelling salesman problem
    ///
    /// \param method          A #TRelaxTSP value specifying the applied bounding procedure
    /// \param root            A node index in the range [0,1,..,n-1] or NoNode
    /// \param bestUpper       The length of the constructed tour or InfFloat
    /// \param branchAndBound  True, if the procedure is called for branch & bound subproblems
    /// \return                The maximal observed reduced length of a one-cycle tree
    ///
    /// This dtermines minimum one-cycle trees again and again. The found trees
    /// are used as follows:
    /// (1) For computing heuristic tours as in TSP_HeuristicTree()
    /// (2) As lower bounds on the minimum tour length
    /// (3) To adjust the potentials of nodes with "wrong" degree.
    ///     (There are some minor differences betwwen the directed and
    ///     the undirected setting)
    /// In the (rare) happy case, the one-cycle tree becomes a Hamiltonian cycle
    /// after several iterations which is an optimal tour then. In the regular
    /// case, for method==#TSP_RELAX_FAST, duality gaps in the range of 1-2% result.
    /// Some instance, however, require method==#TSP_RELAX_SUBOPT for convergence.
    ///
    /// The final one-cycle tree is exported by the subgraph multiplicities,
    /// the best tour is exported by the predecessor labels. The according node
    /// potentials are also exported.
    virtual TFloat  TSP_SubOpt1Tree(abstractMixedGraph::TRelaxTSP method,TNode root,TFloat& bestUpper,
                        bool branchAndBound) throw(ERRange);

    /// \brief  Complete evaluation of a travelling salesman instance
    ///
    /// \param method       A #TRelaxTSP value specifying the applied bounding procedure
    /// \param nCandidates  If non-negative, only a candidate graph is evaluated
    /// \param root         A node index in the range [0,1,..,n-1] or NoNode
    /// \param upperBound   The length of the best tour known in advance
    /// \return             The length of the constructed tour or InfFloat
    ///
    /// This procedure can be used for optimality proofs, but also to construct
    /// good (usually optimal) tours from candidate graphs. For the first goal,
    /// nCandidates==-1 is required, and all branch & bound control parameters
    /// have to be set such that the search is not terminated prematurely. If the
    /// subproblem buffer capacity is exeeded, it may help out to increase the
    /// efforts for lower bounding (method). It is recommended to start a complete
    /// evaluation only after the subgradient optimization has proven a duality
    /// gap in the range of 1-2%. By our experience, instances up to 100-150 nodes
    /// can be evaluated.
    ///
    /// For the candidate graph evaluation, the following edges are selected:
    /// - All current predecessor arcs, practically defining a good tour known in advance
    /// - All current subgraph arcs, practically defining a one-cycle tree which
    ///   has been computed by subgradient optimization
    /// - The arcs of 20 random locally optimal tours
    /// - The nCandidates least length arcs adjacent with each node
    /// It is recommended to run the search twice, once with nCandidates==1 and once
    /// nCandidates==5.
    virtual TFloat  TSP_BranchAndBound(abstractMixedGraph::TRelaxTSP method,int nCandidates,
                        TNode root,TFloat upperBound) throw(ERRejected);

    /// @}


    /// \addtogroup maxCut
    /// @{

public:

    /// \brief  Alternative methods for the maximum cut solver
    enum TMethMaxCut {
        MAX_CUT_DEFAULT = -1,    ///< Apply the default method set in #goblinController::methMaxCut
        MAX_CUT_GRASP   =  0,    ///< Apply the 1/2 approximating GRASP heuristic method
        MAX_CUT_TREE    =  1,    ///< Apply the tree heuristic method
        MAX_CUT_TJOIN   =  2     ///< Apply the dual T-join method
    };

    /// \brief  Compute a maximum weight edge cut
    ///
    /// \param s  A left-hand node index or NoNode
    /// \param t  A right-hand node index or NoNode
    /// \return   The weight of the constructed edge cut
    TFloat  MaxCut(TNode s = NoNode,TNode t = NoNode) throw(ERRange);

    /// \brief  Compute a maximum weight edge cut
    ///
    /// \param method  A #TMethMaxCut value specifying the applied construction heuristic
    /// \param s       A left-hand node index or NoNode
    /// \param t       A right-hand node index or NoNode
    /// \return        The weight of the constructed edge cut
    ///
    /// This is the public interface to all maximum edge cut methods. The
    /// resulting edge cut is provided by the node colour register. As for
    /// other NP-hard problem solvers, the procedure does the following:
    /// - Check the current node colours for feasibility
    /// - Call MXC_Heuristic() to obtain an approximate solution
    /// - If methSolve>1, call MXC_BranchAndBound() to obtain an exact solution
    TFloat  MaxCut(TMethMaxCut method,TNode s = NoNode,TNode t = NoNode) throw(ERRange);

    /// \brief  Compute a maximum weight edge cut
    ///
    /// \param nodeColour  A pointer to an array of node colours
    /// \param s           A left-hand node index or NoNode
    /// \param t           A right-hand node index or NoNode
    /// \return            The weight of the improved edge cut
    ///
    /// This considers the edge cut defined by the nodeColour[] array. For every
    /// node, it is checked if moving the node to the other partition subset
    /// increases the weight. If such nodes exist, the node which admits the
    /// biggest increase is moved (greedy strategy).
    TFloat  MXC_LocalSearch(TNode* nodeColour,TNode s = NoNode,TNode t = NoNode) throw(ERRange,ERRejected);

protected:

    /// \brief  Approximate maximum weight edge cut
    ///
    /// \param method  A #TMethMaxCut value specifying the applied method
    /// \param s       A left-hand node index or NoNode
    /// \param t       A right-hand node index or NoNode
    /// \return        The weight of the constructed edge cut
    ///
    /// In this base class implementation, MXC_HeuristicGRASP() is called.
    virtual TFloat  MXC_Heuristic(abstractMixedGraph::TMethMaxCut method,
                        TNode s = NoNode,TNode t = NoNode) throw(ERRange,ERRejected);

    /// \brief  Approximate maximum weight edge cut by using a GRASP scheme
    ///
    /// \param s  A left-hand node index or NoNode
    /// \param t  A right-hand node index or NoNode
    /// \return   The weight of the constructed edge cut
    ///
    /// This assigns colours to the nodes step by step. In each iteration,
    /// a candidate list with a few nodes is generated and from this list,
    /// an arbitrary node is chosen. Then, this node is always added to the
    /// most profitable component.
    ///
    /// If the graph is undirected, the cut weight is at least 1/2 of the sum of arc weights.
    ///
    /// If CT.methLocal==LOCAL_OPTIMIZE, MXC_LocalSearch() is called for postprocessing.
    TFloat  MXC_HeuristicGRASP(TNode s = NoNode,TNode t = NoNode) throw(ERRange);

    /// \brief  Exact solution of the maximum weight edge cut problem
    ///
    /// \param s           A left-hand node index or NoNode
    /// \param t           A right-hand node index or NoNode
    /// \param lowerBound  An initial bound for pruning the branch tree
    /// \return            The weight of the improved edge cut
    ///
    /// This implements a branch and bound scheme. But due to the poor problem
    /// relexation, it is basically enumeration.
    TFloat  MXC_BranchAndBound(TNode s = NoNode,TNode t = NoNode,TFloat lowerBound = InfFloat)
                throw(ERRange);

    /// @}


    /// \addtogroup groupMipReductions
    /// @{

public:

    /// \brief  Transform a min-cost b-flow instance to an LP
    ///
    /// \return   A pointer to the generated object
    managedObject*  BFlowToLP() throw();

    /// \brief  Transform a layer assignment problem instance to an LP
    ///
    /// \param nodeLayer  NULL or a partial assignment of nodes to grid rows
    /// \return           A pointer to the generated object or NULL
    managedObject*  VerticalCoordinatesModel(TNode* nodeLayer) throw();

    /// \brief  Transform a horizontal coordinate assignment problem instance to an LP
    ///
    /// \return   A pointer to the generated object
    managedObject*  HorizontalCoordinatesModel() throw();

    /// \brief  Transform a stable set instance to a MIP
    ///
    /// \return  A pointer to the generated object
    managedObject*  StableSetToMIP() throw();

    /// @}


    /// \brief  Unconditional display of graph objects
    ///
    /// This always generates some output, depending on the value of displayMode.
    /// Either TextDisplay() is called, or a trace file is written.
    virtual char*  Display() const throw(ERRejected,ERFile);


    /// \addtogroup groupTransscript
    /// @{

public:

    /// \brief  Send a text display in an ASCII format to the transcript channel
    ///
    /// \param i  The minimum displayed node index or NoNode
    /// \param j  The maximum displayed node index or NoNode
    ///
    /// This dumps the graph object with all registers in a node oriented format.
    /// Nodes are listed with all incident arcs. If neither i nor j are specified,
    /// all nodes are listed. If only i is specified, only this node is written.
    /// The format is the same as for ExportToAscii().
    void  TextDisplay(TNode i=NoNode,TNode j=NoNode) const throw();

    /// \brief  Send a path encoded into the predecessor labels to the transcript channel
    ///
    /// \param u  The path start node
    /// \param v  The path start node
    void  DisplayPath(TNode u,TNode v) throw(ERRange,ERRejected);

    /// \brief  Send the cut induced by the distance labels to the transcript channel
    ///
    /// \param separator  The minimum distance value in the right-hand side of the cut
    /// \return           The cut capacity
    TFloat  CutCapacity(TNode separator=NoNode) throw(ERCheck);

    /// @}


    /// \addtogroup groupCanvasBuilder
    /// @{

public:

    /// \brief  Export a graph object to a xfig canvas file
    ///
    /// \param fileName  The destination file name
    void ExportToXFig(const char* fileName) const throw(ERFile);

    /// \brief  Export a graph object to a tk canvas file
    ///
    /// \param fileName  The destination file name
    void ExportToTk(const char* fileName) const throw(ERFile);

    /// \brief  Export a graph object to a graphviz dot file
    ///
    /// \param fileName  The destination file name
    void ExportToDot(const char* fileName) const throw(ERFile);

    /// @}


    /// \addtogroup groupTextDisplay
    /// @{

public:

    /// \brief  Export a graph object to a xfig canvas file
    ///
    /// \param fileName  The destination file name
    /// \param format    For graph objects, a dummy parameter
    ///
    /// This dumps the graph object with all registers in a node oriented format.
    /// Nodes are listed with all incident arcs. The format is the same as for
    /// TextDisplay().
    void ExportToAscii(const char* fileName,TOption format = 0) const throw(ERFile);

    /// @}

};


/// \addtogroup groupInvestigators
/// @{

/// \brief  The default implementation of graph investigators

class iGraph : public virtual investigator
{
private:

    const abstractMixedGraph &G;
    TNode                   n;
    TArc *                  current;

public:

    iGraph(const abstractMixedGraph &GC) throw();
    ~iGraph() throw();

    unsigned long   Size() const throw();
    unsigned long   Allocated() const throw();

    void            Reset() throw();
    void            Reset(TNode v) throw(ERRange);
    TArc            Read(TNode v) throw(ERRange,ERRejected);
    TArc            Peek(TNode v) throw(ERRange,ERRejected);
    bool            Active(TNode v) const throw(ERRange);

};

/// @}


/// \addtogroup groupIndexSets
/// @{

/// \brief Index sets representing graph nodes with non-zero demand

class demandNodes : public indexSet<TNode>
{
private:

    const abstractMixedGraph& G;

public:

    demandNodes(const abstractMixedGraph &_G) throw() :
        managedObject(_G.Context()),indexSet<TNode>(_G.N(),_G.Context()),G(_G) {};
    ~demandNodes() throw() {};

    unsigned long   Size() const throw()
        {return sizeof(demandNodes) + managedObject::Allocated();};

    bool IsMember(const TNode i) const throw(ERRange)
        {return (G.Demand(i)>0);};

};


/// \brief Index sets representing graph nodes with a specified colour

class colouredNodes : public indexSet<TNode>
{
private:

    const abstractMixedGraph& G;
    TNode                     c;

public:

    colouredNodes(const abstractMixedGraph &_G,TNode _c) throw() :
        managedObject(_G.Context()),indexSet<TNode>(_G.N(),_G.Context()),
        G(_G),c(_c) {};
    ~colouredNodes() throw() {};

    unsigned long   Size() const throw()
        {return sizeof(colouredNodes) + managedObject::Allocated();};

    bool IsMember(const TNode i) const throw(ERRange)
        {return (G.NodeColour(i) == c);};

};


/// \brief Index sets representing graph nodes with one of two specified colours

class kempeNodes : public indexSet<TNode>
{
private:

    const abstractMixedGraph& G;
    TNode                     c1;
    TNode                     c2;

public:

    kempeNodes(const abstractMixedGraph &_G,TNode _c1,TNode _c2) throw() :
        managedObject(_G.Context()),indexSet<TNode>(_G.N(),_G.Context()),
        G(_G),c1(_c1),c2(_c2) {};
    ~kempeNodes() throw() {};

    unsigned long   Size() const throw()
        {return sizeof(kempeNodes) + managedObject::Allocated();};

    bool IsMember(const TNode i) const throw(ERRange)
        {return (G.NodeColour(i)==c1 || G.NodeColour(i)==c2);};

};


/// \brief Index sets representing graph arcs with a specified colour

class colouredArcs : public indexSet<TArc>
{
private:

    const abstractMixedGraph& G;
    TArc                      c;

public:

    colouredArcs(const abstractMixedGraph &_G,TArc _c) throw() :
        managedObject(_G.Context()),indexSet<TArc>(_G.M(),_G.Context()),
        G(_G), c(_c) {};
    ~colouredArcs() throw() {};

    unsigned long Size() const throw()
        {return sizeof(colouredArcs) + managedObject::Allocated();};

    bool IsMember(const TArc i) const throw(ERRange)
        {return (G.EdgeColour(i<<1) == c);};

};


/// \brief Index sets representing non-blocking arcs

class nonBlockingArcs : public indexSet<TArc>
{
private:

    const abstractMixedGraph& G;

public:

    nonBlockingArcs(const abstractMixedGraph &_G) throw() :
        managedObject(_G.Context()),indexSet<TArc>(2*_G.M(),_G.Context()),
        G(_G) {};
    ~nonBlockingArcs() throw() {};

    unsigned long Size() const throw()
        {return sizeof(nonBlockingArcs) + managedObject::Allocated();};

    bool IsMember(const TArc i) const throw(ERRange)
        {return (G.UCap(i)>0 && !(G.Blocking(i)));};

};


/// \brief Index sets representing subgraph arcs

class subgraphArcs : public indexSet<TArc>
{
private:

    const abstractMixedGraph& G;

public:

    subgraphArcs(const abstractMixedGraph &_G) throw() :
        managedObject(_G.Context()),indexSet<TArc>(2*_G.M(),_G.Context()),
        G(_G) {};
    ~subgraphArcs() throw() {};

    unsigned long Size() const throw()
        {return sizeof(subgraphArcs) + managedObject::Allocated();};

    bool IsMember(const TArc i) const throw(ERRange)
        {return (G.Sub(i)>0 && !(G.Blocking(i)));};

};


/// \brief Arc index set induced by a specified node index set

class inducedArcs : public indexSet<TArc>
{
private:

    const abstractMixedGraph& G;
    const indexSet<TNode>& nodeSet;

public:

    inducedArcs(const abstractMixedGraph &_G,const indexSet<TNode> &_nodeSet)  throw() :
        managedObject(_G.Context()),indexSet<TArc>(2*_G.M(),_G.Context()),
        G(_G),nodeSet(_nodeSet) {};
    ~inducedArcs() throw() {};

    unsigned long Size() const throw()
        {return sizeof(inducedArcs) + managedObject::Allocated();};

    bool IsMember(TArc i) const throw(ERRange)
        {return (nodeSet.IsMember(G.StartNode(i)) && nodeSet.IsMember(G.EndNode(i)));};

};

/// @}


inline bool abstractMixedGraph::IsRepresented() const throw()
{
    return IsSparse() || IsDense();
}


inline bool abstractMixedGraph::IsEmbedded() const throw()
{
    return (face!=NULL);
}


inline TNode abstractMixedGraph::N() const throw()
{
    return n;
}


inline TNode abstractMixedGraph::L() const throw()
{
    return n+ni;
}


inline TNode abstractMixedGraph::NI() const throw()
{
    return ni;
}


inline TArc abstractMixedGraph::M() const throw()
{
    return m;
}


inline TNode abstractMixedGraph::ND() const throw()
{
    return m-n+2;
}


inline TNode abstractMixedGraph::NodeAnchorPoint(TNode v) const throw(ERRange)
{
    return v;
}


// The following generic implementation lets the code for type <int>
// apply to all enum types of the canvasBuilder class
template <typename T> inline bool abstractMixedGraph::GetLayoutParameter(
    TOptLayoutTokens token,T& value,TLayoutModel model) const throw()
{
    int tmpValue;

    if (!GetLayoutParameter(token,tmpValue,model)) return false;

    value = static_cast<T>(tmpValue);
    return true;
}


template <> inline bool abstractMixedGraph::GetLayoutParameter(
    TOptLayoutTokens token,int& value,TLayoutModel model) const throw()
{
    return GetLayoutParameterImpl(token,value,model);
}


template <> inline bool abstractMixedGraph::GetLayoutParameter(
    TOptLayoutTokens token,double& value,TLayoutModel model) const throw()
{
    return GetLayoutParameterImpl(token,value,model);
}


template <> inline bool abstractMixedGraph::GetLayoutParameter(
    TOptLayoutTokens token,char*& value,TLayoutModel model) const throw()
{
    return GetLayoutParameterImpl(token,value,model);
}


// The following generic implementation lets the code for type <int>
// apply to all enum types of the canvasBuilder class
template <typename T> inline
bool abstractMixedGraph::SetLayoutParameter(TOptLayoutTokens token,const T value,TLayoutModel model) throw()
{
    if (SetLayoutParameterImpl(token,static_cast<int>(value),model)) return true;

    return SetLayoutParameterImpl(token,static_cast<double>(value),model);
}


template <> inline bool abstractMixedGraph::SetLayoutParameter(
    TOptLayoutTokens token,const double value,TLayoutModel model) throw()
{
    return SetLayoutParameterImpl(token,value,model);
}


template <> inline bool abstractMixedGraph::SetLayoutParameter(
    TOptLayoutTokens token,const char* value,TLayoutModel model) throw()
{
    return SetLayoutParameterImpl(token,value,model);
}


template <typename T> inline
bool abstractMixedGraph::SetLayoutParameter(TOptLayoutTokens token,const T value) throw()
{
    attributePool* layoutData = LayoutData();

    if (!layoutData) return false;

    return SetLayoutParameter(token,value,LayoutModel());
}


inline attributePool* abstractMixedGraph::Registers() const throw()
{
    return const_cast<attributePool*>(&registers);
}


inline TArc abstractMixedGraph::Reverse(TArc a) const throw(ERRange)
{
    return a^1;
}


inline TFloat abstractMixedGraph::Divergence(TNode v) throw(ERRange)
{
    return DegIn(v)-DegOut(v);
}


inline bool abstractMixedGraph::ShortestPath(TNode source,TNode target)
    throw(ERRange,ERRejected)
{
    return ShortestPath(SPX_DEFAULT,SPX_ORIGINAL,nonBlockingArcs(*this),source,target);
}


inline TCap abstractMixedGraph::EdgeConnectivity(TNode source,TNode target) throw(ERRange)
{
    return EdgeConnectivity(MCC_DEFAULT,source,target);
}


inline TCap abstractMixedGraph::StrongEdgeConnectivity(TNode source,TNode target) throw(ERRange)
{
    return StrongEdgeConnectivity(MCC_DEFAULT,source,target);
}


inline TFloat abstractMixedGraph::MinTree(TNode root) throw(ERRange,ERRejected)
{
    return MinTree(MST_DEFAULT,MST_PLAIN,root);
}


inline TFloat abstractMixedGraph::MaxFlow(TNode source,TNode target)
    throw(ERRange,ERRejected)
{
    return MaxFlow(MXF_DEFAULT,source,target);
}


inline TFloat abstractMixedGraph::MinCostSTFlow(TNode source,TNode target)
    throw(ERRange,ERRejected)
{
    return MinCostSTFlow(MCF_ST_DEFAULT,source,target);
}


inline TFloat abstractMixedGraph::TSP(TNode root)
    throw(ERRange,ERRejected)
{
    TRelaxTSP methRelax1 = (CT.methSolve>0) ? TSP_RELAX_DEFAULT : TSP_RELAX_NULL;
    TRelaxTSP methRelax2 = (CT.methSolve>1) ? TSP_RELAX_DEFAULT : TSP_RELAX_NULL;

    return TSP(TSP_HEUR_DEFAULT,methRelax1,methRelax2,root);
}


inline TFloat abstractMixedGraph::MaxCut(TNode s,TNode t) throw(ERRange)
{
    return MaxCut(MAX_CUT_DEFAULT,s,t);
}


#endif
