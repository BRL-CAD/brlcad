/***************************************************************************\

  Copyright 1999 The University of Virginia.
  All Rights Reserved.

  Permission to use, copy, modify and distribute this software and its
  documentation without fee, and without a written agreement, is
  hereby granted, provided that the above copyright notice and the
  complete text of this comment appear in all copies, and provided that
  the University of Virginia and the original authors are credited in
  any publications arising from the use of this software.

  IN NO EVENT SHALL THE UNIVERSITY OF VIRGINIA OR THE AUTHOR OF THIS
  SOFTWARE BE LIABLE TO ANY PARTY FOR DIRECT, INDIRECT, SPECIAL,
  INCIDENTAL, OR CONSEQUENTIAL DAMAGES, INCLUDING LOST PROFITS, ARISING
  OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN IF THE
  UNIVERSITY OF VIRGINIA AND/OR THE AUTHOR OF THIS SOFTWARE HAVE BEEN
  ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.

  The author of the vdslib software library may be contacted at:

  US Mail:             Dr. David Patrick Luebke
  Department of Computer Science
  Thornton Hall, University of Virginia
  Charlottesville, VA 22903

Phone:               (804)924-1021

EMail:               luebke@cs.virginia.edu

\*****************************************************************************/

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

#include "common.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vmath.h"
#include "vds.h"

#define MAJOR 1
#define MINOR 1

#ifdef INSTRUMENT
#define STAT(x) x
#else
#define STAT(x)
#endif

#ifdef VDS_DEBUGPRINT
#define VDS_DEBUG(x) printf x
#else
#define VDS_DEBUG(x)
#endif

/**
 * @memo	Macros for examining and changing the path to a vdsNode.
 * @name 	Manipulating vdsNodePath structures.
 *
 * 		These preprocessor macros manipulate vdsNodePath, the bit
 *		vector that encapsulates the path from the root of a vertex
 *		tree to a node.<p><dl>
 *
 * <dd>Details:	For a binary tree, each bit represents a branch.  For an 8-way
 *		tree, 3 bits represent a branch.  And so on.  The least
 *		significant bit(s) represent the branch from the root node to
 *		the level-1 node, the next least significant bit(s) represent
 *		the next branch, and so on; the most significant meaningful
 *		bit(s) represent the final branch to the leaf node.  The root
 *		node has a depth of 0 and none of the bits mean anything (but
 *		all must be set to zero, see below).<p>
 * <dd>Note 1:	Defining VDS_64_BITS enforces 64-bit paths.  This enables much
 *		larger vertex trees but may be slower on some architectures.<p>
 * <dd>Note 2: 	For convenience, the depth of the node (and thus the node path
 *		length) is currently stored separately, and all unused bits
 *		must be set to zero (this eases path comparisons).</dl>
 *
 * @see		path.h
 */
/*@{*/

/** Copy one vdsNodePath structure into another.
 * 		Copies the vdsNodePath <b>src</b> into <b>dst</b>.
 */
#define PATH_COPY(dst,src) ((dst) = (src))

/** Look up a particular branch within a vdsNodePath.
 * 		Given a vdsNodePath P and a depth D, returns a branch number
 *		indicating which child of the node at depth D is an ancestor
 *		of the node represented by P.
 */
#define PATH_BRANCH(P,D) (((P)>>((D)*VDS_NUMBITS))&VDS_BITMASK)

/** Set a particular branch within a vdsNodePath.
 * 		Takes a vdsNodePath P representing node N, a depth D,
 *		and a branch number B.  If node A is the ancestor of N at
 *		depth D, sets P to indicate that A->children[B] is the
 *		ancestor of N at depth D+1.<p>
 * Note:	B is assumed to be <= VDS_MAXDEGREE; no checking is done
 */
#define PATH_SET_BRANCH(P,D,B) (P |= ((vdsNodePath)(B)<<((D)*VDS_NUMBITS)))

/*@}*/

/* Routines for maintaining the vertex tree (dynamic.c) */
extern void vdsUnfoldNode(vdsNode *);
extern void vdsFoldNode(vdsNode *);
extern void vdsFoldSubtree(vdsNode *);
/* Routines for rendering the vertex tree (render.c) */
extern void vdsUpdateTriProxies(vdsTri *t);
extern void vdsUpdateTriNormal(vdsTri *t);
/* Routines for building the vertex tree (build.c) */
extern vdsNode *vdsFindNode(vdsNodeId id, vdsNode *root);
extern vdsNodeId vdsFindCommonId(vdsNodeId id1, vdsNodeId id2, int maxdepth);
extern void vdsComputeTriNodes(vdsNode *node, vdsNode *root);

/* Vector macros for the view-dependent simplification package. */
#ifdef __cplusplus
extern "C" {
#endif

#define VEC3_EQUAL(a,b) (NEAR_EQUAL((a)[0],(b)[0], SMALL_FASTF) && NEAR_EQUAL((a)[1],(b)[1], SMALL_FASTF) && NEAR_EQUAL((a)[2],(b)[2],SMALL_FASTF))

#define VEC3_COPY(dst,src) ((dst)[0]=(src)[0], \
	(dst)[1]=(src)[1], \
	(dst)[2]=(src)[2])

