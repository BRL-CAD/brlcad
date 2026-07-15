/*               A S S E T I M P O R T _ R E A D . C P P
 * BRL-CAD
 *
 * Copyright (c) 2022-2026 United States Government as represented by
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
/** @file assetimport_read.cpp
 *
 * GCV logic for importing geometry using the Open Asset Import Library:
 * https://github.com/assimp/assimp 
 *
 */

#include "common.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "gcv/api.h"
#include "wdb.h"
#include "bg/trimesh.h"
#include "bu/color.h"
#include "bu/cmdschema.h"
#include "bu/path.h"
#include "bu/str.h"
#include "bu/vls.h"

/* assimp headers */
#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

/* pugixml headers */
#include <pugixml.hpp>

typedef struct shader_properties {
    char *name;		                            /* shader name, or NULL */
    char *args;		                            /* shader args, or NULL */
    unsigned char* rgb; 			    /* NULL => no color */
    shader_properties() {
	name = NULL;
	args = NULL;
	rgb = NULL;
    }
    ~shader_properties() {
	delete[] name;
	delete[] args;
	delete[] rgb;
    }
} shader_properties_t;

typedef struct assetimport_read_options {
    int starting_id;                                /* starting ident id number */
    int const_id;                                   /* Constant ident number (assigned to all regions if non-negative) */
    int mat_code;                                   /* default material code */
    int verbose;                                    /* verbose flag */
    char* format;                                   /* output file format */
} assetimport_read_options_t;

typedef struct assetimport_read_state {
    const struct gcv_opts* gcv_options;             /* global options */
    assetimport_read_options_t* assetimport_read_options;     /* internal options */

    std::string input_file;	                    /* name of the input file */
    struct rt_wdb* fd_out;	                    /* Resulting BRL-CAD file */
    struct wmember all;                             /* scene root */

    const aiScene* scene;                           /* assetimport generated scene */
    int id_no;                                      /* region ident number */
    unsigned int dfs;                               /* number of nodes visited */
    unsigned int converted;                         /* number of meshes converted */
    unsigned int converted_nurbs;                   /* number of X3D NURBS objects converted */
    assetimport_read_state() {
	gcv_options = NULL;
	assetimport_read_options = NULL;
	input_file = "";
	fd_out = NULL;
	all = WMEMBER_INIT_ZERO;
	scene = NULL;
	id_no = 0;
	dfs = 0;
	converted = 0;
	converted_nurbs = 0;
    }
    ~assetimport_read_state() {}
} assetimport_read_state_t;

static int
assetimport_rgb_from_floats(unsigned char rgb[3], fastf_t r, fastf_t g, fastf_t b)
{
    vect_t rgbf;
    vect_t min_rgb;
    vect_t max_rgb;
    struct bu_color color = BU_COLOR_INIT_ZERO;

    VSET(rgbf, r, g, b);
    VSETALL(min_rgb, 0.0);
    VSETALL(max_rgb, 1.0);
    VMAX(rgbf, min_rgb);
    VMIN(rgbf, max_rgb);

    if (!bu_color_from_rgb_floats(&color, rgbf))
	return 0;
    return bu_color_to_rgb_chars(&color, rgb);
}

static void
aimatrix_to_arr16(aiMatrix4x4 aimat, fastf_t* ret)
{
    ret[0 ] = aimat.a1;
    ret[1 ] = aimat.a2;
    ret[2 ] = aimat.a3;
    ret[3 ] = aimat.a4;
    ret[4 ] = aimat.b1;
    ret[5 ] = aimat.b2;
    ret[6 ] = aimat.b3;
    ret[7 ] = aimat.b4;
    ret[8 ] = aimat.c1;
    ret[9 ] = aimat.c2;
    ret[10] = aimat.c3;
    ret[11] = aimat.c4;
    ret[12] = aimat.d1;
    ret[13] = aimat.d2;
    ret[14] = aimat.d3;
    ret[15] = aimat.d4;
}

/* known shader names */
std::unordered_set<std::string>brlcad_shaders { "bump", "bwtexture", "camo", "checker", 
                                                "cloud", "envmap", "fbmbump", "fire", 
                                                "glass", "gravel", "light", "marble", 
                                                "mirror", "plastic", "rtrans", "scloud", 
                                                "spm", "stack", "stxt", "texture", "turbump", 
                                                "wood" };

/* checks if in_name exists as a known shader keyword
 * if it is, we assume any remaining string after a space are shader args
 *
 * returns 1 if shader properties are found and set in ret
 * returns 0 otherwise
 */
static int
check_brlcad_shader(std::string in_name, shader_properties_t* ret)
{
    std::string name, args = "";
    size_t space = in_name.find(" ");

    /* for brlcad shaders a space means we have additional shader params
     * in the form "shader_name param1=x .."
     */
    if (space != std::string::npos) {
        name = in_name.substr(0, space);
        args = in_name.substr(space+1);
    } else {
        name = in_name;
    }

    if (brlcad_shaders.find(name) != brlcad_shaders.end()) {
        ret->name = new char[name.size() + 1];
        bu_strlcpy(ret->name, name.c_str(), name.size() + 1);
        if (args != "") {
            ret->args = new char[args.size() + 1];
            bu_strlcpy(ret->args, args.c_str(), args.size() + 1);
        }
        return 1;
    }

    return 0;
}

