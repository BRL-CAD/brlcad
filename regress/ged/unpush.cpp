/*                R E G R E S S _ U N P U S H . C P P
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

#include "common.h"

#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "bu/app.h"
#include "bu/file.h"
#include "bu/log.h"
#include "bu/vls.h"
#include "ged.h"
#include "rt/calc.h"
#include "wdb.h"


static std::string
file_contents(const std::string &path)
{
    std::ifstream stream(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(stream),
	std::istreambuf_iterator<char>());
}


static std::string
temporary_database_path(const char *suffix)
{
    char temporary[MAXPATHLEN] = {0};
    FILE *file = bu_temp_file(temporary, MAXPATHLEN);
    if (file)
	std::fclose(file);

    struct bu_vls database_path = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&database_path, "%s_%s.g", temporary, suffix);
    std::string path = bu_vls_cstr(&database_path);
    bu_vls_free(&database_path);
    return path;
}


static bool
write_superell(struct rt_wdb *wdbp, const char *name, const point_t center,
	       const vect_t a, const vect_t b, const vect_t c,
	       fastf_t n, fastf_t e)
{
    struct rt_db_internal intern;
    struct rt_superell_internal *superell;

    RT_DB_INTERNAL_INIT(&intern);
    intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern.idb_minor_type = ID_SUPERELL;
    intern.idb_meth = &OBJ[ID_SUPERELL];
    BU_ALLOC(intern.idb_ptr, struct rt_superell_internal);
    superell = static_cast<struct rt_superell_internal *>(intern.idb_ptr);
    superell->magic = RT_SUPERELL_INTERNAL_MAGIC;
    VMOVE(superell->v, center);
    VMOVE(superell->a, a);
    VMOVE(superell->b, b);
    VMOVE(superell->c, c);
    superell->n = n;
    superell->e = e;
    return wdb_put_internal(wdbp, name, &intern, 1.0) == 0;
}


static std::string
make_superell_database()
{
    std::string path = temporary_database_path("unpush_superell");
    struct rt_wdb *wdbp = wdb_fopen(path.c_str());
    if (!wdbp)
	return std::string();
    mk_id(wdbp, "unpush SUPERELL regression");

    const point_t center_a = VINIT_ZERO;
    const vect_t a_a = {2.0, 0.0, 0.0};
    const vect_t b_a = {0.0, 3.0, 0.0};
    const vect_t c_a = {0.0, 0.0, 5.0};
    const point_t center_b = {20.0, -10.0, 4.0};
    const vect_t a_b = {0.0, 0.0, 4.0};
    const vect_t b_b = {-6.0, 0.0, 0.0};
    const vect_t c_b = {0.0, -10.0, 0.0};
    bool failed = !write_superell(wdbp, "superell_a.s", center_a, a_a,
	    b_a, c_a, 0.7, 1.4) ||
	!write_superell(wdbp, "superell_b.s", center_b, a_b, b_b, c_b,
	    0.7, 1.4);

    struct wmember root;
    BU_LIST_INIT(&root.l);
    mk_addmember("superell_a.s", &root.l, nullptr, WMOP_UNION);
    mk_addmember("superell_b.s", &root.l, nullptr, WMOP_UNION);
    failed = failed || mk_lcomb(wdbp, "superells.c", &root, 0, nullptr,
	nullptr, nullptr, 0);
    wdb_close(wdbp);
    if (failed) {
	bu_file_delete(path.c_str());
	return std::string();
    }
    return path;
}


static std::string
make_axial_analytic_database()
{
    std::string path = temporary_database_path("unpush_axial_analytics");
    struct rt_wdb *wdbp = wdb_fopen(path.c_str());
    if (!wdbp)
	return std::string();
    mk_id(wdbp, "unpush axial analytic regression");

    const point_t rpc_vertex_a = VINIT_ZERO;
    const vect_t rpc_height_a = {0.0, 0.0, 4.0};
    const vect_t rpc_breadth_a = {3.0, 0.0, 0.0};
    const point_t rpc_vertex_b = {20.0, -10.0, 4.0};
    const vect_t rpc_height_b = {0.0, 8.0, 0.0};
    const vect_t rpc_breadth_b = {-6.0, 0.0, 0.0};
    const point_t rhc_vertex_a = {-5.0, 8.0, 2.0};
    const vect_t rhc_height_a = {0.0, 0.0, 4.0};
    const vect_t rhc_breadth_a = {3.0, 0.0, 0.0};
    const point_t rhc_vertex_b = {30.0, 6.0, -7.0};
    const vect_t rhc_height_b = {0.0, 12.0, 0.0};
    const vect_t rhc_breadth_b = {0.0, 0.0, 9.0};
    const point_t epa_vertex_a = {4.0, -3.0, 1.0};
    const vect_t epa_height_a = {0.0, 0.0, 4.0};
    const vect_t epa_axis_a = {1.0, 0.0, 0.0};
    const point_t epa_vertex_b = {-20.0, 15.0, 6.0};
    const vect_t epa_height_b = {0.0, 8.0, 0.0};
    const vect_t epa_axis_b = {-1.0, 0.0, 0.0};
    const point_t ehy_vertex_a = {-12.0, -4.0, 3.0};
    const vect_t ehy_height_a = {0.0, 0.0, 4.0};
    const vect_t ehy_axis_a = {1.0, 0.0, 0.0};
    const point_t ehy_vertex_b = {25.0, -12.0, -5.0};
    const vect_t ehy_height_b = {0.0, 12.0, 0.0};
    const vect_t ehy_axis_b = {0.0, 0.0, 1.0};
    point_t part_vertex_a = {6.0, -9.0, 2.0};
    vect_t part_height_a = {0.0, 0.0, 4.0};
    point_t part_vertex_b = {-15.0, 7.0, -3.0};
    vect_t part_height_b = {0.0, 8.0, 0.0};
    point_t part_sphere_vertex_a = {-8.0, 3.0, 11.0};
    vect_t part_sphere_height_a = VINIT_ZERO;
    point_t part_sphere_vertex_b = {18.0, -6.0, 5.0};
    vect_t part_sphere_height_b = VINIT_ZERO;
    bool failed = mk_rpc(wdbp, "rpc_a.s", rpc_vertex_a, rpc_height_a,
	    rpc_breadth_a, 2.0) ||
	mk_rpc(wdbp, "rpc_b.s", rpc_vertex_b, rpc_height_b,
	    rpc_breadth_b, 4.0) ||
	mk_rhc(wdbp, "rhc_a.s", rhc_vertex_a, rhc_height_a,
	    rhc_breadth_a, 2.0, 5.0) ||
	mk_rhc(wdbp, "rhc_b.s", rhc_vertex_b, rhc_height_b,
	    rhc_breadth_b, 6.0, 15.0) ||
	mk_epa(wdbp, "epa_a.s", epa_vertex_a, epa_height_a, epa_axis_a,
	    3.0, 2.0) ||
	mk_epa(wdbp, "epa_b.s", epa_vertex_b, epa_height_b, epa_axis_b,
	    6.0, 4.0) ||
	mk_ehy(wdbp, "ehy_a.s", ehy_vertex_a, ehy_height_a, ehy_axis_a,
	    3.0, 2.0, 5.0) ||
	mk_ehy(wdbp, "ehy_b.s", ehy_vertex_b, ehy_height_b, ehy_axis_b,
	    9.0, 6.0, 15.0) ||
	mk_particle(wdbp, "part_a.s", part_vertex_a, part_height_a, 3.0, 2.0) ||
	mk_particle(wdbp, "part_b.s", part_vertex_b, part_height_b, 4.0, 6.0) ||
	mk_particle(wdbp, "part_sphere_a.s", part_sphere_vertex_a,
	    part_sphere_height_a, 2.0, 2.0) ||
	mk_particle(wdbp, "part_sphere_b.s", part_sphere_vertex_b,
	    part_sphere_height_b, 4.0, 4.0);

    struct wmember root;
    BU_LIST_INIT(&root.l);
    mk_addmember("rpc_a.s", &root.l, nullptr, WMOP_UNION);
    mk_addmember("rpc_b.s", &root.l, nullptr, WMOP_UNION);
    mk_addmember("rhc_a.s", &root.l, nullptr, WMOP_UNION);
    mk_addmember("rhc_b.s", &root.l, nullptr, WMOP_UNION);
    mk_addmember("epa_a.s", &root.l, nullptr, WMOP_UNION);
    mk_addmember("epa_b.s", &root.l, nullptr, WMOP_UNION);
    mk_addmember("ehy_a.s", &root.l, nullptr, WMOP_UNION);
    mk_addmember("ehy_b.s", &root.l, nullptr, WMOP_UNION);
    mk_addmember("part_a.s", &root.l, nullptr, WMOP_UNION);
    mk_addmember("part_b.s", &root.l, nullptr, WMOP_UNION);
    mk_addmember("part_sphere_a.s", &root.l, nullptr, WMOP_UNION);
    mk_addmember("part_sphere_b.s", &root.l, nullptr, WMOP_UNION);
    failed = failed || mk_lcomb(wdbp, "axial_analytics.c", &root, 0, nullptr,
	nullptr, nullptr, 0);
    wdb_close(wdbp);
    if (failed) {
	bu_file_delete(path.c_str());
	return std::string();
    }
    return path;
}


static std::string
make_database(bool expose_selected = false)
{
    std::string path = temporary_database_path("unpush");

    struct rt_wdb *wdbp = wdb_fopen(path.c_str());
    if (!wdbp)
	return std::string();
    mk_id(wdbp, "unpush regression");

    point_t center_a = {10.0, 0.0, 0.0};
    point_t center_b = {0.0, 20.0, 0.0};
    point_t center_c = {0.0, 0.0, 30.0};
    point_t root_center_a = {-30.0, 5.0, 2.0};
    point_t root_center_b = {25.0, -15.0, 8.0};
    point_t eto_center_a = {0.0, -20.0, 5.0};
    vect_t eto_normal_a = {0.0, 0.0, 1.0};
    vect_t eto_major_a = {3.0, 0.0, 4.0};
    point_t eto_center_b = {50.0, 10.0, -5.0};
    vect_t eto_normal_b = {0.0, 1.0, 0.0};
    vect_t eto_major_b = {0.0, 8.0, 6.0};
    point_t tgc_base_a = VINIT_ZERO;
    vect_t tgc_height_a = {0.0, 0.0, 4.0};
    vect_t tgc_a_a = {2.0, 0.0, 0.0};
    vect_t tgc_b_a = {0.0, 3.0, 0.0};
    vect_t tgc_c_a = {1.0, 0.0, 0.0};
    vect_t tgc_d_a = {0.0, 1.5, 0.0};
    point_t tgc_base_b = {40.0, -5.0, 7.0};
    vect_t tgc_height_b = {2.0, 0.8, 6.0};
    vect_t tgc_a_b = {0.0, 6.0, 0.0};
    vect_t tgc_b_b = {-6.0, 0.0, 0.0};
    vect_t tgc_c_b = {0.0, 3.0, 0.0};
    vect_t tgc_d_b = {-3.0, 0.0, 0.0};
    point_t arb_a[8] = {
	{0.0, 0.0, 0.0}, {4.0, 0.0, 0.0},
	{4.0, 3.0, 0.0}, {0.0, 3.0, 0.0},
	{0.0, 0.0, 2.0}, {4.0, 0.0, 2.0},
	{4.0, 3.0, 2.0}, {0.0, 3.0, 2.0}
    };
    point_t arb_b[8];
    mat_t arb_transform;
    MAT_IDN(arb_transform);
    arb_transform[0] = 2.0;
    arb_transform[1] = 0.3;
    arb_transform[2] = 0.2;
    arb_transform[4] = 0.1;
    arb_transform[5] = 1.5;
    arb_transform[6] = 0.4;
    arb_transform[8] = 0.2;
    arb_transform[9] = 0.1;
    arb_transform[10] = 1.7;
    MAT_DELTAS(arb_transform, -20.0, 15.0, 8.0);
    for (size_t i = 0; i < 8; i++)
	MAT4X3PNT(arb_b[i], arb_transform, arb_a[i]);

    if (mk_sph(wdbp, "sphere_a.s", center_a, 2.0) ||
	mk_sph(wdbp, "sphere_b.s", center_b, 4.0) ||
	mk_sph(wdbp, "sphere_c.s", center_c, 8.0) ||
	mk_sph(wdbp, "root_a.s", root_center_a, 3.0) ||
	mk_sph(wdbp, "root_b.s", root_center_b, 6.0) ||
	mk_eto(wdbp, "eto_a.s", eto_center_a, eto_normal_a, eto_major_a, 8.0, 2.0) ||
	mk_eto(wdbp, "eto_b.s", eto_center_b, eto_normal_b, eto_major_b, 16.0, 4.0) ||
	mk_tgc(wdbp, "tgc_a.s", tgc_base_a, tgc_height_a,
	    tgc_a_a, tgc_b_a, tgc_c_a, tgc_d_a) ||
	mk_tgc(wdbp, "tgc_b.s", tgc_base_b, tgc_height_b,
	    tgc_a_b, tgc_b_b, tgc_c_b, tgc_d_b) ||
	mk_arb8(wdbp, "arb_a.s", &arb_a[0][X]) ||
	mk_arb8(wdbp, "arb_b.s", &arb_b[0][X])) {
	wdb_close(wdbp);
	bu_file_delete(path.c_str());
	return std::string();
    }

    if (db5_update_attribute("root_a.s", "unpush_test", "preserved", wdbp->dbip) ||
	db5_update_attribute("root_b.s", "unpush_test", "preserved", wdbp->dbip)) {
	wdb_close(wdbp);
	bu_file_delete(path.c_str());
	return std::string();
    }

    struct wmember selected;
    BU_LIST_INIT(&selected.l);
    mk_addmember("sphere_a.s", &selected.l, nullptr, WMOP_UNION);
    mk_addmember("sphere_b.s", &selected.l, nullptr, WMOP_UNION);
    mk_addmember("sphere_c.s", &selected.l, nullptr, WMOP_UNION);
    mk_addmember("eto_a.s", &selected.l, nullptr, WMOP_UNION);
    mk_addmember("eto_b.s", &selected.l, nullptr, WMOP_UNION);
    mk_addmember("tgc_a.s", &selected.l, nullptr, WMOP_UNION);
    mk_addmember("tgc_b.s", &selected.l, nullptr, WMOP_UNION);
    mk_addmember("arb_a.s", &selected.l, nullptr, WMOP_UNION);
    mk_addmember("arb_b.s", &selected.l, nullptr, WMOP_UNION);
    if (mk_lcomb(wdbp, "selected.c", &selected, 0, nullptr, nullptr, nullptr, 0)) {
	wdb_close(wdbp);
	bu_file_delete(path.c_str());
	return std::string();
    }

    struct wmember external;
    BU_LIST_INIT(&external.l);
    mk_addmember("sphere_b.s", &external.l, nullptr, WMOP_UNION);
    if (mk_lcomb(wdbp, "external.c", &external, 0, nullptr, nullptr, nullptr, 0)) {
	wdb_close(wdbp);
	bu_file_delete(path.c_str());
	return std::string();
    }

    struct wmember root_external;
    BU_LIST_INIT(&root_external.l);
    mk_addmember("root_b.s", &root_external.l, nullptr, WMOP_UNION);
    if (mk_lcomb(wdbp, "root_external.c", &root_external, 0, nullptr, nullptr,
	    nullptr, 0)) {
	wdb_close(wdbp);
	bu_file_delete(path.c_str());
	return std::string();
    }

    if (expose_selected) {
	struct wmember selected_external;
	BU_LIST_INIT(&selected_external.l);
	mk_addmember("selected.c", &selected_external.l, nullptr, WMOP_UNION);
	if (mk_lcomb(wdbp, "selected_external.c", &selected_external, 0, nullptr,
		nullptr, nullptr, 0)) {
	    wdb_close(wdbp);
	    bu_file_delete(path.c_str());
	    return std::string();
	}
    }

    wdb_close(wdbp);
    return path;
}


static std::string
make_combination_database()
{
    std::string path = temporary_database_path("unpush_combinations");
    struct rt_wdb *wdbp = wdb_fopen(path.c_str());
    if (!wdbp)
	return std::string();
    mk_id(wdbp, "unpush combination regression");

    point_t a_anchor = {0.0, 0.0, 0.0};
    point_t a_second = {5.0, 0.0, 0.0};
    point_t b_anchor = {20.0, 0.0, 0.0};
    point_t b_second = {30.0, 0.0, 0.0};
    bool failed = mk_sph(wdbp, "a_anchor.s", a_anchor, 1.0) ||
	mk_sph(wdbp, "a_second.s", a_second, 2.0) ||
	mk_sph(wdbp, "b_anchor.s", b_anchor, 2.0) ||
	mk_sph(wdbp, "b_second.s", b_second, 4.0);

    struct wmember part_a;
    BU_LIST_INIT(&part_a.l);
    mk_addmember("a_anchor.s", &part_a.l, nullptr, WMOP_UNION);
    mk_addmember("a_second.s", &part_a.l, nullptr, WMOP_UNION);
    failed = failed || mk_lcomb(wdbp, "part_a.c", &part_a, 0, nullptr,
	nullptr, nullptr, 0);

    struct wmember part_b;
    BU_LIST_INIT(&part_b.l);
    mk_addmember("b_anchor.s", &part_b.l, nullptr, WMOP_UNION);
    mk_addmember("b_second.s", &part_b.l, nullptr, WMOP_UNION);
    failed = failed || mk_lcomb(wdbp, "part_b.c", &part_b, 0, nullptr,
	nullptr, nullptr, 0);

    struct wmember part_attribute;
    BU_LIST_INIT(&part_attribute.l);
    mk_addmember("a_anchor.s", &part_attribute.l, nullptr, WMOP_UNION);
    mk_addmember("a_second.s", &part_attribute.l, nullptr, WMOP_UNION);
    failed = failed || mk_lcomb(wdbp, "part_attribute.c", &part_attribute, 0,
	nullptr, nullptr, nullptr, 0) ||
	db5_update_attribute("part_attribute.c", "unpush_test", "distinct",
	    wdbp->dbip);

    struct wmember part_reverse;
    BU_LIST_INIT(&part_reverse.l);
    mk_addmember("a_second.s", &part_reverse.l, nullptr, WMOP_UNION);
    mk_addmember("a_anchor.s", &part_reverse.l, nullptr, WMOP_UNION);
    failed = failed || mk_lcomb(wdbp, "part_reverse.c", &part_reverse, 0,
	nullptr, nullptr, nullptr, 0);

    struct wmember region_a;
    BU_LIST_INIT(&region_a.l);
    mk_addmember("part_a.c", &region_a.l, nullptr, WMOP_UNION);
    failed = failed || mk_lcomb(wdbp, "region_a.r", &region_a, 1, nullptr,
	nullptr, nullptr, 0);

    struct wmember region_b;
    BU_LIST_INIT(&region_b.l);
    mk_addmember("part_b.c", &region_b.l, nullptr, WMOP_UNION);
    failed = failed || mk_lcomb(wdbp, "region_b.r", &region_b, 1, nullptr,
	nullptr, nullptr, 0);

    struct wmember root;
    BU_LIST_INIT(&root.l);
    mk_addmember("region_a.r", &root.l, nullptr, WMOP_UNION);
    mk_addmember("region_b.r", &root.l, nullptr, WMOP_UNION);
	mk_addmember("part_attribute.c", &root.l, nullptr, WMOP_UNION);
	mk_addmember("part_reverse.c", &root.l, nullptr, WMOP_UNION);
    failed = failed || mk_lcomb(wdbp, "root.c", &root, 0, nullptr, nullptr,
	nullptr, 0);

    wdb_close(wdbp);
    if (failed) {
	bu_file_delete(path.c_str());
	return std::string();
    }
    return path;
}


static bool
contains(const char *text, const char *expected)
{
    return text && std::string(text).find(expected) != std::string::npos;
}


static bool
get_bounds(struct ged *gedp, const char *name, point_t minimum, point_t maximum)
{
    const char *objects[] = {name};
    return rt_obj_bounds(gedp->ged_result_str, gedp->dbip, 1, objects, 1,
	minimum, maximum) == BRLCAD_OK;
}


static bool
single_leaf_combination(struct db_i *dbip, const char *name,
			const char *expected_child)
{
    struct directory *dp = db_lookup(dbip, name, LOOKUP_QUIET);
    if (dp == RT_DIR_NULL || !(dp->d_flags & RT_DIR_COMB) ||
	dp->d_minor_type != ID_COMBINATION)
	return false;

    struct rt_db_internal intern;
    RT_DB_INTERNAL_INIT(&intern);
    if (rt_db_get_internal(&intern, dp, dbip, nullptr) < 0)
	return false;
    const auto *comb = static_cast<const struct rt_comb_internal *>(intern.idb_ptr);
    bool valid = comb && comb->tree && comb->tree->tr_op == OP_DB_LEAF &&
	BU_STR_EQUAL(comb->tree->tr_l.tl_name, expected_child) &&
	(!comb->tree->tr_l.tl_mat ||
	    bn_mat_ck("unpush regression wrapper", comb->tree->tr_l.tl_mat) == 0);
    rt_db_free_internal(&intern);
    return valid;
}


static bool
combination_is_region(struct db_i *dbip, const char *name)
{
    struct directory *dp = db_lookup(dbip, name, LOOKUP_QUIET);
    if (dp == RT_DIR_NULL || !(dp->d_flags & RT_DIR_COMB) ||
	dp->d_minor_type != ID_COMBINATION)
	return false;

    struct rt_db_internal intern;
    RT_DB_INTERNAL_INIT(&intern);
    if (rt_db_get_internal(&intern, dp, dbip, nullptr) < 0)
	return false;
    const auto *comb = static_cast<const struct rt_comb_internal *>(intern.idb_ptr);
    bool is_region = comb && comb->region_flag;
    rt_db_free_internal(&intern);
    return is_region;
}


static bool
attribute_equals(struct db_i *dbip, const char *name,
		 const char *attribute, const char *expected)
{
    struct directory *dp = db_lookup(dbip, name, LOOKUP_QUIET);
    if (dp == RT_DIR_NULL)
	return false;
    struct bu_attribute_value_set attributes = BU_AVS_INIT_ZERO;
    if (db5_get_attributes(dbip, &attributes, dp) < 0)
	return false;
    const char *value = bu_avs_get(&attributes, attribute);
    bool equal = value && BU_STR_EQUAL(value, expected);
    bu_avs_free(&attributes);
    return equal;
}


static bool
primitive_type_equals(struct db_i *dbip, const char *name, int type)
{
    struct directory *dp = db_lookup(dbip, name, LOOKUP_QUIET);
    return dp != RT_DIR_NULL && !(dp->d_flags & RT_DIR_COMB) &&
	dp->d_minor_type == type;
}


struct rollback_fault_state {
    const char *target = nullptr;
    bool armed = true;
    bool injected = false;
};


static bool
rename_first_leaf(union tree *tree, const char *replacement)
{
    if (!tree)
	return false;
    switch (tree->tr_op) {
	case OP_DB_LEAF:
	    bu_free(tree->tr_l.tl_name, "unpush regression fault leaf");
	    tree->tr_l.tl_name = bu_strdup(replacement);
	    return true;
	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
	    return rename_first_leaf(tree->tr_b.tb_left, replacement) ||
		rename_first_leaf(tree->tr_b.tb_right, replacement);
	case OP_NOT:
	case OP_GUARD:
	case OP_XNOP:
	    return rename_first_leaf(tree->tr_b.tb_left, replacement);
	default:
	    return false;
    }
}


static void
inject_validation_failure(struct db_i *dbip, struct directory *dp,
			  int UNUSED(mode), void *data)
{
    auto *state = static_cast<rollback_fault_state *>(data);
    if (!state || !state->armed || !state->target ||
	!BU_STR_EQUAL(dp->d_namep, state->target))
	return;

    state->armed = false;
    struct rt_db_internal intern;
    RT_DB_INTERNAL_INIT(&intern);
    if (rt_db_get_internal(&intern, dp, dbip, nullptr) < 0)
	return;
    auto *comb = static_cast<struct rt_comb_internal *>(intern.idb_ptr);
    if (intern.idb_minor_type == ID_COMBINATION && comb &&
	rename_first_leaf(comb->tree, "__unpush_validation_fault__") &&
	rt_db_put_internal(dp, dbip, &intern) == 0) {
	state->injected = true;
    } else if (intern.idb_ptr) {
	rt_db_free_internal(&intern);
    }
}


static bool
run_analytic_family_regression(const std::string &database,
			       const char *root, const char *label,
			       const char *mode, size_t expected_groups,
			       const std::vector<const char *> &original_names)
{
    if (database.empty())
	return false;

    const std::string before = file_contents(database);
    struct ged *gedp = ged_open("db", database.c_str(), 1);
    if (!gedp)
	return false;

    point_t minimum;
    point_t maximum;
    point_t after_minimum;
    point_t after_maximum;
    bool bounds_before = get_bounds(gedp, root, minimum, maximum);
    const char *dry_run[] = {"unpush", "-D", "-m", mode, root};
    int result = ged_exec(gedp, 5, dry_run);
    const std::string dry_report = bu_vls_cstr(gedp->ged_result_str);
    const std::string primitive_count = "primitive objects: " +
	std::to_string(original_names.size());
    const std::string canonical_count = "canonicalized: " +
	std::to_string(original_names.size());
    const std::string group_count = "verified groups: " +
	std::to_string(expected_groups);
    const std::string grouped_count = "grouped objects: " +
	std::to_string(original_names.size());
    bool ok = result == BRLCAD_OK &&
	contains(dry_report.c_str(), primitive_count.c_str()) &&
	contains(dry_report.c_str(), canonical_count.c_str()) &&
	contains(dry_report.c_str(), group_count.c_str()) &&
	contains(dry_report.c_str(), grouped_count.c_str()) &&
	before == file_contents(database);
    if (!ok)
	bu_log("unexpected %s analysis:\n%s\n", label, dry_report.c_str());

    const char *write[] = {"unpush", "-m", mode, root};
    result = ged_exec(gedp, 4, write);
    const std::string write_report = bu_vls_cstr(gedp->ged_result_str);
    const std::string written_count = "canonical objects written: " +
	std::to_string(expected_groups);
    const std::string removed_count = "original objects removed: " +
	std::to_string(original_names.size());
    bool originals_removed = true;
    for (const char *name : original_names) {
	if (db_lookup(gedp->dbip, name, LOOKUP_QUIET) != RT_DIR_NULL)
	    originals_removed = false;
    }
    bool bounds_preserved = bounds_before &&
	get_bounds(gedp, root, after_minimum, after_maximum) &&
	VNEAR_EQUAL(minimum, after_minimum, BN_TOL_DIST) &&
	VNEAR_EQUAL(maximum, after_maximum, BN_TOL_DIST);
    ok = ok && result == BRLCAD_OK &&
	contains(write_report.c_str(), written_count.c_str()) &&
	contains(write_report.c_str(), "parent combinations rewritten: 1") &&
	contains(write_report.c_str(), removed_count.c_str()) &&
	originals_removed && bounds_preserved;
    if (!ok)
	bu_log("unexpected %s rewrite:\n%s\n", label, write_report.c_str());

    result = ged_exec(gedp, 5, dry_run);
    const std::string second_report = bu_vls_cstr(gedp->ged_result_str);
    ok = ok && result == BRLCAD_OK &&
	contains(second_report.c_str(), "verified groups: 0");
    if (!ok)
	bu_log("unexpected second %s analysis:\n%s\n", label,
	    second_report.c_str());

    ged_close(gedp);
    return ok;
}


int
main(int argc, char *argv[])
{
    bu_setprogname(argv[0]);
    if (argc > 1) {
	if (argc < 3) {
	    bu_log("Usage: %s database.g unpush-arguments...\n", argv[0]);
	    return 1;
	}
	struct ged *corpus_gedp = ged_open("db", argv[1], 1);
	if (!corpus_gedp) {
	    bu_log("unable to open %s\n", argv[1]);
	    return 1;
	}
	std::vector<const char *> command = {"unpush"};
	for (int i = 2; i < argc; i++)
	    command.push_back(argv[i]);
	int result = ged_exec(corpus_gedp, static_cast<int>(command.size()),
	    command.data());
	bu_log("%s", bu_vls_cstr(corpus_gedp->ged_result_str));
	ged_close(corpus_gedp);
	return result == BRLCAD_OK ? 0 : 1;
    }

    std::string database = make_database();
    if (database.empty()) {
	bu_log("unable to create unpush regression database\n");
	return 1;
    }

    const std::string before = file_contents(database);
    struct ged *gedp = ged_open("db", database.c_str(), 1);
    if (!gedp) {
	bu_file_delete(database.c_str());
	return 1;
    }

    const char *selected_object[] = {"selected.c"};
    point_t bounds_before_min;
    point_t bounds_before_max;
    bool bounds_before_ok = rt_obj_bounds(gedp->ged_result_str, gedp->dbip, 1,
	selected_object, 1, bounds_before_min, bounds_before_max) == BRLCAD_OK;

    const char *dry_run[] = {"unpush", "-D", "-L", "-v", "-v", "selected.c"};
    int result = ged_exec(gedp, 6, dry_run);
    const char *report = bu_vls_cstr(gedp->ged_result_str);
    bool report_ok = result == BRLCAD_OK &&
	contains(report, "verified groups: 4") &&
	contains(report, "grouped objects: 9") &&
	contains(report, "duplicate objects: 5") &&
	contains(report, "rewritable selected references: 9") &&
	contains(report, "externally exposed grouped objects: 1") &&
	contains(report, "selected.c/sphere_a.s replacement matrix") &&
	contains(report, "selected.c/eto_a.s replacement matrix");
    if (!report_ok)
	bu_log("unexpected unpush report:\n%s\n", report);

    const std::string after_dry_run = file_contents(database);
    bool dry_run_unchanged = before == after_dry_run;
    if (!dry_run_unchanged)
	bu_log("unpush dry-run changed the database\n");

    const char *write_request[] = {"unpush", "selected.c"};
    result = ged_exec(gedp, 2, write_request);
    const char *write_report = bu_vls_cstr(gedp->ged_result_str);
    bool write_ok = result == BRLCAD_OK &&
	contains(write_report, "canonical objects written: 2") &&
	contains(write_report, "parent combinations rewritten: 2") &&
	contains(write_report, "original objects removed: 5") &&
	contains(write_report, "original objects retained: 0") &&
	contains(write_report, "groups deferred: 0");
    if (!write_ok)
	bu_log("unexpected unpush write report:\n%s\n", write_report);

    const char *post_write_dry_run[] = {"unpush", "-D", "selected.c"};
    result = ged_exec(gedp, 3, post_write_dry_run);
    const char *post_write_report = bu_vls_cstr(gedp->ged_result_str);
    bool post_write_ok = result == BRLCAD_OK &&
	contains(post_write_report, "primitive objects: 6") &&
	contains(post_write_report, "canonicalized: 6") &&
	contains(post_write_report, "verified groups: 2");
    if (!post_write_ok)
	bu_log("unexpected post-write unpush report:\n%s\n", post_write_report);

    point_t bounds_after_min;
    point_t bounds_after_max;
    bool bounds_preserved = bounds_before_ok &&
	rt_obj_bounds(gedp->ged_result_str, gedp->dbip, 1, selected_object, 1,
	    bounds_after_min, bounds_after_max) == BRLCAD_OK &&
	VNEAR_EQUAL(bounds_before_min, bounds_after_min, BN_TOL_DIST) &&
	VNEAR_EQUAL(bounds_before_max, bounds_after_max, BN_TOL_DIST);
    if (!bounds_preserved)
	bu_log("unpush did not preserve selected.c bounds\n");

    const char *old_names[] = {
	"sphere_a.s", "sphere_b.s", "sphere_c.s", "eto_a.s", "eto_b.s"
    };
    bool directory_ok = true;
    for (const char *name : old_names) {
	if (db_lookup(gedp->dbip, name, LOOKUP_QUIET) != RT_DIR_NULL)
	    directory_ok = false;
    }
    for (size_t i = 1; i <= 2; i++) {
	std::string name = "unpush_" + std::to_string(i);
	if (db_lookup(gedp->dbip, name.c_str(), LOOKUP_QUIET) == RT_DIR_NULL)
	    directory_ok = false;
    }
    const char *deferred_names[] = {"tgc_a.s", "tgc_b.s", "arb_a.s", "arb_b.s"};
    for (const char *name : deferred_names) {
	if (db_lookup(gedp->dbip, name, LOOKUP_QUIET) == RT_DIR_NULL)
	    directory_ok = false;
    }
    if (!directory_ok)
	bu_log("unpush did not leave the expected canonical directory entries\n");

    ged_close(gedp);
    const std::string after = file_contents(database);
    bool write_changed_database = before != after;
    if (!write_changed_database)
	bu_log("unpush write did not change the database\n");
    bu_file_delete(database.c_str());

    std::string local_database = make_database();
    struct ged *local_gedp = local_database.empty() ? nullptr :
	ged_open("db", local_database.c_str(), 1);
    bool local_ok = local_gedp != nullptr;
    if (local_gedp) {
	point_t local_selected_min;
	point_t local_selected_max;
	point_t local_external_min;
	point_t local_external_max;
	bool local_bounds_before = get_bounds(local_gedp, "selected.c",
	    local_selected_min, local_selected_max) &&
	    get_bounds(local_gedp, "external.c", local_external_min,
		local_external_max);
	const char *local_write[] = {"unpush", "-L", "selected.c"};
	result = ged_exec(local_gedp, 3, local_write);
	const std::string local_report = bu_vls_cstr(local_gedp->ged_result_str);
	point_t local_selected_after_min;
	point_t local_selected_after_max;
	point_t local_external_after_min;
	point_t local_external_after_max;
	bool local_bounds_preserved = local_bounds_before &&
	    get_bounds(local_gedp, "selected.c", local_selected_after_min,
		local_selected_after_max) &&
	    get_bounds(local_gedp, "external.c", local_external_after_min,
		local_external_after_max) &&
	    VNEAR_EQUAL(local_selected_min, local_selected_after_min, BN_TOL_DIST) &&
	    VNEAR_EQUAL(local_selected_max, local_selected_after_max, BN_TOL_DIST) &&
	    VNEAR_EQUAL(local_external_min, local_external_after_min, BN_TOL_DIST) &&
	    VNEAR_EQUAL(local_external_max, local_external_after_max, BN_TOL_DIST);
	local_ok = result == BRLCAD_OK &&
	    contains(local_report.c_str(), "canonical objects written: 2") &&
	    contains(local_report.c_str(), "parent combinations rewritten: 1") &&
	    contains(local_report.c_str(), "top-level wrappers written: 0") &&
	    contains(local_report.c_str(), "original objects removed: 4") &&
	    contains(local_report.c_str(), "original objects retained: 1") &&
	    contains(local_report.c_str(), "groups deferred: 0") &&
	    db_lookup(local_gedp->dbip, "sphere_a.s", LOOKUP_QUIET) == RT_DIR_NULL &&
	    db_lookup(local_gedp->dbip, "sphere_b.s", LOOKUP_QUIET) != RT_DIR_NULL &&
	    db_lookup(local_gedp->dbip, "sphere_c.s", LOOKUP_QUIET) == RT_DIR_NULL &&
	    db_lookup(local_gedp->dbip, "unpush_1", LOOKUP_QUIET) != RT_DIR_NULL &&
	    db_lookup(local_gedp->dbip, "unpush_2", LOOKUP_QUIET) != RT_DIR_NULL &&
	    single_leaf_combination(local_gedp->dbip, "external.c", "sphere_b.s") &&
	    local_bounds_preserved;
	if (!local_ok)
	    bu_log("unexpected local unpush result:\n%s\n", local_report.c_str());
	ged_close(local_gedp);
    }
    if (!local_database.empty())
	bu_file_delete(local_database.c_str());

    std::string wrapper_database = make_database();
    struct ged *wrapper_gedp = wrapper_database.empty() ? nullptr :
	ged_open("db", wrapper_database.c_str(), 1);
    bool wrapper_ok = wrapper_gedp != nullptr;
    if (wrapper_gedp) {
	point_t root_a_min;
	point_t root_a_max;
	point_t root_b_min;
	point_t root_b_max;
	point_t root_external_min;
	point_t root_external_max;
	bool wrapper_bounds_before = get_bounds(wrapper_gedp, "root_a.s",
	    root_a_min, root_a_max) &&
	    get_bounds(wrapper_gedp, "root_b.s", root_b_min, root_b_max) &&
	    get_bounds(wrapper_gedp, "root_external.c", root_external_min,
		root_external_max);
	const char *wrapper_write[] = {"unpush", "root_a.s", "root_b.s"};
	result = ged_exec(wrapper_gedp, 3, wrapper_write);
	const std::string wrapper_report = bu_vls_cstr(wrapper_gedp->ged_result_str);
	point_t root_a_after_min;
	point_t root_a_after_max;
	point_t root_b_after_min;
	point_t root_b_after_max;
	point_t root_external_after_min;
	point_t root_external_after_max;
	bool wrapper_bounds_preserved = wrapper_bounds_before &&
	    get_bounds(wrapper_gedp, "root_a.s", root_a_after_min,
		root_a_after_max) &&
	    get_bounds(wrapper_gedp, "root_b.s", root_b_after_min,
		root_b_after_max) &&
	    get_bounds(wrapper_gedp, "root_external.c", root_external_after_min,
		root_external_after_max) &&
	    VNEAR_EQUAL(root_a_min, root_a_after_min, BN_TOL_DIST) &&
	    VNEAR_EQUAL(root_a_max, root_a_after_max, BN_TOL_DIST) &&
	    VNEAR_EQUAL(root_b_min, root_b_after_min, BN_TOL_DIST) &&
	    VNEAR_EQUAL(root_b_max, root_b_after_max, BN_TOL_DIST) &&
	    VNEAR_EQUAL(root_external_min, root_external_after_min, BN_TOL_DIST) &&
	    VNEAR_EQUAL(root_external_max, root_external_after_max, BN_TOL_DIST);
	bool wrapper_structure_ok =
	    single_leaf_combination(wrapper_gedp->dbip, "root_a.s", "unpush_1") &&
	    single_leaf_combination(wrapper_gedp->dbip, "root_b.s", "unpush_1") &&
	    single_leaf_combination(wrapper_gedp->dbip, "root_external.c", "root_b.s");
	bool wrapper_attributes_ok =
	    attribute_equals(wrapper_gedp->dbip, "root_a.s", "unpush_test",
		"preserved") &&
	    attribute_equals(wrapper_gedp->dbip, "root_b.s", "unpush_test",
		"preserved") &&
	    attribute_equals(wrapper_gedp->dbip, "unpush_1", "unpush_test",
		"preserved");
	wrapper_ok = result == BRLCAD_OK &&
	    contains(wrapper_report.c_str(), "canonical objects written: 1") &&
	    contains(wrapper_report.c_str(), "parent combinations rewritten: 0") &&
	    contains(wrapper_report.c_str(), "top-level wrappers written: 2") &&
	    contains(wrapper_report.c_str(), "original objects removed: 0") &&
	    contains(wrapper_report.c_str(), "groups deferred: 0") &&
	    wrapper_structure_ok && wrapper_attributes_ok && wrapper_bounds_preserved;
	if (!wrapper_ok) {
	    bu_log("unexpected top-level wrapper result:\n%s\n", wrapper_report.c_str());
	    bu_log("  structure: %s; attributes: %s; bounds: %s\n",
		wrapper_structure_ok ? "ok" : "failed",
		wrapper_attributes_ok ? "ok" : "failed",
		wrapper_bounds_preserved ? "ok" : "failed");
	    if (!wrapper_bounds_preserved) {
		bu_log("  root_a before (%g %g %g)-(%g %g %g), after (%g %g %g)-(%g %g %g)\n",
		    V3ARGS(root_a_min), V3ARGS(root_a_max),
		    V3ARGS(root_a_after_min), V3ARGS(root_a_after_max));
		bu_log("  root_b before (%g %g %g)-(%g %g %g), after (%g %g %g)-(%g %g %g)\n",
		    V3ARGS(root_b_min), V3ARGS(root_b_max),
		    V3ARGS(root_b_after_min), V3ARGS(root_b_after_max));
		bu_log("  root_external before (%g %g %g)-(%g %g %g), after (%g %g %g)-(%g %g %g)\n",
		    V3ARGS(root_external_min), V3ARGS(root_external_max),
		    V3ARGS(root_external_after_min), V3ARGS(root_external_after_max));
	    }
	}
	ged_close(wrapper_gedp);
	/* Reopening exercises directory type reconstruction from the stored data. */
	wrapper_gedp = ged_open("db", wrapper_database.c_str(), 1);
	wrapper_ok = wrapper_ok && wrapper_gedp &&
	    single_leaf_combination(wrapper_gedp->dbip, "root_a.s", "unpush_1") &&
	    single_leaf_combination(wrapper_gedp->dbip, "root_b.s", "unpush_1");
	if (wrapper_gedp)
	    ged_close(wrapper_gedp);
    }
    if (!wrapper_database.empty())
	bu_file_delete(wrapper_database.c_str());

    std::string exposed_database = make_database(true);
    const std::string exposed_before = file_contents(exposed_database);
    struct ged *exposed_gedp = exposed_database.empty() ? nullptr :
	ged_open("db", exposed_database.c_str(), 1);
    bool exposed_ok = exposed_gedp != nullptr;
    if (exposed_gedp) {
	const char *exposed_write[] = {"unpush", "-L", "selected.c"};
	result = ged_exec(exposed_gedp, 3, exposed_write);
	const char *exposed_report = bu_vls_cstr(exposed_gedp->ged_result_str);
	exposed_ok = result == BRLCAD_OK &&
	    contains(exposed_report, "no fully-contained groups are currently safe to rewrite") &&
	    contains(exposed_report, "verified groups: 0") &&
	    contains(exposed_report, "0 deferred");
	if (!exposed_ok)
	    bu_log("unexpected exposed-ancestor result:\n%s\n", exposed_report);
	ged_close(exposed_gedp);
	exposed_ok = exposed_ok && exposed_before == file_contents(exposed_database);
    }
    if (!exposed_database.empty())
	bu_file_delete(exposed_database.c_str());

    std::string superell_database = make_superell_database();

    const std::vector<const char *> superell_names = {
	"superell_a.s", "superell_b.s"
    };
    bool superell_ok = run_analytic_family_regression(superell_database,
	"superells.c", "SUPERELL", "similarity", 1, superell_names);
    if (!superell_database.empty())
	bu_file_delete(superell_database.c_str());

    std::string axial_analytic_database = make_axial_analytic_database();
    const std::vector<const char *> axial_analytic_names = {
	"rpc_a.s", "rpc_b.s", "rhc_a.s", "rhc_b.s",
	"epa_a.s", "epa_b.s", "ehy_a.s", "ehy_b.s",
	"part_a.s", "part_b.s", "part_sphere_a.s", "part_sphere_b.s"
    };
    bool axial_analytic_ok = run_analytic_family_regression(
	axial_analytic_database, "axial_analytics.c", "RPC/RHC/EPA/EHY/PART",
	"affine", 6, axial_analytic_names);
    if (!axial_analytic_database.empty())
	bu_file_delete(axial_analytic_database.c_str());

    std::string combination_database = make_combination_database();
    const std::string combination_before = file_contents(combination_database);
    struct ged *combination_gedp = combination_database.empty() ? nullptr :
	ged_open("db", combination_database.c_str(), 1);
    bool combination_ok = combination_gedp != nullptr;
    if (combination_gedp) {
	point_t combination_minimum;
	point_t combination_maximum;
	point_t combination_after_minimum;
	point_t combination_after_maximum;
	bool combination_bounds_before = get_bounds(combination_gedp, "root.c",
	    combination_minimum, combination_maximum);
	const char *combination_dry_run[] = {"unpush", "-D", "-v", "root.c"};
	result = ged_exec(combination_gedp, 4, combination_dry_run);
	const char *combination_report =
	    bu_vls_cstr(combination_gedp->ged_result_str);
	combination_ok = result == BRLCAD_OK &&
	    contains(combination_report, "combination objects: 7") &&
	    contains(combination_report, "canonicalized combinations: 7") &&
	    contains(combination_report, "combination failures: 0") &&
	    contains(combination_report, "verified combination groups: 2") &&
	    contains(combination_report, "grouped combination objects: 4") &&
	    contains(combination_report, "duplicate combination objects: 2") &&
	    contains(combination_report, "potential facetize boolean reuses: 2") &&
	    contains(combination_report,
		"combination group 1: part_a.c part_b.c") &&
	    contains(combination_report,
		"combination group 2: region_a.r region_b.r");
	if (!combination_ok)
	    bu_log("unexpected combination analysis:\n%s\n", combination_report);
	combination_ok = combination_ok &&
	    combination_before == file_contents(combination_database);

	const char *combination_write[] = {"unpush", "root.c"};
	result = ged_exec(combination_gedp, 2, combination_write);
	const std::string combination_write_report =
	    bu_vls_cstr(combination_gedp->ged_result_str);
	bool combination_bounds_preserved = combination_bounds_before &&
	    get_bounds(combination_gedp, "root.c", combination_after_minimum,
		combination_after_maximum) &&
	    VNEAR_EQUAL(combination_minimum, combination_after_minimum,
		BN_TOL_DIST) &&
	    VNEAR_EQUAL(combination_maximum, combination_after_maximum,
		BN_TOL_DIST);
	combination_ok = combination_ok && result == BRLCAD_OK &&
	    contains(combination_write_report.c_str(),
		"canonical objects written: 1") &&
	    contains(combination_write_report.c_str(),
		"parent combinations rewritten: 6") &&
	    contains(combination_write_report.c_str(),
		"original objects removed: 4") &&
	    contains(combination_write_report.c_str(),
		"combination groups consolidated: 2") &&
	    contains(combination_write_report.c_str(),
		"combination objects removed: 2") &&
	    contains(combination_write_report.c_str(),
		"combination objects retained: 0") &&
	    db_lookup(combination_gedp->dbip, "part_b.c", LOOKUP_QUIET) ==
		RT_DIR_NULL &&
	    db_lookup(combination_gedp->dbip, "region_b.r", LOOKUP_QUIET) ==
		RT_DIR_NULL && combination_bounds_preserved;
	if (!combination_ok)
	    bu_log("unexpected combination rewrite:\n%s\n",
		combination_write_report.c_str());

	const char *combination_second_dry_run[] = {
	    "unpush", "-D", "-m", "similarity", "root.c"
	};
	result = ged_exec(combination_gedp, 5, combination_second_dry_run);
	const char *combination_second_report =
	    bu_vls_cstr(combination_gedp->ged_result_str);
	combination_ok = combination_ok && result == BRLCAD_OK &&
	    contains(combination_second_report, "verified groups: 0") &&
	    contains(combination_second_report,
		"verified combination groups: 0");
	if (!combination_ok)
	    bu_log("unexpected second combination analysis:\n%s\n",
		combination_second_report);
	ged_close(combination_gedp);
    }
    if (!combination_database.empty())
	bu_file_delete(combination_database.c_str());

    std::string combination_wrapper_database = make_combination_database();
    struct ged *combination_wrapper_gedp = combination_wrapper_database.empty() ?
	nullptr : ged_open("db", combination_wrapper_database.c_str(), 1);
    bool combination_wrapper_ok = combination_wrapper_gedp != nullptr;
    if (combination_wrapper_gedp) {
	point_t part_minimum;
	point_t part_maximum;
	point_t root_minimum;
	point_t root_maximum;
	bool wrapper_bounds_before = get_bounds(combination_wrapper_gedp,
	    "part_b.c", part_minimum, part_maximum) &&
	    get_bounds(combination_wrapper_gedp, "root.c", root_minimum,
		root_maximum);
	const char *combination_wrapper_write[] = {
	    "unpush", "part_a.c", "part_b.c"
	};
	result = ged_exec(combination_wrapper_gedp, 3,
	    combination_wrapper_write);
	const std::string wrapper_report =
	    bu_vls_cstr(combination_wrapper_gedp->ged_result_str);
	point_t part_after_minimum;
	point_t part_after_maximum;
	point_t root_after_minimum;
	point_t root_after_maximum;
	bool wrapper_bounds_preserved = wrapper_bounds_before &&
	    get_bounds(combination_wrapper_gedp, "part_b.c",
		part_after_minimum, part_after_maximum) &&
	    get_bounds(combination_wrapper_gedp, "root.c",
		root_after_minimum, root_after_maximum) &&
	    VNEAR_EQUAL(part_minimum, part_after_minimum, BN_TOL_DIST) &&
	    VNEAR_EQUAL(part_maximum, part_after_maximum, BN_TOL_DIST) &&
	    VNEAR_EQUAL(root_minimum, root_after_minimum, BN_TOL_DIST) &&
	    VNEAR_EQUAL(root_maximum, root_after_maximum, BN_TOL_DIST);
	combination_wrapper_ok = result == BRLCAD_OK &&
	    contains(wrapper_report.c_str(), "canonical objects written: 1") &&
	    contains(wrapper_report.c_str(),
		"parent combinations rewritten: 3") &&
	    contains(wrapper_report.c_str(),
		"top-level combination wrappers written: 1") &&
	    contains(wrapper_report.c_str(),
		"combination groups consolidated: 1") &&
	    contains(wrapper_report.c_str(),
		"combination objects removed: 0") &&
	    single_leaf_combination(combination_wrapper_gedp->dbip,
		"part_b.c", "part_a.c") && wrapper_bounds_preserved;
	if (!combination_wrapper_ok)
	    bu_log("unexpected top-level combination wrapper result:\n%s\n",
		wrapper_report.c_str());

	const char *wrapper_second_dry_run[] = {
	    "unpush", "-D", "-m", "similarity", "part_a.c", "part_b.c"
	};
	result = ged_exec(combination_wrapper_gedp, 6,
	    wrapper_second_dry_run);
	const char *wrapper_second_report =
	    bu_vls_cstr(combination_wrapper_gedp->ged_result_str);
	combination_wrapper_ok = combination_wrapper_ok &&
	    result == BRLCAD_OK &&
	    contains(wrapper_second_report, "verified groups: 0") &&
	    contains(wrapper_second_report,
		"verified combination groups: 0");
	if (!combination_wrapper_ok)
	    bu_log("unexpected second wrapper analysis:\n%s\n",
		wrapper_second_report);
	ged_close(combination_wrapper_gedp);

	/* Reopen to verify that the preserved name and wrapper type survive the
	 * in-memory directory state used during the rewrite. */
	combination_wrapper_gedp = ged_open("db",
	    combination_wrapper_database.c_str(), 1);
	combination_wrapper_ok = combination_wrapper_ok &&
	    combination_wrapper_gedp &&
	    single_leaf_combination(combination_wrapper_gedp->dbip,
		"part_b.c", "part_a.c");
	if (combination_wrapper_gedp)
	    ged_close(combination_wrapper_gedp);
    }
    if (!combination_wrapper_database.empty())
	bu_file_delete(combination_wrapper_database.c_str());

    std::string combination_wrapper_rollback_database =
	make_combination_database();
    struct ged *combination_wrapper_rollback_gedp =
	combination_wrapper_rollback_database.empty() ? nullptr :
	ged_open("db", combination_wrapper_rollback_database.c_str(), 1);
    bool combination_wrapper_rollback_ok =
	combination_wrapper_rollback_gedp != nullptr;
    if (combination_wrapper_rollback_gedp) {
	point_t rollback_minimum;
	point_t rollback_maximum;
	point_t rollback_after_minimum;
	point_t rollback_after_maximum;
	bool rollback_bounds_before = get_bounds(
	    combination_wrapper_rollback_gedp, "root.c", rollback_minimum,
	    rollback_maximum);
	rollback_fault_state fault;
	fault.target = "part_b.c";
	bool callback_added = db_add_changed_clbk(
	    combination_wrapper_rollback_gedp->dbip,
	    inject_validation_failure, &fault) == 0;
	const char *wrapper_rollback_write[] = {
	    "unpush", "part_a.c", "part_b.c"
	};
	result = callback_added ? ged_exec(combination_wrapper_rollback_gedp,
	    3, wrapper_rollback_write) : BRLCAD_ERROR;
	const std::string rollback_report =
	    bu_vls_cstr(combination_wrapper_rollback_gedp->ged_result_str);
	bool callback_removed = callback_added && db_rm_changed_clbk(
	    combination_wrapper_rollback_gedp->dbip,
	    inject_validation_failure, &fault) == 1;
	bool rollback_bounds_preserved = rollback_bounds_before &&
	    get_bounds(combination_wrapper_rollback_gedp, "root.c",
		rollback_after_minimum, rollback_after_maximum) &&
	    VNEAR_EQUAL(rollback_minimum, rollback_after_minimum,
		BN_TOL_DIST) &&
	    VNEAR_EQUAL(rollback_maximum, rollback_after_maximum,
		BN_TOL_DIST);
	const char *restored_names[] = {
	    "a_anchor.s", "a_second.s", "b_anchor.s", "b_second.s"
	};
	bool originals_restored = true;
	for (const char *name : restored_names) {
	    if (db_lookup(combination_wrapper_rollback_gedp->dbip, name,
		    LOOKUP_QUIET) == RT_DIR_NULL)
		originals_restored = false;
	}
	combination_wrapper_rollback_ok = callback_added && callback_removed &&
	    fault.injected && result == BRLCAD_ERROR &&
	    contains(rollback_report.c_str(),
		"post-write validation failed") &&
	    contains(rollback_report.c_str(), "original objects restored") &&
	    !single_leaf_combination(combination_wrapper_rollback_gedp->dbip,
		"part_b.c", "part_a.c") && originals_restored &&
	    db_lookup(combination_wrapper_rollback_gedp->dbip, "unpush_1",
		LOOKUP_QUIET) == RT_DIR_NULL && rollback_bounds_preserved;
	if (!combination_wrapper_rollback_ok)
	    bu_log("unexpected combination wrapper rollback result:\n%s\n",
		rollback_report.c_str());
	ged_close(combination_wrapper_rollback_gedp);
    }
    if (!combination_wrapper_rollback_database.empty())
	bu_file_delete(combination_wrapper_rollback_database.c_str());

    std::string region_root_database = make_combination_database();
    struct ged *region_root_gedp = region_root_database.empty() ? nullptr :
	ged_open("db", region_root_database.c_str(), 1);
    bool region_root_ok = region_root_gedp != nullptr;
    if (region_root_gedp) {
	point_t root_minimum;
	point_t root_maximum;
	point_t root_after_minimum;
	point_t root_after_maximum;
	bool region_bounds_before = get_bounds(region_root_gedp, "root.c",
	    root_minimum, root_maximum);
	const char *region_root_write[] = {
	    "unpush", "region_a.r", "region_b.r"
	};
	result = ged_exec(region_root_gedp, 3, region_root_write);
	const std::string region_report =
	    bu_vls_cstr(region_root_gedp->ged_result_str);
	bool region_bounds_preserved = region_bounds_before &&
	    get_bounds(region_root_gedp, "root.c", root_after_minimum,
		root_after_maximum) &&
	    VNEAR_EQUAL(root_minimum, root_after_minimum, BN_TOL_DIST) &&
	    VNEAR_EQUAL(root_maximum, root_after_maximum, BN_TOL_DIST);
	region_root_ok = result == BRLCAD_OK &&
	    contains(region_report.c_str(),
		"top-level combination wrappers written: 0") &&
	    contains(region_report.c_str(),
		"combination groups consolidated: 1") &&
	    contains(region_report.c_str(),
		"combination groups deferred: 1") &&
	    db_lookup(region_root_gedp->dbip, "part_b.c", LOOKUP_QUIET) ==
		RT_DIR_NULL &&
	    single_leaf_combination(region_root_gedp->dbip, "region_b.r",
		"part_a.c") &&
	    combination_is_region(region_root_gedp->dbip, "region_b.r") &&
	    region_bounds_preserved;
	if (!region_root_ok)
	    bu_log("unexpected top-level region result:\n%s\n",
		region_report.c_str());
	ged_close(region_root_gedp);
    }
    if (!region_root_database.empty())
	bu_file_delete(region_root_database.c_str());

    std::string combination_rollback_database = make_combination_database();
    struct ged *combination_rollback_gedp = combination_rollback_database.empty() ?
	nullptr : ged_open("db", combination_rollback_database.c_str(), 1);
    bool combination_rollback_ok = combination_rollback_gedp != nullptr;
    if (combination_rollback_gedp) {
	point_t rollback_minimum;
	point_t rollback_maximum;
	point_t rollback_after_minimum;
	point_t rollback_after_maximum;
	bool rollback_bounds_before = get_bounds(combination_rollback_gedp,
	    "root.c", rollback_minimum, rollback_maximum);
	rollback_fault_state fault;
	fault.target = "region_b.r";
	bool callback_added = db_add_changed_clbk(
	    combination_rollback_gedp->dbip, inject_validation_failure,
	    &fault) == 0;
	const char *combination_rollback_write[] = {"unpush", "root.c"};
	result = callback_added ? ged_exec(combination_rollback_gedp, 2,
	    combination_rollback_write) : BRLCAD_ERROR;
	const std::string rollback_report =
	    bu_vls_cstr(combination_rollback_gedp->ged_result_str);
	bool callback_removed = callback_added && db_rm_changed_clbk(
	    combination_rollback_gedp->dbip, inject_validation_failure,
	    &fault) == 1;
	bool rollback_bounds_preserved = rollback_bounds_before &&
	    get_bounds(combination_rollback_gedp, "root.c",
		rollback_after_minimum, rollback_after_maximum) &&
	    VNEAR_EQUAL(rollback_minimum, rollback_after_minimum, BN_TOL_DIST) &&
	    VNEAR_EQUAL(rollback_maximum, rollback_after_maximum, BN_TOL_DIST);
	const char *restored_names[] = {
	    "a_anchor.s", "a_second.s", "b_anchor.s", "b_second.s",
	    "part_b.c", "region_b.r"
	};
	bool originals_restored = true;
	for (const char *name : restored_names) {
	    if (db_lookup(combination_rollback_gedp->dbip, name,
		    LOOKUP_QUIET) == RT_DIR_NULL)
		originals_restored = false;
	}
	combination_rollback_ok = callback_added && callback_removed &&
	    fault.injected && result == BRLCAD_ERROR &&
	    contains(rollback_report.c_str(), "post-write validation failed") &&
	    contains(rollback_report.c_str(), "original objects restored") &&
	    db_lookup(combination_rollback_gedp->dbip, "unpush_1",
		LOOKUP_QUIET) == RT_DIR_NULL && originals_restored &&
	    rollback_bounds_preserved;
	if (!combination_rollback_ok)
	    bu_log("unexpected combination rollback result:\n%s\n",
		rollback_report.c_str());
	ged_close(combination_rollback_gedp);
    }
    if (!combination_rollback_database.empty())
	bu_file_delete(combination_rollback_database.c_str());

    std::string rollback_database = make_database();
    struct ged *rollback_gedp = rollback_database.empty() ? nullptr :
	ged_open("db", rollback_database.c_str(), 1);
    bool rollback_ok = rollback_gedp != nullptr;
    if (rollback_gedp) {
	point_t rollback_min;
	point_t rollback_max;
	point_t rollback_after_min;
	point_t rollback_after_max;
	bool rollback_bounds_before = get_bounds(rollback_gedp, "selected.c",
	    rollback_min, rollback_max);
	rollback_fault_state fault;
	fault.target = "selected.c";
	bool callback_added = db_add_changed_clbk(rollback_gedp->dbip,
	    inject_validation_failure, &fault) == 0;
	const char *rollback_write[] = {"unpush", "selected.c"};
	result = callback_added ? ged_exec(rollback_gedp, 2, rollback_write) :
	    BRLCAD_ERROR;
	const std::string rollback_report = bu_vls_cstr(rollback_gedp->ged_result_str);
	bool callback_removed = callback_added &&
	    db_rm_changed_clbk(rollback_gedp->dbip, inject_validation_failure,
		&fault) == 1;
	bool rollback_bounds_preserved = rollback_bounds_before &&
	    get_bounds(rollback_gedp, "selected.c", rollback_after_min,
		rollback_after_max) &&
	    VNEAR_EQUAL(rollback_min, rollback_after_min, BN_TOL_DIST) &&
	    VNEAR_EQUAL(rollback_max, rollback_after_max, BN_TOL_DIST);
	const char *restored_names[] = {
	    "sphere_a.s", "sphere_b.s", "sphere_c.s", "eto_a.s", "eto_b.s"
	};
	bool originals_restored = true;
	for (const char *name : restored_names) {
	    if (db_lookup(rollback_gedp->dbip, name, LOOKUP_QUIET) == RT_DIR_NULL)
		originals_restored = false;
	}
	rollback_ok = callback_added && callback_removed && fault.injected &&
	    result == BRLCAD_ERROR &&
	    contains(rollback_report.c_str(), "post-write validation failed") &&
	    contains(rollback_report.c_str(), "original objects restored") &&
	    db_lookup(rollback_gedp->dbip, "unpush_1", LOOKUP_QUIET) == RT_DIR_NULL &&
	    db_lookup(rollback_gedp->dbip, "unpush_2", LOOKUP_QUIET) == RT_DIR_NULL &&
	    originals_restored && rollback_bounds_preserved;
	if (!rollback_ok)
	    bu_log("unexpected parent rollback result:\n%s\n", rollback_report.c_str());
	ged_close(rollback_gedp);
    }
    if (!rollback_database.empty())
	bu_file_delete(rollback_database.c_str());

    std::string wrapper_rollback_database = make_database();
    struct ged *wrapper_rollback_gedp = wrapper_rollback_database.empty() ? nullptr :
	ged_open("db", wrapper_rollback_database.c_str(), 1);
    bool wrapper_rollback_ok = wrapper_rollback_gedp != nullptr;
    if (wrapper_rollback_gedp) {
	point_t wrapper_rollback_min;
	point_t wrapper_rollback_max;
	point_t wrapper_rollback_after_min;
	point_t wrapper_rollback_after_max;
	bool wrapper_rollback_bounds_before = get_bounds(wrapper_rollback_gedp,
	    "root_external.c", wrapper_rollback_min, wrapper_rollback_max);
	rollback_fault_state fault;
	fault.target = "root_a.s";
	bool callback_added = db_add_changed_clbk(wrapper_rollback_gedp->dbip,
	    inject_validation_failure, &fault) == 0;
	const char *wrapper_rollback_write[] = {"unpush", "root_a.s", "root_b.s"};
	result = callback_added ? ged_exec(wrapper_rollback_gedp, 3,
	    wrapper_rollback_write) : BRLCAD_ERROR;
	const std::string wrapper_rollback_report =
	    bu_vls_cstr(wrapper_rollback_gedp->ged_result_str);
	bool callback_removed = callback_added &&
	    db_rm_changed_clbk(wrapper_rollback_gedp->dbip,
		inject_validation_failure, &fault) == 1;
	bool wrapper_rollback_bounds_preserved = wrapper_rollback_bounds_before &&
	    get_bounds(wrapper_rollback_gedp, "root_external.c",
		wrapper_rollback_after_min, wrapper_rollback_after_max) &&
	    VNEAR_EQUAL(wrapper_rollback_min, wrapper_rollback_after_min,
		BN_TOL_DIST) &&
	    VNEAR_EQUAL(wrapper_rollback_max, wrapper_rollback_after_max,
		BN_TOL_DIST);
	bool wrapper_types_restored =
	    primitive_type_equals(wrapper_rollback_gedp->dbip, "root_a.s", ID_ELL) &&
	    primitive_type_equals(wrapper_rollback_gedp->dbip, "root_b.s", ID_ELL);
	bool wrapper_reference_restored =
	    single_leaf_combination(wrapper_rollback_gedp->dbip, "root_external.c",
		"root_b.s");
	wrapper_rollback_ok = callback_added && callback_removed && fault.injected &&
	    result == BRLCAD_ERROR &&
	    contains(wrapper_rollback_report.c_str(), "post-write validation failed") &&
	    contains(wrapper_rollback_report.c_str(), "original objects restored") &&
	    wrapper_types_restored && wrapper_reference_restored &&
	    db_lookup(wrapper_rollback_gedp->dbip, "unpush_1", LOOKUP_QUIET) ==
		RT_DIR_NULL && wrapper_rollback_bounds_preserved;
	if (!wrapper_rollback_ok) {
	    bu_log("unexpected wrapper rollback result:\n%s\n",
		wrapper_rollback_report.c_str());
	    bu_log("  callback added: %d; removed: %d; injected: %d; types: %d; reference: %d; bounds: %d; canonical absent: %d\n",
		callback_added, callback_removed, fault.injected,
		wrapper_types_restored, wrapper_reference_restored,
		wrapper_rollback_bounds_preserved,
		db_lookup(wrapper_rollback_gedp->dbip, "unpush_1", LOOKUP_QUIET) ==
		    RT_DIR_NULL);
	}
	ged_close(wrapper_rollback_gedp);
	wrapper_rollback_gedp = ged_open("db", wrapper_rollback_database.c_str(), 1);
	wrapper_rollback_ok = wrapper_rollback_ok && wrapper_rollback_gedp &&
	    primitive_type_equals(wrapper_rollback_gedp->dbip, "root_a.s", ID_ELL) &&
	    primitive_type_equals(wrapper_rollback_gedp->dbip, "root_b.s", ID_ELL);
	if (wrapper_rollback_gedp)
	    ged_close(wrapper_rollback_gedp);
    }
    if (!wrapper_rollback_database.empty())
	bu_file_delete(wrapper_rollback_database.c_str());

    return report_ok && dry_run_unchanged && write_ok && post_write_ok &&
	directory_ok && bounds_preserved && write_changed_database && local_ok &&
	wrapper_ok && exposed_ok && superell_ok && axial_analytic_ok &&
	combination_ok &&
	combination_wrapper_ok &&
	combination_wrapper_rollback_ok && region_root_ok &&
	combination_rollback_ok && rollback_ok && wrapper_rollback_ok ? 0 : 1;
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
