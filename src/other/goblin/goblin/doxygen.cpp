
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, February 2007
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   doxygen.cpp
/// \brief  Several doxygen commands used to generate this documentation


/// \mainpage  GOBLIN Graph library
///
/// <br>
///
/// \image html  k7.gif
/// \image latex k7.eps
///
/// <br>
///
/// <b><center> A torus map embedding of K_7, the complete graph on 7 nodes </center></b>

/// \page pageOverview  Project overview
///
/// Goblin is a full-featured tool chain for handling \ref pageGraphObjects "graphs".
/// The project provides code from the following areas:
/// - <b>Optimization</b>: Nearly all algorithms described in textbooks on graph optimization
///   are implemented.
/// - <b>Layout</b>: The most common models (\ref secOrthogonalDrawing "orthogonal",
///   \ref secLayeredDrawing "layered", \ref secForceDirected "force directed") are
///   supported.
/// - <b>Composition</b>: Especially for planar graphs, a lot of rules are implemented
///   to derive one graph from another.
/// - <b>File import and export</b>: Graphs can be read from Dimacs, Tsplib and Steinlib
///   formats. Layouts can be exported to nearly arbitrary bitmap and vectorial
///   formats by implicit use of the fig2dev and the netpbm packages.
/// - <b>Manipulation</b>: Graph incidence structures can be edited and attributes can be
///   assigned to nodes and edges in the graphical front end.
///
/// While all algorithms are implemented in a C++ class library (libgoblin.a),
/// the editor code and partially the file I/O code are written in the Tcl/Tk
/// scripting language and use a Tcl wrapper of the C++ library.
///
/// Users can choose from the following interfaces:
/// - A C++ programming interface (API) described here
/// - A Tcl/Tk programming interface which is less efficient, but sometimes more
///   convenient
/// - A graphical user interface (GUI) to manipulate graphs, but also to run
///   problem solvers and to visualize the (intermediate) results
/// - Solver executables for several optmization problems
///
/// <br>
///
///
/// \section intendedAudience  Intended audience
///
/// Any general software on graph optimization can be considered a loose collection
/// of solvers for more or less related combinatorial problems. Other than for LP
/// solvers, the interface is ample and often requires to choose a particular
/// optimization method which is adequate for a certain class of graph instances.
/// Many applications require a programming interface instead of a graphical
/// user interface. In that sense, Goblin is expert software.
///
/// Originally, the library has been designed to visualize graph algorithms. While
/// it takes special efforts to produce good running examples and snap shots, the
/// final output can be interpreted by every undergraduate student who has been
/// instructed with the algorithm's key features. The GUI has been prepared to
/// fiddle about with algorithms.
///
/// Of course, the library is also suited for practical computations. A general
/// statement on the performance is difficult: Some solvers have been added only
/// for sake of completeness (like max-cut), others can solve large-scale instances
/// (like min-cost flow).
///
/// <br>
///
///
/// \section licenceModel  Licence
///
/// GOBLIN is open source software and licenced by the GNU Lesser Public License
/// (LGPL). That is, GOBLIN may be downloaded, compiled and used for scientific,
/// educational and other purposes free of charge. For details, in particular
/// the statements about redistribution and changes of the source code, observe
/// the LGPL document which is attached to the package.
///
/// <br>
///
///
/// \section projectState  Project state
///
/// The project state permanently toggles between stable and beta: There are
/// frequent functional extensions, and these sometimes require revisions of the
/// internal data structures and even the C++ programming interface.
/// 
/// Here are some more detailed remarks on the development state:
/// - The optimization stuff is very comprehensive and, in general, stable. Future
///   releases will come up with alternative methods and performance improvements
///   rather than solvers for additional problems.
/// - The drawing codes are mostly stable, but several general features are missing
///   (3D support, infinite nodes and edges, free-style display features).
///   The orthogonal methods lack strong compaction rules.
/// - The internal graph representations still do not allow for node hierarchies and
///   arbitrary tags on nodes and edges.
/// - Tracing is supported for many but not for all methods. This functionality
///   is still difficult to control.
///
/// <br>
///
///
/// \section thisDocument  About this document
///
/// This document serves as a reference guide for the C++ core library and the Tcl
/// programming interface. For the time being, it describes the API formed by the
/// graph base classes, but skips a lot of internal features such as problem
/// reductions and the LP wrapper. So it is not as comprehensive as the existing
/// latex manual yet, but the latter won't be maintained regularly until this
/// doxygen reference has become stable.
///
/// If you are just reading this online (as a part of the project web presentation),
/// all statements concern the tip of development. Assuming that you will work with
/// a specific source code distribution, note that all such packages admit a
/// <code>make html</code> build rule to generate the matching reference manual.
///
/// Two levels of reading are possible: The introductory pages present mathematical
/// defintions, high-level descriptions of algorithms and some instructive examples.
/// In order to view the according doxygen code comments, follow the [See API] links.
///
/// <br>
///
///
/// \ref pageManualContents "[See manual page index]"


/// \page pageManualContents  Manual page index
///
/// - \ref pageOverview
/// - \ref pageInstallation
/// - Managed data objects
///     - \ref pageGraphObjects
///         - \ref pageGraphSkeleton
///         - \ref pageRegisters
///         - \ref pageSubgraphManagement
///         - \ref pageGraphManipulation
///         - \ref pagePlanarity
///     - \ref pageProxyAndComponents
///     - \ref pageContainers
///     - Branch and bound
/// - Algorithms
///     - Graph optimization
///         - \ref pageEdgeRouting
///             - \ref pageNetworkFlows
///         - \ref pageConnectivity
///         - \ref pageVertexRouting
///         - \ref pageGraphPacking
///     - \ref pageGraphLayout
///     - \ref pageGraphComposition
///     - \ref pageGraphRecognition
/// - \ref pageCodeInstrumentation
/// - Interfaces
///     - \ref groupObjectExport
///     - \ref groupObjectImport
///     - \ref groupCanvasBuilder
///     - \ref groupTextDisplay
///     - \ref groupTransscript
///     - \ref groupMipReductions
/// - \ref pageTclReference
///     - \ref secTclObjectInstanciation
///     - \ref secTclGraphComposition
///     - \ref secTclGraphRetrieval
///     - \ref secTclGraphManipulation
///     - \ref secTclGraphOptimization
///     - \ref secTclGraphLayout
/// - \ref pageTutorials
///     - \ref tutorialConstruction
///     - \ref tutorialSolvers


/// \page pageScreenshots  Screenshots
///
/// \section secScreenshotBrowser  Browsing / Optimization Session
///
/// \image html  browser.gif
///
/// <br>
///
///
/// \section secScreenshotEditor  Editing Session
///
/// \image html  editor.gif
///
/// <br>


/// \defgroup queryReprAttributes  Representational attributes
/// \defgroup nodeCoordinates      Node coordinates
/// \defgroup specialEntities      Special entities

/// \page pageGraphObjects  Graph objects
///
/// Graphs are objects from discrete geometry which consist of so-called nodes
/// and edges. Graphs may be either undirected (where edges are unordered pairs
/// of nodes), directed (where edges are ordered pairs), or mixed from directed
/// and undirected edges. To this mathematic definition, we refer as the
/// incidence structure or the \ref pageGraphSkeleton "graph skeleton".
///
/// Most graph optimization problems require further input, usually numeric values
/// associated with the graph edges. For the time being, the library does not
/// support arbitrary tags on the graph nodes and edges, but attributes which
/// either assign a value to \em all nodes or to \em all edges.
///
/// There are so-called attribute pools to manage attributes with predefined
/// roles, and to make them persistent:
/// - \ref secReprAttributes
/// - \ref pageRegisters
/// - \ref secNodeCoordinates
/// - Layout data
///
/// All graph objects provide consistent interfaces to retrieve values of the
/// attributes in these pools. The attribute values are either virtual (functional
/// dependent on another graph object) or represented in memory. Register
/// attributes are always represented, even for graph objects which result from
/// a combinatorial problem transformation.
///
/// A graph object is called represented if all of the managed attributes are
/// represented. If the graph skeleton is also represented by data structures,
/// the graph representation is called \em sparse (and \em dense otherwise).
///
/// <br>
///
///
/// \section secGraphSubclasses  Subclass hierarchy
///
/// The distinction between directed, undirected and mixed graphs is reflected
/// by the library class hierarchy. All high level methods are associated with
/// according abstract classes. From the abstract class, a sparse and a dense
/// representation subclass is derived.
///
/// <br>
///
///
/// \subsection secMixedGraphs  Mixed graphs
///
/// The class \ref abstractMixedGraph is the base class for all kinds of graph
/// objects. It implements all layout and optimization methods which do not
/// impose restrictions on the graph skeleton, which can ignore edge orientations
/// or which can handle both the directed and the undirected case.
///
/// \ref abstractMixedGraph "[See abstract base class]"
///
/// \ref mixedGraph "[See sparse implementation]"
///
/// <br>
///
///
/// \subsection secDigraphs  Digraphs (directed graphs)
///
/// The class \ref abstractDiGraph is the base class for all digraph objects.
/// It implements most of the network flow code and everything about
/// \ref secDirectedAcyclic "DAGs".
///
/// \ref abstractDiGraph "[See abstract base class]"
///
/// \ref sparseDiGraph "[See sparse implementation]"
///
/// \ref denseDiGraph "[See dense implementation]"
///
/// <br>
///
///
/// \subsection secUndirGraphs  Undirected graphs
///
/// The class \ref abstractGraph is the base class for all undirected graphs.
/// It implements the matching and T-join solvers, and specialized methods
/// for TSP, max-cut and Steiner trees.
///
/// \ref abstractGraph "[See abstract base class]"
///
/// \ref sparseGraph "[See sparse implementation]"
///
/// \ref denseGraph "[See dense implementation]"
///
/// <br>
///
///
/// \subsection secBigraphs  Bigraphs (bipartite graphs)
///
/// Bigraphs divide the node set into two parts such that edges run from one
/// part to the other. For several optimization problems, the bipartite case
/// is either trivial (e.g. node colouring, stable set) or at least, there are
/// special algorithms (e.g. matching, edge colouring).
///
/// \ref abstractBiGraph "[See abstract base class]"
///
/// \ref sparseBiGraph "[See sparse implementation]"
///
/// \ref denseBiGraph "[See dense implementation]"
///
/// <br>
///
///
/// \section secReprAttributes  Representational attributes
///
/// Representational attributes denote the data which must be added to a plain
/// graph in order to obtain a network programming problem instance. That are so far:
/// - Lower and upper edge capacity bounds which restrict the set of feasible subgraphs
/// - Edge length labels which define linear objective functions
/// - Node demands which define terminal or source node sets
///
/// The edge length labels are special in the following sense: In geometric problem
/// instances, the edge lengths are functional dependent on the node coordinate values
/// irrespective of what is set in the physical edge length attribute.
///
/// \ref queryReprAttributes "[See API]"
///
/// <br>
///
///
/// \section secNodeCoordinates  Node coordinate values
///
/// To the nodes of a represented graph, coordinate values can be assigned.
/// Coordinate values form part of the graph drawing, but also can define the
/// edge length labels. The latter depends on the TokGeoMetric
/// attribute value. Currently, there no distinction between display coordinates
/// and length defining coordinates.
/// 
/// When coordinate values exist, one says that the graph is geometrically embedded.
/// All nodes have the same dimension, and this dimension is either 0 or 2.
/// Three-dimensional embeddings are not well-supported yet since all layout
/// methods produce 2D drawings.
///
/// Coordinate values are also provided for all potential layout points. But note
/// that these coordinate values might be relative to graph node coordinates.
///
/// \ref nodeCoordinates "[See API]"
///
/// <br>
///
///
/// \section relatedGraphObjects  Related topics
///
/// - \ref pageGraphSkeleton
/// - \ref pageRegisters
/// - \ref pageSubgraphManagement
/// - \ref pageGraphManipulation
/// - \ref pagePlanarity
/// - \ref secMapIndices
///
/// \ref index "Return to main page"


/// \defgroup arcOrientations  Arc orientations
/// \defgroup queryIncidences  Node and arc incidences
/// \defgroup nodeAdjacencies  Node adjacencies

/// \page pageGraphSkeleton  Graph skeleton
///
/// The graph skeleton denotes all information defining the incidence structure
/// of a graph:
/// - Start and end nodes of a given arc
/// - A node incidence list for every node
/// - Information which edges are directed
///
/// There is a universal interface to retrieve information about the skeleton.
/// That is, dense graphs also have a logical view of incidence lists, but no
/// according memory representation. Only the skeleton of graphs with a sparse
/// representation can be manipulated.
///
/// <br>
///
///
/// \section secArcOrientations  Arc orientations
///
/// Basically, edges are represented by indices in an interval [0,1,..,m-1].
/// Technically however, every edge (say with index i) has two directions
/// (2i and 2i+1), and most methods expect edge parameters including the arc
/// direction bit. This is independent of whether the whole graph is directed,
/// undirected or mixed.
///
/// Observe that Orientation(2i) and Orientation(2i+1) are the same, and this
/// tells whether the edge i is directed. In that case, Blocking(2i) returns false
/// but Blocking(2i+1) returns true. The latter method is used to generalize
/// optimization methods from directed to mixed graphs (e.g. for minimum spanning
/// arborescence or max-cut). It is not applied by shortest path methods which
/// occasionally operate on implicitly modified digraphs (residual networks).
///
/// \ref arcOrientations "[See API]"
///
/// <br>
///
///
/// \section secQueryIncidences  Node and arc incidences
///
/// From the logical viewpoint, all graph objects - not just those with a sparse
/// representation - own an incidence structure to be investigated by algorithms:
/// - StartNode(i) and EndNode(i) give the two nodes which are connected by the arc
///   i with the identity StartNode(i)==EndNode(i^1).
/// - First(v) gives either NoArc or an arc index such that StartNode(First(v))==v.
/// - Right() can be used to enumerate all incidences of a particular node,
///   including the blocking backward arcs. One has StartNode(Right(i))==StartNode(i).
///
/// For dense graphs, all these method internally calculate node indices from arc
/// indices or vice versa. Sparse graphs have incidence lists represented in memory.
///
/// Node incidence lists cannot only be searched by directly calling First() and
/// Right(). There are proxy objects, called investigators, maintaining an incidence
/// list pointer (i.e. an arc index) for every graph node.
///
/// \ref queryIncidences "[See API]"
///
/// <br>
///
///
/// \section secNodeAdjacencies  Node adjacencies
///
/// In some situations, it is necessary to decide whether two given nodes are
/// adjacent (that is, if the nodes are joined by an edge) or even to know a
/// connecting arc. By default, there is no data structure for this purpose.
/// For dense graphs, a call to Adjacency() calculates the connecting arc only
/// by using the given node indices. In its general implementation, the method
/// uses a hash table which is build on the first call. Observe that this hash
/// table is invalidated by any maipulation of the graph skeleton.
///
/// \ref nodeAdjacencies "[See API]"
///
/// <br>
///
///
/// \section relatedGraphSkeleton  Related topics
///
/// - \ref secManipSkeleton
/// - \ref secManipIncidences
///
/// \ref index "Return to main page"


/// \defgroup distanceLabels  Distance labels
/// \defgroup nodePotentials  Node potentials
/// \defgroup nodeColours  Node colours
/// \defgroup edgeColours  Edge colours
/// \defgroup predecessorLabels  Predecessor labels

/// \page pageRegisters  Registers
///
/// Registers denote a variety of graph attributes which are intended to store
/// algorithmic results persistently. All registers are collected into a single
/// attribute pool, and this pool can be accessed by the return of Registers().
///
/// Opposed to the other graph attributes, registers are available for all graph
/// objects, not just for represented graphs. If a graph is represented, manipulations
/// of its skeleton ensure that the register attributes are updated accordingly.
///
/// <br>
///
///
/// \section secNodeColours  Node colours
///
/// The node colour register assigns integer values to the graph nodes, with
/// the intended value range [0,1,..,n-1] + {NoNode}. The applications are:
/// - Orderings of the node set (e.g. st-numberings)
/// - Partitions of the node set (e.g. clique partitions)
///
/// \ref nodeColours "[See API]"
///
/// <br>
///
///
/// \section secEdgeColours  Edge colours
///
/// The edge colour register assigns integer values to the graph arcs, with
/// the intended value range [0,1,..,2m-1] + {NoArc}. The applications are:
/// - Orderings of the arc set (e.g. Euler cycles)
/// - Partitions of the arc set (e.g. partitions into 1-matchings)
/// - Implicit arc orientations (e.g. feedback arc sets)
/// Other than the subgraph multiplicities, edge colours are stored by plain
/// arrays. So, especially for geometric graph instances, the number of arcs
/// is large relative to the size of memory representation. It is therefore
/// recommended to handle subgraph incidence vectors by edge colours only if
/// non-trivial lower capacity bounds prevent from using the subgraph
/// multiplicities.
///
/// \ref edgeColours "[See API]"
///
/// <br>
///
///
/// \section secPredecessorLabels  Predecessor labels
///
/// The predecessor register assigns arc indices to the graph nodes. More
/// particularly, the predecessor arc of a node is either undefined or points
/// to this node.
///
/// Any directed path encoded into the predecessor labels can be backtracked
/// from its end node without scanning the incidence lists of the intermediate
/// nodes.
/// So, whenever possible, subgraphs are stored by the predecessor register:
/// - Simple directed paths and cycles
/// - Rooted trees
/// - One-cycle trees consisting of a directed cycle and some trees pointing
///   away from this cycle
/// - Any node disjoint union of the listed subgraph types
///
/// \ref predecessorLabels "[See API]"
///
/// <br>
///
///
/// \section secDistanceLabels  Distance labels
///
/// The distance label register maintains the results of shortest path methods.
///
/// \ref distanceLabels "[See API]"
///
/// <br>
///
///
/// \section secNodePotentials  Node potentials
///
/// The node potential register maintains the dual solutions for min-cost network
/// flow problems. That is, network flow optimality can be verified at any time
/// after a flow has been been computed, and later calls to the min-cost flow
/// solver will take advantage of the dual solution.
///
/// \ref nodePotentials "[See API]"
///
/// <br>
///
///
/// \ref index "Return to main page"


/// \defgroup subgraphManagement  Subgraph management
/// \defgroup degreeLabels  Node degree labels

/// \page pageSubgraphManagement  Subgraphs
///
/// As the name indicates, subgraph multiplicities encode all kinds of subgraphs
/// and flows, only restricted by the lower and upper capacity bounds.
///
/// Like register attributes, subgraph multiplicities are defined for all graph
/// objects. Other than registers, subgraph multiplicities are represented by
/// arrays only for sparse graphs. For dense graphs, a hash table is used. By
/// that, the memory to store subgraphs grows proportional with the subgraph
/// cardinality. For logical graph instances (e.g. network flow transformations),
/// subgraph multiplicities are not stored separately, but synchronized with the
/// subgraph multiplicities of the original graph.
///
/// \ref subgraphManagement "[See API]"
///
/// <br>
///
///
/// \section secDegreeLabels  Node degree labels
///
/// Degree labels denote the node degrees according to the current subgraph
/// multiplicities. The degree label of a node cumulates the multiplicity of the
/// arcs incident with this node. Actually there are different fuctions Deg(),
/// DegIn() and DegOut(), the first to count the undirected edges, the second to
/// count the entering directed arcs, and the third to count the emanating
/// directed arcs.
///
/// The degree labels are indeed represented in memory. For the sake of efficiency,
/// the following is implemented:
/// - The degree labels are only generated with a call to Deg(), DegIn() or DegOut().
///   But then all node degrees are computed in one pass.
/// - Once generated, with every change of a subgraph multiplicity, the degree labels
///   of the respective end nodes are adjusted.
/// - The degree labels can be disallocated again to save this book keeping operations
///   by calling ReleaseDegrees(). This in particular happens when the subgraph
///   is reset by using InitSubgraph().
/// For the time being, the degree labels of a sparse graph are corrupted by
/// graph skeleton manipulations.
///
/// \ref degreeLabels "[See API]"
///
/// <br>
///
///
/// \section relatedSubgraphManagement  Related topics
///
/// - \ref pageNetworkFlows
/// - \ref secMatching
///
///
/// \ref index "Return to main page"



/// \defgroup nodePartitions  Node partitions
/// \ingroup graphObjects


/// \defgroup manipSkeleton  Manipulating the graph skeleton
/// \defgroup manipIncidences  Manipulating node incidence orders

