
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, December 1998
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   sparseGraph.h
/// \brief  Class interface to sparse undirected graph objects

#if !defined(_SPARSE_GRAPH_H_)
#define _SPARSE_GRAPH_H_

#include "abstractGraph.h"
#include "sparseRepresentation.h"


/// \brief A class for undirected graphs represented with incidence lists

class sparseGraph : public abstractGraph
{
    friend class sparseRepresentation;

private:

    bool mode;

protected:

    sparseRepresentation X;

public:

    /// \brief  Default constructor for sparse undirected graphs
    ///
    /// \param _n       The initial number of nodes
    /// \param _CT      The context to which this graph object is attached
    /// \param _mode    If true, the graph is not displayed before destruction
    sparseGraph(TNode _n=0,goblinController& _CT=goblinDefaultContext,
        bool _mode=false) throw();

    /// \brief  File constructor for sparse undirected graphs
    ///
    /// \param fileName  The source file name
    /// \param _CT       The context to which this graph object is attached
    sparseGraph(const char* fileName,goblinController& _CT=goblinDefaultContext)
        throw(ERFile,ERParse);

    /// \brief  Copy constructor for sparse undirected graphs
    ///
    /// \param G        The original mixed graph object
    /// \param options  A combination of OPT_CLONE, OPT_PARALLELS, OPT_MAPPINGS and OPT_SUB
    sparseGraph(abstractMixedGraph& G,TOption options=0) throw();

    ~sparseGraph() throw();

    unsigned long   Size() const throw();
    unsigned long   Allocated() const throw();

    #include <sparseInclude.h>

};


/// \addtogroup groupGraphDerivation
/// @{

/// \brief Complementary graph
///
/// A graph in which nodes are adjacent if they are not adjacent in the original
/// graph.

class complementaryGraph : public sparseGraph
{
public:

    complementaryGraph(abstractMixedGraph& G,TOption = 0) throw(ERRejected);

};

/// @}


/// \addtogroup groupPlanarComposition
/// @{

/// \brief  Planar line graphs of 3-dimensional convex polyhedra
///
/// A planar line graph has the original graph edges as its node set. Nodes are
/// adjacent if the original edges have an end node and a face in common.

class planarLineGraph : public sparseGraph
{
public:

    planarLineGraph(abstractMixedGraph& G,TOption = 0) throw(ERRejected);

};


/// \brief  Vertex truncations of 3-dimensional convex polyhedra
///
/// In this graph, every node of the original graph is replaced by face.
/// For every original edge, the new end nodes appear in the face
/// associated with the former end nodes. Arcs are added to enclose the
/// the new faces.

class vertexTruncation : public sparseGraph
{
public:

    vertexTruncation(abstractMixedGraph& _G,TOption = 0) throw(ERRejected);

};


/// \brief  Facet separations of 3-dimensional convex polyhedra
///
/// In this graph, every edge of the original graph is split into a pair of
/// edges. The original faces are maintained, but separated from each other.
/// Depending on the #TOptRotation constructor parameter, additional faces
/// are added.

class facetSeparation : public sparseGraph
{
public:

    /// \brief  Rule for adding the separating faces
    enum TOptRotation {
        ROT_NONE  = 0, ///< Adjacent faces are separated by rectangles
        ROT_LEFT  = 1, ///< Adjacent faces are separated by triangles according to some left-hand rule
        ROT_RIGHT = 2  ///< Adjacent faces are separated by triangles according to some right-hand rule
    };

    facetSeparation(abstractMixedGraph& G,TOptRotation mode = ROT_NONE) throw(ERRejected);

};


/// \brief  Undirected duals of planar graphs
///
/// The nodes of the dual graph are the faces in the original graph. Every edge
/// of the original graph is associated with an edge in the dual graph where it
/// connects the nodes mapped from the faces which where originally separated by
/// this arc.

class dualGraph : public sparseGraph
{
public:

    dualGraph(abstractMixedGraph& G,TOption = 0) throw(ERRejected);

};


/// \brief Unroll the faces of regular convex polyhedra
///
/// This constructor requires a spanning tree of the original graph. All
/// edges other than the tree edges are are duplicated. The original faces
/// are maintained, but an exterior face is introduced. This is enclosed by
/// all duplicated edges.
///
/// The original faces are drawn by regular polygones. It my happen that the
/// faces overlap. This depends on the choice of the spanning tree.

class spreadOutRegular : public sparseGraph
{
public:

    spreadOutRegular(abstractMixedGraph& G,TOption = 0) throw(ERRejected);

};

/// @}


/// \addtogroup groupGraphSeries
/// @{

/// \brief  Derived graph with increased chromatic number, but with the same clique number
///
/// The derived graph has the original graph and a star graph as induced subgraphs,
/// and both subgraphs are vertex disjoint. Every original node is matched by a
/// leave in the star.
///
/// By iterated application of this construction, the duality gap between the
/// chromatic number and the clique number can be made arbitrarily large.
///
/// Draw the star nodes as if the original nodes were all on a sphere: The
/// star center node is in the barycenter of the original nodes. The star leave
/// nodes are half-way between the center and the corresponding original node.

class mycielskianGraph : public sparseGraph
{
public:

    /// \brief Apply the Mycielskian construction to a given graph
    ///
    /// \param G        The original graph
    /// \param options  Any combination of OPT_SUB and OPT_MAPPINGS
    mycielskianGraph(abstractMixedGraph& G,TOption options = 0) throw(ERRejected);

    /// \brief Recursive application of the Mycielskian construction
    ///
    /// \param k    The recursion depth. k=2 denotes the K_2 induction base
    /// \param _CT  The controller to handle this object
    mycielskianGraph(unsigned k,goblinController& _CT = goblinDefaultContext) throw(ERRejected);

};

/// @}


/// \addtogroup groupDistanceGraphs
/// @{

/// \brief  Discrete Voronoi diagrams of graphs with non-negative edge lengths
///
/// A Voronoi diagram (in the graph-theoretic sense) is some kind of surface graph.
/// The constructor takes a set of terminal nodes, a partition of the node set
/// (implicitly assigning every node to its minimum distance terminal) and the
/// distance labels. It identifies the nodes in every part (applying the same rules
/// as the #colourContraction constructor) and assigns length labels according to
/// the shortest path lengths between two terminal nodes.

