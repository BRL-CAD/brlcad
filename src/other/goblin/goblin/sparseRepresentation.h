
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, September 1999
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file sparseRepresentation.h
/// \brief #sparseRepresentation class interface

#ifndef _SPARSE_REPRESENTATION_H_
#define _SPARSE_REPRESENTATION_H_

#include "graphRepresentation.h"
#include "canvasBuilder.h"


/// \brief  The representational objects from which sparse graph objects are composed
///
/// The objects support incidence lists, the dynamic insertion and deletion of
/// arcs and control points for the arcs

class sparseRepresentation : public graphRepresentation
{
protected:

    TNode*          SN;     ///< Start node indices
    TArc*           right;  ///< Right-hand incidences
    mutable TArc*   left;   ///< Left-hand incidences
    TArc*           first;  ///< Entry points for the circular incidence lists
    TFloat*         sub;    ///< Subgraph multiplicities

public:

    sparseRepresentation(const abstractMixedGraph& _G) throw();
    ~sparseRepresentation() throw();

    /// \brief Read the incidence lists from a file
    ///
    /// \param F  An import object representing a file
    ///
    /// This allocates and reads the start node indices and incidence lists.
    /// The operation may happen only in an initial state with mMax = nMax = 0
    void ReadIncidences(goblinImport& F) throw(ERParse);

    /// \brief Set the attribute capacities to the expected object dimensions
    ///
    /// \param _n  The expected number of graph nodes
    /// \param _m  The expected number of arcs
    /// \param _l  The expected number of layout points
    ///
    /// This avoids repeated reallocation of the attributes
    void  SetCapacity(TNode _n,TArc _m,TNode _l=NoNode) throw(ERRejected);

    unsigned long   Size() const throw();
    unsigned long   Allocated() const throw();


    /// \addtogroup manipSkeleton
    /// @{

    /// \brief Add an arc with specified attribute values
    ///
    /// \param v   A node index ranged [0,1,..,nAct-1]
    /// \param w   A node index ranged [0,1,..,nAct-1]
    /// \param _u  The desired upper capacity bound
    /// \param _l  The desired lower capacity bound
    /// \param _c  The desired arc length
    /// \return  The index mAct-1 of the generated arc
    ///
    /// This operation adds an arc connecting v and w, inserts it into to the
    /// incidence lists of v and w, and assigns the given attribute values. All
    /// attribute pools are updated.
    TArc  InsertArc(TNode v,TNode w,TCap _u,TFloat _c,TCap _l) throw(ERRange,ERRejected);

    /// \brief Add a graph node
    ///
    /// \return   The index nAct-1 of the generated graph node
    ///
    /// This operation adds a graph node without any incident arcs. All attribute
    /// pools are updated. The indices of layout points are possibly invalidated!
    TNode  InsertNode() throw(ERRejected);

    /// \brief Replace all arcs with capacity > 1 by a bunch of arcs with the same cardinality
    ///
    /// This also distributes the lower capacities and the subgraph multiplicities
    /// among the mapped arcs. Subgraph node degrees and edge cut capacities are
    /// preserved.
    void  ExplicitParallels() throw();

    /// \brief Swap the indices of two given arcs
    ///
    /// \param a1  An arc index ranged [0,1,..,2*mAct-1]
    /// \param a2  An arc index ranged [0,1,..,2*mAct-1]
    ///
    /// This swaps all attribute values and incidence features of the given arcs.
    /// If control points exist, this operation may revert the sequence of control points
    /// and by that, is not O(1). To avoid such overhead, it is recommended only
    /// to swap forward arcs.
    void  SwapArcs(TArc a1,TArc a2) throw(ERRange);

    /// \brief Swap the indices of two given nodes
    ///
    /// \param u  A node index ranged [0,1,..,nAct-1]
    /// \param v  A node index ranged [0,1,..,nAct-1]
    ///
    /// This swaps all attribute values and incidence features of the given nodes.
    /// Because the two incidence lists are traversed, this operation is not O(1).
    void  SwapNodes(TNode u,TNode v) throw(ERRange);

    /// \brief Revert the implicit orientation of a specified arc
    ///
    /// \param a  An arc index ranged [0,1,..,2*mAct-1]
    ///
    /// If control points exist, this operation also reverts the sequence of control points
    /// and by that, is not O(1).
    void  FlipArc(TArc a) throw(ERRange) {SwapArcs(a,a^1);};