static shader_properties_t*
generate_shader(assetimport_read_state_t* pstate, unsigned int mesh_idx)
{
    aiColor3D* mesh_color = (aiColor3D*)pstate->scene->mMeshes[mesh_idx]->mColors[0];
    aiColor3D diffuse_color(1);	/* diffuse color -> defaults to white */
    if (!pstate->scene->HasMaterials() && !mesh_color)
	return NULL;

    shader_properties_t* ret = new shader_properties_t();

    /* check for material data */
    if (pstate->scene->HasMaterials()) {
	unsigned int mat_idx = pstate->scene->mMeshes[mesh_idx]->mMaterialIndex;
	aiMaterial* mat = pstate->scene->mMaterials[mat_idx];

        /* when shader names are written from a .g we use the raw name+args as the shader name
         * otherwise try to build up the shader manually 
         */
        if (!check_brlcad_shader(mat->GetName().data, ret)) {
	    std::string name = "plastic";
	    ret->name = new char[name.size() + 1];
	    bu_strlcpy(ret->name, name.c_str(), name.size() + 1);

	    /* brlcad 'plastic' shader defaults */
	    float tr = 0.0;		/* transparency */
	    float re = 0.0;		/* mirror reflectance */
	    float sp = 0.7;		/* specular reflectivity */
	    float di = 0.3;		/* diffuse reflectivity */
	    float ri = 1.0;		/* refractive index */
	    float ex = 0.0;		/* extinction */
	    float sh = 10.0;	        /* shininess */
	    float em = 0.0;		/* emission */

	    /* gets value if key exists in material, otherwise leaves default */
            /* NOTE: assetimport seems to favor MATKEY_OPACITY over MATKEY_TRANSPARENCYFACTOR
             * so we use 1-opacity for transparency
             */
	    mat->Get(AI_MATKEY_OPACITY, tr);
	    mat->Get(AI_MATKEY_REFLECTIVITY, re);
	    mat->Get(AI_MATKEY_SPECULAR_FACTOR, sp);
	    mat->Get(AI_MATKEY_SHININESS_STRENGTH, di);
	    mat->Get(AI_MATKEY_REFRACTI, ri);
	    mat->Get(AI_MATKEY_ANISOTROPY_FACTOR, ex);
	    mat->Get(AI_MATKEY_SHININESS, sh);
	    mat->Get(AI_MATKEY_EMISSIVE_INTENSITY, em);

	    /* format values into args string */
	    std::string args =
	        "{ tr " + std::to_string(1-tr) +
	         " re " + std::to_string(re) +
	         " sp " + std::to_string(sp) +
	         " di " + std::to_string(di) +
	         " ri " + std::to_string(ri) +
	         " ex " + std::to_string(ex) +
	         " sh " + std::to_string(sh) +
	         " em " + std::to_string(em) +
	         " }";
	    ret->args = new char[args.size() + 1];
	    bu_strlcpy(ret->args, args.c_str(), args.size() + 1);
        }

	/* check for vertex colors, otherwise try to use diffuse color */
        if (!mesh_color) {
            mat->Get(AI_MATKEY_COLOR_DIFFUSE, diffuse_color);
            mesh_color = &diffuse_color;
        }
    }

    /* set the color of the face using the first vertex color data we
     * find if such exists. If not, try to use the material diffuse color.
     * NOTE: we make two assumptions when using the vertex[0] color:
     * 1) each vertex only has one set of colors
     * 2) each vertex in the triangle is the same color
     */
    if (mesh_color) {
	unsigned char rgb[3];
	if (assetimport_rgb_from_floats(rgb, mesh_color->r, mesh_color->g, mesh_color->b)) {
	    ret->rgb = new unsigned char[3];
	    VMOVEN(ret->rgb, rgb, 3);
	    if (pstate->gcv_options->verbosity_level || pstate->assetimport_read_options->verbose)
		bu_log("color data (%d, %d, %d)\n", (int)ret->rgb[0], (int)ret->rgb[1], (int)ret->rgb[2]);
	}
    }

    return ret;
}

static std::string
generate_unique_name(const char* curr_name, const char* def_name, bool is_mesh)
{
    static std::unordered_map<std::string, int>used_names; /* used names in db */

    const char *fallback_name = BU_STR_EMPTY(def_name) ? "assetimport" : def_name;
    std::string name(BU_STR_EMPTY(curr_name) ? fallback_name : curr_name);
    std::string suffix = is_mesh ? ".s" : ".r";

    /* get last path component */
    struct bu_vls basename = BU_VLS_INIT_ZERO;
    if (!BU_STR_EMPTY(curr_name) && bu_path_component(&basename, curr_name, BU_PATH_BASENAME))
	name = bu_vls_cstr(&basename);
    bu_vls_free(&basename);

    /* strip region/solid suffix at end if it exists */
    size_t dotr = name.rfind(suffix);
    if (dotr != std::string::npos && dotr + suffix.size() == name.size())
        name = name.substr(0, dotr);

    /* if curr_name is empty, give a generic name */
    if (!name.size())
	name = fallback_name;

    /* cleanup name - remove spaces, slashes and non-standard characters */
    bu_vls scrub = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&scrub, "%s", name.c_str());
    bu_vls_simplify(&scrub, nullptr, " _\0", " _\0");
    name = scrub.vls_len > 0 ? std::string(bu_vls_cstr(&scrub)) : fallback_name;   /* somemtimes we scrub out the entire name */

    /* check for name collisions */
    auto handle = used_names.find(name);
    if (handle != used_names.end()) {
        while (used_names.find(name + "_" + std::to_string(++handle->second)) != used_names.end())
            ;
        name.append("_" + std::to_string(handle->second));
    }

    used_names.emplace(name, 0);
    name.append(suffix);

    return name;
}

static const char *
x3d_lname(const pugi::xml_node &node)
{
    const char *name = node.name();
    const char *colon = strrchr(name, ':');
    return colon ? colon + 1 : name;
}

static int
x3d_name_is(const pugi::xml_node &node, const char *name)
{
    return BU_STR_EQUAL(x3d_lname(node), name);
}

static pugi::xml_node
x3d_first_descendant(const pugi::xml_node &node, const char *name)
{
    for (pugi::xml_node child: node.children()) {
	if (x3d_name_is(child, name))
	    return child;
	pugi::xml_node ret = x3d_first_descendant(child, name);
	if (ret)
	    return ret;
    }
    return pugi::xml_node();
}

static std::vector<double>
x3d_parse_doubles(const char *text)
{
    std::vector<double> ret;
    if (BU_STR_EMPTY(text))
	return ret;

    std::string clean(text);
    for (char &c: clean) {
	if (c == ',')
	    c = ' ';
    }

    std::vector<char *> argv(clean.size() + 1, NULL);
    size_t argc = bu_argv_from_string(argv.data(), argv.size() - 1, &clean[0]);
    ret.reserve(argc);

    for (size_t i = 0; i < argc; i++) {
	fastf_t val = 0.0;
	const char *av = argv[i];
	if (bu_cmd_number_from_str(&val, av)) {
	    ret.push_back((double)val);
	} else {
	    bu_log("WARNING: skipping invalid X3D numeric value '%s'\n", argv[i]);
	}
    }

    return ret;
}

static std::vector<double>
x3d_attr_doubles(const pugi::xml_node &node, const char *name)
{
    pugi::xml_attribute attr = node.attribute(name);
    return x3d_parse_doubles(attr ? attr.value() : NULL);
}

static int
x3d_attr_int(const pugi::xml_node &node, const char *name, int def)
{
    pugi::xml_attribute attr = node.attribute(name);
    int val = def;
    if (!attr)
	return def;

    const char *av = attr.value();
    if (bu_cmd_integer_from_str(&val, av)) {
	return val;
    }

    bu_log("WARNING: invalid X3D integer value '%s' for %s; using %d\n", attr.value(), name, def);
    return def;
}

static int
x3d_attr_bool(const pugi::xml_node &node, const char *name, int def)
{
    pugi::xml_attribute attr = node.attribute(name);
    if (!attr)
	return def;
    const char *v = attr.value();
    if (BU_STR_EMPTY(v))
	return def;
    if (bu_str_false(v))
	return 0;
    return 1;
}