/// \page pageGraphManipulation  Graph manipulation
///
/// A graph object can be manipulated only if it is represented, and manipulation
/// methods are addressed to its representational object. Register atttributes
/// are not attached to representational objects, but are available for every
/// graph object. That is, register attribute value can be set and modified for
/// logical graph instances also.
///
/// Both sparse and dense representations allow changes of the representational
/// attributes (capacity bounds, arc lengths, node demands) but the skeleton can
/// be manipulated only for sparsely represented graphs.
///
/// A pointer to the representational object is returned by the method
/// abstractMixedGraph::Representation() which is defined for every graph object,
/// but returns a NULL pointer for non-represented graph objects.
///
///
/// <br>
///
///
/// \section secManipSkeleton  Manipulating the graph skeleton
///
/// In order to manipulate the skeleton of a sparse graph G, the representational
/// object of G must be dereferenced and type-casted like this:
///
/// <code> static_cast< \ref sparseRepresentation *>(G.Representation()) </code>.
///
/// Sparse representational objects admit the following operations:
/// - Arc insertions and deletions
/// - Node insertions and deletions
/// - Reversion of the (implicit) arc orientations
/// - Swap the indices of a pair of nodes or of a pair of arcs
/// - Arc contraction to a single node
/// - Identification of a pair of nodes
///
/// Also, the graph layout code is partially implemented by this class.
///
///
/// \subsection secManipCasePlanar  Planarity issues
///
/// In principle, these operations maintain planar representations. For the arc
/// insertions and the node identification operations, this can only work if the
/// two (end) nodes are on a common face, practically the exterior face. If the common
/// face exist, but is not the exterior face, abstractMixedGraph::MarkExteriorFace()
/// must be called to make it temporarily exterior (This sets the First() indices
/// after that new arcs are inserted into the incidence list).
///
///
/// \subsection secInvalidation  Invalidation rules
///
/// The following operations invalidate existing node and arc indices:
/// - sparseRepresentation::DeleteArc()
/// - sparseRepresentation::DeleteNode()
/// - sparseRepresentation::DeleteArcs()
/// - sparseRepresentation::DeleteNodes()
///
/// That is, after the call DeleteArc(a), arc indices might address different
/// data before and after the arc deletion. This does not only concern the
/// specified index a. The method DeleteNode() causes even more trouble, since
/// node deletions imply the deletion of all incident arcs.
///
/// It is possible to delay these index invalidation effects by using the method
/// sparseRepresentation::CancelArc() instead of DeleteArc(a). Doing so, the
/// edge is deleted from its end nodes incidence lists (so that subsequent graph
/// search won't traverse it), but all arc attributes remain unchanged. After the
/// process of arc deletions has been completed, a call of DeleteArcs() will
/// pack all attributes.
///
/// The CancelNode() and DeleteNodes() methods work pretty similar as CancelArc()
/// and DeleteArc(). But observe that programming sparseRepresentation::CancelNode(v)
/// implies a CancelArc() call for every arc incident with v, and that DeleteNode(v)
/// and DeleteNodes() imply a call of DeleteArcs().
///
/// \ref manipSkeleton "[See API]"
///
/// <br>
///
///
/// \section secManipIncidences  Manipulating node incidence orders
///
/// A node incidence list enumerates all arcs with a given start node. The lists
/// are defined by the method abstractMixedGraph::Right() and are essentially
/// circular. But there is a special list entry abstractMixedGraph::First() to
/// break the circular list at.
///
/// Manipulating the incidence order can be done by two means:
/// - Calling sparseRepresentation::SetRight(): This cuts a circular list in three
///   pieces and relinks the pieces such that a single circular list is maintained.
///   This is a tricky operation if several concurrent calls are necessary, since
///   the relative order of the arcs passed to SetRight() is restricted.
/// - Calling sparseRepresentation::ReorderIncidences() takes an array of arc
///   indices which specifies for every arc index a1 in [0,1,..,2m-1] another
///   arc index which is set as the return value of Right(a1). So the procedure
///   assigns all right-hand arc indices simultaneously. It is not so difficult
///   to handle, but efficient only if the incidence orders can be assigned in
///   one pass.
///
/// Observe that in a \ref pagePlanarity "planar representation", the incidence
/// lists of exterior nodes (node on the exterior face) start with two exterior
/// arcs (there might be more than two candidates if the node is a cut node).
/// So in the planar case, rather than setting the First() indices directly, it
/// is recommended to call abstractMixedGraph::MarkExteriorFace() which adjusts
/// the planar representation consistently such that the left-hand region of the
/// passed arc becomes the exterior face.
///
/// \ref manipIncidences "[See API]"
///
/// <br>
///
///
/// \section relatedManipulation  Related topics
///
/// - \ref pageGraphSkeleton
/// - \ref secPlanarEmbedding
/// - \ref secSeriesParallel
///
/// \ref index "Return to main page"


/// \defgroup planarRepresentation  Planar representation
/// \defgroup planarEmbedding  Planarity recognition and combinatorial embedding

/// \page pagePlanarity  Planarity
///
/// A drawing of a graph is called plane if no pair of edges crosses each other.
/// A graph is planar if it admits a plane drawing. This definition of planar
/// graphs is fairly independent of the allowed class of edge visual representations
/// (straight lines, Jordan curves or open polygones).
///
/// Planar graphs often allow efficient optimization where the general case is
/// hard to solve (e.g. maximum cut). Usually, codes on planar graphs do not
/// use explicit drawings but only the respective clockwise ordering of the
/// node incidence lists. If the incidence lists are ordered as in some plane
/// drawing, this is called a planar representation.
///
/// <br>
///
///
/// \section secPlanarRepresentation  Planar representation
///
/// Consider the situation where a sparse graph is given, and one wants to know
/// if the given node incidence lists form a planar representation and
/// occasionally to generate an according plane drawing. In that case, it does
/// not help to perform a planarity test since the graph might be planar but this
/// particular incidence lists do not form a planar representation.
/// Depending on the application, it might be also unwanted to compute a
/// planar representation from scratch.
///
/// For the described purpose, one calls ExtractEmbedding() which determines the
/// left-hand face indices of all arcs in the plane case. Later on, these indices
/// can be retrieved by abstractMixedGraph::Face(). And arcs with the same face
/// index are on the counter-clockwise boundary of this face in any compliant
/// plane drawing.
///
/// Assigning left-hand face indices means to implicitly generate the dual graph.
///
/// \ref planarRepresentation "[See API]"
///
/// <br>
///
///
/// \section secPlanarEmbedding  Planarity recognition and embedding
///
/// A planarity test decides whether a graph is planar or not without having a
/// combinatorial embedding, but only an arbitrary order in the node incidence
/// lists. In addition, a plane embedding code reorders the node incidence lists
/// to form a planar representation.
/// 
/// One of the most famous theorems on graphs states that every non-planar graph
/// has a K_5 or a K_3_3 minor, that are subgraphs obtained by subdividing the
/// edges of a K_5 (a complete graph on five node) or of a K_3_3 (a complete bigraph
/// with three nodes in each part).
/// 
/// While generating the dual graph / the faces is a certificate fora proper
/// planar representation, the mentioned minors allow to verify a negative result
/// of the plane embedding code. Note that only the Hopcroft/Tarjan code can
/// derive such minors, and does only when this is explicitly required.
///
/// \ref planarEmbedding "[See API]"
///
/// <br>
///
///
/// \section relatedPlanarity  Related topics
///
/// - \ref secPlanarDrawing
/// - \ref secSeriesParallel
/// - \ref secPlanarComposition
///
/// \ref index "Return to main page"


/// \defgroup pageProxyAndComponents  Graph proxy and component objects
/// \ingroup dataObjects
/// \defgroup groupAttributes  Attributes and attribute pools
/// \ingroup pageProxyAndComponents
/// \defgroup groupIndexSets  Index sets
/// \ingroup pageProxyAndComponents
/// \defgroup groupInvestigators  Investigators
/// \ingroup pageProxyAndComponents

/// \page pageProxyAndComponents  Graph proxy and component objects
///
/// \section secAttributes  Attributes and attribute pools
///
/// An \ref attribute denotes an object to store array like data. In fact, it
/// is composed from an STL vector which is memory layout compatible with a plain
/// C arrays. This vector is augmented by some functionality:
/// - Default values: The method attribute<T>::GetValue() returns a default value
///   for every index outside the vector index range
/// - Maxima and minima: It is possible to retrieve the indices at which extremal
///   values are achieved. These indices are computed only on demand, maintained
///   as long as this is efficient, and safely invalidated otherwise
///
/// Basically, attributes have been designed to store data indexed by graph arc
/// and node indices. With respect to constant attribute values (e.g. all node
/// demands equal to one), it is possible to keep the representational vector
/// size zero. There are variants of the manipulation procedures known for STL
/// vectors which exclude constant attributes.
///
/// Graph attributes are collected into \ref attributePool objects in which they
/// can be addressed by a so-called <em>token</em>. This denotes an index in the
/// \ref TPoolTable array which is associated with this attribute pool. The pool
/// table lists clear text names, type and size info of all possible attributes.
/// In practice, only few of the listed attributes are allocated.
///
/// All managed graph attributes conform with manipulations of the incidence
/// structure. To allocate attributes of the correct vetor length, it takes the
/// method \ref abstractMixedGraph::SizeInfo() which reports about the problem
/// dimensions.
///
/// Direct access to attributes definetely improves the performance. In order
/// to extract attributes, the contained vectors or the contained arrays from
/// the managing pool object, represented graph objects grant public access
/// to all its compound attribute pools:
/// - \ref abstractMixedGraph::Registers(): For all graph objects, the pool of
///   register data (usually the algorithmic solutions).
/// - \ref abstractMixedGraph::RepresentationalData(): For represented graphs
///   only, the attributes which are input to optimization methods such as:
///   arc capacity bounds, node demands, arc orientations.
/// - \ref abstractMixedGraph::Geometry(): For represented graphs only, all
///   node coordinates.
/// - \ref abstractMixedGraph::LayoutData(): For represented graphs only, all
///   layout parameters, and all arc routing information.
///
/// But observe that direct manipulation of attributes can corrupt the graph
/// representation. For example, reducing an upper arc capacity bound might be
/// incompatible with the lower bound and the subgraph multiplicity. Manipulating
/// attributes by means of the representational object preserves data integrity.
///
/// \ref groupAttributes "[See API]"
///
/// <br>
///
///
/// \section secIndexSets  Index sets
///
/// An <em>index set</em> is an object specifying a certain set of integer values.
/// Such objects are in particular useful for algorithms which apply to arbitrary
/// graph node or edge subsets (e.g. for multi terminal problems like
/// \ref secMatching "T-joins"). Of course, the concept of index set is not
/// restricted to applications on graphs.
///
/// For all index set subclasses, the operation indexSet::IsMember(i) verifies the
/// membership of the index i in constant time. Particular subclasses may or may
/// not provide efficient methods to enumerate the contained indices. The default
/// implementations of indexSet::First() and indexSet::Successor() scan the full
/// index range. For the current applications, this is not performance-critical
/// since the graph algorithms run over the full index range anyway, for sake of
/// initialization.
///
/// There are \ref indexSet derivates which are absolutely generic, namely to
/// specify empty sets, full index intervals 0,1,..,r-1, or singleton sets.
/// There are classes to define the union, meet or difference of two index sets,
/// and index set complements. Other index set objects refer to the nodes or
/// the arcs of a particular graph with a certain property.
///
/// It is worth to note that contiguous memory stacks and queues also work as
/// index sets.
///
/// \ref groupIndexSets "[See API]"
///
/// <br>
///
///
/// \section secInvestigators  Investigators
///
/// An <em>investigator</em> object basically represents an iterator on the
/// incidence list of each node of a particular graph. Investigators are useful
/// whenever the graph has to be search in any order different from BFS.
/// The only need to deal with investigators explicitly is when a new graph
/// searching algorithm has to be encoded.
///
/// Most graphs can be handled by the default implementation \ref iGraph, but
/// some graph classes come with an own investigator class. There is a factory
/// method abstractMixedGraph::NewInvestigator() which supplies with investigator
/// objects of the correct type.
///
/// According to the frequent investigator applications in the library, there
/// is a mechanism to cache iterator ojbects instead of new / delete operations.
/// Iterators taken from the cache are called <em>managed investigators</em>.
/// So, a typical application code does not contain constructor or factory method
/// calls but only calls to abstractMixedGraph::Investigate() which tries to avoid
/// memory allocations.
///
/// \ref groupInvestigators "[See API]"
///
/// \ref index "Return to main page"


/// \defgroup groupContainers  Container Objects
/// \ingroup dataObjects
/// \defgroup groupFixedIndexRangeContainers  Fixed index range containers
/// \ingroup groupContainers
/// \defgroup groupNodeBasedContainers  Node based containers
/// \ingroup groupContainers
/// \defgroup groupPriorityQueues  Priority queues
/// \ingroup groupFixedIndexRangeContainers
/// \defgroup setFamilies  Set families
/// \ingroup groupFixedIndexRangeContainers

/// \page pageContainers  Container objects
///
/// A <em>container</em> is a data structure to store other data objects.
/// Most container applications in this library concern queuing of nodes
/// and arcs by their indices, and eliminating indices from queues in certain
/// order.
///
/// Some algorithms can be equipped with different kinds of queues, so the
/// push/relabel method for \ref secMinCostFlow "min-cost flow".
/// To this end, there is a common interface class \ref goblinQueue with
/// methods for insertion and deletion of members.
///
/// \ref groupContainers "[See API]"
///
/// <br>
///
///
/// \section secFixedIndexRangeMemory  Fixed index range containers
///
/// <em>Fixed index range containers</em> are functional equivalent with the STL
/// stack<TIndex> and STL queue<TIndex> classes. Other than in the STL, fixed
/// index range containers are not really sequential. That is, the physical
/// memory representation does not conform with the logical order of elements.
/// The containers can only store unsigned integers (indices) of a range
/// 0,1,..,r-1 which must be specified at the time of instanciation. The
/// advantages are following:
/// - No memory reallocations occur
/// - The containers also specialize the \ref indexSet interface and one can
///   test for membership in constant time.
/// - Container objects can share physical memory, as long as the represented
///   index sets are disjoint. Even more, when sharing the physical memory,
///   data integrity of disjoint index sets can either be enforced by using the
///   \ref staticQueue::INSERT_NO_THROW option, or conflicting insertion operations
///   will trigger an exception.
///
/// \ref groupFixedIndexRangeContainers "[See API]"
///
/// <br>
///
/// \cond DOCUMENT_THIS_LATER
/// \section secNodeBasedContainers  Node based containers
///
/// <em>Node base containers</em> denote objects which allocate a chunk of memory
/// for every stored element (the nodes), and maintain links amongs these nodes.
/// 
///
///
///
/// \ref groupNodeBasedContainers "[See API]"
///
/// <br>
/// \endcond
///
/// \section secPriorityQueues  Priority queues
///
/// A priority queue stores indices with a numeric key value. Elements are
/// deleted in the order of \em increasing key value. Key values are assigned
/// when elements are added, but can also be modified later. In general, this
/// \ref goblinQueue::ChangeKey() method cannot be reduced to
/// \ref goblinQueue::Insert() / \ref goblinQueue::Delete() pairs. All these
/// operations roughly run in logarithmic time, depending on the actual
/// \ref goblinQueue::Cardinality(), not on the index range.
///
/// \ref groupPriorityQueues "[See API]"
///
/// \ref index "Return to main page"


/// \defgroup objectDimensions      Object dimensions
/// \ingroup dataObjects
/// \defgroup classifications       Object classifications
/// \ingroup dataObjects


/// \defgroup shortestPath      Shortest path methods
/// \defgroup minimumMeanCycle  Minimum mean cycles
/// \defgroup eulerPostman      Eulerian cycles and supergraphs

/// \page pageEdgeRouting  Edge routing problems
///
/// \section secShortestPath  Shortest path methods
///
/// When applying the shortest path solver, one probably thinks of simple paths
/// (paths not repeating any node). In fact, all paths / cycles / trees are
/// represented by the predecessor labels which naturally imposes a restriction
/// to simple paths or cycles. The label correcting codes technically determine
/// walks (which possibly repeat arcs) rather than simple paths and fail if both
/// notations diverge for this graph instance.
///
/// The method to compute shortest a path between two given nodes s and t heavily
/// depends on the configuration of arc length labels and orientations. The
/// following list shows the available codes in decreasing order of computational
/// efficiency, and it is recommended to apply the first method which is feasible
/// for the given instance:
/// - In the case of coinciding, non-negative length labels, a breadth first search is sufficient
/// - If the graph is directed acyclic, apply the DAG search method
/// - If all length labels are non-negative, apply the Dijkstra method
/// - If all negative length arcs are directed, apply a label-correcting method
/// - If the graph is undirected, apply the T-join method (with T={s,t})
///
/// The general NP-hard setting with negative length cycles is not handled by the library.
///
/// Most library methods determine a shortest path tree routed at a given source
/// node s rather than a single shortest path between a given node pair s and t,
/// and this tree is exported by the predecessor labels.
/// Specifying a target node t is relevant for the BFS and the Dijkstra method
/// since the seach can be finished prematurely, when t is reached the first time.
/// Only the T-join method constructs an exports a single st-path.
///
/// The following figure shows an undirected graph and a shortest path between
/// the red filled nodes. Since negative length edges exist, only the T-join
/// method is formally applicable (undirected edges are interpreted as antiparallel
/// arc pairs in the other codes). If one inspects the graph thoroughly, one finds
/// a negative-length cycle. In general, the T-join method will be aborted when
/// extracting the st-path from the T-join subgraph in such a situation. But if
/// the minimum T-join is nothing else than an st-path, it is a shortest st-path:
/// \image html  tjoinShortestPath.gif
/// \image latex tjoinShortestPath.eps
///
/// \ref shortestPath "[See API]"
///
/// <br>
///
///
/// \section secMinimumMeanCycle  Minimum mean cycles
///
/// Several min-cost flow methods search for negative length cycles in the so-called
/// residual network and push the maximum possible augment of flow on these cycles.
/// If the cycles are arbitrary, the number of iterations cannot be bounded polynomially.
/// But one obtains a strongly polynomial running time if the cycles are <em> minimum
/// mean cycles</em>, that is, if the total edge length divided by the number of edges
/// is minimal. Actually, this min-cost flow method has poor performance compared
/// with the primal network simplex code. The core minimum mean cycle code is provided
/// only to serve other applications in the future.
///
/// \ref minimumMeanCycle "[See API]"
///
/// <br>
///
///
/// \section secEulerPostman  Eulerian cycles and supergraphs
///
/// An Eulerian cycle is a cycle which meets every edge exactly once and which
/// traverses every edge in a non-blocking direction. Graphs which admit such
/// cycles are called Eulerian graphs. Due to the interpretation of arc capacities
/// as arc multiplicities, and the fact that Eulerian cycles are stored like an
/// ordering of the edge set, computing an Eulerian cycle requires that the upper
/// capacity bounds are all one. If this is not the case, use
/// sparseRepresentation::ExplicitParallels() for preprocessing.
///
/// The following is a so-called Sierpinksi triangle. This is a recursively
/// defined, infinite graph, and Eulerian at any recurrency level. An Eulerian
/// cycle is displayed both by the edge labels and the edge colours:
/// \image html  eulerSierpinski.gif
/// \image latex eulerSierpinski.eps
///
/// The well-known Chinese postman problem asks for an increase of the upper
/// capacity bounds (interpreted as arc multiplicities) such that the graph
/// becomes Eulerian and the length of an Eulerian cycle is minimal. In the
/// undirected setting (and only then), it is equivalent to determine a maximum
/// length Eulerian subgraph. The library handles the directed and the undirected
/// case, but not the generalized NP-hard problem for mixed graphs.
/// \image html  postmanBefore.gif
/// \image latex postmanBefore.eps
///
/// For the above graph, with the shown length labels and unit capacities, the
/// following picture shows a maximum Eulerian subgraph in bold face, and a
/// minimum length Eulerian augmentation:
/// \image html  postmanAfter.gif
/// \image latex postmanAfter.eps
///
/// \ref eulerPostman "[See API]"
///
/// <br>
///
///
/// \section relatedEdgeRouting  Related topics
///
/// - \ref pageNetworkFlows
/// - \ref secMatching
///
///
/// \ref index "Return to main page"


/// \defgroup flowLowLevel  Low level operations
/// \defgroup maximumFlow   Maximum st-flow
/// \defgroup minCostFlow   Minimum cost st-flow, b-flow and circulation

