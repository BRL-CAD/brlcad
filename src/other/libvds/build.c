/**
 * @memo	Routines for constructing the VDSlib vertex tree.
 * @name 	Building the vertex tree
 *
 * These routines provide a flexible interface for specifying the geometry
 * of the original model, and for constructing the VDSlib vertex tree upon
 * the vertices of that underlying high-resolution model.<p>
 *
 * Building a vertex tree begins with a call to \Ref{vdsBeginVertexTree}()
 * and ends with a call to \Ref{vdsEndVertexTree}().  The building process
 * consists of two stages: specifying geometry and clustering nodes.  All
 * geometry must be specified before node clustering begins.  <p>
 *
 * Use \Ref{vdsBeginGeometry}() to signal the beginning of the first
 * stage.  Geometry (in VDSlib parlance) consists of a list of
 * <i>nodes</i>, or vertices in the original model, and a list of
 * <i>triangles</i>, whose three corners are specified as indices into the
 * list of nodes.  Nodes are added with the \Ref{vdsAddNode}() call and
 * triangles are added with \Ref{vdsAddTri}().  The nodes referenced by a
 * triangle must be added before that triangle can be added.  When all
 * geometry has been specified, call \Ref{vdsEndGeometry}() to move on to
 * the next stage.<p>
 *
 * The vertex clustering stage simply consists of calls to
 * \Ref{vdsClusterNodes}().  This function takes several nodes (up to
 * \Ref{VDS_MAXDEGREE}) and clusters them together, assigning the nodes as
 * children of a new parent node with coordinates specified by the user
 * (it is an error to call \Ref{vdsClusterNodes}() on a node which has
 * already been assigned a parent).  This parent node may then be
 * clustered with other nodes with further calls to
 * \Ref{vdsClusterNodes}(), and so on, until all nodes have been clustered
 * into a single tree.  This is the <i>vertex tree</i>, the fundamental
 * data structure of VDSlib.  Note that the vertices of the original
 * (highest-resolution) model, specified by the user in the first stage,
 * form the leaf nodes of the vertex tree.  Each internal node thus represents
 * some subset of the original vertices, approximating those vertices with
 * a single point.  <p>
 *
 * Once the vertex tree has been built with successive calls to
 * \Ref{vdsClusterNodes}(), the user calls \Ref{vdsEndVertexTree}() to
 * finalize it.  The finished vertex tree is now ready for run-time
 * maintainance and rendering. <p>
 *
 * @see		build.c
 */
/*@{*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "vds.h"
#include "vdsprivate.h"
#include "vector.h"
#include "path.h"

/*
 * Globals
 */
vdsNode *nodearray = NULL;
int vdsNumnodes;
static int maxnodes;
vdsTri *triarray = NULL;
int vdsNumtris;
static int maxtris;
static int curroffset;
static int openflag = 0;

/** Initializes the builder.
 * Note:	Currently nothing to do here.
 */
void vdsBeginVertexTree()
{
    assert(openflag == 0);
    openflag = 1;
}

/** Initialize the geometry structures.
 * 		Must be called before calling vdsAddNode() and vdsAddTri(),
 *		and must be followed by vdsEndGeometry().
 */
void vdsBeginGeometry()
{
    assert(openflag == 1);
    maxnodes = 1024;
    nodearray = (vdsNode *) calloc(maxnodes, sizeof(vdsNode));
    vdsNumnodes = 0;
    maxtris = 1024;
    triarray = (vdsTri *) calloc(maxtris, sizeof(vdsTri));
    vdsNumtris = 0;
    curroffset = 0;
}

/** When all geometry has been added, prepares VDSlib for node clustering.
 * 		When all geometry (tris & verts) has been added, this
 *		function prepares the builder for the next phase (merging
 *		nodes into a tree).  Must be called after vdsBeginGeometry(),
 *		after all vdsAddNode() and vdsAddTri() calls, and before any
 *		vdsClusterNodes() calls.
 * @return	A pointer to an array containing all leaf nodes, in case the
 *		application wants to use this information during clustering.
 *		Leaf nodes in the array are in the order they were given.
 */
vdsNode *vdsEndGeometry()
{
    assert(openflag == 1);
    /* Tighten nodearray and triarray to free up unused memory */
    nodearray = (vdsNode *) realloc(nodearray, vdsNumnodes * sizeof(vdsNode));
    triarray = (vdsTri *) realloc(triarray, vdsNumtris * sizeof(vdsTri));
    return nodearray;
}

