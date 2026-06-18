/*                         T I R E _ C S G . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2026 United States Government as represented by
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
/** @file libged/tire_csg.cpp
 *
 * Shared CSG emission helpers for the tire command.
 */

#include "common.h"

#include <math.h>
#include <stdio.h>
#include <string>
#include <string.h>

#include "vmath.h"
#include "raytrace.h"
#include "rt/db_attr.h"
#include "ged/event_txn.h"
#include "wdb.h"

#include "tire_private.h"

namespace tire_private {

CsgTransaction::CsgTransaction(rt_wdb *file, bool dry_run, struct ged *gedp) :
    file_(file),
    gedp_(gedp),
    dry_run_(dry_run)
{
}

CsgTransaction::~CsgTransaction()
{
    if (!dry_run_ && !committed_ && !created_.empty())
	(void)rollback();
}

bool
CsgTransaction::ok() const
{
    return !failed_;
}

const std::string &
CsgTransaction::error() const
{
    return error_;
}

void
CsgTransaction::set_error(const std::string &message)
{
    fail(message);
}

void
CsgTransaction::commit()
{
    if (!dry_run_ && gedp_ && (!created_.empty() || !attributed_.empty())) {
	int event_depth = ged_event_batch_begin(gedp_);
	if (!created_.empty())
	    (void)ged_event_notify_object_added(gedp_, created_.back().c_str(), NULL);
	for (const std::string &name : attributed_)
	    (void)ged_event_notify_attribute_changed(gedp_, name.c_str(), 0, NULL);
	if (event_depth > 0)
	    (void)ged_event_batch_end(gedp_, NULL);
    }
    committed_ = true;
}

void
CsgTransaction::fail(const std::string &message)
{
    if (!failed_)
	error_ = message;
    failed_ = true;
}

bool
CsgTransaction::can_write(const std::string &name)
{
    if (failed_)
	return false;

    if (created_set_.find(name) != created_set_.end()) {
	fail("duplicate generated object name: " + name);
	return false;
    }

    if (db_lookup(file_->dbip, name.c_str(), LOOKUP_QUIET) != RT_DIR_NULL) {
	fail("generated object name already exists: " + name);
	return false;
    }

    return true;
}

int
CsgTransaction::record_write(const std::string &name, int result)
{
    if (result != 0) {
	fail("failed to write generated object: " + name);
	return BRLCAD_ERROR;
    }

    created_.push_back(name);
    created_set_.insert(name);
    return BRLCAD_OK;
}

int
CsgTransaction::rollback(struct bu_vls *msg)
{
    int ret = BRLCAD_OK;

    if (dry_run_) {
	created_.clear();
	created_set_.clear();
	committed_ = true;
	return ret;
    }

    for (auto it = created_.rbegin(); it != created_.rend(); ++it) {
	struct directory *dp = db_lookup(file_->dbip, it->c_str(), LOOKUP_QUIET);
	if (dp == RT_DIR_NULL)
	    continue;

	if (db_delete(file_->dbip, dp) != 0 || db_dirdelete(file_->dbip, dp) != 0) {
	    ret = BRLCAD_ERROR;
	    if (msg)
		bu_vls_printf(msg, "failed to clean up generated object '%s'.\n", it->c_str());
	}
    }

    created_.clear();
    created_set_.clear();
    committed_ = true;
    return ret;
}

int
CsgTransaction::write_ell(const std::string &name, const point_t center, const vect_t a, const vect_t b, const vect_t c)
{
    if (!can_write(name))
	return BRLCAD_ERROR;
    if (dry_run_)
	return record_write(name, 0);
    return record_write(name, mk_ell(file_, name.c_str(), center, a, b, c));
}

int
CsgTransaction::write_rcc(const std::string &name, const point_t base, const vect_t height, fastf_t radius)
{
    if (!can_write(name))
	return BRLCAD_ERROR;
    if (dry_run_)
	return record_write(name, 0);
    return record_write(name, mk_rcc(file_, name.c_str(), base, height, radius));
}

int
CsgTransaction::write_cone(const std::string &name, const point_t base, const vect_t dirv, fastf_t height, fastf_t base_radius, fastf_t nose_radius)
{
    if (!can_write(name))
	return BRLCAD_ERROR;
    if (dry_run_)
	return record_write(name, 0);
    return record_write(name, mk_cone(file_, name.c_str(), base, dirv, height, base_radius, nose_radius));
}

int
CsgTransaction::write_eto(const std::string &name, const point_t vert, const vect_t norm, const vect_t smajor, fastf_t rrot, fastf_t sminor)
{
    if (!can_write(name))
	return BRLCAD_ERROR;
    if (dry_run_)
	return record_write(name, 0);
    return record_write(name, mk_eto(file_, name.c_str(), vert, norm, smajor, rrot, sminor));
}

int
CsgTransaction::write_sketch(const std::string &name, const struct rt_sketch_internal *skt)
{
    if (!can_write(name))
	return BRLCAD_ERROR;
    if (dry_run_)
	return record_write(name, 0);
    return record_write(name, mk_sketch(file_, name.c_str(), skt));
}

int
CsgTransaction::write_extrusion(const std::string &name, const char *sketch_name, const point_t V, const vect_t h, const vect_t u_vec, const vect_t v_vec, int keypoint)
{
    if (!can_write(name))
	return BRLCAD_ERROR;
    if (dry_run_)
	return record_write(name, 0);
    return record_write(name, mk_extrusion(file_, name.c_str(), sketch_name, V, h, u_vec, v_vec, keypoint));
}

int
CsgTransaction::write_lcomb(const std::string &name, struct wmember *headp, int region_flag, const char *shadername, const char *shaderargs, const unsigned char *rgb, int inherit)
{
    return write_comb(name, &headp->l, region_flag, shadername, shaderargs, rgb, 0, 0, 0, 0, inherit, 0, 0);
}

int
CsgTransaction::write_comb(const std::string &name, struct bu_list *headp, int region_kind, const char *shadername, const char *shaderargs, const unsigned char *rgb, int id, int air, int material, int los, int inherit, int append_ok, int gift_semantics)
{
    if (!can_write(name))
	return BRLCAD_ERROR;
    if (dry_run_)
	return record_write(name, 0);
    return record_write(name, mk_comb(file_, name.c_str(), headp, region_kind, shadername, shaderargs, rgb, id, air, material, los, inherit, append_ok, gift_semantics));
}

int
CsgTransaction::update_attributes(const std::string &name, struct bu_attribute_value_set *attrs)
{
    if (failed_) {
	bu_avs_free(attrs);
	return BRLCAD_ERROR;
    }

    if (dry_run_) {
	bu_avs_free(attrs);
	return BRLCAD_OK;
    }

    struct directory *dp = db_lookup(file_->dbip, name.c_str(), LOOKUP_QUIET);
    if (dp == RT_DIR_NULL) {
	bu_avs_free(attrs);
	fail("unable to locate generated object for attributes: " + name);
	return BRLCAD_ERROR;
    }

    if (db5_update_attributes(dp, attrs, file_->dbip) != 0) {
	fail("failed to update generated object attributes: " + name);
	return BRLCAD_ERROR;
    }

    if (attributed_set_.insert(name).second)
	attributed_.push_back(name);

    return BRLCAD_OK;
}

std::string
object_name(const char *stem, const std::string &scope, const char *extension)
{
    if (!stem || stem[0] == '\0')
	return scope + extension;

    return scope + "." + stem + extension;
}

std::string
indexed_object_name(const char *stem, const std::string &scope, int index, const char *extension)
{
    char index_str[16] = {0};
    snprintf(index_str, sizeof(index_str), "%03d", index);
    return object_name((std::string(stem) + "." + index_str).c_str(), scope, extension);
}

void
add_member(const std::string &name, struct bu_list *members, int op)
{
    (void)mk_addmember(name.c_str(), members, NULL, op);
}

void
add_member(const std::string &name, struct bu_list *members, mat_t transform, int op)
{
    (void)mk_addmember(name.c_str(), members, transform, op);
}

void
tire_y_rot_mat(mat_t (*t), fastf_t theta)
{
    fastf_t sin_ = sin(theta);
    fastf_t cos_ = cos(theta);
    mat_t r;
    MAT_ZERO(r);
    r[0] = cos_;
    r[2] = sin_;
    r[5] = 1;
    r[8] = -sin_;
    r[10] = cos_;
    r[15] = 1;
    memcpy(*t, r, sizeof(*t));
}

int
make_tire_surface(CsgTransaction &builder, const std::string &suffix,
		const SurfaceEtos &etos, fastf_t ztire, fastf_t dztred,
		fastf_t dytred, fastf_t dyhub, fastf_t zhub,
		fastf_t dyside1, fastf_t zside1)
{
    struct wmember tireslicktread, tireslicktopsides, tireslickbottomshapes, tireslickbottomsides;
    struct wmember tireslick;
    struct wmember innersolid;
    vect_t vertex, height;
    point_t origin, normal, C;

    VSET(origin, 0, etos.tread.center_y, 0);
    VSET(normal, 0, 1, 0);
    VSET(C, 0, etos.tread.c_y, etos.tread.c_z);
    builder.write_eto(object_name("surface.tread", suffix, ".s").c_str(), origin, normal, C, etos.tread.major_radius, etos.tread.minor_radius);

    VSET(origin, 0, etos.upper_side.center_y, 0);
    VSET(normal, 0, 1, 0);
    VSET(C, 0, etos.upper_side.c_y, etos.upper_side.c_z);
    builder.write_eto(object_name("surface.upper-left", suffix, ".s").c_str(), origin, normal, C, etos.upper_side.major_radius, etos.upper_side.minor_radius);

    VSET(origin, 0, etos.lower_side.center_y, 0);
    VSET(normal, 0, 1, 0);
    VSET(C, 0, etos.lower_side.c_y, etos.lower_side.c_z);
    builder.write_eto(object_name("surface.lower", suffix, ".s").c_str(), origin, normal, C, etos.lower_side.major_radius, etos.lower_side.minor_radius);

    VSET(origin, 0, -etos.upper_side.center_y, 0);
    VSET(normal, 0, -1, 0);
    VSET(C, 0, -etos.upper_side.c_y, -etos.upper_side.c_z);
    builder.write_eto(object_name("surface.upper-right", suffix, ".s").c_str(), origin, normal, C, etos.upper_side.major_radius, etos.upper_side.minor_radius);

    VSET(vertex, 0, -etos.tread.c_y - etos.tread.c_y * .01, 0);
    VSET(height, 0, 2 * (etos.tread.c_y + etos.tread.c_y * .01), 0);
    builder.write_rcc(object_name("clip.top", suffix, ".s").c_str(), vertex, height, ztire - dztred);

    VSET(vertex, 0, -dyside1 / 2 - 0.1 * dyside1 / 2, 0);
    VSET(height, 0, dyside1 + 0.1 * dyside1, 0);
    builder.write_rcc(object_name("clip.inner", suffix, ".s").c_str(), vertex, height, zhub);

    VSET(vertex, 0, -dyside1 / 2 - 0.1 * dyside1 / 2, 0);
    VSET(height, 0, dyside1 + 0.1 * dyside1, 0);
    builder.write_rcc(object_name("clip.sidewall", suffix, ".s").c_str(), vertex, height, zside1);

    VSET(vertex, 0, -dytred / 2, 0);
    VSET(height, 0, dytred, 0);
    builder.write_rcc(object_name("core", suffix, ".s").c_str(), vertex, height, ztire-dztred);
    if (!ZERO(dytred/2 - dyhub/2)) {
	VSET(normal, 0, 1, 0);
	builder.write_cone(object_name("transition.left", suffix, ".s").c_str(), vertex, normal, dytred / 2 - dyhub / 2,
		ztire - dztred, zhub);
	VSET(vertex, 0, dytred/2, 0);
	VSET(normal, 0, -1, 0);
	builder.write_cone(object_name("transition.right", suffix, ".s").c_str(), vertex, normal, dytred / 2 - dyhub / 2,
		ztire - dztred, zhub);
    }

    BU_LIST_INIT(&innersolid.l);
    add_member(object_name("core", suffix, ".s"), &innersolid.l, WMOP_UNION);
    if ((dytred / 2 - dyhub / 2) > 0 &&
	!ZERO(dytred / 2 - dyhub / 2)) {
	add_member(object_name("transition.left", suffix, ".s"), &innersolid.l, WMOP_SUBTRACT);
	add_member(object_name("transition.right", suffix, ".s"), &innersolid.l, WMOP_SUBTRACT);
    }
    if ((dytred / 2 - dyhub / 2) < 0 &&
	!ZERO(dytred / 2 - dyhub / 2)) {
	add_member(object_name("transition.left", suffix, ".s"), &innersolid.l, WMOP_UNION);
	add_member(object_name("transition.right", suffix, ".s"), &innersolid.l, WMOP_UNION);
    }
    add_member(object_name("clip.inner", suffix, ".s"), &innersolid.l, WMOP_SUBTRACT);
    builder.write_lcomb(object_name("body", suffix, ".c").c_str(), &innersolid, 0, NULL, NULL, NULL, 0);

    BU_LIST_INIT(&tireslicktread.l);
    add_member(object_name("surface.tread", suffix, ".s"), &tireslicktread.l, WMOP_UNION);
    add_member(object_name("clip.top", suffix, ".s"), &tireslicktread.l, WMOP_SUBTRACT);
    builder.write_lcomb(object_name("tread-band", suffix, ".c").c_str(), &tireslicktread, 0, NULL, NULL, NULL, 0);

    BU_LIST_INIT(&tireslicktopsides.l);
    add_member(object_name("surface.upper-left", suffix, ".s"), &tireslicktopsides.l, WMOP_UNION);
    add_member(object_name("clip.sidewall", suffix, ".s"), &tireslicktopsides.l, WMOP_SUBTRACT);
    add_member(object_name("surface.upper-right", suffix, ".s"), &tireslicktopsides.l, WMOP_UNION);
    add_member(object_name("clip.sidewall", suffix, ".s"), &tireslicktopsides.l, WMOP_SUBTRACT);
    builder.write_lcomb(object_name("upper-sides", suffix, ".c").c_str(), &tireslicktopsides, 0, NULL, NULL, NULL, 0);

    BU_LIST_INIT(&tireslickbottomshapes.l);
    add_member(object_name("surface.lower", suffix, ".s"), &tireslickbottomshapes.l, WMOP_UNION);
    add_member(object_name("clip.sidewall", suffix, ".s"), &tireslickbottomshapes.l, WMOP_INTERSECT);
    builder.write_lcomb(object_name("lower-side-shapes", suffix, ".c").c_str(), &tireslickbottomshapes, 0, NULL, NULL, NULL, 0);

    BU_LIST_INIT(&tireslickbottomsides.l);
    add_member(object_name("lower-side-shapes", suffix, ".c"), &tireslickbottomsides.l, WMOP_UNION);
    add_member(object_name("clip.inner", suffix, ".s"), &tireslickbottomsides.l, WMOP_SUBTRACT);
    builder.write_lcomb(object_name("lower-sides", suffix, ".c").c_str(), &tireslickbottomsides, 0, NULL, NULL, NULL, 0);

    BU_LIST_INIT(&tireslick.l);
    add_member(object_name("tread-band", suffix, ".c"), &tireslick.l, WMOP_UNION);
    add_member(object_name("upper-sides", suffix, ".c"), &tireslick.l, WMOP_UNION);
    add_member(object_name("lower-sides", suffix, ".c"), &tireslick.l, WMOP_UNION);
    add_member(object_name("body", suffix, ".c"), &tireslick.l, WMOP_UNION);
    builder.write_lcomb(object_name("", suffix, ".c").c_str(), &tireslick, 0, NULL, NULL, NULL, 0);
    return builder.ok() ? BRLCAD_OK : BRLCAD_ERROR;
}

int
make_tire_region(CsgTransaction &builder, const std::string &suffix, int has_tread)
{
    struct wmember tire;
    unsigned char rgb[3];

    VSET(rgb, 40, 40, 40);

    BU_LIST_INIT(&tire.l);
    add_member(object_name("", suffix + ".tire.outer", ".c"), &tire.l, WMOP_UNION);
    add_member(object_name("", suffix + ".tire.inner-cut", ".c"), &tire.l, WMOP_SUBTRACT);

    if (has_tread)
	add_member(object_name("", suffix + ".tread", ".c"), &tire.l, WMOP_UNION);

    return builder.write_lcomb(object_name("", suffix + ".tire", ".r").c_str(), &tire, 1, "plastic", "di=.8 sp=.2", rgb, 0);
}

int
make_air_region(CsgTransaction &builder, const std::string &suffix, fastf_t dyhub, fastf_t zhub, int usewheel)
{
    struct wmember wheelair;
    struct bu_list air;
    point_t origin;
    vect_t height;

    VSET(origin, 0, -dyhub/2, 0);
    VSET(height, 0, dyhub, 0);
    if (builder.write_rcc(object_name("cyl", suffix + ".air", ".s").c_str(), origin, height, zhub) != BRLCAD_OK)
	return BRLCAD_ERROR;

    if (usewheel != 0) {
	BU_LIST_INIT(&wheelair.l);
	add_member(object_name("cyl", suffix + ".air", ".s"), &wheelair.l, WMOP_UNION);
	add_member(object_name("rim-solid", suffix + ".wheel", ".c"), &wheelair.l, WMOP_SUBTRACT);
	if (builder.write_lcomb(object_name("wheel-clearance", suffix + ".air", ".c").c_str(), &wheelair, 0,  NULL, NULL, NULL, 0) != BRLCAD_OK)
	    return BRLCAD_ERROR;
    }

    BU_LIST_INIT(&air);

    if (usewheel != 0) {
	add_member(object_name("wheel-clearance", suffix + ".air", ".c"), &air, WMOP_UNION);
    }
    add_member(object_name("", suffix + ".tire.inner-cut", ".c"), &air, WMOP_UNION);
    return builder.write_comb(object_name("", suffix + ".air", ".r").c_str(), &air, 1, "air", NULL, NULL, 0, 1, 0, 0, 0, 0, 0);
}

int
make_wheel_and_tire(CsgTransaction &builder, const std::string &name, const std::string &suffix, int usewheel)
{
    struct wmember wheel_and_tire;

    BU_LIST_INIT(&wheel_and_tire.l);
    add_member(object_name("", suffix + ".tire", ".r"), &wheel_and_tire.l, WMOP_UNION);
    add_member(object_name("", suffix + ".air", ".r"), &wheel_and_tire.l, WMOP_UNION);
    if (usewheel != 0)
	add_member(object_name("", suffix + ".wheel", ".r"), &wheel_and_tire.l, WMOP_UNION);

    return builder.write_lcomb(name.c_str(), &wheel_and_tire, 0, NULL, NULL, NULL, 0);
}

} /* namespace tire_private */

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