/// \page pageNetworkFlows  Network flows
///
/// By a <em>pseudo-flow</em>, one usually denotes a subgraph multiplicity vector
/// bounded by given lower and upper capacity vectors. In the library, this is
/// only true for directed arcs. For undirected edges, lower capacity bounds have
/// to be zero, the absolute flow value is bounded by the upper capacity bound,
/// and a negative flow value denotes a flow running in the implicit backward
/// direction.
///
/// The standard network flow models also use the node demand labels and the
/// concept of node divergence: The divergence of a node v is the flow sum of
/// arcs entering v minus the flow sum of arcs leaving v. A b-flow is a
/// pseudo-flow where all node divergences match the respective node demands
/// (say: the nodes are balanced). So in the standard notation, the node
/// demand vector is abbreviated by b.
///
/// <br>
///
///
/// \section secFlowLowLevel  Low level operations
///
/// There is a technique which occurs in nearly all network flow methods, called
/// \em augmentation. This means pushing flow along a given path, increasing the
/// subgraph multiplicities of forward arcs and decreasing the subgraph
/// multiplicities of arcs which occur in backward direction. For a single arc,
/// this is exactly what abstractDiGraph::Push() does to the subgraph multiplicities
/// (the procedure also updates the node degree labels).
///
/// There are two obvious applications of the augmentation technique:
/// - The augmenting path is a cycle and the (residual) edge length of this cycle
///   is negative. Then augmentation will decrease the overall costs of the flow.
/// - The start node of the augmenting path is oversaturated and the end node
///   is undersaturated. In that case, augmentation will reduce the node imbalances.
///
/// In both situations, the methods abstractDiGraph::FindCap() and abstractDiGraph::Augment()
/// apply. The first determines the minimum amount of flow which can be pushed along
/// a path, and the second method actually modifies the subgraph multiplicities and
/// the node degree labels.
///
/// Augmenting paths are usually determined by searching the so-called <em>residual
/// network</em> which is implicitly defined by the functions abstractMixedGraph::ResCap()
/// and abstractMixedGraph::RedLength(). The latter function depends on an array
/// of node potentials (usually stored in the so-named register) and is called in
/// the weighted setting only.
///
/// \ref flowLowLevel "[See API]"
///
/// <br>
///
///
/// \section secMaximumFlow  Maximum st-flow and feasible b-flow
///
/// The maximum st-flow problem distinguishes two nodes s (the source) and t (the
/// sink or target) from the other graph nodes. An st-flow denotes a pseudo-flow
/// which is balanced at all nodes other than s and t. It is maximum if the flow
/// divergence at s is minimized (and maximized at t).
///
/// The following shows a maximum st-flow where s is the left-most node, and t
/// is the right-most node in the drawing. A minimum cut is indicated by the
/// node colours, and the node labels of the white left-hand nodes denote the
/// distance from source in the residual graph:
/// \image html  maxFlow.gif
/// \image latex maxFlow.eps
///
/// A feasible b-flow (a b-flow which satisfies the capacity bounds) can be
/// determined in the same time complexity order as a maximum st-flow. This is
/// achieved by identifying the divergent nodes and then transforming to a
/// maximum st-flow problem instance.
///
/// \ref maximumFlow "[See API]"
///
/// <br>
///
///
/// \section secMinCostFlow  Minimum cost st-flow, b-flow and circulation
///
/// The min-cost flow solver is one of the library cornerstones for several resaons:
/// - Min-cost flow is indeed one of the most important graph optimization models.
///   An important library internal application is in the weighted matching code.
/// - The solver includes various alternative methods, some are intended for practical
///   computations, others have been added only for didactic purposes.
/// - The primal network simplex, the cost-scaling and the capacity scaling method
///   are performant enough to solve problem instances with 10000s of edges.
/// - Min-cost flow perfectly illustrates the application of linear programming
///   duality to graph optimization models.
///
/// The min-cost flow dual variables in terms linear programming are called <em>
/// node potentials </em> as usual, and are stored by the \ref secNodePotentials
/// "register attribute" with this name. All min-cost flow methods return with
/// an optimal flow and with optimal node potentials. When the solver is restarted,
/// it dependends on the particular method and on the intermediate manipulations,
/// if and how the method draws benefit of maintaining the node potentials:
/// - The Klein and the minimum mean cycle canceling method operate on the
///   original edge lengths and so do not depend on the node potentials.
///   In principle, the Klein method computes node potentials in every iteration
///   from scratch.
/// - The cost scaling method initializes the scaling parameter with the
///   most-negative reduced arc length. If nothing has changed since the last
///   solver call, the method detects this.
/// - The capacity scaling method starts with an arbitrary pseudo-flow and searches
///   a restricted residual network depending on the scaling parameter and the reduced
///   edge length. Again, If nothing has changed since the last solver call, no
///   flow augmentations or potential updates occur.
/// - The primal network simplex method starts with growing a spanning tree from
///   the edges with zero reduced length. Even if nothing has changed since the
///   last solver call, degenerate pivots can occur. Explicit application of an LP
///   solver is similar.
///
/// \ref minCostFlow "[See API]"
///
/// <br>
///
///
/// \section relatedNetworkFlows  Related topics
///
/// - \ref secShortestPath
/// - \ref pageEdgeRouting
/// - \ref pageSubgraphManagement
/// - \ref secMatching
///
///
/// \ref index "Return to main page"


/// \defgroup groupMinCut  Minimum cuts
/// \defgroup groupComponents  Connectivity components
/// \defgroup groupPlanarAugmentation  Planar connectivity augmentation

/// \page pageConnectivity  Connectivity and minimum cuts
///
/// \section secMinCut Minimum Cuts
///
/// In the standard setting, a minimum edge cut denotes a node set bipartition of
/// an undirected graph such that a minimum multiplicity sum of the edges with
/// end nodes in different parts results. The library also deals with a couple
/// of problem variations:
/// - The directed setting, better: The general setting of mixed graphs. Only those
///   directed arcs count which are unblocking from the left-hand to the right-hand
///   side.
/// - The setting where a left-hand node and / or a right-hand node are fixed
/// - The setting with restricted node capacities (abstractMixedGraph::NodeConnectivity()
///   and abstractMixedGraph::StrongNodeConnectivity()). If all node capacities are
///   virtually infinite, this is the same as computing edge cuts.
///
/// To all methods, the upper capacity bounds apply as the edge multiplicities and,
/// for the methods dealing with restricted node capacities, the node demands are
/// interpreted as the node capacities.
///
/// The derived cuts a exported by the node colour register with the following
/// interpretation:
/// - Colour index 0 stand for cut nodes
/// - Colour index 1 stand for left-hand nodes
/// - Colour index 2 stand for right-hand nodes
///
/// \ref groupMinCut "[See API]"
///
/// <br>
///
///
/// \section secComponents  Connected components
///
/// Given a fixed connectivity number and characteristic, one might be interested
/// in a connectivity test and maximal connected subgraphs.
///
/// For the edge connectivity and the strong edge connectivity case, the library
/// includes methods to partition the node set such that all partial induced
/// subgraphs are maximally connected. These methods formally return whether the
/// graph object is connected and, if not, export the connected components by
/// the node colour register. The respective codes for 1-edge connectivity,
/// 2-edge connectivity and strong 1-edge connectivity run in linear time. The
/// codes for high connectivity orders iterate on the min-cut methods, and hence
/// are polynomial but not really efficient.
///
/// Concerning vertex connectivity, a partition into edge disjoint blocks is
/// possible (and implemented) only in the low order cases: For 1-connectivity
/// and strong 1-connectivity, the respective edge connectivity methods apply.
/// The depth-first search method abstractMixedGraph::CutNodes() returns the
/// 2-blocks (the edge-disjoint, maximal biconnected subgraphs) by the edge
/// colour register, and the cut nodes by the node colour register. This
/// procedure
///
/// Starting with k=3, the maximal k-connected subgraphs (called <em>k-blocks</em>)
/// are in general not edge disjoint as the following graph illustrates:
/// \image html  cuttingPair.gif
/// \image latex cuttingPair.eps
///
/// Herein, the green nodes form a cutting pair, and the green edge is contained
/// in both 3-blocks. There is the concept of SPQR-trees to represent the 3-blocks,
/// but this is out of the library scope.
///
/// \ref groupComponents "[See API]"
///
/// <br>
///
///
/// \section secPlanarAugmentation  Planar connectivity augmentation
///
/// Especially planar drawing algorithms depend on certain connectivity levels
/// of the input graph. To meet these requirements, the library contains methods
/// to augment planar represented graphs such that the resulting graph is also
/// planar, the derived planar representation can be reduced to the original
/// planar representation and the ouput is connected.
///
/// Here are some caveats:
/// - The methods do not apply to general (non-planar) graphs
/// - The methods do not even apply to implicitly planar graphs, only when a
///   planar representation is at hand
/// - The augmentation methods also require a certain level of connectivity.
///   That is, abstractMixedGraph::Triangulation() requires a biconnected planar
///   graph, and abstractMixedGraph::PlanarBiconnectivityAugmentation() requires
///   a connected graph.
/// - Both methods do not derive minimal augmentations, and do not even achieve a
///   fixed approxiation ratio. Even if each single augmentation step would be
///   optimal, step-by-step application would not.
///
/// Although the library code is very elaborate, there are also theoretical
/// limitations: The planar biconnectivity augmentation problem is NP-hard.
/// For the planar triconnectivity augmentation of biconnected graphs, it is
/// not even known if a polynomial time method is possible.
///
/// Dropping the planarity requirement admits linear time augmentation methods,
/// but no such library code is available.
///
/// \ref groupPlanarAugmentation "[See API]"
///
/// <br>
///
///
/// \ref index "Return to main page"


/// \defgroup spanningTree  Spanning tree methods
/// \defgroup steiner       Steiner trees
/// \defgroup tsp  Travelling salesman

/// \page pageVertexRouting  Vertex routing problems
///
/// \section secSpanningTree  Spanning tree methods
///
/// A tree is a connected, cycle free graph. A tree subgraph which meets all
/// graph nodes is called a spanning tree. One say, a tree is rooted at the node
/// r if for every node v, the tree path connecting r and v is non-blocking.
/// A tree rooted at node r is sometimes called an r-arborescence.
///
/// Trees might be represented by either the predecessor labels, the subgraph
/// multiplicities or the edge colours. Generally, the predecessor label
/// representation is used. Only the Kruskal method differs: At an intermediate
/// step of adding a tree edge, it does not know the arc orientation in the
/// final rooted tree. The subgraph representation is used instead of the edge
/// colours since sparse subgraphs of complete graphs are represented more
/// economically.
///
/// The travelling salesman solver uses the concept of a one-cycle tree exposing
/// an (arbitrary but fixed) node r: This denotes a spanning tree of the nodes
/// other than r, adding the two minimum length edges incident with r. That yields
/// a cycle through r and, when this cycle is contracted to a single node, a
/// spanning tree rooted at the artificial node. Minimum one-cycle trees
/// constitute lower bounds on the length of a Hamiltonian cycle.
///
/// The following shows a set of nodes in the Euclidian plane, a minimum one
/// cycle tree exposing the upper left node, and a minimum spanning tree rooted
/// at the same node:
/// \image html  euclidianOneCycle.gif
/// \image latex euclidianOneCycle.eps
///
/// \ref spanningTree "[See API]"
///
/// <br>
///
///
/// \section secSteinerTree  Steiner trees
///
/// The discrete Steiner tree problem is defined on (potentially sparse) graphs
/// and distinguishs between terminal and Steiner graph nodes. In that setting,
/// a Steiner tree denotes a tree or arborescence which is rooted at some terminal
/// node and which spans all other terminal nodes. The Steiner nodes are spanned
/// only if they form shortcuts between terminal nodes.
///
/// \ref steiner "[See API]"
///
/// <br>
///
///
/// \section secTsp  Travelling salesman
///
/// A Hamiltonian cycle (or simply a tour) is a cycle which meets every graph
/// node exactly once. The travelling salesman problem asks for a Hamiltonian
/// cycle of minimum length.
///
/// As computing an optimal tour is NP-hard, the library includes construction
/// heuristics, methods to compute lower bounds on the minimum tour length, and
/// a branch & bound scheme.
///
/// In the following example, a graph, the length labels and an acoording
/// minimum length tour are shown. The optimality can be concluded from the
/// fact that this tour is also a minimum reduced length one-cycle tree (for
/// the upper left node). The reduced length labels and the node potentials
/// applied for length reduction are also displayed:
/// \image html  tspSubOpt.gif
/// \image latex tspSubOpt.eps
///
/// \ref tsp "[See API]"
///
/// <br>
///
///
/// \ref index "Return to main page"


/// \defgroup stableSet     Stable sets and cliques
/// \defgroup colouring     Colouring
/// \defgroup maxCut        Maximum edge cuts
/// \defgroup feedbackSets  Feedback arc sets
/// \defgroup treePacking   Tree packing
/// \defgroup matching      Matchings & T-Joins

/// \page pageGraphPacking  Packings, coverings and partitions
///
/// \section secStableSet  Stable sets and cliques
///
/// A stable set (or independent node set) is a node set without adjacent node
/// pairs. A clique is a node set such that each pair of contained nodes is
/// adjacent. A vertex cover is a node set which is incident with all edges.
///
/// The solver for the maximum stable set problem uses branch & bound with a
/// fixed clique partition for bounding. Minimum vertex covers are obtained
/// by taking the complementary node set of a maximum stable set. Maximum
/// cliques are obtained by computing maximum stable sets in complementary
/// graphs. The latter is inefficient if the original graph is very sparse.
///
/// The following diagram shows an independent positioning of queens on a chess
/// board. In the underlying graph, nodes are adjacent if they are on a common
/// grid line or diagonal:
/// \image html  queensBoard.gif
/// \image latex queensBoard.eps
///
/// \ref stableSet "[See API]"
///
/// <br>
///
///
/// \section secColouring  Colouring
///
/// A node colouring is an assignment of the node colour register such that the
/// end nodes of every edge have different colours. In other words, this denotes
/// a partition of the node set into stable sets.
///
/// Bipartite graphs mean 2-colourable graphs. Probably the most famous theorem in
/// graph theory states that every planar graph can be coloured with at most 4
/// different colours. The library comes up with a 5-colouring scheme for planar
/// graphs. For the general setting, no efficient approximating method is available.
///
/// Observe that any clique size is a lower bound on the chromatic number (the
/// minimum number of colour classes). If both numbers are equal and a maximum
/// clique is given in advance (by the node colour register!), the node colour
/// enumeration scheme will find a minimal colouring with some luck. Otherwise,
/// only little effort is spent on computing a clique for dual bounding and it
/// is most likely that the enumeration must be interrupted to obtain the heuristic
/// solution found so far.
///
/// Similar to node colouring is the task of partitioning the node set into a
/// minimum number of cliques. In the library, this problem is solved by searching
/// for node colourings in the complementary graph. Needless to say that this is
/// inefficient if the original graph is very sparse. If the graph is triangle
/// free, however, clique partitioning is equivalent with maximum 1-matching, and
/// this can be solved efficiently.
///
/// The following illustrates the clique cover problem. It is obvious that the
/// displayed cover is minimal, since the graph does not include any 4-cliques
/// (and this in turn follows from the fact that the graph is outerplanar):
/// \image html  polarGrid.gif
/// \image latex polarGrid.eps
///
/// \ref colouring "[See API]"
///
/// <br>
///
///
/// \section secMaxCut  Maximum edge cuts
///
/// The maximum cut problem asks for a bipartition of the node set such that
/// the total length of cross edges is maximized where arc capacities are
/// interpreted as multiplicities. Note the difference with the min-cut solver
/// which does not consider arc lengths, only capacities.
///
/// The solver also applies to directed and even mixed graphs. To this end,
/// let the bipartition distinguish into left-hand and right-hand nodes.
/// The goal is to maximize the length of non-blocking arcs in the left-to-right
/// direction.
///
/// The following is a directed unit length and capacity example where the
/// green nodes are left-hand and the red nodes are right-hand nodes:
/// \image html  maxCutDirected.gif
/// \image latex maxCutDirected.eps
///
/// \ref maxCut "[See API]"
///
/// <br>
///
///
/// \section secFeedbackSets  Feedback arc sets
///
/// A feedback arc set of a digraph is an arc set such that reverting these arcs
/// results in a \ref secDirectedAcyclic "directed acyclic graph". Computing a
/// minimum feedback arc set is NP-hard, but near-optimal solutions are used
/// in the \ref secLayeredDrawing "layered drawing" approach to obtain readable
/// drawings where the most arcs run in the same direction.
///
/// There is no elaborate branch & bound scheme for this problem, but only a 1/2
/// approximation method which is pretty analugue of the max-cut approximation.
///
/// \ref feedbackSets "[See API]"
///
/// <br>
///
///
/// \section secTreePacking  Tree packing
///
/// The maximum tree packing problem asks for a maximum cardinality set of edge
/// disjoint spanning trees rooted at a common node, say r. It is well-known that
/// this maximum equals the minimum cardinality of a directed cut with the node
/// r on the left-hand side.
///
/// In principle, the library method for maximum tree packing also applies to
/// the capacitated extension of this problem. But the procedure is only weakly
/// polynomial, and the packing can be saved completely to the edge colour register
/// only in the standard setting since a non-trivial capacity bound C means that
/// this arc can be in C arborescences. The latter limitation can be overcome
/// by using the extended API which allows to generate the arborescences step by
/// step.
///
/// The procedure is little performant, and suited rather to illustrate the min-max
/// relationship with minimum r-cut on small examples. Today, there is no method
/// for the undirected analogue or even the extension to mixed graphs.
///
/// \ref treePacking "[See API]"
///
/// <br>
///
///
/// \section secMatching  Matchings & T-Joins
///
/// In the literature, a b-matching is a subgraph of an undirected graph such
/// that for all nodes, the subgraph node degree does not exceed the node demand,
/// Graph edges may occur in the b-matching with arbitrary multiplicity. A
/// b-matching is called perfect if the subgraph node degrees and the node demands
/// are equal.
///
/// There is also a standard notation of f-factors which differ from perfect
/// b-matchings by that one does not allow for edge multiplicities. The perfect
/// b-matching and the f-factor notation meet in the case of 1-factors respectively
/// perfect 1-matchings which are subgraphs where every node is incident with
/// exactly one edge. And 1-matchings are payed most attention in the literature,
/// usually referring to matchings rather than 1-matchings. An example of a
/// minimum cost 1-factor is given in the following figure:
/// \image html  optMatch.gif
/// \image latex optMatch.eps
///
/// For the library matching solver, edges may be assigned with arbitrary upper
/// capacity bounds so that both f-factors and b-matchings are covered. It is possible
/// to determine maximum matchings and minimum cost perfect matchings, even with
/// specifying differing lower and upper node degree bounds.
///
/// For an undirected graph and some node set T with even cardinality, a T-join
/// is a subgraph such that exactly the nodes in T have odd degree. This model
/// covers several other optimization problems:
/// - Shortest paths problem instances with negative length edges
/// - The Chinese postman problem (to extend a graph to an Eulerian graph)
/// - The 1-matching problem
///
/// While the first two problems are indeed solved in the library by application
/// of a T-join method, the 1-matching problem cannot be solved this way since
/// it self forms part of the T-join method. When optimizing T-joins explicitly,
/// the odd nodes (terminal nodes) are specified by the node demand labels.
///
/// The following figure shows a subgraph with two odd nodes. In fact, it is a
/// minimum T-join for this odd node set. This example shows that a T-join for a
/// 2-element node set T is a path only in certain circumstances. To this end,
/// a sufficient condition is that no negative length cycles exist:
/// \image html  tjoinNoPath.gif
/// \image latex tjoinNoPath.eps
///
/// \ref matching "[See API]"
///
/// <br>
///
///
/// \ref index "Return to main page"


/// \defgroup groupLayoutBasic  Low level procedures
/// \defgroup groupLayoutModel  Layout models
/// \defgroup groupLayoutFilter  Display filters
/// \defgroup groupArcRouting  Arc routing
/// \defgroup groupLayoutCircular  Circular drawing methods
/// \defgroup groupForceDirected  Force directed layout
/// \defgroup groupPlanarDrawing  Planar drawing algorithms
/// \defgroup groupOrthogonalDrawing  Orthogonal drawing algorithms
/// \defgroup groupOrthogonalPlanar  Planar orthogonal drawing
/// \defgroup groupOrthogonalTree  Orthogonal tree drawing
/// \defgroup groupLayeredDrawing  Layered Drawing