/** Start a new object, with separate vertex and triangle lists.
 * 		Models are often organized into "objects", where each object
 *		consists of vertex and triangle lists.  An object's triangles
 *		index corners in that object's vertex list, rather than a
 *		global vertex list.  VDSlib, however, relies on combining
 *		object vertices into such a global vertex list.  This utility
 *		function allows the user to specify triangles without
 *		renumbering the corner indices.  For example, to load two
 *		objects into VDSlib, call vdsAddNode() on the vertices of
 *		object 1, vdsAddTri() on the triangles of object 1, then
 *		call vdsNewObject().  Finally, add the vertices and triangles
 *		of object 2.  VDSlib will automatically offset the indices
 *		of object 2's triangle corners. <p>
 *
 * <b>Note:</b>	vdsNewObject() must be called between vdsBeginGeometry()
 *		and vdsEndGeometry().
 */
void vdsNewObject()
{
    curroffset = vdsNumnodes;
}

/** Add a vertex of the original model as a leaf node of the VDS vertex tree.
 * 		Given a vertex coordinate, creates a vdsNode to represent the
 *		vertex as a leaf node in the vertex tree.
 * @param	x 	The X coordinate of the vertex
 * @param	y 	The Y coordinate of the vertex
 * @param	z 	The Z coordinate of the vertex
 * @return	A pointer to the created node, in case the user wishes to
 *		create a vertex->data field. <b>NOTE</b>: This pointer is
 *		only guaranteed valid until the next call to vdsAddNode()
 *		or vdsEndGeometry().
 *
 * <b>Note:</b>	vdsAddNode() must be called between vdsBeginGeometry()
 *		and vdsEndGeometry().
 */
vdsNode *vdsAddNode(vdsFloat x, vdsFloat y, vdsFloat z)
{
    assert(openflag == 1);
    nodearray[vdsNumnodes].coord[0] = x;
    nodearray[vdsNumnodes].coord[1] = y;
    nodearray[vdsNumnodes].coord[2] = z;
    vdsNumnodes++;
    /* Resize array if necessary */
    if (vdsNumnodes == maxnodes) {
	vdsNode *tmparray = (vdsNode *) calloc(maxnodes * 2, sizeof(vdsNode));

	memcpy(tmparray, nodearray, vdsNumnodes * sizeof(vdsNode));
	free(nodearray);
	nodearray = tmparray;
	maxnodes *= 2;
    }
    return &nodearray[vdsNumnodes - 1];
}

/** Add a triangle of the original model.
 * 		Given a triangle (specified as three corner indices into a
 *		vertex list) creates a vdsTri struct with the given attributes.
 *		Later, when vdsEndVertexTree() is called, each tri will be
 *		copied directly into the subtri array of the appropriate node.
 *		Note that the vertices index by the triangle must already have
 *		been added via vdsAddNode().
 * @return	A pointer to the created vdsTri struct, in case the user
 *		wishes to create a tri->data field. <b>NOTE</b>: This pointer
 *		is only guaranteed valid until the next call to vdsAddTri()
 *		or vdsEndGeometry().
 *
 * <b>Note:</b>	vdsAddTri() must be called between vdsBeginGeometry()
 *		and vdsEndGeometry().
 */
vdsTri *vdsAddTri(int v0, int v1, int v2,
		  vdsVec3 n0, vdsVec3 n1, vdsVec3 n2,
		  vdsByte3 c0, vdsByte3 c1, vdsByte3 c2)
{
    assert(openflag == 1);
    triarray[vdsNumtris].corners[0].index = v0 + curroffset;
    triarray[vdsNumtris].corners[1].index = v1 + curroffset;
    triarray[vdsNumtris].corners[2].index = v2 + curroffset;
    assert(v0 + curroffset < vdsNumnodes); 		/* Sanity check */
    assert(v1 + curroffset < vdsNumnodes);		/* Sanity check */
    assert(v2 + curroffset < vdsNumnodes);		/* Sanity check */
    VEC3_COPY(triarray[vdsNumtris].normal[0], n0);
    VEC3_COPY(triarray[vdsNumtris].normal[1], n1);
    VEC3_COPY(triarray[vdsNumtris].normal[2], n2);
    BYTE3_COPY(triarray[vdsNumtris].color[0], c0);
    BYTE3_COPY(triarray[vdsNumtris].color[1], c1);
    BYTE3_COPY(triarray[vdsNumtris].color[2], c2);
    vdsNumtris ++;
    /* Resize array if necessary */
    if (vdsNumtris == maxtris) {
	vdsTri *tmparray = (vdsTri *) calloc(maxtris * 2, sizeof(vdsTri));

	memcpy(tmparray, triarray, vdsNumtris * sizeof(vdsTri));
	free(triarray);
	triarray = tmparray;
	maxtris *= 2;
    }
    return &triarray[vdsNumtris - 1];
}

