/**
 * @memo	Routines for reading and writing vertex trees.
 * @name 	Vertex tree file I/O
 *
 * These routines provide a mechanism for saving a VDS vertex tree to a
 * terse binary file format, and reading it back.  This file format is a
 * bit rough at the moment.  For example, it does not deal with differences
 * between big-endian and little-endian machines, so files created on one
 * architecture may not run on another architecture.  Expect file I/O to
 * change and improve with following versions of the library.<p>
 *
 * @see file.c */
/*@{*/

#include <stdlib.h>
#include <stdio.h>

#include "vds.h"
#include "vdsprivate.h"
#include "vector.h"

#define MAJOR 1
#define MINOR 1

static void writeNode(FILE *f, vdsNode *node, vdsNodeDataWriter putdata);
static vdsNode *readNode(FILE *f, vdsNodeDataReader getdata);
static vdsNode *readChildren(FILE *f, vdsNode *parent, vdsNodeDataReader get);
static void writeChildren(FILE *f, vdsNode *parent, vdsNodeDataWriter put);

/** Read a vertex tree from a VDS binary file.
 * 		The user may specify a vdsNodeDataReader function
 *		<b>readdata()</b> if the vertex tree was written with
 *		custom data in the node->data field.  If <b>readdata()</b>
 *		is NULL, the node->data field is simply copied
 *		byte-for-byte into memory.
 * @param	f 		The file to be read.
 * @param	readdata	A vdsNodeDataReader function callback to read
 *				the contents of the node->data field, or NULL
 *				to simply read node->data byte-for-byte.
 * @return	A pointer to the root node of the new tree.
 */
vdsNode *vdsReadTree(FILE *f, vdsNodeDataReader readdata)
{
    int major, minor;
    int numnodes, numverts, numtris;
    vdsNode *root, *currentnode, *lastnode, *parent;

    assert(f != NULL);
    if (fscanf(f, "VDS Vertex Tree file format version %d.%d\n",
	       &major, &minor) != 2) {
	fprintf(stderr, "Error reading line 1 of input file.\n");
	exit(1);
    }
    if (fscanf(f, "Total nodes: %d\nVertices: %d\nTriangles: %d\n",
	       &numnodes, &numverts, &numtris) != 3) {
	fprintf(stderr, "Error reading lines 2-4 of input file.\n");
	exit(1);
    }
    /* Read the tree, starting with the root node */
    root = readNode(f, readdata);
    root->parent = NULL;
    root->depth = 0;
    root->children = readChildren(f, root, readdata);
    vdsComputeTriNodes(root, root);
    root->status = Boundary;
    root->next = root->prev = root;
    return root;
}

/*
 * Function:	readChildren
 * Description:	Reads a group of nodes, which are sibling children of <parent>
 *		stored adjacently in a VDS binary file.  Then, recursively
 *		reads THEIR children in depth-first order.
 * Returns: 	The address of the first node in the group of siblings.
 */
static vdsNode *readChildren(FILE *f, vdsNode *parent, vdsNodeDataReader get)
{
    vdsNode *node, *firstnode;

    assert(parent != NULL);
    firstnode = readNode(f, get);
    node = firstnode;
    while (node != NULL) {
	node->parent = parent;
	node->depth = parent->depth + 1;
	/* Unless its NULL, overwrite node->sibling (which is garbage) */
	if (node->sibling != NULL) {
	    node->sibling = readNode(f, get);
	}
	node = node->sibling;
    }
    node = firstnode;
    while (node != NULL) {
	if (node->children != NULL) {
	    node->children = readChildren(f, node, get);
	}
	node = node->sibling;
    }
    return firstnode;
}

/*
 * Function:	readNode
 * Description:	Reads a single node from the VDS binary file, leaving
 *		information like parent and depth to be filled in by the
 *		caller.
 */