/// \page pageGraphLayout  Graph layout
///
/// \section secLayoutModels  Layout models
///
/// The <em>layout model</em> determines many but not all of the display
/// parameters: For example, colours, edge widths and text labels are assigned
/// independently of the layout model, whereas node and arc shape modes are
/// layout model dependent.
///
/// Any transition from one layout model to another requires to call
/// \ref abstractMixedGraph::Layout_ConvertModel() which does some kind of
/// garbage collection for the old layout points and sets the correct display
/// parameters for the new model. When calling a layout method, this is done
/// implicitly. There may be various drawing methods supporting the same layout
/// model!
///
/// In the very basic setting, the <em>straight line model</em>, graph drawing
/// just means to position the graph nodes in the Euclidian plane. But this only
/// works with simple graphs (graphs without parallel edges and loops). Node and
/// arc labels are placed according to certain global rules. It is obvious that
/// such rules produce readable drawings without overlaps only in special cases.
///
/// The are several drawing methods which produce straight line drawings, for
/// example placement of all nodes on a circle or exposing a predecessor tree.
/// The resulting drawings may be input to a
/// \ref secForceDirected "force directed placement (FDP) method". The latter
/// do not always produce straight line drawings but call
/// \ref sparseRepresentation::Layout_ArcRouting() in order to expose possible
/// parallel edges.
///
/// The more elaborate layout models require to distinguish between graph nodes
/// and <em>layout points</em>. The latter are addressed by node indices ranged
/// in an interval [0,1,..,n+ni-1]. All graph nodes are mapped to layout points,
/// called <em>node anchor points</em>. The additional layout points can be
/// grouped as follows:
/// - <em>Arc label anchor points</em> which denote absolute arc labels positions
/// - <em>Edge control points</em> which denote also absolute positions. When arcs are
///   displayed by open polygones, the control points are traversed. In case of smooth
///   curves, it depends on the graphical tool kit if the control point positions are
///   interpolated or only approximated (the regular case).
/// - <em>Arc port nodes</em>
/// - Points to determine the graph node display size. At the time, this is
///   feasible only for the visibility representation model.
///
/// For the time being, graph node indices and the respective node anchor point
/// indices coincide, that is <code>NodeAnchorPoint(v)==v</code>.
///
/// \ref groupLayoutModel "[See API]"
///
/// <br>
///
///
/// \section secArcRouting  Arc routing
///
/// The display of any arc is determined by the following layout nodes:
/// - An arc label anchor point given by \ref abstractMixedGraph::ArcLabelAnchor()
/// - A sequence of control points enumerated by \ref abstractMixedGraph::ThreadSuccessor()
/// - Occasionally, two port nodes on the boundary of the arc end nodes visualizations.
///
/// Depending on the applied layout model, these nodes are either implicit or
/// represented by a layout node index and respective coordinates:
/// - In the most simple case, the straight line drawing of simple graphs,
///   no control points exist. The arc label anchor points and the port nodes are
///   implicit and determined by the display of graph nodes.
/// - For all available orthogonal drawing methods, the number of control points is
///   3 or less. In high-degree models, port nodes are explicit, and are implicit
///   in models which admit a maximum node degree of 4 or less.
///
/// Provided that the number of control points is generally bounded, use
/// \ref sparseRepresentation::GetArcControlPoints() to write the control point sequence of a
/// specified arc to aa array, including the port nodes. The result depends on the
/// arc orientation bit. If the number of control points is not known in advance, the
/// sequence of control points of any forward arc a (an even arc index!) is obtained
/// by this recurrency rule:
///
/// <code>
/// controlPoint[0] = PortNode(a);
///
/// for (i=0;controlPoint[i]!=NoNode;++i) {
///    controlPoint[i+1] = ThreadSuccessor(controlPoint[i]);
/// }
/// </code>
///
/// If the layout model uses explicit port nodes, those are the extremal
/// points in the control point sequence, namely <code>controlPoint[0]</code> and
/// <code>controlPoint[i-1]</code>.
///
/// Manipulation of arc routings is somewhat tricky: Currently, control points
/// cannot be assigned independently from arc label anchor points. Use the most
/// high-level operation \ref sparseRepresentation::InsertArcControlPoint() which still
/// has some side effects: It will implicitly allocate and position an arc label
/// anchor point if it does not exist in advance.
///
/// \ref groupArcRouting "[See API]"
///
/// <br>
///
///
/// \section secLayoutBounding  Bounding boxes
///
/// Bounding boxes determine the extra space between the display boundaries
/// and the graph nodes (layout points are not considered). The bounding box
/// is retrieved component-wise by calling
/// \ref abstractMixedGraph::Layout_GetBoundingInterval().
///
/// By default, bounding intervals are the extremal coordinate values augmented
/// by the node spacing parameter value, and thus computed dynamically. The
/// drawback of this rule is, that the bounding intervals can change dynamically
/// with the graph nodes. In particular, for proper subgraphs, different intervals
/// result.
///
/// To fix up such behavior, sparse graph objects can maintain two layout nodes
/// which denote the upper-left and lower-right corner of the bounding box. The
/// coordinates refer to the same metric space as the graph node coordinates.
///
/// If the current bounding box is as desired, and should stay unchanged with
/// subsequent modifications of the graph or its drawing, just call
/// \ref abstractMixedGraph::Layout_FreezeBoundingBox(). There is also a method
/// \ref abstractMixedGraph::Layout_ReleaseBoundingBox() to revert to the dynamic
/// bouding box computation.
///
/// All layout codes and most graph constructors (in particular, the copy
/// constructors) set up a fixed bounding box.
///
/// In case of circular or radial drawings, the bounding box is the tight square
/// around the bounding sphere.
///
/// \ref groupLayoutBounding "[See API]"
///
/// <br>
///
///
/// \section secPlanarDrawing  Planar straight-line drawing
///
/// Basically, planar drawing means to create a drawing of a planar graph
/// without any edge crossings. In fact, all planar drawing methods depend on a
/// particular planar representation, that is a clockwise ordering of incidence
/// lists according to a virtual plane drawing. Before drawing a planar graph,
/// it is necessary to determine such a planar representation! If no exterior
/// arc has been specified in advance, an exterior face with a maximum number of
/// nodes is set up.
///
/// The library comes up with a method for convex drawing of triconnected planar
/// graphs. This method has been be extended to arbtrirary planar graphs by making
/// the input graph triconnected, then applying the convex drawing method and
/// mapping back the node placement to the original graph. The final drawing
/// probably has non-convex faces even if the input graph is triconnected, since
/// graphs are actually triangulated. It is recommended either to apply the convex
/// method directly or to use the restricted
/// \ref secForceDirected "force directed placement (FDP) method" for post-precessing.
///
/// The following is a convex drawing of the dodecahedron. The good resolution
/// of area and angles stems from the regularity of the input graph:
/// \image html  dodekahedronConvex.gif
/// \image latex dodekahedronConvex.eps
///
/// \ref groupPlanarDrawing "[See API]"
///
/// <br>
///
///
/// \section secCircularDrawing  Circular drawing and outerplanarity
///
/// The basic method of placing all nodes on a circle only depends on a particular
/// clock-wise node order. This may be specified explicitly by the node colour
/// register (with arbitrary tie-breaking for nodes with equal colour indices).
/// It is also possible to use the predecessor arc labels, with the special intent
/// to expose Hamiltonian cycles. In the general case, the method backtracks on
/// predecessor arcs as long as possible and places the traversed nodes, and then
/// selects a further unmapped node for backtracking.
///
/// There is another specialization of the circular method which travels around
/// the exterior face of a planar graph. It is restricted to the class of
/// \em outerplanar graphs. This denotes all graphs which have a plane drawing
/// with all nodes on the exterior face. The outerplanar \ref secPlanarRepresentation
/// representation must be given in advance.
///
/// The next two figures show the same outerplanar graph, starting with the single
/// circular drawing:
/// \image html  outerplanar.gif
/// \image latex outerplanar.eps
///
/// Some biconnected outerplanar graphs also admit an \em equilateral drawing.
/// In that drawing, all nodes of a single interior face form a regular polyhedron.
/// Observe that all equilateral drawings of a graph are congruent. A plane drawing
/// results only in special cases. The original intent was to obtain paper models
/// of Platonian and Archimedian polyhedra. For this purpose, there is another
/// method to cut triconnected planar graphs along the edges of some spanning tree.
/// What follows, is an example of such a paper model, obtained for the octahedron:
/// \image html  equilateral.gif
/// \image latex equilateral.eps
///
/// \ref groupLayoutCircular "[See API]"
///
/// <br>
///
///
/// \section secForceDirected  Force directed layout
///
/// Force directed drawing is a post-opimization strategy for straight line
/// drawings. It models attracting and repelling forces between nodes and,
/// occasionally, between node-edge pairs. One iterates for a node placement
/// which brings all forces into equilibriance. The classical spring embedder
/// moves all nodes simultaneously, but the much faster GEM code moves only
/// one node at a time. There is a version which restricts the node movements
/// such that nodes do not cross any edges. The application to plane drawings
/// is obvious, but the method can be used for general graphs, in order to
/// maintain a so-called mental map.
///
/// \ref groupForceDirected "[See API]"
///
/// <br>
///
///
/// \section secOrthogonalDrawing  Orthogonal drawing algorithms
///
/// An orthogonal drawing is a drawing where all graph nodes are displayed by
/// rectangles and all arcs are displayed by polygones running in a rectangular
/// grid. The display points at which an arc is attached to its end nodes, are
/// called port nodes. For sake of readability, overlapping nodes and crossings
/// of edges with non-incident nodes are excluded.
///
/// There are various specializations of this general orthogonal grid drawing
/// model. Most refer to the output of particular drawing algorithms, and usually
/// restrict the input graph somehow (planarity, connectivity, loops, parallels,..):
/// - The small node model is limited to graphs where node degrees are at most 4.
///   In this model, no explicit port nodes are handled. There are specialized
///   methods for planar graphs and for binary trees, but also a method for the
///   non-planar setting.
/// - The edge display can be restricted to a certain number of bends. No more
///   than three bends per edge are used in the library methods.
/// - In the 1-visibility representation model, edges are bend-free and running
///   only in vertical directions. The nodes only have horizontal extent. Thus
///   this model is naturally restricted to planar graphs.
/// - In the Kandinsky model, two grids are used. Nodes are displayed as squares
///   of equal size in a coarse grid which is specified by the nodeSpacing parameter.
///   When post optimization is turned off, every edge has exactly one control point.
///   There are methods for the general and for the planar setting, for trees and
///   for redrawing a graph, in order to maintain the so-called <em>mental map</em>.
///
/// The following is a small node drawing of a dodekahedron. The library does
/// not provide an exact method for bend minimization, but it is easy to see that
/// this particular drawing is bend-minimal:
/// \image html  dodekahedron.gif
/// \image latex dodekahedron.eps
///
/// This figure has not been generated automatically, but by using the interactive
/// Kandinski method. Although the procedure does not adapt for plane or small-node
/// drawings, it is also helpful for post-processing such drawings!
///
///
/// \ref groupOrthogonalDrawing "[See API]"
///
/// <br>
///
///
/// \subsection secOrthogonalPlanar  Planar case
///
/// The library admits three strategies to obtain plane orthogonal drawings,
/// quite different to orchestrate:
/// - Visibility representation: For a given biconnected planar (represented)
///   graph, bipolar orientations of the input graph and its dual graph are
///   computed. The node coordinates are determined by the distance labels in
///   these digraphs. If the input graph is not biconnected, it is augmented.
///   There is an optional post-processing mode, reducing the node sizes to the
///   minimum of what is possible without re-routing the arcs. For graphs where
///   the node degree is bounded by 3, a small node drawing can be obtained.
/// - 4-orthogonal drawings: The method also applies to non-planar graphs. It
///   checks for the existence of a planar representation and occasionally adapts
///   itself to the planar setting. However, the input graph must be biconnected
///   (Augmentation does not make sense here!)
/// - Kandinski drawings: The so-called <em>staircase method</em> starts with an
///   exterior edge and inserts the other nodes step by step, according to a
///   <em>canonical ordering</em> which is available for triconnected planar
///   graphs only. In each intermediate step, the overall shape is a half-square
///   triangle, and node are inserted on the diagonal side. There is an interface
///   which extends the method to non-triconnected graphs by triangulation of
///   the input graph.
///
/// The following figure has been obtained from applying the staircase method
/// to the dodecahedron without any post-proceesing. In this example, the post
/// optimization code would reduce the area by a factor of 10.3 (to the same
/// area as in the previous drawing), and the number of bends by a factor
/// of 5.0 (two more bends than in the optimal solution above):
/// \image html  dodekaStaircase.gif
/// \image latex dodekaStaircase.eps
///
/// An example of a visibility representation can be found with the description
/// of \ref secSeriesParallel "series-parallel graphs".
///
/// \ref groupOrthogonalPlanar "[See API]"
///
/// <br>
///
///
/// \subsection secOrthogonalTree  Orthogonal tree drawing
///
/// An HV-drawing of a binary tree is a plane straight line drawing such that
/// the child nodes of any node are placed right-hand or below of the parent
/// node. It is a rather small class of graph to which the method can be applied.
/// But the results are appealing for that symmetries are exposed, as in the
/// following drawing of a depth 4 regular binary tree:
/// \image html  binTree.gif
/// \image latex binTree.eps
///
/// Trees with high degree nodes can be drawn in the 1-bent Kandinski model.
/// As in the binary case, parent nodes appear in the upper left corner of the
/// spanned subtree bounding box. Other than for HV-trees, child nodes are always
/// displayed below of the parent nodes.
///
/// \ref groupOrthogonalTree "[See API]"
///
/// <br>
///
///
/// \subsection secOrthogonalRefinement  Post-Optimization Methods
///
/// Orthogonal drawings which have been generated from scratch are far from optimal,
/// in terms of the required area and of the total edge span. To this end, all layout
/// methods call some post-optimization procedure, and there is a control parameter
/// \ref goblinController::methOrthoRefine which determines the effect of all these
/// post-optimization procedures. Several optimization levels are provided:
/// - ORTHO_REFINE_NONE : Do not apply any post-optimization code
/// - ORTHO_REFINE_PRESERVE : Apply a stripe dissection model to reduce the area,
///   but preserve all bend nodes
/// - ORTHO_REFINE_FLOW : Apply a stripe dissection model to reduce the area
/// - ORTHO_REFINE_SWEEP : Apply a sweep-line method for further reduction of the
///   number of bend nodes
/// - ORTHO_REFINE_MOVE : Apply a moving-line model for the most exhaustive reduction
///   of the number of bend nodes
///
/// Assigning ORTHO_REFINE_FLOW will most likely halve the drawing area at a
/// moderate computational expense. Assigning ORTHO_REFINE_SWEEP, one might still
/// expect a small area reduction. At the level ORTHO_REFINE_MOVE, only small edge
/// span reductions should be expected.
///
/// A full post-optimization scheme #abstractMixedGraph::Layout_OrthoCompaction()
/// is provided for the small node model. This is automatically applied after any
/// drawing from scratch. It also serves as a standalone tool, as long as the input
/// drawing comnforms with the layout model.
///
/// For the Kandinsky model, there is a sweep-line method encoded into the
/// #abstractMixedGraph::Layout_Kandinsky() interface method. There is
///
///
///
/// <br>
///
///
/// \section secLayeredDrawing  Layered Drawing
///
/// In a layered drawing, the node set is partitioned into independent sets
/// (the layers), each layer is assigned to a horizontal grid line,
/// and control points are placed wherever edges would cross grid lines in a
/// straight line drawing. This first phase of the strategy is called vertical
/// placement. Then in the horizontal placement phase, the nodes are arranged
/// within their layers. Hereby, original graph nodes and control points are treated
/// almost the same.
///
/// The optimization goals in the vertical placement step are the aspect ratio
/// and the total edge span (this denotes the number of required control points).
/// If the graph is directed or mixed, it is also intended to orient the arcs
/// top-down. The goals in the horizontal placement are to prevent from edge
/// crossings and to maximize the slopes of the edge segments.
///
/// Although it is a couple of more or less independent tools, the layered
/// drawing code is called through a single interface function. If LAYERED_DEFAULT
/// is specified, the graph is drawn from scratch. One should not expect plane
/// or even upward-plane drawings for planar graphs. The main reason is that the
/// crossing minimization code does not question the layer assignment done in
/// advance.
///
/// Frankly, the rules which code is applied under which conditions are complicated:
/// - By specifying LAYERED_FEEDBACK, the the top-down arc orientations are
///   explicitly computed (In case of directed acyclic graphs, all arcs will point
///   downwards). If this option is not specified, the orientations must be either
///   given in advance, are implicated by an existing layer assignment or by the
///   given drawing (depending on the other rules for vertical placement).
/// - When specifying LAYERED_EDGE_SPAN, nodes are assigned to layers by applying
///   the exact edge span minimization code.
/// - When specifying LAYERED_COLOURS, all nodes with finite colour index are
///   forced to the respective layer. If the layering is not complete, the edge
///   span minimization code method is applied.
/// - In any case, arcs spanning more than one layer, are subdivided by control points.
///   This completes the vertic placement phase.
/// - By specifying LAYERED_SWEEP, the crossing minimization code is applied.
///   This performs local optimization between two adjacent layers, sweeping
///   top-down and bottom-up again, until no further improvement is possible.
///   Crossing minimization is the performance critical part of the layered
///   drawing method. After that step, the relative order of nodes in each layer
///   is fixed.
/// - When specifying LAYERED_FDP, a one dimensional force directed method does
///   the horizontal node alignment. The node moves are either unlimited or
///   preserve the relative node order set by the crossing minimization code.
/// - By specifying LAYERED_ALIGN, an LP model is applied to maximize the slopes
///   of the edge segments.
///
/// In order to refine a given plane drawing, one should specify LAYERED_EDGE_SPAN
/// and a method for horizontal placement, but skip the crossing minimization step.
/// If no implicit top-down orientation has been computed in advance and
/// delivered by the edge colours, the orientation is chosen as for the current
/// drawing. It is not required that the previous drawing is in the layered model,
/// but on the other hand, nodes might move vertically.
///
/// The following shows an edge span minimum upward drawing of upward planar
/// digraph. This has been obtained in the default mode. In order to obtain a
/// plane drawing, one node must be shifted manually to the next layer. But even
/// then, the crossing minimization code would not clean up all edge crossings
/// automatically:
/// \image html  unix.gif
/// \image latex unix.eps
///
/// \ref groupLayeredDrawing "[See API]"
///
/// <br>
///
///
/// \ref index "Return to main page"


/// \defgroup groupGraphComposition  Graph composition
/// \defgroup groupGraphSeries  Graph series
/// \ingroup groupGraphComposition
/// \defgroup groupGraphDerivation  Graph derivation
/// \ingroup groupGraphComposition
/// \defgroup groupPlanarComposition  Planar composition
/// \ingroup groupGraphComposition
/// \defgroup groupDagComposition  DAG composition
/// \ingroup groupGraphComposition
/// \ingroup groupDirectedAcyclic
/// \defgroup groupMergeGraphs  Merging graphs
/// \ingroup groupGraphComposition
/// \defgroup groupDistanceGraphs  Graph composition by node distances
/// \ingroup groupGraphComposition
/// \defgroup groupRandomManipulation  Random manipulations
/// \ingroup groupGraphComposition
/// \defgroup groupMapIndices  Mapping node and arc indices back
/// \ingroup groupGraphComposition

