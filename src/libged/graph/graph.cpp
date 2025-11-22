/*                       G R A P H . C P P
 * BRL-CAD
 *
 * Copyright (c) 2012-2025 United States Government.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file libged/graph.cpp
 *
 *  Unified graph command supporting:
 *
 *   1) Legacy "igraph" textual mode (original graph.cpp functionality)
 *      enabled via --igraph. Produces textual node rectangles and edge
 *      polylines for the Tcl/Tk GraphEditor/igraph scripts.
 *      updated for current libavoid API usage (Router::deleteConnector(),
 *      Router::deleteShape(), batched transactions, pin creation semantics).
 *
 *   2) New high-level Adaptagrams mode (default) producing an SVG file
 *      (required positional argument). Options:
 *        --tree               Collapse to spanning tree (at most one in-edge/node)
 *        --skip-routing       Skip connector routing (nodes only)
 *        --depth D            Limit traversal depth (non-negative)
 *        --root-obj OBJ       Specify traversal roots (repeatable)
 *
 * In new mode no textual positions/edges are printed; only the SVG file is written.
 *
 * Future Enhancements (TODO):
 *   - Distinguish union/intersect/subtract edges visually (edge labels).
 *   - Optional orthogonal routing (OrthogonalRouting).
 *   - Separation constraints for logical layering (SepMatrix usage).
 *   - Cluster visualization (RootCluster from node groupings).
 *   - Edge direction arrowheads in SVG.
 *   - Color coding legend embedded in SVG.

 * NOTE ON MACRO COLLISIONS:
 *   BRL-CAD headers define UP, DOWN, LEFT, RIGHT macros. Adaptagrams uses
 *   identifiers with same names. We undefine those macros before including
 *   Adaptagrams headers to avoid expansion conflicts.
 */

#include "common.h"

#include <cstdlib>
#include <cstdint>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>
#include <deque>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <limits>

#include "bu/vls.h"
#include "bu/malloc.h"
#include "bu/hash.h"
#include "bu/opt.h"
#include "ged.h"

#ifdef HAVE_ADAPTAGRAMS

/* Avoid macro collisions with Adaptagrams compass enums */
#ifdef UP
#  undef UP
#endif
#ifdef DOWN
#  undef DOWN
#endif
#ifdef LEFT
#  undef LEFT
#endif
#ifdef RIGHT
#  undef RIGHT
#endif

#undef PARALLEL
#include <libavoid/geomtypes.h>
#include <libavoid/shape.h>
#include <libavoid/connectionpin.h>
#include <libavoid/connend.h>
#include <libavoid/connector.h>
#include <libavoid/router.h>

#include <libcola/compound_constraints.h>
#include <libdialect/graphs.h>
#include <libdialect/routing.h>
#include <libdialect/ortho.h>
#include <libdialect/opts.h>

using namespace dialect;

/* -------------------------------------------------------------------- */
/* Constants                                                            */
/* -------------------------------------------------------------------- */
static const double BASE_NODE_WIDTH   = 60.0;
static const double BASE_NODE_HEIGHT  = 30.0;
static const double CHAR_WIDTH_APPROX = 7.0;
static const double TEXT_PADDING      = 16.0;

/* -------------------------------------------------------------------- */
/* Options                                                              */
/* -------------------------------------------------------------------- */
struct graph_opts {
    bool igraph_mode = false;
    bool tree_mode = false;
    bool skip_routing = false;
    bool have_depth_limit = false;
    int  depth_limit = 0;
    std::vector<std::string> roots;
    std::string svg_filename;
    std::string igraph_subcmd;
};

/* -------------------------------------------------------------------- */
/* ======================= LEGACY (--igraph) MODE ===================== */
/* -------------------------------------------------------------------- */

