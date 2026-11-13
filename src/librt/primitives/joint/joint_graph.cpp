/*                  J O I N T _ G R A P H . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

#include "common.h"

#include <cmath>
#include <cstdarg>
#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

#include "raytrace.h"
#include "rt/primitives/joint.h"

struct joint_link_state {
    std::string name;
    mat_t matrix;
};

struct rt_joint_eval {
    std::vector<joint_link_state> links;
    std::unordered_map<std::string, size_t> indices;
};

struct joint_graph_edge {
    const struct rt_joint_internal *joint;
    std::string parent;
    std::string child;
};

static void
joint_graph_message(struct bu_vls *messages, const char *format, ...)
{
    va_list arguments;

    if (!messages)
	return;
    if (bu_vls_strlen(messages))
	bu_vls_strcat(messages, "; ");
    va_start(arguments, format);
    bu_vls_vprintf(messages, format, arguments);
    va_end(arguments);
}

static int
joint_graph_visit(struct rt_joint_eval *evaluation,
	const std::vector<joint_graph_edge> &edges,
	const std::unordered_map<std::string, std::vector<size_t>> &children,
	std::vector<unsigned char> &edge_state, const std::string &parent,
	const mat_t parent_matrix, struct bu_vls *messages)
{
    auto child_edges = children.find(parent);

    if (child_edges == children.end())
	return BRLCAD_OK;
    for (const size_t edge_index : child_edges->second) {
	const joint_graph_edge &edge = edges[edge_index];
	mat_t relative;
	joint_link_state child_state;

	if (edge_state[edge_index] == 1) {
	    joint_graph_message(messages, "joint graph contains a cycle at link '%s'",
		    edge.child.c_str());
	    return BRLCAD_ERROR;
	}
	if (edge_state[edge_index] == 2)
	    continue;
	edge_state[edge_index] = 1;
	if (rt_joint_relative_mat(relative, edge.joint) != BRLCAD_OK) {
	    joint_graph_message(messages, "unable to evaluate joint for child link '%s'",
		    edge.child.c_str());
	    return BRLCAD_ERROR;
	}
	child_state.name = edge.child;
	bn_mat_mul(child_state.matrix, parent_matrix, relative);
	evaluation->indices.emplace(child_state.name, evaluation->links.size());
	evaluation->links.push_back(child_state);
	if (joint_graph_visit(evaluation, edges, children, edge_state,
		edge.child, evaluation->links.back().matrix, messages) != BRLCAD_OK)
	    return BRLCAD_ERROR;
	edge_state[edge_index] = 2;
    }
    return BRLCAD_OK;
}

extern "C" int
rt_joint_eval_create(struct rt_joint_eval **result,
	const struct rt_joint_internal * const *joints, size_t joint_count,
	const struct rt_joint_eval_options *options)
{
    std::vector<joint_graph_edge> edges;
    std::unordered_map<std::string, size_t> child_owners;
    std::unordered_map<std::string, std::vector<size_t>> children;
    std::vector<unsigned char> edge_state;
    struct rt_joint_eval *evaluation;
    joint_link_state root_state;

    const size_t minimum_options_size = offsetof(struct rt_joint_eval_options,
	reserved);
    if (!result || !joints || !joint_count || !options ||
	options->size < minimum_options_size ||
	options->version != RT_JOINT_EVAL_OPTIONS_VERSION ||
	!options->root_link || !options->root_link[0])
	return BRLCAD_ERROR;
    if (options->flags) {
	joint_graph_message(options->messages, "unsupported joint evaluation flags 0x%llx",
		(unsigned long long)options->flags);
	return BRLCAD_ERROR;
    }
    if (options->root_matrix) {
	for (size_t i = 0; i < ELEMENTS_PER_MAT; ++i) {
	    if (!std::isfinite(options->root_matrix[i])) {
		joint_graph_message(options->messages, "root matrix is not finite");
		return BRLCAD_ERROR;
	    }
	}
    }
    *result = nullptr;
    edges.reserve(joint_count);
    for (size_t i = 0; i < joint_count; ++i) {
	const struct rt_joint_internal *joint = joints[i];
	joint_graph_edge edge;
	if (!joint || rt_joint_validate(joint, options->messages))
	    return BRLCAD_ERROR;
	edge.joint = joint;
	edge.parent = bu_vls_cstr(&joint->parent_link);
	edge.child = bu_vls_cstr(&joint->child_link);
	if (edge.parent.empty() || edge.child.empty()) {
	    joint_graph_message(options->messages, "joint graph links must have stable identifiers");
	    return BRLCAD_ERROR;
	}
	if (edge.parent == edge.child) {
	    joint_graph_message(options->messages, "link '%s' cannot be its own parent",
		    edge.child.c_str());
	    return BRLCAD_ERROR;
	}
	if (!child_owners.emplace(edge.child, i).second) {
	    joint_graph_message(options->messages, "child link '%s' has multiple incoming joints",
		    edge.child.c_str());
	    return BRLCAD_ERROR;
	}
	edges.push_back(edge);
    }
    for (size_t i = 0; i < edges.size(); ++i)
	children[edges[i].parent].push_back(i);

    evaluation = new rt_joint_eval;
    evaluation->links.reserve(joint_count + 1);
    root_state.name = options->root_link;
    if (options->root_matrix)
	MAT_COPY(root_state.matrix, options->root_matrix);
    else
	MAT_IDN(root_state.matrix);
    evaluation->indices.emplace(root_state.name, 0);
    evaluation->links.push_back(root_state);
    edge_state.resize(joint_count, 0);
    if (joint_graph_visit(evaluation, edges, children, edge_state,
	    root_state.name, root_state.matrix, options->messages) != BRLCAD_OK) {
	delete evaluation;
	return BRLCAD_ERROR;
    }
    for (size_t i = 0; i < edge_state.size(); ++i) {
	if (edge_state[i] != 2) {
	    joint_graph_message(options->messages,
		    "joint for child link '%s' is not reachable from root '%s'",
		    edges[i].child.c_str(), options->root_link);
	    delete evaluation;
	    return BRLCAD_ERROR;
	}
    }
    *result = evaluation;
    return BRLCAD_OK;
}

extern "C" void
rt_joint_eval_destroy(struct rt_joint_eval *evaluation)
{
    delete evaluation;
}

extern "C" size_t
rt_joint_eval_count(const struct rt_joint_eval *evaluation)
{
    return evaluation ? evaluation->links.size() : 0;
}

extern "C" const char *
rt_joint_eval_link(const struct rt_joint_eval *evaluation, size_t index)
{
    if (!evaluation || index >= evaluation->links.size())
	return nullptr;
    return evaluation->links[index].name.c_str();
}

extern "C" int
rt_joint_eval_matrix(mat_t result, const struct rt_joint_eval *evaluation,
	size_t index)
{
    if (!result || !evaluation || index >= evaluation->links.size())
	return BRLCAD_ERROR;
    MAT_COPY(result, evaluation->links[index].matrix);
    return BRLCAD_OK;
}

extern "C" int
rt_joint_eval_find(mat_t result, const struct rt_joint_eval *evaluation,
	const char *link)
{
    if (!result || !evaluation || !link)
	return BRLCAD_ERROR;
    auto iterator = evaluation->indices.find(link);
    if (iterator == evaluation->indices.end())
	return BRLCAD_ERROR;
    MAT_COPY(result, evaluation->links[iterator->second].matrix);
    return BRLCAD_OK;
}

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