/// \page pageGraphComposition  Graph composition
///
/// \section secGraphSeries  Composition from scratch
///
/// The library comes with several constructor methods which grow graph objects
/// from one or more numeric input parameters:
/// - \ref regularTree : Rooted trees where all non-leaf nodes have a specified
///   degree. Either with a specified depth or a specified number of nodes
/// - \ref sierpinskiTriangle : Recursive construction of a series of planar,
///   nearly 4-regular graphs
/// - \ref openGrid : Tessilations of the Euclidan plane with regular faces
///   (either triangles, squares or hexagons), truncated to a specified grid size
/// - \ref polarGrid : Radial grids with 0, 1 or two poles and triangular or
///   quadrilateral faces. Useful for the construction of regular polyhedra
///   (see Section \ref secPlanarComposition )
/// - \ref toroidalGrid : Similar to the \ref openGrid construction. Wrap-around
///   in replace of truncation, to preserve the node regularity. These graphs
///   are non-planar, but can be embedded on a torus surface
/// - \ref moebiusLadder : A series of non-planar graphs which can be embedded
///   on a projective plane
/// - \ref gridCompletion : Similar to the \ref openGrid construction, but here
///   all pairs of nodes on a grid line are adjacent. The input parameter denotes
///   the maximum clique sizes
/// - \ref triangularGraph : Graphs on the 2-element subsets of a ground set with
///   specified cardinality. Non-planar, node regular
/// - \ref permutationGraph : Graphs on a given node set, and an optional permutation.
///   Special case of comparabilty graphs and, by that, of perfect graphs
/// - \ref thresholdGraph : Graphs on a given node set, a threshold value for edge
///   weights, and optional node weights. Special case of chordal and, by that,
///   perfect graphs
/// - \ref intervalGraph : Intersection graphs on a given node set, and optional
///   node intervals. Special case of chordal and, by that, perfect graphs
/// - \ref mycielskianGraph : A series of graphs with growing chromatic number, but
///   constant clique number 2
/// - \ref butterflyGraph : A series of layered graphs
/// - \ref cyclicButterfly : A series of regular digraphs with a cyclic layering
///
/// These classes apply as test cases for optimization and drawing methods, in
/// particular when used as a seed for the composition principles which follow,
/// and the randomization procedures described in Section \ref secRandomManipulation.
///
/// Except for the triangular and the Mycielskian graphs (which do not admit
/// readable drawings), all constructor methods provide the expectable geometric
/// embeddings.
///
/// \ref groupGraphSeries "[See API]"
///
/// <br>
///
///
/// \section secGraphDerivation  Graph derivation
///
/// What follows is a list of well-known principles to construct graph objects
/// from other graphs:
/// - \ref complementaryGraph : A node pair is adjacent if it is non-adjacent in
///   the original graph
/// - \ref lineGraph : Defined on the edge set of the original graph. Edged are
///   adjacent if sharing an end point
/// - \ref colourContraction : Identify all equally coloured nodes
/// - \ref explicitSurfaceGraph : Identify nodes in the same toplevel set
///   of a specified \ref nestedFamily
/// - \ref completeOrientation : Split each undirected edge of the original graph
///   into a paair of anti-paralllel arcs
/// - \ref inducedOrientation : Orient the edges such that colour indices are
///   increasing from arc tails to head nodes
/// - \ref nodeSplitting : Break each node into a pair of nodes, joined by an
///   arc which has the original node demand as its upper capacity bound
/// - \ref inducedSubgraph : The subgraph induced by a node index set and,
///   optionally, an edge index set
/// - \ref inducedBigraph : The bipartite subgraph induced by two node index sets
///
/// Other than the methods discussed further on, the constructors listed here
/// work for general graph objects.
///
/// \ref groupGraphDerivation "[See API]"
///
/// <br>
///
///
/// \section secPlanarComposition  Planar composition
///
/// The library provides several methods to generate planar graphs from other
/// planar graphs. It is required that the original graph comes with a planar
/// representation, and the new graph is generated with a respective planar
/// representation. The composition methods generalize from construction
/// principles for regular polyhedra.
///
/// Most important is the \ref dualGraph construction. This takes the original
/// regions as the node set, connecting nodes if the original faces share
/// an edge. Actually, the edges are mapped one-to-one. The \ref directedDual
/// constructor works similar but assigns arc orientations acoording to the
/// orientation of the original arc.
///
/// Observe that the dual of the dual is basically the original graph, provided
/// that the graph is connected. In the directed case, the arc orientations are
/// reverted.
///
/// A potential drawing of the original graph is mapped, but the quality of
/// the resulting drawing is currently limited by the dual node representing
/// the exterior face of the original graph (A special treatment of infinite
/// nodes is missing). The following shows an outer-planar graph and its dual
/// graph, with the exterior face displayed in the center:
/// \image html  primalDual.gif
/// \image latex primalDual.eps
///
/// Other constructors (\ref vertexTruncation and \ref planarLineGraph) replace the
/// original nodes by faces. The constructor \ref facetSeparation maintains the
/// original faces and duplicates the original nodes for every adjacent face.
/// Observe that all constructors yield node-regular graphs:
/// - In \ref vertexTruncation, every node has degree 3
/// - In \ref planarLineGraph, every node has degree 4
/// - In \ref facetSeparation with the facetSeparation::ROT_NONE option,
///   every node has degree 4
/// - In \ref facetSeparation with the facetSeparation::ROT_LEFT or the
///   facetSeparation::ROT_RIGHT option, every node has degree 5
///
/// In order to obtain the full set of Platonian and Archimedian polyhedra, the
/// constructor method \ref polarGrid comes into play which is very flexible:
/// - The tetrahedron is a polar grid with 1 rows, 3 columns and 1 pole
/// - The octahedron is a polar grid with 1 rows, 4 columns and 2 poles
/// - The icosahedron is a polar grid with 2 rows, 4 columns, triangular faces and 2 poles
/// - The dodekahedron is the dual of an icosahedron
/// - Prisms are polar grids with 2 rows, square faces and no poles (and the
///   cube the prism with 4 columns)
/// - Antiprisms are polar grids with 2 rows, triangular faces and no poles
///
/// The following shows a polar grid with 4 rows, 11 columns and a single pole
/// (to obtain this drawing, polarGrid::POLAR_CONE must be specified):
/// \image html  polarGrid.gif
/// \image latex polarGrid.eps
///
/// \ref groupPlanarComposition "[See API]"
///
/// <br>
///
///
/// \section secDagComposition  DAG composition
///
/// In a directed graph, the arc set can be considered a relation of the graph
/// nodes. According to that, the \ref transitiveClosure "transitive closure"
/// of a digraph is the digraph with the same node set and with arcs uv if
/// there is a directed uv-path in the original graph. If the original digraph
/// is acyclic, the transitive closure defines a partially ordered set.
///
/// Of more practical importance is the reverse construction principle: An arc
/// uv is called transitive if there is a directed uv-path with intermediate
/// nodes. The \ref intransitiveReduction constructor omits all transitive
/// arcs of the original digraph. Observe that this subgraph is well-defined
/// only if the original digraph is acyclic and simple. The so-called <em> Hasse
/// diagram </em> of a DAG is obtained by taking the intransitive reduction and
/// computing a layered drawing of it.
///
/// The following shows a directd acyclic graph, with the red arcs forming its
/// intransitive reduction:
/// \image html  intransitiveReduction.gif
/// \image latex intransitiveReduction.eps
///
/// \ref groupDagComposition "[See API]"
///
/// <br>
///
///
/// \section secMergeGraphs  Merging graphs
///
/// This addresses the methods which derive graph objects from two given input
/// graphs. Actually, no new object is instanciated, but one graph is modified
/// by means of the other. Two methods are available:
/// - \ref abstractMixedGraph::AddGraphByNodes() is for node disjoint merge.
///   The earlier figure - showing a primal-dual pair - has been built by first
///   constructing the dual graph and then using this disjoint merge method.
/// - \ref abstractMixedGraph::FacetIdentification() requires a planar target graph,
///   and another graph with a simple eterior orbit. The latter is inserted into
///   the target graph, into each face with a matching cardinality.
///
/// The facet identification method can be used to construct star solids from
/// convex solids, and \em tilings (subdivisions of 2D grid graphs) as provided
/// in earlier GOBLIN releases. This also includes the possibily of constructing
/// non-planar graphs on a planar skeleton.
///
/// \ref groupMergeGraphs "[See API]"
///
/// <br>
///
///
/// \section secDistanceGraphs  Graph composition by node distances
///
/// Two constructor methods are provided which result in dense graph objects,
/// whose edge length labels encode the node distances of the input graph:
/// - \ref distanceGraph, which is defined for general input graphs,
///   essentially a square distance matrix.
/// - \ref metricGraph, which is defined for undirected input graphs only,
///   essentially a triangular distance matrix.
///
/// There is also a sparse concept, \ref voronoiDiagram, which takes a set of
/// terminal nodes of the input graph, assigning the non-terminal nodes to the
/// region of a nearest terminal. The Voronoi regions are the nodes of the new
/// graph, and the edge lengths are the distances of the respective terminals.
///
/// \ref groupDistanceGraphs "[See API]"
///
/// <br>
///
///
/// \section secRandomManipulation  Random manipulations
///
/// All graph objects with a sparse representation can be randomly augmented
/// to disturb the incidence structure. All methods augment by a specified
/// number of arcs, either without any restrictions, by a closed walk, or such
/// that no node exceeds a given degree bound.
///
/// \ref groupRandomManipulation "[See API]"
///
/// <br>
///
///
/// \section secMapIndices  Mapping node and arc indices back
///
/// After a graph object has been derived from another data object, and computations
/// have been done on the derived graph instance, it is often required to map back
/// the computational results to the original graph. For this goal, two methods
/// abstractMixedGraph::OriginalOfNode() and abstractMixedGraph::OriginalOfArc()
/// are provided.
///
/// There is no global rule of what the index returned by abstractMixedGraph::OriginalOfNode(v)
/// represents. It may be a node index, an arc index with or without the arc
/// direction bit or any other entity which can be addressed by an index.
///
/// Backtransformations are maintained only if abstractMixedGraph::OPT_MAPPINGS
/// is specified to the respective constructor method. Not all concrete classes
/// support this feature at the time!
///
/// \ref groupMapIndices "[See API]"
///
/// <br>
///
///
/// \ref index "Return to main page"


/// \defgroup groupSeriesParallel  Series-parallel graphs
/// \ingroup manipIncidences
/// \defgroup groupDirectedAcyclic  Directed acyclic graphs
/// \ingroup shortestPath
/// \defgroup groupPerfectGraphs  Perfect graphs

/// \page pageGraphRecognition  Graph recognition
///
/// \section secSeriesParallel   Series-parallel graphs
///
/// A series-parallel graph is a graph which can be obtained from a single edge
/// by recursive application of two production rules: (1) Replacing any edge by
/// parallel edges with the same end nodes as the original edge, and (2)
/// subdividing an edge. It is possible to extend this concept to digraphs and
/// mixed graphs.
///
/// Recognition of series-parallel (di)graphs means to revert the production
/// process and to represent it by a so-called decomposition tree. The tree is used
/// for the combinatorial embedding (ordering of the incidence list) and by the
/// drawing method.
///
/// It is pretty obvious that series-parallel graphs are planar, even upward-planar.
/// And embedding a series-parallel graph exposes plane incidence orders. When
/// series-parallel graphs are drawn, visibility representations result:
/// \image html  seriesParallelMixed.gif
/// \image latex seriesParallelMixed.eps
///
/// \ref groupSeriesParallel "[See API]"
///
/// <br>
///
///
/// \section secDirectedAcyclic  Directed acyclic graphs (DAGs)
///
/// A DAG is a digraph not containing directed cycles or, equivalently,
/// which has a <em> topologic ordering </em> (a node enumeration such that for
/// every eligible arc, the start node has a smaller number than the end node).
/// And by the second characterization, DAGs can be recognized in linear time.
///
/// The importance of DAGs is that minimum and maximum length paths can be also
/// computed in linear time, irrespective of the edge length signs: Predecessor
/// arcs and distance labels are just assigned in the topologic node order.
///
///
/// \ref groupDirectedAcyclic "[See API]"
///
/// <br>
///
///
/// \section secChordal  Chordal graphs
///
/// A <em>chord</em> with respect to to a cycle is an arc connecting two non-adjacent
/// nodes of the cycle. A non-triangle cycle without any chords is called a <em>hole</em>.
/// A graph is <em>chordal</em> if it does not have any holes.
/// A graph is <em>co-chordal</em> if the complementary graph is chordal.
///
/// As a positive characterization, chordal graphs are the graphs which admit a
/// so-called <em>perfect elimination order</em>. The first node in this order
/// is <em>simplicial</em>, that is, its neighbors form a clique. After deleting
/// the minimal node from the graph, the same holds for the remaining graph, the
/// induced order and the follow-up minimal node.
///
/// Both chordal and co-chordal graph can be recognized in linear time by using a
/// technique called <em>lexicographic breadth first search</em>. The recognition
/// takes some further steps which also admit a linear time implementation, but the
/// actual code is O(m log(m)) due to a universal edge sort.
///
/// It is remarkable that the co-chordality check runs without explicit generation
/// of the complementary graph.
///
///
/// \ref groupPerfectGraphs "[See API]"
///
/// <br>
///
///
/// \ref index "Return to main page"


/// \defgroup branchAndBound  Branch & bound
/// \ingroup algorithms
/// \defgroup mixedInteger  (Mixed) integer programming
/// \ingroup branchAndBound


/// \defgroup groupObjectExport  Exporting objects to files
/// \defgroup groupObjectImport  Importing objects from file
/// \defgroup groupCanvasBuilder  Building canvasses
/// \defgroup groupTextDisplay  Text display
/// \defgroup groupTransscript  Output to the transscript channel
/// \defgroup groupMipReductions  Problem reductions to MIP

/// \page pageFileExport
///
///
/// \ref index "Return to main page"


/// \defgroup groupTimers  Timers
/// \defgroup groupLogging  Logging
/// \defgroup groupTraceObjects  Trace Objects
/// \defgroup groupOptimiztionBounds  Optimization bounds
/// \defgroup groupProgressCounting  Progress counting
/// \defgroup groupHeapMonitoring  Heap monitoring

/// \page pageCodeInstrumentation  Code instrumentation
///
/// A considerable portion of the library code is for diagnosis, investigative
/// and didactic purposes. This manual page discusses all of these features and -
/// since these are useful when running small instances in the GUI only - how to
/// enable or to disable it.
///
/// <br>
///
///
/// \section secCompileTimeComfiguration  Compile time configuration
///
/// The core library comes with a header file \ref configuration.h to control
/// the overhead of code instrumentation at compile time. By enabling a macro,
/// the respective functionality does not necessarily become active. There are
/// also runtime configuration variables, but these are effective only if the
/// code has been compiled as required.
///
/// <br>
///
///
/// \section secModules  Modules
///
/// An <em>optimization module</em> denotes a set of C++ methods, either forming
/// the implementation of an optimization algorithm or the public interface to
/// a series of alternative methods for the same optimization problem.
/// In every module entry function, a local \ref moduleGuard object is defined
/// to publish the module information to the user interface.
///
/// By means of module guards, the progress of a computation can be measured
/// two ways: Either by measuring and estimating the computation times
/// (\ref secProgressCounting), or by keeping track of \ref secProblemBounds
/// "lower and upper bounds" on the objective value.
///
/// Module guards may run both nested and iterated. There is an implicit stack
/// of module executions, and this has some impact on the timer, bounding and
/// progress counting functionalities.
///
/// Once contructed, module guards become active. Basically, guard objects
/// need not be deactivated explicitly - this is usually done by the destructor
/// running when the hosting method or code block is left. This saves a lot of
/// code lines for shutting down the instrumentation functionalities, and
/// is much more exception safe than explicit instrumentation code. In a
/// few situations, it is useful to call moduleGuard::Shutdown() manually.
///
/// All modules are listed in the module data base \ref listOfModules[], This
/// structure determines authors, the bibliography and a \ref goblinTimer object.
///
/// Introducing a new module to the library means to add an enum value to
/// \ref TModule and to extend the \ref listOfModules[] data base. In occasion,
/// the bibliography data base \ref listOfReferences[] and the author's data base
/// \ref listOfAuthors[] also have to be extended. If the new module is not just
/// an alternative method for an existing solver, it might further be necessary
/// to extend the list of global timers \ref listOfTimers[].
///
/// <br>
///
///
/// \section secTimers  Timers
///
/// When the macro \ref _TIMERS_ is set, the library compiles with support for
/// runtime statistics. This particular functionality is available for UNIX like
/// platforms only, as it takes the POSIX headers <code>sys/times.h</code> and
/// <code>unistd.h</code>.
///
/// The \ref goblinTimer class computes and memorizes minimum, maximum, average,
/// previous and accumulated run times. If it is supplied with a global timer
/// list, it also keeps track of time measurements in nested function calls.
///
/// Timers can be started both nested and iterated:
/// - If a timer is started and stopped in a nested manner, only the top-level
///   instrumentation has the apparent effect. That is, one does not have to
///   take care of the calling context before starting a timer.
/// - If a timer is started and stopped in a iterated manner, the timer memorizes
///   both the time of the most recent \ref goblinTimer::Enable() /
///   \ref goblinTimer::Disable() sequence and the accumulated
///   running times since the most recent \ref goblinTimer::Reset() command
///
/// For many use cases, the existing global timers suffice, and those are
/// orchestrated automatically, namely by \ref moduleGuard objects. If the
/// \ref goblinController::logTimers flag is set, timer reports are written
/// whenever module guards are destructed.
///
/// \ref groupTimers "[See API]"
///
///
/// <br>
///
///
/// \section secLogging  Filtering the logging output
///
/// Most high-level methods generate some text output, either for debugging or
/// exemplification. The logging information is classified inline by one of the
/// following enum values:
/// - \ref LOG_METH and \ref LOG_METH2 : Course of algorithms
/// - \ref LOG_RES and \ref LOG_RES2 : Computational results
/// - \ref LOG_MEM : Object instanciations
/// - \ref LOG_MAN : Object manipulations
/// - \ref LOG_IO : File I/O operations
///
/// Up to the classes \ref LOG_RES2 or \ref LOG_METH2, the amount of output should
/// not depend on the instance sizes. Code for the latter classes is only
/// conditionally compiled, depending on the macro \ref _LOGGING_.
///
/// \verbatim
/// THandle LH = LogStart(LOG_METH2,"Adjacent nodes: ");
/// TArc a = First(u);
///
/// do
/// {
///    sprintf(CT.logBuffer,"%ld ", EndNode(a));
///    LogAppend(LH,CT.logBuffer);
///    a = Right(a,u);
/// }
/// while (a!=First(u));
///
/// LogEnd(LH);
/// \endverbatim
///
/// There are runtime variables to filter the effective output:
/// - \ref goblinController::logMeth to control the output associated with \ref LOG_METH and \ref LOG_METH2
/// - \ref goblinController::logRes to control the output associated with \ref LOG_RES and \ref LOG_RES2
/// - \ref goblinController::logMem to control the output associated with \ref LOG_MEM
/// - \ref goblinController::logMan to control the output associated with \ref LOG_MAN
/// - \ref goblinController::logIO to control the output associated with \ref LOG_IO
/// - \ref goblinController::logTimers to control the output of method running times
/// - \ref goblinController::logGaps to control the output of objective value and duality gaps
/// - \ref goblinController::logWarn to control the output of warning messages
///
/// Feasible values are 0 and 1, up to \ref goblinController::logMeth and
/// \ref goblinController::logRes where a value of 2 denotes more detailed output.
///
/// Before routing to file, the \ref goblinController attaches the module ID and
/// the module nesting / indentation level.
///
/// \ref groupLogging "[See API]"
///
/// <br>
///
///
/// \section secTracing  Tracing of method executions
///
/// <em> Tracing </em> a computation means to save certain intermediate object
/// states to file. It is possible to use trace points as break points. In that
/// case, the trace file is written by the method exectution thread, re-read by
/// the user interface, and the method execution thread waits for a feedback
/// from the GUI.
///
/// When tracing is activated, this produces considerable computational overhead -
/// at least for writing the trace files. Sometimes the trace objects are not
/// generated in regular computations explicitly (e.g. branch trees in the branch &
/// bound method). That is, the overhead may also include generation and layout
/// of data objects.
///
/// The following trace levels are implemented:
/// - All trace points are disabled (specify \ref goblinController::traceLevel = 0)
/// - Trace points in object constructors and destructors are enabled
///   (specify \ref goblinController::traceLevel = 1)
/// - All trace points are enabled (specify \ref goblinController::traceLevel = 2)
///
/// A trace point may either be conditional - when \ref goblinController::Trace()
/// is called - or unconditional -when \ref managedObject::Display() is called.
///
/// \ref groupTraceObjects "[See API]"
///
/// <br>
///
///
/// \section secProblemBounds  Upper and lower optimization bounds
///
/// When a module guard is generated, the lower and upper bound are initializes
/// to <code>+/-InfFloat</code> and can be improved by using \ref moduleGuard::SetBounds(),
/// \ref moduleGuard::SetLowerBound() and \ref moduleGuard::SetUpperBound(). When
/// one of these methods is called, and if \ref goblinController::logGaps is set,
/// every improvement of bounds is logged. It is checked by each of these methods
/// that bounds really improve, and that the lower bound does not overrun the upper
/// bound.
///
/// By using the \ref moduleGuard::SYNC_BOUNDS specifier, the bounds of a module
/// guard are identified with the respective bounds in the parenting module guard.
/// This makes it possible to share lower and upper bounding code among different
/// modules, and is especially useful when one method determines potential solutions
/// and another method determines dual bounds for the same optimization problem.
/// This synchronization only works if the parent module refers to the same timer!
///
/// \ref groupOptimiztionBounds "[See API]"
///
/// <br>
///
///
/// \section secProgressCounting  Progress counting
///
/// This features is enabled by the macro \ref _PROGRESS_. Other than timers which
/// return absolute values, \ref goblinController::ProgressCounter() returns the
/// estimated fraction of executed and total computation steps. The feature is
/// useful either in a multi-threaded application or if a special trace event handler
/// has been registered to report about the computational progress.
///
/// In the most simple use case, an algorithm cyclically publishs this fraction by
/// calling \ref moduleGuard::SetProgressCounter(). Things are somewhat more
/// complicated if there is a predefined number of computation steps, and on nested
/// module calls:
///
/// Every module guard stores three values for the progress counting functionality:
/// - The maximum progress value. This can be either a number of computation steps
///   (set by calling \ref moduleGuard::InitProgressCounter() ) or the relative value
///   <code>1.0</code> (the default value after the module guard construction).
/// - The current progress value. This is iniitialized to <code>0.0</code> by the module
///   guard constructor and \ref moduleGuard::InitProgressCounter() ), and updated by
///   calling \ref moduleGuard::SetProgressCounter() or \ref moduleGuard::ProgressStep().
/// - A forecast value for the improvement in the next computation step. This is <code>1.0</code>
///   by default, can can be set by calling \ref moduleGuard::InitProgressCounter().
///
/// When calling \ref moduleGuard::ProgressStep() without a value, it realizes the
/// forecasted improvement, and the latter value is maintained for subsequent steps.
///
/// The forecasting feature becomes important on nested module calls: Evaluating
/// a global progress value means to scan up the stack of module executions, ending
/// at the top-level module. At every level, the fractional progress value is computed
/// and multiplied with the forecasted improvement in the parent module. Then, the
/// current progress of the parent module is added and one tracks back in the module
/// execution stack.
///
/// In one extremal case, none of the modules on the execution stack has been
/// instrumentated explicity - up to the least one. Then the fractional progress
/// value of this module is raised to the top level (as both the forecasted and
/// the maximum progress value are <code>1.0</code> in the ancestor modules.
///
/// In the case when a model sets fractional progress values by using
/// \ref moduleGuard::SetProgressCounter(), this should be preceded by a call
/// <code>InitProgressCounter(1.0,0.0)</code> so that nested module cannot influence
/// the global progress value.
///
/// \ref groupProgressCounting "[See API]"
///
/// <br>
///
///
/// \section secHeapMonitoring  Heap Monitoring
///
/// This feature is enabled by the macro \ref _HEAP_MON_. Then, <code>new</code>
/// and <code>delete</code> are reimplemented such that applications can keep
/// track of the memory usage, namely by global variables \ref goblinHeapSize,
/// \ref goblinMaxSize, \ref goblinNFragments, \ref goblinNAllocs and \ref goblinNObjects.
///
/// If the feature is enabled, the size of memory fragments increases by one
/// pointer. If it is disabled, the global variable still exist, but all with
/// zero values. Needless to mention the impact of this feature on the running times.
///
/// \ref groupHeapMonitoring "[See API]"
///
///
/// <br>
///
/// \ref pageManualContents "[See manual page index]"