    /// \brief Mark an arc for deletion
    ///
    /// \param a  An arc index ranged [0,1,..,2*mAct-1]
    ///
    /// This deletes the arc from the incidence list of its end nodes irreversibly,
    /// but does not invalidate any arc indices. The operation must be followed by
    /// a DeleteArcs() call for reindexing. Canceled arcs have undefined start
    /// nodes which can be used to distinguish them from regular arcs.
    ///
    /// Where necessary, the cancel operation also updates the node degrees, node
    /// adjacencies and other data structures held by the graph object.
    void  CancelArc(TArc a) throw(ERRange,ERRejected);

    /// \brief Mark a graph node for deletion
    ///
    /// \param v  A node index ranged [0,1,..,nAct-1]
    ///
    /// This cancels all incident arcs and marks the node as hidden. The operation
    /// must be followed by a DeleteNodes() call for reindexing. Canceled arcs are
    /// not distinguished from other nodes without incidences.
    void  CancelNode(TNode v) throw(ERRange);

    /// \brief Eliminate all control points assigned with a given edge
    ///
    /// \param a      An arc index ranged [0,1,..,2*mAct-1]
    /// \retval true  If some layout node has been deleted
    ///
    /// This operation deletes the arc label anchor point and all control points
    /// assigned with the arc a. The indices of the remaining layout nodes are
    /// invalidated!
    bool  ReleaseEdgeControlPoints(TArc a) throw(ERRange);

    /// \brief Eliminate all collocated control points assigned with edges
    ///
    /// \param portMode  The mode for displaying arc ends
    /// \retval true     If some control point has been deleted
    ///
    /// This operation traverses all control points assigned with the edges,
    /// checks for pairs of consecutive control points with the same position
    /// in the drawing and occasionally deletes one of these control points.
    /// The check includes the end nodes of all edges. Note that the indices
    /// of the remaining layout nodes are invalidated!
    bool  ReleaseDoubleEdgeControlPoints(TPortMode portMode) throw();

    /// \brief Eliminate all spanned layout nodes assigned with arcs
    ///
    /// \param portMode  The mode for displaying arc ends
    /// \retval true     If some control point has been deleted
    ///
    /// This operation traverses all control points assigned with the arcs, checks
    /// for triples of consecutive control points which are collinear and occasionally
    /// deletes the second of these points. If portMode==PORTS_EXPLICIT, this check
    /// includes the real end nodes.
    ///
    /// Control points which are hidden by the box representation of some end node
    /// are also deleted - independently from the portMode.
    ///
    /// The indices of the remaining layout nodes are invalidated!
    bool  ReleaseCoveredEdgeControlPoints(TPortMode portMode) throw();

    /// \brief Eliminate all layout nodes assigned with a given node
    ///
    /// \param v  A node index ranged [0,1,..,nAct-1]
    ///
    /// This operation deletes all control points assigned with the node v.
    /// The indices of the remaining layout nodes are invalidated!
    bool  ReleaseNodeControlPoints(TNode v) throw(ERRange);

    ///
    /// \param a  An arc index ranged [0,1,..,2m-1]
    ///

    /// \brief  Instant deletion of a specified arc
    ///
    /// \param a  An arc index ranged [0,1,..,2*mAct-1]
    ///
    /// This is equivalent with calling CancelArc(a) followed by DeleteArcs()
    /// up to other cancelled arcs which are not deleted here.
    /// This operation invalidates all indices of other arcs!
    void  DeleteArc(TArc a) throw(ERRange);

    /// \brief Delete a specified graph node and reindex some other node
    ///
    /// \param v  A node index ranged [0,1,..,nAct-1]
    ///
    /// This is equivalent with calling CancelNode(v) followed by DeleteNodes()
    /// up to other zero degree nodes which are not deleted. Note that all
    /// cancelled arcs are deleted, not just those which are incident with v.
    void  DeleteNode(TNode v) throw(ERRange);

    /// \brief Finalize the deletion of canceled arcs
    ///
    /// This causes a reindexing of all arcs by which the canceled arcs are omitted.
    void  DeleteArcs() throw();

    /// \brief Finalize the deletion of canceled nodes
    ///
    /// This causes a reindexing of all nodes by which the nodes without
    /// incidences are omitted.
    void  DeleteNodes() throw();

private:

    /// \brief Delete a specified layout node and reindex some other layout node
    ///
    /// \param v  A node index ranged [nAct,..,lAct-1]
    ///
    /// This is only a helper procedure. It causes a reindexing of all nodes by
    /// which the node v is omitted. In order to delete v completely, use DeleteNode()
    void  EraseLayoutNode(TNode v) throw(ERRange);

public:

    /// \brief Contract a specified arc to a single node
    ///
    /// \param a  An arc index ranged [0,1,..,2*mAct-1]
    ///
    /// This merges the incidence lists of the end nodes of the arc a, attaches
    /// the resulting list to the start node and cancels the end node and the
    /// arc itself. Planarity is maintained by this operation.
    void  ContractArc(TArc a) throw(ERRange,ERRejected);