static vdsNode *readNode(FILE *f, vdsNodeDataReader getdata)
{
    vdsNode *node;
    int numchildren, numsubtris;

    node = (vdsNode *) calloc(1, sizeof(vdsNode));
    assert(node != NULL);
    node->status = Inactive;
    fread(&node->bound, sizeof(vdsBoundingVolume), 1, f);
    fread(&node->coord, sizeof(vdsVec3), 1, f);
    fread(&numchildren, sizeof(numchildren), 1, f);
    assert(numchildren <= VDS_MAXDEGREE);
    /* if numchildren is 0, set node->children NULL, otherwise to non-NULL */
    node->children = (numchildren == 0 ? NULL : (vdsNode *) 0x1);
    /* Read sibling pointer (actually we just care if it's NULL or not) */
    fread(&node->sibling, sizeof(node->sibling), 1, f);
    /*
     * We can read and write the triangles directly to the binary file, at
     * the expense of re-running vdsComputeTriNodes() afterwards.  Also,
     * writing the entire structure (versus each field separately) means
     * that we are locked to a particular architecture/compiler's memory
     * alignment...not very portable.
     */
    fread(&numsubtris, sizeof(numsubtris), 1, f);
    node->nsubtris = numsubtris;
    node = (vdsNode *) realloc(node, sizeof(vdsNode) +
		       numsubtris * sizeof(vdsTri));
    fread(&node->subtris[0], sizeof(vdsTri), numsubtris, f);
    /* Now get the associated node data if user has provided a function */
    if (getdata == NULL) {
	fread(&node->data, sizeof(vdsNodeData), 1, f);
    } else {
	node->data = getdata(f);
    }
    return node;
}

/** Writes a vertex tree to a VDS binary file.
 * 		Writes a vertex tree to the specified file.  The user may
 *		specify a vdsNodeDataWriter function <b>writedata()</b> if the
 *		vertex tree was written with custom data in the node->data
 *		field.  If <b>writedata()</b> is NULL, the contents of the
 *		node->data field are simply copied byte-for-byte into the file.
 * @param	f 	The file to which the tree is written
 * @param	root		The root of the vertex tree to be written.
 * @param	writedata	A vdsNodeDataReader function callback to read
 *				the contents of the node->data field, or NULL
 *				to simply read node->data byte-for-byte.
 */
void vdsWriteTree(FILE *f, vdsNode *root, vdsNodeDataWriter writedata)
{
    int numnodes, numverts, numtris;
    vdsNode *child;

    assert(f != NULL);
    fprintf(f, "VDS Vertex Tree file format version %d.%d\n", MAJOR, MINOR);
    vdsStatTree(root, &numnodes, &numverts, &numtris);
    fprintf(f, "Total nodes: %d\n", numnodes);
    fprintf(f, "Vertices: %d\n", numverts);
    fprintf(f, "Triangles: %d\n", numtris);
    fflush(f);
    /* Save the root node to disk */
    writeNode(f, root, writedata);
    /* Now recursively save its children, grouping siblings together on disk */
    writeChildren(f, root, writedata);
}

/*
 * Function:	writeChildren
 * Description:	Writes all of the given node's children to disk, then
 *		recursively writes their children in depth-first order.
 */
static void writeChildren(FILE *f, vdsNode *parent, vdsNodeDataWriter put)
{
    vdsNode *child;

    child = parent->children;
    while (child != NULL) {
	writeNode(f, child, put);
	child = child->sibling;
    }
    child = parent->children;
    while (child != NULL) {
	if (child->children != NULL) {
	    writeChildren(f, child, put);
	}
	child = child->sibling;
    }
}

/*
 * Function:	writeNode
 * Description:	Writes the essential fields of a single vdsNode to a
 * VDS binary file.
 */
static void writeNode(FILE *f, vdsNode *node, vdsNodeDataWriter putdata)
{
    vdsNode *child;
    int i, numchildren = 0, numsubtris = node->nsubtris;

    fwrite(&node->bound, sizeof(vdsBoundingVolume), 1, f);
    fwrite(&node->coord, sizeof(vdsVec3), 1, f);
    child = node->children;
    while (child != NULL) {
	numchildren ++;
	child = child->sibling;
    }
    assert(numchildren <= VDS_MAXDEGREE);
    fwrite(&numchildren, sizeof(numchildren), 1, f);
    /* Write the sibling pointer (actually we just care if it's NULL or not) */
    fwrite(&node->sibling, sizeof(node->sibling), 1, f);
    /* Write all the subtris (if any) associated with this node */
    fwrite(&numsubtris, sizeof(numsubtris), 1, f);
    fwrite(&node->subtris[0], sizeof(vdsTri), numsubtris, f);
    /* Now write associated node data using user-supplied function (if any) */
    if (putdata == NULL) {
	fwrite(&node->data, sizeof(vdsNodeData), 1, f);
    } else {
	putdata(f, node);
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