/** Cluster a set of nodes under a single new node.
 * 		Attaches the given list of nodes to a new parent node, with
 *		a representative vertex at the specified coordinates.
 * @param 	nnodes 	The number of nodes being merged
 * @param	nodes 	Nodes being merged (it is an error if any of these
 *			nodes already has a parent node)
 * @param	XYZ  The coordinates of the new node's repvert
 * @return	A pointer to the newly created parent node. <b>NOTE</b>:
 *		This pointer is only valid until the vdsEndVertexTree() call.
 */
vdsNode *vdsClusterNodes(int nnodes, vdsNode **nodes,
		 vdsFloat x, vdsFloat y, vdsFloat z)
{
    int i;
    vdsNode *parent = (vdsNode *) calloc(1, sizeof(vdsNode));

    assert(nnodes <= VDS_MAXDEGREE);
    assert(nodes[0] != NULL);
    parent->children = nodes[0];
    parent->children->parent = parent;
    for (i = 1; i < nnodes; i++) {
	assert(nodes[i] != NULL);
	assert(nodes[i]->parent == NULL);
	nodes[i]->parent = parent;
	nodes[i - 1]->sibling = nodes[i];
    }
    nodes[nnodes - 1]->sibling = NULL;
    parent->coord[0] = x;
    parent->coord[1] = y;
    parent->coord[2] = z;

    return parent;
}

/*
 * Function:	idEqual
 * Description:	Returns 1 if the two given vdsNodeIds are equal, 0 otherwise
 */
static int idEqual(vdsNodeId n1, vdsNodeId n2)
{
    if (n1.depth != n2.depth) {
	return 0;
    } else {
	if (n1.path != n2.path) {
	    return 0;
	} else {
	    return 1;
	}
    }
}

/*
 * Function:	assignNodeIds
 * Description:	Recursively descends vertex tree, assigning node paths & depths
 *		and initializing node->status to Inactive (since 0 == Boundary
 *		for speed reasons).
 * Arguments:	node - the node rooting the given subtree
 *		nodeId - the node id of the given node
 * 		idArray - array in which we put ids of leaf nodes
 */
static int assignNodeIds(vdsNode *node, vdsNodeId nodeId, vdsNodeId *idArray)
{
    int i, j;
    vdsNode *child;
    vdsNodeId childId;
    static int maxdepth = 0;
    static int maxdegree = 0;

    node->status = Inactive;
    node->depth = nodeId.depth;
    j = 0;
    child = node->children;
    while (child != NULL) {
	childId.depth = nodeId.depth + 1;
	PATH_COPY(childId.path, nodeId.path);
	PATH_SET_BRANCH(childId.path, nodeId.depth, j);

	assignNodeIds(child, childId, idArray);
	j++;
	child = child->sibling;
    }
    /*
     * If this is a leaf node (i.e., a vertex in original model) store its
     * NodeId in idArray[].  Later we will convert tri->corners to reference
     * these nodes by NodeId rather than index in nodearray[].
     */
    if (j == 0) {
	int index = node - nodearray;		/* pointer arithmetic */
	idArray[index] = nodeId;
    }
    if (j > maxdegree) {
	maxdegree = j;
    }
    if (nodeId.depth > maxdepth) {
	maxdepth = nodeId.depth;
    }
    assert (maxdegree <= VDS_MAXDEGREE);
    assert(maxdepth <= VDS_MAXDEPTH);

    return maxdepth;
}

/*
 * Function:	verifyRootedTree
 * Description:	Verifies that all the leaf nodes in nodearray belong to a
 *		tree rooted at root.  Must be called after assignNodeIds().
 */
