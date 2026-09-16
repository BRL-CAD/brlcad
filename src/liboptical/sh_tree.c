/*                       S H _ T R E E . C
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
/** @file liboptical/sh_tree.c
 *
 * Procedural tree foliage shader.  This shader treats the shaded region as a
 * sparse leaf volume: rays entering the proxy canopy march through a
 * deterministic grid of procedural leaf cards and shade the closest card hit.
 */

#include "common.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

#include "vmath.h"
#include "bn.h"
#include "raytrace.h"
#include "optical.h"

#define tree_MAGIC 0x7e3e
#define CK_tree_SP(_p) BU_CKMAG(_p, tree_MAGIC, "tree_specific")
#define TREE_O(m) bu_offsetof(struct tree_specific, m)
#define TREE_MAX_BRANCHES 48
#define TREE_SHADER_BRANCHES 48
#define TREE_BRANCH_O(_i) (bu_offsetof(struct tree_specific, branch) + sizeof(struct tree_branch_path) * (_i))
#define TREE_STYLE_BROADLEAF 0
#define TREE_STYLE_NEEDLE 1
#define TREE_STYLE_PALM 2
#define TREE_STYLE_SHRUB 3

struct tree_branch_path {
    point_t a;
    point_t m;
    point_t b;
};

struct tree_specific {
    uint32_t magic;
    point_t base;
    point_t vein;
    point_t mottle;
    double cell;
    double density;
    double leaf_length;
    double leaf_width;
    double jitter;
    double normal_bias;
    point_t origin;
    double height;
    double spread;
    double crown_base;
    double branch_bias;
    double branch_radius;
    double edge_density;
    double layer_strength;
    double mottle_strength;
    double vein_strength;
    double noise_lacunarity;
    double noise_h_val;
    double noise_octaves;
    double noise_size;
    int seed;
    int foliage_style;
    int max_leaves_per_cell;
    int branch_count;
    int branch_data;
    struct tree_branch_path branch[TREE_MAX_BRANCHES];
    mat_t m_to_sh;
    mat_t sh_to_m;
    mat_t noise_xform;
};

struct tree_leaf_hit {
    int hit;
    fastf_t dist;
    point_t point_sh;
    vect_t normal_sh;
    fastf_t u;
    fastf_t v;
};

static int tree_setup(register struct region *rp, struct bu_vls *matparm, void **dpp, const struct mfuncs *mfp, struct rt_i *rtip);
static int tree_render(struct application *ap, const struct partition *pp, struct shadework *swp, void *dp);
static void tree_print(register struct region *rp, void *dp);
static void tree_free(void *cp);
static void tree_color_fix(const struct bu_structparse *sdp, const char *name, void *base, const char *value, void *data);

static const struct tree_specific tree_defaults = {
    tree_MAGIC,
    {0.188, 0.463, 0.243},
    {0.420, 0.650, 0.270},
    {0.090, 0.280, 0.110},
    170.0,
    0.72,
    90.0,
    35.0,
    0.88,
    0.40,
    {0.0, 0.0, 0.0},
    3000.0,
    900.0,
    0.36,
    0.72,
    190.0,
    0.36,
    0.62,
    0.22,
    0.28,
    2.1753974,
    1.0,
    4.0,
    320.0,
    1,
    TREE_STYLE_BROADLEAF,
    4,
    18,
    0,
    {{{0.0, 0.0, 0.0}, {0.0, 0.0, 0.0}, {0.0, 0.0, 0.0}}},
    MAT_INIT_IDN,
    MAT_INIT_IDN,
    MAT_INIT_IDN
};

