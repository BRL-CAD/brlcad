/*                      T I R E _ P R I V A T E . H
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
/** @file libged/tire_private.h
 *
 * Internal declarations shared by the tire command implementation files.
 */

#ifndef LIBGED_TIRE_PRIVATE_H
#define LIBGED_TIRE_PRIVATE_H

#include <array>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

#include "common.h"

#include "bu/avs.h"
#include "bu/list.h"
#include "bu/vls.h"
#include "vmath.h"
#include "wdb.h"

namespace tire_private {

using TreadPoint = std::array<fastf_t, 2>;
using TreadSketch = std::vector<TreadPoint>;

constexpr int MAX_TREAD_PATTERN_COUNT = 512;
constexpr int TREAD_PATTERN_SCHEMA_VERSION = 1;

struct EtoParams {
    fastf_t center_y = 0.0;
    fastf_t major_radius = 0.0;
    fastf_t c_y = 0.0;
    fastf_t c_z = 0.0;
    fastf_t minor_radius = 0.0;
};

struct SurfaceEtos {
    EtoParams tread;
    EtoParams upper_side;
    EtoParams lower_side;
};

struct ConicEquation {
    fastf_t a = 0.0;
    fastf_t b = 0.0;
    fastf_t c = 0.0;
    fastf_t d = 0.0;
    fastf_t e = 0.0;
};

struct SurfaceConics {
    ConicEquation tread;
    ConicEquation upper_side;
    ConicEquation lower_side;
};

struct TreadSpec {
    int shape = 0;
    int count = 30;
    fastf_t depth_mm = 0.0;
    std::string pattern_id;
    std::string pattern_source;
    std::string pattern_file;
    std::string pattern_json;
    std::vector<TreadSketch> sketches;
};

struct TreadPatternDefinition {
    int schema_version = TREAD_PATTERN_SCHEMA_VERSION;
    std::string id;
    std::string description;
    std::string type = "rib";
    std::string symmetry = "symmetric";
    std::string terrain = "HT";
    std::string axle = "all";
    fastf_t void_ratio = 0.25;
    int grooves = 4;
    fastf_t groove_angle = 0.0;
    std::string block = "medium";
    std::string pitch = "uniform";
    int pitch_count = 0;
    std::vector<fastf_t> pitch_sequence;
    std::string sipes = "none";
    std::string shoulder = "closed";
    std::string center = "continuous";
    bool tie_bars = false;
    int shape = 1;
    int default_count = 30;
    std::vector<TreadSketch> sketches;
};

struct TireSpec {
    fastf_t dytred = 0.0;
    fastf_t dztred = 0.0;
    fastf_t d1 = 0.0;
    fastf_t dyside1 = 0.0;
    fastf_t zside1 = 0.0;
    fastf_t ztire = 0.0;
    fastf_t dyhub = 0.0;
    fastf_t zhub = 0.0;
    fastf_t thickness = 0.0;
};

struct WheelSpec {
    int bolts = 5;
    fastf_t bolt_diam = 10.0;
    fastf_t bolt_circ_diam = 75.0;
    fastf_t spigot_diam = 40.0;
    fastf_t fixing_offset = 15.0;
    fastf_t bead_height = 8.0;
    fastf_t bead_width = 8.0;
    fastf_t rim_thickness = 0.0;
};

class CsgTransaction {
public:
    explicit CsgTransaction(rt_wdb *file, bool dry_run = false);
    ~CsgTransaction();

    CsgTransaction(const CsgTransaction &) = delete;
    CsgTransaction &operator=(const CsgTransaction &) = delete;

    bool ok() const;
    const std::string &error() const;
    void set_error(const std::string &message);
    void commit();
    int rollback(struct bu_vls *msg = nullptr);

    int write_ell(const std::string &name, const point_t center, const vect_t a, const vect_t b, const vect_t c);
    int write_rcc(const std::string &name, const point_t base, const vect_t height, fastf_t radius);
    int write_cone(const std::string &name, const point_t base, const vect_t dirv, fastf_t height, fastf_t base_radius, fastf_t nose_radius);
    int write_eto(const std::string &name, const point_t vert, const vect_t norm, const vect_t smajor, fastf_t rrot, fastf_t sminor);
    int write_sketch(const std::string &name, const struct rt_sketch_internal *skt);
    int write_extrusion(const std::string &name, const char *sketch_name, const point_t V, const vect_t h, const vect_t u_vec, const vect_t v_vec, int keypoint);
    int write_lcomb(const std::string &name, struct wmember *headp, int region_flag, const char *shadername, const char *shaderargs, const unsigned char *rgb, int inherit);
    int write_comb(const std::string &name, struct bu_list *headp, int region_kind, const char *shadername, const char *shaderargs, const unsigned char *rgb, int id, int air, int material, int los, int inherit, int append_ok, int gift_semantics);
    int update_attributes(const std::string &name, struct bu_attribute_value_set *attrs);

private:
    bool can_write(const std::string &name);
    int record_write(const std::string &name, int result);
    void fail(const std::string &message);

