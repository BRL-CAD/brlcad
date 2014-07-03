/**
 * @memo	Some important macros used to configure VDSlib.
 * @name	VDSlib macros
 *
 * These macros are defined in the main header file for the view-dependent
 * simplification library.  \Ref{VDS_MAXDEGREE} sets the maximum degree of
 * the vertex tree.  For example, a tree built by octree-style clustering
 * has a maximum degree of 8; a tree built by progressive edge-collapse
 * operations has a maximum degree of 2.  At the moment VDS_MAXDEGREE only
 * affects the number of bits used in the vdsNodePath (see
 * \Ref{VDS_64_BITS}), but eventually VDSlib will include special
 * optimizations for binary trees (VDS_MAXDEGREE = 2).  Currently VDSlib
 * only supports setting VDS_MAXDEGREE to 2, 4, or 8.<p>
 *
 * Define \Ref{VDS_DOUBLE_PRECISION} to make all floating-point numbers
 * used by VDSlib (i.e., vdsFloats) double-precision.  This is highly
 * recommended if you are dealing with complex models; on many modern
 * architectures double-precision arithmetic is the faster option anyway.
 *
 * @see vds.h */
/*@{*/

#ifndef _VDS_H
#define _VDS_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef VDS_EXPORT
#  if defined(VDS_DLL_EXPORTS) && defined(VDS_DLL_IMPORTS)
#    error "Only VDS_DLL_EXPORTS or VDS_DLL_IMPORTS can be defined, not both."
#  elif defined(VDS_DLL_EXPORTS)
#    define VDS_EXPORT __declspec(dllexport)
#  elif defined(VDS_DLL_IMPORTS)
#    define VDS_EXPORT __declspec(dllimport)
#  else
#    define VDS_EXPORT
#  endif
#endif

#include <stdio.h>

/** VDS_MAXDEGREE defines the maximum degree of vertex tree nodes	*/
#define VDS_MAXDEGREE 8
/** VDS_CULLDEPTH defines the depth to which the vertex tree is culled	*/
#define VDS_CULLDEPTH 4

/** Define VDS_DOUBLE_PRECISION to make everything double precision */
#define VDS_DOUBLE_PRECISION

#ifdef VDS_DOUBLE_PRECISION
typedef double vdsFloat;
#else
typedef float vdsFloat;
#endif

typedef vdsFloat vdsVec2[2];
typedef vdsFloat vdsVec3[3];
typedef unsigned char vdsByte3[3];

/** Define VDS_64_BITS for 64-bit node paths.
 *
 * A vdsNodePath is a bit vector encapsulating the path from the root of a
 * vertex tree down to a given node.  For a binary tree, each bit represents
 * a branch; for an 8-way tree, 3 bits represent a branch, etc.  Defining
 * VDS_64_BITS enforces 64-bit paths.  This enables much larger vertex trees
 * but may be slower on some architectures.<p>
 *
 * Note: for convenience, the depth of the node (and thus the node path length)
 * is currently stored separately, and all unused bits must be set to zero.
 */
#define VDS_64_BITS		/* don't define for 32-bit node paths   */
#ifdef VDS_64_BITS
typedef unsigned long long vdsNodePath;
#else
typedef unsigned int vdsNodePath;
#endif

/* Label node as Active (unfolded), Inactive (folded), or on the Boundary: */
typedef enum _vdsNodeStatus {
    Boundary = 0,			/* Set Boundary to 0 for speed 	*/
    Inactive,
    Active
} vdsNodeStatus;

/* Bound the triangles supported by a node.
 * 		A bounding volume is associated with each node; the volume
 *		contains all triangles supported by the node.  Currently I
 *		use a bounding sphere for simplicity, but this may be less
 *		than optimal.  Two possible improvements: <ll>
 *		<li>Following Hoppe's "Efficient Implementation of Progressive
 *		    Meshes", center the bounding sphere on node->coord.  This
 *		    saves storing 3 floats per node, at the cost of more
 *		    conservative visibility and screenspace-extent tests.
 *		<li>Use a tighter bounding volume (AABBs? OBBs? Ellipsoids?).
 *		    There is a tradeoff here between storage and accuracy.</ll>
 */