/// \page pageInstallation  Installation guidelines
///
/// \section secDistribution  Distribution packages
///
/// Source code distribution packages are provided roughly once per month at
/// <a href="http://sourceforge.net" target="_parent">Sourceforge</a>. Those packages fall into
/// four parts:
/// - The core C++ class library. This ought to compile in any environment
///   with any contemporary C++ compiler. The <code>Makefile</code> and
///   <code>Makefile.conf</code> assume a GNU make tool, and
///   <code>Makefile.conf</code> works out of the box in the linux/gcc
///   environment only. For the Windows/cygwin environment, it should
///   suffice to set <code>os = cygwin</code> in <code>Makefile.conf</code>.
/// - A wrapper of the most library methods for the Tcl scripting language.
///   A successful build ends up with a <code>gosh</code> shell tool.
///   This requires that Tcl/Tk header files can be located by the compiler.
///   If shared object (<code>*.so</code> or <code>*.dll</code>) build is
///   supported, those objects include the Tcl initialization procedures.
/// - The graphical user interface. Provided that the Tcl/Tk system installation
///   is working properly, and the shell tool has been built, the GUI should
///   start without errors. Some \ref secKnownRuntimeProblems "runtime errors"
///   might occur due to inappropriate Tcl installations of missing graphical
///   filtering tools.
/// - GLPK source code, adapted to the GOBLIN linear programming interface.
///   The included GLPK source code is not updated regularly. The build
///   should run without problems.
///
/// Binaries are not distributed regularly. An MS Windows setup will be provided
/// for every major build (2.7, 2.8, ...), roughly once every year. The latter
/// is a condensed cygwin runtime environment including several packages required
/// for GUI output to bitmap and vectorial file formats. The Windows setup is
/// intended for non-experienced end users and for quick validation. To advanced
/// end users and developers, a regular \ref secCygwin "cygwin installation" is
/// recommended, since the cygwin condensation step does not take care of package
/// management.
///
/// <br>
///
///
/// \section secUnpacking  Unpacking the source code
///
/// Supposed, you want to install from a source code distribution package (say
/// Release 2.8) on a UNIX machine, the first step to build the library, the
/// shell and/or problem solvers is to unpack the <code>goblin.2.8.tgz</code>
/// archive by typing
///
/// <code>tar xfz goblin.2.8.tgz</code>
///
/// This will extract to a <code>goblin.2.8</code> directory right below the
/// current working directory. If you also want to install the test code
/// package (say <code>goblin.2.8.test.tgz</code>, do it from the same working
/// directory.
///
/// <br>
///
///
/// \section secBuildByMake  Build from source code distributions in UNIX like environments
///
/// Change to the extracted <code>goblin.2.8</code> root directory. If necessary,
/// edit the <code>Makefile.conf</code> file (and changes are indeed necessary for
/// platforms other than linux/gcc). Build the binaries by typing
///
/// <code>make private</code>
///
/// This produces several libraries (copied to the the <code>lib</code>
/// subfolder) and the shell tool <code>gosh</code> (copied to the the
/// <code>bin</code> subfolder).
///
/// You may perform a basic test of your personal installation of the graph browser
/// by typing <code>./bin/goblet</code> (assuming that you are still in the
/// <code>Makefile</code> directory), and then loading one of the test problems
/// in the <code>./samples</code> folder.
///
/// If you get stuck with the shell tool build, but you only want to use the C++
/// API, type
///
/// <code>make goblin</code>
///
/// If you want to use a solver executable rather than the API, say the matching
/// solver, then
///
/// <code>make exe pr=optmatch</code>
///
/// creates the executable <code>optmatch</code>. The complete list of executable solvers
/// can be found in the folder <code>main_src</code>.
///
/// <br>
///
///
/// \section secBuildCore  Core library build without the shipped Makefile
///
/// The core library is platform independent code. To built it without the
/// shipped Makefile, just import all files from the <code>lib_src</code> and
/// <code>include</code> directories and edit the <code>configuration.h</code>
/// to get ridd off the code instrumentation utilized by the GUI. Details can
/// be found in Section \ref secCompileTimeComfiguration .
///
///
///
/// <br>
///
///
/// \section secBuildDocumentation  Building this reference manual
///
/// You are just reading some version of the doxygen reference manual. A more
/// recent html version might be online at the
/// <a href="http://goblin2.sourceforge.net" target="_parent">Sourceforge project homepage</a>
/// You can build the html reference from the source distribution root directory by typing
///
/// <code>make html</code>
///
/// In the output, the entry point for browsing the documentation is
/// <code>./html/index.html</code>.
///
/// There is also a latex manual which is build by the command
///
/// <code>make latex</code>
///
///
///
/// <br>
///
///
/// \section secCygwin  Preparing cygwin
///
/// Nothing is special with the GOBLIN build in the cygwin environment (just set
/// <code>os = linux</code> in <code>Makefile.conf</code>. In order to have a full
/// featured GUI, the following packages are required:
/// - <code>xfig</code> with <code>fig2dev</code> for processing vectorial
///   graphing formats
/// - <code>netpbm</code> for processing bitmaps
///
/// These packages are also used in the linux environment, but the cygwin setup
/// does not install them by default.
///
/// <br>
///
///
/// \section secKnownRuntimeProblems  Potential GUI runtime problems
///
/// 
///
/// - <b><code>init.tcl</code> cannot be found</b>: In occasion, this probably also
///   happens when one of the shell tools <code>tclsh</code>, <code>wish</code>
///   or <code>gosh</code> is started directly. The problem has been observed
///   with cygwin installations, and can be fixed by brute copying the file where
///   the tcl interpreter finds it.
/// - <b>Tcl/Tk does not support multiple threads</b>: This should be reported once
///   after installation (updates). If you disable the application of threads,
///   the GUI won't refresh the display as long as library functions are running.
///   In particular, one cannot interrupt solvers. If you enable the GUI thread
///   applications, it will sometimes crash. Unfortunately, Tcl/Tk is usually
///   shipped without the <code>tcl_platform(threaded)</code> option. One can
///   fix the problem either by re-compiling the Tcl library or by installing
///   a special distribution from the net (for example at
///   <a href="http://mkextensions.sourceforge.net" target="_parent">mK extensions project</a>
///   or the <a href="http://activestate.com/Products/activetcl/index.plex" target="_parent">Active Tcl</a>
///   distribution). Download works fine for linux, but in the cygwin case
///   the original problem is most likely replaced by the following bug:
/// - <b>The GUI is confused by a wrong HOME environement variable</b>: This happens
///   when a windows Tcl libraray is used instead of a cygwin Tcl library.
///   Resolve the problem by re-installation of the Tcl library.
/// - <b>Cannot export to bitmaps or vectorial formats other than <code>fig</code></b>:
///   Some graphing tool is missing. If vectorial formats such as <code>eps</code>
///   can be generated, but <code>gif</code> cannot, something is wrong with the
///   <a href="http://netpbm.sourceforge.net" target="_parent">netpbm</a> installation
///   (the package used for bitmap manipulation). Otherwise, the <code>fig2dev</code>
///   command line tool cannot be accessed. The latter tool is associated with the
///   <code>xfig</code> vectorial drawing program (sharing the input file format),
///   but not necessarily installed with <code>xfig</code>). Output to the
///   <code>fig</code> format is possible in any case.
///
/// <br>
///
///
/// \section secBuildTest  T
///
/// If you have observed runtime problems with your installation, and you are
/// willing to support this software project, you might download the unit test
/// code to the Makefile folder. Type
///
/// <code>tar xfz goblin.2.8.test.tgz</code>
///
/// from the Makefile folder. Then, start the GUI, select
/// <code>Browser -> Run unit tests...</code>, and confirm the proposed Tcl script.
/// Tests roughly run for one or two hours. For every passed test case, a single
/// line of shell output results, and an exclamation mark indicates an exception.
///
/// If the build numbers differ, the test suite might be outdated. In that case,
/// check the change log for relevant modifications of the Tcl programming interface.
/// In the worst case, the whole application crashes. This might indicating a local
/// installation problem, one of the \ref secKnownRuntimeProblems "known runtime problems".
/// In any other case, report negative test results.
///
/// <br>
///
///
/// \ref pageManualContents "[See manual page index]"