#define VEC3_ADD(dst,a,b) 	{(dst)[0]=(a)[0]+(b)[0];\
    (dst)[1]=(a)[1]+(b)[1];\
    (dst)[2]=(a)[2]+(b)[2];}


#define VEC3_SUBTRACT(dst,a,b) {(dst)[0]=(a)[0]-(b)[0];\
    (dst)[1]=(a)[1]-(b)[1];\
    (dst)[2]=(a)[2]-(b)[2];}

#define VEC3_SCALE(dst,scal,vec) {(dst)[0]=(vec)[0]*(scal);\
    (dst)[1]=(vec)[1]*(scal);\
    (dst)[2]=(vec)[2]*(scal);}

#define VEC3_AVERAGE(dst,a,b) {(dst)[0]=((a)[0]+(b)[0])/2.0;\
    (dst)[1]=((a)[1]+(b)[1])/2.0;\
    (dst)[2]=((a)[2]+(b)[2])/2.0;}

#define VEC3_DOT(_v0, _v1) ((_v0)[0] * (_v1)[0] +    \
	(_v0)[1] * (_v1)[1] +    \
	(_v0)[2] * (_v1)[2])

#define VEC3_CROSS(dst,a,b) {(dst)[0]=(a)[1]*(b)[2]-(a)[2]*(b)[1];\
    (dst)[1]=(a)[2]*(b)[0]-(a)[0]*(b)[2];\
    (dst)[2]=(a)[0]*(b)[1]-(a)[1]*(b)[0];}

#define VEC3_LENGTH_SQUARED(v) ((v)[0]*(v)[0] + (v)[1]*(v)[1] + (v)[2]*(v)[2])

#define VEC3_LENGTH(v) (sqrt(VEC3_LENGTH_SQUARED(v)))

#define VEC3_DISTANCE_SQUARED(a,b) (((a)[0]-(b)[0])*((a)[0]-(b)[0]) +	\
	((a)[1]-(b)[1])*((a)[1]-(b)[1]) +	\
	((a)[2]-(b)[2])*((a)[2]-(b)[2]))

#define VEC3_DISTANCE(a,b) (sqrt(VEC3_DISTANCE_SQUARED((a),(b))))

#define VEC3_NORMALIZE(v) {static vdsFloat _n;				\
    _n = 1.0/sqrt((v)[0]*(v)[0] +		\
	    (v)[1]*(v)[1] +		\
	    (v)[2]*(v)[2]);		\
    (v)[0]*=_n;					\
    (v)[1]*=_n;					\
    (v)[2]*=_n;}

#define VEC3_FIND_MAX(dst,a,b) {(dst)[0]=(a)[0]>(b)[0]?(a)[0]:(b)[0];\
    (dst)[1]=(a)[1]>(b)[1]?(a)[1]:(b)[1];\
    (dst)[2]=(a)[2]>(b)[2]?(a)[2]:(b)[2];}

#define VEC3_FIND_MIN(dst,a,b) {(dst)[0]=(a)[0]<(b)[0]?(a)[0]:(b)[0];\
    (dst)[1]=(a)[1]<(b)[1]?(a)[1]:(b)[1];\
    (dst)[2]=(a)[2]<(b)[2]?(a)[2]:(b)[2];}

#define BYTE3_EQUAL(a,b) ((a)[0]==(b)[0]&&(a)[1]==(b)[1]&&(a)[2]==(b)[2])

#define BYTE3_COPY(dst,src) ((dst)[0]=(src)[0], \
	(dst)[1]=(src)[1], \
	(dst)[2]=(src)[2])

#ifdef __cplusplus
}
#endif

/** Initializes the builder.
 * Note:	Currently nothing to do here.
 */
void vdsBeginVertexTree(struct vdsState *s)
{
    s->openflag = 1;
}

/** Initialize the geometry structures.
 * 		Must be called before calling vdsAddNode() and vdsAddTri(),
 *		and must be followed by vdsEndGeometry().
 */