namespace legacy {

/* Layout constants (unchanged) */
static const double POSITION_COORDINATE = 50.0;
static const double X_WIDTH = 30.0;
static const double Y_HEIGHT = 30.0;
static const unsigned CENTRE_PIN_CLASS = 1;

struct graph_data {
    Avoid::Router *router = nullptr;
    struct bu_hash_tbl *ids = nullptr;          /* id string -> name */
    struct bu_hash_tbl *object_types = nullptr; /* name -> raw dp->d_flags string */
    struct bu_hash_tbl *name_to_id = nullptr;   /* name -> id string */
    uint32_t object_nr = 0;                     /* count of shapes actually created */
    uint32_t last_connref_id = 0;               /* starting connector id */
    std::unordered_map<unsigned, Avoid::ShapeRef *> shapes_by_id;
};

/* Ensure each shape has exactly one centre pin before any connector */
static void ensure_centre_pin(Avoid::ShapeRef *shape)
{
    /* Check if a centre pin already exists for class CENTRE_PIN_CLASS */
    /* libavoid does not expose direct pin enumeration publicly here, assume we always add once */
    new Avoid::ShapeConnectionPin(shape,
                                  CENTRE_PIN_CLASS,
                                  0.5, 0.5,      /* proportional centre */
                                  true, 0.0,
                                  Avoid::ConnDirNone);
}

static Avoid::ShapeRef *find_shape(graph_data *dag, unsigned id)
{
    for (auto obs : dag->router->m_obstacles) {
        if (obs->id() == id) return dynamic_cast<Avoid::ShapeRef *>(obs);
    }
    return nullptr;
}

static Avoid::ShapeRef *create_shape_if_absent(graph_data *dag, unsigned id)
{
    // Fast lookup independent of transaction commit
    auto it = dag->shapes_by_id.find(id);
    if (it != dag->shapes_by_id.end()) return it->second;

    // Fallback scan (kept for safety, but usually redundant now)
    for (auto obs : dag->router->m_obstacles) {
        if (obs->id() == id) {
            Avoid::ShapeRef *s = dynamic_cast<Avoid::ShapeRef *>(obs);
            if (s) {
                dag->shapes_by_id[id] = s;
                return s;
            }
        }
    }

    Avoid::Point tl(dag->object_nr * POSITION_COORDINATE + 2.0,
                    dag->object_nr * POSITION_COORDINATE + 2.0);
    Avoid::Point br(POSITION_COORDINATE + dag->object_nr * POSITION_COORDINATE,
                    POSITION_COORDINATE + dag->object_nr * POSITION_COORDINATE);
    Avoid::Rectangle rect(tl, br);
    Avoid::ShapeRef *sr = new Avoid::ShapeRef(dag->router, rect, id);
    ensure_centre_pin(sr);
    dag->object_nr++;
    dag->shapes_by_id[id] = sr;
    return sr;
}

static void decorate(graph_data *dag, const char *name, int flags)
{
    if (!dag->object_types) {
        dag->object_types = bu_hash_create(1);
        if (!dag->object_types) {
            bu_log("legacy::decorate: failed to allocate object_types hash table\n");
            return;
        }
    }
    if (bu_hash_get(dag->object_types,
                    (uint8_t *)name,
                    strlen(name)+1)) return;

    char buf[32];
    snprintf(buf, sizeof(buf), "%d", flags);
    size_t slen = strlen(buf);
    char *val = (char *)bu_malloc(slen + 1, "legacy type");
    memcpy(val, buf, slen + 1); /* copies terminating '\0' */
    bu_hash_set(dag->object_types,
                (uint8_t *)name,
                strlen(name)+1,
                (void *)val);
}

/* Build bidirectional id maps (names -> ids, ids -> names) */
static void build_id_maps(struct ged *gedp, graph_data *dag)
{
    /* Allocate needed hash tables for IDs if not already done */
    if (!dag->name_to_id) dag->name_to_id = bu_hash_create(1);
    if (!dag->ids)        dag->ids        = bu_hash_create(1);

    int next_id = 0;
    for (int i = 0; i < RT_DBNHASH; ++i) {
        for (struct directory *dp = gedp->dbip->dbi_Head[i];
             dp != RT_DIR_NULL; dp = dp->d_forw) {
            if (!bu_hash_get(dag->name_to_id,
                             (uint8_t *)dp->d_namep,
                             strlen(dp->d_namep)+1)) {
                next_id++;
		char ibuf[32];
                snprintf(ibuf, sizeof(ibuf), "%d", next_id);
                size_t slen = strlen(ibuf);
                char *idstr = (char *)bu_malloc(slen + 1, "legacy id");
                memcpy(idstr, ibuf, slen + 1); // includes '\0'
                bu_hash_set(dag->name_to_id,
                            (uint8_t *)dp->d_namep,
                            strlen(dp->d_namep)+1, idstr);
                bu_hash_set(dag->ids,
                            (uint8_t *)idstr,
                            strlen(idstr)+1,
                            (void *)dp->d_namep);
                }
        }
    }
    dag->last_connref_id = gedp->dbip->dbi_nrec;
}

/* Process a combination: ensure parent shape exists, flatten leaves, create child shapes + connectors immediately */
static void process_comb(struct ged *gedp,
                         struct directory *dp,
                         graph_data *dag)
{
    const char *parent_id_str = (const char *)bu_hash_get(dag->name_to_id,
                                       (uint8_t *)dp->d_namep,
                                       strlen(dp->d_namep)+1);
    if (!parent_id_str) return;
    unsigned parent_id = (unsigned)atoi(parent_id_str);
    Avoid::ShapeRef *parent_shape = create_shape_if_absent(dag, parent_id);
    decorate(dag, dp->d_namep, dp->d_flags);

    struct rt_db_internal intern;
    if (rt_db_get_internal(&intern, dp, gedp->dbip,
                           (fastf_t *)NULL, &rt_uniresource) < 0)
        return;
    struct rt_comb_internal *comb = (struct rt_comb_internal *)intern.idb_ptr;

    if (!comb->tree) {
        rt_db_free_internal(&intern);
        return;
    }

    if (db_ck_v4gift_tree(comb->tree) < 0) {
        db_non_union_push(comb->tree, &rt_uniresource);
        if (db_ck_v4gift_tree(comb->tree) < 0) {
            rt_db_free_internal(&intern);
            return;
        }
    }

    size_t leaf_cnt = db_tree_nleaves(comb->tree);
    if (leaf_cnt == 0) {
        rt_db_free_internal(&intern);
        return;
    }

    struct rt_tree_array *rta = (struct rt_tree_array *)bu_calloc(
            leaf_cnt, sizeof(struct rt_tree_array), "legacy comb flatten");
    size_t actual = (struct rt_tree_array *)db_flatten_tree(rta, comb->tree,
                        OP_UNION, 1, &rt_uniresource) - rta;
    BU_ASSERT(actual == leaf_cnt);
    comb->tree = TREE_NULL;

    for (size_t i = 0; i < actual; ++i) {
        const char *child_name = rta[i].tl_tree->tr_l.tl_name;
        const char *child_id_str = (const char *)bu_hash_get(dag->name_to_id,
                               (uint8_t *)child_name,
                               strlen(child_name)+1);
        if (!child_id_str) {
            db_free_tree(rta[i].tl_tree, &rt_uniresource);
            continue;
        }
        unsigned child_id = (unsigned)atoi(child_id_str);
        Avoid::ShapeRef *child_shape = create_shape_if_absent(dag, child_id);

        /* If child is a solid (or any object) decorate if not already */
        struct directory *cdp = db_lookup(gedp->dbip, child_name, LOOKUP_QUIET);
        if (cdp) {
            if (!bu_hash_get(dag->object_types,
                             (uint8_t *)child_name,
                             strlen(child_name)+1)) {
                decorate(dag, child_name, cdp->d_flags);
            }
        }

        /* Create connector (centre to centre) */
        Avoid::ConnEnd dst(parent_shape, CENTRE_PIN_CLASS);
        Avoid::ConnEnd src(child_shape,  CENTRE_PIN_CLASS);
        dag->last_connref_id++;
        Avoid::ConnRef *cr = new Avoid::ConnRef(dag->router, src, dst,
                                                dag->last_connref_id);
        /* Callback: route when needed (matches legacy intent) */
        cr->setCallback([](void *ptr){
            ((Avoid::ConnRef *)ptr)->route();
        }, cr);

        db_free_tree(rta[i].tl_tree, &rt_uniresource);
    }

    bu_free(rta, "legacy comb rta free");
    rt_db_free_internal(&intern);
}

/* Build shapes and connectors replicating original logic */
static int build_graph(struct ged *gedp, graph_data *dag)
{
    dag->router->setTransactionUse(true);

#ifdef idealNudgingDistance
    dag->router->setRoutingParameter(Avoid::idealNudgingDistance, 25);
#endif

    /* Ensure hash tables are allocated before use */
    build_id_maps(gedp, dag);
    if (!dag->object_types) {
        dag->object_types = bu_hash_create(1);
        if (!dag->object_types) {
            bu_log("legacy::build_graph: failed to allocate object_types\n");
            return BRLCAD_ERROR;
        }
    }

    /* Pass 1: create shapes for primitives (solids) and decorate all objects */
    for (int i = 0; i < RT_DBNHASH; ++i) {
        for (struct directory *dp = gedp->dbip->dbi_Head[i];
             dp != RT_DIR_NULL; dp = dp->d_forw) {

            const char *id_str = (const char *)bu_hash_get(dag->name_to_id,
                                 (uint8_t *)dp->d_namep,
                                 strlen(dp->d_namep)+1);
            if (!id_str) continue;
            unsigned sid = (unsigned)atoi(id_str);

            decorate(dag, dp->d_namep, dp->d_flags);

            if (dp->d_flags & RT_DIR_SOLID) {
                (void)create_shape_if_absent(dag, sid);
            }
        }
    }

    dag->router->processTransaction(); /* shapes committed */

    /* Pass 2: process combinations, creating edges immediately */
    for (int i = 0; i < RT_DBNHASH; ++i) {
        for (struct directory *dp = gedp->dbip->dbi_Head[i];
             dp != RT_DIR_NULL; dp = dp->d_forw) {
            if (dp->d_flags & RT_DIR_COMB) {
                process_comb(gedp, dp, dag);
            }
        }
    }

    dag->router->processTransaction(); /* connectors committed */
    return BRLCAD_OK;
}

/* Root finding (same semantics as original) */
static std::vector<unsigned> find_roots(graph_data *dag)
{
    std::vector<unsigned> roots;
    for (auto obs : dag->router->m_obstacles) {
        Avoid::IntList incoming;
        dag->router->attachedShapes(incoming, obs->id(), Avoid::runningFrom);
        if (incoming.empty()) roots.push_back(obs->id());
    }
    return roots;
}

static void position_node(graph_data *dag,
                          bool has_parent,
                          Avoid::ShapeRef *parent,
                          Avoid::ShapeRef *child,
                          unsigned &level,
                          std::vector<double> &maxX_level)
{
    if (!child) return;
    Avoid::Box cb = child->polygon().offsetBoundingBox(0);
    double c_min_y = cb.min.y;
    double c_max_x = cb.max.x;

    if (has_parent && parent) {
        Avoid::Box pb = parent->polygon().offsetBoundingBox(0);
        double target_y = pb.min.y + POSITION_COORDINATE;
        if (!NEAR_EQUAL(c_min_y, target_y, 0.001)) {
            c_min_y = target_y;
        } else {
            maxX_level[level] = std::max(c_max_x, maxX_level[level]);
            return;
        }
    } else {
        c_min_y = POSITION_COORDINATE - 20.0;
    }

    double new_x0 = maxX_level[level] + POSITION_COORDINATE;
    double new_x1 = new_x0 + X_WIDTH;
    double new_y0 = c_min_y;
    double new_y1 = new_y0 + Y_HEIGHT;

    Avoid::Point tl(new_x0, new_y0);
    Avoid::Point br(new_x1, new_y1);
    Avoid::Rectangle nr(tl, br);
    dag->router->moveShape(child, nr, true);
    maxX_level[level] = std::max(new_x1, maxX_level[level]);
}

static void set_layout(graph_data *dag,
                       bool has_parent,
                       unsigned long parent_id,
                       unsigned long child_id,
                       unsigned &level,
                       std::vector<double> &maxX_level)
{
    Avoid::ShapeRef *parent = nullptr;
    Avoid::ShapeRef *child  = nullptr;
    parent = has_parent ? find_shape(dag, parent_id) : nullptr;
    child  = find_shape(dag, child_id);
    if (!child) return;

    std::list<unsigned> grandkids;
    dag->router->attachedShapes(grandkids, child_id, Avoid::runningTo);

    position_node(dag, has_parent, parent, child, level, maxX_level);
    dag->router->processTransaction();

    if (grandkids.empty()) return;

    has_parent = true;
    level++;
    if (maxX_level.size() <= level) maxX_level.push_back(0.0);
    for (auto cid : grandkids) {
        set_layout(dag, has_parent, child_id, cid, level, maxX_level);
    }
    level--;
}

static void emit_nodes(struct ged *gedp, graph_data *dag)
{
    for (auto obs : dag->router->m_obstacles) {
        unsigned sid = obs->id();
        char ibuf[32];
        snprintf(ibuf, sizeof(ibuf), "%u", sid);
        const char *name = (const char *)bu_hash_get(dag->ids,
                               (uint8_t *)ibuf, strlen(ibuf)+1);
        if (!name) continue;
        const char *type_str = (const char *)bu_hash_get(dag->object_types,
                                    (uint8_t *)name,
                                    strlen(name)+1);
        if (!type_str) continue;
        Avoid::Box bb = obs->polygon().offsetBoundingBox(0);
        /* Output: name raw_flags minX minY maxX maxY (legacy format) */
        bu_vls_printf(gedp->ged_result_str, "%s %s %f %f %f %f\n",
                      name, type_str,
                      bb.min.x, bb.min.y, bb.max.x, bb.max.y);
    }
}

static void emit_edges(struct ged *gedp, graph_data *dag)
{
    for (auto cr : dag->router->connRefs) {
        bu_vls_printf(gedp->ged_result_str, "edge\n");
        Avoid::PolyLine &pls = cr->displayRoute();
        for (auto &pt : pls.ps) {
            bu_vls_printf(gedp->ged_result_str, "%f %f\n", pt.x, pt.y);
        }
    }
}

/* Cleanup unchanged except ensure null checks */
static void cleanup(graph_data *dag)
{
    if (!dag) return;
    if (dag->router) {
        for (auto cr : dag->router->connRefs) {
            cr->setCallback(nullptr, nullptr);
        }
        delete dag->router;
    }
    if (dag->name_to_id) {
        struct bu_hash_entry *e = bu_hash_next(dag->name_to_id, NULL);
        while (e) {
            void *v = bu_hash_value(e, NULL);
            if (v) bu_free(v, "name_to_id val");
            e = bu_hash_next(dag->name_to_id, e);
        }
        bu_hash_destroy(dag->name_to_id);
    }
    if (dag->object_types) {
        struct bu_hash_entry *e = bu_hash_next(dag->object_types, NULL);
        while (e) {
            void *v = bu_hash_value(e, NULL);
            if (v) bu_free(v, "object_types val");
            e = bu_hash_next(dag->object_types, e);
        }
        bu_hash_destroy(dag->object_types);
    }
    if (dag->ids) bu_hash_destroy(dag->ids);
}

static int positions(struct ged *gedp)
{
    graph_data *dag = new graph_data();
    dag->router = new Avoid::Router(Avoid::PolyLineRouting);

    if (build_graph(gedp, dag) != BRLCAD_OK) {
        cleanup(dag);
        delete dag;
        return BRLCAD_ERROR;
    }

    std::vector<unsigned> roots = find_roots(dag);
    std::vector<double> maxX_level;
    unsigned level = 0;
    for (size_t i = 0; i < roots.size(); ++i) {
        level = 0;
        if (i == 0) maxX_level.push_back(0.0);
        set_layout(dag, false, roots[i], roots[i], level, maxX_level);
    }
    dag->router->processTransaction();

    for (auto cr : dag->router->connRefs) {
	cr->route();
    }
    dag->router->processTransaction();

    emit_nodes(gedp, dag);

    cleanup(dag);
    delete dag;
    return BRLCAD_OK;
}

static int show(struct ged *gedp)
{
    graph_data *dag = new graph_data();
    dag->router = new Avoid::Router(Avoid::PolyLineRouting);

    if (build_graph(gedp, dag) != BRLCAD_OK) {
        cleanup(dag);
        delete dag;
        return BRLCAD_ERROR;
    }

    std::vector<unsigned> roots = find_roots(dag);
    std::vector<double> maxX_level;
    unsigned level = 0;
    for (size_t i = 0; i < roots.size(); ++i) {
        level = 0;
        if (i == 0) maxX_level.push_back(0.0);
        set_layout(dag, false, roots[i], roots[i], level, maxX_level);
    }
    dag->router->processTransaction();

    for (auto cr : dag->router->connRefs) {
	cr->route();
    }
    dag->router->processTransaction();

    emit_nodes(gedp, dag);
    emit_edges(gedp, dag);

    cleanup(dag);
    delete dag;
    return BRLCAD_OK;
}

} /* namespace legacy */