/// \page pageTclReference  Tcl programming interface
///
/// Right after shell start, the Tcl wrapper provides a single object command,
/// <code>goblin</code>. This library root object serves as as a factory of graph
/// objects and mixed integer programs. And generating a library object implies
/// generating the respective Tcl object command.
///
/// For the time being, this Tcl command reference is not complete but, at least,
/// covers all graph object commands.
/// Descriptions for the missing LP / MIP commands will be added after some necessary
/// interface cleanup steps.
///
/// <br>
///
///
/// \section secTclObjectInstanciation  Object instanciation
///
/// Objects are instanciated by sending one of the following messages to the
/// <code>goblin</code> root object:
/// - <code>mixedGraph</code>: Mixed graph, represented by incidence lists
///     - <code>-nodes \<number of graph nodes\></code> (mandatory)
/// - <code>sparseGraph</code>: Undirected graph, represented by incidence lists
///     - <code>-nodes \<number of graph nodes\></code> (mandatory)
/// - <code>sparseDiGraph</code>: Directed graph, represented by incidence lists
///     - <code>-nodes \<number of graph nodes\></code> (mandatory)
/// - <code>sparseBiGraph</code>: Bipartite graph, represented by incidence lists
///     - <code>-nodes \<number of graph nodes\></code> (mandatory)
/// - <code>denseGraph</code>: Undirected graph, represented by an adjacency matrix
///     - <code>-nodes \<number of graph nodes\></code> (mandatory)
/// - <code>denseDiGraph</code>: Directed graph, represented by an adjacency matrix
///     - <code>-nodes \<number of graph nodes\></code> (mandatory)
/// - <code>denseBiGraph</code>: Bipartite graph, represented by an adjacency matrix
///     - <code>-nodes \<number of graph nodes\></code> (mandatory)
/// - <code>openGrid</code>: Plane graph with regular interior faces
///     - <code>-rows \<number of horizontal grid lines\></code> (mandatory)
///     - <code>-columns \<Number of vertical grid lines\></code> (mandatory)
///     - <code>-square</code>: Square interior faces (default)
///     - <code>-triangular</code>: Triangular interior faces (optional)
///     - <code>-hexagonal</code>: Hexagonal interior faces (optional)
/// - <code>gridCompletion</code>: Grid graph with cliques of nodes on the same grid line
///     - <code>-rows \<number of horizontal grid lines\></code> (mandatory)
///     - <code>-columns \<Number of vertical grid lines\></code> (mandatory)
///     - <code>-square</code>: Square interior faces (default)
///     - <code>-triangular</code>: Triangular interior faces (optional)
///     - <code>-hexagonal</code>: Hexagonal interior faces (optional)
/// - <code>polarGrid</code>: Plane or spheric graph with radial grid lines
///     - <code>-rows \<number of horizontal grid lines</code> (mandatory)
///     - <code>-columns \<number of vertical grid lines</code> (mandatory)
///     - <code>-poles \<number of vertical grid lines</code> (0,1 or 2, default: 0)
///     - <code>-quadrilateral</code>: Quadrilateral faces (default)
///     - <code>-triangular</code>: Triangular faces (optional)
///     - <code>-disc</code>: 2D drawing on a disc (optional)
///     - <code>-sphere</code>: 3D drawing on a sphere (optional)
///     - <code>-hemisphere</code>: 3D drawing on a hemisphere (optional)
///     - <code>-tube</code>: 3D drawing on a cylinder (optional)
///     - <code>-cone</code>: 3D drawing on a cone (optional)
/// - <code>toroidalGrid</code>: 2D or 3D grid graph with (almost) regular faces
///     - <code>-girth \<\></code> (mandatory)
///     - <code>-perimeter \<\></code> (mandatory)
///     - <code>-quadrilateral</code>: Quadrilateral faces (default)
///     - <code>-triangular</code>: Triangular faces (optional)
///     - <code>-hexagonal</code>: Hexagonal faces (optional)
/// - <code>moebiusLadder</code>: Projective, but non-planar graph
///     - <code>-nodes \<number of graph nodes\></code> (mandatory)
/// - <code>generalizedPetersen</code>: 3-node regular, non-planar graphs with high girth
///     - <code>-perimeter \<number of exterior graph nodes\></code> (mandatory)
///     - <code>-skew \<connectivity parameter for interior nodes\></code> (mandatory)
/// - <code>intersectionGraph</code>: Intersection graph on the k-element subsets of a ground set
///     - <code>-groundSet \<ground set cardinality\></code> (mandatory)
///     - <code>-subset \<subset cardinality\></code> (mandatory)
///     - <code>-meet \<lower meet cardinality bound\></code> \<upper meet cardinality bound> (optional, default: 0)
/// - <code>regularTree</code>: Regular tree
///     - <code>-nodes \<number of graph nodes\></code> (optional)
///     - <code>-depth \<tree depth\></code> (mandatory)
///     - <code>-deg \<maximal node out degree\></code> (mandatory)
/// - <code>butterflyGraph</code> : Layered graph with "butterfly" bicliques
///     - <code>-length \<word length\></code> (mandatory)
///     - <code>-base \<alphabnet cardinality\></code> (mandatory)
/// - <code>cyclicButterfly</code> : Regular digraph with a cyclic layering
///     - <code>-length \<word length\></code> (mandatory)
///     - <code>-base \<alphabnet cardinality\></code> (mandatory)
/// - <code>sierpinskiTriangle</code>: Plane, recursive construction from triangles
///     - <code>-depth \<recursion depth\></code> (mandatory)
/// - <code>mycielskianGraph</code>: Graphs with low clique number, but large chromatic number
///     - <code>-depth \<recursion depth\></code> (mandatory)
/// - <code>triangularGraph</code>: Graph on the 2-element subsets of a ground set
///     - <code>-cardinality \<ground set cardinality\></code> (mandatory)
/// - <code>permutationGraph</code>: Random permutation graph
///     - <code>-nodes \<number of graph nodes\></code> (mandatory)
/// - <code>thresholdGraph</code>: Threshold graph with random node weights
///     - <code>-nodes \<number of graph nodes\></code> (mandatory)
///     - <code>-threshold \<minimum edge weight\></code> (optional, default: 0)
///     - <code>-min \<lower bound on the node weights\></code> (optional, default: 0)
///     - <code>-max \<upper bound on the node weights\></code> (mandatory)
/// - <code>intervalGraph</code>: Interval graph with random node intervals
///     - <code>-nodes \<number of graph nodes\></code> (mandatory)
///     - <code>-range \<total value range\></code> (mandatory)
///
/// The general command structure is <code>goblin \<class_name\> \<options\> \<object_name\></code>.
/// The results are a GOBLIN library object and a new Tcl command <code>\<object_name\></code>
/// to address the library object. Example:
/// \verbatim
/// goblin regularTree -depth 5 -deg 2 myTree
/// \endverbatim
/// generates a regular tree named <code>myTree</code>. A regular tree is a special
/// case of a \em sparseDiGraph object, and the respective Tcl command accepts the
/// same command options as if it was generated like
/// <code>goblin sparseDiGraph \<options\> myTree</code>.
///
/// <br>
///
///
/// \section secTclObjectClassification  Object classification
///
/// To obtain the inherent type of a data object, use the command structure
/// <code>\<object_name\> info -\<class_specifier\></code>:
/// - <code>-graphObject</code>: Returns true if the object is some kind of graph object
/// - <code>-mipObject</code>: Returns true if the object is a (mixed integer) linear program
///
/// By the same syntax, graph object can be classified more detailed:
/// - <code>-sparse</code>: Returns true if this is a graph object, implemented by incidence lists
/// - <code>-directed</code>: Returns true if this is a digraph object
/// - <code>-undirected</code>: Returns true if this is an undirected graph object
/// - <code>-bipartite</code>: Returns true if this is a bigraph object
/// - <code>-balanced</code>: Returns true if this is a balanced (skew-symmetric) digraph object
///
/// The preceding commands only evaluate the type of the C++ core library object.
/// For example, if a graph object has been instanciated as a mixedGraph, both
/// <code>\<object_name\> info -directed</code> and <code>\<object_name\> info -undirected</code>
/// return false, irrespective of the actual arc orientations.
///
/// The following options are different in that a high-level recognition method is called:
/// - <code>-planar</code>: Returns true if this is a planar graph object
/// - <code>-chordal</code>: Returns true if this is a chordal graph object
/// - <code>-co-chordal</code>: Returns true if the complementary graph is chordal
///
/// <br>
///
///
/// \section secTclDefaultValues  Default values
///
/// Many object commands allow to explicitly pass default, undefined or
/// infinite values instead of concrete parameter values. In all such
/// cases, an asterisk <code>*</code> applies. For example,
/// \verbatim
/// goblin polarGrid -rows 4 -columns 4 myGraph
/// myGraph spanningTree -rootNode *
/// \endverbatim
/// and
/// \verbatim
/// goblin polarGrid -rows 4 -columns 4 myGraph
/// myGraph spanningTree
/// \endverbatim
/// are equivalent codes, as specifying an undefined root node explicitly instructs
/// the spanning tree method to select a root node by the a certain default rule:
/// The immediate results of
/// \verbatim
/// myGraph configure -rootNode $r
/// myGraph spanningTree
/// \endverbatim
/// and
/// \verbatim
/// myGraph spanningTree -rootNode $r
/// \endverbatim
/// and
/// \verbatim
/// myGraph configure -rootNode $r
/// myGraph spanningTree -rootNode [myGraph info -rootNode]
/// \endverbatim
/// are the same, but the <code>configure</code> command makes the root node
/// selection persistent. That is, subsequent vertex routing method calls apply the
/// configured root node, even if the graph object is written to and reread from
/// file.
///
/// With a few exceptions, this substitution of missing <code>-rootNode</code>,
/// <code>-sourceNode</code> and <code>-targetNode</code> parameters applies to
/// all optimization and layout commands:
/// - <code>shortestPath</code> allows to omit a <code>-targetNode</code> and
///   occasionally determines a full shortest path tree
/// - <code>connectivity</code> selects a missing <code>-sourceNode</code> or
///   <code>-targetNode</code> such that the cut capacity becomes minimal
/// - <code>layout orthogonal -tree -binary</code> determines a missing
///   <code>-rootNode</code> by inspection of the node degrees or the predecessor arcs
///
/// The default rules and its exceptions even apply when the C++ interface is used.
///
/// <br>
///
///
/// \section secTclGraphComposition  Graph Composition
///
/// To construct a graph object from another, the general command structure
/// <code>\<original_object\> \<derived_class\> \<options\> \<derived_object\> </code> and
/// some of the following options apply:
/// - <code>explicitSubgraph</code>: Generate an incidence structure for the
///   subgraph encoded into the flow labels
/// - <code>lineGraph</code>: Generate a graph whose nodes are the edges of the original graph
///     - <code>-planar</code>: Generate edges only for adjacent edges in the original regions
///     - <code>-directed</code>: Assign arc orientations 
/// - <code>dualGraph</code>: For planar graphs, generate a graph whose nodes are
///   the regions of the original graph
///     - <code>-directed</code>: Assign arc orientations due to a given a bipolar orientation
/// - <code>spreadOutRegular</code>: For planar graphs, generate an outerplanar
///   graph by unrolling the faces of regular convex polyhedra
/// - <code>vertexTruncation</code>: For planar graphs, replace every original node by a face
/// - <code>facetSeparation</code>: For planar graphs, replace every edge by one
///   quadrilateral (default) or two triangular faces
///     - <code>-turnLeft</code>: Replace by two triangular faces, as if the original
///       faces would be rotated clockwise
///     - <code>-turnRight</code>: Replace by two triangular faces, as if the original
///       faces would be rotated counter-clockwise
/// - <code>mycielskianGraph</code>: Generate a graph which results from once applying
///   the Mycielski recursion rule
/// - <code>complementaryGraph</code>: Generate an undirected graph in which nodes
///   are adjacent if they are not adjacent in the original graph
/// - <code>underlyingGraph</code>: Generate an undirected graph which differs from
///   the original only by the missing arc orientations
/// - <code>inducedSubgraph</code>: Generate an explicit subgraph, induced by the
///   specified colour classes
///     - <code>-nodeColour \<colour index\></code> (optional, default 0)
///     - <code>-edgeColour \<colour index\></code> (optional. By default, edge
///       colours are not considered)
/// - <code>inducedOrientation</code>: Generate an orientation of the original graph
///   such that arcs are running from lower to higher node colour indices
/// - <code>inducedBigraph</code>: Generate the bipartite subgraph w
///     - <code>-nodeColours \<colour index\> \<colour index\></code>
/// - <code>colourContraction</code>: Generate a mixed graph by identifying all
///   equal coloured nodes
/// - <code>nodeSplitting</code>: For a digraph, generate another digraph in which
///   every node of the original graph is replaced by an arc with the original node
///   demand as the upper capacity bound
/// - <code>completeOrientation</code>: Generate a digraph in which every undirected
///   edge of the original graph is replaced by an antiparallel arc pair
/// - <code>explicitSubdivision</code>: For graphs with a poly-line drawing, replace
///  all bend points by regular graph nodes
/// - <code>distanceGraph</code>: Generate the complete digraph whose edge lengths
///   are the node distances in the original graph object
/// - <code>integerStableSetModel</code>: Generate an integer program which is
///   equivalent with finding a maximum stable set
///
/// For undirected graphs only:
/// - <code>metricGraph</code>: Generate the complete graph whose edge lengths
///   are the node distances in the original graph object
///
/// For digraphs only:
/// - <code>linearFlowModel</code>: Generate a linear program which is equivalent
///   with finding a minimum-cost flow
/// - <code>transitiveClosure</code>: For a DAG, generate the minimal transitive supergraph
/// - <code>intransitiveReduction</code>: From a DAG, export the maximal subgraph
///   without transitive arcs
/// - <code>splitGraph</code>: Generate a balanced digraph, consisting of two
///   disjoint copies of the given digraph
///
/// <br>
///
///
/// \section secTclGraphOptimization  Graph Optimization
///
/// - <code>shortestPath</code>: Determine a shortest path (tree) from a given source node
///     - <code>-sourceNode \<source node index\></code> (optional)
///     - <code>-targetNode \<target node index\></code> (optional)
///     - <code>-residual</code>: Search the residual graph with reduced length labels
/// - <code>networkFlow</code>: Determine a (minimum-cost) network flow
///     - <code>-sourceNode \<source node index\></code> (optional)
///     - <code>-targetNode \<target node index\></code> (optional)
///     - <code>-feasible</code>: Only find a flow which satisfies the node demands
///     - <code>-maximize</code>: Determine a maximum st-flow
/// - <code>connectivity</code>: Determine a minimum edge cut or separating vertex set
///     - <code>-edge</code>: Only the edges, but not the nodes impose capacity restrictions
///     - <code>-strong</code>: Directed arcs count in forward direction only
///     - <code>-vertex</code>: Apply unit node capacities
///     - <code>-sourceNode \<source node index\></code> (optional): A node on the left-hand side
///     - <code>-targetNode \<target node index\></code> (optional): A node on the right-hand side
/// - <code>components</code>: Determine the edge connected components
///     - <code>-kappa \<degree of connectivity\></code> (default value: 1)
///     - <code>-strong</code>: Determine strong components
/// - <code>bipolarOrientation</code>: For biconnected graphs, determine a bipolar
///   numbering and implicit arc orientations
///     - <code>-sourceNode \<source node index\></code> (optional): The desired minimal node
///     - <code>-targetNode \<target node index\></code> (optional): The desired maximal node
///     - <code>-rootArc \<arc index\></code> (optional): The desired return arc
///     - <code>-decompose</code>: Export an ear decomposition instead of the implicit arc orientations
/// - <code>eulerianCycle</code>: Determine a closed walk which covers all edges
/// - <code>chinesePostman</code>: Determine a maximum weight Eulerian subgraph
///     - <code>-adjust</code>: Make the original graph Eulerian by increasing the capacity bounds
/// - <code>maximumEdgeCut</code>: Determine a maximum edge cut
///     - <code>-sourceNode \<source node index\></code> (optional): A node on the left-hand side
///     - <code>-targetNode \<target node index\></code> (optional): A node on the right-hand side
/// - <code>spanningTree</code>: Determine a minimum-cost spanning tree
///     - <code>-rootNode \<root node index\></code> (optional in the undirected case)
///     - <code>-maximize</code>: Reverse the optimization direction
///     - <code>-cycle</code>: Determine a one-cycle tree
/// - <code>steinerTree</code>: Determine a minimum-cost tree which spans all demand nodes
///     - <code>-rootNode \<root node index\></code> (optional in the undirected case)
/// - <code>hamiltonianCycle</code>: Determine a minimum-cost Hamiltonian cycle
///     - <code>-rootNode \<root node index\></code> (optional): Used in spanning tree heuristics
/// - <code>stableSet</code>: Determine a maximum cardinality stable set (void induced subgraph)
/// - <code>maximumClique</code>: Determine a maximum cardinality clique (complete induced subgraph)
/// - <code>vertexCover</code>: Determine a minimum cardinality node which is incident with all edges
/// - <code>nodeColouring</code>: Split the node set into a minimum number of stable sets
///     - <code>-threshold \<accepted number of stable sets\></code> (optional)
/// - <code>cliqueCover</code>: Split the node set into a minimum number of cliques
///     - <code>-threshold \<accepted number of cliques\></code> (optional)
/// - <code>edgeColouring</code>: Split the edge set into a minimum number of 1-matchings
///     - <code>-threshold \<accepted number of matchings\></code> (optional)
/// - <code>feedbackArcSet</code>: Determine a minimum cardinality feedback arc set
///
/// For digraphs only:
/// - <code>topSort</code>: Check if this digraph is acyclic. Assign node colours
///   such that colour indices are increasing from tail to head nodes
/// - <code>criticalPath</code>: If this digraph is acyclic, determine a maximum
///   length path starting at the given root node
/// - <code>treePacking</code>: Packing with arborescences
///     - <code>-rootNode \<root node index\></code> (optional)
///
/// For undirected graphs only:
/// - <code>maximumMatching</code>: Determine a maximum capacity subgraph such that
///   the node degrees do not exceed the node demands
/// - <code>minimumCostMatching</code>: Determine a minimum-cost f-factor (a subgraph
///   such that node degrees and the node demands are all equal)
/// - <code>tJoin</code>: Determine a minimum-cost T-join (a collection of paths
///   starting and ending at demand nodes
/// - <code>edgeCover</code>: Determine a mimimum length edge cover (a set of edges which covers all nodes)
///
/// For balanced digraphs only:
/// - <code>balancedFlow</code>: Determine a (minimum-cost) balanced network flow
///     - <code>-sourceNode \<source node index\></code> (optional)
///     - <code>-maximize</code>: Determine a maximum st-flow
///
/// <br>
///
///
/// \section secTclGraphLayout  Graph Layout
///
/// Graph visualization is controlled with messages like <code>\<object_name\> layout ...</code>
/// and the following options:
/// - <code>configure -\<parameter_name\> \<parameter_value\></code>:
///   For any layout parmater in the layout data pool
/// - <code>info</code>
///     - <code>-exists</code>: True is the graph is visualized anyway
///     - <code>-\<parameter_name\></code>: For any layout parameter in the layout data pool
/// - <code>point \<layout point index\></code>
///     - <code>placeAt \<coordinate values\></code>: Set a layout point position
///     - <code>insertSuccessor</code>: Insert a new layout point into a sequence of control points
///     - <code>info</code>: See Section \ref secTclLayoutPointRetrieval
/// - <code>tree</code>: Layered drawing of predecessor trees
///     - <code>-spacing \<minimum node distance\></code> (optional)
///     - <code>-dx \<minimum node distance on the x-coordinate\></code> (optional)
///     - <code>-dy \<minimum node distance on the y-coordinate\></code> (optional)
///     - <code>-left</code>: Parent nodes are placed atop the left-most child node
///     - <code>-right</code>: Parent nodes are placed atop with the left-most child node
///     - <code>-center</code>: Parent nodes are horizontally aligned in the center of their child nodes
///     - <code>-fdp</code>: Nodes are horizontally aligned according to a force model
/// - <code>fdp</code>: Straight line drawing, node placement according to a force model
///     - <code>-spacing \<recommended node distance\></code>
///     - <code>-preserve</code>: Do not allow changes of the edge crossing properties
///     - <code>-layered</code>: Allow horizontal node movements only
///     - <code>-unrestricted</code>: Explicitly allow for changes of the edge crossing properties
/// - <code>layered</code>: Layered drawing of general graphs
///     - <code>-spacing \<minimum node distance\></code> (optional)
///     - <code>-dx \<minimum node distance on the x-coordinate\></code> (optional)
///     - <code>-dy \<minimum node distance on the y-coordinate\></code> (optional)
///     - <code>-vertical</code> Apply the default set of rules for vertical node placement
///     - <code>-span</code> Determine an edge span minimal node layer assignment
///     - <code>-colours</code> The node layer assignment is determined by the node colours
///     - <code>-horizontal</code> Apply the default set of rules for horizontal node placement
///     - <code>-sweep</code> Reduce the number of edge crossings by changing the horizontal node order
///     - <code>-align</code> For the given horizontal node order, align the nodes by an LP model
///     - <code>-fdp</code> For the given horizontal node order, align the nodes due to a force model
/// - <code>circular</code>: All nodes are placed on the same circle
///     - <code>-spacing \<minimum node distance\></code> (optional)
///     - <code>-outerplanar</code>Expose the current incidence list order (graph must be embedded)
///     - <code>-predecessors</code>Expose the predecessor arcs
///     - <code>-colours</code>Expose the order given by the node colours
/// - <code>plane</code>: Plane straight line drawing
///     - <code>-grid \<minimum node distance\></code> (optional)
///     - <code>-convex</code>: Determine a convex drawing (graph must be triconnected)
///     - <code>-basis \<arc index\></code>: Put the specified arc on the bottom line
/// - <code>orthogonal</code>: Ortogonal drawing with nodes represented by squares
///     - <code>-grid \<minimum node distance\></code> (optional)
///     - <code>-tree</code>: Input graph is a rooted tree
///         - <code>-rootNode \<root node index\></code> (optional)
///         - <code>-binary</code>: HV tree layout with small nodes and without bends
///           (nodes degrees must be two or less)
///     - <code>-small</code>: Drawing with small nodes, plane drawing of planar graphs
///       (graph must be biconnected, node degrees must be 4 or less)
///     - <code>-staircase</code> Plane Kandinsky drawing with big nodes (graph must be triconnected)
///     - <code>-planar</code> Plane Kandinsky drawing with big nodes
/// - <code>visibility</code>: Visibility representation without bends and nodes represented
///   by line segments (graph must be planar)
///     - <code>-grid \<minimum node distance\></code> (optional)
///     - <code>-raw</code>: Results as for the original method, with overshooting node segments
///     - <code>-giotto</code>: Reduce node size to the minimum, without generating edge-edge overlap
///     - <code>-seriesParallel</code>: Expose an intrinsic series-parallel ordering
/// - <code>equilateral</code>: Draw all edges with the same edge length (graph must be outerplanar)
///     - <code>-spacing \<minimum node distance\></code> (optional)
/// - <code>arcRouting</code>: Draw ordinary edges by straight lines, but loops
///   and parallels with bends
///     - <code>-spacing \<minimum node distance\></code> (optional)
/// - <code>alignWithOrigin</code>: Shift all nodes by a constant amount to obtain
///   small non-negative coordinate values
/// - <code>boundingBox</code>
///     - <code>freeze</code>: Freeze the current bounding box
///     - <code>release</code>: Revert to the dynamic bounding box determination
///     - <code>default</code>: Freeze the bounding box currently obtained by the dynamic rule
///     - <code>transform</code>: Move all nodes to a specified bounding interval
///         - <code>-coordinate \<coordinate index\></code> (mandatory)
///         - <code>-range \<minimum coordinate value\> \<maximum coordinate value\></code> (mandatory)
///     - <code>info</code>: Retrieve a current bounding box coordinate value
///         - <code>-coordinate \<coordinate index\></code> (mandatory)
///         - <code>-max</code>: Return the maximum coordinate value
///         - <code>-min</code>: Return the minimum coordinate value
///
/// As an example,
/// \verbatim
/// goblin polarGrid -rows 4 -columns 4 myGraph
/// myGraph layout orthogonal -small
/// \endverbatim
/// generates a 4-planar, face-regular graph and determines an orthogonal plane
/// drawing with small nodes for it.
///
/// <br>
///
///
/// \section secTclGraphRetrieval  Retrieving information about graph objects
///
/// The following problem dimensions can be queried from graph objects:
/// - <code>\#nodes</code>: The number of graph nodes
/// - <code>\#edges</code>: The number of edges, not accouting for arc directions
/// - <code>layout \#points</code>: The number of layout points, including the actual graph nodes
///
/// For bigraphs objects only:
/// - <code>\#leftHand</code>: The number of left-hand nodes
/// - <code>\#rightHand</code>: The number of right-hand nodes
///
/// Most attributes are associated with graph nodes and edges, and the access to
/// particular attribute values is described later. What follows is a list of
/// commands which refer to node or arc attributes, but not to a specific
/// node or arc entity:
/// - <code>constant</code>
///   - <code>-upperBound</code>: 1, if the upper arc capacity bounds all coincide
///   - <code>-lowerBound</code>: 1, if the lower arc capacity bounds all coincide
///   - <code>-edgeLength</code>: 1, if the arc length labels all coincide
///   - <code>-nodeDemand</code>: 1, if the node demands / capacitys all coincide
/// - <code>max</code>
///   - <code>-upperBound</code>: Return the maximum absolute upper arc capacity bound
///   - <code>-lowerBound</code>: Return the maximum absolute lower arc capacity bound
///   - <code>-edgeLength</code>: Return the maximum absolute arc length label
///   - <code>-nodeDemand</code>: Return the maximum absolute node demand / capacity
/// - <code>info</code>
///   - <code>-sourceNode</code>: Return the default source node
///   - <code>-targetNode</code>: Return the default target node
///   - <code>-rootNode</code>: Return the default root node
///   - <code>-metricType</code>: Return the (integer) type of edge length determination
///   - <code>-cardinality</code>: Return the sum of subgraph edge multiplicities
///   - <code>-edgeLength</code>: Return the sum of edge lengths, counting all predecessor arcs
///   - <code>-subgraphWeight</code>: Return the sum of edge lengths,
///     weighted by the subgraph edge multiplicities
/// - <code>adjacency \<start node index\> \<end node index\></code>: Returns an arc connecting
///   the two specified nodes, or <code>*</code> if the nodes are non-adjacent
///
///
/// \subsection secTclArcRetrieval  Retrieving arc attribute values
///
/// Graph arc retrieval messages all start like <code>\<object_name\> arc \<arc index\> info ...</code>.
/// Most messages do not actually depend on the arc direction, but all expect arc indices
/// including the direction bit:
/// - <code>-upperBound</code>: The upper capacity bound of this arc
/// - <code>-lowerBound</code>: The lower capacity bound of this arc
/// - <code>-edgeLength</code>: The length label of this arc
/// - <code>-startNode</code>: The start node (tail) of this arc
/// - <code>-endNode</code>: The end node (head) of this arc
/// - <code>-righthandArc</code>: The successor in the start node's incidence list
/// - <code>-directed</code>: Returns 1, if the arc is directed
/// - <code>-labelAnchorPoint</code>: The index of the layout point, at which the arc label is displayed
/// - <code>-portNode</code>: The control point of this arc, attached to the start node
/// - <code>-hidden</code>: True, if this edge is completely invisible
/// - <code>-subgraph</code>: The subgraph multiplicity of this edge
/// - <code>-edgeColour</code>: The colour index of this edge
///
///
/// \subsection secTclNodeRetrieval  Retrieving node attribute values
///
/// Graph node retrieval messages start like
/// <code>\<object_name\> node \<node index\> info ...</code>:
/// - <code>-firstIncidence</code>: The "first" arc in the node's cyclic incidence list
/// - <code>-nodeDemand</code>: The node demand
/// - <code>-hidden</code>: True, if this node is invisible
/// - <code>-distance</code>: The node distance label
/// - <code>-potential</code>: The node potential
/// - <code>-nodeColour</code>: The node colour
/// - <code>-predecessorArc</code>: The predecessor arc ending at this node
/// - <code>-degree</code>: The node degree in the current subgraph
///
///
/// \subsection secTclLayoutPointRetrieval  Retrieving layout point attribute values
///
/// Layout point retrieval messages start like
/// <code>\<object_name\> layout point \<point index\> info ...</code>:
/// - <code>-successor</code>: The successor in a sequence of layout control points or <code>*</code>
/// - <code>-cx</code>: The x-coordinate value in the current drawing
/// - <code>-cy</code>: The y-coordinate value in the current drawing
/// - <code>-hidden</code>: True, if this layout point is invisible
///
/// <br>
///
///
/// \section secTclGraphManipulation  Graph Manipulation
///
/// - <code>delete</code>: Delete this graph object
/// - <code>node insert</code>: Generate a graph node and return its index
/// - <code>node \<node index\> delete</code>: Delete the specified node
///
/// - <code>configure</code>:
///   - <code>-upperBound \<capacity value\></code>: Assign a constant upper arc capacity bound
///   - <code>-lowerBound \<capacity value\></code>: Assign a constant lower arc capacity bound
///   - <code>-edgeLength \<arc length value\></code>: Assign a constant arc length label
///   - <code>-nodeDemand \<capacity value\></code>: Assign a constant node demand
///   - <code>-sourceNode \<node index\></code>: Assign a default source node
///   - <code>-targetNode \<node index\></code>: Assign a default target node
///   - <code>-rootNode \<node index\></code>: Assign a default root node
///   - <code>-metricType \<edge length mode\></code>: Specify the mode of edge length determination,
///     either <code>disabled</code>, <code>manhattan</code>, <code>euclidian</code>,
///     <code>maximum</code> or <code>spheric</code>
///   - <code>-exteriorArc \<arc index\></code>: Assign an exterior face, specified by one of its arcs
///
/// - <code>random</code>
///   - <code>edges</code>: Generate a specified number of equally distributed edges
///     - <code>-numEdges \<number of egdes to be generated\></code>
///   - <code>eulerian</code>: Generate a random closed walk with a specified number of edge
///     - <code>-numEdges< \<number of egdes to be generated\></code>
///   - <code>regular</code>: Generate a regular graph by adding ranodm arcs
///     - <code>-degree \<desired node degree\></code>
///   - <code>-upperBound</code>: Generate equally distributed upper edge capacity bounds
///   - <code>-lowerBound</code>: Generate equally distributed lower edge capacity bounds
///   - <code>-edgeLength</code>: Generate equally distributed edge length labels
///   - <code>-geometry</code>: Generate equally distributed node positions
///
/// - <code>extract</code>
///   - <code>tree</code>: Load the predecessor arcs with a tree subgraph
///     - <code>-rootNode \<node index\></code>
///   - <code>path</code>: Load the predecessor arcs with a path subgraph
///     - <code>-sourceNode \<node index\></code>
///     - <code>-targetNode \<node index\></code>
///   - <code>cycles</code>: Load the predecessor arcs with a disjoint cycle subgraph
///   - <code>matching</code>: Load the predecessor arcs with a 1-matching subgraph
///   - <code>edgeCover</code>: Load the edge colours with an edge cover obtained
///     from a 1-matching subgraph
///   - <code>trees</code>: Load the predecessor arcs with a 1-matching subgraph
///   - <code>cut</code>: Load the node colours with a bipartition given by the finite distance labels
///   - <code>bipartition</code>: Load the node colours with a bipartition given by the odd distance labels
///   - <code>colours</code>: Load the node colours with a partition
///
/// - <code>release</code>: Release the specified register attributes
///   - <code>-subgraph</code>
///   - <code>-distance</code>
///   - <code>-predecessorArc</code>
///   - <code>-nodeColour</code>
///   - <code>-edgeColour</code>
///   - <code>-potential</code>
///   - <code>-partition</code>
///
///
/// For sparse graph objects only:
/// - <code>arc insert</code>: Generate a graph edge and return its index (excluding the arc orintation bit)
/// - <code>arc \<arc index\> delete</code>: Delete the specified edge
/// - <code>arc \<arc index\> contract</code>: Identify both end nodes and then delete the arc
/// - <code>reorder</code>
///     - <code>incidences</code>: Manipulate the node incidence lists
///         - <code>-planar</code>: If possible, achieve a planar representation
///         - <code>-shuffle</code>: Random ordering
///         - <code>-geometric</code>: Adopt from the current drawing
///         - <code>-outerplanar</code>: From a given planar representation,
///           increase the exterior face length
///     - <code>nodeIndices</code>: Manipulate the node indices
///         - <code>-colours</code>: Order by the colour index values
///         - <code>-demands</code>: Order by the node demand values
///         - <code>-shuffle</code>: Random ordering
///     - <code>edgeIndices</code>:Manipulate the edge indices
///         - <code>-colours</code>: Order by the colour index values
///         - <code>-lengths</code>: Order by the edge length labels
///         - <code>-shuffle</code>: Random ordering
/// - <code>seriesParallel</code>: Apply the edge series-parallel decomposition method
///     - <code>-embedding</code>: If the graph is series-parallel,
///       determine respective node incidence orders
///     - <code>-orient</code>: If the graph is series-parallel, assign respective arc orientations
///     - <code>-undirected</code>: Ignore the arc orientations
///     - <code>-layout</code>: Determine a visibility representation
///     - <code>-minor</code>: If the graph is not series-parallel, find a forbidden minor
///     - <code>-sourceNode \<source node index\></code> (optional)
///     - <code>-targetNode \<target node index\></code> (optional)
/// - <code>merge \<option\> \<object handle\></code>: Merge another graph into this object
///   - <code>-right</code>: Disjoint merge, added graph is displayed on the right-hand side
///   - <code>-below</code>: Disjoint merge, added graph is displayed below
///   - <code>-facets</code>: FacetIdentification() method
/// - <code>explicitParallels</code>: Replace high-capacity edges by couples of parallel edges,
///
///
/// \subsection secTclArcManipulation  Setting Arc Attribute Values
///
/// Graph arc manipulation messages all start like <code>\<object_name\> arc \<arc index\> ...</code>.
/// Most messages do not actually depend on the arc direction, but all expect arc indices
/// including the direction bit.
///
/// - <code>configure</code>
///   - <code>-upperBound \<capacity value\></code>: Assign an upper capacity bound to this edge
///   - <code>-lowerBound \<capacity value\></code>: Assign a lower capacity bound to this edge
///   - <code>-edgeLength \<arc length value\></code>: Assign a length label to this edge
///   - <code>-righthandArc \<arc index\></code>:
///   - <code>-directed \<binary number\></code>: Mark this edge as directed (1) or undirected (0)
///   - <code>-subgraph \<subgraph mulitplicity\></code>: Assign a subgraph mulitplicity to this edge
///   - <code>-edgeColour \<arc index\></code>: Assign a colour index to this edge
/// - <code>provide</code>:
///   - <code>-labelAnchorPoint</code>: If not yet present, reserve an arc label anchor point, and return its index
///   - <code>-portNode</code>: If not yet present, reserve a port node, and return its index
/// - <code>straightLine</code>: Release all control points for this edge
/// - <code>reverse</code>: Reverse the orientation of this arc
///
///
/// \subsection secTclNodeManipulation  Setting Node Attribute Values
///
/// Graph node manipulation messages all start like <code>\<object_name\> node \<node index\> ...</code>
///
/// - <code>configure</code>
///   - <code>-firstIncidence \<arc index\></code>: Select the first arc in the node's cyclic incidence list
///   - <code>-nodeDemand \<capacity value\></code>: Assign a node demand / capacity
///   - <code>-distance</code>: Assign a node distance label
///   - <code>-potential</code>: Assign a node potential
///   - <code>-nodeColour \<node index\></code>: Assign a node colour index
///   - <code>-predecessorArc \<arc index\></code>: Select a predecessor arc pointing to this node
///
/// For sparse bigraphs objects only:
/// - <code>swap</code>: Move this node to the other component
///
///
/// \subsection secTclLayoutPointManipulation  Manipulating layout points
///
/// Layout point manipulation messages all start like
/// <code>\<object_name\> layout point \<point index\> ...</code>
///
/// - <code>insertSuccessor</code>: Add a control point right after this layout point, and return its index
/// - <code>placeAt \<sequence of coordinate values\></code>
///
///
/// \section secTclFileWrite  Writing graph objects to file
///
/// The message <code>write \<file name\></code> applies to all library data objects.
/// The output depends, however, on the concrete data object type.
/// For the time being, only the proprietary formats are supported.
///
/// <br>