typedef struct _vdsBoundingVolume {
    vdsVec3	center;			/* center of the sphere 	*/
    vdsFloat 	radius;			/* size of the sphere		*/
} vdsBoundingVolume;

typedef void *vdsNodeData;		/* hook for user data		*/

typedef struct _vdsNodeId {
    vdsNodePath	path;			/* path down tree to node	*/
    char 	depth;			/* 1 byte depth info for now	*/
} vdsNodeId;

typedef union {
    vdsNodeId	id;	/* node depth, path down tree	*/
    int		index;	/* leaf node index; used during	vertex tree construction */
} vdsTriCorner;

/*
 * Note on vdsTri: should use Hoppe's "wedge" structures or something similar
 * to avoid replicating colors and normals at every triangle corner.  Would
 * this be hard keep memory-coherent in an out-of-core implementation?
 */
typedef struct _vdsTri {
    vdsTriCorner	corners[3];	/* leaf nodes corresponding to 	*
				     * 3 original corner vertices	*/
    struct _vdsNode	*proxies[3];	/* nodes representing corners;	*
				     * i.e., the FAA of each corner	*/
    struct _vdsNode	*node;		/* smallest node containing tri	*/
    vdsVec3		normal[3];	/* normal for the tri		*/
    vdsByte3		color[3];	/* RGB for the tri's corners	*/
    struct _vdsTri	*next;		/* next in linked vistri list	*/
    struct _vdsTri	*prev;		/* prev in linked vistri list	*/
} vdsTri;

/*
 * Note on vdsNode: Should combine depth and status into a single word,
 * saving 32 bits, and center bounding volume on coord to eliminate
 * a vdsVec3, saving 96-192 bits (depending on VDS_DOUBLE_PRECISION)
 */
typedef struct _vdsNode {
    /* 3 bitfields (32 bits total): */
    char 		depth;		/* depth of node in vertex tree */
    int			nsubtris;	/* size of node->subtris[] array*/
    vdsNodeStatus	status;		/* is node currently active?	*/

    vdsBoundingVolume 	bound;		/* bounding volume of triangles	*
				     * supported by the node	*/
    vdsVec3		coord;		/* coordinate of node's proxy	*/
    vdsTri		*vistris;	/* linked list of visible tris 	*
				     * contained by this node	*/
    vdsNodeData		data;		/* auxiliary node data		*/
    struct _vdsNode	*next;		/* next node in boundary path	*/
    struct _vdsNode	*prev;		/* prev node in boundary path	*/
    struct _vdsNode	*parent;	/* the parent node		*/
    struct _vdsNode 	*children;	/* first of the node's children */
    struct _vdsNode	*sibling;	/* node's next sibling		*/
    /* Array of tris created/deleted if node is folded/unfolded.	*
     * NOTE: Actually a variable-length array!				*/
    vdsTri		subtris[1];
} vdsNode;

/*
 * Callback typedefs
 *
 *	vdsRenderFunction: 	render the given node
 *		Note:			Must call vdsUpdateTriCorners() on tris
 * 	vdsVisibilityFunction: 	determine visibility of the given node
 * 	  	Return codes: 		0: node invisible
 *					1: node potentially visible
 *					2: node & subtree *completely* visible
 * 	vdsFoldCriterion: 	decide whether a node should be folded
 * 	  	Return codes:		0: node should be unfolded
 *					1: node should be folded
 * 	vdsNodeDataWriter:	write node->data contents to a file
 * 	vdsNodeDataReader:	read node->data contents from a file
 *		Returns:		allocated vdsNodeData structure
 */
typedef void (*vdsRenderFunction) (const vdsNode *, void *);
typedef int (*vdsVisibilityFunction) (const vdsNode *);
typedef int (*vdsFoldCriterion) (const vdsNode *, void *);
typedef void (*vdsNodeDataWriter) (FILE *f, const vdsNode *);
typedef vdsNodeData (*vdsNodeDataReader) (FILE *f);

