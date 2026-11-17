/*                S C E N E _ Q U E R Y . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 */

#include "common.h"

#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

#include "vmath.h"
#include "bv/vlist.h"
#include "bu/vls.h"
#include "raytrace.h"
#include "rt/calc.h"

#include "scene_query.h"

namespace {

struct BoundData {
    point_t minimum;
    point_t maximum;
    bool have_bounds = false;
    bool error = false;
};

static union tree *
scene_bound_leaf(struct db_tree_state *tsp, const struct db_full_path *, struct rt_db_internal *ip, void *client_data)
{
    BoundData *data = static_cast<BoundData *>(client_data);
    point_t minimum, maximum;
    int status = -1;
    union tree *tree;
    struct soltab *solid;
    struct soltab solid_initial = RT_SOLTAB_INIT_ZERO;

    VSETALL(minimum, INFINITY);
    VSETALL(maximum, -INFINITY);
    if (ip->idb_meth->ft_bbox)
	status = ip->idb_meth->ft_bbox(ip, &minimum, &maximum, tsp->ts_tol);
    if (status < 0 && ip->idb_meth->ft_plot) {
	struct bu_list vhead;
	BU_LIST_INIT(&vhead);
	if (ip->idb_meth->ft_plot(&vhead, ip, tsp->ts_ttol, tsp->ts_tol, NULL) >= 0 &&
	    bv_vlist_bbox(&vhead, &minimum, &maximum, NULL, NULL) == 0)
	    status = 0;
	bv_vlist_cleanup(&vhead);
    }
    if (status < 0) {
	data->error = true;
	return TREE_NULL;
    }

    BU_GET(solid, struct soltab);
    *solid = solid_initial;
    solid->l.magic = RT_SOLTAB_MAGIC;
    solid->l2.magic = RT_SOLTAB2_MAGIC;
    solid->st_aradius = 1.0;
    VMOVE(solid->st_min, minimum);
    VMOVE(solid->st_max, maximum);

    BU_GET(tree, union tree);
    RT_TREE_INIT(tree);
    tree->tr_op = OP_SOLID;
    tree->tr_a.tu_stp = solid;
    return tree;
}

static void
scene_bound_tree_clear(union tree *tree, bool keep_root)
{
    if (!tree) return;
    switch (tree->tr_op) {
	case OP_SOLID:
	    if (tree->tr_a.tu_stp) BU_PUT(tree->tr_a.tu_stp, struct soltab);
	    break;
	case OP_UNION:
	case OP_XOR:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	    scene_bound_tree_clear(tree->tr_b.tb_left, false);
	    scene_bound_tree_clear(tree->tr_b.tb_right, false);
	    break;
	case OP_GUARD:
	case OP_XNOP:
	case OP_NOT:
	    scene_bound_tree_clear(tree->tr_b.tb_left, false);
	    break;
	default:
	    break;
    }
    if (keep_root) {
	RT_TREE_INIT(tree);
	tree->tr_op = OP_NOP;
    } else {
	BU_PUT(tree, union tree);
    }
}

static union tree *
scene_bound_region_end(struct db_tree_state *, const struct db_full_path *, union tree *tree, void *client_data)
{
    BoundData *data = static_cast<BoundData *>(client_data);
    point_t minimum, maximum;
    if (rt_bound_tree(tree, minimum, maximum) == 0) {
	if (!data->have_bounds) {
	    VMOVE(data->minimum, minimum);
	    VMOVE(data->maximum, maximum);
	    data->have_bounds = true;
	} else {
	    VMIN(data->minimum, minimum);
	    VMAX(data->maximum, maximum);
	}
    }
    scene_bound_tree_clear(tree, true);
    return tree;
}

static void
check_bounds(const SceneBounds &bounds)
{
    if (!std::isfinite(bounds.minimum[X]) || !std::isfinite(bounds.minimum[Y]) || !std::isfinite(bounds.minimum[Z]) ||
	!std::isfinite(bounds.maximum[X]) || !std::isfinite(bounds.maximum[Y]) || !std::isfinite(bounds.maximum[Z]))
	throw std::runtime_error("animation planning requires finite rendered-object bounds");
}

}

