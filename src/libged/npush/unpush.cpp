/*                      U N P U S H . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
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
/** @file libged/npush/unpush.cpp
 *
 * Read-only analysis for restoring shared primitive definitions from pushed
 * geometry.  Database rewriting is intentionally deferred until the analysis
 * and reference accounting have independent regression coverage.
 */

#include "common.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "bg/trimesh.h"
#include "bu/opt.h"
#include "rt/func.h"

#include "../ged_private.h"


namespace {

constexpr fastf_t BUCKET_TOLERANCE_MULTIPLIER = 8.0;


struct unpush_reference {
    struct directory *parent = nullptr;
    struct directory *child = nullptr;
    mat_t matrix = MAT_INIT_IDN;
};


struct unpush_walk_state {
    struct db_i *dbip = nullptr;
    std::set<struct directory *> active_combinations;
    std::set<struct directory *> visited_combinations;
    std::set<struct directory *> primitives;
    std::set<struct directory *> root_primitives;
    std::map<struct directory *, size_t> selected_references;
    std::map<struct directory *, std::vector<struct directory *>> children;
    std::vector<unpush_reference> references;
    size_t missing_references = 0;
    bool cycle_found = false;
    bool read_error = false;
};


struct unpush_primitive_record {
    struct directory *dp = nullptr;
    mat_t canonical_to_input = MAT_INIT_IDN;
    std::string bucket;
};


struct unpush_group {
    std::vector<size_t> records;
};


class canonical_object {
public:
    canonical_object(struct db_i *dbip, struct directory *dp,
		     const struct bn_tol *tol, enum rt_canonicalize_mode mode)
    {
	RT_DB_INTERNAL_INIT(&input);
	RT_DB_INTERNAL_INIT(&canonical);
	if (rt_db_get_internal(&input, dp, dbip, nullptr) < 0) {
	    status = RT_CANONICALIZE_ERROR;
	    return;
	}
	input_loaded = true;
	status = rt_obj_canonicalize(&canonical, placement, &input, tol, mode);
    }

    canonical_object(const canonical_object &) = delete;
    canonical_object &operator=(const canonical_object &) = delete;

    ~canonical_object()
    {
	if (canonical.idb_ptr)
	    rt_db_free_internal(&canonical);
	if (input_loaded)
	    rt_db_free_internal(&input);
    }

