/*       B R E P _ C O M B _ C O N V E R S I O N _ F A I L U R E S . C P P
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 */

#include "common.h"

#include <cstdlib>
#include <string>

#include "bu/app.h"
#include "bu/bitv.h"
#include "bu/env.h"
#include "bu/file.h"
#include "bu/malloc.h"
#include "bu/str.h"

#include "brep.h"
#include "brep/intersect.h"
#include "raytrace.h"
#include "rt/comb.h"
#include "wdb.h"


class CombinationFixture {
public:
    CombinationFixture()
	: combination(NULL)
    {
	RT_DB_INTERNAL_INIT(&internal);
	internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	internal.idb_type = ID_COMBINATION;
	internal.idb_meth = &OBJ[ID_COMBINATION];
	BU_ALLOC(combination, struct rt_comb_internal);
	RT_COMB_INTERNAL_INIT(combination);
	internal.idb_ptr = combination;
    }

    ~CombinationFixture()
    {
	rt_db_free_internal(&internal);
    }

    void
    SetTree(union tree *tree)
    {
	if (combination->tree != NULL) {
	    db_free_tree(combination->tree);
	}
	combination->tree = tree;
    }

    struct rt_db_internal internal;
    struct rt_comb_internal *combination;
};


static union tree *
make_node(int operation)
{
    union tree *node;
    BU_GET(node, union tree);
    RT_TREE_INIT(node);
    node->tr_op = operation;
    return node;
}


static union tree *
make_leaf(const char *name)
{
    union tree *leaf = make_node(OP_DB_LEAF);
    leaf->tr_l.tl_name = bu_strdup(name);
    leaf->tr_l.tl_mat = NULL;
    return leaf;
}


static union tree *
make_transformed_leaf(const char *name, const mat_t matrix)
{
    union tree *leaf = make_leaf(name);
    leaf->tr_l.tl_mat = bn_mat_dup(matrix);
    return leaf;
}


static union tree *
make_binary(int operation, union tree *left, union tree *right)
{
    union tree *node = make_node(operation);
    node->tr_b.tb_left = left;
    node->tr_b.tb_right = right;
    return node;
}


static bool
conversion_has_status(struct rt_db_internal *internal, struct db_i *dbip,
    int expected_status)
{
    ON_Brep *owner = ON_Brep::New();
    ON_Brep *converted = owner;
    const int status = rt_comb_brep_status(&converted, internal, NULL, dbip);

    const bool transactional = expected_status >= BREP_BOOLEAN_SUCCESS ||
	converted == NULL;
    if (converted != NULL && converted != owner) {
	delete converted;
    }
    delete owner;
    return status == expected_status && transactional;
}


static bool
conversion_has_failure(struct rt_db_internal *internal, struct db_i *dbip,
    int expected_status, enum rt_brep_failure_stage expected_stage,
    enum rt_brep_failure_reason expected_reason, const char *expected_path,
    const char *expected_object)
{
    ON_Brep *owner = ON_Brep::New();
    ON_Brep *converted = owner;
    struct rt_brep_conversion_info info = RT_BREP_CONVERSION_INFO_INIT;
    const int status = rt_comb_brep_status_info(&converted, internal, NULL,
	dbip, &info);

    const bool matches = status == expected_status && converted == NULL &&
	info.failure.stage == expected_stage &&
	info.failure.reason == expected_reason &&
	BU_STR_EQUAL(info.failure.tree_path, expected_path) &&
	BU_STR_EQUAL(info.failure.object_name, expected_object);
    if (converted != NULL && converted != owner)
	delete converted;
    delete owner;
    return matches;
}


static bool
conversion_has_fidelity(struct rt_db_internal *internal, struct db_i *dbip,
    enum rt_brep_conversion_fidelity expected_fidelity,
    double expected_relative_tolerance)
{
    ON_Brep *owner = ON_Brep::New();
    ON_Brep *converted = owner;
    struct rt_brep_conversion_info info = RT_BREP_CONVERSION_INFO_INIT;
    const int status = rt_comb_brep_status_info(&converted, internal, NULL,
	dbip, &info);

    const bool matches = status == BREP_BOOLEAN_SUCCESS &&
	info.fidelity == expected_fidelity &&
	NEAR_EQUAL(info.declared_relative_tolerance,
	    expected_relative_tolerance, SMALL_FASTF);
    if (converted != NULL && converted != owner) {
	delete converted;
    }
    delete owner;
    return matches;
}


static int
add_single_member_combination(struct rt_wdb *writer, const char *name,
    const char *member_name)
{
    struct wmember members;
    BU_LIST_INIT(&members.l);
    if (!mk_addmember(member_name, &members.l, NULL, WMOP_UNION)) {
	mk_freemembers(&members.l);
	return -1;
    }
    return mk_lcomb(writer, name, &members, 0, NULL, NULL, NULL, 0);
}


static ON_Brep *
make_box()
{
    const ON_3dPoint corners[8] = {
	ON_3dPoint(0.0, 0.0, 0.0),
	ON_3dPoint(1.0, 0.0, 0.0),
	ON_3dPoint(1.0, 1.0, 0.0),
	ON_3dPoint(0.0, 1.0, 0.0),
	ON_3dPoint(0.0, 0.0, 1.0),
	ON_3dPoint(1.0, 0.0, 1.0),
	ON_3dPoint(1.0, 1.0, 1.0),
	ON_3dPoint(0.0, 1.0, 1.0)
    };
    return ON_BrepBox(corners);
}


