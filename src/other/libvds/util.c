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

#include <string.h>
#include <stdio.h>
#include <math.h>
#include <assert.h>

#include "vds.h"
#include "vector.h"
#include "path.h"

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
	    assert(tmp != NULL);
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
    assert(maxcommondepth <= VDS_MAXDEPTH);

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

/** Print a vdsNodeId to stdout.
 * 		Prints the node ID (depth & path down the vertex tree) in
 *		human-readable form to stdout.
 *
 */
void vdsPrintNodeId(const vdsNodeId *id)
{
    int i;
    vdsNodePath tmppath = id->path;

    if (id->depth == 0) {
	printf("<root>\n");
    } else {
	for (i = 0; i < id->depth; i++) {
	    putchar((char) (tmppath & VDS_BITMASK) + '0');
	    tmppath >>= VDS_NUMBITS;
	}
	putchar('\n');
    }
}

/** Print a vdsNodeID to a string.
 * 		Prints the node ID (depth & path down the vertex tree) in
 *		human-readable form to the given string <b>str</b>.  The caller
 *		must ensure that <b>str</b> points to VDS_MAXDEPTH+1 allocated
 *		bytes.  The returned <b>str</b> is null-terminated.
 */
void vdsSprintNodeId(char *str, const vdsNodeId *id)
{
    int i;
    vdsNodePath tmppath = id->path;

    if (id->depth == 0) {
	strcpy(str, "<root>");
    } else {
	for (i = 0; i < id->depth; i++) {
	    str[i] = (tmppath & VDS_BITMASK) + '0';
	    tmppath >>= VDS_NUMBITS;
	}
	str[i] = '\0';
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

/** Calculate a triangle's current normal.
 * 		Calculates a triangle's normal, say for flat-shading, by
 *		taking the cross product of two of its edges.  Places the
 *		result in <b>normal</b>. <p>
 *
 * <b>Note</b>:	This calculates the normal of the triangle as currently
 *		simplified!  Triangles that don't exist in the current
 *		simplification will produce undefined behavior.
 */
void vdsCalcTriNormal(vdsTri *t, vdsVec3 normal)
{
    vdsVec3 tmp1, tmp2;

    VEC3_SUBTRACT(tmp1, t->proxies[1]->coord, t->proxies[0]->coord);
    VEC3_SUBTRACT(tmp2, t->proxies[2]->coord, t->proxies[0]->coord);
    VEC3_CROSS(normal, tmp1, tmp2);
    VEC3_NORMALIZE(normal);
}

/** Free all memory used by a vertex tree.
 * 		Recursively frees the given VDS vertex tree and all associated
 *		vdsTri structs.<p>
 * <b>Note</b>:	Does NOT free anything pointed at by the node->data field;
 *		this is left to application.
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