    struct rt_db_internal input;
    struct rt_db_internal canonical;
    mat_t placement = MAT_INIT_IDN;
    int status = RT_CANONICALIZE_ERROR;

private:
    bool input_loaded = false;
};


void collect_object(unpush_walk_state &state, struct directory *dp);


void
collect_tree(unpush_walk_state &state, union tree *tree, struct directory *parent)
{
    if (!tree || state.cycle_found || state.read_error)
	return;

    RT_CK_TREE(tree);
    switch (tree->tr_op) {
	case OP_DB_LEAF: {
	    struct directory *child = db_lookup(state.dbip, tree->tr_l.tl_name, LOOKUP_QUIET);
	    if (child == RT_DIR_NULL) {
		state.missing_references++;
		return;
	    }
	    state.selected_references[child]++;
	    state.children[parent].push_back(child);
	    unpush_reference reference;
	    reference.parent = parent;
	    reference.child = child;
	    if (tree->tr_l.tl_mat)
		MAT_COPY(reference.matrix, tree->tr_l.tl_mat);
	    state.references.push_back(reference);
	    collect_object(state, child);
	    return;
	}
	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
	    collect_tree(state, tree->tr_b.tb_left, parent);
	    collect_tree(state, tree->tr_b.tb_right, parent);
	    return;
	case OP_NOT:
	case OP_GUARD:
	case OP_XNOP:
	    collect_tree(state, tree->tr_b.tb_left, parent);
	    return;
	case OP_NOP:
	    return;
	default:
	    state.read_error = true;
	    return;
    }
}


void
collect_object(unpush_walk_state &state, struct directory *dp)
{
    if (!(dp->d_flags & RT_DIR_COMB)) {
	state.primitives.insert(dp);
	return;
    }
    if (state.active_combinations.find(dp) != state.active_combinations.end()) {
	state.cycle_found = true;
	return;
    }
    if (state.visited_combinations.find(dp) != state.visited_combinations.end())
	return;

    state.active_combinations.insert(dp);
    state.visited_combinations.insert(dp);

    struct rt_db_internal intern;
    RT_DB_INTERNAL_INIT(&intern);
    if (rt_db_get_internal(&intern, dp, state.dbip, nullptr) < 0) {
	state.read_error = true;
	state.active_combinations.erase(dp);
	return;
    }
    const struct rt_comb_internal *comb = static_cast<const struct rt_comb_internal *>(intern.idb_ptr);
    RT_CK_COMB(comb);
    collect_tree(state, comb->tree, dp);
    rt_db_free_internal(&intern);

    state.active_combinations.erase(dp);
}


long long
bucket_value(fastf_t value, const struct bn_tol *tol)
{
    fastf_t resolution = std::max(tol->dist * BUCKET_TOLERANCE_MULTIPLIER,
	static_cast<fastf_t>(SMALL_FASTF));
    long double scaled = static_cast<long double>(value) / resolution;
    long double maximum = static_cast<long double>((std::numeric_limits<long long>::max)());

    if (scaled >= maximum)
	return (std::numeric_limits<long long>::max)();
    if (scaled <= -maximum)
	return -(std::numeric_limits<long long>::max)();
    return std::llround(scaled);
}


std::string
canonical_bucket(const canonical_object &object, const struct bn_tol *tol)
{
    std::ostringstream key;
    key << object.canonical.idb_minor_type << ':' << object.input.idb_avs.count;

    switch (object.canonical.idb_minor_type) {
	case ID_HALF:
	    break;
	case ID_ELL:
	case ID_SPH: {
	    const auto *ell = static_cast<const struct rt_ell_internal *>(object.canonical.idb_ptr);
	    key << ':' << bucket_value(MAGNITUDE(ell->a), tol)
		<< ':' << bucket_value(MAGNITUDE(ell->b), tol)
		<< ':' << bucket_value(MAGNITUDE(ell->c), tol);
	    break;
	}
	case ID_TOR: {
	    const auto *tor = static_cast<const struct rt_tor_internal *>(object.canonical.idb_ptr);
	    key << ':' << bucket_value(tor->r_a, tol)
		<< ':' << bucket_value(tor->r_h, tol);
	    break;
	}
	case ID_ETO: {
	    const auto *eto = static_cast<const struct rt_eto_internal *>(object.canonical.idb_ptr);
	    key << ':' << bucket_value(eto->eto_C[X], tol)
		<< ':' << bucket_value(eto->eto_C[Z], tol)
		<< ':' << bucket_value(eto->eto_r, tol)
		<< ':' << bucket_value(eto->eto_rd, tol);
	    break;
	}
	case ID_TGC:
	case ID_REC: {
	    const auto *tgc = static_cast<const struct rt_tgc_internal *>(object.canonical.idb_ptr);
	    const fastf_t *vectors[] = {tgc->h, tgc->a, tgc->b, tgc->c, tgc->d};
	    for (const fastf_t *vector : vectors) {
		key << ':' << bucket_value(vector[X], tol)
		    << ':' << bucket_value(vector[Y], tol)
		    << ':' << bucket_value(vector[Z], tol);
	    }
	    break;
	}
	case ID_ARB8: {
	    const auto *arb = static_cast<const struct rt_arb_internal *>(object.canonical.idb_ptr);
	    for (const point_t &point : arb->pt) {
		key << ':' << bucket_value(point[X], tol)
		    << ':' << bucket_value(point[Y], tol)
		    << ':' << bucket_value(point[Z], tol);
	    }
	    break;
	}
	case ID_BOT: {
	    const auto *bot = static_cast<const struct rt_bot_internal *>(object.canonical.idb_ptr);
	    unsigned long long hash = bg_trimesh_hash(
		bot->faces, bot->num_faces,
		reinterpret_cast<const point_t *>(bot->vertices), bot->num_vertices,
		tol->dist);
	    key << ':' << static_cast<unsigned int>(bot->mode)
		<< ':' << static_cast<unsigned int>(bot->orientation)
		<< ':' << static_cast<unsigned int>(bot->bot_flags)
		<< ':' << bot->num_vertices << ':' << bot->num_faces << ':' << hash;
	    break;
	}
	default:
	    break;
    }

    return key.str();
}


const struct bu_attribute_value_pair *
find_attribute(const struct bu_attribute_value_set *attributes, const char *name)
{
    for (size_t i = 0; i < attributes->count; i++) {
	if (BU_STR_EQUAL(attributes->avp[i].name, name))
	    return &attributes->avp[i];
    }
    return nullptr;
}


bool
attributes_equal(const struct bu_attribute_value_set *a,
		 const struct bu_attribute_value_set *b)
{
    if (a->count != b->count)
	return false;

    for (size_t i = 0; i < a->count; i++) {
	const struct bu_attribute_value_pair *other = find_attribute(b, a->avp[i].name);
	if (!other)
	    return false;
	if ((a->avp[i].value == nullptr) != (other->value == nullptr))
	    return false;
	if (a->avp[i].value && !BU_STR_EQUAL(a->avp[i].value, other->value))
	    return false;
#if defined(USE_BINARY_ATTRIBUTES)
	if (a->avp[i].binvaluelen != other->binvaluelen)
	    return false;
	if (a->avp[i].binvaluelen &&
	    std::memcmp(a->avp[i].binvalue, other->binvalue, a->avp[i].binvaluelen) != 0)
	    return false;
#endif
    }

    return true;
}


bool
near_value(fastf_t a, fastf_t b, fastf_t tolerance)
{
    return std::fabs(a - b) <= tolerance;
}


bool
bot_payload_equal(const struct rt_bot_internal *a, const struct rt_bot_internal *b,
		  const struct bn_tol *tol)
{
    if (a->mode != b->mode || a->orientation != b->orientation ||
	a->bot_flags != b->bot_flags || a->num_vertices != b->num_vertices ||
	a->num_faces != b->num_faces || a->num_normals != b->num_normals ||
	a->num_face_normals != b->num_face_normals || a->num_uvs != b->num_uvs ||
	a->num_face_uvs != b->num_face_uvs)
	return false;

    if (bg_trimesh_diff(a->faces, a->num_faces,
	reinterpret_cast<const point_t *>(a->vertices), a->num_vertices,
	b->faces, b->num_faces, reinterpret_cast<const point_t *>(b->vertices),
	b->num_vertices, tol->dist))
	return false;

    if ((a->thickness == nullptr) != (b->thickness == nullptr) ||
	(a->face_mode == nullptr) != (b->face_mode == nullptr) ||
	(a->normals == nullptr) != (b->normals == nullptr) ||
	(a->face_normals == nullptr) != (b->face_normals == nullptr) ||
	(a->uvs == nullptr) != (b->uvs == nullptr) ||
	(a->face_uvs == nullptr) != (b->face_uvs == nullptr))
	return false;

    for (size_t i = 0; a->thickness && i < a->num_faces; i++) {
	if (!near_value(a->thickness[i], b->thickness[i], tol->dist))
	    return false;
    }
    for (size_t i = 0; a->face_mode && i < a->num_faces; i++) {
	if (BU_BITTEST(a->face_mode, i) != BU_BITTEST(b->face_mode, i))
	    return false;
    }
    for (size_t i = 0; a->normals && i < 3 * a->num_normals; i++) {
	if (!near_value(a->normals[i], b->normals[i], tol->perp))
	    return false;
    }
    for (size_t i = 0; a->face_normals && i < 3 * a->num_face_normals; i++) {
	if (a->face_normals[i] != b->face_normals[i])
	    return false;
    }
    for (size_t i = 0; a->uvs && i < 3 * a->num_uvs; i++) {
	if (!near_value(a->uvs[i], b->uvs[i], tol->dist))
	    return false;
    }
    for (size_t i = 0; a->face_uvs && i < 3 * a->num_face_uvs; i++) {
	if (a->face_uvs[i] != b->face_uvs[i])
	    return false;
    }

    return true;
}


bool
canonical_geometry_equal(const struct rt_db_internal *a,
			 const struct rt_db_internal *b,
			 const struct bn_tol *tol)
{
    if (a->idb_minor_type != b->idb_minor_type)
	return false;

    switch (a->idb_minor_type) {
	case ID_HALF:
	    return true;
	case ID_ELL:
	case ID_SPH: {
	    const auto *aell = static_cast<const struct rt_ell_internal *>(a->idb_ptr);
	    const auto *bell = static_cast<const struct rt_ell_internal *>(b->idb_ptr);
	    return VNEAR_EQUAL(aell->v, bell->v, tol->dist) &&
		VNEAR_EQUAL(aell->a, bell->a, tol->dist) &&
		VNEAR_EQUAL(aell->b, bell->b, tol->dist) &&
		VNEAR_EQUAL(aell->c, bell->c, tol->dist);
	}
	case ID_TOR: {
	    const auto *ator = static_cast<const struct rt_tor_internal *>(a->idb_ptr);
	    const auto *btor = static_cast<const struct rt_tor_internal *>(b->idb_ptr);
	    return VNEAR_EQUAL(ator->v, btor->v, tol->dist) &&
		VNEAR_EQUAL(ator->h, btor->h, tol->perp) &&
		near_value(ator->r_a, btor->r_a, tol->dist) &&
		near_value(ator->r_h, btor->r_h, tol->dist);
	}
	case ID_ETO: {
	    const auto *aeto = static_cast<const struct rt_eto_internal *>(a->idb_ptr);
	    const auto *beto = static_cast<const struct rt_eto_internal *>(b->idb_ptr);
	    return VNEAR_EQUAL(aeto->eto_V, beto->eto_V, tol->dist) &&
		VNEAR_EQUAL(aeto->eto_N, beto->eto_N, tol->perp) &&
		VNEAR_EQUAL(aeto->eto_C, beto->eto_C, tol->dist) &&
		near_value(aeto->eto_r, beto->eto_r, tol->dist) &&
		near_value(aeto->eto_rd, beto->eto_rd, tol->dist);
	}
	case ID_TGC:
	case ID_REC: {
	    const auto *atgc = static_cast<const struct rt_tgc_internal *>(a->idb_ptr);
	    const auto *btgc = static_cast<const struct rt_tgc_internal *>(b->idb_ptr);
	    return VNEAR_EQUAL(atgc->v, btgc->v, tol->dist) &&
		VNEAR_EQUAL(atgc->h, btgc->h, tol->dist) &&
		VNEAR_EQUAL(atgc->a, btgc->a, tol->dist) &&
		VNEAR_EQUAL(atgc->b, btgc->b, tol->dist) &&
		VNEAR_EQUAL(atgc->c, btgc->c, tol->dist) &&
		VNEAR_EQUAL(atgc->d, btgc->d, tol->dist);
	}
	case ID_ARB8: {
	    const auto *aarb = static_cast<const struct rt_arb_internal *>(a->idb_ptr);
	    const auto *barb = static_cast<const struct rt_arb_internal *>(b->idb_ptr);
	    for (size_t i = 0; i < 8; i++) {
		if (!VNEAR_EQUAL(aarb->pt[i], barb->pt[i], tol->dist))
		    return false;
	    }
	    return true;
	}
	case ID_BOT:
	    return bot_payload_equal(static_cast<const struct rt_bot_internal *>(a->idb_ptr),
		static_cast<const struct rt_bot_internal *>(b->idb_ptr), tol);
	default:
	    return false;
    }
}


std::vector<unpush_group>
verify_groups(struct db_i *dbip, const std::vector<unpush_primitive_record> &records,
	      const std::map<std::string, std::vector<size_t>> &buckets,
	      const struct bn_tol *tol, enum rt_canonicalize_mode mode)
{
    std::vector<unpush_group> groups;

    for (const auto &bucket : buckets) {
	if (bucket.second.size() < 2)
	    continue;

	std::set<size_t> remaining(bucket.second.begin(), bucket.second.end());
	while (remaining.size() > 1) {
	    size_t representative_index = *remaining.begin();
	    remaining.erase(remaining.begin());
	    canonical_object representative(dbip, records[representative_index].dp, tol, mode);
	    if (representative.status != RT_CANONICALIZE_OK)
		continue;

	    unpush_group group;
	    group.records.push_back(representative_index);
	    for (auto candidate_it = remaining.begin(); candidate_it != remaining.end();) {
		canonical_object candidate(dbip, records[*candidate_it].dp, tol, mode);
		if (candidate.status == RT_CANONICALIZE_OK &&
		    attributes_equal(&representative.input.idb_avs, &candidate.input.idb_avs) &&
		    canonical_geometry_equal(&representative.canonical, &candidate.canonical, tol)) {
		    group.records.push_back(*candidate_it);
		    candidate_it = remaining.erase(candidate_it);
		} else {
		    ++candidate_it;
		}
	    }
	    if (group.records.size() > 1)
		groups.push_back(group);
	}
    }

    return groups;
}


void
mark_exposed_descendants(struct directory *dp,
			 const std::map<struct directory *, std::vector<struct directory *>> &children,
			 std::set<struct directory *> &exposed)
{
    auto child_it = children.find(dp);
    if (child_it == children.end())
	return;
    for (struct directory *child : child_it->second) {
	if (exposed.insert(child).second)
	    mark_exposed_descendants(child, children, exposed);
    }
}


const char *
mode_name(enum rt_canonicalize_mode mode)
{
    switch (mode) {
	case RT_CANONICALIZE_RIGID:
	    return "rigid";
	case RT_CANONICALIZE_SIMILARITY:
	    return "similarity";
	case RT_CANONICALIZE_AFFINE:
	    return "affine";
    }
    return "unknown";
}


void
unpush_usage(struct bu_vls *output, struct bu_opt_desc *options)
{
    char *option_help = bu_opt_describe(options, nullptr);
    bu_vls_sprintf(output, "Usage: unpush -D [options] object...\n\n");
    bu_vls_printf(output,
	"Analyzes pushed primitive geometry and reports opportunities to restore "
	"shared objects and reference matrices.  The current implementation is "
	"read-only and requires -D.\n\n");
    if (option_help) {
	bu_vls_printf(output, "Options:\n%s\n", option_help);
	bu_free(option_help, "unpush option help");
    }
}

}