/*
 * Prototypes
 */

/* Routines for maintaining the vertex tree (dynamic.c) */
VDS_EXPORT extern void vdsAdjustTreeBoundary(vdsNode *, vdsFoldCriterion, void *);
VDS_EXPORT extern void vdsAdjustTreeTopDown(vdsNode *, vdsFoldCriterion, void *);
/* Low-level vertex tree maintainance routines; not need by most users: */
VDS_EXPORT extern void vdsFoldNode(vdsNode *);
VDS_EXPORT extern void vdsUnfoldNode(vdsNode *);
VDS_EXPORT extern void vdsFoldSubtree(vdsNode *);

/* Routines for rendering the vertex tree (render.c) */
VDS_EXPORT extern void vdsUpdateTriProxies(vdsTri *t);
VDS_EXPORT extern void vdsRenderTree(vdsNode *node, vdsRenderFunction render,
		  vdsVisibilityFunction visible, void *udata);

/* Routines for building the vertex tree (build.c) */
VDS_EXPORT extern void vdsBeginVertexTree();
VDS_EXPORT extern void vdsBeginGeometry();
VDS_EXPORT extern vdsNode *vdsAddNode(vdsFloat x, vdsFloat y, vdsFloat z);
VDS_EXPORT extern vdsTri *vdsAddTri(int v0, int v1, int v2,
		 vdsVec3 n0, vdsVec3 n1, vdsVec3 n2,
		 vdsByte3 c0, vdsByte3 c1, vdsByte3 c2);
VDS_EXPORT extern void vdsNewObject();
VDS_EXPORT extern vdsNode *vdsEndGeometry();
VDS_EXPORT extern vdsNode *vdsClusterNodes(int nnodes, vdsNode **nodes,
		vdsFloat x, vdsFloat y, vdsFloat z);
VDS_EXPORT extern vdsNode *vdsEndVertexTree();

/* Routines for reading and writing the vertex tree (file.c) */
VDS_EXPORT extern vdsNode *vdsReadTree(FILE *f, vdsNodeDataReader readdata);
VDS_EXPORT extern void vdsWriteTree(FILE *f, vdsNode *root, vdsNodeDataWriter writedata);

/* Assorted useful routines (util.c) */
VDS_EXPORT extern vdsNode *vdsFindNode(vdsNodeId id, vdsNode *root);
VDS_EXPORT extern void vdsPrintNodeId(const vdsNodeId *id);
VDS_EXPORT extern void vdsSprintNodeId(char *str, const vdsNodeId *id);
VDS_EXPORT extern void vdsStatTree(vdsNode *root, int *nodes, int *leaves, int *tris);
VDS_EXPORT extern void vdsFreeTree(vdsNode *node);

/* (cluster.c) */
VDS_EXPORT extern vdsNode *vdsClusterOctree(vdsNode **nodes, int nnodes, int depth);

/*
 * The following macros relate to the maximum degree of the vertex tree,
 * which dictates the maximum depth and the number of bits stored per branch.
 */
#if VDS_MAXDEGREE == 2
#    define VDS_NUMBITS 1
#    define VDS_BITMASK 1
#    ifdef VDS_64_BITS
#	define VDS_MAXDEPTH 64
#    else
#	define VDS_MAXDEPTH 32
#    endif
#elif VDS_MAXDEGREE == 4
#    define VDS_NUMBITS 2
#    define VDS_BITMASK 3
#    ifdef VDS_64_BITS
#	define VDS_MAXDEPTH 32
#    else
#	define VDS_MAXDEPTH 16
#    endif
#elif VDS_MAXDEGREE == 8
#    define VDS_NUMBITS 3
#    define VDS_BITMASK 7
#    ifdef VDS_64_BITS
#	define VDS_MAXDEPTH 21
#    else
#	define VDS_MAXDEPTH 10
#    endif
#else
#    error "Only values of 2, 4, and 8 supported for VDS_MAXDEGREE"
#endif

#ifdef __cplusplus
}
#endif

#endif		/* _VDS_H */

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