static struct bu_structparse tree_shader_parse[] = {
    {"%f", 3, "base", TREE_O(base), tree_color_fix, NULL, NULL},
    {"%f", 3, "rgb", TREE_O(base), tree_color_fix, NULL, NULL},
    {"%f", 3, "vein", TREE_O(vein), tree_color_fix, NULL, NULL},
    {"%f", 3, "mottle", TREE_O(mottle), tree_color_fix, NULL, NULL},
    {"%g", 1, "cell", TREE_O(cell), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%g", 1, "density", TREE_O(density), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%g", 1, "length", TREE_O(leaf_length), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%g", 1, "width", TREE_O(leaf_width), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%g", 1, "jitter", TREE_O(jitter), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%g", 1, "normal_bias", TREE_O(normal_bias), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%f", 3, "origin", TREE_O(origin), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%g", 1, "height", TREE_O(height), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%g", 1, "spread", TREE_O(spread), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%g", 1, "crown_base", TREE_O(crown_base), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%g", 1, "branch_bias", TREE_O(branch_bias), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%g", 1, "branch_radius", TREE_O(branch_radius), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%g", 1, "edge_density", TREE_O(edge_density), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%g", 1, "layer_strength", TREE_O(layer_strength), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%g", 1, "mottle_strength", TREE_O(mottle_strength), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%g", 1, "vein_strength", TREE_O(vein_strength), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%g", 1, "lacunarity", TREE_O(noise_lacunarity), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%g", 1, "H", TREE_O(noise_h_val), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%g", 1, "octaves", TREE_O(noise_octaves), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%g", 1, "size", TREE_O(noise_size), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%d", 1, "seed", TREE_O(seed), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%d", 1, "style", TREE_O(foliage_style), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%d", 1, "leaves", TREE_O(max_leaves_per_cell), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%d", 1, "branches", TREE_O(branch_count), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%d", 1, "branch_data", TREE_O(branch_data), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%f", 9, "b0", TREE_BRANCH_O(0), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%f", 9, "b1", TREE_BRANCH_O(1), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%f", 9, "b2", TREE_BRANCH_O(2), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%f", 9, "b3", TREE_BRANCH_O(3), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%f", 9, "b4", TREE_BRANCH_O(4), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%f", 9, "b5", TREE_BRANCH_O(5), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%f", 9, "b6", TREE_BRANCH_O(6), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%f", 9, "b7", TREE_BRANCH_O(7), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%f", 9, "b8", TREE_BRANCH_O(8), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%f", 9, "b9", TREE_BRANCH_O(9), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%f", 9, "b10", TREE_BRANCH_O(10), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%f", 9, "b11", TREE_BRANCH_O(11), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%f", 9, "b12", TREE_BRANCH_O(12), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%f", 9, "b13", TREE_BRANCH_O(13), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%f", 9, "b14", TREE_BRANCH_O(14), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%f", 9, "b15", TREE_BRANCH_O(15), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%f", 9, "b16", TREE_BRANCH_O(16), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%f", 9, "b17", TREE_BRANCH_O(17), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%f", 9, "b18", TREE_BRANCH_O(18), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%f", 9, "b19", TREE_BRANCH_O(19), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%f", 9, "b20", TREE_BRANCH_O(20), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%f", 9, "b21", TREE_BRANCH_O(21), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%f", 9, "b22", TREE_BRANCH_O(22), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%f", 9, "b23", TREE_BRANCH_O(23), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%f", 9, "b24", TREE_BRANCH_O(24), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%f", 9, "b25", TREE_BRANCH_O(25), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%f", 9, "b26", TREE_BRANCH_O(26), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%f", 9, "b27", TREE_BRANCH_O(27), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%f", 9, "b28", TREE_BRANCH_O(28), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%f", 9, "b29", TREE_BRANCH_O(29), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%f", 9, "b30", TREE_BRANCH_O(30), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%f", 9, "b31", TREE_BRANCH_O(31), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%f", 9, "b32", TREE_BRANCH_O(32), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%f", 9, "b33", TREE_BRANCH_O(33), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%f", 9, "b34", TREE_BRANCH_O(34), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%f", 9, "b35", TREE_BRANCH_O(35), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%f", 9, "b36", TREE_BRANCH_O(36), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%f", 9, "b37", TREE_BRANCH_O(37), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%f", 9, "b38", TREE_BRANCH_O(38), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%f", 9, "b39", TREE_BRANCH_O(39), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%f", 9, "b40", TREE_BRANCH_O(40), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%f", 9, "b41", TREE_BRANCH_O(41), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%f", 9, "b42", TREE_BRANCH_O(42), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%f", 9, "b43", TREE_BRANCH_O(43), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%f", 9, "b44", TREE_BRANCH_O(44), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%f", 9, "b45", TREE_BRANCH_O(45), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%f", 9, "b46", TREE_BRANCH_O(46), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%f", 9, "b47", TREE_BRANCH_O(47), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"", 0, (char *)0, 0, BU_STRUCTPARSE_FUNC_NULL, NULL, NULL}
};

struct mfuncs tree_mfuncs[] = {
    {MF_MAGIC, "tree", 0, MFI_HIT | MFI_NORMAL, MFF_PROC, tree_setup, tree_render, tree_print, tree_free},
    {0, (char *)0, 0, 0, 0, 0, 0, 0, 0}
};