static void
x3d_vec3_attr(const pugi::xml_node &node, const char *name, vect_t v, const vect_t def)
{
    VMOVE(v, def);

    pugi::xml_attribute attr = node.attribute(name);
    if (!attr)
	return;

    vect_t parsed;
    const char *av = attr.value();
    if (bu_cmd_vector3_from_argv(parsed, 1, &av) > 0) {
	VMOVE(v, parsed);
    } else {
	bu_log("WARNING: invalid X3D vector value '%s' for %s\n", attr.value(), name);
    }
}

static void
x3d_rotation_attr(const pugi::xml_node &node, const char *name, hvect_t v, const hvect_t def)
{
    HMOVE(v, def);
    std::vector<double> vals = x3d_attr_doubles(node, name);
    if (vals.size() >= 4)
	HSET(v, vals[0], vals[1], vals[2], vals[3]);
}

static void
x3d_scale_mat(mat_t m, const vect_t s)
{
    MAT_IDN(m);
    MAT_SCALE_VEC(m, s);
}

static void
x3d_rotation_mat(mat_t m, const hvect_t r)
{
    vect_t dir;
    point_t origin;
    double mag;

    VSET(dir, r[X], r[Y], r[Z]);
    VSETALL(origin, 0.0);
    mag = MAGNITUDE(dir);

    if (NEAR_ZERO(mag, SMALL_FASTF)) {
	MAT_IDN(m);
	return;
    }

    VUNITIZE(dir);
    bn_mat_arb_rot(m, origin, dir, r[W]);
}

static void
x3d_post_mul(mat_t accum, const mat_t rhs)
{
    mat_t tmp;
    bn_mat_mul(tmp, accum, rhs);
    MAT_COPY(accum, tmp);
}

static void
x3d_transform_mat(const pugi::xml_node &node, mat_t m)
{
    static const vect_t zero3 = VINIT_ZERO;
    static const vect_t one3 = VINITALL(1.0);
    static const hvect_t zero_rot = {0.0, 0.0, 1.0, 0.0};
    vect_t center, scale, translation;
    hvect_t rotation, scale_orientation, inv_scale_orientation;
    mat_t tm, cm, rm, sm, som, isom, icm;

    MAT_IDN(m);
    x3d_vec3_attr(node, "center", center, zero3);
    x3d_vec3_attr(node, "scale", scale, one3);
    x3d_vec3_attr(node, "translation", translation, zero3);
    x3d_rotation_attr(node, "rotation", rotation, zero_rot);
    x3d_rotation_attr(node, "scaleOrientation", scale_orientation, zero_rot);

    HMOVE(inv_scale_orientation, scale_orientation);
    inv_scale_orientation[W] = -inv_scale_orientation[W];

    MAT_IDN(tm);
    MAT_DELTAS_VEC(tm, translation);
    MAT_IDN(cm);
    MAT_DELTAS_VEC(cm, center);
    x3d_rotation_mat(rm, rotation);
    x3d_rotation_mat(som, scale_orientation);
    x3d_scale_mat(sm, scale);
    x3d_rotation_mat(isom, inv_scale_orientation);
    MAT_IDN(icm);
    MAT_DELTAS_VEC_NEG(icm, center);

    x3d_post_mul(m, tm);
    x3d_post_mul(m, cm);
    x3d_post_mul(m, rm);
    x3d_post_mul(m, som);
    x3d_post_mul(m, sm);
    x3d_post_mul(m, isom);
    x3d_post_mul(m, icm);
}

static void
x3d_ancestor_transform(const pugi::xml_node &node, mat_t m)
{
    std::vector<pugi::xml_node> transforms;
    for (pugi::xml_node parent = node.parent(); parent; parent = parent.parent()) {
	if (x3d_name_is(parent, "Transform"))
	    transforms.push_back(parent);
    }

    MAT_IDN(m);
    for (std::vector<pugi::xml_node>::reverse_iterator it = transforms.rbegin(); it != transforms.rend(); ++it) {
	mat_t tm;
	x3d_transform_mat(*it, tm);
	x3d_post_mul(m, tm);
    }
}

static ON_3dPoint
x3d_point3(const std::vector<double> &vals, size_t point_index, const mat_t xform, double scale_factor)
{
    point_t in, out;
    size_t offset = point_index * 3;
    VSET(in, vals[offset], vals[offset + 1], vals[offset + 2]);
    MAT4X3PNT(out, xform, in);
    VSCALE(out, out, scale_factor);
    return ON_3dPoint(out[X], out[Y], out[Z]);
}

static std::vector<double>
x3d_control_points3(const pugi::xml_node &node)
{
    pugi::xml_node coord = x3d_first_descendant(node, "Coordinate");
    if (!coord)
	coord = x3d_first_descendant(node, "CoordinateDouble");
    return coord ? x3d_attr_doubles(coord, "point") : std::vector<double>();
}

static std::vector<double>
x3d_control_points2(const pugi::xml_node &node)
{
    std::vector<double> ret = x3d_attr_doubles(node, "controlPoint");
    if (ret.size())
	return ret;

    pugi::xml_node coord = x3d_first_descendant(node, "Coordinate");
    if (!coord)
	coord = x3d_first_descendant(node, "CoordinateDouble");
    return coord ? x3d_attr_doubles(coord, "point") : std::vector<double>();
}

static void
x3d_set_surface_knots(ON_NurbsSurface *surface, int dir, const std::vector<double> &knots)
{
    int knot_count = surface->KnotCount(dir);

    if (!knots.size()) {
	surface->MakeClampedUniformKnotVector(dir, 1.0);
	return;
    }

    if ((int)knots.size() == knot_count + 2) {
	for (int i = 0; i < knot_count; i++)
	    surface->SetKnot(dir, i, knots[i + 1]);
	return;
    }

    if ((int)knots.size() == knot_count) {
	for (int i = 0; i < knot_count; i++)
	    surface->SetKnot(dir, i, knots[i]);
	return;
    }

    bu_log("WARNING: X3D NURBS surface knot count mismatch; using clamped uniform knots\n");
    surface->MakeClampedUniformKnotVector(dir, 1.0);
}

static void
x3d_set_curve_knots(ON_NurbsCurve *curve, const std::vector<double> &knots)
{
    int knot_count = curve->KnotCount();

    if (!knots.size()) {
	curve->MakeClampedUniformKnotVector();
	return;
    }

    if ((int)knots.size() == knot_count + 2) {
	for (int i = 0; i < knot_count; i++)
	    curve->SetKnot(i, knots[i + 1]);
	return;
    }

    if ((int)knots.size() == knot_count) {
	for (int i = 0; i < knot_count; i++)
	    curve->SetKnot(i, knots[i]);
	return;
    }

    bu_log("WARNING: X3D NURBS curve knot count mismatch; using clamped uniform knots\n");
    curve->MakeClampedUniformKnotVector();
}