static bool
failure_snapshot_replays(CombinationFixture &fixture, struct db_i *dbip)
{
    const char *environment_name = "LIBRT_BREP_BOOLEAN_FAILURE_DIR";
    const char *old_environment = getenv(environment_name);
    const bool had_environment = old_environment != NULL;
    const std::string old_value = had_environment ? old_environment : "";
    const std::string directory = std::string(
	bu_temp_file_name("brep_failure_snapshot", 0)) + "_dir";
    bu_mkdir(directory.c_str());
    if (!bu_file_directory(directory.c_str()))
	return false;
    (void)bu_setenv(environment_name, directory.c_str(), 1);

    fixture.SetTree(make_binary(OP_INTERSECT,
	make_leaf("budget-first.s"), make_leaf("budget-second.s")));
    const brlcad::IntersectionWorkBudgetHandle first_budget =
	brlcad::CreateIntersectionWorkBudget(1);
    brlcad::SetIntersectionWorkBudget(first_budget);
    ON_Brep *owner = ON_Brep::New();
    ON_Brep *converted = owner;
    struct rt_brep_conversion_info info = RT_BREP_CONVERSION_INFO_INIT;
    const int status = rt_comb_brep_status_info(&converted,
	&fixture.internal, NULL, dbip, &info);
    brlcad::ClearIntersectionWorkBudget();
    if (converted != NULL && converted != owner)
	delete converted;
    delete owner;
    (void)bu_setenv(environment_name,
	had_environment ? old_value.c_str() : "", 1);

    bool valid = status == BREP_BOOLEAN_BUDGET_EXCEEDED &&
	info.failure.stage == RT_BREP_FAILURE_STAGE_BOOLEAN_EVALUATION &&
	info.failure.reason == RT_BREP_FAILURE_REASON_WORK_BUDGET &&
	info.failure.operation == BOOLEAN_INTERSECT &&
	info.failure.left_faces > 0 && info.failure.right_faces > 0 &&
	info.failure.boolean_failure.stage ==
	    BREP_BOOLEAN_FAILURE_STAGE_SURFACE_INTERSECTION &&
	info.failure.boolean_failure.reason ==
	    BREP_BOOLEAN_FAILURE_REASON_WORK_BUDGET &&
	info.failure.boolean_failure.face_index >= 0 &&
	info.failure.boolean_failure.peer_face_index >= 0 &&
	info.failure.boolean_failure.intersection_candidates > 0 &&
	info.failure.snapshot_name[0] != '\0';
    char snapshot_path[MAXPATHLEN] = {0};
    if (valid && !bu_dir(snapshot_path, sizeof(snapshot_path),
	    directory.c_str(), info.failure.snapshot_name, NULL))
    {
	valid = false;
    }

    struct db_i *snapshot = valid ?
	db_open(snapshot_path, DB_OPEN_READONLY) : DBI_NULL;
    if (snapshot == DBI_NULL || db_dirbuild(snapshot) < 0) {
	valid = false;
    } else {
	struct directory *left = db_lookup(snapshot, "left.brep", LOOKUP_QUIET);
	struct directory *right = db_lookup(snapshot, "right.brep", LOOKUP_QUIET);
	struct directory *replay = db_lookup(snapshot, "failure.r", LOOKUP_QUIET);
	valid = left != RT_DIR_NULL && right != RT_DIR_NULL &&
	    replay != RT_DIR_NULL;
	if (valid) {
	    struct rt_db_internal replay_internal;
	    RT_DB_INTERNAL_INIT(&replay_internal);
	    valid = rt_db_get_internal(&replay_internal, replay, snapshot,
		bn_mat_identity) >= 0;
	    if (valid) {
		const char *stage = bu_avs_get(&replay_internal.idb_avs,
		    "brep_boolean_failure_stage");
		const char *reason = bu_avs_get(&replay_internal.idb_avs,
		    "brep_boolean_failure_reason");
		const char *candidates = bu_avs_get(&replay_internal.idb_avs,
		    "brep_boolean_failure_intersection_candidates");
		valid = stage && BU_STR_EQUAL(stage, "surface_intersection") &&
		    reason && BU_STR_EQUAL(reason, "work_budget") &&
		    candidates && strtoull(candidates, NULL, 10) > 0;
	    }
	    if (valid) {
		const brlcad::IntersectionWorkBudgetHandle replay_budget =
		    brlcad::CreateIntersectionWorkBudget(1);
		brlcad::SetIntersectionWorkBudget(replay_budget);
		ON_Brep *replay_owner = ON_Brep::New();
		ON_Brep *replay_result = replay_owner;
		const int replay_status = rt_comb_brep_status(&replay_result,
		    &replay_internal, NULL, snapshot);
		brlcad::ClearIntersectionWorkBudget();
		valid = replay_status == BREP_BOOLEAN_BUDGET_EXCEEDED &&
		    replay_result == NULL;
		if (replay_result != NULL && replay_result != replay_owner)
		    delete replay_result;
		delete replay_owner;
	    }
	    rt_db_free_internal(&replay_internal);
	}
    }
    if (snapshot != DBI_NULL)
	db_close(snapshot);
    if (snapshot_path[0] != '\0')
	(void)bu_file_delete(snapshot_path);
    (void)bu_file_delete(directory.c_str());
    return valid;
}