    /// \brief Identify two graph nodes
    ///
    /// \param u  A node index ranged [0,1,..,nAct-1]
    /// \param v  A node index ranged [0,1,..,nAct-1]
    ///
    /// This merges the incidence lists of the nodes u and v, attaches the
    /// resulting list to node u and cancels the node v.
    void  IdentifyNodes(TNode u,TNode v) throw(ERRange,ERRejected);

    /// \brief  Assign a specified node index order
    ///
    /// \param key  An array of node key values
    ///
    /// Reorder the node indices based on the values specified in key[]. Nodes
    /// with smaller key values obtain smaller indices. For equal key values,
    /// the tie breaking is arbitrary. This operation invalidates all node indices.
    void  ReorderNodeIndices(const TFloat* key) throw();

    /// \brief  Assign a specified edge index order
    ///
    /// \param key  An array of edge key values
    ///
    /// Reorder the edge indices based on the values specified in key[]. Edges
    /// with smaller key values obtain smaller indices. For equal key values,
    /// the tie breaking is arbitrary. This operation invalidates all arc indices.
    void  ReorderEdgeIndices(const TFloat* key) throw();

    /// @}


    /// \addtogroup queryIncidences
    /// @{

    /// \brief Query the start node of a given arc
    ///
    /// \param a  An arc index ranged [0,1,..,2*mAct-1]
    /// \return   The index of the start node
    TNode  StartNode(TArc a) const throw(ERRange);

    /// \brief Query the end node of a given arc
    ///
    /// \param a  An arc index ranged [0,1,..,2*mAct-1]
    /// \return   The index of the end node
    TNode  EndNode(TArc a) const throw(ERRange);

    /// \brief Query the successor of a given arc in the incidence list of its start node
    ///
    /// \param a  An arc index ranged [0,1,..,2*mAct-1]
    /// \return   The index of the right-hand incident arc
    TArc  Right(TArc a) const throw(ERRange);

    /// \brief Query the predecessor of a given arc in the incidence list of its start node
    ///
    /// \param a  An arc index ranged [0,1,..,2*mAct-1]
    /// \return   The index of the left-hand incident arc
    ///
    /// Other than the right-hand incidence, left-hand incidences are generated
    /// only on demand. The initialization is O(mAct).
    TArc  Left(TArc a) const throw(ERRange);

    /// \brief Dismiss the left-hand incidence lists
    ///
    /// Deletion is only for sake of efficiency. The reverse incidence lists
    /// are rebuilt by any subsequent call to Left().
    void  ReleaseReverseIncidences() throw();

    /// \brief Retrieve an arc with a given start node
    ///
    /// \param v  An arc index ranged [0,1,..,nAct-1]
    /// \return   The index of an outgoing arc
    ///
    /// The First(v) incidence is somewhat arbitrary, since icidence lists are
    /// cyclic. But when an arc is inserted, it is inserted into the incidence
    /// lists right after the First() incidence. If the graph is planely represented,
    /// an exterior face is assigned, and v is an exterior node, then the first
    /// two incidences are exterior arcs, and inserting an arc with two exterior
    /// end nodes preserves the planar representation.
    TArc  First(TNode v) const throw(ERRange);

    /// @}


    /// \addtogroup manipIncidences
    /// @{

    /// \brief Move an arc in the incidence list of its start node
    ///
    /// \param a1  An arc index ranged [0,1,..,2*mAct-1]
    /// \param a2  An arc index ranged [0,1,..,2*mAct-1]
    /// \param a3  An optional arc index ranged [0,1,..,2*mAct-1]
    ///
    /// This makes a2 the successor of a1 in the start nodes' incidence list
    /// and makes the original successor of a1 the new successor of a3. If only
    /// two arguments are specified, a2 == a3 is assumed. Provided that the
    /// previous right-hand incidence order is .., a1, .., a2, .., a3, ..,
    /// a circular incidence list is maintained.
    void  SetRight(TArc a1,TArc a2,TArc a3=NoArc) throw(ERRange,ERRejected);

    /// \brief Set the first entry of a circular incidence list
    ///
    /// \param v  A node index ranged [0,1,..,nAct-1]
    /// \param a  An arc index ranged [0,1,..,2*mAct-1]
    void  SetFirst(TNode v,TArc a) throw(ERRange,ERRejected);