static ON_NurbsSurface *
x3d_make_surface(const pugi::xml_node &node, const mat_t xform, double scale_factor)
{
    int u_dim = x3d_attr_int(node, "uDimension", 0);
    int v_dim = x3d_attr_int(node, "vDimension", 0);
    int u_order = x3d_attr_int(node, "uOrder", 3);
    int v_order = x3d_attr_int(node, "vOrder", 3);
    std::vector<double> points = x3d_control_points3(node);
    std::vector<double> weights = x3d_attr_doubles(node, "weight");
    size_t point_count = points.size() / 3;

    if (!u_dim && v_dim && point_count % (size_t)v_dim == 0)
	u_dim = (int)(point_count / (size_t)v_dim);
    if (!v_dim && u_dim && point_count % (size_t)u_dim == 0)
	v_dim = (int)(point_count / (size_t)u_dim);

    if (u_dim < 2 || v_dim < 2 || u_order < 2 || v_order < 2 || u_order > u_dim || v_order > v_dim ||
	    points.size() < (size_t)(u_dim * v_dim * 3)) {
	bu_log("WARNING: skipping malformed X3D NURBS surface\n");
	return NULL;
    }

    bool rational = (weights.size() == (size_t)(u_dim * v_dim));
    if (weights.size() && !rational)
	bu_log("WARNING: X3D NURBS surface weight count mismatch; importing as non-rational\n");

    ON_NurbsSurface *surface = ON_NurbsSurface::New(3, rational, u_order, v_order, u_dim, v_dim);
    if (!surface)
	return NULL;

    x3d_set_surface_knots(surface, 0, x3d_attr_doubles(node, "uKnot"));
    x3d_set_surface_knots(surface, 1, x3d_attr_doubles(node, "vKnot"));

    for (int v = 0; v < v_dim; v++) {
	for (int u = 0; u < u_dim; u++) {
	    size_t pindex = (size_t)v * (size_t)u_dim + (size_t)u;
	    ON_3dPoint p = x3d_point3(points, pindex, xform, scale_factor);
	    if (rational)
		surface->SetCV(u, v, ON_4dPoint(p.x, p.y, p.z, weights[pindex]));
	    else
		surface->SetCV(u, v, p);
	}
    }

    return surface;
}

static ON_NurbsCurve *
x3d_make_curve3(const pugi::xml_node &node, const mat_t xform, double scale_factor)
{
    int order = x3d_attr_int(node, "order", 3);
    std::vector<double> points = x3d_control_points3(node);
    std::vector<double> weights = x3d_attr_doubles(node, "weight");
    int cv_count = (int)(points.size() / 3);

    if (cv_count < 2 || order < 2 || order > cv_count) {
	bu_log("WARNING: skipping malformed X3D NURBS curve\n");
	return NULL;
    }

    bool rational = (weights.size() == (size_t)cv_count);
    if (weights.size() && !rational)
	bu_log("WARNING: X3D NURBS curve weight count mismatch; importing as non-rational\n");

    ON_NurbsCurve *curve = ON_NurbsCurve::New(3, rational, order, cv_count);
    if (!curve)
	return NULL;

    x3d_set_curve_knots(curve, x3d_attr_doubles(node, "knot"));
    for (int i = 0; i < cv_count; i++) {
	ON_3dPoint p = x3d_point3(points, (size_t)i, xform, scale_factor);
	if (rational)
	    curve->SetCV(i, ON_4dPoint(p.x, p.y, p.z, weights[(size_t)i]));
	else
	    curve->SetCV(i, p);
    }

    return curve;
}

static ON_Curve *
x3d_make_curve2(const pugi::xml_node &node)
{
    std::vector<double> points;

    if (x3d_name_is(node, "ContourPolyline2D") || x3d_name_is(node, "Polyline2D")) {
	points = x3d_attr_doubles(node, "controlPoint");
	if (!points.size())
	    points = x3d_attr_doubles(node, "lineSegments");
	if (points.size() < 4)
	    return NULL;

	ON_3dPointArray pnts;
	for (size_t i = 0; i + 1 < points.size(); i += 2)
	    pnts.Append(ON_3dPoint(points[i], points[i + 1], 0.0));
	if (pnts.Count() > 2 && pnts[0].DistanceTo(pnts[pnts.Count() - 1]) > ON_ZERO_TOLERANCE)
	    pnts.Append(pnts[0]);
	return new ON_PolylineCurve(pnts);
    }

    if (!x3d_name_is(node, "NurbsCurve2D"))
	return NULL;

    int order = x3d_attr_int(node, "order", 3);
    points = x3d_control_points2(node);
    std::vector<double> weights = x3d_attr_doubles(node, "weight");
    int cv_count = (int)(points.size() / 2);

    if (cv_count < 2 || order < 2 || order > cv_count)
	return NULL;

    bool rational = (weights.size() == (size_t)cv_count);
    ON_NurbsCurve *curve = ON_NurbsCurve::New(2, rational, order, cv_count);
    if (!curve)
	return NULL;

    x3d_set_curve_knots(curve, x3d_attr_doubles(node, "knot"));
    for (int i = 0; i < cv_count; i++) {
	size_t offset = (size_t)i * 2;
	if (rational)
	    curve->SetCV(i, ON_4dPoint(points[offset], points[offset + 1], 0.0, weights[(size_t)i]));
	else
	    curve->SetCV(i, ON_3dPoint(points[offset], points[offset + 1], 0.0));
    }

    return curve;
}

static ON_Curve *
x3d_curve_on_surface_approx(const ON_Surface *surface, const ON_Curve *curve2d)
{
    ON_Interval domain = curve2d->Domain();
    int samples = std::max(16, curve2d->SpanCount() * 8 + 1);
    ON_3dPointArray pnts;

    for (int i = 0; i < samples; i++) {
	double t = domain.ParameterAt((double)i / (double)(samples - 1));
	ON_3dPoint uv = curve2d->PointAt(t);
	pnts.Append(surface->PointAt(uv.x, uv.y));
    }

    ON_PolylineCurve *curve3d = new ON_PolylineCurve(pnts);
    curve3d->SetDomain(domain.Min(), domain.Max());
    return curve3d;
}