static void verifyRootedTree(vdsNode *root)
{
    int i;
    vdsNode *node;

    for (i = 0; i < vdsNumnodes; i++) {
	node = &nodearray[i];
	if (node->depth == 0 && node != root) {
	    fprintf(stderr, "\tError: leaf node #%d is not part of the same\n"
		    "\trooted tree as previous nodes\n", i);
	    exit(1);
	}
    }
}

/*
 * Function:	computeSubtris
 * Description:	Processes the tris in triarray and assigns each as the subtri
 *		of the deepest node in the vertex tree to cluster two or more
 *		corners of the triangle.  Subtris are stored temporarily in
 *		the node->vistris linked list.
 */
static void computeSubtris(vdsNode *root)
{
    int i;

    for (i = 0; i < vdsNumtris; i++) {
	vdsTri *t = &triarray[i];
	vdsNodeId c0, c1, c2;		/* The 3 corners of the triangle */
	vdsNodeId com01, com02, com12;	/* Common ancestor of c0c1,c1c2,c0c2*/
	vdsNode *N = NULL;		/* The node t is a subtri of	 */

	c0 = t->corners[0].id;
	c1 = t->corners[1].id;
	c2 = t->corners[2].id;
	com01 = vdsFindCommonId(c0, c1, VDS_MAXDEPTH);
	com02 = vdsFindCommonId(c0, c2, VDS_MAXDEPTH);
	com12 = vdsFindCommonId(c1, c2, VDS_MAXDEPTH);
	/* t is a subtri of the deepest common ancestor of its corner nodes */
	if (com01.depth >= com02.depth) {
	    if (com01.depth >= com12.depth) {
		N = vdsFindNode(com01, root);
	    } else {
		N = vdsFindNode(com12, root);
	    }
	} else {
	    if (com02.depth >= com12.depth) {
		N = vdsFindNode(com02, root);
	    } else {
		N = vdsFindNode(com12, root);
	    }
	}
	/* t is a subtri of node N; add it to the N->vistris linked list */
	t->next = N->vistris;
	if (t->next != NULL) {
	    t->next->prev = t;
	}
	N->vistris = t;
	N->nsubtris++;
    }

}

/*
 * Function:	moveTrisToNodes
 * Description:	After all subtris have been associated with nodes via the
 *		node->vistris linked list, this function traverses the tree
 *		and reallocates each node to include room for the subtris
 *		directly in the node->subtris[] field.  The triangle data
 *		is then copied into this field, allowing triarray to be freed.
 *		After reallocating the node, corrects parent/child pointers.
 * Note:	Since leaf nodes of the vertex tree (the original vertices
 *		of the model) are reallocated, nodearray can now be freed.
 */
static vdsNode *moveTrisToNodes(vdsNode *N)
{
    vdsTri *t, *nextt;
    int whichtri, numchildren;
    vdsNode *newN, *child;

    /* Reallocate node and adjust child pointers */
    newN = (vdsNode *) malloc(sizeof(vdsNode) + N->nsubtris * sizeof(vdsTri));
    memcpy(newN, N, sizeof(vdsNode));
    child = N->children;
    numchildren = 0;
    while (child != NULL) {
	child->parent = newN;
	numchildren++;
	child = child->sibling;
    }
    /* Adjust parent's pointer to this node */
    if (N->parent != NULL) {
	vdsNode *pp = N->parent->children;

	if (pp == N) {
	    N->parent->children = newN;
	} else {
	    while (pp->sibling != N) {
		pp = pp->sibling;
	    }
	    pp->sibling = newN;
	}
    }
    /* Don't try to free leaf nodes, which belong to nodearray */
    if (numchildren) {
	free(N);
    }
    N = newN;
    /* Copy node->vistris list into node->subtris[]; clear linked list ptrs */
    t = N->vistris;
    N->vistris = NULL;
    whichtri = 0;
    while (t != NULL) {
	N->subtris[whichtri] = *t;
	whichtri++;
	nextt = t->next;
	t->next = t->prev = NULL;
	t = nextt;
    }
    /* Recurse on children nodes */
    child = N->children;
    while (child != NULL) {
	/* since moveTrisToNodes() reallocates nodes, have to reset child */
	child = moveTrisToNodes(child);
	child = child->sibling;
    }
    return N;
}

