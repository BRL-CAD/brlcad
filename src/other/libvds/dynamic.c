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

#include "vds.h"
#include "vdsprivate.h"
#include "vector.h"

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

    if (node->vistris == tri)
    {
	node->vistris = tri->next;	/* Note that tri->next may be NULL */
    }
    if (tri->prev) { tri->prev->next = tri->next; }
    if (tri->next) { tri->next->prev = tri->prev; }
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

    if (node->vistris == NULL)
    {
	node->vistris = tri;
	tri->prev = tri->next = NULL;
    }
    else
    {
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
    
    assert(node->status == Active);

    /* Activate node and deactivate children */
    node->status = Boundary;
    child = node->children;
    assert(child != NULL);
    prev = child->prev;			/* Set prev to first child's prev */
    while (child != NULL)
    {
	assert(child->status == Boundary);

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
    for (i=0; i < node->nsubtris; i++)
    {
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
    
    assert(node->status == Boundary);

    if (child == NULL) { return; }	/* Node is a leaf, leave on boundary */

    /* Activate node children; deactivate node; maintain active boundary */
    prev = node->prev;
    next = node->next;
    while (child != NULL)
    {
	child->prev = prev;
	prev->next = child; 
	prev = child;

	assert(child->status == Inactive);
	child->status = Boundary;
	child = child->sibling;			/* Advance to next child */
    } 
    prev->next = next;
    next->prev = prev;
    node->status = Active;
    /* Add node->subtris to scene */
    for (i=0;i < node->nsubtris; i++)
    {
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
    
    assert(node->status == Active);
    while (child != NULL)
    {
	if (child->status == Active)
	{
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

    assert(node->status == Inactive);
    if (parent != NULL)
    {
	if (parent->status == Boundary)
	{
	    vdsUnfoldNode(parent);
	}
	else
	{
	    vdsUnfoldAncestors(parent);
	}
    }
    /* We've unfolded the parent, so node should now be on the boundary */
    vdsUnfoldNode(node);
}

/** Adjust the active boundary of the vertex tree.
 *
 * Walks the boundary of the active tree from left to right,
 * calling a user-specified function to determine whether to
 * unfold boundary nodes, lowering the boundary, or fold parents
 * of boundary nodes, raising the boundary. 
 *
 * @param	root 		root of tree whose boundary is to be adjusted
 * @param	foldtest	returns 1 if node should be folded, 0 otherwise
 */
void vdsAdjustTreeBoundary(vdsNode *root, vdsFoldCriterion foldtest)
{
    vdsNode *current;			/* node currently being tested	*/
    vdsNode *parent = NULL;		/* parent of current node	*/
    vdsNode *lastparent = NULL;		/* parent node last iteration	*/
    
    current = root->next;
    do 
    {
	parent = current->parent;
	/*
	 * If we haven't already, test parent: should it be folded?
	 * Note: if current is root node, parent == lastparent == NULL, so
	 * we won't attempt to fold (or even access) the NULL root->parent.
	 */
	if (parent != lastparent)
	{
	    lastparent = parent;
	    /* Test parent: should it be folded? */
	    if (foldtest(parent))
	    {
		vdsFoldSubtree(parent);
		/* Now parent is on the boundary; check it next */
		current = parent;
		continue; 
	    }
	}
	/*
	 * Don't need to fold parent.  If current node can be unfolded,
	 * should we do so, or leave on the boundary?
	 */
	if (current->children != NULL && foldtest(current) == 0)
	{
	    vdsUnfoldNode(current);
	    /* Now node->children are on the boundary; check them */
	    current = current->children;
	}
	else
	{
	    /* No folds or unfolds, move on to next node on the boundary */
	    assert(current->next != NULL);
	    current = current->next;
	}
    }
    while (current != root);
}

/** Traverse given vertex tree top-down, adjusting the boundary.
 *
 * Given the root node of a vertex tree, traverses the tree
 * in a top-down fashion, using a user-specified function to
 * determine whether to fold or unfold nodes.
 *
 * @param	node 		the root of the (sub)tree to be adjusted
 * @param	foldtest() 	returns 1 if node should be folded, 0 otherwise
 */
void vdsAdjustTreeTopDown(vdsNode *node, vdsFoldCriterion foldtest)
{
    vdsNode *child = node->children;

    if (node->status == Active)
    {
	if (foldtest(node) == 1)
	{
	    vdsFoldSubtree(node);
	    return;
	}
    }
    else /* node->status == Boundary */
    {
	if (foldtest(node) == 0)
	{
	    vdsUnfoldNode(node);
	}
	else 
	{
	    return;	
	}
    }

    while (child != NULL)
    {
	/* Can't fold or unfold leaf nodes */
	if (child->children != NULL)	
	{
	    vdsAdjustTreeTopDown(child, foldtest);
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