static int
x3d_add_trim_loop(ON_Brep *brep, ON_BrepFace &face, const pugi::xml_node &contour, ON_BrepLoop::TYPE loop_type)
{
    const ON_Surface *surface = face.SurfaceOf();
    if (!surface)
	return 0;

    std::vector<ON_Curve *> curves2d;
    for (pugi::xml_node child: contour.children()) {
	if (x3d_name_is(child, "NurbsCurve2D") || x3d_name_is(child, "ContourPolyline2D") || x3d_name_is(child, "Polyline2D")) {
	    ON_Curve *curve = x3d_make_curve2(child);
	    if (curve)
		curves2d.push_back(curve);
	}
    }

    if (!curves2d.size())
	return 0;

    ON_BrepLoop &loop = brep->NewLoop(loop_type, face);
    int first_vi = -1;
    int prev_vi = -1;
    ON_3dPoint first_point;

    for (size_t i = 0; i < curves2d.size(); i++) {
	ON_Curve *c2d = curves2d[i];
	ON_Curve *c3d = x3d_curve_on_surface_approx(surface, c2d);
	ON_3dPoint start2d = c2d->PointAtStart();
	ON_3dPoint end2d = c2d->PointAtEnd();
	ON_3dPoint start3d = surface->PointAt(start2d.x, start2d.y);
	ON_3dPoint end3d = surface->PointAt(end2d.x, end2d.y);

	int v0 = prev_vi;
	if (v0 < 0 || start3d.DistanceTo(brep->m_V[v0].Point()) > ON_ZERO_TOLERANCE) {
	    ON_BrepVertex &v = brep->NewVertex(start3d);
	    v.m_tolerance = 0.0;
	    v0 = v.m_vertex_index;
	}
	if (first_vi < 0) {
	    first_vi = v0;
	    first_point = start3d;
	}

	int v1 = -1;
	if (i == curves2d.size() - 1 && end3d.DistanceTo(first_point) <= ON_ZERO_TOLERANCE) {
	    v1 = first_vi;
	} else {
	    ON_BrepVertex &v = brep->NewVertex(end3d);
	    v.m_tolerance = 0.0;
	    v1 = v.m_vertex_index;
	}

	int c3i = brep->AddEdgeCurve(c3d);
	ON_BrepEdge &edge = brep->NewEdge(brep->m_V[v0], brep->m_V[v1], c3i);
	edge.m_tolerance = 0.0;

	int c2i = brep->m_C2.Count();
	brep->m_C2.Append(c2d);

	ON_BrepTrim &trim = brep->NewTrim(edge, false, loop, c2i);
	trim.m_iso = surface->IsIsoparametric(*c2d);
	trim.m_type = ON_BrepTrim::boundary;
	trim.m_tolerance[0] = 0.0;
	trim.m_tolerance[1] = 0.0;

	prev_vi = v1;
    }

    return 1;
}

static void
x3d_add_natural_loop(ON_Brep *brep, ON_BrepFace &face)
{
    const ON_Surface *surface = face.SurfaceOf();
    double u0, u1, v0, v1;
    ON_3dPoint corners[4];

    surface->GetDomain(0, &u0, &u1);
    surface->GetDomain(1, &v0, &v1);
    corners[0] = surface->PointAt(u0, v0);
    corners[1] = surface->PointAt(u1, v0);
    corners[2] = surface->PointAt(u1, v1);
    corners[3] = surface->PointAt(u0, v1);

    int vi[4];
    for (int i = 0; i < 4; i++) {
	ON_BrepVertex &v = brep->NewVertex(corners[i]);
	v.m_tolerance = 0.0;
	vi[i] = v.m_vertex_index;
    }

    ON_BrepLoop &loop = brep->NewLoop(ON_BrepLoop::outer, face);
    struct side_def {
	int v0i;
	int v1i;
	int dir;
	double c;
	int reverse;
	ON_2dPoint from;
	ON_2dPoint to;
	ON_Surface::ISO iso;
    } sides[4] = {
	{0, 1, 0, v0, 0, ON_2dPoint(u0, v0), ON_2dPoint(u1, v0), ON_Surface::S_iso},
	{1, 2, 1, u1, 0, ON_2dPoint(u1, v0), ON_2dPoint(u1, v1), ON_Surface::E_iso},
	{2, 3, 0, v1, 1, ON_2dPoint(u1, v1), ON_2dPoint(u0, v1), ON_Surface::N_iso},
	{3, 0, 1, u0, 1, ON_2dPoint(u0, v1), ON_2dPoint(u0, v0), ON_Surface::W_iso}
    };

    for (int i = 0; i < 4; i++) {
	ON_Curve *c3d = surface->IsoCurve(sides[i].dir, sides[i].c);
	if (sides[i].reverse)
	    c3d->Reverse();
	int c3i = brep->AddEdgeCurve(c3d);
	ON_BrepEdge &edge = brep->NewEdge(brep->m_V[vi[sides[i].v0i]], brep->m_V[vi[sides[i].v1i]], c3i);
	edge.m_tolerance = 0.0;

	ON_Curve *c2d = new ON_LineCurve(sides[i].from, sides[i].to);
	ON_Interval domain = c3d->Domain();
	c2d->SetDomain(domain.Min(), domain.Max());
	int c2i = brep->m_C2.Count();
	brep->m_C2.Append(c2d);

	ON_BrepTrim &trim = brep->NewTrim(edge, false, loop, c2i);
	trim.m_iso = sides[i].iso;
	trim.m_type = ON_BrepTrim::boundary;
	trim.m_tolerance[0] = 0.0;
	trim.m_tolerance[1] = 0.0;
    }
}

static ON_Brep *
x3d_make_surface_brep(const pugi::xml_node &node, const mat_t xform, double scale_factor)
{
    ON_NurbsSurface *surface = x3d_make_surface(node, xform, scale_factor);
    if (!surface)
	return NULL;

    ON_Brep *brep = ON_Brep::New();
    int si = brep->AddSurface(surface);
    ON_BrepFace &face = brep->NewFace(si);
    face.m_bRev = !x3d_attr_bool(node, "ccw", 1);

    int added_trim_loop = 0;
    if (x3d_name_is(node, "NurbsTrimmedSurface")) {
	int loop_count = 0;
	for (pugi::xml_node child: node.children()) {
	    if (x3d_name_is(child, "Contour2D")) {
		ON_BrepLoop::TYPE type = (loop_count == 0) ? ON_BrepLoop::outer : ON_BrepLoop::inner;
		if (x3d_add_trim_loop(brep, face, child, type))
		    added_trim_loop = 1;
		loop_count++;
	    }
	}
	if (loop_count && !added_trim_loop)
	    bu_log("WARNING: unable to import X3D NURBS trim contours; using natural surface boundary\n");
    }

    if (!added_trim_loop)
	x3d_add_natural_loop(brep, face);

    brep->Compact();
    return brep;
}