/*
 * Function:	computeInternalBounds
 * Description:	Recursively descends the vertex tree, computing each node's
 *		bounding sphere from its children's bounds.
 * Algorithm:	Computes the center of the new bounding sphere by averaging
 *		the centers of the child spheres.  For each child sphere,
 *		find the distance from its center to the averaged parent
 *		center, plus the radius of the child sphere.  The maximum of
 *		these is used as the radius for the parent sphere.
 * XXX:		This is certainly suboptimal.  Perhaps I should modify and use
 *		the Graphics Gems code for finding tightly-enclosing spheres?
 */
static void computeInternalBounds(vdsNode *node)
{
    vdsNode *child = node->children;
    int numchildren = 0;
    vdsFloat maxdist = 0;
    vdsVec3 center = {0, 0, 0};

    /* If node is a leaf, just return */
    if (child == NULL) {
	return;
    }

    while (child != NULL) {
	computeInternalBounds(child);	/* Recurse to compute child sphere */
	VEC3_ADD(center, center, child->bound.center);
	child = child->sibling;
	numchildren++;
    }
    VEC3_SCALE(center,  1.0 / (vdsFloat)numchildren, center);
    child = node->children;
    while (child != NULL) {
	vdsFloat dist =
	    VEC3_DISTANCE(center, child->bound.center) + child->bound.radius;
	if (dist > maxdist) {
	    maxdist = dist;
	}
	child = child->sibling;
    }
    VEC3_COPY(node->bound.center, center);
    node->bound.radius = maxdist;
}

/* Recursively assign bounding volumes to each node.
 *
 * Algorithm:	For the moment, uses a very simple, crude way of estimating
 *		bounding spheres: spins through triarray and for each leaf
 *		node, computes an axis-aligned bounding box that completely
 *		contains all triangles associated with that node.  From this
 *		AABB bounding spheres are calculated for each leaf node, then
 *		a recursive postorder traversal builds bounding spheres for
 *		the internal nodes of the vertex tree.
 */
static void assignNodeBounds(vdsNode *root)
{
    typedef struct {
	vdsVec3 min, max;
    } Box;
    Box *boxes;
    int i, j, k;

    boxes = (Box *) malloc(vdsNumnodes * sizeof(Box));
    for (i = 0; i < vdsNumnodes; i++) {
	VEC3_COPY(boxes[i].min, nodearray[i].coord);
	VEC3_COPY(boxes[i].max, nodearray[i].coord);
    }
    for (i = 0; i < vdsNumtris; i++) {	/* iterate over tris */
	vdsTri *t = &triarray[i];
	vdsNode *c0 = &nodearray[t->corners[0].index];
	vdsNode *c1 = &nodearray[t->corners[1].index];
	vdsNode *c2 = &nodearray[t->corners[2].index];

	for (j = 0; j < 3; j++) {	/* iterate over corners of tri */
	    int cindex = t->corners[j].index;
	    vdsNode *corner = &nodearray[cindex];
	    /* Some effort wasted here testing each corner against itself: */
	    VEC3_FIND_MAX(boxes[cindex].max, boxes[cindex].max, c0->coord);
	    VEC3_FIND_MIN(boxes[cindex].min, boxes[cindex].min, c0->coord);
	    VEC3_FIND_MAX(boxes[cindex].max, boxes[cindex].max, c1->coord);
	    VEC3_FIND_MIN(boxes[cindex].min, boxes[cindex].min, c1->coord);
	    VEC3_FIND_MAX(boxes[cindex].max, boxes[cindex].max, c2->coord);
	    VEC3_FIND_MIN(boxes[cindex].min, boxes[cindex].min, c2->coord);
	}
    }
    /* Calculate bounding spheres from leaf nodes' bounding boxes */
    for (i = 0; i < vdsNumnodes; i++) {
	vdsNode *N = &nodearray[i];

	VEC3_AVERAGE(N->bound.center, boxes[i].min, boxes[i].max);
	N->bound.radius = VEC3_DISTANCE(boxes[i].min, boxes[i].max) / 2.0;
    }
    free(boxes);
    /* Recursively calculate internal node bounds from leaf bounds */
    computeInternalBounds(root);
}