/* -------------------------------------------------------------------- */
/* High-level (default) mode                                            */
/* -------------------------------------------------------------------- */

struct HLContext {
    Graph_SP graph;
    std::unordered_map<std::string, int> type_flags;
    std::unordered_map<std::string, std::vector<std::string>> children;
    std::unordered_map<std::string, unsigned> ext_id;
    std::unordered_map<unsigned, std::string> id_to_name;
    unsigned next_ext = 1;
};

static unsigned
hl_assign(HLContext &ctx, const std::string &n)
{
    auto it = ctx.ext_id.find(n);
    if (it != ctx.ext_id.end()) return it->second;
    unsigned id = ctx.next_ext++;
    ctx.ext_id[n] = id;
    ctx.id_to_name[id] = n;
    return id;
}

static void
hl_scan_db(struct ged *gedp, HLContext &ctx)
{
    for (int i = 0; i < RT_DBNHASH; ++i) {
	for (struct directory *dp = gedp->dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {

	    ctx.type_flags[dp->d_namep] = dp->d_flags;
	    hl_assign(ctx, dp->d_namep);

	    if (dp->d_flags & RT_DIR_COMB) {
		struct rt_db_internal intern;
		if (rt_db_get_internal(&intern, dp, gedp->dbip, (fastf_t *)NULL, &rt_uniresource) < 0)
		    continue;
		struct rt_comb_internal *comb = (struct rt_comb_internal *)intern.idb_ptr;
		if (comb->tree) {
		    if (db_ck_v4gift_tree(comb->tree) < 0) {
			db_non_union_push(comb->tree, &rt_uniresource);
			if (db_ck_v4gift_tree(comb->tree) < 0) {
			    rt_db_free_internal(&intern);
			    continue;
			}
		    }
		    size_t leaf_cnt = db_tree_nleaves(comb->tree);
		    if (leaf_cnt > 0) {
			struct rt_tree_array *rta = (struct rt_tree_array *)bu_calloc(
				leaf_cnt, sizeof(struct rt_tree_array), "hl rta");
			size_t actual = (struct rt_tree_array *)db_flatten_tree(rta,
				comb->tree, OP_UNION, 1, &rt_uniresource) - rta;
			BU_ASSERT(actual == leaf_cnt);
			comb->tree = TREE_NULL;
			auto &vec = ctx.children[dp->d_namep];
			for (size_t k = 0; k < actual; ++k) {
			    vec.push_back(rta[k].tl_tree->tr_l.tl_name);
			    db_free_tree(rta[k].tl_tree, &rt_uniresource);
			}
			bu_free(rta, "hl rta free");
		    }
		}
		rt_db_free_internal(&intern);
	    }
	}
    }
}

static void
hl_populate_graph(HLContext &ctx,
	dialect::Graph &G,
	const std::vector<std::string> &roots,
	bool tree_mode,
	bool have_depth,
	int depth_limit)
{
    struct EdgeRec { std::string src; std::string dst; };
    std::vector<EdgeRec> edge_recs;
    std::unordered_set<std::string> node_seen;

    for (const auto &root : roots) {
	if (!ctx.type_flags.count(root)) continue;
	std::unordered_set<std::string> local_incoming;
	std::deque<std::pair<std::string,int>> q;
	q.emplace_back(root, 0);
	while (!q.empty()) {
	    auto cur = q.front(); q.pop_front();
	    const std::string &n = cur.first;
	    int d = cur.second;
	    if (have_depth && d > depth_limit) continue;
	    node_seen.insert(n);
	    auto cit = ctx.children.find(n);
	    if (cit == ctx.children.end()) continue;
	    for (const auto &child : cit->second) {
		if (!ctx.type_flags.count(child)) continue;
		bool add = true;
		if (tree_mode && local_incoming.count(child)) add = false;
		if (add) edge_recs.push_back({n, child});
		if (!node_seen.count(child)) q.emplace_back(child, d + 1);
		if (tree_mode) local_incoming.insert(child);
	    }
	}
    }
    for (const auto &r : roots) {
	if (ctx.type_flags.count(r)) node_seen.insert(r);
    }

    /* Create Nodes */
    for (const auto &nm : node_seen) {
	unsigned eid = hl_assign(ctx, nm);
	double w = std::max(BASE_NODE_WIDTH,
		TEXT_PADDING + nm.size() * CHAR_WIDTH_APPROX);
	double h = BASE_NODE_HEIGHT;
	Node_SP node = Node::allocate(w, h);
	try { node->setExternalId((int)eid); } catch (...) {}
	G.addNode(node, true);
    }

    /* Map external ID to Node_SP */
    std::unordered_map<unsigned, Node_SP> ext_to_node;
    for (auto &p : G.getNodeLookup()) {
	Node_SP u = p.second;
	int ext;
	try { ext = u->getExternalId(); } catch (...) { ext = u->id(); }
	if (ext >= 0) ext_to_node[(unsigned)ext] = u;
    }

    /* Create Edges */
    for (const auto &er : edge_recs) {
	unsigned sE = ctx.ext_id[er.src];
	unsigned tE = ctx.ext_id[er.dst];
	Node_SP sN = ext_to_node[sE];
	Node_SP tN = ext_to_node[tE];
	if (!sN || !tN) continue;
	Edge_SP e = Edge::allocate(sN, tN);
	G.addEdge(e, true);
    }
}

/* SVG document builder */
namespace svg {
static std::string escape(const std::string &s)
{
    std::ostringstream out;
    for (unsigned char c : s) {
	switch (c) {
	    case '&': out << "&amp;"; break;
	    case '<': out << "&lt;";  break;
	    case '>': out << "&gt;";  break;
	    case '"': out << "&quot;"; break;
	    case '\'': out << "&apos;"; break;
	    default: out << c; break;
	}
    }
    return out.str();
}
struct Document {
    double minX =  std::numeric_limits<double>::max();
    double minY =  std::numeric_limits<double>::max();
    double maxX = -std::numeric_limits<double>::max();
    double maxY = -std::numeric_limits<double>::max();
    bool empty = true;
    std::ostringstream body;
    void update_point(double x, double y) {
	minX = std::min(minX, x);
	minY = std::min(minY, y);
	maxX = std::max(maxX, x);
	maxY = std::max(maxY, y);
	empty = false;
    }
    void update_rect(double x, double y, double w, double h) {
	update_point(x, y);
	update_point(x+w, y+h);
    }
    void pad(double p) {
	if (empty) { minX = 0; minY = 0; maxX = 10; maxY = 10; }
	minX -= p; minY -= p; maxX += p; maxY += p;
	if (maxX - minX < 10) maxX = minX + 10;
	if (maxY - minY < 10) maxY = minY + 10;
    }
    void rect(double x, double y, double w, double h,
	    const std::string &fill, const std::string &stroke,
	    const std::string &label)
    {
	body << "<rect x=\"" << x << "\" y=\"" << y
	    << "\" width=\"" << w << "\" height=\"" << h
	    << "\" fill=\"" << escape(fill)
	    << "\" stroke=\"" << escape(stroke)
	    << "\" stroke-width=\"1\"/>\n";
	if (!label.empty()) {
	    body << "<text x=\"" << (x + w/2.0) << "\" y=\"" << (y + h/2.0)
		<< "\" font-size=\"12\" text-anchor=\"middle\" "
		<< "dominant-baseline=\"central\" fill=\"#000\">"
		<< escape(label) << "</text>\n";
	}
    }
    void polyline(const std::vector<Avoid::Point> &pts, const std::string &col)
    {
	if (pts.empty()) return;
	body << "<polyline points=\"";
	for (auto &p : pts) body << p.x << "," << p.y << " ";
	body << "\" fill=\"none\" stroke=\"" << escape(col)
	    << "\" stroke-width=\"2\"/>\n";
    }
    std::string str() const {
	std::ostringstream out;
	out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
	out << "<svg xmlns=\"http://www.w3.org/2000/svg\" "
	    "viewBox=\"" << minX << " " << minY << " "
	    << (maxX - minX) << " " << (maxY - minY) << "\" "
	    << "width=\"" << (maxX - minX) << "\" height=\""
	    << (maxY - minY) << "\">\n";
	out << "<defs><style><![CDATA[text{font-family:Helvetica,Arial,sans-serif;}]]></style></defs>\n";
	out << body.str();
	out << "</svg>\n";
	return out.str();
    }
};
} /* namespace svg */

static void
hl_layout_and_output(struct ged *gedp, const graph_opts &opts)
{
    HLContext ctx;
    ctx.graph = std::make_shared<Graph>();
    hl_scan_db(gedp, ctx);

    /* Roots: if none specified, choose indegree-0 nodes */
    std::vector<std::string> roots;
    if (opts.roots.empty()) {
	std::unordered_map<std::string,int> indeg;
	for (auto &kv : ctx.children) for (auto &c : kv.second) indeg[c]++;
	for (auto &kv : ctx.type_flags) if (!indeg.count(kv.first)) roots.push_back(kv.first);
    } else {
	roots = opts.roots;
    }

    hl_populate_graph(ctx, *ctx.graph, roots,
	    opts.tree_mode, opts.have_depth_limit, opts.depth_limit);

    ctx.graph->putInBasePosition();
    ColaOptions layout_opts;
    layout_opts.useMajorization = false;     /* faster default */
    layout_opts.preventOverlaps = false;
    layout_opts.makeFeasible = true;
    layout_opts.xAxis = true;
    layout_opts.yAxis = true;
    layout_opts.idealEdgeLength = 70.0;
    ctx.graph->destress(layout_opts);

    if (!opts.skip_routing) ctx.graph->route(Avoid::PolyLineRouting);

    /* SVG */
    svg::Document doc;

    for (auto &pair : ctx.graph->getNodeLookup()) {
	dialect::Node_SP u = pair.second;
	int ext;
	try { ext = u->getExternalId(); } catch (...) { ext = u->id(); }
	if (ext < 0) continue;
	std::string name = ctx.id_to_name[ext];
	int flags = ctx.type_flags[name];
	Avoid::Point c = u->getCentre();
	dimensions d = u->getDimensions();
	double x = c.x - d.first/2.0;
	double y = c.y - d.second/2.0;
	std::string fill = "#ffffff";
	if (flags & RT_DIR_SOLID) fill = "#a5c8ff";
	else if (flags & RT_DIR_COMB) fill = "#b9ffb9";
	else fill = "#ffc4c4";
	doc.rect(x, y, d.first, d.second, fill, "#333333", name);
	doc.update_rect(x, y, d.first, d.second);
    }

    if (!opts.skip_routing) {
	for (auto &ep : ctx.graph->getEdgeLookup()) {
	    Edge_SP e = ep.second;
	    std::vector<Avoid::Point> route = e->getRoute();
	    if (route.empty()) continue;
	    doc.polyline(route, "#444444");
	    for (auto &pt : route) doc.update_point(pt.x, pt.y);
	}
    }

    doc.pad(20.0);

    std::ofstream ofs(opts.svg_filename.c_str(), std::ios::out | std::ios::trunc);
    if (!ofs.good()) {
	bu_vls_printf(gedp->ged_result_str,
		"ERROR: cannot open SVG output file '%s'\n",
		opts.svg_filename.c_str());
	return;
    }
    ofs << doc.str();
    ofs.close();
}

/* -------------------------------------------------------------------- */
/* bu_opt option parsing callbacks                                      */
/* -------------------------------------------------------------------- */
/* Signature: int cb(struct bu_vls *msgs, size_t argc, const char **argv, void *set)
 * Return number of argv entries consumed (1 for flag, 2 for flag+arg), or -1 on error.
 */

static int
opt_igraph_cb(struct bu_vls * /*msgs*/, size_t argc, const char **argv, void *set)
{
    if (argc < 1) return 0;
    (void)argv;
    graph_opts *go = (graph_opts *)set;
    go->igraph_mode = true;
    return 1;
}

static int
opt_tree_cb(struct bu_vls * /*msgs*/, size_t argc, const char **argv, void *set)
{
    if (argc < 1) return 0;
    (void)argv;
    graph_opts *go = (graph_opts *)set;
    go->tree_mode = true;
    return 1;
}

static int
opt_skip_rt_cb(struct bu_vls * /*msgs*/, size_t argc, const char **argv, void *set)
{
    if (argc < 1) return 0;
    (void)argv;
    graph_opts *go = (graph_opts *)set;
    go->skip_routing = true;
    return 1;
}

static int
opt_depth_cb(struct bu_vls *msgs, size_t argc, const char **argv, void *set)
{
    if (argc < 2) {
	if (msgs) bu_vls_printf(msgs, "--depth requires a value\n");
	return -1;
    }
    char *endp = NULL;
    long v = strtol(argv[1], &endp, 10);
    if (!endp || *endp != '\0' || v < 0) {
	if (msgs) bu_vls_printf(msgs, "Invalid depth value: %s\n", argv[1]);
	return -1;
    }
    graph_opts *go = (graph_opts *)set;
    go->have_depth_limit = true;
    go->depth_limit = (int)v;
    return 2;
}

static int
opt_root_cb(struct bu_vls * /*msgs*/, size_t argc, const char **argv, void *set)
{
    if (argc < 2) return -1;
    graph_opts *go = (graph_opts *)set;
    go->roots.emplace_back(argv[1]);
    return 2;
}

/* -------------------------------------------------------------------- */
/* Dispatcher                                                           */
/* -------------------------------------------------------------------- */
extern "C" int
ged_graph(struct ged *gedp, int argc, const char *argv[])
{
    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);
    bu_vls_trunc(gedp->ged_result_str, 0);

    graph_opts opts;
    int print_help = 0;

    const struct bu_opt_desc d[] = {
	{"h", "help",        "",    NULL,    (void *)&print_help, "Show help and exit"},
	{"",  "tree",        "",    opt_tree_cb,   (void *)&opts, "Collapse layout to spanning tree"},
	{"",  "skip-routing","",    opt_skip_rt_cb,(void *)&opts, "Skip connector routing in new mode"},
	{"",  "depth",       "D",   opt_depth_cb,  (void *)&opts, "Limit traversal depth (non-negative)"},
	{"",  "root-obj",    "OBJ", opt_root_cb,   (void *)&opts, "Specify a root object (repeatable)"},
	{"",  "igraph",      "",    opt_igraph_cb, (void *)&opts, "Enable legacy igraph textual mode"},
	BU_OPT_DESC_NULL
    };

    argc-=(argc>0); argv+=(argc>0); /* skip command name argv[0] */

    int optargc = bu_opt_parse(NULL, (size_t)argc, argv, d);
    if (optargc < 0) {
	bu_vls_printf(gedp->ged_result_str, "graph: option parse error\n");
	return BRLCAD_ERROR;
    }

    if (!opts.igraph_mode && (print_help || optargc != 1)) {
	    bu_vls_printf(gedp->ged_result_str,
		    "Usage (SVG): graph [options] output.svg\n"
		    "Options:\n"
		    "  --tree\n"
		    "  --skip-routing\n"
		    "  --depth D\n"
		    "  --root-obj OBJ (repeatable)\n"
		    "  --igraph (legacy textual mode)\n");
	    return (optargc != 1) ? BRLCAD_ERROR : BRLCAD_OK;
    }

    if (opts.igraph_mode) {
	/* Expect exactly one subcommand: show | positions.  Advance
	 * past --igraph option. */
	argc-=(argc>0); argv+=(argc>0);
	if (argc != 1) {
	    bu_vls_printf(gedp->ged_result_str,
		    "Usage (igraph): graph --igraph [show|positions]\n");
	    return BRLCAD_ERROR;
	}
	opts.igraph_subcmd = argv[0];
	if (BU_STR_EQUAL(opts.igraph_subcmd.c_str(), "show")) {
	    return legacy::show(gedp);
	} else if (BU_STR_EQUAL(opts.igraph_subcmd.c_str(), "positions")) {
	    return legacy::positions(gedp);
	} else {
	    bu_vls_printf(gedp->ged_result_str,
		    "graph: unknown igraph subcommand '%s'\n",
		    opts.igraph_subcmd.c_str());
	    return BRLCAD_ERROR;
	}
    } else {
	opts.svg_filename = argv[0];
	if (opts.svg_filename.size() < 4 ||
		opts.svg_filename.substr(opts.svg_filename.size() - 4) != ".svg") {
	    bu_vls_printf(gedp->ged_result_str,
		    "graph: output file must end in .svg\n");
	    return BRLCAD_ERROR;
	}
	hl_layout_and_output(gedp, opts);
	return BRLCAD_OK;
    }
}

#define PARALLEL BRLCAD_PARALLEL
#undef BRLCAD_PARALLEL

#else /* HAVE_ADAPTAGRAMS not defined */

#include "../ged_private.h"

/**
 * Dummy graph function in case no Adaptagrams library is found.
 */
extern "C" int
ged_graph(struct ged *gedp, int argc, const char *argv[])
{
    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* Initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    bu_vls_printf(gedp->ged_result_str, "%s : ERROR This command is disabled due to the absence of Adaptagrams library",
	    argv[0]);
    return BRLCAD_ERROR;
}

#endif /* HAVE_ADAPTAGRAMS */

#ifdef GED_PLUGIN
#include "../include/plugin.h"
extern "C" {
    struct ged_cmd_impl graph_cmd_impl = { "graph", ged_graph, GED_CMD_DEFAULT };
    const struct ged_cmd graph_cmd = { &graph_cmd_impl };
    const struct ged_cmd *graph_cmds[] = { &graph_cmd, NULL };
    static const struct ged_plugin pinfo = { GED_API, graph_cmds, 1 };
    COMPILER_DLLEXPORT const struct ged_plugin *ged_plugin_info(void)
    { return &pinfo; }
}
#endif

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8 cino=N-s