SceneQuery::SceneQuery(const std::string &database)
{
    database_ = db_open(database.c_str(), DB_OPEN_READONLY);
    if (!database_ || db_dirbuild(database_) < 0) {
	if (database_) db_close(database_);
	database_ = nullptr;
	throw std::runtime_error("unable to open geometry database for animation planning");
    }
}

SceneQuery::~SceneQuery()
{
    if (database_) db_close(database_);
}

SceneBounds
SceneQuery::bounds(const std::vector<std::string> &objects) const
{
    if (objects.empty()) throw std::runtime_error("animation planning requires at least one object");
    std::vector<const char *> names;
    for (const std::string &object : objects) names.push_back(object.c_str());
    SceneBounds result;
    struct bu_vls messages = BU_VLS_INIT_ZERO;
    int status = rt_obj_bounds(&messages, database_, (int)names.size(), names.data(), 0,
	result.minimum, result.maximum);
    if (status != BRLCAD_OK) {
	std::string error = "unable to determine rendered-object bounds";
	if (bu_vls_strlen(&messages)) error += ": " + std::string(bu_vls_cstr(&messages));
	bu_vls_free(&messages);
	throw std::runtime_error(error);
    }
    bu_vls_free(&messages);
    check_bounds(result);
    return result;
}

std::pair<double, double>
SceneQuery::projected_bounds(const std::vector<std::string> &objects, const std::vector<double> &direction) const
{
    vect_t axis = {direction[0], direction[1], direction[2]};
    vect_t reference, xaxis, yaxis;
    mat_t rotation;
    if (MAGNITUDE(axis) <= SQRT_SMALL_FASTF) throw std::runtime_error("cutting direction must be nonzero");
    VUNITIZE(axis);
    if (std::fabs(axis[Z]) < 0.9) VSET(reference, 0.0, 0.0, 1.0);
    else VSET(reference, 0.0, 1.0, 0.0);
    VCROSS(xaxis, reference, axis);
    VUNITIZE(xaxis);
    VCROSS(yaxis, axis, xaxis);
    VUNITIZE(yaxis);
    MAT_IDN(rotation);
    VMOVE(&rotation[0], xaxis);
    VMOVE(&rotation[4], yaxis);
    VMOVE(&rotation[8], axis);
    SceneBounds result = transformed_bounds(objects, rotation);
    return {result.minimum[Z], result.maximum[Z]};
}

SceneBounds
SceneQuery::transformed_bounds(const std::vector<std::string> &objects, const mat_t transform) const
{
    if (objects.empty()) throw std::runtime_error("animation planning requires at least one object");
    struct db_tree_state tree_state = RT_DBTS_INIT_IDN;
    struct bg_tess_tol tess_tol = BG_TESS_TOL_INIT_ZERO;
    struct bn_tol tolerance = BN_TOL_INIT_TOL;
    BoundData data;
    std::vector<const char *> names;
    for (const std::string &object : objects) names.push_back(object.c_str());
    tree_state.ts_dbip = database_;
    tree_state.ts_ttol = &tess_tol;
    tree_state.ts_tol = &tolerance;
    MAT_COPY(tree_state.ts_mat, transform);
    int status = db_walk_tree(database_, (int)names.size(), names.data(), 1, &tree_state, NULL,
	    scene_bound_region_end, scene_bound_leaf, &data);
    if (status < 0 || data.error || !data.have_bounds)
	throw std::runtime_error("unable to determine projected rendered-object bounds");
    SceneBounds result;
    VMOVE(result.minimum, data.minimum);
    VMOVE(result.maximum, data.maximum);
    check_bounds(result);
    return result;
}

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
