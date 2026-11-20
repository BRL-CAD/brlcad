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
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "gcv/api.h"
#include "bio.h"
#include "../mime_catalog.h"
#include "../mime_probe.h"
#include "format_map.h"
#include "mime_skeletal.h"
#include "mime_nff.h"
#include "mime_raw.h"
#include "mime_scene_text.h"
#include "mime_assbin.h"
#include "mime_ply.h"
#include "mime_stl.h"
#include "mime_fbx.h"
#include "mime_opengex.h"
#include "mime_obj.h"
#include "mime_off.h"
#include "mime_dxf.h"
#include "mime_xml.h"
#include "mime_gltf.h"
#include "mime_p21.h"
#include "mime_unreal.h"
#include "xgl_mime.h"
#include "wdb.h"
#include <assimp/BaseImporter.h>
#include "bg/trimesh.h"
#include "bu/color.h"
#include "bu/opt.h"
#include "bu/path.h"
#include "bu/str.h"
#include "bu/vls.h"

/* assimp headers */
#include <assimp/cimport.h>
#include <assimp/config.h>
#include <assimp/cexport.h>
#include <assimp/Importer.hpp>
#include <assimp/importerdesc.h>
#include <assimp/scene.h>
#include <assimp/metadata.h>
#include <assimp/commonMetaData.h>
#include <assimp/postprocess.h>

/* pugixml headers */
#include <pugixml.hpp>

typedef struct shader_properties {
    char *name;		/* shader name, or NULL */
    char *args;		/* shader args, or NULL */
    unsigned char *rgb; /* NULL => no color */
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
    char* format;                                   /* legacy input type assertion */
} assetimport_read_options_t;