class voronoiDiagram : public sparseGraph
{
private:

    abstractMixedGraph&     G;
    const indexSet<TNode>&  Terminals;
    TArc*                   revMap;

public:

    voronoiDiagram(abstractMixedGraph& _G,const indexSet<TNode>& _Terminals) throw();
    ~voronoiDiagram() throw();

    TFloat UpdateSubgraph() throw();

};

/// @}


/// \addtogroup groupGraphSeries
/// @{

/// \brief  Line graphs of complete graphs with a given node set cardinality

class triangularGraph : public sparseGraph
{
public:

    triangularGraph(TNode cardinality,
        goblinController& _CT = goblinDefaultContext) throw();

};


/// \brief  k-subset intersection graphs with cardinality restrictions on the intersection of adjacent subsets

class intersectionGraph : public sparseGraph
{
public:

    /// \brief  k-subset intersection graphs with cardinality restrictions on the intersection of adjacent subsets
    ///
    /// \param groundSetCardinality  The ground set cardinality
    /// \param subsetCardinality     The subset cardinality
    /// \param minimumIntersection   A lower bound on the intersection cardinality of adjacent subsets
    /// \param maximumIntersection   An upper bound on the intersection cardinality of adjacent subsets
    /// \param _CT                   The controller object to manage the created graph
    ///
    /// The node set of this graph consists from all subsets of a certain ground
    /// set with a given subsetCardinality. Of the ground set, only the
    /// groundSetCardinality is specified. Two graph nodes are adjacent if the intersection
    /// cardinality of the corresponding subsets is in the range
    /// [minimumIntersection,maximumIntersection].
    ///
    /// The drawing shows the nodes on concentric circles with node indices
    /// increasing from the center point to the exterior circle. The nodes
    /// on the k-th circle represent the subset which can be built from the
    /// first <code>subsetCardinality + k</code> ground set elements - actually
    /// containing the k-th element.
    ///
    /// This construction covers several known graph classes:
    /// - Complete graphs, taking <code>minimumIntersection = maximumIntersection = 0</code>
    ///   and <code>subsetCardinality = 1</code>. But prefer \ref denseGraph for its efficent
    ///   data structures.
    /// - Triangular graphs, taking <code>minimumIntersection = maximumIntersection = 1</code>
    ///   and <code>subsetCardinality = 2</code>. But prefer \ref triangularGraph to obtain
    ///   symmetric drawings.
    /// - Kneser graphs, taking <code>minimumIntersection = maximumIntersection = 0</code>
    /// - Generalized Kneser graphs, taking <code>minimumIntersection = 0</code>
    /// - Johnson graphs, taking <code>minimumIntersection = maximumIntersection = subsetCardinality-1</code>
    intersectionGraph(TNode groundSetCardinality,TNode subsetCardinality,
        TNode minimumIntersection,TNode maximumIntersection,
        goblinController& _CT = goblinDefaultContext) throw();

};


/// \brief  Sierpinski triangles with a given depth

class sierpinskiTriangle : public sparseGraph
{
public:

    sierpinskiTriangle(TNode depth,
        goblinController& _CT = goblinDefaultContext) throw();
};


/// \brief Place a regular grid in the plane

class openGrid : public sparseGraph
{
public:

    /// \brief  Open grid type
    enum TOptGrid {
        GRID_TRIANGULAR = 0, ///< Grid is formed of triangular faces
        GRID_SQUARE     = 1, ///< Grid is formed of square faces
        GRID_HEXAGONAL  = 2  ///< Grid is formed of hexagonal faces
    };

    /// \param _k     Number of columns. Must be >= 2 and odd in the hexagonal case
    /// \param _l     Number of rows
    /// \param shape  Type of interior faces
    /// \param _CT    The controller object to manage the created graph
    openGrid(TNode _k,TNode _l,TOptGrid shape,
        goblinController& _CT = goblinDefaultContext) throw(ERRejected);

};


/// \brief Place a regular grid on a sphere

class polarGrid : public sparseGraph
{
public:

    /// \brief  Polar grid type
    enum TOptPolar {
        POLAR_DEFAULT    = 0, ///< Use default options
        POLAR_TRIANGULAR = 1, ///< Grid is formed of triangles
        POLAR_SQUARE     = 2, ///< Grid is formed of squares
        POLAR_CONE       = 3, ///< Grid is embedded on a cone
        POLAR_HEMISPHERE = 4, ///< Grid is embedded on a hemisphere
        POLAR_SPHERE     = 5, ///< Grid is embedded on a sphere (default rule)
        POLAR_TUBE       = 6, ///< Grid is embedded on a cylinder
        POLAR_DISC       = 7  ///< Grid is embedded on a disc (2D instead of 3D)
    };

    polarGrid(TNode _k,TNode _l,TNode _p,TOptPolar facets,TOptPolar dim,
        goblinController& _CT = goblinDefaultContext) throw(ERRejected);

};


/// \brief Place a regular grid on the surface of a torus

class toroidalGrid : public sparseGraph
{
public:

    /// \brief  Toroidal grid type
    enum TOptTorus {
        TORUS_TRIANGULAR    = 1, ///< Grid is formed of triangular regions
        TORUS_QUADRILATERAL = 2, ///< Grid is formed of quadrilateral regions
        TORUS_HEXAGONAL     = 3  ///< Grid is formed of hexagonal regions
    };