    /// \brief Adjust the end nodes of a given arc
    ///
    /// \param a  An arc index ranged [0,1,..,2*mAct-1]
    /// \param u  A node index ranged [0,1,..,nAct-1]
    /// \param v  A node index ranged [0,1,..,nAct-1]
    ///
    /// This calls SetRouting() and updates / invalidates some data
    /// structures of the assiciated graph object.
    void  RouteArc(TArc a,TNode u,TNode v) throw(ERRange,ERRejected);

private:

    /// \brief Adjust the end nodes of a given arc
    ///
    /// \param a  An arc index ranged [0,1,..,2*mAct-1]
    /// \param u  A node index ranged [0,1,..,nAct-1]
    /// \param v  A node index ranged [0,1,..,nAct-1]
    ///
    /// This assign to the arc a the start node u and the end node v. The method
    /// also inserts a to the incidence list of u right after First(u),
    /// and inserts a^1 to the incidence list of v right after First(v).
    /// This method applies to cancelled arcs as well.
    void  SetRouting(TArc af,TNode u,TNode v) throw();

public:

    /// \brief General manipulation of the node incidence order
    ///
    /// \param predArc       For every arc, the arc which will occur left hand
    ///                      in the resulting incidence order
    /// \param nodeOriented  If true, for every arc a, both a and predArc[a] share
    ///                      the start point, and predArc[a] is considered to occur
    ///                      counter-clockwise of a. Otherwise, the end node of
    ///                      predArc[a] is the start node of a, and predArc[a] is
    ///                      is the predecessor of a when traveling around a face
    ///                      in an imagined plane straight line drawing
    ///
    /// Reorder all node incidences as specified in predArc[].
    /// This operation applies to sparse represented graphs only.
    /// The operation is virtually synchronous for all nodes.
    void  ReorderIncidences(const TArc* predArc,bool nodeOriented = false) throw();

    /// @}


    /// \addtogroup groupArcRouting
    /// @{

private:

    /// \brief Allocate a layout point
    ///
    /// \return   The index of generated layout point
    TNode  InsertLayoutPoint() throw(ERRejected);

public:

    /// \brief Insert a layout point after a specified control point
    ///
    /// \param x  The index of the predecessor layout point
    /// \return   The index of generated layout point
    ///
    /// This allocates a layout point and inserts it into a sequence of control
    /// points right after x.
    TNode  InsertThreadSuccessor(TNode x) throw(ERRejected,ERRange);

    /// \brief Retrieve the arc label alignment point of a given arc
    ///
    /// \param a  An arc index ranged [0,1,..,2*mAct-1]
    /// \return   The index of the label anchor point of a or NoNode
    TNode  ArcLabelAnchor(TArc a) const throw(ERRange);

    /// \brief Check if any arc label display anchor points exist
    ///
    /// \retval true  No arc label anchor points are allocated
    bool  NoArcLabelAnchors() const throw();

    /// \brief Retrieve the thread successor point of a given layout point
    ///
    /// \param x  A node index ranged [0,1,..,lAct-1]
    /// \return   The index of the successor point of x or NoNode
    TNode  ThreadSuccessor(TNode x) const throw(ERRange);

    /// \brief Check if any thread successors exist
    ///
    /// \retval true  If no thread successor nodes are allocated
    bool  NoThreadSuccessors() const throw();

    /// \brief Insert a layout point into a sequence of control points
    ///
    /// \param a  An arc index ranged [0,1,..,2*mAct-1]
    /// \param x  Index of the predecessor control point
    /// \return   The index of generated control point
    ///
    /// This adds a control point for the arc a, right after the node x or right
    /// before x (if a is a backward arc). It is verified that x actually is
    /// one of the end nodes or a control point on a. If not already present,
    /// an arc label anchor point is also added.
    TNode  InsertArcControlPoint(TArc a,TNode x) throw(ERRange,ERRejected);

    /// \brief  Get the sequence of control points of a given arc
    ///
    /// \param a           An arc index ranged [0,1,..,2m-1]
    /// \param layoutNode  An array to be filled with the control point indices
    /// \param length      The dimension of layoutNode[]
    /// \param portMode    If PORTS_IMPLICIT, the incident graph nodes are also listed
    /// \return            The number of control points incident with a
    ///
    /// This respects the arc orientations. That is, for an odd index a,
    /// the list starts with the final control point instead of the first one.
    TNode  GetArcControlPoints(TArc a,TNode* layoutNode,TNode length,TPortMode portMode) const throw(ERRange);

    /// \brief Provide a label anchor point for a specified arc
    ///
    /// \param a  An arc index ranged [0,1,..,2*mAct-1]
    /// \return   The index of the arc label alignment point
    TNode  ProvideArcLabelAnchor(TArc a) throw(ERRejected,ERRange);