/// \page pageTutorials  Tutorials
///
/// - \ref tutorialConstruction
///     - \ref secDependentConstruction
///     - \ref secPlainConstruction
///     - \ref secRecursiveConstruction
///     - \ref secDerivedConstructors
///     - \ref secFileConstructors
/// - \ref tutorialSolvers
///     - \ref tutorialMaxFlow
///     - \ref tutorialShortestPath


/// \page tutorialConstruction  Graph construction
///
/// \section secNoteControllers  A note on controller objects
///
/// Every graph object is assigned to a \ref goblinController object at the time
/// of construction. The controller object supplies with default strategies and
/// values, and with registered callbacks for logging and tracing of methods.
/// A controller object can be specified with default and file constructors,
/// but copy constructors link to the controller object of the original graph.
///
/// The examples which follow do not specify controller objects. By that, all constructed
/// objects are assigned to the \ref goblinDefaultContext controller. If you do
/// so also with your application code - and this is adequate if you do not run
/// concurrent jobs - all library configuration must be addressed to the default
/// controller object. For example, <code>goblinDefaultContext.SuppressLogging()</code>
/// would disable the logging event handler.
///
/// <br>
///
///
/// \section secDependentConstruction  Construction from existing objects
///
/// The first example illustrates how, from a given graph G, its \em complementary
/// graph H is generated:
///
/// \verbatim
/// graph H(G.N());
///
/// for (TNode u=0;u<G.N();++u) {
///    for (TNode v=u+1;v<G.N();++v) {
///       if (G.Adjacency(u,v)==NoArc) H.InsertArc(u,v);
///    }
/// }
/// \endverbatim
///
/// The initial constructor call results in a void graph with an identical node
/// set as G. Then, the procedure iterates on all unordered node pairs u,v and
/// checks if these nodes are adjacent in G. If not, an arc is added to H.
///
/// <br>
///
///
/// \section secPlainConstruction  Construction from scratch
///
/// The first example illustrates how a \em triangular graph is generated.
/// The nodes of this graph are the 2-element subsets of an anonymous ground
/// set. Two subsets are adjacent if an ground set element is in common:
///
/// \verbatim
/// graph G(TNode(0));
/// TNode** subset = new TNode*[cardinality];
///
/// for (TNode i=0;i<cardinality;++i) {
///    subset[i] = new TNode[cardinality];
///
///    for (TNode j=i+1;j<cardinality;++j) {
///       subset[i][j] = G.InsertNode();
///
///       for (TNode k=0  ;k<i;++k) G.InsertArc(subset[i][j],subset[k][j]);
///       for (TNode k=i+1;k<j;++k) G.InsertArc(subset[i][j],subset[i][k]);
///       for (TNode k=0  ;k<i;++k) G.InsertArc(subset[i][j],subset[k][i]);
///    }
/// }
///
/// for (TNode i=0;i<cardinality;++i) delete[] subset[i];
/// delete[] subset;
/// \endverbatim
///
/// Both loops enumerate on the ground set elements which are represented by
/// indices 0, 1, .., cardinality-1. The InsertNode() call returns the index
/// of the added graph node and saves it in a table for convenience. So, the
/// <code>subset[i][j]</code> is an obvious encoding of the subset {i,j},
/// assuming i<j.
///
/// Basically, the sequence of node indices returned by InsertNode() is
/// is 0,1,2,.., and it would be more economic (but less comprehensible)
/// to use a formula in replace of table lookup.
///
/// The calls of InsertArc() require indices of already existing nodes, and
/// so the enumeration of arcs still takes some care. Convince yourself that
/// no graph incidence is missed.
///
/// This code has been taken from the \ref triangularGraph constructor, to be found
/// in \ref sparseGraph.cpp. This file also defines constructors like \ref openGrid
/// which compute node indices from formula instead of table lookup. The following
/// depicts the special case of constructing a grid graph with quadrangular faces:
///
/// \verbatim
/// graph G(TNode(numRows*numColumns));
///
/// for (TNode k=0;k<numRows;++k)
/// {
///    for (TNode l=0;l<numColumns;++l)
///    {
///       if (l<numColumns-1) InsertArc(k*numColumns + l,  k  *numColumns + (l+1));
///       if (k<numRows-1)    InsertArc(k*numColumns + l,(k+1)*numColumns +   l  );
///    }
/// }
/// \endverbatim
///
///
/// <br>
///
///
/// \section secRecursiveConstruction  Recursive construction
///
/// The next illustrates the recursive build of the incidence structure
/// of a hypercube of arbitrary dimension:
///
/// \verbatim
/// sparseDiGraph *Hypercube = new sparseDiGraph(TNode(1));
///
/// for (unsigned long i=0;i<dimension;++i) {
///    sparseDiGraph *NextCube = new sparseDiGraph(*Hypercube,Hypercube->OPT_CLONE);
///    NextCube -> AddGraphByNodes(*Hypercube);
///
///    TNode n0 = Hypercube->N();
///
///    for (TNode j=0;j<n0;++j) {
///       NextCube -> InsertArc(j,n0+j);
///    }
///
///    delete Hypercube;
///    Hypercube = NextCube;
/// }
/// \endverbatim
///
/// The procedure starts with a one-node digraph, that is the hypercube of
/// dimension zero. To obtain the (d+1)-hypercube from the (d)-hypercube,
/// the digraph copy constructor is used. The OPT_CLONE specifier herein
/// demands that the new sparseDiGraph instance has an identical incidence structure
/// (otherwise arcs with zero upper capacity bounds are not mapped, and the
/// order of arc indices changes).
///
/// The AddGraphByNodes() call generates a second, node-disjoint copy of the
/// (d)-hypercube. This has node indices n0, n0+1, .., 2n0-1 in the compound
/// graph. Both (d)-hypercube subgraphs are then connected by explicit InsertArc()
/// method calls which take the end node indices as parameters.
///
/// Similar, compilable code is provided by the file \ref testHypercube.cpp.
///
/// <br>
///
///
/// \section secDerivedConstructors  Derived constructor methods
///
/// The following illustrates the recursive build of the incidence structure
/// of a binary tree. Other than in the previous example, a new class is
/// introduced, with only a constructor method:
///
/// \verbatim
/// class binaryTree : public sparseDiGraph
/// {
///     binaryTree(TNode depth,goblinController& _CT = goblinDefaultContext);
/// };
///
///
/// binaryTree::binaryTree(TNode depth,goblinController &_CT) :
///    managedObject(_CT), sparseDiGraph(TNode(1))
/// {
///    if (depth==0) return;
///
///    binaryTree TR(depth-1,_CT);
///
///    static_cast<sparseRepresentation*>(Representation())
///       -> SetCapacity(2*TR.N()+1,2*TR.N()+2);
///
///    AddGraphByNodes(TR);
///    AddGraphByNodes(TR);
///
///    InsertArc(0,1);
///    InsertArc(0,TR.N()+1);
/// }
/// \endverbatim
///
/// The managedObject initializer assigns new instances to a goblinController
/// object. The sparseDiGraph initializer lets the constructor start from a singleton
/// node graph. This node serves as the root node. If the specified tree depth
/// is zero, nothing more has to be done. Otherwise, the depth (d+1) binary tree
/// is obtained from two disjoint copies of a depth (d) binary tree, calling
/// abstractMixedGraph::AddGraphByNodes() twice. To obtain the (d) binary tree,
/// the displayed constructor method is called recursively.
///
/// The sparseRepresentation::SetCapacity() call is not essential. But it
/// prevents from reallocations, every time, a node or arc is added. It is
/// directly addressed to the representational object, like most of the methods
/// which manipulate graph instances.
///
/// In a final step, the root node of the depth (d+1) binary tree is connected
/// to the root nodes of the depth (d) binary trees by calling InsertArc() with
/// the respective node indices.
///
/// Similar, compilable code is provided by the file \ref testBinTree.cpp.
///
/// <br>
///
///
/// \section secFileConstructors  Construction from file
///
/// The high-level interface method to load graph objects from file is applied
/// like this:
///
/// \verbatim
/// managedObject* X =
///        goblinDefaultContext.ImportByFormatName(filename,formatName);
///
/// if (X && X->IsGraphObject())
/// {
///    abstractMixedGraph *G = dynamic_cast<abstractMixedGraph*>(X);
///    ...
/// }
/// \endverbatim
///
/// This code can load from any file format supported by the library, and
/// construct objects of any target types, as required for the selected
/// <code>formatName</code>. The obvious overhead is that the precise object
/// type must be reconstructed later.
///
/// When the input file format is known in advance, the cast operations can be
/// avoided by using a more low-level method:
///
/// \verbatim
/// sparseDiGraph* G = goblinDefaultContext.Import_DimacsEdge(filename);
///
/// if (G)
/// {
///    ...
/// }
/// \endverbatim
///
/// or, for the library native formats, like this:
///
/// \verbatim
/// sparseDiGraph* G = NULL;
///
/// try {
///    G = new sparseDiGraph(filename);
/// }
/// catch (ERParse) {
///    ...
/// }
/// catch (ERFile)  {
///    ...
/// }
/// \endverbatim
///
/// Note that the first two methods return a NULL pointer in every case
/// of trouble. The third method might throw two different exceptions,
/// for non-existing files and for syntax violations.
///
/// Similar, compilable code is provided by the file \ref testPlanarity.cpp.
///
/// <br>


/// \page tutorialSolvers  Using problem solvers
///
/// \section tutorialMaxFlow  Solving max-flow instances
///
/// To call a problem solver, one usually can choose from two interface
/// functions. To the one function, a mathematical method is explicitly
/// specified. The second function looks up the preferred mathematical method
/// in the \ref goblinController default settings.
///
/// It is unwanted to call a mathematical implementation method directly, since
/// some code is method independent, and centralized in the interface function.
/// In the max-flow case, the mathematical implementation methods apply to digraphs
/// only. The interface function handles the flow values of undirected edges.
///
/// The codes
///
/// \verbatim
/// TCap flowValue = G.MaxFlow(MXF_SAP_SCALE,source,target);
/// \endverbatim
///
/// and
///
/// \verbatim
/// G.Context().methMaxFlow = G.MXF_SAP_SCALE;
/// TCap flowValue = G.MaxFlow(source,target);
/// \endverbatim
///
/// are equivalent up to the obvious side-effect of the second code: Later calls
/// of the max-flow solver would also apply the capacity scaling method. This tells
/// that the first version is best practice.
///
/// As other solvers, the max-flow solver returns an objective value, and stores
/// the found solutions object internally: The flow is maintained by the
/// \ref pageSubgraphManagement "subgraph multiplicities", and the dual solution
/// (a minimum edge cut) by the \ref secDistanceLabels "node distance
/// labels". When the object is saved to file, the solutions are also saved.
///
/// If the max-flow solver is called again, it starts from the existing subgraph.
/// This might be unwanted, especially in the case of changing source and target
/// nodes:
/// \verbatim
/// sparseDiGraph G(filename);
/// TCap lambda = InfCap;
///
/// for (TNode source=0;source<n;++source)
/// {
///    TNode target = (source+1)%n;
///
///    G.InitSubgraph();
///    TCap flowValue = G.MaxFlow(source,target);
///
///    if (flowValue < lambda) lambda = flowValue;
/// }
/// \endverbatim
///
/// This procedure loads a sparseDiGraph G from file and then determines the strong edge
/// connectivity number of G as the final value of <code>lambda</code>. The flow
/// of the previous iteration is infeasible as a starting solution, since all nodes
/// other than the source and the target must be balanced.
/// The \ref abstractMixedGraph::InitSubgraph() operation reverts to a zero
/// flow - but only if the lower capacity bounds are all zero. Most likely, the lines
/// \verbatim
/// sparseDiGraph G(filename);
/// static_cast<sparseRepresentation*>(G.Representation()) -> SetCUCap(1);
/// static_cast<sparseRepresentation*>(G.Representation()) -> SetCLCap(0);
/// static_cast<sparseRepresentation*>(G.Representation()) -> SetCDemand(0);
/// ...
/// \endverbatim
///
/// should be added to obtain the expected result. The
/// \ref graphRepresentation::SetCDemand() call is necessary when the
/// goblinController::methFailSave option is set: Then the max-flow solver
/// verifies on exit if the node demands and flow imbalances match.
///
/// The max-flow solver can deal with infinite capacity bounds. But if the lower
/// capacity bounds are non-trivial, the solver must be supplied with a starting
/// solution, by calling \ref abstractMixedGraph::AdmissibleBFlow(). This evaluates
/// the node demands, and calls the max-flow solver for a \ref digraphToDigraph
/// instance (but this is only an implementational detail).
///
/// Further reading:
/// - \ref pageNetworkFlows
/// - \ref subgraphManagement
///
/// <br>
///
///
/// \section tutorialShortestPath  Solving shortest path instances
///
/// The determination of shortest paths between two graph nodes is probably the
/// most fundamental task in graph optimization. It forms part of many other graph
/// optimization methods, several min-cost flow methods for example. Usually, it
/// is assumed that all edge lengths are non-negative. If so, the well-known Dijkstra
/// method applies, and this is elementary in that it does not depend on any library
/// configuration parameters. In the setting with negative edge lengths, selecting
/// an adequate methods takes much more care.
///
/// Hiding all technical details, the basic application of the shortest path solver
/// is as follows:
///
/// \verbatim
/// if (ShortestPath(source,target))
/// {
///    TFloat dt = Dist(target);
///    TArc pred = GetPredecessors();
///    TNode v = target;
///
///    do {
///       TArc a = pred[v];
///       v = StartNode(a);
///    }
///    while (v!=source);
/// }
/// \endverbatim
///
/// The return value of \ref abstractMixedGraph::ShortestPath() tells whether the
/// target node can be accessed on an \em eligible path from the source node.
/// In occasion, the determined path is stored by the \ref secPredecessorLabels,
/// and the path length can be retrieved by the \ref abstractMixedGraph::Dist()
/// function which is short-hand for accessing the \ref secDistanceLabels. The
/// loop does nothing else than traversing this path back from the target node
/// to the source node.
///
/// A first technical explanation concerns the definition of eligible arcs:
/// In the default setting as above, directed arcs may be traversed only one-way,
/// but undirected edge can be traversed in both directions. By using other
/// interface methods, one might modify this eligibily rule:
/// \verbatim
/// BFS(subgraphArcs(*this),source,target);
/// \endverbatim
/// would determine a path in the current subgraph. This path does not minimize
/// the sum of edge length but only the number of path edges, as it is computed
/// by breadth-first search. This code is roughly the same as
/// \verbatim
/// ShortestPath(SPX_BFS,SPX_ORIGINAL,subgraphArcs(*this),source,target);
/// \endverbatim
/// but the latter method requires uniform edge lengths.
///
/// There is a strong impact of method selection and solver configuration on the
/// performance:
/// - What method is best performing depends on the input graph.
///   Is the graph acyclic? Is it undirected? Are the edge lengths uniform?
///   Are the edge lengths non-negative? Do negative length cycles exist?
/// - \ref abstractMixedGraph::SPX_Dijkstra() and \ref abstractMixedGraph::BFS()
///   stop when the target node is reached the first time, as the corresponding
///   path is minimal. But if no target node is specified, a <em>shortest path
///   tree</em> is determined. This is a predecessor tree which entirely consists
///   of shortest paths.
/// - For sake of efficiency, one often considers several potential start and
///   target nodes at one time. But this only works for the Dijkstra method.
/// - In order to avoid explicit construction of graph instances, there are
///   modifiers for the edge lengths (\ref abstractMixedGraph::TOptSPX), and
///   arc index sets to vary the eligibility rule.
///
/// At the time, there is no implementation to handle the very general setting
/// of mixed graphs with negative edge lengths. This setting is equivalent with
/// the <em>longest path problem</em> in the literature, which can be considered
/// hard to solve.
///
/// Even if such a general method would exist, it would be no good idea to
/// apply it by default. There are a lot of applications which can guarantee
/// non-negativity of the edge lengths, and hence can solved by the Dijkstra
/// method.
///
/// But what is special about the non-negative edge length setting? To understand
/// this, consider the difference between \em walks (a series of adjacent arcs),
/// \em paths (walks without arc repetitions) and \em simple paths (walks without
/// node repetitions):
/// - When all arc lengths are positive, every minimum length walk is a simple path.
/// - When all arc lengths are non-negative, every minimum length walk can be reduced
///   to a simple path of the same length.
/// - When negative edge lengths are allowed, the problem of finding a shortest
///   simple path is bounded. The problem of finding a shortest walk is unbounded
///   if there is a walk containing a negative length cycle.
/// - In the negative edge length setting, the problem of finding a shortest, not
///   necessarily simple path is bounded, and can be solved efficiently by min-cost
///   flow techniques.
///
/// The predecessor labels register can only represent simple paths, and those
/// are the intended output of the shortest path solver. Technically, all methods
/// which deal with negative edge lengths operate on paths rather than simple paths.
/// If no negative length subcycle exists in the found path, it can be reduced
/// to a simple path of the same length.
///
/// Accepting negative edge lengths, the following cases can be solved efficiently:
/// - Directed acyclic graphs (DAGs) admit a linear time label setting method
///   (specify \ref abstractMixedGraph::SPX_DAG with the solver interface).
/// - Mixed graphs with negative length arcs, but without negative length cycles
///   admit label correcting methods such as \ref abstractMixedGraph::SPX_FIFOLabelCorrecting().
///   (specify \ref abstractMixedGraph::SPX_FIFO or \ref abstractMixedGraph::SPX_BELLMAN).
///   Unfortunately, negative length undirected edges constitute negative length cycles
///   which consist of two antiparallel arcs.
/// - Undirected graphs with negative length edges, but without negative length
///   cycles. Can be solved by reduction to a T-Join problem
///   (specify \ref abstractMixedGraph::SPX_TJOIN). And so depends on the min-cost flow
///   and the weighted matching solver.
///
/// Further reading:
/// - \ref secShortestPath
/// - \ref secMatching
///
/// <br>


// The following page has no reference but only copies the background image
/// \page pageDummy
/// \image html  wallpaper.jpg