    rt_wdb *file_ = nullptr;
    std::vector<std::string> created_;
    std::unordered_set<std::string> created_set_;
    bool dry_run_ = false;
    bool committed_ = false;
    bool failed_ = false;
    std::string error_;
};

std::string object_name(const char *stem, const std::string &scope, const char *extension = "");
std::string indexed_object_name(const char *stem, const std::string &scope, int index, const char *extension = "");
void add_member(const std::string &name, struct bu_list *members, int op);
void add_member(const std::string &name, struct bu_list *members, mat_t transform, int op);
void tire_y_rot_mat(mat_t (*t), fastf_t theta);

fastf_t conic_partial_at(const ConicEquation &conic, fastf_t x, fastf_t y);
std::optional<fastf_t> conic_z_at_y(const ConicEquation &conic, fastf_t y);
std::optional<ConicEquation> solve_tread_conic(fastf_t dytred, fastf_t dztred, fastf_t d1, fastf_t ztire);
std::optional<ConicEquation> solve_upper_side_conic(fastf_t dytred, fastf_t dztred,
						    fastf_t dyside1, fastf_t zside1, fastf_t ztire,
						    fastf_t dyhub, fastf_t zhub, fastf_t ell1partial);
std::optional<SurfaceConics> solve_surface_conics(fastf_t dytred, fastf_t dztred, fastf_t d1,
						  fastf_t dyside1, fastf_t zside1, fastf_t ztire,
						  fastf_t dyhub, fastf_t zhub);
std::optional<EtoParams> conic_to_eto(const ConicEquation &conic);
std::optional<SurfaceEtos> surface_conics_to_etos(const SurfaceConics &conics);

std::vector<TreadSketch> generate_tread_sketches(const TreadPatternDefinition &def);
const TreadPatternDefinition *find_predefined_tread_pattern(const std::string &id);
bool load_tread_pattern_file(const char *path, TreadPatternDefinition &def, struct bu_vls *msg);
std::string tread_pattern_json(const TreadPatternDefinition &def);
void list_tread_patterns(struct bu_vls *out);
int make_tire(CsgTransaction &builder, const std::string &suffix, const TireSpec &spec, const TreadSpec &tread_spec);

int make_wheel_rims(CsgTransaction &builder, const std::string &suffix, fastf_t dyhub,
		   fastf_t zhub, int bolts, fastf_t bolt_diam,
		   fastf_t bolt_circ_diam, fastf_t spigot_diam,
		   fastf_t fixing_offset, fastf_t bead_height,
		   fastf_t bead_width, fastf_t rim_thickness);

int make_tread_pattern(CsgTransaction &builder, const std::string &suffix, fastf_t dwidth,
		      fastf_t z_base, fastf_t ztire, int number_of_patterns,
		      const TreadSpec &tread_spec);

int make_tread_profile_surface_cut(CsgTransaction &builder, const std::string &suffix,
				   const ConicEquation &upper_side_conic, fastf_t ztire, fastf_t dztred,
				   fastf_t d1, fastf_t dytred, fastf_t dyhub, fastf_t zhub,
				   fastf_t dyside1, const TreadSpec &tread_spec);

int make_tread_profile_outer_ellipse(CsgTransaction &builder, const std::string &suffix,
				     const ConicEquation &upper_side_conic, fastf_t ztire, fastf_t dztred,
				     fastf_t d1, fastf_t dytred, fastf_t dyhub, fastf_t zhub,
				     fastf_t dyside1, const TreadSpec &tread_spec);

int make_tire_surface(CsgTransaction &builder, const std::string &suffix,
		    const SurfaceEtos &etos, fastf_t ztire, fastf_t dztred,
		    fastf_t dytred, fastf_t dyhub, fastf_t zhub,
		    fastf_t dyside1, fastf_t zside1);

int make_tire_region(CsgTransaction &builder, const std::string &suffix, int has_tread);
int make_air_region(CsgTransaction &builder, const std::string &suffix, fastf_t dyhub, fastf_t zhub, int usewheel);
int make_wheel_and_tire(CsgTransaction &builder, const std::string &name, const std::string &suffix, int usewheel);

} /* namespace tire_private */

#endif /* LIBGED_TIRE_PRIVATE_H */

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