int
main(int, char *argv[])
{
    bu_setprogname(argv[0]);
    struct db_i *dbip = db_create_inmem();
    if (dbip == NULL) {
	return 1;
    }

    CombinationFixture fixture;
    if (!conversion_has_status(&fixture.internal, dbip,
	    BREP_BOOLEAN_INVALID_INPUT))
    {
	db_close(dbip);
	return 2;
    }

    union tree *malformed_union = make_node(OP_UNION);
    malformed_union->tr_b.tb_left = make_leaf("unused");
    malformed_union->tr_b.tb_right = NULL;
    fixture.SetTree(malformed_union);
    const bool malformed_union_rejected = conversion_has_status(
	&fixture.internal, dbip, BREP_BOOLEAN_INVALID_INPUT);
    /* db_free_tree requires structurally complete binary nodes. */
    malformed_union->tr_b.tb_right = make_leaf("unused-too");
    if (!malformed_union_rejected)
    {
	db_close(dbip);
	return 3;
    }

    fixture.SetTree(make_leaf(""));
    if (!conversion_has_status(&fixture.internal, dbip,
	    BREP_BOOLEAN_INVALID_INPUT))
    {
	db_close(dbip);
	return 4;
    }

    fixture.SetTree(make_leaf("missing-object"));
    if (!conversion_has_status(&fixture.internal, dbip,
	    BREP_BOOLEAN_INVALID_INPUT))
    {
	db_close(dbip);
	return 5;
    }
    if (!conversion_has_failure(&fixture.internal, dbip,
	    BREP_BOOLEAN_INVALID_INPUT, RT_BREP_FAILURE_STAGE_LEAF_LOOKUP,
	    RT_BREP_FAILURE_REASON_MISSING_OBJECT, "r", "missing-object"))
    {
	db_close(dbip);
	return 52;
    }

    fixture.SetTree(make_node(OP_NOP));
    if (!conversion_has_status(&fixture.internal, dbip,
	    BREP_BOOLEAN_UNSUPPORTED))
    {
	db_close(dbip);
	return 6;
    }

    const uint32_t combination_magic = fixture.combination->magic;
    fixture.combination->magic = 0;
    const bool invalid_combination_rejected = conversion_has_status(
	&fixture.internal, dbip, BREP_BOOLEAN_INVALID_INPUT);
    fixture.combination->magic = combination_magic;
    if (!invalid_combination_rejected) {
	db_close(dbip);
	return 7;
    }

    const uint32_t internal_magic = fixture.internal.idb_magic;
    fixture.internal.idb_magic = 0;
    const bool invalid_internal_rejected = conversion_has_status(
	&fixture.internal, dbip, BREP_BOOLEAN_INVALID_INPUT);
    fixture.internal.idb_magic = internal_magic;
    if (!invalid_internal_rejected) {
	db_close(dbip);
	return 8;
    }

    const uint32_t database_magic = dbip->dbi_magic;
    dbip->dbi_magic = 0;
    const bool invalid_database_rejected = conversion_has_status(
	&fixture.internal, dbip, BREP_BOOLEAN_INVALID_INPUT);
    dbip->dbi_magic = database_magic;
    if (!invalid_database_rejected) {
	db_close(dbip);
	return 9;
    }

    ON_Brep *null_output = NULL;
    if (rt_comb_brep_status(&null_output, &fixture.internal, NULL, dbip) !=
	    BREP_BOOLEAN_INVALID_INPUT ||
	rt_comb_brep_status(NULL, &fixture.internal, NULL, dbip) !=
	    BREP_BOOLEAN_INVALID_INPUT)
    {
	db_close(dbip);
	return 10;
    }

    struct rt_wdb *writer = wdb_dbopen(dbip, RT_WDB_TYPE_DB_INMEM);
    ON_Brep empty_brep;
    ON_Brep open_brep;
    ON_Plane open_plane(ON_3dPoint(0.0, 0.0, 0.0),
	ON_3dPoint(1.0, 0.0, 0.0), ON_3dPoint(0.0, 1.0, 0.0));
    ON_PlaneSurface open_surface(open_plane);
    open_surface.SetDomain(0, 0.0, 1.0);
    open_surface.SetDomain(1, 0.0, 1.0);
    open_brep.NewFace(open_surface);
    ON_Brep *box_brep = make_box();
    const point_t torus_center = VINIT_ZERO;
    const vect_t torus_normal = {0.0, 0.0, 1.0};
    const vect_t halfspace_normal = {0.0, 0.0, 1.0};
    const point_t cline_base = VINIT_ZERO;
    const vect_t cline_height = {0.0, 0.0, 10.0};
    const point_t heart_center = VINIT_ZERO;
    const vect_t heart_x = {1.0, 0.0, 0.0};
    const vect_t heart_y = {0.0, 1.0, 0.0};
    const vect_t heart_z = {0.0, 0.0, 1.0};
    const point_t tgc_base = VINIT_ZERO;
    const vect_t tgc_height = {0.0, 0.0, 1.0};
    const vect_t tgc_a = {1.0, -1.0, 0.0};
    const vect_t tgc_b = {1.0, 1.0, 0.0};
    const vect_t tgc_reversed_c = {-0.1, 0.1, 0.0};
    const vect_t tgc_reversed_d = {-0.2, -0.2, 0.0};
    const point_t budget_first_base = {0.0, 0.0, -6.0};
    const point_t budget_second_base = {0.0, 0.0, -3.0};
    const vect_t budget_height = {0.0, 0.0, 12.0};
    const point_t concave_arb[8] = {
	{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0},
	{1.0, 1.0, 0.0}, {0.6, 0.4, 0.0},
	{0.0, 0.0, 1.0}, {1.0, 0.0, 1.0},
	{1.0, 1.0, 1.0}, {0.6, 0.4, 1.0}
    };
    const point_t twisted_concave_arb[8] = {
	{0.0, 0.0, 0.0}, {1.0, 1.0, 0.0},
	{0.0, 1.0, 0.0}, {1.0, 0.0, 0.0},
	{0.0, 0.0, 1.0}, {1.0, 1.0, 1.0},
	{0.0, 1.0, 1.0}, {1.0, 0.0, 1.0}
    };
    const point_t near_nonplanar_arb[8] = {
	{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0},
	{1.0, 1.0, 0.0}, {0.0, 1.0, 0.0},
	{0.0, 0.0, 1.0}, {1.0, 0.0, 1.0006},
	{1.0, 1.0, 1.0}, {0.0, 1.0, 1.0}
    };
    const point_t far_nonplanar_arb[8] = {
	{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0},
	{1.0, 1.0, 0.0}, {0.0, 1.0, 0.0},
	{0.0, 0.0, 1.0}, {1.0, 0.0, 1.2},
	{1.0, 1.0, 1.0}, {0.0, 1.0, 1.0}
    };
    const point_t bounded_nonplanar_arb[8] = {
	{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0},
	{1.0, 1.0, 0.0}, {0.0, 1.0, 0.0},
	{0.0, 0.0, 1.0}, {1.0, 0.0, 1.01},
	{1.0, 1.0, 1.0}, {0.0, 1.0, 1.0}
    };
    /* A legacy M35 ARB whose point set is a valid convex ARB but whose
     * encoded vertex order gives the canonical face table a non-planar
     * 1-2-3-4 face.  Repair only needs to canonicalize the ordering; it does
     * not move the geometric point set. */
    const point_t reordered_nonplanar_arb[8] = {
	{0.0, 0.0, 0.0},
	{0.0, -261.6201171875, 0.0},
	{-10.66834068298, -261.6201171875, 194.2414398193},
	{-24.09698486328, 0.0, 438.7570800781},
	{-72.93624877930, 0.0, -4.285009384155},
	{-72.93624877930, -261.6201171875, -4.285009384155},
	{-83.60420227051, -261.6201171875, 189.9589538574},
	{-97.03321838379, 0.0, 434.4746093750}
    };
    fastf_t bot_vertices[] = {
	0.0, 0.0, 0.0,
	1.0, 0.0, 0.0,
	0.0, 1.0, 0.0,
	0.0, 0.0, 1.0
    };
    int bot_faces[] = {
	0, 2, 1, 0, 1, 3, 0, 3, 2, 1, 2, 3
    };
    fastf_t plate_thickness[] = {0.1, 0.1, 0.1, 0.1};
    struct bu_bitv *plate_face_mode = bu_bitv_new(4);
    if (writer == NULL || box_brep == NULL ||
	mk_brep(writer, "empty.brep", &empty_brep) < 0 ||
	mk_brep(writer, "open.brep", &open_brep) < 0 ||
	mk_brep(writer, "box.brep", box_brep) < 0 ||
	mk_tor(writer, "torus.s", torus_center, torus_normal, 5.0, 1.0) < 0 ||
	mk_half(writer, "clip.half", halfspace_normal, 0.5) < 0 ||
	mk_cline(writer, "plate.cline", cline_base, cline_height, 2.0, 0.5) < 0 ||
	mk_hrt(writer, "heart.s", heart_center, heart_x, heart_y, heart_z,
	    0.5) < 0 ||
	mk_tgc(writer, "invalid.tgc", tgc_base, tgc_height, tgc_a, tgc_b,
	    tgc_reversed_c, tgc_reversed_d) < 0 ||
	mk_rcc(writer, "budget-first.s", budget_first_base, budget_height,
	    6.0) < 0 ||
	mk_rcc(writer, "budget-second.s", budget_second_base, budget_height,
	    6.0) < 0 ||
	mk_arb8(writer, "concave.s", &concave_arb[0][X]) < 0 ||
	mk_arb8(writer, "twisted-concave.s", &twisted_concave_arb[0][X]) < 0 ||
	mk_arb8(writer, "near-nonplanar.s", &near_nonplanar_arb[0][X]) < 0 ||
	mk_arb8(writer, "bounded-nonplanar.s", &bounded_nonplanar_arb[0][X]) < 0 ||
	mk_arb8(writer, "reordered-nonplanar.s",
	    &reordered_nonplanar_arb[0][X]) < 0 ||
	mk_arb8(writer, "far-nonplanar.s", &far_nonplanar_arb[0][X]) < 0 ||
	mk_bot(writer, "surface.bot", RT_BOT_SURFACE, RT_BOT_UNORIENTED,
	    0, 4, 4, bot_vertices, bot_faces, NULL, NULL) < 0 ||
	mk_bot(writer, "plate.bot", RT_BOT_PLATE, RT_BOT_CCW, 0, 4, 4,
	    bot_vertices, bot_faces, plate_thickness, plate_face_mode) < 0 ||
	mk_bot(writer, "plate-nocos.bot", RT_BOT_PLATE_NOCOS, RT_BOT_CCW,
	    0, 4, 4, bot_vertices, bot_faces, plate_thickness,
	    plate_face_mode) < 0 ||
	mk_bot(writer, "invalid-solid.bot", RT_BOT_SOLID, RT_BOT_CCW, 0,
	    4, 3, bot_vertices, bot_faces, NULL, NULL) < 0 ||
	mk_bot(writer, "solid.bot", RT_BOT_SOLID, RT_BOT_CCW, 0, 4, 4,
	    bot_vertices, bot_faces, NULL, NULL) < 0)
    {
	bu_bitv_free(plate_face_mode);
	delete box_brep;
	db_close(dbip);
	return 11;
    }
    bu_bitv_free(plate_face_mode);
    delete box_brep;
    if (add_single_member_combination(writer, "nested-heart.c", "heart.s") < 0 ||
	add_single_member_combination(writer, "cycle.c", "cycle.c") < 0)
    {
	db_close(dbip);
	return 31;
    }
    db_update_nref(dbip);

    fixture.SetTree(make_leaf("empty.brep"));
    if (!conversion_has_status(&fixture.internal, dbip,
	    BREP_BOOLEAN_INDETERMINATE))
    {
	db_close(dbip);
	return 12;
    }

    fixture.SetTree(make_leaf("open.brep"));
    if (!conversion_has_status(&fixture.internal, dbip,
	    BREP_BOOLEAN_INDETERMINATE))
    {
	db_close(dbip);
	return 13;
    }
    if (!conversion_has_failure(&fixture.internal, dbip,
	    BREP_BOOLEAN_INDETERMINATE,
	    RT_BREP_FAILURE_STAGE_LEAF_CONVERSION,
	    RT_BREP_FAILURE_REASON_OUTPUT_NOT_SOLID, "r", "open.brep"))
    {
	db_close(dbip);
	return 53;
    }

    fixture.SetTree(make_leaf("box.brep"));
    if (!conversion_has_status(&fixture.internal, dbip,
	    BREP_BOOLEAN_SUCCESS))
    {
	db_close(dbip);
	return 14;
    }

    if (!conversion_has_fidelity(&fixture.internal, dbip,
	    RT_BREP_CONVERSION_FIDELITY_EXACT, 0.0))
    {
	db_close(dbip);
	return 32;
    }

    fixture.SetTree(make_leaf("nested-heart.c"));
    if (!conversion_has_fidelity(&fixture.internal, dbip,
	    RT_BREP_CONVERSION_FIDELITY_APPROXIMATE, 0.001))
    {
	db_close(dbip);
	return 33;
    }

    fixture.SetTree(make_leaf("cycle.c"));
    if (!conversion_has_status(&fixture.internal, dbip,
	    BREP_BOOLEAN_INVALID_INPUT))
    {
	db_close(dbip);
	return 36;
    }
    if (!conversion_has_failure(&fixture.internal, dbip,
	    BREP_BOOLEAN_INVALID_INPUT, RT_BREP_FAILURE_STAGE_TREE_STRUCTURE,
	    RT_BREP_FAILURE_REASON_INVALID_TREE, "r/c", "cycle.c"))
    {
	db_close(dbip);
	return 54;
    }

    /* A BRL-CAD matrix stores uniform scale as the reciprocal homogeneous
     * divisor.  Applying that matrix directly as an OpenNURBS projective
     * transform scales control points but not all analytic surface vectors,
     * producing invalid torus topology. */
    mat_t scaled_transform = MAT_INIT_IDN;
    scaled_transform[MDX] = 2.0;
    scaled_transform[MDY] = -3.0;
    scaled_transform[MDZ] = 1.5;
    scaled_transform[MSA] = 0.25;
    fixture.SetTree(make_transformed_leaf("torus.s", scaled_transform));

    ON_Brep *scaled_owner = ON_Brep::New();
    ON_Brep *scaled_torus = scaled_owner;
    const int scaled_status = rt_comb_brep_status(&scaled_torus,
	&fixture.internal, NULL, dbip);
    const ON_BoundingBox scaled_bounds = scaled_torus != NULL ?
	scaled_torus->BoundingBox() : ON_BoundingBox::EmptyBoundingBox;
    const ON_3dPoint expected_minimum(-16.0, -36.0, 2.0);
    const ON_3dPoint expected_maximum(32.0, 12.0, 10.0);
    const bool scaled_torus_valid =
	scaled_status == BREP_BOOLEAN_SUCCESS && scaled_torus != NULL &&
	scaled_torus->IsValid() && scaled_torus->IsSolid() &&
	scaled_bounds.m_min.DistanceTo(expected_minimum) <= 1.0e-6 &&
	scaled_bounds.m_max.DistanceTo(expected_maximum) <= 1.0e-6;
    if (scaled_torus != scaled_owner) {
	delete scaled_torus;
    }
    delete scaled_owner;
    if (!scaled_torus_valid) {
	db_close(dbip);
	return 15;
    }

    /* Some legacy editing paths left roundoff in the projective row of an
     * otherwise affine matrix.  Accept dropping that row only when its full
     * displacement over the converted leaf bounds remains below the modeling
     * tolerance, and report that bounded normalization as approximate. */
    mat_t near_affine_transform = MAT_INIT_IDN;
    near_affine_transform[12] = 1.0e-5;
    fixture.SetTree(make_transformed_leaf("box.brep",
	near_affine_transform));
    ON_Brep *near_affine_owner = ON_Brep::New();
    ON_Brep *near_affine_brep = near_affine_owner;
    struct rt_brep_conversion_info near_affine_info =
	RT_BREP_CONVERSION_INFO_INIT;
    const int near_affine_status = rt_comb_brep_status_info(
	&near_affine_brep, &fixture.internal, NULL, dbip, &near_affine_info);
    const bool near_affine_valid =
	near_affine_status == BREP_BOOLEAN_SUCCESS &&
	near_affine_brep != NULL && near_affine_brep->IsValid() &&
	near_affine_brep->IsSolid() &&
	near_affine_info.fidelity == RT_BREP_CONVERSION_FIDELITY_APPROXIMATE &&
	near_affine_info.declared_relative_tolerance > 0.0 &&
	near_affine_info.declared_relative_tolerance < 1.0e-4;
    if (near_affine_brep != NULL && near_affine_brep != near_affine_owner)
	delete near_affine_brep;
    delete near_affine_owner;
    if (!near_affine_valid) {
	db_close(dbip);
	return 44;
    }

    mat_t unbounded_near_affine_transform;
    MAT_COPY(unbounded_near_affine_transform, near_affine_transform);
    unbounded_near_affine_transform[MDX] = 100.0;
    fixture.SetTree(make_transformed_leaf("box.brep",
	unbounded_near_affine_transform));
    if (!conversion_has_status(&fixture.internal, dbip,
	    BREP_BOOLEAN_INVALID_INPUT))
    {
	db_close(dbip);
	return 45;
    }

    mat_t projective_transform;
    MAT_COPY(projective_transform, scaled_transform);
    projective_transform[12] = 0.5;
    fixture.SetTree(make_transformed_leaf("torus.s", projective_transform));
    if (!conversion_has_status(&fixture.internal, dbip,
	    BREP_BOOLEAN_INVALID_INPUT))
    {
	db_close(dbip);
	return 16;
    }
    if (!conversion_has_failure(&fixture.internal, dbip,
	    BREP_BOOLEAN_INVALID_INPUT, RT_BREP_FAILURE_STAGE_LEAF_TRANSFORM,
	    RT_BREP_FAILURE_REASON_INVALID_TRANSFORM, "r", "torus.s"))
    {
	db_close(dbip);
	return 55;
    }

    /* Contextually bounded halfspaces bypass the ordinary primitive B-Rep
     * transform step.  They must still reject a genuinely projective leaf
     * matrix rather than interpreting three transformed sample points as an
     * affine plane. */
    fixture.SetTree(make_binary(OP_SUBTRACT, make_leaf("box.brep"),
	make_transformed_leaf("clip.half", projective_transform)));
    if (!conversion_has_status(&fixture.internal, dbip,
	    BREP_BOOLEAN_INVALID_INPUT))
    {
	db_close(dbip);
	return 46;
    }

    fixture.SetTree(make_leaf("clip.half"));
    ON_Brep *half_owner = ON_Brep::New();
    ON_Brep *half_brep = half_owner;
    struct rt_brep_conversion_info half_info =
	RT_BREP_CONVERSION_INFO_INIT;
    const int half_status = rt_comb_brep_status_info(&half_brep,
	&fixture.internal, NULL, dbip, &half_info);
    const bool half_rejected = half_status == BREP_BOOLEAN_UNSUPPORTED &&
	half_brep == NULL &&
	half_info.initial_status == BREP_BOOLEAN_UNSUPPORTED;
    delete half_owner;
    if (!half_rejected) {
	db_close(dbip);
	return 50;
    }

    mat_t zero_divisor_transform;
    MAT_COPY(zero_divisor_transform, scaled_transform);
    zero_divisor_transform[MSA] = 0.0;
    fixture.SetTree(make_transformed_leaf("torus.s", zero_divisor_transform));
    if (!conversion_has_status(&fixture.internal, dbip,
	    BREP_BOOLEAN_INVALID_INPUT))
    {
	db_close(dbip);
	return 17;
    }

    mat_t singular_transform = MAT_INIT_IDN;
    singular_transform[0] = 0.0;
    fixture.SetTree(make_transformed_leaf("torus.s", singular_transform));
    if (!conversion_has_status(&fixture.internal, dbip,
	    BREP_BOOLEAN_INVALID_INPUT))
    {
	db_close(dbip);
	return 18;
    }

    fixture.SetTree(make_leaf("concave.s"));
    if (!conversion_has_status(&fixture.internal, dbip,
	    BREP_BOOLEAN_SUCCESS))
    {
	db_close(dbip);
	return 19;
    }

    fixture.SetTree(make_leaf("twisted-concave.s"));
    if (!conversion_has_status(&fixture.internal, dbip,
	    BREP_BOOLEAN_INVALID_INPUT))
    {
	db_close(dbip);
	return 20;
    }

    fixture.SetTree(make_leaf("near-nonplanar.s"));
    ON_Brep *repaired_owner = ON_Brep::New();
    ON_Brep *repaired_brep = repaired_owner;
    struct rt_brep_conversion_info repaired_info =
	RT_BREP_CONVERSION_INFO_INIT;
    const int repaired_status = rt_comb_brep_status_info(&repaired_brep,
	&fixture.internal, NULL, dbip, &repaired_info);
    const bool repaired_valid = repaired_status == BREP_BOOLEAN_SUCCESS &&
	repaired_brep != NULL && repaired_brep->IsValid() &&
	repaired_brep->IsSolid() &&
	repaired_info.fidelity == RT_BREP_CONVERSION_FIDELITY_APPROXIMATE &&
	(repaired_info.approximation_flags &
	    RT_BREP_APPROXIMATION_ARB_REPAIR) != 0 &&
	repaired_info.repaired_arb_count == 1 &&
	repaired_info.declared_relative_tolerance > 0.0;
    if (repaired_brep != NULL && repaired_brep != repaired_owner)
	delete repaired_brep;
    delete repaired_owner;
    if (!repaired_valid) {
	db_close(dbip);
	return 47;
    }

    /* Legacy coordinate rounding can require more than one absolute modeling
     * tolerance while still being a small bounded relative edit. */
    fixture.SetTree(make_leaf("bounded-nonplanar.s"));
    ON_Brep *bounded_owner = ON_Brep::New();
    ON_Brep *bounded_brep = bounded_owner;
    struct rt_brep_conversion_info bounded_info =
	RT_BREP_CONVERSION_INFO_INIT;
    const int bounded_status = rt_comb_brep_status_info(&bounded_brep,
	&fixture.internal, NULL, dbip, &bounded_info);
    const bool bounded_valid = bounded_status == BREP_BOOLEAN_SUCCESS &&
	bounded_brep != NULL && bounded_brep->IsValid() &&
	bounded_brep->IsSolid() &&
	bounded_info.fidelity == RT_BREP_CONVERSION_FIDELITY_APPROXIMATE &&
	(bounded_info.approximation_flags &
	    RT_BREP_APPROXIMATION_ARB_REPAIR) != 0 &&
	bounded_info.repaired_arb_count == 1 &&
	bounded_info.declared_relative_tolerance > 0.0 &&
	bounded_info.declared_relative_tolerance < 0.005;
    if (bounded_brep != NULL && bounded_brep != bounded_owner)
	delete bounded_brep;
    delete bounded_owner;
    if (!bounded_valid) {
	db_close(dbip);
	return 48;
    }

    fixture.SetTree(make_leaf("reordered-nonplanar.s"));
    ON_Brep *reordered_owner = ON_Brep::New();
    ON_Brep *reordered_brep = reordered_owner;
    struct rt_brep_conversion_info reordered_info =
	RT_BREP_CONVERSION_INFO_INIT;
    const int reordered_status = rt_comb_brep_status_info(&reordered_brep,
	&fixture.internal, NULL, dbip, &reordered_info);
    const bool reordered_valid =
	reordered_status == BREP_BOOLEAN_SUCCESS &&
	reordered_brep != NULL && reordered_brep->IsValid() &&
	reordered_brep->IsSolid() &&
	reordered_info.fidelity == RT_BREP_CONVERSION_FIDELITY_APPROXIMATE &&
	(reordered_info.approximation_flags &
	    RT_BREP_APPROXIMATION_ARB_REPAIR) != 0 &&
	reordered_info.repaired_arb_count == 1;
    if (reordered_brep != NULL && reordered_brep != reordered_owner)
	delete reordered_brep;
    delete reordered_owner;
    if (!reordered_valid) {
	db_close(dbip);
	return 51;
    }

    /* The interactive ARB repair permits larger edits.  Conversion still
     * rejects changes outside the relative/absolute conversion budget. */
    fixture.SetTree(make_leaf("far-nonplanar.s"));
    if (!conversion_has_status(&fixture.internal, dbip,
	    BREP_BOOLEAN_INVALID_INPUT))
    {
	db_close(dbip);
	return 49;
    }

    /* Ray preparation rejects a TGC whose corresponding end axes point in
     * opposite directions.  A ruled B-Rep can still be manufactured from
     * those curves, but it is not the same primitive and must not turn an
     * invalid raytrace leaf into a successful CSG conversion. */
    fixture.SetTree(make_leaf("invalid.tgc"));
    if (!conversion_has_status(&fixture.internal, dbip,
	    BREP_BOOLEAN_INVALID_INPUT))
    {
	db_close(dbip);
	return 37;
    }

    fixture.SetTree(make_binary(OP_SUBTRACT, make_leaf("box.brep"),
	make_leaf("invalid.tgc")));
    if (!conversion_has_status(&fixture.internal, dbip,
	    BREP_BOOLEAN_INVALID_INPUT))
    {
	db_close(dbip);
	return 38;
    }

    /* Invalid geometry that is provably outside the converted minuend cannot
     * affect a subtraction.  Its primitive box is still usable for this
     * proof even though direct B-Rep conversion rejects it. */
    mat_t disjoint_transform = MAT_INIT_IDN;
    disjoint_transform[MDX] = 100.0;
    fixture.SetTree(make_binary(OP_SUBTRACT, make_leaf("box.brep"),
	make_transformed_leaf("twisted-concave.s", disjoint_transform)));
    if (!conversion_has_status(&fixture.internal, dbip,
	    BREP_BOOLEAN_SUCCESS))
    {
	db_close(dbip);
	return 34;
    }

    fixture.SetTree(make_leaf("surface.bot"));
    if (!conversion_has_status(&fixture.internal, dbip,
	    BREP_BOOLEAN_UNSUPPORTED))
    {
	db_close(dbip);
	return 21;
    }

    fixture.SetTree(make_leaf("plate.bot"));
    if (!conversion_has_status(&fixture.internal, dbip,
	    BREP_BOOLEAN_UNSUPPORTED))
    {
	db_close(dbip);
	return 22;
    }

    fixture.SetTree(make_leaf("plate-nocos.bot"));
    if (!conversion_has_status(&fixture.internal, dbip,
	    BREP_BOOLEAN_UNSUPPORTED))
    {
	db_close(dbip);
	return 23;
    }

    fixture.SetTree(make_leaf("solid.bot"));
    if (!conversion_has_status(&fixture.internal, dbip,
	    BREP_BOOLEAN_SUCCESS))
    {
	db_close(dbip);
	return 24;
    }

    /* A solid mode flag does not make an open indexed mesh a solid.  Reject
     * malformed source topology as invalid input rather than reporting the
     * inevitable open B-Rep as an indeterminate conversion. */
    fixture.SetTree(make_leaf("invalid-solid.bot"));
    if (!conversion_has_status(&fixture.internal, dbip,
	    BREP_BOOLEAN_INVALID_INPUT))
    {
	db_close(dbip);
	return 39;
    }

    fixture.SetTree(make_leaf("plate.cline"));
    if (!conversion_has_status(&fixture.internal, dbip,
	    BREP_BOOLEAN_UNSUPPORTED))
    {
	db_close(dbip);
	return 30;
    }

    fixture.SetTree(make_binary(OP_UNION, make_leaf("box.brep"),
	make_leaf("box.brep")));
    if (!conversion_has_status(&fixture.internal, dbip,
	    BREP_BOOLEAN_SUCCESS))
    {
	db_close(dbip);
	return 25;
    }

    mat_t identity_transform = MAT_INIT_IDN;
    fixture.SetTree(make_binary(OP_INTERSECT, make_leaf("box.brep"),
	make_transformed_leaf("box.brep", identity_transform)));
    if (!conversion_has_status(&fixture.internal, dbip,
	    BREP_BOOLEAN_SUCCESS))
    {
	db_close(dbip);
	return 26;
    }

    fixture.SetTree(make_binary(OP_SUBTRACT, make_leaf("box.brep"),
	make_leaf("box.brep")));
    if (!conversion_has_status(&fixture.internal, dbip,
	    BREP_BOOLEAN_EMPTY))
    {
	db_close(dbip);
	return 27;
    }

    fixture.SetTree(make_binary(OP_XOR, make_leaf("box.brep"),
	make_leaf("box.brep")));
    if (!conversion_has_status(&fixture.internal, dbip,
	    BREP_BOOLEAN_EMPTY))
    {
	db_close(dbip);
	return 28;
    }

    fixture.SetTree(make_binary(OP_SUBTRACT, make_leaf("missing-object"),
	make_leaf("missing-object")));
    if (!conversion_has_status(&fixture.internal, dbip,
	    BREP_BOOLEAN_INVALID_INPUT))
    {
	db_close(dbip);
	return 29;
    }

    /* Bounds pruning must not hide a missing subtractor member. */
    fixture.SetTree(make_binary(OP_SUBTRACT, make_leaf("box.brep"),
	make_leaf("missing-object")));
    if (!conversion_has_status(&fixture.internal, dbip,
	    BREP_BOOLEAN_INVALID_INPUT))
    {
	db_close(dbip);
	return 35;
    }

    /* Exercise both orientations of the exact absorption identity
     *
     *                 (A - B) union B = A union B.
     *
     * Use a transformed copy for B so structural identity must include the
     * complete leaf transform rather than merely matching object names. */
    mat_t inner_transform = MAT_INIT_IDN;
    inner_transform[0] = 0.5;
    inner_transform[5] = 0.5;
    inner_transform[10] = 0.5;
    inner_transform[MDX] = 0.25;
    inner_transform[MDY] = 0.25;
    inner_transform[MDZ] = 0.25;
    fixture.SetTree(make_binary(OP_UNION,
	make_binary(OP_SUBTRACT, make_leaf("box.brep"),
	    make_transformed_leaf("box.brep", inner_transform)),
	make_transformed_leaf("box.brep", inner_transform)));
    if (!conversion_has_status(&fixture.internal, dbip,
	    BREP_BOOLEAN_SUCCESS))
    {
	db_close(dbip);
	return 40;
    }

    fixture.SetTree(make_binary(OP_UNION,
	make_transformed_leaf("box.brep", inner_transform),
	make_binary(OP_SUBTRACT, make_leaf("box.brep"),
	    make_transformed_leaf("box.brep", inner_transform))));
    if (!conversion_has_status(&fixture.internal, dbip,
	    BREP_BOOLEAN_SUCCESS))
    {
	db_close(dbip);
	return 41;
    }

    /* Absorption may remove one occurrence of the repeated subtree, but it
     * must still convert and validate the surviving occurrence. */
    fixture.SetTree(make_binary(OP_UNION,
	make_binary(OP_SUBTRACT, make_leaf("box.brep"),
	    make_leaf("missing-object")),
	make_leaf("missing-object")));
    if (!conversion_has_status(&fixture.internal, dbip,
	    BREP_BOOLEAN_INVALID_INPUT))
    {
	db_close(dbip);
	return 42;
    }

    fixture.SetTree(make_binary(OP_UNION, make_leaf("missing-object"),
	make_binary(OP_SUBTRACT, make_leaf("box.brep"),
	    make_leaf("missing-object"))));
    if (!conversion_has_status(&fixture.internal, dbip,
	    BREP_BOOLEAN_INVALID_INPUT))
    {
	db_close(dbip);
	return 43;
    }

    if (!failure_snapshot_replays(fixture, dbip)) {
	db_close(dbip);
	return 56;
    }

    db_close(dbip);
    return 0;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