    /// \brief  Place a regular grid on the surface of a torus
    ///
    /// \param hSkew   The slope of the first orbit
    /// \param vSize   A grid height such that hSkew <= vSize
    /// \param vSkew   The slope inverse of the second orbit
    /// \param hSize   A grid width
    /// \param facets  The type of facets
    /// \param _CT     The controller object to manage the created graph
    ///
    /// This generates a node- and face-regular graph, and a quadrilateral torus
    /// map embedding. The horizontal [vertical] boundary is divided into hSize [vSize]
    /// portions. Two non-parallel straight lines (called the \em orbits) are placed,
    /// spanning a quadrilateral grid. Both orbits start at the upper left vertex,
    /// called the \em origin. The \em horizontal orbit meets the right-hand boundary at the
    /// vSize/hSkew fraction. Provided that vSkew > 0, the \em vertical orbit meets
    /// the bottom boundary at the hSize/vSkew fraction.
    ///
    /// It is legal that <code>vSkew < 0</code>, in which case the origin might
    /// be considered upper right vertex. It is also legal that hSkew and / or
    /// vSkew are zero, telling that the horizontal [vertical] orbit is indeed a
    /// horizontal [vertical] line. It is even legal to let <code>vSkew > hSize</code>,
    /// but only as long as <code>vSize*hSize - vSkew*hSkew</code> is a positive number.
    /// And this turns out to be the number of graph nodes in the triangular and the
    /// quadrilateral case. This implies that both orbits are non-parallel.
    ///
    /// If hSkew and vSize [vSkew and hSize] are not relatively prime - especially
    /// if hSkew [vSkew] is zero - <code>gcd(hSkew,vSize)</code> [<code>gcd(vSkew,hSize)</code>]
    /// parallels of the horizontal [vertical] orbit are added to complete the grid.
    ///
    /// The graph nodes are just the cut points of the constructed straight lines,
    /// enumerated by the first orbit (and its parallels). The graph edges are the
    /// minimal orbit segments.
    ///
    /// At this point, the construction in the \ref TORUS_QUADRILATERAL case is
    /// complete. All nodes and all regions have degree 4, and the graphs are
    /// self-dual. If hSkew and hSize [vSkew and vSize] have the same residue
    /// modulo 2, the resulting graph is bipartite.
    ///
    /// If \ref TORUS_TRIANGULAR is specified, a third non-parallel orbit is added,
    /// splitting each of the regions so far into two triangles. All nodes now
    /// have degree 6.
    ///
    /// If \ref TORUS_HEXAGONAL is specified, there are some more restrictions on the
    /// numeric parameters: hSkew and hSize [vSkew and vSize] must have the same
    /// residue modulo 3. If not, hSkew and / or vSkew rounded up to meet this
    /// requirement. First, the triagonal grid is generated. Then, from the first
    /// generating line, every third node is eliminated. After that operation,
    /// all nodes have degree 3, and all regions are hexagons.
    ///
    /// The constructor yields regular tessilations for several series of non-planar
    /// graphs (e.g. Moebius lattices, taking
    /// <code>hSkew = 1,vSize = 2,vSkew = -1,facets = TORUS_QUADRILATERAL</code>),
    /// but also for specific graph instances:
    /// - The tetrahedron (The complete graph on 4 nodes):
    ///  <code>hSkew = 0,vSize = 2,vSkew = -1,hSize = 3,facets = TORUS_HEXAGONAL</code>
    /// - K_5 (The complete graph on 4 nodes):
    ///  <code>hSkew = 1,vSize = 2,vSkew = -1,hSize = 2,facets = TORUS_QUADRILATERAL</code>
    /// - K_7 (The complete graph on 4 nodes):
    ///  <code>hSkew = 1,vSize = 3,vSkew = -1,hSize = 2,facets = TORUS_TRIANGULAR</code>
    /// - K_3_3 (The complete bigraph with 3 nodes on each side):
    ///  <code>hSkew = 0,vSize = 3,vSkew = 0,hSize = 3,facets = TORUS_HEXAGONAL</code>
    /// - K_4_4 (The complete bigraph with 4 nodes on each side):
    ///  <code>hSkew = 1,vSize = 3,vSkew = 1,hSize = 3,facets = TORUS_QUADRILATERAL</code>
    /// - Heawood graph (The dual graph of K_7):
    ///  <code>hSkew = 1,vSize = 5,vSkew = -1,hSize = 4,facets = TORUS_HEXAGONAL</code>
    /// - Moebius-Cantor graph (G(8,3) generalized Petersen graph):
    ///  <code>hSkew = 4,vSize = 5,vSkew = -1,hSize = 4,facets = TORUS_HEXAGONAL</code>
    /// - Moebius-Cantor graph dual:
    ///  <code>hSkew = 0,vSize = 2,vSkew = 1,hSize = 4,facets = TORUS_TRIANGULAR</code>
    /// - Shrikhande graph:
    ///  <code>hSkew = 0,vSize = 4,vSkew = 0,hSize = 4,facets = TORUS_TRIANGULAR</code>
    ///
    /// On the other hand, there are no representations of K_6 (nodes have degree 5)
    /// and the Petersen graph (would be hexagonal but non-bipartite).
    ///
    /// Triangular and hexagonal torus grids are dual to each other. In terms of its
    /// construction, the dual graph of an hexagonal torus grid has consists of the
    /// nodes which have been ommitted from the original triangular grid.
    toroidalGrid(unsigned short hSkew,unsigned short vSize,short vSkew,unsigned short hSize,
        TOptTorus facets,goblinController& _CT = goblinDefaultContext) throw(ERRejected);

};


/// \brief  Moebius ladder, embedded on a projective plane

class moebiusLadder : public sparseGraph
{
public:

    moebiusLadder(TNode _k,goblinController& _CT = goblinDefaultContext) throw(ERRejected);

};


/// \brief  A family 3-node regular, usually non-planar graphs with high girth

class generalizedPetersen : public sparseGraph
{
public:

    /// \brief  Generate a Petersen like graph
    ///
    /// \param perimeter  The length of the exterior cycle
    /// \param skew       Some value in the interval (0, .., perimeter)
    /// \param _CT        The controller object to manage the created graph
    ///
    /// This generates a graph which consists of two equal cardinality node sets.
    /// The subgraph induced by the set <code>0, 1, .., perimeter-1</code> of
    /// \em exterior nodes forms a cycle. Exterior and interior nodes are joined
    /// by the edges <code>(i, perimeter+i)</code> with i in the interval
    /// <code>[0, .., perimeter)</code>. Interior nodes are joined by the edges
    /// <code>(perimeter+i, perimeter+(i+skew)%perimeter)</code>.
    ///
    /// In the regular setting, perimeter and skew are relatively prime,
    /// and the subgraph induced by the second node set
    /// <code>perimeter, perimeter+1, .., 2*perimeter-1</code> also forms a cycle,
    /// but the node order is determined by the skew parameter.
    /// In any case, the generated graph is 3-node regular.
    ///
    /// Up to the implicit arc orientations of the interior cycle, <code>skew = k</code>
    /// and <code>skew = perimeter-k</code> give the same graph.
    /// For <code>skew == 1</code>, the result is a prism.
    /// For <code>skew == 2</code> and even perimeter, the result is planar.
    ///
    /// The original Petersen graph is given by <code>generalizedPetersen(5,2)</code>.
    /// Some other graphs in this family have received special attention:
    /// - <code>generalizedPetersen(6,2)</code> is known as the Duerer graph,
    /// - <code>generalizedPetersen(8,3)</code> is known as the Moebius-Cantor graph,
    /// - <code>generalizedPetersen(10,2)</code> is the dodecahedron,
    /// - <code>generalizedPetersen(10,3)</code> is known as the Desargues graph.
    generalizedPetersen(TNode perimeter,TNode skew,goblinController& _CT = goblinDefaultContext) throw(ERRejected);

};


