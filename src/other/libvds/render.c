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

#include <math.h>

#include "vds.h"
#include "vdsprivate.h"
#include "vector.h"
#include "path.h"

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
	    assert(proxy != NULL);
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
		   vdsVisibilityFunction visible)
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
    render(node);
    /* If node is at VDS_CULLDEPTH, children have no vistris, so can return */
    if (node->depth == VDS_CULLDEPTH) {
	return;
    }
    child = node->children;
    while (child != NULL) {
	vdsRenderTree(child, render, visible);
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