static ON_Brep *
x3d_make_curve_brep(const pugi::xml_node &node, const mat_t xform, double scale_factor)
{
    ON_NurbsCurve *curve = x3d_make_curve3(node, xform, scale_factor);
    if (!curve)
	return NULL;

    ON_Brep *brep = ON_Brep::New();
    int c3i = brep->AddEdgeCurve(curve);
    ON_BrepVertex &v0 = brep->NewVertex(curve->PointAtStart());
    ON_BrepVertex &v1 = brep->NewVertex(curve->PointAtEnd());
    v0.m_tolerance = 0.0;
    v1.m_tolerance = 0.0;
    ON_BrepEdge &edge = brep->NewEdge(v0, v1, c3i);
    edge.m_tolerance = 0.0;
    brep->Compact();
    return brep;
}

static int
x3d_find_material_rgb(const pugi::xml_node &node, unsigned char rgb[3])
{
    for (pugi::xml_node parent = node.parent(); parent; parent = parent.parent()) {
	if (!x3d_name_is(parent, "Shape"))
	    continue;

	pugi::xml_node app = x3d_first_descendant(parent, "Appearance");
	pugi::xml_node mat = app ? x3d_first_descendant(app, "Material") : pugi::xml_node();
	if (!mat)
	    return 0;

	std::vector<double> vals = x3d_attr_doubles(mat, "diffuseColor");
	if (vals.size() < 3)
	    return 0;

	return assetimport_rgb_from_floats(rgb, vals[RED], vals[GRN], vals[BLU]);
    }

    return 0;
}

static const char *
x3d_node_name(const pugi::xml_node &node, const char *def_name)
{
    pugi::xml_attribute def = node.attribute("DEF");
    if (def)
	return def.value();
    for (pugi::xml_node parent = node.parent(); parent; parent = parent.parent()) {
	if (x3d_name_is(parent, "Shape")) {
	    def = parent.attribute("DEF");
	    if (def)
		return def.value();
	}
    }
    return def_name;
}

static int
x3d_write_brep(assetimport_read_state_t *pstate, const pugi::xml_node &node, ON_Brep *brep)
{
    std::string solid_name = generate_unique_name(x3d_node_name(node, "nurbs"), pstate->gcv_options->default_name, 1);
    std::string region_name = generate_unique_name(x3d_node_name(node, "nurbs"), pstate->gcv_options->default_name, 0);
    struct wmember region;
    unsigned char rgb[3];
    unsigned char *rgbp = NULL;

    if (!brep)
	return 0;

    if (x3d_find_material_rgb(node, rgb))
	rgbp = rgb;

    if (mk_brep(pstate->fd_out, solid_name.c_str(), (void *)brep)) {
	bu_log("WARNING: failed to write X3D NURBS brep %s\n", solid_name.c_str());
	delete brep;
	return 0;
    }

    BU_LIST_INIT(&region.l);
    (void)mk_addmember(solid_name.c_str(), &region.l, NULL, WMOP_UNION);
    mk_lrcomb(pstate->fd_out, region_name.c_str(), &region, 1, (char *)NULL, (char *)NULL, rgbp,
	    pstate->id_no, 0, pstate->assetimport_read_options->mat_code, 100, 0);
    (void)mk_addmember(region_name.c_str(), &pstate->all.l, NULL, WMOP_UNION);

    if (!pstate->assetimport_read_options->const_id)
	pstate->id_no++;
    pstate->converted_nurbs++;

    delete brep;
    return 1;
}

static void
x3d_convert_nurbs_node(assetimport_read_state_t *pstate, const pugi::xml_node &node)
{
    mat_t xform;

    if (x3d_name_is(node, "NurbsPatchSurface") || x3d_name_is(node, "NurbsTrimmedSurface")) {
	x3d_ancestor_transform(node, xform);
	x3d_write_brep(pstate, node, x3d_make_surface_brep(node, xform, pstate->gcv_options->scale_factor));
    } else if (x3d_name_is(node, "NurbsCurve")) {
	x3d_ancestor_transform(node, xform);
	x3d_write_brep(pstate, node, x3d_make_curve_brep(node, xform, pstate->gcv_options->scale_factor));
    }

    for (pugi::xml_node child: node.children())
	x3d_convert_nurbs_node(pstate, child);
}

static int
convert_x3d_nurbs(assetimport_read_state_t *pstate)
{
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file(pstate->input_file.c_str());
    if (!result)
	return 0;

    pugi::xml_node root = doc.document_element();
    if (!root || !x3d_name_is(root, "X3D"))
	return 0;

    unsigned int before = pstate->converted_nurbs;
    x3d_convert_nurbs_node(pstate, root);
    return (int)(pstate->converted_nurbs - before);
}

static void
generate_geometry(assetimport_read_state_t* pstate, wmember &region, unsigned int mesh_idx)
{
    aiMesh* mesh = pstate->scene->mMeshes[mesh_idx];
    /* sanity check: importer handles triangulation but make sure
       we are dealing with only triangles */
    if (mesh->mPrimitiveTypes != aiPrimitiveType_TRIANGLE) {
	bu_log("WARNING: unknown primitive in mesh[%d] -- skipping\n", mesh_idx);
	return;
    }

    int* faces = new int[mesh->mNumFaces * 3];
    double* vertices = new double[mesh->mNumVertices * 3];
    fastf_t* normals = NULL;
    fastf_t* thickness = NULL;
    bu_bitv* bitv = NULL;

    if (pstate->gcv_options->verbosity_level || pstate->assetimport_read_options->verbose) {
	bu_log("mesh[%d] num Faces: %d\n", mesh_idx, mesh->mNumFaces);
	bu_log("mesh[%d] num vertices: %d\n", mesh_idx, mesh->mNumVertices);
    }

    /* add all faces */
    for (size_t i = 0; i < mesh->mNumFaces; i++) {
	VMOVEN(&faces[i * 3], mesh->mFaces[i].mIndices, 3);
    }

    /* add all vertices */
    for (size_t i = 0; i < mesh->mNumVertices; i++) {
	point_t vertex;
	VSET(vertex, mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);
	VSCALE(vertex, vertex, pstate->gcv_options->scale_factor);
	VMOVE(&vertices[i * 3], vertex);
    }

    /* check bot mode */
    unsigned char orientation = RT_BOT_CCW; /* default ccw https://assimp.sourceforge.net/lib_html/data.html*/
    unsigned char mode = RT_BOT_SURFACE;
    {
	rt_bot_internal bot = {};
	bot.magic = RT_BOT_INTERNAL_MAGIC;
	bot.orientation = orientation;
	bot.num_vertices = mesh->mNumVertices;
	bot.num_faces = mesh->mNumFaces;
	bot.vertices = vertices;
	bot.faces = faces;
	/* TODO this generates a lot of false plate modes */
	mode = bg_trimesh_solid((int)bot.num_vertices, (int)bot.num_faces, bot.vertices, bot.faces, NULL) ? RT_BOT_PLATE : RT_BOT_SOLID;
    }

    if (mode == RT_BOT_PLATE) {
	const fastf_t plate_thickness = 1.0;
	bitv = bu_bitv_new(mesh->mNumFaces * 3);
	thickness = new fastf_t[mesh->mNumFaces * 3] {plate_thickness};
    }

    /* add mesh to region list */
    std::string mesh_name = generate_unique_name(pstate->scene->mMeshes[mesh_idx]->mName.data, pstate->gcv_options->default_name, 1);
    /* check for normals */
    if (mesh->HasNormals()) {
	normals = new fastf_t[mesh->mNumVertices * 3];
	for (size_t i = 0; i < mesh->mNumVertices; i++) {
	    vect_t normal;
	    VSET(normal, mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z);
	    VUNITIZE(normal);
	    VMOVE(&normals[i * 3], normal);
	}

	mk_bot_w_normals(pstate->fd_out, mesh_name.c_str(), mode, orientation, 0, mesh->mNumVertices, mesh->mNumFaces, vertices, faces, thickness, bitv, mesh->mNumVertices, normals, faces);
    } else {
	mk_bot(pstate->fd_out, mesh_name.c_str(), mode, orientation, 0, mesh->mNumVertices, mesh->mNumFaces, vertices, faces, thickness, bitv);
    }
    (void)mk_addmember(mesh_name.c_str(), &region.l, NULL, WMOP_UNION);

    /* book keeping to log converted meshes */
    pstate->converted++;

    /* cleanup memory */
    delete[] faces;
    delete[] vertices;
    delete[] normals;
    delete[] thickness;
    if (bitv)
        bu_bitv_free(bitv);
}