    /// \brief Provide port nodes for a specified arc
    ///
    /// \param a  An arc index ranged [0,1,..,2*mAct-1]
    /// \return   The index of the port node
    ///
    /// Ports are allocated always in pairs, for both end nodes of the given arc.
    /// So it does not matter whether this method is called first for the forward
    /// arc or for the backward arc.
    /// The returned port node index depends on the arc direction:
    /// - For forward arcs, the first edge control point index is returned
    /// - For backward arcs, the final edge control point index is returned
    TNode  ProvidePortNode(TArc a) throw(ERRejected,ERRange);

    /// \brief  Provide a specified number of control points for a specified edge
    ///
    /// \param a           An edge index ranged [0,1,..,mAct-1]
    /// \param layoutNode  An array to be filled with the control points
    /// \param length      The desired number of control points
    /// \param portMode    If PORTS_IMPLICIT, the incident graph nodes are also counted
    void  ProvideEdgeControlPoints(TArc a,TNode* layoutNode,TNode length,TPortMode portMode) throw(ERRange);

    /// \brief Proper drawing of parallel arcs and loops
    ///
    /// \param spacing    If positive, the achieved distance between two parallel arcs.
    ///                   Otherwise, the bendSpacing layout parameter applies
    /// \param drawLoops  If true, loops are also routed visibly
    ///
    /// This method releases the existing control points and then inserts new ones so that
    /// parallel arcs become distinguishable and, occasionally, loops become visible
    void  Layout_ArcRouting(TFloat spacing = 0,bool drawLoops = true) throw();

    /// \brief Place control points on long arcs so that layered drawing methods can apply
    ///
    /// \param spacing  If positive, the achieved vertical distance between two
    ///                 (bend) nodes. Otherwise, the bendSpacing parameter applies
    void  Layout_SubdivideArcs(TFloat spacing = 0.0) throw();

    /// \brief Adopt the arc routing of a supergraph
    ///
    /// \param G  A supergraph which has been drawn in advance
    ///
    /// In the supergraph, the original arcs must be numbered 0,1,..,m-1.
    /// Practically, the node routing must be also synchronized somehow
    void  Layout_AdoptArcRouting(abstractMixedGraph& G) throw();

    /// \brief Align arc labels with the current arc routing
    ///
    /// Remove unused arc label positions and move the maintained ones
    /// to the position of the first control point of the respective edge
    void  Layout_OrthoAlignArcLabels() throw();

    /// @}


    /// \addtogroup groupLayoutBounding
    /// @{

    /// \brief  Set a bounding interval
    ///
    /// \param i     A component of the current drawing
    /// \param cMin  A lower bounding value for this component of the current drawing
    /// \param cMax  An upper bounding value for this component of the current drawing
    ///
    /// For the given component, the bounding interval is frozen. That is, the
    /// dynamic bounding box computation implemented in Layout_GetBoundingInterval()
    /// does not apply any longer.
    void  Layout_SetBoundingInterval(TDim i,TFloat cMin,TFloat cMax) throw();

    /// \brief  Copy the bounding box from another graph object
    ///
    /// \param G  The graph object to copy from
    ///
    /// This is used by graph copy constructors to inherit the bounding box from
    /// the original graph object. Especially for proper subgraphs the node
    /// placement is the same as for the original graph (other than for the
    /// default bounding box rule.
    void  Layout_AdoptBoundingBox(abstractMixedGraph& G) throw();

    /// \brief  Retrieve the absolute position range of a graph node
    ///
    /// \param v     A graph node index
    /// \param i     A coordinate index
    /// \param cMin  Hereby, the lower absolute position bound is returned
    /// \param cMax  Hereby, the upper absolute position bound is returned
    void  Layout_GetNodeRange(TNode v,TDim i,TFloat& cMin,TFloat& cMax) throw();

    /// \brief  Retrieve the absolute position range of a graph node
    ///
    /// \param v     A graph node index
    /// \param i     A coordinate index
    /// \param cMin  A lower absolute position bound
    /// \param cMax  An upper absolute position bound
    void  Layout_SetNodeRange(TNode v,TDim i,TFloat cMin,TFloat cMax) throw();

    /// @}


    /// \addtogroup subgraphManagement
    /// @{

    void  NewSubgraph() throw(ERRejected);
    void  ReleaseSubgraph() throw();
    TFloat  Sub(TArc a) const throw(ERRange);
    void  SetSub(TArc a,TFloat multiplicity) throw(ERRange,ERRejected);
    void  SetSubRelative(TArc a,TFloat lambda) throw(ERRange,ERRejected);

    /// @}

};


#endif