void vdsBeginGeometry(struct vdsState *s)
{
    s->maxnodes = 1024;
    s->nodearray = (vdsNode *) calloc(s->maxnodes, sizeof(vdsNode));
    s->vdsNumnodes = 0;
    s->maxtris = 1024;
    s->triarray = (vdsTri *) calloc(s->maxtris, sizeof(vdsTri));
    s->vdsNumtris = 0;
    s->curroffset = 0;
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
vdsNode *vdsEndGeometry(struct vdsState *s)
{
    /* Tighten nodearray and triarray to free up unused memory */
    s->nodearray = (vdsNode *) realloc(s->nodearray, s->vdsNumnodes * sizeof(vdsNode));
    s->triarray = (vdsTri *) realloc(s->triarray, s->vdsNumtris * sizeof(vdsTri));
    return s->nodearray;
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
vdsNode *vdsAddNode(struct vdsState *s, vdsFloat x, vdsFloat y, vdsFloat z)
{
    s->nodearray[s->vdsNumnodes].coord[0] = x;
    s->nodearray[s->vdsNumnodes].coord[1] = y;
    s->nodearray[s->vdsNumnodes].coord[2] = z;
    s->vdsNumnodes++;
    /* Resize array if necessary */
    if (s->vdsNumnodes == s->maxnodes) {
	vdsNode *tmparray = (vdsNode *) calloc(s->maxnodes * 2, sizeof(vdsNode));

	memcpy(tmparray, s->nodearray, s->vdsNumnodes * sizeof(vdsNode));
	free(s->nodearray);
	s->nodearray = tmparray;
	s->maxnodes *= 2;
    }
    return &s->nodearray[s->vdsNumnodes - 1];
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
vdsTri *vdsAddTri(struct vdsState *s, int v0, int v1, int v2,
	vdsVec3 n0, vdsVec3 n1, vdsVec3 n2,
	vdsByte3 c0, vdsByte3 c1, vdsByte3 c2)
{
    s->triarray[s->vdsNumtris].corners[0].index = v0 + s->curroffset;
    s->triarray[s->vdsNumtris].corners[1].index = v1 + s->curroffset;
    s->triarray[s->vdsNumtris].corners[2].index = v2 + s->curroffset;
    VEC3_COPY(s->triarray[s->vdsNumtris].normal[0], n0);
    VEC3_COPY(s->triarray[s->vdsNumtris].normal[1], n1);
    VEC3_COPY(s->triarray[s->vdsNumtris].normal[2], n2);
    BYTE3_COPY(s->triarray[s->vdsNumtris].color[0], c0);
    BYTE3_COPY(s->triarray[s->vdsNumtris].color[1], c1);
    BYTE3_COPY(s->triarray[s->vdsNumtris].color[2], c2);
    s->vdsNumtris ++;
    /* Resize array if necessary */
    if (s->vdsNumtris == s->maxtris) {
	vdsTri *tmparray = (vdsTri *) calloc(s->maxtris * 2, sizeof(vdsTri));

	memcpy(tmparray, s->triarray, s->vdsNumtris * sizeof(vdsTri));
	free(s->triarray);
	s->triarray = tmparray;
	s->maxtris *= 2;
    }
    return &s->triarray[s->vdsNumtris - 1];
}

/** Cluster a set of nodes under a single new node.
 * 		Attaches the given list of nodes to a new parent node, with
 *		a representative vertex at the specified coordinates.
 * @param 	nnodes 	The number of nodes being merged
 * @param	nodes 	Nodes being merged (it is an error if any of these
 *			nodes already has a parent node)
 * @param	x  The X coordinate of the new node's repvert
 * @param	y  The Y coordinate of the new node's repvert
 * @param	z  The Z coordinate of the new node's repvert
 * @return	A pointer to the newly created parent node. <b>NOTE</b>:
 *		This pointer is only valid until the vdsEndVertexTree() call.
 */
vdsNode *vdsClusterNodes(int nnodes, vdsNode **nodes,
	vdsFloat x, vdsFloat y, vdsFloat z)
{
    int i;
    vdsNode *parent = (vdsNode *) calloc(1, sizeof(vdsNode));

    parent->children = nodes[0];
    parent->children->parent = parent;
    for (i = 1; i < nnodes; i++) {
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
 * Function:	verifyRootedTree
 * Description:	Verifies that all the leaf nodes in nodearray belong to a
 *		tree rooted at root.  Must be called after assignNodeIds().
 */
static void verifyRootedTree(struct vdsState *s, vdsNode *root)
{
    int i;
    vdsNode *node;

    for (i = 0; i < s->vdsNumnodes; i++) {
	node = &s->nodearray[i];
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
static void computeSubtris(struct vdsState *s, vdsNode *root)
{
    int i;

    for (i = 0; i < s->vdsNumtris; i++) {
	vdsTri *t = &s->triarray[i];
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

    if (!N || !N->children)
	return NULL;

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
vdsNode *vdsEndVertexTree(struct vdsState *s)
{
    vdsNode *root;
#ifdef VDS_DEBUGPRINT
    vdsNodeId rootId = {0, 0};
#endif
    vdsNodeId *ids;
    int i;
    int have_parent = 0;

    root = &s->nodearray[0];
    while (root->parent != NULL) {
	root = root->parent;
	have_parent = 1;
    }

    VDS_DEBUG(("Assigning node ids..."));
    ids = (vdsNodeId *) calloc(s->vdsNumnodes, sizeof(vdsNodeId));
#ifdef VDS_DEBUGPRINT
    int maxdepth = assignNodeIds(root, rootId, ids);
#endif
    VDS_DEBUG(("Done.\n"));
    VDS_DEBUG(("Verifying that all nodes form a single rooted tree..."));
    verifyRootedTree(s, root);
    VDS_DEBUG(("Done.\n"));
    VDS_DEBUG(("Assigning node->bound for all nodes..."));
#if 0
    /* This is a very expensive operation. Only calculate bounds if
     * you actually intend to use them.
     */
    assignNodeBounds(root);
#endif
    VDS_DEBUG(("Done.\n"));
    VDS_DEBUG(("Converting triangles to reference nodes by ID..."));
    /* Convert tris to reference node IDs, rather than indices */
    for (i = 0; i < s->vdsNumtris; i++) {
	vdsTri *T = &s->triarray[i];
	int c0, c1, c2;

	c0 = T->corners[0].index;
	c1 = T->corners[1].index;
	c2 = T->corners[2].index;

	T->corners[0].id = ids[c0];
	T->corners[1].id = ids[c1];
	T->corners[2].id = ids[c2];
    }
    free(ids);
    VDS_DEBUG(("Done.\n"));
    VDS_DEBUG(("Computing subtris..."));
    computeSubtris(s, root);
    VDS_DEBUG(("Done.\n"));
    VDS_DEBUG(("Reallocating nodes and copying triangles into node->subtris "
		"fields..."));
    root = moveTrisToNodes(root);	/* Note: reallocates all nodes	    */
    if (have_parent)
	free(s->nodearray);
    VDS_DEBUG(("Done.\n"));
    VDS_DEBUG(("Computing triangle container nodes..."));
    vdsComputeTriNodes(root, root);
    free(s->triarray);			/* nodearray still holds leaf nodes */
    root->status = Boundary;		/* root initially on boundary	    */
    root->next = root->prev = root;	/* root always on boundary path	    */
#ifdef VDS_DEBUGPRINT
    VDS_DEBUG(("Done.\nVertex tree complete! Maximum depth = %d\n", maxdepth));
#endif
    s->openflag = 0;
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
/*
 * File:	cluster.c
 * Description:	Routines for building a VDS vertex tree using a VERY simple
 *		octree-based clustering scheme.
 */

/*
 * Function:	vdsClusterOctree
 * Description:	Builds an octree over the given leaf nodes using
 *		vdsClusterNodes().  Takes an array <nodes> of vdsNode pointers
 *		that represent vertices in the original model (i.e., leaf nodes
 *		in the vertex tree to be generated).  This array is partitioned
 *		into eight subarrays by splitting across the x, y, and z
 *		midplanes of the tightest-fitting bounding cube, and
 *		vdsClusterOctree() is called recursively on each subarray.
 *		Finally, vdsClusterNodes() is called on the 2-8 nodes returned
 *		by these recursive calls, and vdsClusterOctree returns the newly
 *		created internal node.
 */
vdsNode *vdsClusterOctree(vdsNode **nodes, int nnodes, int depth)
{
    vdsNode *thisnode;
    vdsNode *children[8] = {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};
    int nchildren = 0;
    vdsNode **childnodes[8];
    int nchildnodes[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    int i, j;
    vdsVec3 min, max, center, average = {0, 0, 0};

    /* Overestimate array size needs for childnodes now; shrink later */
    for (i = 0; i < 8; i++) {
	childnodes[i] = (vdsNode **) malloc(sizeof(vdsNode *) * nnodes);
    }
    /* Find the min and max bounds of nodes, and accumulate average coord */
    VEC3_COPY(min, nodes[0]->coord);
    VEC3_COPY(max, nodes[0]->coord);
    for (i = 0; i < nnodes; i++) {
	for (j = 0; j < 3; j++) {
	    if (nodes[i]->coord[j] > max[j]) {
		max[j] = nodes[i]->coord[j];
	    }
	    if (nodes[i]->coord[j] < min[j]) {
		min[j] = nodes[i]->coord[j];
	    }
	}
	VEC3_ADD(average, average, nodes[i]->coord);
    }
    VEC3_SCALE(average, 1.0 / (float) nnodes, average);
    VEC3_AVERAGE(center, min, max);
    if (VEC3_EQUAL(min, max)) {
	/* vertices coincide, just partition evenly among children */
	for (i = 0; i < nnodes; i++) {
	    int index = i % VDS_MAXDEGREE;
	    childnodes[index][nchildnodes[index]] = nodes[i];
	    nchildnodes[index] ++;
	}
    } else {
	/* Partition the nodes among the 8 octants defined by min and max */
	for (i = 0; i < nnodes; i++) {
	    int whichchild = 0;
	    if (nodes[i]->coord[0] > center[0]) {
		whichchild |= 1;
	    }
	    if (nodes[i]->coord[1] > center[1]) {
		whichchild |= 2;
	    }
	    if (nodes[i]->coord[2] > center[2]) {
		whichchild |= 4;
	    }

	    childnodes[whichchild][nchildnodes[whichchild]] = nodes[i];
	    nchildnodes[whichchild] ++;
	}
    }
    /* Resize childnodes arrays to use only as much space as necessary */
    for (i = 0; i < 8; i++) {
	childnodes[i] = (vdsNode **)
	    realloc(childnodes[i], sizeof(vdsNode *) * nchildnodes[i]);
    }
    /* Recurse or store non-empty children */
    for (i = 0; i < 8; i++) {
	if (nchildnodes[i] > 0) {
	    if (nchildnodes[i] == 1) {	/* 1 node in octant; store directly  */
		children[nchildren] = childnodes[i][0];
	    } else {		/* 2 or more nodes in octant; recurse*/
		children[nchildren] =
		    vdsClusterOctree(childnodes[i], nchildnodes[i], depth + 1);
	    }
	    nchildren++;
	}

    }
    /* Finally, cluster nonempty children; this node is the resulting parent */
    thisnode = vdsClusterNodes(nchildren, children,
	    average[0], average[1], average[2]);
    for (i = 0; i < 8; i++) {
	if (nchildnodes[i]) {
	    free(childnodes[i]);
	}
    }
    return thisnode;
}
/**
 * @memo	Routines for controlling simplification at run-time.
 * @name 	Dynamic vertex tree maintenance
 *
 * These routines form the heart of the run-time view-dependent
 * simplification processs.  They maintain the vertex tree dynamically at
 * render time, as opposed to statically during the tree-building
 * preprocess.<p>
 *
 * The VDS vertex tree spans the entire model, organizing every vertex of
 * every polygon into one global hierarchy encoding all simplifications VDS
 * can produce.  Leaf nodes each represent a single vertex of the original
 * model; internal nodes represent the merging of multiple vertices from
 * the original model into a single vertex called the <i>proxy</i>.  A
 * proxy is associated with each node in the vertex tree.<p>
 *
 * <i>Folding</i> a node merges the vertices supported by the node into its
 * proxy, and <i>unfolding</i> a node reverses the process.  Folding and
 * unfolding a node always affects certain triangles.  One set of triangles
 * will change in shape as a corner shifts during fold and unfold
 * operations.  Another set of triangles, called the node's <i>subtris</i>,
 * will disappear when the node is folded and reappear when the node is
 * unfolded. Since subtris do not depend on the state of other nodes in the
 * vertex tree, they can be computed offline and accessed very quickly at
 * runtime.  This is the key observation behind VDS.<p>
 *
 * Unfolded nodes are labeled <i>Active</i>; folded nodes are labeled
 * <i>Inactive</i>.  During operation the active nodes constitute a cut of
 * the vertex tree, rooted at the root node, called the <i>active tree</i>.
 * Folded nodes with active parents are a special case; these nodes form
 * the boundary of the active tree and are labeled <i>Boundary</i> (Figure
 * 6).  Since the location of the boundary nodes determines which vertices
 * in the original model have been collapsed together, the path of the
 * boundary nodes across the vertex tree completely determines the current
 * simplification.  <p>
 *
 * To fold a node, use \Ref{vdsFoldNode}(); to unfold a node, use
 * \Ref{vdsUnfoldNode}().  Note that by definition, only Boundary nodes can
 * be unfolded and only Active nodes whose children are all labeled
 * boundary can be folded.  For convenience, the \Ref{vdsFoldSubtree}()
 * function recursively folds a given node after first folding all nodes in
 * the subtree rooted below that node.<p>
 *
 * Usually, the VDSlib user will want to examine all nodes in the vertex
 * tree, applying some criterion to each node to decide whether the node
 * should be folded or unfolded.  The \Ref{vdsAdjustTreeTopDown}() call
 * traverses a vertex tree depth-first, applying a user-specified callback
 * function to determine whether to fold, unfold, or leave each node.  <p>
 *
 * Since all folds and unfolds by definition occur at Boundary nodes, it
 * is also possible to simply walk across the active boundary, applying
 * the fold criterion to whether to unfold a Boundary node, fold its
 * parent, or leave both nodes.  The \Ref{vdsAdjustTreeBoundary}() call
 * does just this; the boundary path across the vertex tree is maintained
 * as a doubly-linked list of nodes.  If the active boundary is not
 * expected to change much from call to call, this function is more
 * efficient than \Ref{vdsAdjustTreeTopDown}().<p>
 *
 * @see dynamic.c */
/*@{*/

/* Remove a triangle from the scene.
 *
 *		Remove a triangle from the vistris list of its containing node.
 *
 *		Note that I currently maintain vistris as a doubly-linked list
 *		for simplicity.  Eventually I should use arrays with garbage
 *		collection, or a similar scheme, for better memory coherence.
 *
 * @param        tri     The triangle to remove.
 */
static void removeTri(vdsTri *tri)
{
    vdsNode *node = tri->node;

    if (node->vistris == tri) {
	node->vistris = tri->next;	/* Note that tri->next may be NULL */
    }
    if (tri->prev) {
	tri->prev->next = tri->next;
    }
    if (tri->next) {
	tri->next->prev = tri->prev;
    }
}

/* Add a triangle to the scene.
 *
 *  		Add the specified triangle to its containing node's list
 *		of visible tris.
 *
 * @see 	See the note on removeTri().
 *
 * @param 	tri     the triangle to add
 */
static void addTri(vdsTri *tri)
{
    vdsNode *node = tri->node;

    if (node->vistris == NULL) {
	node->vistris = tri;
	tri->prev = tri->next = NULL;
    } else {
	tri->prev = NULL;
	tri->next = node->vistris;
	tri->next->prev = tri;
	node->vistris = tri;
    }
}

/** Fold the given node.
 *
 * Given a node N, collapses the representative vertices of the children
 * of N to the representative vertex of N.  This removes some triangles
 * from the scene (namely, N->subtris).  N is assumed to lie just above the
 * active boundary before the foldNode() operation, and lies on it
 * afterwards.<p>
 *
 * Requires:  All of N's child nodes must be labeled Boundary.  If this is
 * not the case, use vdsFoldSubtree().<p>
 *
 * Implementation note: The active boundary is maintained as a circular
 * linked list which always starts at the root, goes to the leftmost node
 * labeled * Boundary, threads through the active boundary of the vertex
 * tree to the rightmost Boundary node, and returns to the root.
 *
 * @param		node 		The node to be folded.
 */
void vdsFoldNode(vdsNode *node)
{
    vdsNode *child;
    vdsNode *prev, *next;
    vdsTri *t;
    int i;

    if (!node || !node->children)
	return;

    /* Activate node and deactivate children */
    node->status = Boundary;
    child = node->children;
    prev = child->prev;			/* Set prev to first child's prev */
    while (child != NULL) {
	child->status = Inactive;
	next = child->next;		/* Set next to last child's next */
	child = child->sibling;		/* Advance to next child	 */
    }
    /* Adjust active boundary */
    node->prev = prev;
    node->next = next;
    node->prev->next = node;
    node->next->prev = node;
    /* Remove subtris from the scene */
    for (i = 0; i < node->nsubtris; i++) {
	t = &node->subtris[i];
	removeTri(t);
    }
}

/** Unfold the given node.
 *
 * The dual operation of vdsFoldNode(), expanding the
 * representative vertex of the given node N to the
 * representative vertices of N's children.  This adds
 * some triangles to the scene (N->subtris).  N must lie
 * on the active boundary, which will be adjusted to follow
 * N's child nodes.
 *
 * @see		See note on vdsFoldNode()
 *
 * @param 	node 	The node to be unfolded.
 */
void vdsUnfoldNode(vdsNode *node)
{
    vdsNode *child = node->children;
    vdsNode *prev, *next;
    vdsTri *t;
    int i;

    if (child == NULL) {
	return;    /* Node is a leaf, leave on boundary */
    }

    /* Activate node children; deactivate node; maintain active boundary */
    prev = node->prev;
    next = node->next;
    while (child != NULL) {
	child->prev = prev;
	prev->next = child;
	prev = child;

	child->status = Boundary;
	child = child->sibling;			/* Advance to next child */
    }
    prev->next = next;
    next->prev = prev;
    node->status = Active;
    /* Add node->subtris to scene */
    for (i = 0; i < node->nsubtris; i++) {
	t = &node->subtris[i];
	addTri(t);
    }
}

/** Fold the entire subtree rooted at the given node.
 *
 * Recursively fold the subtree rooted at a given node N.  If
 * any child C of N is not labeled Boundary, recursively fold
 * the subtree rooted at C.  After folding all descendents,
 * fold N.  N must lie above the active boundary; that is, N must
 * be labeled Active.
 *
 * @param	node	Root of the subtree to be folded.
 * @see 	vdsFoldNode(), vdsUnfoldAncestors()
 */
void vdsFoldSubtree(vdsNode *node)
{
    vdsNode *child = node->children;

    while (child != NULL) {
	if (child->status == Active) {
	    vdsFoldSubtree(child);
	}
	child = child->sibling;			/* Advance to next child */
    }
    /* All children should now be labeled Boundary */
    vdsFoldNode(node);
}

/** Unfold the given node, recursively unfolding the node's ancestors first
 *  if necessary.
 *
 * The dual operation of \Ref{vdsFoldSubtree}.  Recursively unfold all
 * ancestors of the given node N, so that N lies on the active boundary,
 * then unfold N.  N must lie below the active boundary; that is, N must
 * be labeled Inactive.
 *
 * @param 	node	The node to be recursively unfolded.
 * @see		vdsUnfoldNode(), vdsFoldSubtree()
 */
void vdsUnfoldAncestors(vdsNode *node)
{
    vdsNode *parent = node->parent;

    if (parent != NULL) {
	if (parent->status == Boundary) {
	    vdsUnfoldNode(parent);
	} else {
	    vdsUnfoldAncestors(parent);
	}
    }
    /* We've unfolded the parent, so node should now be on the boundary */
    vdsUnfoldNode(node);
}

/** Traverse given vertex tree top-down, adjusting the boundary.
 *
 * Given the root node of a vertex tree, traverses the tree
 * in a top-down fashion, using a user-specified function to
 * determine whether to fold or unfold nodes.
 *
 * @param	node 		the root of the (sub)tree to be adjusted
 * @param	foldtest	returns 1 if node should be folded, 0 otherwise
 * @param	udata           user data 
 */
void vdsAdjustTreeTopDown(vdsNode *node, vdsFoldCriterion foldtest, void *udata)
{
    vdsNode *child = node->children;

    if (node->status == Active) {
	if (foldtest(node, udata) == 1) {
	    vdsFoldSubtree(node);
	    return;
	}
    } else { /* node->status == Boundary */
	if (foldtest(node, udata) == 0) {
	    vdsUnfoldNode(node);
	} else {
	    return;
	}
    }

    while (child != NULL) {
	/* Can't fold or unfold leaf nodes */
	if (child->children != NULL) {
	    vdsAdjustTreeTopDown(child, foldtest, udata);
	}
	child = child->sibling;
    }
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
/**
 * @memo	Routines for rendering the vertex tree.
 * @name 	Rendering the current simplification
 *
 * 		These routines traverse a vdsNode tree, using user-specified
 *		callbacks to cull and render the active triangle list
 *		associated with each node.<p>
 *
 * @see		render.c
 */
/*@{*/

/*
 * Function:	firstActiveAncestor
 * Description:	Takes a node N representing a vertex in the original model,
 *		specified as a vdsNodeId, and returns the first active
 *		ancestor of N, i.e., the first ancestor of N on or above
 *		the active boundary. Note that this need not be a proper
 * 		ancestor; a node may be its own FAA.  To exploit temporal
 *		coherence, also takes a node "proxy" which was recently N's
 *		FAA.  The search for the new FAA starts at proxy, moving up
 *		or down as necessary.
 */
static vdsNode *firstActiveAncestor(vdsNodeId id, vdsNode *proxy)
{
    if (proxy->status == Inactive) {
	do {
	    proxy = proxy->parent;
	} while (proxy->status == Inactive);
	return proxy;	/* proxy->status is now Boundary */
    } else {
	while (proxy->status != Boundary) {
	    /* proxy->status is Active; walk down id->path to find Boundary */
	    register int whichchild = PATH_BRANCH(id.path, proxy->depth);

	    proxy = proxy->children;
	    while (whichchild > 0) {
		proxy = proxy->sibling;
		whichchild --;
	    }
	}
	return proxy;	/* proxy->status is now Boundary */
    }
}

/** Update tri->proxies (<b>must</b> be called by rendering callbacks).
 * 		Checks each corner of the given tri to see if the node
 *		representing the corner has been folded or unfolded, and
 *		updates the node->proxy field if so.<p>
 *
 * Note:	THIS FUNCTION MUST BE CALLED ON EACH TRI BY THE RENDERING
 *		CALLBACK.  Users writing custom rendering callbacks take note!
 */
void vdsUpdateTriProxies(vdsTri *t)
{
    if (t->proxies[0]->status != Boundary) {
	t->proxies[0] = firstActiveAncestor(t->corners[0].id, t->proxies[0]);
    }
    if (t->proxies[1]->status != Boundary) {
	t->proxies[1] = firstActiveAncestor(t->corners[1].id, t->proxies[1]);
    }
    if (t->proxies[2]->status != Boundary) {
	t->proxies[2] = firstActiveAncestor(t->corners[2].id, t->proxies[2]);
    }
}

/** Traverses the vertex tree, culling and rendering nodes according to
 *  user-specified callbacks.
 * 		Traverses the vdsNode tree, calling a user-specified
 *		function to render the active triangle list stored with
 *		each node (via the node->vistris linked list).
 *		Maintaining multiple lists (one per node) allows for
 *		visibility culling, again via a user-specified callback.
 *		This callback should take a vdsNode and return 0 (the node
 *		is completely invisible), 1 (the node may be partially
 *		visible), or 2 (the node is completely visible).<p>
 * <b>Note:</b>	Culling is only done down to level VDS_CULLDEPTH of the
 *		tree, and only nodes at or above VDS_CULLDEPTH have an
 *		associated vistri list.  There ought to be a more elegant
 *		way to indicate whether a node is cullable.  A 1-bit flag?
 * @param	node 	The vdsNode being considered for rendering.
 *		render	The user-specified function for rendering a node.
 *		cull	The user-specifed function for checking the visibility
 *		        of a node.  NULL if the node is to be assumed visible.
 */
void vdsRenderTree(vdsNode *node, vdsRenderFunction render,
	vdsVisibilityFunction visible, void *udata)
{
    vdsNode *child;

    if (visible != NULL) {
	int visibility = visible(node);
	if (visibility == 0) {
	    /* Node invisible, return */
	    return;
	} else if (visibility == 2) {
	    /* Node completely visible, turn off visibility checking */
	    visible = NULL;
	}
    }
    /* If we got this far, node is visible.  Render it */
    render(node, udata);
    /* If node is at VDS_CULLDEPTH, children have no vistris, so can return */
    if (node->depth == VDS_CULLDEPTH) {
	return;
    }
    child = node->children;
    while (child != NULL) {
	vdsRenderTree(child, render, visible, udata);
	child = child->sibling;
    }
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
/**
 * @memo	Debugging and utility routines used throughout VDSlib.
 * @name	General-purpose utility routines
 *
 * 		Various debugging and utility functions used throughout the
 * 		view-dependent simplification library.  Basic users of the
 * 		library will probably only care about functions such as
 * 		\Ref{vdsFreeTree}().<p>
 *
 * @see util.c */
/*@{*/

/** Find a node from its vdsNodeId.
 * 		Given a node id and the vertex tree root node, returns a
 *		pointer to the node
 */
vdsNode *vdsFindNode(vdsNodeId id, vdsNode *root)
{
    vdsNode *tmp = root;
    int tmpdepth = 0;

    while (tmpdepth < id.depth) {
	register int whichchild = PATH_BRANCH(id.path, tmpdepth);

	tmp = tmp->children;
	while (whichchild > 0) {
	    tmp = tmp->sibling;
	    --whichchild;
	}
	++tmpdepth;
    }
    return tmp;
}

/** Find the common ancestor of two nodes, specified by vdsNodeID.
 * 		Given two node IDs, returns the ID of the first common
 *		ancestor of the two nodes.  Strictly based on manipulation
 *		of the IDs; no node structures are actually used, so the
 *		nodes in question need not be present in memory.
 * @param	id1 		The ID of the first node.
 * @param	id2 		The ID of the second node.
 * @param	maxdepth	If the nodes share a common ancestor with
 *				depth greater than maxdepth, the common
 *				ancestor with depth=maxdepth is returned.
 */
vdsNodeId vdsFindCommonId(vdsNodeId id1, vdsNodeId id2, int maxdepth)
{
    vdsNodeId common = {0, 0};
    int i, maxcommondepth;
    vdsNodePath bitmask = VDS_BITMASK;

    maxcommondepth = (id1.depth < id2.depth) ? id1.depth : id2.depth;
    maxcommondepth = (maxdepth < maxcommondepth) ? maxdepth : maxcommondepth;

    common.depth = 0;
    for (i = 0; i < maxcommondepth; i++) {
	if ((id1.path & bitmask) == (id2.path & bitmask)) {
	    /* paths are identical at bits covered by bitmask */
	    common.path |= id1.path & bitmask;
	    common.depth = i + 1;
	    bitmask <<= VDS_NUMBITS;
	} else {
	    break;
	}
    }
    return common;
}

/* Assign container nodes for each triangle.
 * 		For each subtri of node, finds the smallest node
 *		at or above VDS_CULLDEPTH which completely contains the tri.
 *		When N is unfolded, the subtri is added to the node->vistris
 *		list of this smallest node for view-frustum culling.  This
 *		function also assigns all tri->proxies[] to the root node.
 * @param	node 	the node whose subtris are being assigned containers
 * @param	root 	the root of the vertex tree
 * Note:	Must be called after moveTrisToNodes() !
 */
void vdsComputeTriNodes(vdsNode *node, vdsNode *root)
{
    int i;
    vdsTri *t;
    vdsNodeId c0, c1, c2;		/* The 3 corners of the triangle */
    vdsNodeId commonId;
    vdsNode *common;
    vdsNode *child;

    for (i = 0; i < node->nsubtris; i++) {
	t = &node->subtris[i];
	c0 = t->corners[0].id;
	c1 = t->corners[1].id;
	c2 = t->corners[2].id;
	commonId = vdsFindCommonId(c0, c1, VDS_CULLDEPTH);
	commonId = vdsFindCommonId(c2, commonId, VDS_CULLDEPTH);
	common = vdsFindNode(commonId, root);
	t->node = common;
	t->proxies[0] = t->proxies[1] = t->proxies[2] = root;
    }
    child = node->children;
    while (child != NULL) {
	vdsComputeTriNodes(child, root);
	child = child->sibling;
    }
}

/** Gather statistics for a vertex tree.
 * 		Recursively counts the number of nodes, the number of
 *		leaf nodes (i.e., vertices from the original model), and the
 *		number of subtris (i.e., triangles in the original model)
 *		in the subtree rooted at <root>.
 */
void vdsStatTree(vdsNode *root, int *nodes, int *leaves, int *tris)
{
    vdsNode *child;
    int childnodes, childleaves, childtris;

    child = root->children;
    *nodes = *leaves = *tris = 0;
    childnodes = childleaves = childtris = 0;
    /* If the child is internal, recursivily count its children's nodes */
    if (child != NULL) {
	while (child != NULL) {
	    vdsStatTree(child, &childnodes, &childleaves, &childtris);
	    *nodes += childnodes;
	    *leaves += childleaves;
	    *tris += childtris;
	    child = child->sibling;
	}
	/* don't forget to count this node: */
	*nodes += 1;
	*tris += root->nsubtris;
    } else {
	*nodes = 1;
	*leaves = 1;
	/* leaf nodes have no subtris */
    }
}

/** Free all memory used by a vertex tree.
 *             Recursively frees the given VDS vertex tree and all associated
 *             vdsTri structs.<p>
 * <b>Note</b>:        Does NOT free anything pointed at by the node->data field;
 *             this is left to application.
 */
void vdsFreeTree(vdsNode *node)
{
    vdsNode *child = node->children;

    while (child != NULL) {
	vdsNode *sibling = child->sibling;

	vdsFreeTree(child);
	child = sibling;
    }
    /* I think just freeing node will free up all its subtris correctly... */
    free(node);
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