static void
handle_node(assetimport_read_state_t* pstate, aiNode* curr, struct wmember &regions)
{
    shader_properties_t *shader_prop = NULL;
    std::string region_name = generate_unique_name(curr->mName.data, pstate->gcv_options->default_name, 0);

    if (pstate->gcv_options->verbosity_level || pstate->assetimport_read_options->verbose) {
	bu_log("\nCurr node name | dfs index: %s | %d\n", curr->mName.data, pstate->dfs);
	bu_log("Curr node transformation:\n%f %f %f %f\n%f %f %f %f\n%f %f %f %f\n%f %f %f %f\n", 
		curr->mTransformation.a1, curr->mTransformation.a2, curr->mTransformation.a3, curr->mTransformation.a4,
		curr->mTransformation.b1, curr->mTransformation.b2, curr->mTransformation.b3, curr->mTransformation.b4,
		curr->mTransformation.c1, curr->mTransformation.c2, curr->mTransformation.c3, curr->mTransformation.c4,
		curr->mTransformation.d1, curr->mTransformation.d2, curr->mTransformation.d3, curr->mTransformation.d4);
	bu_log("Curr node children: %d\n", curr->mNumChildren);
	bu_log("Curr node meshes: %d\n", curr->mNumMeshes);
	bu_log("Region name: %s\n", region_name.c_str());
	if (!curr->mNumMeshes)
	    bu_log("\n");
    }

    /* book keeping for dfs node traversal */
    pstate->dfs++;

    /* generate a list to hold this node's meshes */
    struct wmember mesh;
    BU_LIST_INIT(&mesh.l);

    /* handle the current nodes meshes if any */
    for (size_t i = 0; i < curr->mNumMeshes; i++) {
	unsigned int mesh_idx = curr->mMeshes[i];\
				if (pstate->gcv_options->verbosity_level || pstate->assetimport_read_options->verbose)
				    bu_log("      uses mesh %d\n", mesh_idx);

	generate_geometry(pstate, mesh, mesh_idx);

	/* FIXME generate shader and color 
	 * this assumes all mesh under a region are using the same material data.
	 * if they're not, we're just using the last one
	 */
	if (shader_prop)
	    delete shader_prop;
	shader_prop = generate_shader(pstate, mesh_idx);
    }
    if (curr->mNumMeshes) {
	/* apply parent transformations */
	fastf_t tra[16];
	aimatrix_to_arr16(curr->mTransformation, tra);

	/* make region with all meshes */
	mk_lrcomb(pstate->fd_out, region_name.c_str(), &mesh, 1, shader_prop->name, shader_prop->args, shader_prop->rgb, pstate->id_no, 0, pstate->assetimport_read_options->mat_code, 100, 0);
	(void)mk_addmember(region_name.c_str(), &mesh.l, tra, WMOP_UNION);

	if (!pstate->assetimport_read_options->const_id)
	    pstate->id_no++;
    }

    /* when we're done adding children, add to top level */
    if (!curr->mNumChildren) {
	/* apply child's transformations */
	fastf_t tra[16];
	aimatrix_to_arr16(curr->mTransformation, tra);
	(void)mk_addmember(region_name.c_str(), &regions.l, tra, WMOP_UNION);
    }

    /* recursive call all children */
    for (size_t i = 0; i < curr->mNumChildren; i++) {
	/* skip children with no meshes under them (cameras, etc) */
	if (curr->mChildren[i]->mNumChildren || curr->mChildren[i]->mNumMeshes)
	    handle_node(pstate, curr->mChildren[i], regions);
    }

    if (shader_prop)
	delete shader_prop;
}