static double
tree_clamp(double val, double lo, double hi)
{
    if (val < lo)
	return lo;
    if (val > hi)
	return hi;
    return val;
}

static uint32_t
tree_hash(uint32_t x)
{
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

static uint32_t
tree_cell_hash(long ix, long iy, long iz, int seed, int leaf)
{
    uint32_t h = (uint32_t)seed;
    h ^= tree_hash((uint32_t)ix * 0x8da6b343U);
    h ^= tree_hash((uint32_t)iy * 0xd8163841U);
    h ^= tree_hash((uint32_t)iz * 0xcb1ab31fU);
    h ^= tree_hash((uint32_t)leaf * 0x165667b1U);
    return tree_hash(h);
}

static double
tree_unit(uint32_t *state)
{
    *state = tree_hash(*state + 0x9e3779b9U);
    return (double)(*state & 0x00ffffffU) / (double)0x01000000U;
}

static void
tree_color_fix(const struct bu_structparse *sdp, const char *UNUSED(name), void *base, const char *UNUSED(value), void *UNUSED(data))
{
    double *p = (double *)((char *)base + sdp->sp_offset);
    int normalized = 1;

    for (size_t i = 0; i < sdp->sp_count; i++) {
	if (p[i] < 0.0 || p[i] > 1.0) {
	    normalized = 0;
	    break;
	}
    }
    if (!normalized) {
	for (size_t i = 0; i < sdp->sp_count; i++)
	    p[i] /= 255.0;
    }
    for (size_t i = 0; i < sdp->sp_count; i++)
	p[i] = tree_clamp(p[i], 0.0, 1.0);
}

static void
tree_build_branches(struct tree_specific *tree_sp)
{
    for (int i = 0; i < tree_sp->branch_count; i++) {
	uint32_t state = tree_hash((uint32_t)tree_sp->seed ^ ((uint32_t)i * 0x45d9f3bU));
	double layer = (double)i / (double)tree_sp->branch_count;
	double start_t = tree_sp->crown_base + (0.50 - tree_sp->crown_base) * (0.22 * tree_unit(&state));
	double height_span = 0.48 * pow(layer, 0.82) + 0.06 * tree_unit(&state);
	double az = 2.0 * M_PI * (layer * 5.236067977 + 0.17 * tree_unit(&state));
	double reach = tree_sp->spread * (0.46 + 0.58 * tree_unit(&state)) * (1.08 - 0.38 * height_span);
	double rise = tree_sp->height * (0.08 + 0.22 * tree_unit(&state)) * (1.0 - 0.30 * layer);
	double sag = tree_sp->height * (0.08 + 0.08 * tree_unit(&state)) * tree_clamp(layer - 0.25, 0.0, 1.0);
	point_t *a = &tree_sp->branch[i].a;
	point_t *m = &tree_sp->branch[i].m;
	point_t *b = &tree_sp->branch[i].b;

	start_t = tree_clamp(start_t + height_span, tree_sp->crown_base * 0.82, 0.88);
	VSET(*a, tree_sp->origin[X], tree_sp->origin[Y], tree_sp->origin[Z] + tree_sp->height * start_t);
	VSET(*b,
		(*a)[X] + cos(az) * reach,
		(*a)[Y] + sin(az) * reach,
		(*a)[Z] + rise - sag);
	VSET(*m,
		(*a)[X] + cos(az) * reach * 0.52,
		(*a)[Y] + sin(az) * reach * 0.52,
		(*a)[Z] + ((*b)[Z] - (*a)[Z]) * 0.60 + tree_sp->height * 0.045);
    }
}

static int
tree_setup(register struct region *rp, struct bu_vls *matparm, void **dpp, const struct mfuncs *UNUSED(mfp), struct rt_i *rtip)
{
    struct tree_specific *tree_sp;
    mat_t scale;

    RT_CHECK_RTI(rtip);
    BU_CK_VLS(matparm);
    RT_CK_REGION(rp);

    BU_GET(tree_sp, struct tree_specific);
    *dpp = tree_sp;
    memcpy(tree_sp, &tree_defaults, sizeof(struct tree_specific));

    if (rp->reg_mater.ma_color_valid)
	VMOVE(tree_sp->base, rp->reg_mater.ma_color);

    if (bu_struct_parse(matparm, tree_shader_parse, (char *)tree_sp, NULL) < 0)
	return -1;

    if (tree_sp->cell <= SMALL_FASTF)
	tree_sp->cell = tree_defaults.cell;
    if (tree_sp->leaf_length <= SMALL_FASTF)
	tree_sp->leaf_length = tree_defaults.leaf_length;
    if (tree_sp->leaf_width <= SMALL_FASTF)
	tree_sp->leaf_width = tree_defaults.leaf_width;
    if (tree_sp->noise_size <= SMALL_FASTF)
	tree_sp->noise_size = tree_defaults.noise_size;
    if (tree_sp->height <= SMALL_FASTF)
	tree_sp->height = tree_defaults.height;
    if (tree_sp->spread <= SMALL_FASTF)
	tree_sp->spread = tree_defaults.spread;
    if (tree_sp->branch_radius <= SMALL_FASTF)
	tree_sp->branch_radius = tree_defaults.branch_radius;
    tree_sp->density = tree_clamp(tree_sp->density, 0.0, 1.0);
    tree_sp->jitter = tree_clamp(tree_sp->jitter, 0.0, 1.0);
    tree_sp->normal_bias = tree_clamp(tree_sp->normal_bias, 0.0, 1.0);
    tree_sp->crown_base = tree_clamp(tree_sp->crown_base, 0.0, 0.95);
    tree_sp->branch_bias = tree_clamp(tree_sp->branch_bias, 0.0, 1.0);
    tree_sp->edge_density = tree_clamp(tree_sp->edge_density, 0.0, 1.0);
    tree_sp->layer_strength = tree_clamp(tree_sp->layer_strength, 0.0, 1.0);
    tree_sp->mottle_strength = tree_clamp(tree_sp->mottle_strength, 0.0, 1.0);
    tree_sp->vein_strength = tree_clamp(tree_sp->vein_strength, 0.0, 1.0);
    if (tree_sp->foliage_style < TREE_STYLE_BROADLEAF || tree_sp->foliage_style > TREE_STYLE_SHRUB)
	tree_sp->foliage_style = TREE_STYLE_BROADLEAF;
    if (tree_sp->max_leaves_per_cell < 1)
	tree_sp->max_leaves_per_cell = 1;
    if (tree_sp->max_leaves_per_cell > 8)
	tree_sp->max_leaves_per_cell = 8;
    if (tree_sp->branch_count < 1)
	tree_sp->branch_count = 1;
    if (tree_sp->branch_count > 48)
	tree_sp->branch_count = 48;
    if (tree_sp->branch_data) {
	if (tree_sp->branch_count > TREE_SHADER_BRANCHES)
	    tree_sp->branch_count = TREE_SHADER_BRANCHES;
    } else {
	tree_build_branches(tree_sp);
    }

    db_region_mat(tree_sp->m_to_sh, rtip->rti_dbip, rp->reg_name);
    bn_mat_inv(tree_sp->sh_to_m, tree_sp->m_to_sh);

    MAT_IDN(scale);
    scale[0] = scale[5] = scale[10] = 1.0 / tree_sp->noise_size;
    bn_mat_mul(tree_sp->noise_xform, scale, tree_sp->m_to_sh);

    if (optical_debug&OPTICAL_DEBUG_SHADE)
	bu_struct_print(rp->reg_name, tree_shader_parse, (char *)tree_sp);

    return 1;
}

static double
tree_dist_to_segment(const point_t p, const point_t a, const point_t b, double *along)
{
    vect_t ab;
    vect_t ap;
    point_t q;
    double denom;
    double t;

    VSUB2(ab, b, a);
    VSUB2(ap, p, a);
    denom = MAGSQ(ab);
    if (denom <= SMALL_FASTF) {
	if (along)
	    *along = 0.0;
	return DIST_PNT_PNT(p, a);
    }

    t = VDOT(ap, ab) / denom;
    t = tree_clamp(t, 0.0, 1.0);
    if (along)
	*along = t;
    VJOIN1(q, a, t, ab);
    return DIST_PNT_PNT(p, q);
}

static double
tree_branch_score(const struct tree_specific *tree_sp, const point_t p)
{
    double score = 0.0;
    double zrel = (p[Z] - tree_sp->origin[Z]) / tree_sp->height;
    double dx = p[X] - tree_sp->origin[X];
    double dy = p[Y] - tree_sp->origin[Y];
    double radial = sqrt(dx * dx + dy * dy);
    double crown_top_taper;

    if (zrel < tree_sp->crown_base - 0.08 || zrel > 1.06)
	return 0.0;

    crown_top_taper = tree_clamp((1.06 - zrel) / 0.22, 0.0, 1.0);
    score = tree_sp->foliage_style == TREE_STYLE_NEEDLE ? 0.0 : 0.08 * crown_top_taper;

    for (int i = 0; i < tree_sp->branch_count; i++) {
	const struct tree_branch_path *branch = &tree_sp->branch[i];
	double along = 0.0;
	double dist1;
	double dist2;
	double dist;
	double influence;
	double terminal;
	double branch_score;

	dist1 = tree_dist_to_segment(p, branch->a, branch->m, &along);
	terminal = along * 0.5;
	dist2 = tree_dist_to_segment(p, branch->m, branch->b, &along);
	dist = dist1;
	if (dist2 < dist1) {
	    dist = dist2;
	    terminal = 0.5 + along * 0.5;
	}

	if (tree_sp->foliage_style == TREE_STYLE_NEEDLE)
	    influence = tree_sp->branch_radius * (0.58 + 0.98 * terminal);
	else
	    influence = tree_sp->branch_radius * (0.72 + 1.18 * terminal);
	branch_score = tree_clamp(1.0 - dist / influence, 0.0, 1.0);
	if (tree_sp->foliage_style == TREE_STYLE_NEEDLE)
	    branch_score *= 0.55 + 0.45 * pow(terminal, 0.58);
	else
	    branch_score *= 0.35 + 0.65 * pow(terminal, 0.72);
	score = fmax(score, branch_score);
    }

    if (radial < tree_sp->spread * 0.10 && zrel < 0.72)
	score *= tree_clamp(radial / (tree_sp->spread * 0.10), 0.25, 1.0);

    return tree_clamp(score, 0.0, 1.0);
}

static double
tree_edge_score(const struct tree_specific *tree_sp, const point_t p)
{
    double zrel = (p[Z] - tree_sp->origin[Z]) / tree_sp->height;
    double dx = p[X] - tree_sp->origin[X];
    double dy = p[Y] - tree_sp->origin[Y];
    double radial = sqrt(dx * dx + dy * dy);
    double crown_span = fmax(1.0 - tree_sp->crown_base, 0.05);
    double crown_t = tree_clamp((zrel - tree_sp->crown_base) / crown_span, 0.0, 1.0);
    double expected = tree_sp->spread * (0.34 + 0.62 * sin(M_PI * crown_t));
    double band;
    double edge;

    if (tree_sp->foliage_style == TREE_STYLE_NEEDLE)
	expected = tree_sp->spread * (0.92 * (1.0 - crown_t) + 0.08);
    else if (tree_sp->foliage_style == TREE_STYLE_PALM)
	expected = tree_sp->spread * (0.22 + 0.72 * tree_clamp(crown_t, 0.0, 0.72));
    else if (tree_sp->foliage_style == TREE_STYLE_SHRUB)
	expected = tree_sp->spread * (0.46 + 0.46 * sin(M_PI * tree_clamp(crown_t * 0.92, 0.0, 1.0)));

    band = expected * (tree_sp->foliage_style == TREE_STYLE_NEEDLE ? 0.34 : 0.52);
    edge = 1.0 - fabs(radial - expected) / fmax(band, tree_sp->branch_radius);

    return tree_clamp(edge, 0.0, 1.0);
}

static int
tree_leaf_intersect(const struct tree_specific *tree_sp, const struct xray *ray, long ix, long iy, long iz, struct tree_leaf_hit *best)
{
    point_t cell_min;
    int hits = 0;

    VSET(cell_min, (fastf_t)ix * tree_sp->cell, (fastf_t)iy * tree_sp->cell, (fastf_t)iz * tree_sp->cell);

    for (int i = 0; i < tree_sp->max_leaves_per_cell; i++) {
	uint32_t state = tree_cell_hash(ix, iy, iz, tree_sp->seed, i);
	point_t center;
	vect_t normal;
	vect_t axis_u;
	vect_t axis_v;
	fastf_t denom;
	fastf_t dist;
	vect_t rel;
	fastf_t lu;
	fastf_t lv;
	fastf_t half_len;
	fastf_t half_width;
	fastf_t profile;
	double branch_score;
	double edge_score;
	double accept;

	center[X] = cell_min[X] + (0.5 + (tree_unit(&state) - 0.5) * tree_sp->jitter) * tree_sp->cell;
	center[Y] = cell_min[Y] + (0.5 + (tree_unit(&state) - 0.5) * tree_sp->jitter) * tree_sp->cell;
	center[Z] = cell_min[Z] + (0.5 + (tree_unit(&state) - 0.5) * tree_sp->jitter) * tree_sp->cell;

	branch_score = tree_branch_score(tree_sp, center);
	edge_score = tree_edge_score(tree_sp, center);
	if (tree_sp->foliage_style == TREE_STYLE_NEEDLE) {
	    double zrel = tree_clamp((center[Z] - tree_sp->origin[Z]) / tree_sp->height, 0.0, 1.0);
	    double cone_fill = tree_clamp((1.02 - zrel) / 0.82, 0.10, 1.0);
	    accept = tree_sp->density * ((1.0 - tree_sp->branch_bias) * 0.12 + tree_sp->branch_bias * (0.88 * branch_score + 0.09 * edge_score)) * cone_fill;
	} else if (tree_sp->foliage_style == TREE_STYLE_PALM) {
	    accept = tree_sp->density * (0.08 + 0.86 * branch_score + 0.22 * edge_score);
	} else if (tree_sp->foliage_style == TREE_STYLE_SHRUB) {
	    accept = tree_sp->density * (0.26 + 0.58 * branch_score + 0.30 * edge_score);
	} else {
	    accept = tree_sp->density * ((1.0 - tree_sp->branch_bias) * 0.30 + tree_sp->branch_bias * branch_score);
	    accept += tree_sp->density * tree_sp->edge_density * edge_score * 0.28;
	}
	if (tree_unit(&state) > tree_clamp(accept, 0.0, 1.0))
	    continue;

	{
	    vect_t outward;
	    VSET(outward, center[X] - tree_sp->origin[X], center[Y] - tree_sp->origin[Y], 0.22 * tree_sp->spread);
	    if (MAGSQ(outward) <= SMALL_FASTF)
		VSET(outward, 0.0, 0.0, 1.0);
	    VUNITIZE(outward);
	    if (tree_sp->foliage_style == TREE_STYLE_NEEDLE) {
		VSET(normal,
			outward[X] * 0.64 + 0.24 * (tree_unit(&state) * 2.0 - 1.0),
			outward[Y] * 0.64 + 0.24 * (tree_unit(&state) * 2.0 - 1.0),
			0.18 + 0.28 * (tree_unit(&state) * 2.0 - 1.0));
	    } else if (tree_sp->foliage_style == TREE_STYLE_PALM) {
		VSET(normal,
			outward[X] * 0.30 + 0.18 * (tree_unit(&state) * 2.0 - 1.0),
			outward[Y] * 0.30 + 0.18 * (tree_unit(&state) * 2.0 - 1.0),
			0.76 + 0.18 * (tree_unit(&state) * 2.0 - 1.0));
	    } else {
		VSET(normal,
			outward[X] * (0.42 + 0.30 * edge_score) + 0.42 * (tree_unit(&state) * 2.0 - 1.0),
			outward[Y] * (0.42 + 0.30 * edge_score) + 0.42 * (tree_unit(&state) * 2.0 - 1.0),
			outward[Z] * 0.34 + 0.34 * (tree_unit(&state) * 2.0 - 1.0) + tree_sp->normal_bias);
	    }
	}
	if (MAGSQ(normal) <= SMALL_FASTF)
	    VSET(normal, 0.0, 0.0, 1.0);
	VUNITIZE(normal);

	{
	    vect_t from_origin;
	    VSET(from_origin, center[X] - tree_sp->origin[X], center[Y] - tree_sp->origin[Y], 0.0);
	    if (MAGSQ(from_origin) > SMALL_FASTF) {
		VUNITIZE(from_origin);
		VSET(axis_u,
			from_origin[X] + 0.35 * (tree_unit(&state) * 2.0 - 1.0),
			from_origin[Y] + 0.35 * (tree_unit(&state) * 2.0 - 1.0),
			0.22 * (tree_unit(&state) * 2.0 - 1.0));
	    } else {
		VSET(axis_u, tree_unit(&state) * 2.0 - 1.0, tree_unit(&state) * 2.0 - 1.0, 0.15 * (tree_unit(&state) * 2.0 - 1.0));
	    }
	}
	VCROSS(axis_v, normal, axis_u);
	if (MAGSQ(axis_v) <= SMALL_FASTF) {
	    VSET(axis_u, 1.0, 0.0, 0.0);
	    VCROSS(axis_v, normal, axis_u);
	}
	VUNITIZE(axis_v);
	VCROSS(axis_u, axis_v, normal);
	VUNITIZE(axis_u);

	denom = VDOT(ray->r_dir, normal);
	if (fabs(denom) <= 1.0e-7)
	    continue;
	VSUB2(rel, center, ray->r_pt);
	dist = VDOT(rel, normal) / denom;
	if (dist <= SMALL_FASTF)
	    continue;

	VJOIN1(rel, ray->r_pt, dist, ray->r_dir);
	VSUB2(rel, rel, center);
	lu = VDOT(rel, axis_u);
	lv = VDOT(rel, axis_v);

	if (tree_sp->foliage_style == TREE_STYLE_NEEDLE) {
	    half_len = tree_sp->leaf_length * (0.54 + 0.34 * tree_unit(&state));
	    half_width = tree_sp->leaf_width * (0.22 + 0.16 * tree_unit(&state));
	} else if (tree_sp->foliage_style == TREE_STYLE_PALM) {
	    half_len = tree_sp->leaf_length * (0.58 + 0.30 * tree_unit(&state));
	    half_width = tree_sp->leaf_width * (0.20 + 0.16 * tree_unit(&state));
	} else {
	    half_len = tree_sp->leaf_length * (0.36 + 0.22 * tree_unit(&state));
	    half_width = tree_sp->leaf_width * (0.34 + 0.20 * tree_unit(&state));
	}
	if (fabs(lu) > half_len)
	    continue;
	if (tree_sp->foliage_style == TREE_STYLE_NEEDLE)
	    profile = 0.48 + 0.52 * (1.0 - fabs(lu) / half_len);
	else if (tree_sp->foliage_style == TREE_STYLE_PALM)
	    profile = 1.0 - pow(fabs(lu) / half_len, 2.6);
	else
	    profile = 1.0 - pow(fabs(lu) / half_len, 1.7);
	if (profile <= 0.0 || fabs(lv) > half_width * profile)
	    continue;

	if (best->u < 24.0)
	    best->u += 1.0;
	if (dist < best->dist) {
	    best->hit = 1;
	    best->dist = dist;
	    VJOIN1(best->point_sh, ray->r_pt, dist, ray->r_dir);
	    VMOVE(best->normal_sh, normal);
	    if (VDOT(best->normal_sh, ray->r_dir) > 0.0)
		VREVERSE(best->normal_sh, best->normal_sh);
	    best->v = lv / (half_width * profile);
	}
	hits++;
    }

    return hits;
}

static void
tree_check_cell(const struct tree_specific *tree_sp, const struct xray *ray, point_t p, struct tree_leaf_hit *best)
{
    const double inv_cell = 1.0 / tree_sp->cell;
    const double fx = floor(p[X] * inv_cell);
    const double fy = floor(p[Y] * inv_cell);
    const double fz = floor(p[Z] * inv_cell);
    const long ix = (long)fx;
    const long iy = (long)fy;
    const long iz = (long)fz;

    for (long dz = -1; dz <= 1; dz++) {
	for (long dy = -1; dy <= 1; dy++) {
	    for (long dx = -1; dx <= 1; dx++)
		(void)tree_leaf_intersect(tree_sp, ray, ix + dx, iy + dy, iz + dz, best);
	}
    }
}

static int
tree_render(struct application *ap, const struct partition *pp, struct shadework *swp, void *dp)
{
    struct tree_specific *tree_sp = (struct tree_specific *)dp;
    struct xray ray_sh;
    struct tree_leaf_hit best;
    point_t in_pt;
    point_t out_pt;
    vect_t ray_delta;
    fastf_t in_dist;
    fastf_t out_dist;
    fastf_t march_step;
    fastf_t march_dist;

    RT_AP_CHECK(ap);
    RT_CHECK_PT(pp);
    CK_tree_SP(tree_sp);

    MAT4X3PNT(ray_sh.r_pt, tree_sp->m_to_sh, ap->a_ray.r_pt);
    MAT4X3VEC(ray_sh.r_dir, tree_sp->m_to_sh, ap->a_ray.r_dir);
    VUNITIZE(ray_sh.r_dir);

    MAT4X3PNT(in_pt, tree_sp->m_to_sh, swp->sw_hit.hit_point);
    VJOIN1(out_pt, ap->a_ray.r_pt, pp->pt_outhit->hit_dist, ap->a_ray.r_dir);
    MAT4X3PNT(out_pt, tree_sp->m_to_sh, out_pt);

    VSUB2(ray_delta, in_pt, ray_sh.r_pt);
    in_dist = MAGNITUDE(ray_delta);
    VSUB2(ray_delta, out_pt, ray_sh.r_pt);
    out_dist = MAGNITUDE(ray_delta);
    if (out_dist <= in_dist) {
	swp->sw_transmit = 1.0;
	VSETALL(swp->sw_basecolor, 1.0);
	(void)rr_render(ap, pp, swp);
	return 0;
    }

    best.hit = 0;
    best.dist = out_dist;
    VSETALL(best.point_sh, 0.0);
    VSET(best.normal_sh, 0.0, 0.0, 1.0);
    best.u = 0.0;
    best.v = 0.0;

    march_step = tree_sp->cell * 0.48;
    if (march_step < tree_sp->leaf_width)
	march_step = tree_sp->leaf_width;

    for (march_dist = in_dist; march_dist <= out_dist; march_dist += march_step) {
	point_t p;
	VJOIN1(p, ray_sh.r_pt, march_dist, ray_sh.r_dir);
	tree_check_cell(tree_sp, &ray_sh, p, &best);
    }

    if (!best.hit) {
	swp->sw_transmit = 1.0;
	VSETALL(swp->sw_basecolor, 1.0);
	(void)rr_render(ap, pp, swp);
	return 0;
    }

    {
	point_t hit_model;
	vect_t normal_model;
	vect_t to_hit;
	point_t npt;
	point_t color;
	double noise;
	double vein;
	double zrel;
	double depth_frac;
	double layers;
	double interior;
	double edge;
	double shade;

	MAT4X3PNT(hit_model, tree_sp->sh_to_m, best.point_sh);
	MAT4X3VEC(normal_model, tree_sp->sh_to_m, best.normal_sh);
	VUNITIZE(normal_model);
	if (VDOT(normal_model, ap->a_ray.r_dir) > 0.0)
	    VREVERSE(normal_model, normal_model);

	VSUB2(to_hit, hit_model, ap->a_ray.r_pt);
	swp->sw_hit.hit_dist = VDOT(to_hit, ap->a_ray.r_dir);
	if (swp->sw_hit.hit_dist < 0.0)
	    swp->sw_hit.hit_dist = 0.0;
	VMOVE(swp->sw_hit.hit_point, hit_model);
	VMOVE(swp->sw_hit.hit_normal, normal_model);
	swp->sw_inputs |= MFI_HIT | MFI_NORMAL;

	MAT4X3PNT(npt, tree_sp->noise_xform, hit_model);
	noise = 0.5 + 0.5 * bn_noise_fbm(npt, tree_sp->noise_h_val, tree_sp->noise_lacunarity, tree_sp->noise_octaves);
	noise = tree_clamp(noise, 0.0, 1.0);
	vein = pow(1.0 - tree_clamp(fabs(best.v), 0.0, 1.0), 9.0) * tree_sp->vein_strength;
	if (tree_sp->foliage_style == TREE_STYLE_NEEDLE)
	    vein *= 0.18;
	else if (tree_sp->foliage_style == TREE_STYLE_PALM)
	    vein *= 0.55;
	zrel = tree_clamp((hit_model[Z] - tree_sp->origin[Z]) / tree_sp->height, 0.0, 1.0);
	depth_frac = tree_clamp((best.dist - in_dist) / fmax(out_dist - in_dist, SMALL_FASTF), 0.0, 1.0);
	layers = tree_clamp(best.u / 12.0, 0.0, 1.0);
	interior = tree_clamp((1.0 - depth_frac) * 0.55 + layers * tree_sp->layer_strength, 0.0, 1.0);
	edge = tree_edge_score(tree_sp, best.point_sh);
	shade = 1.04 - 0.42 * interior + 0.12 * edge + 0.10 * zrel;
	shade = tree_clamp(shade, 0.48, 1.18);

	VMOVE(color, tree_sp->base);
	VBLEND2(color, 1.0 - tree_sp->mottle_strength * noise, color, tree_sp->mottle_strength * noise, tree_sp->mottle);
	VBLEND2(color, 1.0 - vein, color, vein, tree_sp->vein);
	VSCALE(color, color, shade);
	VMOVE(swp->sw_basecolor, color);
	VMOVE(swp->sw_color, color);
	swp->sw_transmit = 0.0;
    }

    return 1;
}

static void
tree_print(register struct region *rp, void *dp)
{
    bu_struct_print(rp->reg_name, tree_shader_parse, (char *)dp);
}

static void
tree_free(void *cp)
{
    BU_PUT(cp, struct tree_specific);
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