typedef struct assetimport_read_state {
    const struct gcv_opts* gcv_options;             /* global options */
    assetimport_read_options_t* assetimport_read_options;     /* internal options */

    std::string input_file;			    /* name of the input file */
    struct rt_wdb *fd_out;			    /* Resulting BRL-CAD file */
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
    aiColor3D diffuse_color(1); /* diffuse color -> defaults to white */
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
	    float tr = 0.0;  /* transparency */
	    float re = 0.0;  /* mirror reflectance */
	    float sp = 0.7;  /* specular reflectivity */
	    float di = 0.3;  /* diffuse reflectivity */
	    float ri = 1.0;  /* refractive index */
	    float ex = 0.0;  /* extinction */
	    float sh = 10.0; /* shininess */
	    float em = 0.0;  /* emission */

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
	struct bu_vls msg = BU_VLS_INIT_ZERO;
	if (bu_opt_fastf_t(&msg, 1, &av, &val) == 1) {
	    ret.push_back((double)val);
	} else {
	    bu_vls_trimspace(&msg);
	    bu_log("WARNING: skipping invalid X3D numeric value '%s'%s%s\n", argv[i],
		    bu_vls_strlen(&msg) ? ": " : "", bu_vls_cstr(&msg));
	}
	bu_vls_free(&msg);
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
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    if (bu_opt_int(&msg, 1, &av, &val) == 1) {
	bu_vls_free(&msg);
	return val;
    }

    bu_vls_trimspace(&msg);
    bu_log("WARNING: invalid X3D integer value '%s' for %s; using %d%s%s%s\n", attr.value(), name, def,
	    bu_vls_strlen(&msg) ? " (" : "", bu_vls_cstr(&msg), bu_vls_strlen(&msg) ? ")" : "");
    bu_vls_free(&msg);
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
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    if (bu_opt_vect_t(&msg, 1, &av, (void *)&parsed) > 0) {
	VMOVE(v, parsed);
    } else {
	bu_vls_trimspace(&msg);
	bu_log("WARNING: invalid X3D vector value '%s' for %s%s%s%s\n", attr.value(), name,
		bu_vls_strlen(&msg) ? " (" : "", bu_vls_cstr(&msg), bu_vls_strlen(&msg) ? ")" : "");
    }
    bu_vls_free(&msg);
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
convert_x3d_nurbs(assetimport_read_state_t *pstate, const pugi::xml_node &root)
{
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

static const aiImporterDesc *
assetimport_loaded_format(const Assimp::Importer &importer)
{
    /* The SDK records the loader it chose independently of the filename.
     * SourceAsset_Format metadata can contain only the filename extension. */
    int index = importer.GetPropertyInteger("importerIndex", -1);
    return index < 0 ? NULL : importer.GetImporterInfo(static_cast<size_t>(index));
}

static bool
assetimport_ogre_filename(const std::string &filename, const char *type)
{
    std::string suffix = BU_STR_EQUAL(type, "model/vnd.ogre.mesh") ? ".mesh" : ".mesh.xml";
    return filename.size() >= suffix.size() && !bu_strcasecmp(filename.c_str() + filename.size() - suffix.size(), suffix.c_str());
}

static bool
assetimport_ogre_xml_root(const mime_xml::profile &root)
{
    return root.valid && root.qname == "mesh" && (!root.namespace_known || root.uri.empty());
}

static int
assetimport_ogre_xml_traits(mime_xml::profile root, bu_mime_traits &traits)
{
    root.version.clear(); // XML syntax is not an Ogre serializer revision.
    return root.append_traits(traits) && bu_mime_traits_add(&traits, "compression", "none");
}

static bu_mime_probe_status
assetimport_ogre_xml_prefix(const bu_mime_probe_input *input, bu_mime_traits *traits)
{
    std::string xml;
    if (input->size) xml.assign(reinterpret_cast<const char *>(input->data), input->size);
    std::istringstream stream(xml);
    auto root = mime_xml::inspect(stream);
    if (!root.valid)
	return !input->complete && root.plausible ? BU_MIME_PROBE_INCOMPLETE : BU_MIME_PROBE_NO_MATCH;
    if (!assetimport_ogre_xml_root(root)) return BU_MIME_PROBE_NO_MATCH;
    pugi::xml_document document;
    auto parsed = document.load_buffer(xml.data(), xml.size());
    if (parsed.status == pugi::status_out_of_memory) return BU_MIME_PROBE_ERROR;
    auto mesh = document.document_element();
    bool recognized = mesh.child("submeshes").child("submesh") ||
	mesh.child("sharedgeometry").child("vertexbuffer").child("vertex").child("position");
    if (!recognized) return input->complete ? BU_MIME_PROBE_NO_MATCH : BU_MIME_PROBE_INCOMPLETE;
    if (traits && !assetimport_ogre_xml_traits(root, *traits)) return BU_MIME_PROBE_ERROR;
    return BU_MIME_PROBE_HEURISTIC;
}

static bu_mime_probe_status
assetimport_probe(const bu_mime_probe_input *input, const char *type)
{
    if (BU_STR_EQUAL(type, mime_blender::type)) {
	const auto result = assetimport_blender::prefix(*input);
	if (result.error) return BU_MIME_PROBE_ERROR;
	if (result.header.identified) return BU_MIME_PROBE_SIGNATURE;
	return result.header.incomplete ? BU_MIME_PROBE_INCOMPLETE : BU_MIME_PROBE_NO_MATCH;
    }
    if (BU_STR_EQUAL(type, "model/vnd.ogre.mesh+xml")) return assetimport_ogre_xml_prefix(input, NULL);
    return assetimport_xgl::probe(input, type);
}

static int
assetimport_inspect(const bu_mime_probe_input *input, const char *type, bu_mime_traits *traits)
{
    if (BU_STR_EQUAL(type, mime_blender::type)) {
	const auto result = assetimport_blender::prefix(*input);
	return !result.error && (!result.header.identified || result.append(*traits));
    }
    if (BU_STR_EQUAL(type, "model/vnd.ogre.mesh+xml")) return assetimport_ogre_xml_prefix(input, traits) != BU_MIME_PROBE_ERROR;
    return assetimport_xgl::inspect(input, type, traits);
}

static const char *
assetimport_loaded_type(const aiImporterDesc *format, const std::string &filename, struct gcv_context *context)
{
    if (!format) return NULL;
    if (BU_STR_EQUAL(format->mName, "Industry Foundation Classes (IFC) Importer")) return "application/p21";
    if (BU_STR_EQUAL(format->mName, "Collada Importer")) {
	/* The SDK tries ZIP before XML, except when .dae forces XML input. */
	return !assetimport_zip::suffix(filename, ".dae") && assetimport_zip::end_record(filename) ?
	    assetimport_zip::zae_type : "model/vnd.collada+xml";
    }
    if (BU_STR_EQUAL(format->mName, "Unreal Mesh Importer")) {
	mime_unreal::files files(filename);
	if (!files.valid()) return NULL;
	return files.part == "script" ? mime_unreal::script_type : mime_unreal::mesh_type;
    }
    if (BU_STR_EQUAL(format->mName, "Quake III BSP Importer")) return "application/vnd.idsoftware.pk3+zip";
    if (BU_STR_EQUAL(format->mName, "Ogre3D Mesh Importer"))
	return assetimport_ogre_filename(filename, "model/vnd.ogre.mesh") ? "model/vnd.ogre.mesh" : "model/vnd.ogre.mesh+xml";
    if (BU_STR_EQUAL(format->mName, "XGL Importer"))
	return assetimport_xgl::compressed_filename(filename) ? assetimport_xgl::compressed_type : assetimport_xgl::xml_type;
    if (BU_STR_EQUAL(format->mName, "Neutral File Format Importer")) {
	/* The SDK selects its two NFF parsers from the initial token, before
	 * skipping comments or whitespace. Verify the branch it actually used. */
	std::ifstream stream(filename, std::ios::binary);
	if (!stream.is_open()) return NULL;
	const size_t token_size = 3, bom_size = 3;
	char header[bom_size + token_size + 1];
	stream.read(header, sizeof(header));
	if (stream.bad()) return NULL;
	std::string prefix(header, static_cast<size_t>(stream.gcount()));
	if (prefix.compare(0, bom_size, "\xef\xbb\xbf") == 0) prefix.erase(0, bom_size);
	bool sense8 = prefix.compare(0, token_size, "nff") == 0 && prefix.size() > token_size &&
	    prefix.find_first_of(" \t\r\n\f", token_size) == token_size;
	return sense8 ? "model/vnd.sense8.nff" : "model/prs.haines.nff";
    }
    bool gltf = assetimport_gltf_importer(format->mName);
    bool lightwave = BU_STR_EQUAL(format->mName, "LightWave Scene Importer");
    if (gltf || lightwave) {
	FILE *file = fopen(filename.c_str(), "rb");
	if (!file) return NULL;
	char magic[4];
	size_t size = fread(magic, 1, sizeof(magic), file);
	int error = ferror(file);
	if (fclose(file)) error = 1;
	if (error || size != sizeof(magic)) return NULL;
	if (lightwave)
	    return !memcmp(magic, "LWSC", sizeof(magic)) ? "image/x-lws" :
		!memcmp(magic, "LWMO", sizeof(magic)) ? "model/vnd.newtek.motion" : NULL;
	auto identities = assetimport_import_identities(format->mName, !memcmp(magic, "glTF", sizeof(magic)) ? "glb" : "gltf");
	return identities.size() == 1 ? identities.front() : NULL;
    }
    std::vector<const char *> identities;
    for (const auto &extension : gcv_mime_catalog::split(format->mFileExtensions)) {
	for (const char *candidate : assetimport_import_identities(format->mName, extension))
	    if (std::none_of(identities.begin(), identities.end(), [candidate](const char *name) {return BU_STR_EQUAL(name, candidate);}))
		identities.push_back(candidate);
    }
    if (identities.empty()) return NULL;
    if (identities.size() == 1) return identities.front();
    if (std::any_of(identities.begin(), identities.end(), mime_xml::is_type)) {
	/* XML declarations and comments can precede the document element by
	 * more than a fixed signature window. Reuse full root inspection. */
	std::ifstream stream(filename, std::ios::binary);
	if (!stream.is_open()) return NULL;
	auto root = mime_xml::inspect(stream);
	const char *type = stream.bad() ? NULL : root.type();
	for (const char *candidate : identities)
	    if (type && BU_STR_EQUAL(candidate, type)) return candidate;
	return NULL;
    }
    /* One SDK importer/extension may accept unrelated file families. Match
     * actual content against the audited identities rather than choosing
     * the first binding or trusting the extension supplied by the caller. */
    const size_t header_size = 3 * sizeof(uint32_t);
    unsigned char header[header_size];
    FILE *file = fopen(filename.c_str(), "rb");
    if (!file) return NULL;
    size_t size = fread(header, 1, sizeof(header), file);
    int error = ferror(file);
    if (fclose(file)) error = 1;
    if (error) return NULL;
    bu_mime_result *result = NULL;
    bool inspected = bu_mime_detect(gcv_mime_registry(context), NULL, header, size, 0, &result) != BU_MIME_ERROR;
    const char *identity = NULL;
    for (const char *candidate : identities)
	for (size_t i = 0; inspected && result && i < result->count; ++i)
	    if (BU_STR_EQUAL(result->candidates[i].name, candidate)) {
		if (identity && !BU_STR_EQUAL(identity, candidate)) {
		    bu_mime_result_free(result);
		    return NULL;
		}
		identity = candidate;
	    }
    bu_mime_result_free(result);
    return identity;
}

static size_t
assetimport_profile_header(const aiImporterDesc &format, const std::string &filename,
    unsigned char *header, size_t capacity)
{
    FILE *file = fopen(filename.c_str(), "rb");
    if (!file) return 0;
    size_t size = 0;
    if (BU_STR_EQUAL(format.mName, "LightWave/Modo Object Importer")) {
	const size_t form_header_size = 3 * sizeof(uint32_t);
	const size_t chunk_header_size = 2 * sizeof(uint32_t);
	const size_t version_size = 2 * sizeof(uint32_t);
	if (capacity >= form_header_size && fread(header, 1, form_header_size, file) == form_header_size) {
	    size = form_header_size;
	    if (!memcmp(header, "FORM", sizeof(uint32_t)) && !memcmp(header + 2 * sizeof(uint32_t), "LXOB", sizeof(uint32_t)) &&
		capacity >= form_header_size + chunk_header_size + version_size) {
		uint32_t form_size = gcv_probe_u32be(header + sizeof(uint32_t));
		uint64_t remaining = UINT64_MAX;
		if (form_size) remaining = form_size >= sizeof(uint32_t) ? form_size - sizeof(uint32_t) : 0;
		unsigned char *chunk = header + form_header_size;
		while (remaining >= chunk_header_size && fread(chunk, 1, chunk_header_size, file) == chunk_header_size) {
		    uint32_t chunk_size = gcv_probe_u32be(chunk + sizeof(uint32_t));
		    uint64_t padded_size = uint64_t(chunk_size) + (chunk_size & 1);
		    remaining -= chunk_header_size;
		    if (padded_size > remaining) break;
		    if (!memcmp(chunk, "VRSN", sizeof(uint32_t)) && chunk_size >= version_size) {
			if (fread(chunk + chunk_header_size, 1, version_size, file) == version_size)
			    size = form_header_size + chunk_header_size + version_size;
			break;
		    }
		    if (bu_fseek(file, static_cast<b_off_t>(padded_size), SEEK_CUR)) break;
		    remaining -= padded_size;
		}
	    }
	}
    } else if (BU_STR_EQUAL(format.mName, "Silo SIB Importer")) {
	const size_t chunk_header_size = 2 * sizeof(uint32_t);
	const size_t version_size = sizeof(uint32_t);
	/* SIB's version chunk need not be first in the file. */
	if (capacity >= chunk_header_size + version_size)
	    while (fread(header, 1, chunk_header_size, file) == chunk_header_size) {
		uint32_t chunk_size = gcv_probe_u32le(header + sizeof(uint32_t));
		if (!memcmp(header, "HEAD", sizeof(uint32_t))) {
		    if (chunk_size >= version_size && fread(header + chunk_header_size, 1, version_size, file) == version_size)
			size = chunk_header_size + version_size;
		    break;
		}
		if (bu_fseek(file, static_cast<b_off_t>(chunk_size), SEEK_CUR)) break;
	    }
    } else {
	size = fread(header, 1, capacity, file);
    }
    int error = ferror(file);
    if (fclose(file)) error = 1;
    return error ? 0 : size;
}

static bool
assetimport_text_type(const char *type)
{
    return mime_skeletal::is_type(type) || mime_nff::is_type(type) || mime_raw::is_type(type);
}

static int
assetimport_xml_traits(const std::string &filename, const char *type, const bu_mime_traits *requested, bu_mime_traits &traits)
{
    std::ifstream stream(filename, std::ios::binary);
    if (!stream.is_open()) return 0;
    auto root = mime_xml::inspect(stream);
    const bool ogre = BU_STR_EQUAL(type, "model/vnd.ogre.mesh+xml");
    if (stream.bad() || (ogre ? !assetimport_ogre_xml_root(root) : !root.type() || !BU_STR_EQUAL(root.type(), type))) return 0;
    if (ogre) root.version.clear();
    const bool charset = bu_mime_traits_get(requested, "encoding") || bu_mime_traits_get(requested, "text.ascii-only");
    if (charset) {
	stream.clear();
	stream.seekg(0);
	if (!stream) return 0;
	const auto text = mime_xml::inspect_text(stream, root);
	if (stream.bad() || text.encoding.empty() || !text.append_traits(root, traits)) return 0;
    } else if (!root.append_traits(traits)) return 0;
    if (ogre) return bu_mime_traits_add(&traits, "compression", "none");
    if (BU_STR_EQUAL(type, "model/amf") || BU_STR_EQUAL(type, "model/vnd.collada+xml") || BU_STR_EQUAL(type, "model/x3d+xml")) return 1;

    pugi::xml_document document;
    if (!document.load_file(filename.c_str())) return 0;
    auto element = document.document_element();
    if (root.qname != element.name()) return 0;
    auto lower = [](std::string value) {
	for (char &c : value) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	return value.empty() ? std::string("unspecified") : value;
    };
    auto local_name = [](const std::string &name) {
	size_t colon = name.find_last_of(':');
	return colon == std::string::npos ? name : name.substr(colon + 1);
    };
    std::set<std::string> features;
    const char *key;
    if (BU_STR_EQUAL(type, "model/vnd.irrlicht.mesh+xml")) {
	key = "irrlicht.vertex-layouts";
	for (auto buffer : element.children()) {
	    std::string name = lower(buffer.name());
	    if (name == "buffer") features.insert(lower(buffer.child("vertices").attribute("type").value()));
	    else if (local_name(name) == "buffer")
		features.insert("qualified-buffer");
	}
    } else {
	key = "irrlicht.node-types";
	std::vector<pugi::xml_node> parents = {element};
	while (!parents.empty()) {
	    auto parent = parents.back();
	    parents.pop_back();
	    for (auto node : parent.children()) {
		std::string name = lower(node.name());
		if (name == "node") {
		    std::string kind = lower(node.attribute("type").value());
		    if ((kind == "cube" || kind == "sphere") && !node.child("materials").child("attributes")) {
			/* Its default-material path does not create a material for
			 * these procedural shapes. Do not invoke that path. */
			bu_log("SDK Irrlicht %s requires an explicit material\n", kind.c_str());
			return 0;
		    }
		    features.insert(kind);
		    parents.push_back(node);
		} else if (local_name(name) == "node")
		    features.insert("qualified-node");
	    }
	}
    }
    return mime_detail::append_set(traits, key, features);
}

static int
assetimport_unreal_traits(const std::string &filename, const char *type, bu_mime_traits &traits)
{
    mime_unreal::files files(filename);
    if (!files.valid() || (files.part == "script") != BU_STR_EQUAL(type, mime_unreal::script_type)) {
	bu_log("SDK Unreal input requires native _d.3d, _a.3d or .uc companion names\n");
	return 0;
    }
    auto read_header = [](const std::string &path, const std::string &part) {
	mime_unreal::binary invalid;
	struct stat info;
	if (stat(path.c_str(), &info) || info.st_size < 0) return invalid;
	FILE *file = fopen(path.c_str(), "rb");
	if (!file) return invalid;
	unsigned char header[mime_unreal::data_header_size];
	size_t count = fread(header, 1, sizeof(header), file);
	int error = ferror(file);
	if (fclose(file)) error = 1;
	if (error) return invalid;
	bu_mime_probe_input input = {header, count, static_cast<uint64_t>(count) == static_cast<uint64_t>(info.st_size), static_cast<uint64_t>(info.st_size), 1};
	return mime_unreal::inspect_binary(part, input);
    };
    auto data = read_header(files.base + "_d.3d", "data");
    auto animation = read_header(files.base + "_a.3d", "animation");
    if (!data.valid || !animation.valid || animation.frame_size % data.vertices) {
	bu_log("SDK Unreal input requires compatible topology and frame companions\n");
	return 0;
    }
    uint32_t stride = animation.frame_size / data.vertices;
    if (files.part == "script") {
	if (stride != mime_unreal::packed_vertex_size) return 0;
	std::ifstream source(filename, std::ios::binary);
	if (!source.is_open()) return 0;
	auto profile = mime_unreal::inspect_script(source, true);
	return !source.bad() && profile.append(traits);
    }
    const auto &source = files.part == "data" ? data : animation;
    const auto &companion = files.part == "data" ? animation : data;
    return source.append(traits) && companion.append_counts(traits) &&
	bu_mime_traits_add(&traits, "unreal.vertex-stride", std::to_string(stride).c_str());
}

static int
assetimport_fbx_traits(const std::string &filename, const bu_mime_traits *requested,
    const aiScene *scene, bu_mime_traits &traits)
{
    aiString declared;
    if (!scene || !scene->mMetaData || !scene->mMetaData->Get(AI_METADATA_SOURCE_FORMAT_VERSION, declared)) return 0;
    const std::string code = mime_detail::decimal_field(declared.C_Str());
    if (code.empty()) return 0;
    std::ifstream stream(filename, std::ios::binary);
    if (!stream.is_open()) return 0;
    char header[mime_fbx::binary_header_size];
    stream.read(header, sizeof(header));
    const size_t size = static_cast<size_t>(stream.gcount());
    mime_fbx::profile profile;
    profile.identified = true;
    if (mime_fbx::sdk_binary(header, size)) {
	if (size < sizeof(header)) return 0;
	profile.serialization = "binary";
	profile.encoding = "binary";
	/* The SDK ignores the signature's padding but always reads this
	 * little-endian word. Keep it separate from the parsed DOM version. */
	profile.binary_version = std::to_string(mime_binary::little_u32(
	    reinterpret_cast<const unsigned char *>(header) + mime_fbx::binary_version_at));
    } else {
	stream.clear();
	stream.seekg(0);
	if (!stream) return 0;
	mime_unicode::text_source source(stream);
	profile.observe_text(source, true, requested && bu_mime_traits_get(requested, "encoding"));
    }
    const char *version = mime_fbx::version(code);
    const bool charset = requested && bu_mime_traits_get(requested, "encoding");
    return !stream.bad() && (!charset || !profile.encoding.empty()) && profile.append_traits(traits) &&
	bu_mime_traits_add(&traits, "fbx.document-version-code", code.c_str()) &&
	(!version || bu_mime_traits_add(&traits, "version", version));
}

static int
assetimport_input_profile(const aiImporterDesc *format, const std::string &filename,
    const char *mime_type, struct gcv_context *context, const aiScene *scene = NULL)
{
    if (!format) return 1;
    if (assetimport_gltf_importer(format->mName) && !assetimport_gltf_host()) return 0;
    auto supported = assetimport_import_profiles(format->mName, mime_type);
    if (!mime_type) return 0;
    const bu_mime_traits *requested = gcv_target_request_traits(context);
    const bool charset = requested && (bu_mime_traits_get(requested, "encoding") || bu_mime_traits_get(requested, "text.ascii-only"));
    const bool declared_profile = !supported.empty();
    if (!declared_profile && (!requested || !requested->count)) return 1;
    if ((BU_STR_EQUAL(mime_type, mime_opengex::type) || BU_STR_EQUAL(mime_type, mime_obj::type)) && !scene) {
	std::ifstream stream(filename, std::ios::binary);
	if (!stream.is_open()) return 0;
	mime_unicode::text_source source(stream);
	const bool supported_width = BU_STR_EQUAL(source.width(), "8");
	if (!supported_width) bu_log("SDK %s input requires 8-bit source text\n", mime_type);
	return !stream.bad() && supported_width;
    }
    if (BU_STR_EQUAL(mime_type, mime_fbx::type) && !scene) return 1;
    /* The 3MF decoder opens ZIP only, then follows the package's root
     * relationship. Successful decoding establishes those container facts
     * without mistaking a ZIP prefix for a particular model identity. */
    if (assetimport_zip::is_3mf(mime_type) && !scene) return 1;
    if (assetimport_zip::is_zae(mime_type) && !scene) {
	bool native = assetimport_zip::suffix(filename, ".zae");
	if (!native) bu_log("SDK ZAE reader requires a .zae filename suffix\n");
	return native;
    }
    if (BU_STR_EQUAL(mime_type, "application/vnd.idsoftware.pk3+zip") && !scene) {
	bu_vls extension = BU_VLS_INIT_ZERO;
	bu_path_component(&extension, filename.c_str(), BU_PATH_EXT);
	bool native = !bu_strcasecmp(bu_vls_cstr(&extension), "pk3") || !bu_strcasecmp(bu_vls_cstr(&extension), "bsp");
	bu_vls_free(&extension);
	if (!native) bu_log("SDK PK3 reader requires a .pk3 or .bsp filename suffix\n");
	/* This decoder always opens a ZIP before finding its map. Use that
	 * verified result, allowing archive preambles, rather than guessing
	 * container identity from a short header. No data is published yet. */
	return native;
    }

    bu_mime_result *result = NULL;
    bu_mime_traits text_traits = BU_MIME_TRAITS_INIT_ZERO;
    bool inspected = false;
    const bu_mime_traits *observed = NULL;
    if (!declared_profile) {
	/* A type-only capability does not establish additional file properties.
	 * Check explicit assertions independently of the resolved request. */
	inspected = gcv_detect_file(context, filename.c_str(), &result, NULL) != BU_MIME_ERROR;
	for (size_t i = 0; inspected && result && i < result->count; ++i)
	    if (BU_STR_EQUAL(result->candidates[i].name, mime_type)) observed = &result->candidates[i].traits;
	supported.emplace_back();
    } else if (BU_STR_EQUAL(mime_type, mime_obj::type)) {
	std::ifstream stream(filename, std::ios::binary);
	if (!stream.is_open()) return 0;
	mime_unicode::text_source source(stream);
	if (!BU_STR_EQUAL(source.width(), "8")) return 0;
	const auto profile = mime_obj::observe(source, charset,
	    bu_mime_traits_get(requested, "obj.material-library-reference") != NULL);
	inspected = scene && !stream.bad() && (!charset || profile.repertoire_known) && profile.append_traits(text_traits);
	observed = &text_traits;
    } else if (BU_STR_EQUAL(mime_type, mime_opengex::type)) {
	std::ifstream stream(filename, std::ios::binary);
	if (!stream.is_open()) return 0;
	mime_unicode::text_source source(stream);
	mime_opengex::profile profile;
	profile.identified = scene != NULL;
	profile.observe_text(source, true, charset);
	inspected = !stream.bad() && (!charset || profile.repertoire_known) && profile.append_traits(text_traits);
	observed = &text_traits;
    } else if (BU_STR_EQUAL(mime_type, mime_fbx::type)) {
	inspected = assetimport_fbx_traits(filename, requested, scene, text_traits) != 0;
	observed = &text_traits;
    } else if (BU_STR_EQUAL(mime_type, "model/stl")) {
	FILE *file = fopen(filename.c_str(), "rb");
	if (!file) return 0;
	const auto profile = mime_stl_file_profile(file, requested && bu_mime_traits_get(requested, "encoding"));
	inspected = !ferror(file) && mime_stl_append(&profile, &text_traits);
	if (fclose(file)) inspected = false;
	if (inspected && !assetimport_zip::suffix(filename, ".stl")) {
	    /* The SDK's filename hint can select files whose identifying text
	     * is outside its own bounded loader search. Query that search for
	     * neutral filenames rather than guessing its configured limit. */
	    Assimp::Importer detector;
	    const auto *reader = detector.GetImporter("stl");
	    inspected = reader && reader->CanRead(filename, detector.GetIOHandler(), true);
	    if (!inspected) bu_log("SDK STL input requires a .stl filename suffix for this header\n");
	}
	observed = &text_traits;
    } else if (BU_STR_EQUAL(mime_type, mime_ply::type)) {
	std::ifstream stream(filename, std::ios::binary);
	if (!stream.is_open()) return 0;
	const auto profile = mime_ply::inspect(stream, true);
	inspected = !stream.bad() && !profile.encoding.empty() && profile.append_traits(text_traits);
	observed = &text_traits;
    } else if (BU_STR_EQUAL(mime_type, mime_blender::type)) {
	inspected = assetimport_blender::file_traits(filename, text_traits) != 0;
	observed = &text_traits;
    } else if (BU_STR_EQUAL(format->mName, "Industry Foundation Classes (IFC) Importer")) {
	std::ifstream stream(filename, std::ios::binary);
	if (!stream.is_open()) return 0;
	auto profile = mime_p21::inspect(stream, true);
	inspected = profile.append_traits(text_traits);
	observed = &text_traits;
    } else if (assetimport_zip::is_zae(mime_type)) {
	aiString version;
	inspected = scene && scene->mMetaData && scene->mMetaData->Get(AI_METADATA_SOURCE_FORMAT_VERSION, version);
	std::string family = inspected ? mime_xml::collada_schema(version.C_Str()) : std::string{};
	inspected = inspected && !family.empty() &&
	    bu_mime_traits_add(&text_traits, "encoding", "binary") && bu_mime_traits_add(&text_traits, "container", "zip") &&
	    bu_mime_traits_add(&text_traits, "collada.document-version", version.C_Str()) &&
	    bu_mime_traits_add(&text_traits, "collada.schema-version", family.c_str());
	observed = &text_traits;
    } else if (assetimport_gltf_importer(format->mName)) {
	inspected = mime_gltf::file_traits(filename.c_str(), mime_type, text_traits);
	const char *minimum = bu_mime_traits_get(&text_traits, "gltf.minimum-version");
	int comparison = 0;
	const char *maximum = BU_STR_EQUAL(format->mName, "glTF2 Importer") ? "2.0" : "1.0";
	inspected = inspected && minimum && bu_mime_version_compare(minimum, maximum, &comparison) && comparison <= 0;
	observed = &text_traits;
    } else if (mime_unreal::is_type(mime_type)) {
	inspected = assetimport_unreal_traits(filename, mime_type, text_traits) != 0;
	observed = &text_traits;
    } else if (BU_STR_EQUAL(mime_type, "application/vnd.idsoftware.pk3+zip") || assetimport_zip::is_3mf(mime_type)) {
	inspected = scene && bu_mime_traits_add(&text_traits, "encoding", "binary") && bu_mime_traits_add(&text_traits, "container", "zip");
	observed = &text_traits;
    } else if (assetimport_xgl::is_type(mime_type)) {
	inspected = assetimport_xgl::file_traits(filename, mime_type, requested, text_traits) != 0;
	observed = &text_traits;
    } else if (mime_xml::is_type(mime_type)) {
	inspected = assetimport_xml_traits(filename, mime_type, requested, text_traits) != 0;
	observed = &text_traits;
    } else if (BU_STR_EQUAL(mime_type, mime_x::type)) {
	inspected = assetimport_x::file_traits(filename, requested, text_traits) != 0;
	observed = &text_traits;
    } else if (BU_STR_EQUAL(mime_type, mime_assbin::type)) {
	static_assert(mime_assbin::double_support == ASSIMP_CFLAGS_DOUBLE_SUPPORT, "Assimp real-width flag changed");
	std::ifstream stream(filename, std::ios::binary);
	if (!stream.is_open()) return 0;
	unsigned char header[mime_assbin::header_size];
	stream.read(reinterpret_cast<char *>(header), sizeof(header));
	const bu_mime_probe_input input = {header, static_cast<size_t>(stream.gcount()), 0, 0, 0};
	const auto profile = mime_assbin::inspect(input);
	inspected = !stream.bad() && input.size == sizeof(header) && profile.identified && profile.append_traits(text_traits);
	observed = &text_traits;
    } else if (mime_scene_text::is_type(mime_type)) {
	std::ifstream stream(filename, std::ios::binary);
	if (!stream.is_open()) return 0;
	const bool inspect_encoding = requested && bu_mime_traits_get(requested, "encoding");
	const auto profile = mime_scene_text::inspect(stream, true, inspect_encoding);
	inspected = !stream.bad() && profile.type == mime_type && profile.append_traits(text_traits);
	if (inspected && BU_STR_EQUAL(mime_type, mime_scene_text::ac3d_type) && profile.bom && profile.width == "8" &&
	    !assetimport_zip::suffix(filename, ".ac") && !assetimport_zip::suffix(filename, ".acc") &&
	    !assetimport_zip::suffix(filename, ".ac3d")) {
	    bu_log("SDK AC3D input with a UTF-8 BOM requires a native AC3D filename suffix\n");
	    inspected = false;
	}
	observed = &text_traits;
    } else if (BU_STR_EQUAL(mime_type, mime_dxf::type)) {
	std::ifstream stream(filename, std::ios::binary);
	if (!stream.is_open()) return 0;
	const auto profile = mime_dxf::inspect(stream, true);
	inspected = !stream.bad() && profile.identified && (!profile.header_seen || profile.header_complete) &&
	    profile.append_traits(text_traits);
	observed = &text_traits;
    } else if (BU_STR_EQUAL(mime_type, mime_off::type)) {
	std::ifstream stream(filename, std::ios::binary);
	if (!stream.is_open()) return 0;
	/* SDK loader selection searches only three bytes. Prefixed keywords,
	 * preambles and headerless records therefore need its native suffix. */
	constexpr size_t signature_search_size = sizeof("OFF") - 1;
	char signature[signature_search_size];
	stream.read(signature, sizeof(signature));
	const bool bare_magic = stream.gcount() == static_cast<std::streamsize>(sizeof(signature)) &&
	    !memcmp(signature, "OFF", sizeof(signature));
	stream.clear();
	stream.seekg(0);
	if (!bare_magic && !assetimport_zip::suffix(filename, ".off")) {
	    bu_log("SDK OFF reader requires a .off filename for prefixed or headerless input\n");
	    return 0;
	}
	const auto profile = mime_off::inspect(stream, true, true);
	inspected = !stream.bad() && profile.header_complete && profile.append_traits(text_traits);
	observed = &text_traits;
    } else if (assetimport_text_type(mime_type)) {
	std::ifstream stream(filename, std::ios::binary);
	if (!stream.is_open()) return 0;
	if (mime_raw::is_type(mime_type)) {
	    auto profile = mime_raw::inspect(stream, true);
	    inspected = !stream.bad() && profile.type == mime_type && profile.append_traits(text_traits);
	} else if (mime_nff::is_type(mime_type)) {
	    auto profile = mime_nff::inspect(stream, true);
	    inspected = !stream.bad() && profile.type == mime_type && profile.append_traits(text_traits);
	} else {
	    auto profile = mime_skeletal::inspect(stream, true);
	    inspected = !stream.bad() && profile.type == mime_type;
	    for (const auto &value : profile.traits())
		if (!bu_mime_traits_add(&text_traits, value.first.c_str(), value.second.c_str())) inspected = false;
	}
	observed = &text_traits;
    } else {
	/* Include an IFF form, chunk header and LXO revision words. Full
	 * validation can seek past chunks outside the inspection prefix. */
	const size_t header_size = 7 * sizeof(uint32_t);
	unsigned char header[header_size];
	size_t size = assetimport_profile_header(*format, filename, header, sizeof(header));
	if (!size) return 0;
	inspected = bu_mime_detect(gcv_mime_registry(context), NULL, header, size, 0, &result) != BU_MIME_ERROR;
	for (size_t i = 0; result && i < result->count; ++i)
	    if (BU_STR_EQUAL(result->candidates[i].name, mime_type)) observed = &result->candidates[i].traits;
    }
    bool valid = false;
    if (inspected && observed && gcv_probe_file_traits_match(context, filename.c_str(), mime_type, observed, requested))
	for (const auto &profile : supported) {
	    bu_mime_traits values = BU_MIME_TRAITS_INIT_ZERO;
	    bool built = true;
	    for (const auto &trait : profile)
		if (!bu_mime_traits_add(&values, trait.first.c_str(), trait.second.c_str())) built = false;
	    for (const auto &set : assetimport_import_sets(format->mName, mime_type)) {
		std::vector<const char *> members;
		for (const auto &member : set.second) members.push_back(member.c_str());
		if (!bu_mime_traits_add_set(&values, set.first.c_str(), members.data(), members.size())) built = false;
	    }
	    const bu_mime_capability capability = BU_MIME_CAPABILITY(mime_type, values.items, values.count, 0);
	    valid = built && bu_mime_capability_match(&capability, observed, BU_MIME_INPUT);
	    bu_mime_traits_free(&values);
	    if (valid || !built) break;
	}
    if (valid && assetimport_text_type(mime_type)) {
	bu_vls extension = BU_VLS_INIT_ZERO;
	bu_path_component(&extension, filename.c_str(), BU_PATH_EXT);
	if (mime_raw::is_type(mime_type))
	    valid = !bu_strcasecmp(bu_vls_cstr(&extension), "raw");
	else if (mime_nff::is_type(mime_type))
	    valid = !bu_strcasecmp(bu_vls_cstr(&extension), "nff") || !bu_strcasecmp(bu_vls_cstr(&extension), "enff");
	else if (BU_STR_EQUAL(mime_type, "model/vnd.idsoftware.md5"))
	    valid = BU_STR_EQUAL(bu_vls_cstr(&extension), "md5mesh");
	else valid = !bu_strcasecmp(bu_vls_cstr(&extension), "smd") || !bu_strcasecmp(bu_vls_cstr(&extension), "vta");
	bu_vls_free(&extension);
	if (!valid) bu_log("SDK reader requires a native filename suffix for %s\n", mime_type);
    }
    if (valid && BU_STR_EQUAL(format->mName, "Ogre3D Mesh Importer") && !assetimport_ogre_filename(filename, mime_type)) {
	bu_log("SDK Ogre reader requires a native filename suffix for %s\n", mime_type);
	valid = false;
    }
    if (valid && BU_STR_EQUAL(mime_type, "model/vnd.blitzresearch.b3d") && !assetimport_zip::suffix(filename, ".b3d")) {
	bu_log("SDK B3D reader requires a .b3d filename suffix\n");
	valid = false;
    }
    if (!valid) {
	bu_vls message = BU_VLS_INIT_ZERO;
	bu_mime_traits_describe(observed, &message);
	bu_log("SDK cannot convert the declared %s profile:%s\n", mime_type, bu_vls_cstr(&message));
	bu_vls_free(&message);
    }
    bu_mime_result_free(result);
    bu_mime_traits_free(&text_traits);
    return valid;
}

static int
convert_input(assetimport_read_state_t* pstate, const char *mime_type, struct gcv_context *context)
{
    /* Do not enter an archive path whose member behavior we cannot yet
     * describe as a conversion capability, including legacy execution. */
    if (assetimport_zip::suffix(pstate->input_file, ".ifczip")) {
	bu_log("SDK IFCZIP conversion is unavailable; extract the IFC document first\n");
	return 0;
    }
    BU_LIST_INIT(&pstate->all.l);
    Assimp::Importer importer;
    const aiImporterDesc *profiled_format = NULL;
    if (mime_type) {
	/* Validate declared layouts before conversion. The same driver-specific
	 * check is also applied if the SDK ultimately chooses another loader. */
	for (size_t i = 0; i < importer.GetImporterCount() && !profiled_format; ++i) {
	    const auto *format = importer.GetImporterInfo(i);
	    if (!format) continue;
	    /* The SDK chooses separate glTF 1/2 loaders for the same MIME types.
	     * Validate the actual loader after decoding, before publication. */
	    if (assetimport_gltf_importer(format->mName)) continue;
	    for (const auto &extension : gcv_mime_catalog::split(format->mFileExtensions))
		for (const char *type : assetimport_import_identities(format->mName, extension))
		    if (BU_STR_EQUAL(type, mime_type) && !assetimport_import_profiles(format->mName, mime_type).empty()) profiled_format = format;
	}
	if (profiled_format && !assetimport_input_profile(profiled_format, pstate->input_file, mime_type, context)) return 0;
    }
    /* have importer remove points and lines as we can't do anything with them */
    if (mime_type && BU_STR_EQUAL(mime_type, "model/vnd.idsoftware.md5"))
	importer.SetPropertyBool(AI_CONFIG_IMPORT_MD5_NO_ANIM_AUTOLOAD, true);
    /* Generated skeleton previews are not geometry supplied by the input. */
    importer.SetPropertyBool(AI_CONFIG_IMPORT_NO_SKELETON_MESHES, true);
    importer.SetPropertyInteger(AI_CONFIG_PP_SBP_REMOVE, aiPrimitiveType_LINE | aiPrimitiveType_POINT);
    /* we are taking one of the postprocessing presets to have
     * max quality with reasonable render times. But, we must keep 
     * seemingly redundant materials as we use the names for BRLCAD shaders
     */
    unsigned int import_flags = aiProcessPreset_TargetRealtime_MaxQuality & ~aiProcess_RemoveRedundantMaterials;
    pstate->scene = importer.ReadFile(pstate->input_file, import_flags);
    if (pstate->scene) {
	const aiImporterDesc *format = assetimport_loaded_format(importer);
	const char *loaded_type = assetimport_loaded_type(format, pstate->input_file, context);
	if (mime_type && (!loaded_type || !BU_STR_EQUAL(loaded_type, mime_type))) {
	    bu_log("SDK input format %s does not match selected MIME type %s\n",
		loaded_type ? loaded_type : "unknown", mime_type);
	    return 0;
	}
	if ((format != profiled_format || assetimport_zip::is_zae(loaded_type) || assetimport_zip::is_3mf(loaded_type) ||
	    (loaded_type && BU_STR_EQUAL(loaded_type, mime_fbx::type)) ||
	    (loaded_type && BU_STR_EQUAL(loaded_type, mime_opengex::type)) ||
	    (loaded_type && BU_STR_EQUAL(loaded_type, mime_obj::type)) ||
	    (loaded_type && BU_STR_EQUAL(loaded_type, "application/vnd.idsoftware.pk3+zip"))) &&
	    !assetimport_input_profile(format, pstate->input_file, loaded_type, context, pstate->scene)) return 0;
    }

    /* The supplemental XML path can succeed for NURBS-only X3D files even
     * when the SDK produces no scene. Validate its root before publishing. */
    pugi::xml_document x3d;
    pugi::xml_node x3d_root;
    if (!mime_type || BU_STR_EQUAL(mime_type, "model/x3d+xml")) {
	if (x3d.load_file(pstate->input_file.c_str())) {
	    pugi::xml_node root = x3d.document_element();
	    if (root && x3d_name_is(root, "X3D")) x3d_root = root;
	}
	if (mime_type && !x3d_root) {
	    bu_log("Selected XML X3D input has no X3D document root\n");
	    return 0;
	}
    }
    if (!pstate->scene && !x3d_root) {
	bu_log("ERROR: bad scene conversion: %s\n", importer.GetErrorString());
	return 0;
    }
    if (mk_id_units(pstate->fd_out, "Conversion using Asset Importer Library (assetimport)", "mm"))
	return 0;
    if (x3d_root) convert_x3d_nurbs(pstate, x3d_root);
    if (!pstate->scene) {
	if (pstate->converted_nurbs) {
	    mk_lcomb(pstate->fd_out, "all.g", &pstate->all, 0, (char *)NULL, (char *)NULL, (unsigned char *)NULL, 0);
	    bu_log("Converted %d X3D NURBS objects\n", pstate->converted_nurbs);
	    return 1;
	}
	bu_log("ERROR: bad scene conversion\n");
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

    return (pstate->converted || pstate->converted_nurbs);
}

static void
assetimport_read_create_opts(struct bu_opt_desc **options_desc, void **dest_options_data)
{
    assetimport_read_options_t* options_data;

    BU_ALLOC(options_data, assetimport_read_options_t);
    *dest_options_data = options_data;
    *options_desc = (struct bu_opt_desc *)bu_malloc(6 * sizeof(struct bu_opt_desc), "options_desc");

    options_data->starting_id = 1000;
    options_data->const_id = 0;
    options_data->mat_code = 0;
    options_data->verbose = 0;
    options_data->format = NULL;

    BU_OPT((*options_desc)[0], NULL, "starting-ident", "number", bu_opt_int, options_data, "specify the starting ident for the regions created");
    BU_OPT((*options_desc)[1], NULL, "constant-ident", NULL, NULL, &options_data->const_id, "specify that the starting ident should remain constant");
    BU_OPT((*options_desc)[2], NULL, "material", "number", bu_opt_int, &options_data->mat_code, "specify the material code that will be assigned to created regions");
    BU_OPT((*options_desc)[3], "v", "verbose", NULL, NULL, &options_data->verbose, "specify for verbose output");
    BU_OPT((*options_desc)[4], NULL, "format", "string", bu_opt_str, &options_data->format, "Assert an input MIME type or file extension; must agree with the selected type");
    BU_OPT_NULL((*options_desc)[5]);
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

    const char *mime_type = gcv_target_mime(context);
    const char *format_option = state.assetimport_read_options->format;
    struct bu_vls asserted_type = BU_VLS_INIT_ZERO;
    struct bu_vls messages = BU_VLS_INIT_ZERO;
    bool valid = true;
    if (!mime_type || !BU_STR_EMPTY(format_option)) {
	/* Older callers may pass an extension with a leading dot. */
	if (format_option && format_option[0] == '.') ++format_option;
	valid = gcv_resolve_variant(context, GCV_FILTER_READ, source_path, format_option,
	    gcv_target_request_traits(context), "Assetimport Reader", &asserted_type, NULL, &messages) != NULL;
	if (valid && mime_type && !BU_STR_EQUAL(mime_type, bu_vls_cstr(&asserted_type)))
	    valid = false;
	if (!valid)
	    bu_log("Cannot select SDK input%s%s\n%s", BU_STR_EMPTY(format_option) ? "" : " --format ",
		BU_STR_EMPTY(format_option) ? "" : format_option, bu_vls_cstr(&messages));
	else
	    mime_type = bu_vls_cstr(&asserted_type);
    }
    int status = valid ? convert_input(&state, mime_type, context) : 0;
    bu_vls_free(&asserted_type);
    bu_vls_free(&messages);
    return status;
}


/* filter setup */
extern "C"
{
static const struct gcv_filter gcv_conv_assetimport_read = {
    "Assetimport Reader", GCV_FILTER_READ, BU_MIME_MODEL_VND_ASSETIMPORT, NULL,
    assetimport_read_create_opts, assetimport_read_free_opts, assetimport_read};

extern const struct gcv_filter gcv_conv_assetimport_write;
static const struct gcv_filter *const filters[] = {&gcv_conv_assetimport_read, &gcv_conv_assetimport_write, NULL};

const struct gcv_plugin gcv_plugin_info_s = {filters};

COMPILER_DLLEXPORT const struct gcv_plugin *
gcv_plugin_info()
{
    return &gcv_plugin_info_s; }

COMPILER_DLLEXPORT const struct gcv_plugin_v2 *
gcv_plugin_info_v2()
{
    static const gcv_mime_catalog catalog = []() {
	gcv_mime_catalog result;
	for (size_t i = 0; i < aiGetImportFormatCount(); ++i) {
	    const aiImporterDesc *format = aiGetImportFormatDescription(i);
	    if (!format) continue;
	    for (const auto &extension : gcv_mime_catalog::split(format->mFileExtensions)) {
		for (const char *name : assetimport_import_identities(format->mName, extension)) {
		    if (!assetimport_geometry_input(name)) {
			result.add(name, format->mName, {extension}, false, false);
			continue;
		    }
		    auto profiles = assetimport_import_profiles(format->mName, name);
		    if (profiles.empty()) result.add(name, format->mName, {extension}, true, false);
		    for (const auto &traits : profiles)
			result.add(name, format->mName, {extension}, true, false, traits, BU_MIME_DEFER_INPUT_VALIDATION,
			    assetimport_import_sets(format->mName, name));
		}
	    }
	}
	for (size_t i = 0; i < aiGetExportFormatCount(); ++i) {
	    const aiExportFormatDesc *format = aiGetExportFormatDescription(i);
	    if (!format) continue;
	    const assetimport_export_format *identity = assetimport_export_identity(format->id);
	    if (identity)
		for (const auto &traits : assetimport_export_profiles(*identity))
		    result.add(identity->mime, format->description, {format->fileExtension}, false, true, traits);
	    aiReleaseExportFormatDescription(format);
	}
	result.finish(&gcv_conv_assetimport_read, &gcv_conv_assetimport_write, assetimport_probe, assetimport_inspect);
	return result;
    }();
    return catalog.plugin();
}
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