/// \brief Place a regular grid in the plane and connect all nodes on common grid lines

class gridCompletion : public sparseGraph
{
public:

    /// \brief  Complete grid type
    enum TOptShape {
        SHAPE_TRIANGULAR = 0, ///< Grid is formed of triangular faces, overall triangular shape
        SHAPE_SQUARE     = 1, ///< Grid is formed of square faces, overall square shape
        SHAPE_HEXAGONAL  = 2  ///< Grid is formed of triangular faces, overall hexagonal shape
    };

    /// \brief  Generate a grid completion from scratch
    ///
    /// \param dim    The number of nodes on an exterior grid line
    /// \param shape  The shape of the convex hull of nodes
    /// \param _CT    The controller object to manage the created graph
    gridCompletion(TNode dim,TOptShape shape,
        goblinController& _CT = goblinDefaultContext) throw(ERRejected);

};


/// \brief  Threshold graph

class thresholdGraph : public sparseGraph
{
public:

    /// \brief  Generate a threshold graph
    ///
    /// \param numNodes    The number of nodes
    /// \param threshold   A threshold value for the arc weights
    /// \param nodeWeight  An (optional) array of node weights
    /// \param _CT         The controller object to manage the created graph
    ///
    /// This generates a graph with the specified number of nodes and arcs (u,v)
    /// if nodeWeight[u]+nodeWeight[v]>=threshold.
    thresholdGraph(TNode numNodes,TFloat threshold,TFloat* nodeWeight,
        goblinController& _CT = goblinDefaultContext) throw();

    /// \brief  Generate a random threshold graph
    ///
    /// \param numNodes   The number of nodes
    /// \param threshold  A threshold value for the arc weights
    /// \param randMin    A lower bound on the node weights
    /// \param randMax    An upper bound on the node weights
    /// \param _CT        The controller object to manage the created graph
    ///
    /// This generates a graph with the specified number of nodes and arcs (u,v)
    /// if weight[u]+weight[v]>=threshold. In replace of an explicit weight[],
    /// integer random node weights of the interval [randMin,..,randMax] are assumed.
    thresholdGraph(TNode numNodes,TFloat threshold,long randMin,long randMax,
        goblinController& _CT = goblinDefaultContext) throw();

private:

    /// \brief  Generate a threshold graph
    ///
    /// \param threshold   A threshold value for the arc weights
    /// \param nodeWeight  An (optional) array of node weights
    /// \param randMin     An alternative lower bound on the node weights
    /// \param randMax     An alternative upper bound on the node weights
    ///
    /// This generates arcs (u,v) if nodeWeight[u]+nodeWeight[v]>=threshold.
    /// If nodeWeight[] is not passed, integer random node weights of the
    /// interval [randMin,..,randMax] are assumed.
    void GenerateThis(TFloat threshold,TFloat* nodeWeight,long randMin,long randMax) throw();

};


/// \brief  Permutation graph

class permutationGraph : public sparseGraph
{
public:

    /// \brief  Generate a threshold graph
    ///
    /// \param numNodes   The number of nodes
    /// \param map        A bijective mapping of the node indices
    /// \param _CT        The controller object to manage the created graph
    ///
    /// This generates a graph with the specified number of nodes and arcs (u,v)
    /// if (map[u]-map[v])*(u-v)>0. If map[] is not specified, a random permutation
    /// is generated.
    permutationGraph(TNode numNodes,TNode* map = NULL,goblinController& _CT = goblinDefaultContext) throw();

};


/// \brief  Interval graph

class intervalGraph : public sparseGraph
{
public:

    /// \brief  Generate an interval graph
    ///
    /// \param numNodes   The number of nodes
    /// \param minRange   An array of interval minima
    /// \param maxRange   An array of interval maxima
    /// \param _CT        The controller object to manage the created graph
    ///
    /// This generates a graph with the specified number of nodes and arcs (u,v)
    /// if the intervals [minRange[u],..,maxRange[u]] and [minRange[v],..,maxRange[v]]
    /// are not disjoint - including the case maxRange[u]==minRange[v].
    intervalGraph(TNode numNodes,TFloat* minRange,TFloat* maxRange,
        goblinController& _CT = goblinDefaultContext) throw();

    /// \brief  Generate a random interval graph
    ///
    /// \param numNodes    The number of nodes
    /// \param valueRange  The value range of node intervals
    /// \param _CT         The controller object to manage the created graph
    ///
    /// This generates a graph with the specified number of nodes and arcs (u,v)
    /// if the intervals [minRange[u],..,maxRange[u]] and [minRange[v],..,maxRange[v]]
    /// are not disjoint - including the case maxRange[u]==minRange[v]. The intervals
    /// [minRange[v],..,maxRange[v]] are random subset intervals of [0,..,valueRange-1]
    intervalGraph(TNode numNodes,TIndex valueRange,
        goblinController& _CT = goblinDefaultContext) throw();

private:

    /// \brief  Generate an interval graph
    ///
    /// \param minRange    An (optional) array of interval minima
    /// \param maxRange    An (optional) array of interval maxima
    /// \param valueRange  An alternative range value
    ///
    /// This generates a graph with the specified number of nodes and arcs (u,v)
    /// if the intervals [minRange[u],..,maxRange[u]] and [minRange[v],..,maxRange[v]]
    /// are not disjoint - including the case maxRange[u]==minRange[v]. If no
    /// intervals are specified, random intervals are generated.
    void GenerateThis(TFloat* minRange,TFloat* maxRange,TIndex valueRange) throw();

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

/// \file   sparseGraph.h
/// \brief  Class interface to sparse undirected graph objects

#if !defined(_SPARSE_GRAPH_H_)
#define _SPARSE_GRAPH_H_

#include "abstractGraph.h"
#include "sparseRepresentation.h"


/// \brief A class for undirected graphs represented with incidence lists

class sparseGraph : public abstractGraph
{
    friend class sparseRepresentation;

private:

    bool mode;

protected:

    sparseRepresentation X;

public:

    /// \brief  Default constructor for sparse undirected graphs
    ///
    /// \param _n       The initial number of nodes
    /// \param _CT      The context to which this graph object is attached
    /// \param _mode    If true, the graph is not displayed before destruction
    sparseGraph(TNode _n=0,goblinController& _CT=goblinDefaultContext,
        bool _mode=false) throw();

    /// \brief  File constructor for sparse undirected graphs
    ///
    /// \param fileName  The source file name
    /// \param _CT       The context to which this graph object is attached
    sparseGraph(const char* fileName,goblinController& _CT=goblinDefaultContext)
        throw(ERFile,ERParse);

    /// \brief  Copy constructor for sparse undirected graphs
    ///
    /// \param G        The original mixed graph object
    /// \param options  A combination of OPT_CLONE, OPT_PARALLELS, OPT_MAPPINGS and OPT_SUB
    sparseGraph(abstractMixedGraph& G,TOption options=0) throw();

    ~sparseGraph() throw();

    unsigned long   Size() const throw();
    unsigned long   Allocated() const throw();

    #include <sparseInclude.h>

};


/// \addtogroup groupGraphDerivation
/// @{

/// \brief Complementary graph
///
/// A graph in which nodes are adjacent if they are not adjacent in the original
/// graph.

class complementaryGraph : public sparseGraph
{
public:

    complementaryGraph(abstractMixedGraph& G,TOption = 0) throw(ERRejected);

};

/// @}


/// \addtogroup groupPlanarComposition
/// @{

/// \brief  Planar line graphs of 3-dimensional convex polyhedra
///
/// A planar line graph has the original graph edges as its node set. Nodes are
/// adjacent if the original edges have an end node and a face in common.

class planarLineGraph : public sparseGraph
{
public:

    planarLineGraph(abstractMixedGraph& G,TOption = 0) throw(ERRejected);

};


/// \brief  Vertex truncations of 3-dimensional convex polyhedra
///
/// In this graph, every node of the original graph is replaced by face.
/// For every original edge, the new end nodes appear in the face
/// associated with the former end nodes. Arcs are added to enclose the
/// the new faces.

class vertexTruncation : public sparseGraph
{
public:

    vertexTruncation(abstractMixedGraph& _G,TOption = 0) throw(ERRejected);

};


/// \brief  Facet separations of 3-dimensional convex polyhedra
///
/// In this graph, every edge of the original graph is split into a pair of
/// edges. The original faces are maintained, but separated from each other.
/// Depending on the #TOptRotation constructor parameter, additional faces
/// are added.

class facetSeparation : public sparseGraph
{
public:

    /// \brief  Rule for adding the separating faces
    enum TOptRotation {
        ROT_NONE  = 0, ///< Adjacent faces are separated by rectangles
        ROT_LEFT  = 1, ///< Adjacent faces are separated by triangles according to some left-hand rule
        ROT_RIGHT = 2  ///< Adjacent faces are separated by triangles according to some right-hand rule
    };

    facetSeparation(abstractMixedGraph& G,TOptRotation mode = ROT_NONE) throw(ERRejected);

};


/// \brief  Undirected duals of planar graphs
///
/// The nodes of the dual graph are the faces in the original graph. Every edge
/// of the original graph is associated with an edge in the dual graph where it
/// connects the nodes mapped from the faces which where originally separated by
/// this arc.

class dualGraph : public sparseGraph
{
public:

    dualGraph(abstractMixedGraph& G,TOption = 0) throw(ERRejected);

};


/// \brief Unroll the faces of regular convex polyhedra
///
/// This constructor requires a spanning tree of the original graph. All
/// edges other than the tree edges are are duplicated. The original faces
/// are maintained, but an exterior face is introduced. This is enclosed by
/// all duplicated edges.
///
/// The original faces are drawn by regular polygones. It my happen that the
/// faces overlap. This depends on the choice of the spanning tree.

class spreadOutRegular : public sparseGraph
{
public:

    spreadOutRegular(abstractMixedGraph& G,TOption = 0) throw(ERRejected);

};

/// @}


/// \addtogroup groupGraphSeries
/// @{

/// \brief  Derived graph with increased chromatic number, but with the same clique number
///
/// The derived graph has the original graph and a star graph as induced subgraphs,
/// and both subgraphs are vertex disjoint. Every original node is matched by a
/// leave in the star.
///
/// By iterated application of this construction, the duality gap between the
/// chromatic number and the clique number can be made arbitrarily large.
///
/// Draw the star nodes as if the original nodes were all on a sphere: The
/// star center node is in the barycenter of the original nodes. The star leave
/// nodes are half-way between the center and the corresponding original node.

class mycielskianGraph : public sparseGraph
{
public:

    /// \brief Apply the Mycielskian construction to a given graph
    ///
    /// \param G        The original graph
    /// \param options  Any combination of OPT_SUB and OPT_MAPPINGS
    mycielskianGraph(abstractMixedGraph& G,TOption options = 0) throw(ERRejected);

    /// \brief Recursive application of the Mycielskian construction
    ///
    /// \param k    The recursion depth. k=2 denotes the K_2 induction base
    /// \param _CT  The controller to handle this object
    mycielskianGraph(unsigned k,goblinController& _CT = goblinDefaultContext) throw(ERRejected);

};

/// @}


/// \addtogroup groupDistanceGraphs
/// @{

/// \brief  Discrete Voronoi diagrams of graphs with non-negative edge lengths
///
/// A Voronoi diagram (in the graph-theoretic sense) is some kind of surface graph.
/// The constructor takes a set of terminal nodes, a partition of the node set
/// (implicitly assigning every node to its minimum distance terminal) and the
/// distance labels. It identifies the nodes in every part (applying the same rules
/// as the #colourContraction constructor) and assigns length labels according to
/// the shortest path lengths between two terminal nodes.

class voronoiDiagram : public sparseGraph
{
private:

    abstractMixedGraph&     G;
    const indexSet<TNode>&  Terminals;
    TArc*                   revMap;

public:

    voronoiDiagram(abstractMixedGraph& _G,const indexSet<TNode>& _Terminals) throw();
    ~voronoiDiagram() throw();

    TFloat UpdateSubgraph() throw();

};

/// @}


/// \addtogroup groupGraphSeries
/// @{

/// \brief  Line graphs of complete graphs with a given node set cardinality

class triangularGraph : public sparseGraph
{
public:

    triangularGraph(TNode cardinality,
        goblinController& _CT = goblinDefaultContext) throw();

};


/// \brief  k-subset intersection graphs with cardinality restrictions on the intersection of adjacent subsets

class intersectionGraph : public sparseGraph
{
public:

    /// \brief  k-subset intersection graphs with cardinality restrictions on the intersection of adjacent subsets
    ///
    /// \param groundSetCardinality  The ground set cardinality
    /// \param subsetCardinality     The subset cardinality
    /// \param minimumIntersection   A lower bound on the intersection cardinality of adjacent subsets
    /// \param maximumIntersection   An upper bound on the intersection cardinality of adjacent subsets
    /// \param _CT                   The controller object to manage the created graph
    ///
    /// The node set of this graph consists from all subsets of a certain ground
    /// set with a given subsetCardinality. Of the ground set, only the
    /// groundSetCardinality is specified. Two graph nodes are adjacent if the intersection
    /// cardinality of the corresponding subsets is in the range
    /// [minimumIntersection,maximumIntersection].
    ///
    /// The drawing shows the nodes on concentric circles with node indices
    /// increasing from the center point to the exterior circle. The nodes
    /// on the k-th circle represent the subset which can be built from the
    /// first <code>subsetCardinality + k</code> ground set elements - actually
    /// containing the k-th element.
    ///
    /// This construction covers several known graph classes:
    /// - Complete graphs, taking <code>minimumIntersection = maximumIntersection = 0</code>
    ///   and <code>subsetCardinality = 1</code>. But prefer \ref denseGraph for its efficent
    ///   data structures.
    /// - Triangular graphs, taking <code>minimumIntersection = maximumIntersection = 1</code>
    ///   and <code>subsetCardinality = 2</code>. But prefer \ref triangularGraph to obtain
    ///   symmetric drawings.
    /// - Kneser graphs, taking <code>minimumIntersection = maximumIntersection = 0</code>
    /// - Generalized Kneser graphs, taking <code>minimumIntersection = 0</code>
    /// - Johnson graphs, taking <code>minimumIntersection = maximumIntersection = subsetCardinality-1</code>
    intersectionGraph(TNode groundSetCardinality,TNode subsetCardinality,
        TNode minimumIntersection,TNode maximumIntersection,
        goblinController& _CT = goblinDefaultContext) throw();

};


/// \brief  Sierpinski triangles with a given depth

class sierpinskiTriangle : public sparseGraph
{
public:

    sierpinskiTriangle(TNode depth,
        goblinController& _CT = goblinDefaultContext) throw();
};


/// \brief Place a regular grid in the plane

class openGrid : public sparseGraph
{
public:

    /// \brief  Open grid type
    enum TOptGrid {
        GRID_TRIANGULAR = 0, ///< Grid is formed of triangular faces
        GRID_SQUARE     = 1, ///< Grid is formed of square faces
        GRID_HEXAGONAL  = 2  ///< Grid is formed of hexagonal faces
    };

    /// \param _k     Number of columns. Must be >= 2 and odd in the hexagonal case
    /// \param _l     Number of rows
    /// \param shape  Type of interior faces
    /// \param _CT    The controller object to manage the created graph
    openGrid(TNode _k,TNode _l,TOptGrid shape,
        goblinController& _CT = goblinDefaultContext) throw(ERRejected);

};


/// \brief Place a regular grid on a sphere

class polarGrid : public sparseGraph
{
public:

    /// \brief  Polar grid type
    enum TOptPolar {
        POLAR_DEFAULT    = 0, ///< Use default options
        POLAR_TRIANGULAR = 1, ///< Grid is formed of triangles
        POLAR_SQUARE     = 2, ///< Grid is formed of squares
        POLAR_CONE       = 3, ///< Grid is embedded on a cone
        POLAR_HEMISPHERE = 4, ///< Grid is embedded on a hemisphere
        POLAR_SPHERE     = 5, ///< Grid is embedded on a sphere (default rule)
        POLAR_TUBE       = 6, ///< Grid is embedded on a cylinder
        POLAR_DISC       = 7  ///< Grid is embedded on a disc (2D instead of 3D)
    };

    polarGrid(TNode _k,TNode _l,TNode _p,TOptPolar facets,TOptPolar dim,
        goblinController& _CT = goblinDefaultContext) throw(ERRejected);

};


/// \brief Place a regular grid on the surface of a torus

class toroidalGrid : public sparseGraph
{
public:

    /// \brief  Toroidal grid type
    enum TOptTorus {
        TORUS_TRIANGULAR    = 1, ///< Grid is formed of triangular regions
        TORUS_QUADRILATERAL = 2, ///< Grid is formed of quadrilateral regions
        TORUS_HEXAGONAL     = 3  ///< Grid is formed of hexagonal regions
    };