static int
convert_input(assetimport_read_state_t* pstate)
{
    BU_LIST_INIT(&pstate->all.l);
    convert_x3d_nurbs(pstate);

    /* have importer remove points and lines as we can't do anything with them */
    aiPropertyStore* props = aiCreatePropertyStore();
    aiSetImportPropertyInteger(props, AI_CONFIG_PP_SBP_REMOVE, aiPrimitiveType_LINE | aiPrimitiveType_POINT);
    /* we are taking one of the postprocessing presets to have
     * max quality with reasonable render times. But, we must keep 
     * seemingly redundant materials as we use the names for BRLCAD shaders
     */
    unsigned int import_flags = aiProcessPreset_TargetRealtime_MaxQuality & ~aiProcess_RemoveRedundantMaterials;
    pstate->scene = aiImportFileExWithProperties(pstate->input_file.c_str(),
						 import_flags,
						 NULL,
						 props);
    
    if (!pstate->scene) {
	if (pstate->converted_nurbs) {
	    mk_lcomb(pstate->fd_out, "all.g", &pstate->all, 0, (char *)NULL, (char *)NULL, (unsigned char *)NULL, 0);
	    bu_log("Converted %d X3D NURBS objects\n", pstate->converted_nurbs);
	    aiReleasePropertyStore(props);
	    return 1;
	}
	bu_log("ERROR: bad scene conversion\n");
	aiReleasePropertyStore(props);
	return 0;
    }

    if (pstate->gcv_options->verbosity_level || pstate->assetimport_read_options->verbose) {
	bu_log("Scene num meshes: %d\n", pstate->scene->mNumMeshes);
	bu_log("Scene num materials: %d\n", pstate->scene->mNumMaterials);
	bu_log("Scene num Animations: %d\n", pstate->scene->mNumAnimations);
	bu_log("Scene num Textures: %d\n", pstate->scene->mNumTextures);
	bu_log("Scene num Lights: %d\n", pstate->scene->mNumLights);
	bu_log("Scene num Cameras: %d\n\n", pstate->scene->mNumCameras);
    }

    if (pstate->scene->mNumMeshes) {
	if (pstate->gcv_options->verbosity_level || pstate->assetimport_read_options->verbose)
	    bu_log("-- root node --\n");
	handle_node(pstate, &pstate->scene->mRootNode[0], pstate->all);
    }

    /* make a top level 'all.g' */
    if (!BU_LIST_IS_EMPTY(&pstate->all.l))
	mk_lcomb(pstate->fd_out, "all.g", &pstate->all, 0, (char *)NULL, (char *)NULL, (unsigned char *)NULL, 0);

    /* import handles triangulation and scrubs extra shapes - this should *in theory* always report 100% */
    if (pstate->scene->mNumMeshes)
	bu_log("Converted ( %d / %d ) meshes ... %.2f%%\n", pstate->converted, pstate->scene->mNumMeshes, (float)pstate->converted / (float)pstate->scene->mNumMeshes * 100.0);
    if (pstate->converted_nurbs)
	bu_log("Converted %d X3D NURBS objects\n", pstate->converted_nurbs);

    /* cleanup */
    aiReleaseImport(pstate->scene);
    aiReleasePropertyStore(props);

    return (pstate->converted || pstate->converted_nurbs);
}

static const struct bu_cmd_option assetimport_read_schema_options[] = {
    BU_CMD_INTEGER(NULL, "starting-ident", assetimport_read_options_t, starting_id, "number",
	"Specify the starting ident for created regions."),
    BU_CMD_FLAG(NULL, "constant-ident", assetimport_read_options_t, const_id,
	"Keep the starting ident constant."),
    BU_CMD_INTEGER(NULL, "material", assetimport_read_options_t, mat_code, "number",
	"Specify the material code assigned to created regions."),
    BU_CMD_FLAG("v", "verbose", assetimport_read_options_t, verbose,
	"Print verbose conversion output."),
    BU_CMD_STRING(NULL, "format", assetimport_read_options_t, format, "format",
	"Specify the input file format."),
    BU_CMD_OPTION_NULL
};


static const struct bu_cmd_schema assetimport_read_schema = {
    "assetimport-read", "Asset import reader options.", assetimport_read_schema_options, NULL,
    BU_CMD_PARSE_INTERSPERSED, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};


static void
assetimport_read_create_schema_opts(const struct bu_cmd_schema **schema, void **dest_options_data)
{
    assetimport_read_options_t* options_data;

    BU_ALLOC(options_data, assetimport_read_options_t);
    *dest_options_data = options_data;

    options_data->starting_id = 1000;
    options_data->const_id = 0;
    options_data->mat_code = 1;
    options_data->verbose = 0;
    options_data->format = NULL;

    *schema = &assetimport_read_schema;
}

static void
assetimport_read_free_opts(void *options_data)
{
    bu_free(options_data, "options_data");
}

static int
assetimport_read(struct gcv_context *context, const struct gcv_opts* gcv_options, const void *options_data, const char *source_path)
{
    assetimport_read_state_t state;

    state.gcv_options = gcv_options;
    state.assetimport_read_options = (assetimport_read_options_t*)options_data;
    state.id_no = state.assetimport_read_options->starting_id;
    state.input_file = source_path;
    struct rt_wdb *wdbp = wdb_dbopen(context->dbip, RT_WDB_TYPE_DB_INMEM);
    state.fd_out = wdbp;

    /* check and validate the specified input file type against ai
     * checks using file extension if no --format is supplied
     * this is likely all a 'can_read' function would need
     */
    std::string extension;
    if (!BU_STR_EMPTY(state.assetimport_read_options->format)) {
	/* intentional setting format trumps file extension */
	extension = state.assetimport_read_options->format;
    } else {
	struct bu_vls ext = BU_VLS_INIT_ZERO;
	if (bu_path_component(&ext, source_path, BU_PATH_EXT))
	    extension = "." + std::string(bu_vls_cstr(&ext));
	bu_vls_free(&ext);
    }
    if (extension.empty()) {
	bu_log("ERROR: Please provide a file with a valid extension, or specify format with --format\n");
	return 0;
    }
    if (AI_FALSE == aiIsExtensionSupported(extension.c_str())) {
	bu_log("ERROR: The specified model file type is currently unsupported in assetimport conversion.\n");
	return 0;
    }

    mk_id_units(state.fd_out, "Conversion using Asset Importer Library (assetimport)", "mm");

    int ret = convert_input(&state);
    return ret;
}


static int
assetimport_can_read(const char* data)
{
    /* TODO FIXME - currently 'can_read' is unused by gcv */
    if (!data)
	return 0;

    return 1;
}


/* filter setup */
extern "C"
{
    static const struct gcv_filter gcv_conv_assetimport_read = {
	"Assetimport Reader", GCV_FILTER_READ, BU_MIME_MODEL_VND_ASSETIMPORT, assetimport_can_read,
	NULL, assetimport_read_free_opts, assetimport_read
    };

    extern const struct gcv_filter gcv_conv_assetimport_write;
    extern void assetimport_write_create_schema_opts(const struct bu_cmd_schema **schema,
	void **dest_options_data);
    static const struct gcv_filter * const filters[] = { &gcv_conv_assetimport_read, &gcv_conv_assetimport_write, NULL };
    static const struct gcv_filter_schema filter_schemas[] = {
	{&gcv_conv_assetimport_read, assetimport_read_create_schema_opts},
	{&gcv_conv_assetimport_write, assetimport_write_create_schema_opts},
	{NULL, NULL}
    };

    const struct gcv_plugin gcv_plugin_info_s = { filters };
    static const struct gcv_native_plugin gcv_plugin_native_info_s = { filter_schemas };

    COMPILER_DLLEXPORT const struct gcv_plugin *
	gcv_plugin_info() { return &gcv_plugin_info_s; }

    COMPILER_DLLEXPORT const struct gcv_native_plugin *
	gcv_plugin_native_info() { return &gcv_plugin_native_info_s; }
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