extern "C" int
ged_unpush_core(struct ged *gedp, int argc, const char *argv[])
{
    int print_help = 0;
    int dry_run = 0;
    int local_changes_only = 0;
    int verbosity = 0;
    const char *mode_argument = "affine";
    struct bu_opt_desc options[7];
    BU_OPT(options[0], "h", "help", "", nullptr, &print_help, "Print help and exit");
    BU_OPT(options[1], "?", "", "", nullptr, &print_help, "");
    BU_OPT(options[2], "D", "dry-run", "", nullptr, &dry_run, "Analyze without modifying the database");
    BU_OPT(options[3], "L", "local", "", nullptr, &local_changes_only, "Preserve uses outside the selected trees");
    BU_OPT(options[4], "v", "verbosity", "", &bu_opt_incr_long, &verbosity, "Increase reporting verbosity");
    BU_OPT(options[5], "m", "mode", "rigid|similarity|affine", &bu_opt_str, &mode_argument, "Maximum transform class to factor out");
    BU_OPT_NULL(options[6]);

    argc--;
    argv++;
    int option_result = bu_opt_parse(nullptr, argc, argv, options);
    argc = option_result;

    if (print_help || argc == 0) {
	unpush_usage(gedp->ged_result_str, options);
	return BRLCAD_OK;
    }

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (!dry_run) {
	bu_vls_printf(gedp->ged_result_str,
	    "unpush: database rewriting is not enabled yet; use -D for analysis\n");
	return BRLCAD_ERROR;
    }

    enum rt_canonicalize_mode mode = RT_CANONICALIZE_AFFINE;
    if (BU_STR_EQUAL(mode_argument, "rigid"))
	mode = RT_CANONICALIZE_RIGID;
    else if (BU_STR_EQUAL(mode_argument, "similarity"))
	mode = RT_CANONICALIZE_SIMILARITY;
    else if (!BU_STR_EQUAL(mode_argument, "affine")) {
	bu_vls_printf(gedp->ged_result_str, "unpush: unknown mode '%s'\n", mode_argument);
	return BRLCAD_ERROR;
    }

    struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
    if (!wdbp) {
	bu_vls_printf(gedp->ged_result_str, "unpush: unable to access database tolerances\n");
	return BRLCAD_ERROR;
    }
    const struct bn_tol *tol = &wdbp->wdb_tol;

    db_update_nref(gedp->dbip);
    unpush_walk_state walk;
    walk.dbip = gedp->dbip;
    for (int i = 0; i < argc; i++) {
	struct directory *root = db_lookup(gedp->dbip, argv[i], LOOKUP_QUIET);
	if (root == RT_DIR_NULL) {
	    bu_vls_printf(gedp->ged_result_str, "unpush: object '%s' does not exist\n", argv[i]);
	    return BRLCAD_ERROR;
	}
	if (!(root->d_flags & RT_DIR_COMB))
	    walk.root_primitives.insert(root);
	collect_object(walk, root);
    }

    if (walk.cycle_found || walk.read_error) {
	bu_vls_printf(gedp->ged_result_str, "unpush: unable to analyze selected trees (%s)\n",
	    walk.cycle_found ? "cyclic combination reference" : "database read or tree error");
	return BRLCAD_ERROR;
    }

    std::vector<unpush_primitive_record> records;
    std::map<std::string, std::vector<size_t>> buckets;
    std::map<int, size_t> unsupported_types;
    std::map<int, size_t> failed_types;
    std::map<int, std::vector<std::string>> failed_objects;
    size_t unsupported = 0;
    size_t failures = 0;
    std::vector<struct directory *> primitive_objects(walk.primitives.begin(), walk.primitives.end());
    std::sort(primitive_objects.begin(), primitive_objects.end(),
	[](const struct directory *a, const struct directory *b) {
	    return std::strcmp(a->d_namep, b->d_namep) < 0;
	});
    for (struct directory *primitive : primitive_objects) {
	canonical_object object(gedp->dbip, primitive, tol, mode);
	if (object.status == RT_CANONICALIZE_UNSUPPORTED) {
	    unsupported++;
	    unsupported_types[primitive->d_minor_type]++;
	    continue;
	}
	if (object.status != RT_CANONICALIZE_OK) {
	    failures++;
	    failed_types[primitive->d_minor_type]++;
	    failed_objects[primitive->d_minor_type].push_back(primitive->d_namep);
	    continue;
	}

	unpush_primitive_record record;
	record.dp = primitive;
	MAT_COPY(record.canonical_to_input, object.placement);
	record.bucket = canonical_bucket(object, tol);
	records.push_back(record);
	buckets[record.bucket].push_back(records.size() - 1);
    }

    std::vector<unpush_group> groups = verify_groups(gedp->dbip, records, buckets, tol, mode);

    std::set<struct directory *> exposed;
    for (struct directory *combination : walk.visited_combinations) {
	size_t selected = walk.selected_references[combination];
	if (combination->d_nref > static_cast<long>(selected)) {
	    exposed.insert(combination);
	    mark_exposed_descendants(combination, walk.children, exposed);
	}
    }

    size_t grouped_objects = 0;
    size_t duplicate_objects = 0;
    size_t rewritable_references = 0;
    size_t potential_reuse = 0;
    size_t estimated_granule_reduction = 0;
    std::set<struct directory *> grouped_exposed;
    std::map<struct directory *, size_t> grouped_record_indices;
    for (const unpush_group &group : groups) {
	grouped_objects += group.records.size();
	duplicate_objects += group.records.size() - 1;
	size_t group_instances = 0;
	size_t group_storage = 0;
	size_t largest_object = 0;
	for (size_t record_index : group.records) {
	    struct directory *dp = records[record_index].dp;
	    grouped_record_indices[dp] = record_index;
	    size_t selected = walk.selected_references[dp];
	    if (walk.root_primitives.find(dp) != walk.root_primitives.end())
		selected++;
	    rewritable_references += selected;
	    group_instances += selected;
	    size_t object_size = dp->d_len;
	    group_storage += object_size;
	    largest_object = std::max(largest_object, object_size);
	    if (exposed.find(dp) != exposed.end() ||
		dp->d_nref > static_cast<long>(walk.selected_references[dp]))
		grouped_exposed.insert(dp);
	}
	if (group_instances > 1)
	    potential_reuse += group_instances - 1;
	estimated_granule_reduction += group_storage - largest_object;
    }

    bu_vls_printf(gedp->ged_result_str,
	"unpush dry-run analysis\n"
	"  mode: %s\n"
	"  local preservation: %s\n"
	"  primitive objects: %zu\n"
	"  canonicalized: %zu\n"
	"  unsupported: %zu\n"
	"  failed: %zu\n"
	"  verified groups: %zu\n"
	"  grouped objects: %zu\n"
	"  duplicate objects: %zu\n"
	"  rewritable selected references: %zu\n"
	"  top-level primitive wrappers: %zu\n"
	"  externally exposed grouped objects: %zu\n"
	"  estimated removable database granules: %zu\n"
	"  potential facetize primitive reuses: %zu\n",
	mode_name(mode), local_changes_only ? "yes" : "no", walk.primitives.size(),
	records.size(), unsupported, failures, groups.size(), grouped_objects,
	duplicate_objects, rewritable_references, walk.root_primitives.size(),
	grouped_exposed.size(), estimated_granule_reduction, potential_reuse);

    if (walk.missing_references)
	bu_vls_printf(gedp->ged_result_str, "  pre-existing missing references: %zu\n",
	    walk.missing_references);

    if (verbosity > 0) {
	for (const auto &type_count : unsupported_types) {
	    const char *label = (type_count.first >= 0 && type_count.first <= ID_MAX_SOLID) ?
		OBJ[type_count.first].ft_label : "unknown";
	    bu_vls_printf(gedp->ged_result_str, "  unsupported %s: %zu\n",
		label, type_count.second);
	}
	for (const auto &type_count : failed_types) {
	    const char *label = (type_count.first >= 0 && type_count.first <= ID_MAX_SOLID) ?
		OBJ[type_count.first].ft_label : "unknown";
	    bu_vls_printf(gedp->ged_result_str, "  failed %s: %zu\n",
		label, type_count.second);
	    bu_vls_printf(gedp->ged_result_str, "    objects:");
	    for (const std::string &name : failed_objects[type_count.first])
		bu_vls_printf(gedp->ged_result_str, " %s", name.c_str());
	    bu_vls_putc(gedp->ged_result_str, '\n');
	}
	for (size_t group_index = 0; group_index < groups.size(); group_index++) {
	    bu_vls_printf(gedp->ged_result_str, "  group %zu:", group_index + 1);
	    for (size_t record_index : groups[group_index].records)
		bu_vls_printf(gedp->ged_result_str, " %s", records[record_index].dp->d_namep);
	    bu_vls_putc(gedp->ged_result_str, '\n');
	}
    }

    if (verbosity > 1) {
	for (const unpush_reference &reference : walk.references) {
	    auto record_it = grouped_record_indices.find(reference.child);
	    if (record_it == grouped_record_indices.end())
		continue;

	    mat_t replacement;
	    bn_mat_mul(replacement, reference.matrix,
		records[record_it->second].canonical_to_input);
	    struct bu_vls title = BU_VLS_INIT_ZERO;
	    bu_vls_sprintf(&title, "  %s/%s replacement matrix",
		reference.parent->d_namep, reference.child->d_namep);
	    bn_mat_print_vls(bu_vls_cstr(&title), replacement, gedp->ged_result_str);
	    bu_vls_free(&title);
	}
	std::vector<struct directory *> root_objects(walk.root_primitives.begin(), walk.root_primitives.end());
	std::sort(root_objects.begin(), root_objects.end(),
	    [](const struct directory *a, const struct directory *b) {
		return std::strcmp(a->d_namep, b->d_namep) < 0;
	    });
	for (struct directory *root : root_objects) {
	    auto record_it = grouped_record_indices.find(root);
	    if (record_it == grouped_record_indices.end())
		continue;
	    struct bu_vls title = BU_VLS_INIT_ZERO;
	    bu_vls_sprintf(&title, "  %s wrapper matrix", root->d_namep);
	    bn_mat_print_vls(bu_vls_cstr(&title),
		records[record_it->second].canonical_to_input, gedp->ged_result_str);
	    bu_vls_free(&title);
	}
    }

    return BRLCAD_OK;
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