    /// \brief  Place a regular grid on the surface of a torus
    ///
    /// \param hSkew   The slope of the first orbit
    /// \param vSize   A grid height such that hSkew <= vSize
    /// \param vSkew   The slope inverse of the second orbit
    /// \param hSize   A grid width
    /// \param facets  The type of facets
    /// \param _CT     The controller object to manage the created graph
    ///
    /// This generates a node- and face-regular graph, and a quadrilateral torus
    /// map embedding. The horizontal [vertical] boundary is divided into hSize [vSize]
    /// portions. Two non-parallel straight lines (called the \em orbits) are placed,
    /// spanning a quadrilateral grid. Both orbits start at the upper left vertex,
    /// called the \em origin. The \em horizontal orbit meets the right-hand boundary at the
    /// vSize/hSkew fraction. Provided that vSkew > 0, the \em vertical orbit meets
    /// the bottom boundary at the hSize/vSkew fraction.
    ///
    /// It is legal that <code>vSkew < 0</code>, in which case the origin might
    /// be considered upper right vertex. It is also legal that hSkew and / or
    /// vSkew are zero, telling that the horizontal [vertical] orbit is indeed a
    /// horizontal [vertical] line. It is even legal to let <code>vSkew > hSize</code>,
    /// but only as long as <code>vSize*hSize - vSkew*hSkew</code> is a positive number.
    /// And this turns out to be the number of graph nodes in the triangular and the
    /// quadrilateral case. This implies that both orbits are non-parallel.
    ///
    /// If hSkew and vSize [vSkew and hSize] are not relatively prime - especially
    /// if hSkew [vSkew] is zero - <code>gcd(hSkew,vSize)</code> [<code>gcd(vSkew,hSize)</code>]
    /// parallels of the horizontal [vertical] orbit are added to complete the grid.
    ///
    /// The graph nodes are just the cut points of the constructed straight lines,
    /// enumerated by the first orbit (and its parallels). The graph edges are the
    /// minimal orbit segments.
    ///
    /// At this point, the construction in the \ref TORUS_QUADRILATERAL case is
    /// complete. All nodes and all regions have degree 4, and the graphs are
    /// self-dual. If hSkew and hSize [vSkew and vSize] have the same residue
    /// modulo 2, the resulting graph is bipartite.
    ///
    /// If \ref TORUS_TRIANGULAR is specified, a third non-parallel orbit is added,
    /// splitting each of the regions so far into two triangles. All nodes now
    /// have degree 6.
    ///
    /// If \ref TORUS_HEXAGONAL is specified, there are some more restrictions on the
    /// numeric parameters: hSkew and hSize [vSkew and vSize] must have the same
    /// residue modulo 3. If not, hSkew and / or vSkew rounded up to meet this
    /// requirement. First, the triagonal grid is generated. Then, from the first
    /// generating line, every third node is eliminated. After that operation,
    /// all nodes have degree 3, and all regions are hexagons.
    ///
    /// The constructor yields regular tessilations for several series of non-planar
    /// graphs (e.g. Moebius lattices, taking
    /// <code>hSkew = 1,vSize = 2,vSkew = -1,facets = TORUS_QUADRILATERAL</code>),
    /// but also for specific graph instances:
    /// - The tetrahedron (The complete graph on 4 nodes):
    ///  <code>hSkew = 0,vSize = 2,vSkew = -1,hSize = 3,facets = TORUS_HEXAGONAL</code>
    /// - K_5 (The complete graph on 4 nodes):
    ///  <code>hSkew = 1,vSize = 2,vSkew = -1,hSize = 2,facets = TORUS_QUADRILATERAL</code>
    /// - K_7 (The complete graph on 4 nodes):
    ///  <code>hSkew = 1,vSize = 3,vSkew = -1,hSize = 2,facets = TORUS_TRIANGULAR</code>
    /// - K_3_3 (The complete bigraph with 3 nodes on each side):
    ///  <code>hSkew = 0,vSize = 3,vSkew = 0,hSize = 3,facets = TORUS_HEXAGONAL</code>
    /// - K_4_4 (The complete bigraph with 4 nodes on each side):
    ///  <code>hSkew = 1,vSize = 3,vSkew = 1,hSize = 3,facets = TORUS_QUADRILATERAL</code>
    /// - Heawood graph (The dual graph of K_7):
    ///  <code>hSkew = 1,vSize = 5,vSkew = -1,hSize = 4,facets = TORUS_HEXAGONAL</code>
    /// - Moebius-Cantor graph (G(8,3) generalized Petersen graph):
    ///  <code>hSkew = 4,vSize = 5,vSkew = -1,hSize = 4,facets = TORUS_HEXAGONAL</code>
    /// - Moebius-Cantor graph dual:
    ///  <code>hSkew = 0,vSize = 2,vSkew = 1,hSize = 4,facets = TORUS_TRIANGULAR</code>
    /// - Shrikhande graph:
    ///  <code>hSkew = 0,vSize = 4,vSkew = 0,hSize = 4,facets = TORUS_TRIANGULAR</code>
    ///
    /// On the other hand, there are no representations of K_6 (nodes have degree 5)
    /// and the Petersen graph (would be hexagonal but non-bipartite).
    ///
    /// Triangular and hexagonal torus grids are dual to each other. In terms of its
    /// construction, the dual graph of an hexagonal torus grid has consists of the
    /// nodes which have been ommitted from the original triangular grid.
    toroidalGrid(unsigned short hSkew,unsigned short vSize,short vSkew,unsigned short hSize,
        TOptTorus facets,goblinController& _CT = goblinDefaultContext) throw(ERRejected);

};


/// \brief  Moebius ladder, embedded on a projective plane

class moebiusLadder : public sparseGraph
{
public:

    moebiusLadder(TNode _k,goblinController& _CT = goblinDefaultContext) throw(ERRejected);

};


/// \brief  A family 3-node regular, usually non-planar graphs with high girth

class generalizedPetersen : public sparseGraph
{
public:

    /// \brief  Generate a Petersen like graph
    ///
    /// \param perimeter  The length of the exterior cycle
    /// \param skew       Some value in the interval (0, .., perimeter)
    /// \param _CT        The controller object to manage the created graph
    ///
    /// This generates a graph which consists of two equal cardinality node sets.
    /// The subgraph induced by the set <code>0, 1, .., perimeter-1</code> of
    /// \em exterior nodes forms a cycle. Exterior and interior nodes are joined
    /// by the edges <code>(i, perimeter+i)</code> with i in the interval
    /// <code>[0, .., perimeter)</code>. Interior nodes are joined by the edges
    /// <code>(perimeter+i, perimeter+(i+skew)%perimeter)</code>.
    ///
    /// In the regular setting, perimeter and skew are relatively prime,
    /// and the subgraph induced by the second node set
    /// <code>perimeter, perimeter+1, .., 2*perimeter-1</code> also forms a cycle,
    /// but the node order is determined by the skew parameter.
    /// In any case, the generated graph is 3-node regular.
    ///
    /// Up to the implicit arc orientations of the interior cycle, <code>skew = k</code>
    /// and <code>skew = perimeter-k</code> give the same graph.
    /// For <code>skew == 1</code>, the result is a prism.
    /// For <code>skew == 2</code> and even perimeter, the result is planar.
    ///
    /// The original Petersen graph is given by <code>generalizedPetersen(5,2)</code>.
    /// Some other graphs in this family have received special attention:
    /// - <code>generalizedPetersen(6,2)</code> is known as the Duerer graph,
    /// - <code>generalizedPetersen(8,3)</code> is known as the Moebius-Cantor graph,
    /// - <code>generalizedPetersen(10,2)</code> is the dodecahedron,
    /// - <code>generalizedPetersen(10,3)</code> is known as the Desargues graph.
    generalizedPetersen(TNode perimeter,TNode skew,goblinController& _CT = goblinDefaultContext) throw(ERRejected);

};


/// \brief Place a regular grid in the plane and connect all nodes on common grid lines

class gridCompletion : public sparseGraph
{
public:

    /// \brief  Complete grid type
    enum TOptShape {
        SHAPE_TRIANGULAR = 0, ///< Grid is formed of triangular faces, overall triangular shape
        SHAPE_SQUARE     = 1, ///< Grid is formed of square faces, overall square shape
        SHAPE_HEXAGONAL  = 2  ///< Grid is formed of triangular faces, overall hexagonal shape
    };

    /// \brief  Generate a grid completion from scratch
    ///
    /// \param dim    The number of nodes on an exterior grid line
    /// \param shape  The shape of the convex hull of nodes
    /// \param _CT    The controller object to manage the created graph
    gridCompletion(TNode dim,TOptShape shape,
        goblinController& _CT = goblinDefaultContext) throw(ERRejected);

};


/// \brief  Threshold graph

class thresholdGraph : public sparseGraph
{
public:

    /// \brief  Generate a threshold graph
    ///
    /// \param numNodes    The number of nodes
    /// \param threshold   A threshold value for the arc weights
    /// \param nodeWeight  An (optional) array of node weights
    /// \param _CT         The controller object to manage the created graph
    ///
    /// This generates a graph with the specified number of nodes and arcs (u,v)
    /// if nodeWeight[u]+nodeWeight[v]>=threshold.
    thresholdGraph(TNode numNodes,TFloat threshold,TFloat* nodeWeight,
        goblinController& _CT = goblinDefaultContext) throw();

    /// \brief  Generate a random threshold graph
    ///
    /// \param numNodes   The number of nodes
    /// \param threshold  A threshold value for the arc weights
    /// \param randMin    A lower bound on the node weights
    /// \param randMax    An upper bound on the node weights
    /// \param _CT        The controller object to manage the created graph
    ///
    /// This generates a graph with the specified number of nodes and arcs (u,v)
    /// if weight[u]+weight[v]>=threshold. In replace of an explicit weight[],
    /// integer random node weights of the interval [randMin,..,randMax] are assumed.
    thresholdGraph(TNode numNodes,TFloat threshold,long randMin,long randMax,
        goblinController& _CT = goblinDefaultContext) throw();

private:

    /// \brief  Generate a threshold graph
    ///
    /// \param threshold   A threshold value for the arc weights
    /// \param nodeWeight  An (optional) array of node weights
    /// \param randMin     An alternative lower bound on the node weights
    /// \param randMax     An alternative upper bound on the node weights
    ///
    /// This generates arcs (u,v) if nodeWeight[u]+nodeWeight[v]>=threshold.
    /// If nodeWeight[] is not passed, integer random node weights of the
    /// interval [randMin,..,randMax] are assumed.
    void GenerateThis(TFloat threshold,TFloat* nodeWeight,long randMin,long randMax) throw();

};


/// \brief  Permutation graph

class permutationGraph : public sparseGraph
{
public:

    /// \brief  Generate a threshold graph
    ///
    /// \param numNodes   The number of nodes
    /// \param map        A bijective mapping of the node indices
    /// \param _CT        The controller object to manage the created graph
    ///
    /// This generates a graph with the specified number of nodes and arcs (u,v)
    /// if (map[u]-map[v])*(u-v)>0. If map[] is not specified, a random permutation
    /// is generated.
    permutationGraph(TNode numNodes,TNode* map = NULL,goblinController& _CT = goblinDefaultContext) throw();

};


/// \brief  Interval graph

class intervalGraph : public sparseGraph
{
public:

    /// \brief  Generate an interval graph
    ///
    /// \param numNodes   The number of nodes
    /// \param minRange   An array of interval minima
    /// \param maxRange   An array of interval maxima
    /// \param _CT        The controller object to manage the created graph
    ///
    /// This generates a graph with the specified number of nodes and arcs (u,v)
    /// if the intervals [minRange[u],..,maxRange[u]] and [minRange[v],..,maxRange[v]]
    /// are not disjoint - including the case maxRange[u]==minRange[v].
    intervalGraph(TNode numNodes,TFloat* minRange,TFloat* maxRange,
        goblinController& _CT = goblinDefaultContext) throw();

    /// \brief  Generate a random interval graph
    ///
    /// \param numNodes    The number of nodes
    /// \param valueRange  The value range of node intervals
    /// \param _CT         The controller object to manage the created graph
    ///
    /// This generates a graph with the specified number of nodes and arcs (u,v)
    /// if the intervals [minRange[u],..,maxRange[u]] and [minRange[v],..,maxRange[v]]
    /// are not disjoint - including the case maxRange[u]==minRange[v]. The intervals
    /// [minRange[v],..,maxRange[v]] are random subset intervals of [0,..,valueRange-1]
    intervalGraph(TNode numNodes,TIndex valueRange,
        goblinController& _CT = goblinDefaultContext) throw();

private:

    /// \brief  Generate an interval graph
    ///
    /// \param minRange    An (optional) array of interval minima
    /// \param maxRange    An (optional) array of interval maxima
    /// \param valueRange  An alternative range value
    ///
    /// This generates a graph with the specified number of nodes and arcs (u,v)
    /// if the intervals [minRange[u],..,maxRange[u]] and [minRange[v],..,maxRange[v]]
    /// are not disjoint - including the case maxRange[u]==minRange[v]. If no
    /// intervals are specified, random intervals are generated.
    void GenerateThis(TFloat* minRange,TFloat* maxRange,TIndex valueRange) throw();

};

/// @}


#endif
