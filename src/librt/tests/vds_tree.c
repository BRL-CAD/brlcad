/*                        V D S _ T R E E . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

#include "common.h"

#include "bu/app.h"
#include "bu/log.h"
#include "vds.h"


static int rendered_nodes = 0;
static int rendered_tris = 0;
static int invalid_proxies = 0;


static int
never_fold(const vdsNode *UNUSED(node), void *UNUSED(data))
{
    return 0;
}


static void
count_rendered(const vdsNode *node, void *UNUSED(data))
{
    vdsTri *tri = node->vistris;

    rendered_nodes++;
    while (tri != NULL) {
	vdsUpdateTriProxies(tri);
	if (tri->proxies[0] == NULL || tri->proxies[1] == NULL ||
	    tri->proxies[2] == NULL)
	    invalid_proxies++;
	rendered_tris++;
	tri = tri->next;
    }
}


static int
validate_subtree(const vdsNode *node, const vdsNode *parent, int depth,
	int *node_count, int *leaf_count)
{
    const vdsNode *child;
    int child_count = 0;
    int failures = 0;

    if (node == NULL)
	return 1;

    (*node_count)++;
    if (node->parent != parent) {
	bu_log("VDS node at depth %d has an incorrect parent\n", depth);
	failures++;
    }
    if (node->depth != depth) {
	bu_log("VDS node depth is %d, expected %d\n", node->depth, depth);
	failures++;
    }

    for (child = node->children; child != NULL; child = child->sibling) {
	child_count++;
	failures += validate_subtree(child, node, depth + 1,
		node_count, leaf_count);
    }
    if (child_count == 0)
	(*leaf_count)++;
    if (child_count > VDS_MAXDEGREE) {
	bu_log("VDS node has %d children, maximum is %d\n",
		child_count, VDS_MAXDEGREE);
	failures++;
    }

    return failures;
}


int
main(int argc, char **argv)
{
    static const vdsFloat vertices[4][3] = {
	{0.0, 0.0, 0.0},
	{1.0, 0.0, 0.0},
	{0.0, 1.0, 0.0},
	{0.0, 0.0, 1.0}
    };
    static const int faces[4][3] = {
	{0, 2, 1},
	{0, 1, 3},
	{0, 3, 2},
	{1, 2, 3}
    };
    struct vdsState state = VDS_STATE_INIT_ZERO;
    vdsVec3 normal = {1.0, 0.0, 0.0};
    vdsByte3 color = {255, 0, 0};
    vdsNode *nodes[4];
    vdsNode *leaves;
    vdsNode *root;
    int reported_nodes = 0;
    int reported_leaves = 0;
    int reported_tris = 0;
    int visited_nodes = 0;
    int visited_leaves = 0;
    int failures = 0;
    int i;

    bu_setprogname(argv[0]);
    if (argc != 1) {
	bu_log("Usage: %s\n", argv[0]);
	return 1;
    }

    vdsBeginVertexTree(&state);
    vdsBeginGeometry(&state);
    for (i = 0; i < 4; i++)
	vdsAddNode(&state, vertices[i][0], vertices[i][1], vertices[i][2]);
    for (i = 0; i < 4; i++)
	vdsAddTri(&state, faces[i][0], faces[i][1], faces[i][2],
		normal, normal, normal, color, color, color);

    leaves = vdsEndGeometry(&state);
    for (i = 0; i < 4; i++)
	nodes[i] = &leaves[i];
    vdsClusterOctree(nodes, 4, 0);
    root = vdsEndVertexTree(&state);

    if (root == NULL) {
	bu_log("VDS failed to finalize a vertex tree\n");
	return 1;
    }
    if (state.nodearray != NULL || state.triarray != NULL) {
	bu_log("VDS retained its temporary builder arrays\n");
	failures++;
    }

    vdsStatTree(root, &reported_nodes, &reported_leaves, &reported_tris);
    if (reported_nodes != 5 || reported_leaves != 4 || reported_tris != 4) {
	bu_log("VDS reported %d nodes, %d leaves, and %d triangles; "
		"expected 5, 4, and 4\n",
		reported_nodes, reported_leaves, reported_tris);
	failures++;
    }

    failures += validate_subtree(root, NULL, 0,
	    &visited_nodes, &visited_leaves);
    if (visited_nodes != reported_nodes || visited_leaves != reported_leaves) {
	bu_log("VDS traversal found %d nodes and %d leaves; expected %d and %d\n",
		visited_nodes, visited_leaves, reported_nodes, reported_leaves);
	failures++;
    }

    vdsAdjustTreeTopDown(root, never_fold, NULL);
    vdsRenderTree(root, count_rendered, NULL, NULL);
    if (rendered_nodes != reported_nodes || rendered_tris != reported_tris ||
	invalid_proxies != 0) {
	bu_log("VDS rendered %d nodes and %d triangles with %d invalid proxies; "
		"expected %d nodes, %d triangles, and no invalid proxies\n",
		rendered_nodes, rendered_tris, invalid_proxies,
		reported_nodes, reported_tris);
	failures++;
    }

    vdsFreeTree(root);
    return failures ? 1 : 0;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