/** Finalize the VDS vertex tree.
 * 		After all geometry has been added and all leaf nodes (created
 *		by vdsAddNode()) merged to form a single hierarchy of vdsNodes,
 *		call vdsEndVertexTree() to process the hierarchy and produce
 *		a finished vertex tree, ready for simplification/rendering.<p>
 *
 * Tasks:	Performs the following tasks, in order:		<p><ll>
 *		<li> Computes node paths and ids
 *		<li> Verifies that node hierarchy forms a single rooted tree
 *		<li> Computes node bounds for fold & visibility tests
 *		<li> Computes node subtris, attaching via vistris linked list
 *		<li> Converts vdsTris to use leaf node ids, rather than indices
 *		<li> Reallocates each node to store subtris directly
 *		<li> Nullifies node->vistris list and tri next,prev pointers
 *		<li> Computes triangle container nodes for tri->node field
 *		<li> Assigns tri->proxies[] pointers to point at root node
 *		<li> Deletes triarray
 *		<li> Labels root node Boundary			</ll>
 * @return	Pointer to the root node of the new vertex tree.
 */
vdsNode *vdsEndVertexTree()
{
    vdsNode *root;
    vdsNodeId rootId = {0, 0};
    vdsNodeId *ids;
    int maxdepth;
    int i, j;

    assert(openflag == 1);
    root = &nodearray[0];
    while (root->parent != NULL) {
	root = root->parent;
    }
    VDS_DEBUG(("Assigning node ids..."));
    ids = (vdsNodeId *) calloc(vdsNumtris, sizeof(vdsNodeId));
    maxdepth = assignNodeIds(root, rootId, ids);
    VDS_DEBUG(("Done.\n"));
    VDS_DEBUG(("Verifying that all nodes form a single rooted tree..."));
    verifyRootedTree(root);
    VDS_DEBUG(("Done.\n"));
    VDS_DEBUG(("Assigning node->bound for all nodes..."));
    assignNodeBounds(root);
    VDS_DEBUG(("Done.\n"));
    VDS_DEBUG(("Converting triangles to reference nodes by ID..."));
    /* Convert tris to reference node IDs, rather than indices */
    for (i = 0; i < vdsNumtris; i++) {
	vdsTri *T = &triarray[i];

	T->corners[0].id = ids[T->corners[0].index];
	T->corners[1].id = ids[T->corners[1].index];
	T->corners[2].id = ids[T->corners[2].index];
	assert(! idEqual(T->corners[0].id, T->corners[1].id) &&
	       ! idEqual(T->corners[1].id, T->corners[2].id) &&
	       ! idEqual(T->corners[2].id, T->corners[0].id));
    }
    free(ids);
    VDS_DEBUG(("Done.\n"));
    VDS_DEBUG(("Computing subtris..."));
    computeSubtris(root);
    VDS_DEBUG(("Done.\n"));
    VDS_DEBUG(("Reallocating nodes and copying triangles into node->subtris "
	       "fields..."));
    root = moveTrisToNodes(root);	/* Note: reallocates all nodes	    */
    free(nodearray);
    VDS_DEBUG(("Done.\n"));
    VDS_DEBUG(("Computing triangle container nodes..."));
    vdsComputeTriNodes(root, root);
    free(triarray);			/* nodearray still holds leaf nodes */
    root->status = Boundary;		/* root initially on boundary	    */
    root->next = root->prev = root;	/* root always on boundary path	    */
    VDS_DEBUG(("Done.\nVertex tree complete! Maximum depth = %d\n", maxdepth));

    openflag = 0;
    return root;
}

/*@}*/
/***************************************************************************\

  Copyright 1999 The University of Virginia.
  All Rights Reserved.

  Permission to use, copy, modify and distribute this software and its
  documentation without fee, and without a written agreement, is
  hereby granted, provided that the above copyright notice and the
  complete text of this comment appear in all copies, and provided that
  the University of Virginia and the original authors are credited in
  any publications arising from the use of this software.

  IN NO EVENT SHALL THE UNIVERSITY OF VIRGINIA
  OR THE AUTHOR OF THIS SOFTWARE BE LIABLE TO ANY PARTY FOR DIRECT,
  INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, INCLUDING
  LOST PROFITS, ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS
  DOCUMENTATION, EVEN IF THE UNIVERSITY OF VIRGINIA AND/OR THE
  AUTHOR OF THIS SOFTWARE HAVE BEEN ADVISED OF THE POSSIBILITY OF
  SUCH DAMAGES.

  The author of the vdslib software library may be contacted at:

  US Mail:             Dr. David Patrick Luebke
		       Department of Computer Science
		       Thornton Hall, University of Virginia
		       Charlottesville, VA 22903

  Phone:               (804)924-1021

  EMail:               luebke@cs.virginia.edu

\*****************************************************************************/
