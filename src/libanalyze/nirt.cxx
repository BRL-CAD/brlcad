/*                        N I R T . C X X
 * BRL-CAD
 *
 * Copyright (c) 2004-2018 United States Government as represented by
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
/** @file nirt.cxx
 *
 * Implementation of Natalie's Interactive Ray-Tracer (NIRT)
 * functionality.
 *
 */

/* BRL-CAD includes */
#include "common.h"

#include <algorithm>
#include <string>
#include <sstream>
#include <fstream>
#include <set>
#include <vector>
#include <limits>

extern "C" {
#include "bu/color.h"
#include "bu/cmd.h"
#include "bu/malloc.h"
#include "bu/path.h"
#include "bu/units.h"
#include "bu/str.h"
#include "bu/vls.h"
#include "analyze.h"
}

/* NIRT segment types */
#define NIRT_HIT_SEG      1    /**< @brief Ray segment representing a solid region */
#define NIRT_MISS_SEG     2    /**< @brief Ray segment representing a gap */
#define NIRT_AIR_SEG      3    /**< @brief Ray segment representing an air region */
#define NIRT_OVERLAP_SEG  4    /**< @brief Ray segment representing an overlap region */

#define SILENT_UNSET    0
#define SILENT_YES      1
#define SILENT_NO       -1

#define HORZ            0
#define VERT            1
#define DIST            2

#define DEBUG_INTERACT  0x001
#define DEBUG_SCRIPTS   0x002
#define DEBUG_MAT       0x004
#define DEBUG_BACKOUT   0x008
#define DEBUG_HITS      0x010
#define DEBUG_FMT        "\020\5HITS\4BACKOUT\3MAT\2SCRIPTS\1INTERACT"

#define OVLP_RESOLVE            0
#define OVLP_REBUILD_FASTGEN    1
#define OVLP_REBUILD_ALL        2
#define OVLP_RETAIN             3

#define NIRT_PRINTF_SPECIFIERS "difeEgGs"
#define NIRT_OUTPUT_TYPE_SPECIFIERS "rhpfmog"

static const char *ovals =
"x_orig,         FLOAT,         Ray origin X coordinate,"
"y_orig,         FLOAT,         Ray origin Y coordinate,"
"z_orig,         FLOAT,         Ray origin Z coordinate,"
"h,              FLOAT,         ,"
"v,              FLOAT,         ,"
"d_orig,         FLOAT,         ,"
"x_dir,          FNOUNIT,       Ray direction unit vector x component,"
"y_dir,          FNOUNIT,       Ray direction unit vector y component,"
"z_dir,          FNOUNIT,       Ray direction unit vector z component,"
"a,              FNOUNIT,       Azimuth,"
"e,              FNOUNIT,       Elevation,"
"x_in,           FLOAT,         ,"
"y_in,           FLOAT,         ,"
"z_in,           FLOAT,         ,"
"d_in,           FLOAT,         ,"
"x_out,          FLOAT,         ,"
"y_out,          FLOAT,         ,"
"z_out,          FLOAT,         ,"
"d_out,          FLOAT,         ,"
"los,            FLOAT,         ,"
"scaled_los,     FLOAT,         ,"
"path_name,      STRING,        ,"
"reg_name,       STRING,        ,"
"reg_id,         INT,           ,"
"obliq_in,       FNOUNIT,       ,"
"obliq_out,      FNOUNIT,       ,"
"nm_x_in,        FNOUNIT,       ,"
"nm_y_in,        FNOUNIT,       ,"
"nm_z_in,        FNOUNIT,       ,"
"nm_d_in,        FNOUNIT,       ,"
"nm_h_in,        FNOUNIT,       ,"
"nm_v_in,        FNOUNIT,       ,"
"nm_x_out,       FNOUNIT,       ,"
"nm_y_out,       FNOUNIT,       ,"
"nm_z_out,       FNOUNIT,       ,"
"nm_d_out,       FNOUNIT,       ,"
"nm_h_out,       FNOUNIT,       ,"
"nm_v_out,       FNOUNIT,       ,"
"ov_reg1_name,   STRING,        ,"
"ov_reg1_id,     INT,           ,"
"ov_reg2_name,   STRING,        ,"
"ov_reg2_id,     INT,           ,"
"ov_sol_in,      STRING,        ,"
"ov_sol_out,     STRING,        ,"
"ov_los,         FLOAT,         ,"
"ov_x_in,        FLOAT,         ,"
"ov_y_in,        FLOAT,         ,"
"ov_z_in,        FLOAT,         ,"
"ov_d_in,        FLOAT,         ,"
"ov_x_out,       FLOAT,         ,"
"ov_y_out,       FLOAT,         ,"
"ov_z_out,       FLOAT,         ,"
"ov_d_out,       FLOAT,         ,"
"surf_num_in,    INT,           ,"
"surf_num_out,   INT,           ,"
"claimant_count, INT,           ,"
"claimant_list,  STRING,        ,"
"claimant_listn, STRING,        ,"
"attributes,     STRING,        ,"
"x_gap_in,       FLOAT,         ,"
"y_gap_in,       FLOAT,         ,"
"z_gap_in,       FLOAT,         ,"
"gap_los,        FLOAT,         ,";


struct overlap {
    struct application *ap;
    struct partition *pp;
    struct region *reg1;
    struct region *reg2;
    fastf_t in_dist;
    fastf_t out_dist;
    point_t in_point;
    point_t out_point;
    struct overlap *forw;
    struct overlap *backw;
};
#define OVERLAP_NULL ((struct overlap *)0)

/* This record structure doesn't have to correspond exactly to the above list
 * of available values, but it needs to retain sufficient information to
 * support the ability to generate all of them upon request. */
struct nout_record {
    point_t orig;
    fastf_t h;
    fastf_t v;
    fastf_t d_orig;
    vect_t dir;
    fastf_t a;
    fastf_t e;
    point_t in;
    fastf_t d_in;
    point_t out;
    fastf_t d_out;
    fastf_t los;
    fastf_t scaled_los;
    struct bu_vls *path_name;
    struct bu_vls *reg_name;
    int reg_id;
    fastf_t obliq_in;
    fastf_t obliq_out;
    vect_t nm_in;
    fastf_t nm_d_in;
    fastf_t nm_h_in;
    fastf_t nm_v_in;
    vect_t nm_out;
    fastf_t nm_d_out;
    fastf_t nm_h_out;
    fastf_t nm_v_out;
    struct bu_vls *ov_reg1_name;
    int ov_reg1_id;
    struct bu_vls *ov_reg2_name;
    int ov_reg2_id;
    struct bu_vls *ov_sol_in;
    struct bu_vls *ov_sol_out;
    fastf_t ov_los;
    point_t ov_in;
    fastf_t ov_d_in;
    point_t ov_out;
    fastf_t ov_d_out;
    int surf_num_in;
    int surf_num_out;
    int claimant_count;
    struct bu_vls *claimant_list;
    struct bu_vls *claimant_listn; /* uses \n instead of space to separate claimants */
    struct bu_vls *attributes;
    point_t gap_in;
    fastf_t gap_los;
    struct overlap ovlp_list;
};



struct nirt_state {
    /* Output options */
    struct bu_color *hit_color;
    struct bu_color *miss_color;
    struct bu_color *overlap_color;
    int print_header;
    int print_ident_flag;
    int silent_flag;
    unsigned int rt_debug;
    unsigned int nirt_debug;

    /* Output */
    struct bu_vls *out;
    int out_accumulate;
    struct bu_vls *err;
    int err_accumulate;
    struct bu_list s_vlist; /* used by the segs vlblock */
    struct bn_vlblock *segs;
    //TODO - int segs_accumulate;
    int ret;  // return code to be returned by nirt_exec after execution

    /* Callbacks */
    nirt_hook_t h_state;   // any state change
    nirt_hook_t h_out;     // output changes
    nirt_hook_t h_err;     // err changes
    nirt_hook_t h_segs;    // segement list is changed
    nirt_hook_t h_objs;    // active list of objects in scene changes
    nirt_hook_t h_frmts;   // output formatting is changed
    nirt_hook_t h_view;    // the camera view is changed


    /* state variables */
    double base2local;
    double local2base;
    int backout;
    int overlap_claims;
    int use_air;
    std::set<std::string> attrs;        // active attributes
    std::vector<std::string> active_paths; // active paths for raytracer
    struct nout_record *vals;

    /* state alteration flags */
    bool b_state;   // updated for any state change
    bool b_segs;    // updated when segement list is changed
    bool b_objs;    // updated when active list of objects in scene changes
    bool b_frmts;   // updated when output formatting is changed
    bool b_view;    // updated when the camera view is changed

    /* internal containers for raytracing */
    struct application *ap;
    struct db_i *dbip;
    /* Note: Parallel structures are needed for operation w/ and w/o air */
    struct rt_i *rtip;
    struct resource *res;
    struct rt_i *rtip_air;
    struct resource *res_air;

    /* internal format specifier arrays */
    struct bu_attribute_value_set *val_types;
    struct bu_attribute_value_set *val_docs;
    std::vector<std::pair<std::string,std::string> > fmt_ray;
    std::vector<std::pair<std::string,std::string> > fmt_head;
    std::vector<std::pair<std::string,std::string> > fmt_part;
    std::vector<std::pair<std::string,std::string> > fmt_foot;
    std::vector<std::pair<std::string,std::string> > fmt_miss;
    std::vector<std::pair<std::string,std::string> > fmt_ovlp;
    std::vector<std::pair<std::string,std::string> > fmt_gap;

    void *u_data; // user data
};

/**************************
 * Internal functionality *
 **************************/

static void lout(struct nirt_state *nss, const char *fmt, ...) _BU_ATTR_PRINTF23;
static void lout(struct nirt_state *nss, const char *fmt, ...)
{
    va_list ap;
    if (nss->silent_flag != SILENT_YES && nss->h_out) {
	struct bu_vls *vls = nss->out;
	BU_CK_VLS(vls);
	va_start(ap, fmt);
	bu_vls_vprintf(vls, fmt, ap);
	(*nss->h_out)(nss, nss->u_data);
	bu_vls_trunc(vls, 0);
    }
    va_end(ap);
}

static void lerr(struct nirt_state *nss, const char *fmt, ...) _BU_ATTR_PRINTF23;
static void lerr(struct nirt_state *nss, const char *fmt, ...)
{
    va_list ap;
    if (nss->h_err){
	struct bu_vls *vls = nss->err;
	BU_CK_VLS(vls);
	va_start(ap, fmt);
	bu_vls_vprintf(vls, fmt, ap);
	(*nss->h_err)(nss, nss->u_data);
	bu_vls_trunc(vls, 0);
    }
    va_end(ap);
}

static void ldbg(struct nirt_state *nss, int flag, const char *fmt, ...)
{
    va_list ap;
    if (nss->silent_flag != SILENT_YES && (nss->nirt_debug & flag) && nss->h_out) {
	struct bu_vls *vls = nss->out;
	BU_CK_VLS(vls);
	va_start(ap, fmt);
	bu_vls_vprintf(vls, fmt, ap);
	(*nss->h_out)(nss, nss->u_data);
	bu_vls_trunc(vls, 0);
    }
    va_end(ap);
}


/********************************
 * Conversions and Calculations *
 ********************************/

void grid2targ(struct nirt_state *nss)
{
    vect_t grid;
    double ar = nss->vals->a * DEG2RAD;
    double er = nss->vals->e * DEG2RAD;
    VSET(grid, nss->vals->h, nss->vals->v, nss->vals->d_orig);
    nss->vals->orig[X] = - nss->vals->h * sin(ar) - nss->vals->v * cos(ar) * sin(er) + nss->vals->d_orig * cos(ar) * cos(er);
    nss->vals->orig[Y] =   nss->vals->h * cos(ar) - nss->vals->v * sin(ar) * sin(er) + nss->vals->d_orig * sin(ar) * cos(er);
    nss->vals->orig[Z] =   nss->vals->v * cos(er) + nss->vals->d_orig * sin(er);
}

void targ2grid(struct nirt_state *nss)
{
    vect_t target;
    double ar = nss->vals->a * DEG2RAD;
    double er = nss->vals->e * DEG2RAD;
    VMOVE(target, nss->vals->orig);
    nss->vals->h = - target[X] * sin(ar) + target[Y] * cos(ar);
    nss->vals->v = - target[X] * cos(ar) * sin(er) - target[Y] * sin(er) * sin(ar) + target[Z] * cos(er);
    nss->vals->d_orig =   target[X] * cos(er) * cos(ar) + target[Y] * cos(er) * sin(ar) + target[Z] * sin(er);
}

void dir2ae(struct nirt_state *nss)
{
    int zeroes = ZERO(nss->vals->dir[Y]) && ZERO(nss->vals->dir[X]);
    double square = sqrt(nss->vals->dir[X] * nss->vals->dir[X] + nss->vals->dir[Y] * nss->vals->dir[Y]);

    nss->vals->a = zeroes ? 0.0 : atan2 (-(nss->vals->dir[Y]), -(nss->vals->dir[X])) / DEG2RAD;
    nss->vals->e = atan2(-(nss->vals->dir[Z]), square) / DEG2RAD;
}

void ae2dir(struct nirt_state *nss)
{
    vect_t dir;
    double ar = nss->vals->a * DEG2RAD;
    double er = nss->vals->e * DEG2RAD;

    dir[X] = -cos(ar) * cos(er);
    dir[Y] = -sin(ar) * cos(er);
    dir[Z] = -sin(er);
    VUNITIZE(dir);
    VMOVE(nss->vals->dir, dir);
}

double backout(struct nirt_state *nss)
{
    double bov;
    point_t ray_point;
    vect_t diag, dvec, ray_dir, center_bsphere;
    fastf_t bsphere_diameter, dist_to_target, delta;

    if (!nss || !nss->backout) return 0.0;

    VMOVE(ray_point, nss->vals->orig);
    VMOVE(ray_dir, nss->vals->dir);

    VSUB2(diag, nss->ap->a_rt_i->mdl_max, nss->ap->a_rt_i->mdl_min);
    bsphere_diameter = MAGNITUDE(diag);

    /*
     * calculate the distance from a plane normal to the ray direction through the center of
     * the bounding sphere and a plane normal to the ray direction through the aim point.
     */
    VADD2SCALE(center_bsphere, nss->ap->a_rt_i->mdl_max, nss->ap->a_rt_i->mdl_min, 0.5);

    dist_to_target = DIST_PT_PT(center_bsphere, ray_point);

    VSUB2(dvec, ray_point, center_bsphere);
    VUNITIZE(dvec);
    delta = dist_to_target*VDOT(ray_dir, dvec);

    /*
     * this should put us about a bounding sphere radius in front of the bounding sphere
     */
    bov = bsphere_diameter + delta;

    return bov;
}

fastf_t
get_obliq(fastf_t *ray, fastf_t *normal)
{
    fastf_t cos_obl;
    fastf_t obliquity;

    cos_obl = fabs(VDOT(ray, normal) / (MAGNITUDE(normal) * MAGNITUDE(ray)));
    if (cos_obl < 1.001) {
	if (cos_obl > 1)
	    cos_obl = 1;
	obliquity = acos(cos_obl);
    } else {
	fflush(stdout);
	fprintf (stderr, "Error:  cos(obliquity) > 1 (%g)\n", cos_obl);
	obliquity = 0;
	bu_exit(1, NULL);
    }

    /* convert obliquity to degrees */
    obliquity = fabs(obliquity * RAD2DEG);
    if (obliquity > 90 && obliquity <= 180)
	obliquity = 180 - obliquity;
    else if (obliquity > 180 && obliquity <= 270)
	obliquity = obliquity - 180;
    else if (obliquity > 270 && obliquity <= 360)
	obliquity = 360 - obliquity;

    return obliquity;
}

/********************
 * Raytracing setup *
 ********************/

HIDDEN int
raytrace_gettrees(struct nirt_state *nss)
{
    int i = 0;
    int ocnt = 0;
    int acnt = 0;

    // Prepare C-style arrays for rt prep
    const char **objs = (const char **)bu_calloc(nss->active_paths.size() + 1, sizeof(char *), "objs");
    const char **attrs = (const char **)bu_calloc(nss->attrs.size() + 1, sizeof(char *), "attrs");
    for (size_t ui = 0; ui < nss->active_paths.size(); ui++) {
	const char *o = nss->active_paths[ui].c_str();
	objs[ocnt] = o;
	ocnt++;
    }
    objs[ocnt] = NULL;
    std::set<std::string>::iterator s_it;
    for (s_it = nss->attrs.begin(); s_it != nss->attrs.end(); s_it++) {
	const char *a = (*s_it).c_str();
	attrs[acnt] = a;
	acnt++;
    }
    attrs[acnt] = NULL;
    lout(nss, "Get trees...\n");

    i = rt_gettrees_and_attrs(nss->ap->a_rt_i, attrs, ocnt, objs, 1);
    bu_free(objs, "objs");
    bu_free(attrs, "objs");
    if (i) {
	lerr(nss, "rt_gettrees() failed\n");
	return -1;
    }

    return 0;
}

HIDDEN struct rt_i *
get_rtip(struct nirt_state *nss)
{
    if (!nss || nss->dbip == DBI_NULL) return NULL;

    if (nss->use_air) {
	if (nss->rtip_air == RTI_NULL) {
	    nss->rtip_air = rt_new_rti(nss->dbip); /* clones dbip, so we can operate on the copy */
	    nss->rtip_air->rti_dbip->dbi_fp = fopen(nss->dbip->dbi_filename, "rb"); /* get read-only fp */
	    if (nss->rtip_air->rti_dbip->dbi_fp == NULL) {
		rt_free_rti(nss->rtip_air);
		nss->rtip_air = RTI_NULL;
		return RTI_NULL;
	    }
	    nss->rtip_air->rti_dbip->dbi_read_only = 1;
	    rt_init_resource(nss->res_air, 0, nss->rtip_air);
	}
	return nss->rtip_air;
    }

    if (nss->rtip == RTI_NULL) {
	nss->rtip = rt_new_rti(nss->dbip); /* clones dbip, so we can operate on the copy */
	nss->rtip->rti_dbip->dbi_fp = fopen(nss->dbip->dbi_filename, "rb"); /* get read-only fp */
	if (nss->rtip->rti_dbip->dbi_fp == NULL) {
	    rt_free_rti(nss->rtip);
	    nss->rtip = RTI_NULL;
	    return RTI_NULL;
	}
	nss->rtip->rti_dbip->dbi_read_only = 1;
	rt_init_resource(nss->res, 0, nss->rtip);
    }
    return nss->rtip;
}

HIDDEN struct resource *
get_resource(struct nirt_state *nss)
{
    if (nss->use_air) return nss->res_air;
    return nss->res;
}

HIDDEN int
raytrace_prep(struct nirt_state *nss)
{
    std::set<std::string>::iterator s_it;
    lout(nss, "Prepping the geometry...\n");
    if (nss->rtip_air) {
	rt_prep(nss->rtip_air);
    } 
    if (nss->rtip) {
	rt_prep(nss->rtip);
    }
    lout(nss, "%s ", (nss->active_paths.size() == 1) ? "Object" : "Objects");
    for (size_t i = 0; i < nss->active_paths.size(); i++) {
	if (i == 0) {
	    lout(nss, "'%s'", nss->active_paths[i].c_str());
	} else {
	    lout(nss, " '%s'", nss->active_paths[i].c_str());
	}
    }
    lout(nss, " processed\n");
    return 0;
}

/*****************************************
 * Routines to support output formatting *
 *****************************************/

/* Count how many format specifiers we might find in a format string */
unsigned int
fmt_sp_cnt(const char *fmt) {
    unsigned int fcnt = 0;
    const char *uos = NULL;
    for (uos = fmt; *(uos + 1) != '\0'; ++uos) {
	if (*uos == '%' && (*(uos + 1) == '%')) continue;
	if (*uos == '%' && (*(uos + 1) != '\0')) {
	    fcnt++;
	}
    }
    return fcnt;
}


/* Validate the width specification portion of the format specifier against
 * the system limits and types supported by NIRT.*/
#define NIRT_PRINTF_FLAGS "-+# "
int
fmt_sp_flags_check(struct nirt_state *nss, std::string &fmt_sp)
{
    size_t sp = fmt_sp.find_first_of(NIRT_PRINTF_FLAGS);
    size_t ep = fmt_sp.find_last_of(NIRT_PRINTF_FLAGS);
    if (sp == std::string::npos) return 0; // no flags is acceptable
    std::string flags = fmt_sp.substr(sp, ep - sp + 1);
    size_t invalid_char = flags.find_first_not_of(NIRT_PRINTF_FLAGS);
    if (invalid_char != std::string::npos) {
	lerr(nss, "Error: invalid characters in flags substring (\"%s\") of format specifier \"%s\"\n", flags.c_str(), fmt_sp.c_str());
	return -1;
    }
    return 0;
}

/* Validate the precision specification portion of the format specifier against
 * the system limits */
#define NIRT_PRINTF_PRECISION "0123456789."
#define NIRT_PRINTF_MAXWIDTH 1000 //Arbitrary sanity boundary for width specification
int
fmt_sp_width_precision_check(struct nirt_state *nss, std::string &fmt_sp)
{
    size_t sp = fmt_sp.find_first_of(NIRT_PRINTF_PRECISION);
    size_t ep = fmt_sp.find_last_of(NIRT_PRINTF_PRECISION);
    if (sp == std::string::npos) return 0; // no width/precision settings is acceptable
    std::string p = fmt_sp.substr(sp, ep - sp + 1);
    size_t invalid_char = p.find_first_not_of(NIRT_PRINTF_PRECISION);
    if (invalid_char != std::string::npos) {
	lerr(nss, "Error: invalid characters in precision substring (\"%s\") of format specifier \"%s\"\n", p.c_str(), fmt_sp.c_str());
	return -1;
    }

    /* If we've got one or more numbers, check them for sanity */
    size_t ps = p.find_first_of(".");
    std::string wn, pn;
    if (ps != std::string::npos) {
	if (ps == 0) {
	    pn = p.substr(1, std::string::npos);
	} else {
	    pn = p.substr(ps + 1, std::string::npos);
	    wn = p.substr(0, ps);
	}
    } else {
	wn = p;
    }
    if (wn.length() > 0) {
	// Width check
	struct bu_vls optparse_msg = BU_VLS_INIT_ZERO;
	int w = 0;
	const char *wn_cstr = wn.c_str();
	if (bu_opt_int(&optparse_msg, 1, (const char **)&wn_cstr, (void *)&w) == -1) {
	    lerr(nss, "Error: bu_opt value read failure reading format specifier width from substring \"%s\" of specifier \"%s\": %s\n", wn.c_str(), fmt_sp.c_str(), bu_vls_addr(&optparse_msg));
	    bu_vls_free(&optparse_msg);
	    return -1;
	}
	bu_vls_free(&optparse_msg);
	if (w > NIRT_PRINTF_MAXWIDTH) {
	    lerr(nss, "Error: width specification in format specifier substring \"%s\" of specifier \"%s\" exceeds allowed max width (%d)\n", wn.c_str(), fmt_sp.c_str(), NIRT_PRINTF_MAXWIDTH);
	    return -1;
	} 
    }
    if (pn.length() > 0) {
	// Precision check
	struct bu_vls optparse_msg = BU_VLS_INIT_ZERO;
	int p_num = 0;
	const char *pn_cstr = pn.c_str();
	if (bu_opt_int(&optparse_msg, 1, (const char **)&pn_cstr, (void *)&p_num) == -1) {
	    lerr(nss, "Error: bu_opt value read failure reading format specifier precision from substring \"%s\" of specifier \"%s\": %s\n", pn.c_str(), fmt_sp.c_str(), bu_vls_addr(&optparse_msg));
	    bu_vls_free(&optparse_msg);
	    return -1;
	}
	bu_vls_free(&optparse_msg);

	switch (fmt_sp.c_str()[fmt_sp.length()-1]) {
	    case 'd':
	    case 'i':
		if (p_num > std::numeric_limits<int>::digits10) {
		    lerr(nss, "Error: precision specification in format specifier substring \"%s\" of specifier \"%s\" exceeds allowed maximum (%d)\n", pn.c_str(), fmt_sp.c_str(), std::numeric_limits<int>::digits10);
		    return -1;
		} 
		break;
	    case 'f':
	    case 'e':
	    case 'E':
	    case 'g':
	    case 'G':
		// TODO - once we enable C++ 11 switch the test below to (p > std::numeric_limits<fastf_t>::max_digits10) and update the lerr msg
		if (p_num > std::numeric_limits<fastf_t>::digits10 + 2) {
		    lerr(nss, "Error: precision specification in format specifier substring \"%s\" of specifier \"%s\" exceeds allowed maximum (%d)\n", pn.c_str(), fmt_sp.c_str(), std::numeric_limits<fastf_t>::digits10 + 2);
		    return -1;
		}
		break;
	    case 's':
		if ((size_t)p_num > pn.max_size()) {
		    lerr(nss, "Error: precision specification in format specifier substring \"%s\" of specifier \"%s\" exceeds allowed maximum (%d)\n", pn.c_str(), fmt_sp.c_str(), pn.max_size());
		    return -1;
		}
		break;
	    default:
		lerr(nss, "Error: NIRT internal error in format width/precision validation.\n");
		return -1;
		break;
	}
    }
    return 0;
}

int
fmt_sp_validate(struct nirt_state *nss, std::string &fmt_sp)
{
    // Sanity check any sub-specifiers
    if (fmt_sp_flags_check(nss, fmt_sp)) return -1;
    if (fmt_sp_width_precision_check(nss, fmt_sp)) return -1;
    return 0;
}

/* Processes the first format specifier.  (We use split_fmt to break up a format string into
 * substrings with one format specifier per string - fmt_sp_get's job is to extract the actual
 * format specifier from those substrings.) */
int
fmt_sp_get(struct nirt_state *nss, const char *fmt, std::string &fmt_sp)
{
    int found = 0;
    const char *uos = NULL;
    if (!fmt) return -1;
    /* Find uncommented '%' format specifier */
    for (uos = fmt; *uos != '\0'; ++uos) {
	if (*uos == '%' && (*(uos + 1) == '%')) continue;
	if (*uos == '%' && (*(uos + 1) != '\0')) {
	    found++;
	    break;
	}
    }
    if (!found) {
	lerr(nss, "Error: could not find format specifier in fmt substring \"%s\"\n", fmt);
	return -1;
    }

    // Find the terminating character of the specifier and build
    // the substring.
    std::string wfmt(uos);
    size_t ep = wfmt.find_first_of(NIRT_PRINTF_SPECIFIERS);
    if (ep == std::string::npos) {
	lerr(nss, "Error: could not find valid format specifier terminator in fmt substring \"%s\" - candidates are \"%s\"\n", fmt, NIRT_PRINTF_SPECIFIERS);
	return -1;
    }
    fmt_sp = wfmt.substr(0, ep+1);

    // Check for unsupported fmt features
    size_t invalid_char;
    invalid_char = fmt_sp.find_first_of("*");
    if (invalid_char != std::string::npos) {
	lerr(nss, "Error: NIRT format specifiers do not support wildcard ('*') characters for width or precision: \"%s\"\n", fmt_sp.c_str());
	fmt_sp.clear();
	return -1;
    }
    invalid_char = fmt_sp.find_first_of("hljztL");
    if (invalid_char != std::string::npos) {
	lerr(nss, "Error: NIRT format specifiers do not support length sub-specifiers: \"%s\"\n", fmt_sp.c_str());
	fmt_sp.clear();
	return -1;
    }

    return 0;
}

/* Given a key and a format specifier string, use the NIRT type to check that the
 * supplied key is an appropriate input for that specifier. */
int
fmt_sp_key_check(struct nirt_state *nss, const char *key, std::string &fmt_sp)
{
    int key_ok = 1;
    const char *type = NULL;

    if (!nss || !nss->val_types || fmt_sp.length() == 0) {
	lerr(nss, "Internal NIRT format processing error.\n");
	return -1;
    }

    if (!key || strlen(key) == 0) {
	lerr(nss, "Format specifier \"%s\" has no matching NIRT value key\n", fmt_sp.c_str());
	return -1;
    }

    type = bu_avs_get(nss->val_types, key);
    if (!type) {
	lerr(nss, "Key \"%s\" supplied to format specifier \"%s\" is not a valid NIRT value key\n", key, fmt_sp.c_str());
	return -1;
    }

    switch (fmt_sp.c_str()[fmt_sp.length()-1]) {
	case 'd':
	case 'i':
	    if (!BU_STR_EQUAL(type, "INT")) key_ok = 0;
	    break;
	case 'f':
	case 'e':
	case 'E':
	case 'g':
	case 'G':
	    if (!BU_STR_EQUAL(type, "FLOAT") && !BU_STR_EQUAL(type, "FNOUNIT")) key_ok = 0;
	    break;
	case 's':
	    if (!BU_STR_EQUAL(type, "STRING")) key_ok = 0;
	    break;
	default:
	    lerr(nss, "Error: format specifier terminator \"%s\" is invalid/unsupported: \"%c\" \n", fmt_sp.c_str(), fmt_sp.c_str()[fmt_sp.length()-1]);
	    return -1;
	    break;
    }

    if (!key_ok) {
	lerr(nss, "Error: NIRT value key \"%s\" has type %s, which does not match the type expected by format specifier \"%s\" \n", key, bu_avs_get(nss->val_types, key), fmt_sp.c_str());
	return -1;
    }

    return 0;
}

/* Given a format string, produce an array of substrings each of which contain
 * at most one format specifier */
int
split_fmt(const char *ofmt, char ***breakout)
{
    int i = 0;
    int fcnt = 0;
    const char *up = NULL;
    const char *uos = NULL;
    char **fstrs = NULL;
    struct bu_vls specifier = BU_VLS_INIT_ZERO;
    if (!ofmt) return -1;
    char *cfmt = bu_strdup(ofmt);
    char *fmt = cfmt;

    /* Count maximum possible number of format specifiers.  We have at most
     * fcnt+1 "units" to handle */
    fcnt = fmt_sp_cnt(fmt);
    fstrs = (char **)bu_calloc(fcnt + 1, sizeof(const char *), "output formatters");

    /* initialize to just after the initial '"' char */
    uos = (fmt[0] == '"') ? fmt + 1 : fmt;
    if (fmt[strlen(fmt)-1] == '"') fmt[strlen(fmt)-1] = '\0';

    /* Find one specifier per substring breakout */
    fcnt = 0;
    while (*uos != '\0') {
	int have_specifier= 0;
	up = uos;
	/* Find first '%' format specifier*/
	for (; *uos != '\0'; ++uos) {
	    if (*uos == '%' && (*(uos + 1) == '%')) continue;
	    if (*uos == '%' && have_specifier) break;
	    if (*uos == '%' && !have_specifier) have_specifier = 1;
	}

	/* Store the format. */
	bu_vls_trunc(&specifier, 0);
	while (up != uos) {
	    if (*up == '\\') {
		switch (*(up + 1)) {
		    case 'n':
			bu_vls_putc(&specifier, '\n');
			up += 2;
			break;
		    case '\042':
		    case '\047':
		    case '\134':
			bu_vls_printf(&specifier, "%c", *(up + 1));
			up += 2;
			break;
		    default:
			bu_vls_putc(&specifier, *up++);
			break;
		}
	    } else {
		bu_vls_putc(&specifier, *up++);
	    }
	}
	fstrs[fcnt] = bu_strdup(bu_vls_addr(&specifier));
	fcnt++;
    }
    if (fcnt) {
	(*breakout) = (char **)bu_calloc(fcnt, sizeof(const char *), "breakout");
	for (i = 0; i < fcnt; i++) {
	    (*breakout)[i] = fstrs[i];
	}
	bu_free(fstrs, "temp container");
	bu_vls_free(&specifier);
	bu_free(cfmt, "fmt cpy");
	return fcnt;
    } else {
	bu_free(fstrs, "temp container");
	bu_vls_free(&specifier);
	bu_free(cfmt, "fmt cpy");
	return -1;
    }
}

/* Given a vector breakout of a format string, generate the NIRT report string
 * displaying the full, assembled format string and listing NIRT keys (Items) */
void
print_fmt_str(struct bu_vls *ostr, std::vector<std::pair<std::string,std::string> > &fmt_vect)
{
    std::string fmt_str, fmt_keys;
    std::vector<std::pair<std::string,std::string> >::iterator f_it;

    if (!ostr) return;

    for(f_it = fmt_vect.begin(); f_it != fmt_vect.end(); f_it++) {
	for (std::string::size_type i = 0; i < (*f_it).first.size(); ++i) {
	    if ((*f_it).first[i] == '\n')  {
		fmt_str.push_back('\\');
		fmt_str.push_back('n');
	    } else {
		fmt_str.push_back((*f_it).first[i]);
	    }
	}
	fmt_keys.append(" ");
	fmt_keys.append((*f_it).second);
    }
    bu_vls_sprintf(ostr, "Format: \"%s\"\nItem(s):%s", fmt_str.c_str(), fmt_keys.c_str());
    bu_vls_trimspace(ostr);
}

/* Translate NIRT fmt substrings into fully evaluated printf output, while also
 * handling units on key values.  This should never be called using any fmt
 * string that hasn't been validated by fmt_sp_validate and fmt_sp_key_check */
#define nirt_print_key(keystr,val) if (BU_STR_EQUAL(key, keystr)) bu_vls_printf(ostr, fmt, val)
void
nirt_print_fmt_substr(struct bu_vls *ostr, const char *fmt, const char *key, struct nout_record *r, fastf_t base2local)
{
    if (!ostr || !fmt || !r) return;
    if (!key || !strlen(key)) {
	bu_vls_printf(ostr, fmt);
	return;
    }
    nirt_print_key("x_orig", r->orig[X] * base2local);
    nirt_print_key("y_orig", r->orig[Y] * base2local);
    nirt_print_key("z_orig", r->orig[Z] * base2local);
    nirt_print_key("h", r->h * base2local);
    nirt_print_key("v", r->v * base2local);
    nirt_print_key("d_orig", r->d_orig * base2local);
    nirt_print_key("x_dir", r->dir[X]);
    nirt_print_key("y_dir", r->dir[Y]);
    nirt_print_key("z_dir", r->dir[Z]);
    nirt_print_key("a", r->a);
    nirt_print_key("e", r->e);
    nirt_print_key("x_in", r->in[X] * base2local);
    nirt_print_key("y_in", r->in[Y] * base2local);
    nirt_print_key("z_in", r->in[Z] * base2local);
    nirt_print_key("d_in", r->d_in * base2local);
    nirt_print_key("x_out", r->out[X] * base2local);
    nirt_print_key("y_out", r->out[Y] * base2local);
    nirt_print_key("z_out", r->out[Z] * base2local);
    nirt_print_key("d_out", r->d_out * base2local);
    nirt_print_key("los", r->los * base2local);
    nirt_print_key("scaled_los", r->scaled_los * base2local);
    nirt_print_key("path_name", bu_vls_addr(r->path_name));
    nirt_print_key("reg_name", bu_vls_addr(r->reg_name));
    nirt_print_key("reg_id", r->reg_id);
    nirt_print_key("obliq_in", r->obliq_in);
    nirt_print_key("obliq_out", r->obliq_out);
    nirt_print_key("nm_x_in", r->nm_in[X]);
    nirt_print_key("nm_y_in", r->nm_in[Y]);
    nirt_print_key("nm_z_in", r->nm_in[Z]);
    nirt_print_key("nm_d_in", r->nm_d_in);
    nirt_print_key("nm_h_in", r->nm_h_in);
    nirt_print_key("nm_v_in", r->nm_v_in);
    nirt_print_key("nm_x_out", r->nm_out[X]);
    nirt_print_key("nm_y_out", r->nm_out[Y]);
    nirt_print_key("nm_z_out", r->nm_out[Z]);
    nirt_print_key("nm_d_out", r->nm_d_out);
    nirt_print_key("nm_h_out", r->nm_h_out);
    nirt_print_key("nm_v_out", r->nm_v_out);
    nirt_print_key("ov_reg1_name", bu_vls_addr(r->ov_reg1_name));
    nirt_print_key("ov_reg1_id", r->ov_reg1_id);
    nirt_print_key("ov_reg2_name", bu_vls_addr(r->ov_reg2_name));
    nirt_print_key("ov_reg2_id", r->ov_reg2_id);
    nirt_print_key("ov_sol_in", bu_vls_addr(r->ov_sol_in));
    nirt_print_key("ov_sol_out", bu_vls_addr(r->ov_sol_out));
    nirt_print_key("ov_los", r->ov_los * base2local);
    nirt_print_key("ov_x_in", r->ov_in[X] * base2local);
    nirt_print_key("ov_y_in", r->ov_in[Y] * base2local);
    nirt_print_key("ov_z_in", r->ov_in[Z] * base2local);
    nirt_print_key("ov_d_in", r->ov_d_in * base2local);
    nirt_print_key("ov_x_out", r->ov_out[X] * base2local);
    nirt_print_key("ov_y_out", r->ov_out[Y] * base2local);
    nirt_print_key("ov_z_out", r->ov_out[Z] * base2local);
    nirt_print_key("ov_d_out", r->ov_d_out * base2local);
    nirt_print_key("surf_num_in", r->surf_num_in);
    nirt_print_key("surf_num_out", r->surf_num_out);
    nirt_print_key("claimant_count", r->claimant_count);
    nirt_print_key("claimant_list", bu_vls_addr(r->claimant_list));
    nirt_print_key("claimant_listn", bu_vls_addr(r->claimant_listn));
    nirt_print_key("attributes", bu_vls_addr(r->attributes));
    nirt_print_key("x_gap_in", r->gap_in[X] * base2local);
    nirt_print_key("y_gap_in", r->gap_in[Y] * base2local);
    nirt_print_key("z_gap_in", r->gap_in[Z] * base2local);
    nirt_print_key("gap_los", r->gap_los * base2local);
}

/* Generate the full report string defined by the array of fmt,key pairs
 * associated with the supplied type, based on current values */
void
report(struct nirt_state *nss, char type, struct nout_record *r)
{
    struct bu_vls rstr = BU_VLS_INIT_ZERO;
    std::vector<std::pair<std::string,std::string> > *fmt_vect = NULL;
    std::vector<std::pair<std::string,std::string> >::iterator f_it;

    switch (type) {
	case 'r':
	    fmt_vect = &nss->fmt_ray;
	    break;
	case 'h':
	    fmt_vect = &nss->fmt_head;
	    break;
	case 'p':
	    fmt_vect = &nss->fmt_part;
	    break;
	case 'f':
	    fmt_vect = &nss->fmt_foot;
	    break;
	case 'm':
	    fmt_vect = &nss->fmt_miss;
	    break;
	case 'o':
	    fmt_vect = &nss->fmt_ovlp;
	    break;
	case 'g':
	    fmt_vect = &nss->fmt_gap;
	    break;
	default:
	    lerr(nss, "Error: NIRT internal error, attempt to report using unknown fmt type %c\n", type);
	    return;
    }
    for(f_it = fmt_vect->begin(); f_it != fmt_vect->end(); f_it++) {
	const char *key = (*f_it).second.c_str();
	const char *fmt = (*f_it).first.c_str();
	nirt_print_fmt_substr(&rstr, fmt, key, r, nss->base2local);
    }
    lout(nss, "%s", bu_vls_addr(&rstr));
    bu_vls_free(&rstr);
}


/************************
 * Raytracing Callbacks *
 ************************/

struct overlap *
find_ovlp(struct nirt_state *nss, struct partition *pp)
{
    struct overlap *op;

    for (op = nss->vals->ovlp_list.forw; op != &(nss->vals->ovlp_list); op = op->forw) {
	if (((pp->pt_inhit->hit_dist <= op->in_dist)
		    && (op->in_dist <= pp->pt_outhit->hit_dist)) ||
		((pp->pt_inhit->hit_dist <= op->out_dist)
		 && (op->in_dist <= pp->pt_outhit->hit_dist)))
	    break;
    }
    return (op == &(nss->vals->ovlp_list)) ? OVERLAP_NULL : op;
}


void
del_ovlp(struct overlap *op)
{
    op->forw->backw = op->backw;
    op->backw->forw = op->forw;
    bu_free((char *)op, "free op in del_ovlp");
}

void
init_ovlp(struct nirt_state *nss)
{
    nss->vals->ovlp_list.forw = nss->vals->ovlp_list.backw = &(nss->vals->ovlp_list);
}


extern "C" int
nirt_if_hit(struct application *ap, struct partition *part_head, struct seg *UNUSED(finished_segs))
{
    struct nirt_state *nss = (struct nirt_state *)ap->a_uptr;
    struct nout_record *vals = nss->vals;
    fastf_t ar = nss->vals->a * DEG2RAD;
    fastf_t er = nss->vals->e * DEG2RAD;
    int part_nm = 0;
    overlap *ovp;
    point_t inormal;
    point_t onormal;
    struct partition *part;

    report(nss, 'r', vals);
    report(nss, 'h', vals);

    if (nss->overlap_claims == OVLP_REBUILD_FASTGEN) {
	rt_rebuild_overlaps(part_head, ap, 1);
    } else if (nss->overlap_claims == OVLP_REBUILD_ALL) {
	rt_rebuild_overlaps(part_head, ap, 0);
    }

    for (part = part_head->pt_forw; part != part_head; part = part->pt_forw) {
	++part_nm;

	RT_HIT_NORMAL(inormal, part->pt_inhit, part->pt_inseg->seg_stp,
		&ap->a_ray, part->pt_inflip);
	RT_HIT_NORMAL(onormal, part->pt_outhit, part->pt_outseg->seg_stp,
		&ap->a_ray, part->pt_outflip);

	/* Update the output values */
	if (part_nm > 1) {
	    VMOVE(vals->gap_in, vals->out);
	    // Stash the previous d_out value for use after we calculate the new d_in
	    vals->gap_los = vals->d_out;
	}

	VMOVE(vals->in, part->pt_inhit->hit_point);
	VMOVE(vals->out, part->pt_outhit->hit_point);
	VMOVE(vals->nm_in, inormal);
	VMOVE(vals->nm_out, onormal);

	ldbg(nss, DEBUG_HITS, "Partition %d entry: (%g, %g, %g) exit: (%g, %g, %g)\n",
		part_nm, V3ARGS(vals->in), V3ARGS(vals->out));

	vals->d_in = vals->in[X] * cos(er) * cos(ar) + vals->in[Y] * cos(er) * sin(ar) + vals->in[Z] * sin(er);
	vals->d_out = vals->out[X] * cos(er) * cos(ar) + vals->out[Y] * cos(er) * sin(ar) + vals->out[Z] * sin(er);

	vals->nm_d_in = vals->nm_in[X] * cos(er) * cos(ar) + vals->nm_in[Y] * cos(er) * sin(ar) + vals->nm_in[Z] * sin(er);
	vals->nm_h_in = vals->nm_in[X] * (-sin(ar)) + vals->nm_in[Y] * cos(ar);
	vals->nm_v_in = vals->nm_in[X] * (-sin(er)) * cos(ar) + vals->nm_in[Y] * (-sin(er)) * sin(ar) + vals->nm_in[Z] * cos(er);

	vals->nm_d_out = vals->nm_out[X] * cos(er) * cos(ar) + vals->nm_out[Y] * cos(er) * sin(ar) + vals->nm_out[Z] * sin(er);
	vals->nm_h_out = vals->nm_out[X] * (-sin(ar)) + vals->nm_out[Y] * cos(ar);
	vals->nm_v_out = vals->nm_out[X] * (-sin(er)) * cos(ar) + vals->nm_out[Y] * (-sin(er)) * sin(ar) + vals->nm_out[Z] * cos(er);

	vals->los = vals->d_in - vals->d_out;
	vals->scaled_los = 0.01 * vals->los * part->pt_regionp->reg_los;

	if (part_nm > 1) {
	    /* old d_out - new d_in */
	    vals->gap_los = vals->gap_los - vals->d_in;
	    report(nss, 'g', vals);
	}

	bu_vls_sprintf(vals->path_name, "%s", part->pt_regionp->reg_name);
	{
	    char *tmp_regname = (char *)bu_calloc(bu_vls_strlen(vals->path_name) + 1, sizeof(char), "tmp reg_name");
	    bu_basename(part->pt_regionp->reg_name, tmp_regname);
	    bu_vls_sprintf(vals->reg_name, "%s", tmp_regname);
	    bu_free(tmp_regname, "tmp reg_name");
	}

	vals->reg_id = part->pt_regionp->reg_regionid;
	vals->surf_num_in = part->pt_inhit->hit_surfno;
	vals->surf_num_out = part->pt_outhit->hit_surfno;
	vals->obliq_in = get_obliq(ap->a_ray.r_dir, inormal);
	vals->obliq_out = get_obliq(ap->a_ray.r_dir, onormal);

	bu_vls_trunc(vals->claimant_list, 0);
	bu_vls_trunc(vals->claimant_listn, 0);
	if (part->pt_overlap_reg == 0) {
	    vals->claimant_count = 1;
	} else {
	    struct region **rpp;
	    struct bu_vls tmpcp = BU_VLS_INIT_ZERO;
	    vals->claimant_count = 0;
	    for (rpp = part->pt_overlap_reg; *rpp != REGION_NULL; ++rpp) {
		char *base = NULL;
		if (vals->claimant_count) bu_vls_strcat(vals->claimant_list, " ");
		vals->claimant_count++;
		bu_vls_sprintf(&tmpcp, "%s", (*rpp)->reg_name);
		base = bu_basename(bu_vls_addr(&tmpcp), NULL);
		bu_vls_strcat(vals->claimant_list, base);
		bu_free(base, "bu_basename");
	    }
	    bu_vls_free(&tmpcp);

	    /* insert newlines instead of spaces for listn */
	    bu_vls_sprintf(vals->claimant_listn, "%s", bu_vls_addr(vals->claimant_list));
	    for (char *cp = bu_vls_addr(vals->claimant_listn); *cp != '\0'; ++cp) {
		if (*cp == ' ') *cp = '\n';
	    }
	}

	bu_vls_trunc(vals->attributes, 0);
	std::set<std::string>::iterator a_it;
	for (a_it = nss->attrs.begin(); a_it != nss->attrs.end(); a_it++) {
	    const char *key = (*a_it).c_str();
	    const char *val = bu_avs_get(&part->pt_regionp->attr_values, db5_standard_attribute(db5_standardize_attribute(key)));
	    if (val != NULL) {
		bu_vls_printf(vals->attributes, "%s=%s ", key, val);
	    }
	}

	report(nss, 'p', vals);

	while ((ovp = find_ovlp(nss, part)) != OVERLAP_NULL) {

	    // TODO - do this more cleanly
	    char *copy_ovlp_reg1 = bu_strdup(ovp->reg1->reg_name);
	    char *copy_ovlp_reg2 = bu_strdup(ovp->reg2->reg_name);
	    char *t1 = (char *)bu_calloc(strlen(copy_ovlp_reg1), sizeof(char), "if_hit sval2");
	    bu_basename(copy_ovlp_reg1, t1);
	    bu_vls_sprintf(vals->ov_reg1_name, "%s", t1);
	    bu_free(t1, "t1");
	    bu_free(copy_ovlp_reg1, "cp1");
	    char *t2 = (char *)bu_calloc(strlen(copy_ovlp_reg2), sizeof(char), "if_hit sval3");
	    bu_basename(copy_ovlp_reg2, t2);
	    bu_vls_sprintf(vals->ov_reg2_name, "%s", t2);
	    bu_free(t2, "t2");
	    bu_free(copy_ovlp_reg2, "cp2");


	    vals->ov_reg1_id = ovp->reg1->reg_regionid;
	    vals->ov_reg2_id = ovp->reg2->reg_regionid;
	    bu_vls_sprintf(vals->ov_sol_in, "%s", part->pt_inseg->seg_stp->st_dp->d_namep);
	    bu_vls_sprintf(vals->ov_sol_out, "%s", part->pt_outseg->seg_stp->st_dp->d_namep);
	    VMOVE(vals->ov_in, ovp->in_point);
	    VMOVE(vals->ov_out, ovp->out_point);

	    vals->ov_d_in = vals->d_orig - ovp->in_dist; // TODO looks sketchy in NIRT - did they really mean target(D) ?? -> (VTI_XORIG + 3 -> VTI_H)
	    vals->ov_d_out = vals->d_orig - ovp->out_dist; // TODO looks sketchy in NIRT - did they really mean target(D) ?? -> (VTI_XORIG + 3 -> VTI_H)
	    vals->ov_los = vals->ov_d_in - vals->ov_d_out;

	    report(nss, 'o', vals);

	    del_ovlp(ovp);
	}
    }

    report(nss, 'f', vals);

    if (vals->ovlp_list.forw != &(vals->ovlp_list)) {
	lerr(nss, "Previously unreported overlaps.  Shouldn't happen\n");
	ovp = vals->ovlp_list.forw;
	while (ovp != &(vals->ovlp_list)) {
	    lerr(nss, " OVERLAP:\n\t%s %s (%g %g %g) %g\n", ovp->reg1->reg_name, ovp->reg2->reg_name, V3ARGS(ovp->in_point), ovp->out_dist - ovp->in_dist);
	    ovp = ovp->forw;
	}
    }

    return HIT;
}

extern "C" int
nirt_if_miss(struct application *ap)
{
    struct nirt_state *nss = (struct nirt_state *)ap->a_uptr;
    report(nss, 'r', nss->vals);
    report(nss, 'm', nss->vals);
    return MISS;
}

extern "C" int
nirt_if_overlap(struct application *ap, struct partition *pp, struct region *reg1, struct region *reg2, struct partition *InputHdp)
{
    struct nirt_state *nss = (struct nirt_state *)ap->a_uptr;
    struct overlap *o;
    BU_ALLOC(o, overlap);
    o->ap = ap;
    o->pp = pp;
    o->reg1 = reg1;
    o->reg2 = reg2;
    o->in_dist = pp->pt_inhit->hit_dist;
    o->out_dist = pp->pt_outhit->hit_dist;
    VJOIN1(o->in_point, ap->a_ray.r_pt, pp->pt_inhit->hit_dist, ap->a_ray.r_dir);
    VJOIN1(o->out_point, ap->a_ray.r_pt, pp->pt_outhit->hit_dist, ap->a_ray.r_dir);

    /* Insert the new overlap into the list of overlaps */
    o->forw = nss->vals->ovlp_list.forw;
    o->backw = &(nss->vals->ovlp_list);
    o->forw->backw = o;
    nss->vals->ovlp_list.forw = o;

    /* Match current BRL-CAD default behavior */
    return rt_defoverlap (ap, pp, reg1, reg2, InputHdp);
}

/**************
 *  Commands  *
 **************/

struct nirt_cmd_desc {
    const char *cmd;  // Must match the eventual bu_cmtab key
    const char *desc;
    const char *args;
};

const struct nirt_cmd_desc nirt_descs[] = {
    { "attr",           "select attributes",                             NULL },
    { "ae",             "set/query azimuth and elevation",               "azimuth elevation" },
    { "dir",            "set/query direction vector",                    "x-component y-component z-component" },
    { "hv",             "set/query gridplane coordinates",               "horz vert [dist]" },
    { "xyz",            "set/query target coordinates",                  "X Y Z" },
    { "s",              "shoot a ray at the target",                     NULL },
    { "backout",        "back out of model",                             NULL },
    { "useair",         "set/query use of air",                          "<0|1|2|...>" },
    { "units",          "set/query local units",                         "<mm|cm|m|in|ft>" },
    { "overlap_claims", "set/query overlap rebuilding/retention",        "<0|1|2|3>" },
    { "fmt",            "set/query output formats",                      "{rhpfmog} format item item ..." },
    { "print",          "query an output item",                          "item" },
    { "bot_minpieces",  "Get/Set value for rt_bot_minpieces (0 means do not use pieces, default is 32)", "min_pieces" },
    { "libdebug",       "set/query librt debug flags",                   "hex_flag_value" },
    { "debug",          "set/query nirt debug flags",                    "hex_flag_value" },
    { "q",              "quit",                                          NULL },
    { "?",              "display this help menu",                        NULL },
    { (char *)NULL,     NULL,                                            NULL}
};

const char *
get_desc_args(const char *key)
{
    const struct nirt_cmd_desc *d;
    for (d = nirt_descs; d->cmd; d++) {
	if (BU_STR_EQUAL(d->cmd, key)) return d->args;
    }
    return NULL;
}

extern "C" int
cm_attr(void *ns, int argc, const char *argv[])
{
    if (!ns) return -1;
    struct nirt_state *nss = (struct nirt_state *)ns;
    int i = 0;
    int ac = 0;
    int print_help = 0;
    int attr_print = 0;
    int attr_flush = 0;
    int val_attrs = 0;
    size_t orig_size = nss->attrs.size();
    struct bu_vls optparse_msg = BU_VLS_INIT_ZERO;
    struct bu_opt_desc d[5];
    BU_OPT(d[0],  "h", "help",  "",  NULL,   &print_help, "print help and exit");
    BU_OPT(d[1],  "p", "print", "",  NULL,   &attr_print, "print active attributes");
    BU_OPT(d[2],  "f", "flush", "",  NULL,   &attr_flush, "clear attribute list");
    BU_OPT(d[3],  "v", "",      "",  NULL,   &val_attrs,  "validate attributes - only accept attributes used by one or more objects in the active database");
    BU_OPT_NULL(d[4]);
    const char *help = bu_opt_describe(d, NULL);
    const char *ustr = "Usage: attr <opts> <attribute_name_1> <attribute_name_2> ...\nNote: attr with no options and no attributes to add will print the list of active attributes.\nOptions:";

    argv++; argc--;

    if ((ac = bu_opt_parse(&optparse_msg, argc, (const char **)argv, d)) == -1) {
	lerr(nss, "Error: bu_opt value read failure: %s\n\n%s\n%s\n", bu_vls_addr(&optparse_msg), ustr, help);
	if (help) bu_free((char *)help, "help str");
	bu_vls_free(&optparse_msg);
	return -1;
    }
    bu_vls_free(&optparse_msg);

    if (attr_flush) nss->attrs.clear();

    if (print_help || (attr_print && ac > 0)) {
	lerr(nss, "%s\n%s", ustr, help);
	if (help) bu_free((char *)help, "help str");
	return -1;
    }
    if (attr_print || ac == 0) {
	std::set<std::string>::iterator a_it;
	for (a_it = nss->attrs.begin(); a_it != nss->attrs.end(); a_it++) {
	    lout(nss, "\"%s\"\n", (*a_it).c_str());
	}
	return 0;
    }

    for (i = 0; i < ac; i++) {
	const char *sattr = db5_standard_attribute(db5_standardize_attribute(argv[i]));
	if (val_attrs) {
	    //TODO - check against the .g for any usage of the attribute
	}
	nss->attrs.insert(argv[i]);
	/* If there is a standardized version of this attribute that is
	 * different from the supplied version, activate it as well. */
	if (sattr) nss->attrs.insert(sattr);
    }

    if (nss->active_paths.size() != orig_size) raytrace_prep(nss);
    return 0;
}

extern "C" int
az_el(void *ns, int argc, const char *argv[])
{
    double az = 0.0;
    double el = 0.0;
    int ret = 0;
    struct bu_vls opt_msg = BU_VLS_INIT_ZERO;
    struct nirt_state *nss = (struct nirt_state *)ns;
    if (!ns) return -1;

    if (argc == 1) {
	lout(nss, "(az, el) = (%4.2f, %4.2f)\n", nss->vals->a, nss->vals->e);
	return 0;
    }

    argv++; argc--;

    if (argc != 2) {
	lerr(nss, "Usage:  ae %s\n", get_desc_args("ae"));
	return -1;
    }

    if ((ret = bu_opt_fastf_t(&opt_msg, 1, argv, (void *)&az)) == -1) {
	lerr(nss, "Error: bu_opt value read failure: %s\n", bu_vls_addr(&opt_msg));
	goto azel_done;
    }

    if (fabs(az) > 360) {
	lerr(nss, "Error:  |azimuth| <= 360\n");
	ret = -1;
	goto azel_done;
    }

    argc--; argv++;
    if ((ret = bu_opt_fastf_t(&opt_msg, 1, argv, (void *)&el)) == -1) {
	lerr(nss, "%s\n", bu_vls_addr(&opt_msg));
	goto azel_done;
    }

    if (fabs(el) > 90) {
	lerr(nss, "Error:  |elevation| <= 90\n");
	ret = -1;
	goto azel_done;
    }

    nss->vals->a = az;
    nss->vals->e = el;
    ae2dir(nss);
    ret = 0;

azel_done:
    bu_vls_free(&opt_msg);
    return ret;
}

extern "C" int
dir_vect(void *ns, int argc, const char *argv[])
{
    vect_t dir = VINIT_ZERO;
    struct bu_vls opt_msg = BU_VLS_INIT_ZERO;
    struct nirt_state *nss = (struct nirt_state *)ns;
    if (!ns) return -1;

    if (argc == 1) {
	lout(nss, "(x, y, z) = (%4.2f, %4.2f, %4.2f)\n", V3ARGS(nss->vals->dir));
	return 0;
    }

    argc--; argv++;
    if (argc != 3) {
	lerr(nss, "Usage:  dir %s\n", get_desc_args("dir"));
	return -1;
    }

    if (bu_opt_vect_t(&opt_msg, 3, argv, (void *)&dir) == -1) {
	lerr(nss, "%s\n", bu_vls_addr(&opt_msg));
	bu_vls_free(&opt_msg);
	return -1;
    }

    VUNITIZE(dir);
    VMOVE(nss->vals->dir, dir);
    dir2ae(nss);
    return 0;
}

extern "C" int
grid_coor(void *ns, int argc, const char *argv[])
{
    vect_t grid = VINIT_ZERO;
    struct bu_vls opt_msg = BU_VLS_INIT_ZERO;
    struct nirt_state *nss = (struct nirt_state *)ns;
    if (!ns) return -1;

    if (argc == 1) {
	VSET(grid, nss->vals->h, nss->vals->v, nss->vals->d_orig);
	VSCALE(grid, grid, nss->base2local);
	lout(nss, "(h, v, d) = (%4.2f, %4.2f, %4.2f)\n", V3ARGS(grid));
	return 0;
    }

    argc--; argv++;
    if (argc != 2 && argc != 3) {
	lerr(nss, "Usage:  hv %s\n", get_desc_args("hv"));
	return -1;
    }
    if (bu_opt_fastf_t(&opt_msg, 1, argv, (void *)&(nss->vals->h)) == -1) {
	lerr(nss, "%s\n", bu_vls_addr(&opt_msg));
	bu_vls_free(&opt_msg);
	return -1;
    }
    argc--; argv++;
    if (bu_opt_fastf_t(&opt_msg, 1, argv, (void *)&(nss->vals->v)) == -1) {
	lerr(nss, "%s\n", bu_vls_addr(&opt_msg));
	bu_vls_free(&opt_msg);
	return -1;
    }
    argc--; argv++;
    if (argc) {
	if (bu_opt_fastf_t(&opt_msg, 1, argv, (void *)&(nss->vals->d_orig)) == -1) {
	    lerr(nss, "%s\n", bu_vls_addr(&opt_msg));
	    bu_vls_free(&opt_msg);
	    return -1;
	}
    } else {
	grid[2] = nss->vals->d_orig;
    }

    VSCALE(grid, grid, nss->local2base);
    nss->vals->h = grid[0];
    nss->vals->v = grid[1];
    nss->vals->d_orig = grid[2];
    grid2targ(nss);

    return 0;
}

extern "C" int
target_coor(void *ns, int argc, const char *argv[])
{
    vect_t target = VINIT_ZERO;
    struct bu_vls opt_msg = BU_VLS_INIT_ZERO;
    struct nirt_state *nss = (struct nirt_state *)ns;
    if (!ns) return -1;

    if (argc == 1) {
	VMOVE(target, nss->vals->orig);
	VSCALE(target, target, nss->base2local);
	lout(nss, "(x, y, z) = (%4.2f, %4.2f, %4.2f)\n", V3ARGS(target));
	return 0;
    }

    argc--; argv++;
    if (argc != 3) {
	lerr(nss, "Usage:  xyz %s\n", get_desc_args("xyz"));
	return -1;
    }

    if (bu_opt_vect_t(&opt_msg, 3, argv, (void *)&target) == -1) {
	lerr(nss, "%s\n", bu_vls_addr(&opt_msg));
	bu_vls_free(&opt_msg);
	return -1;
    }

    VSCALE(target, target, nss->local2base);
    VMOVE(nss->vals->orig, target);
    VMOVE(nss->vals->orig, target);
    targ2grid(nss);

    return 0;
}

extern "C" int
shoot(void *ns, int argc, const char **UNUSED(argv))
{
    int i;
    struct nirt_state *nss = (struct nirt_state *)ns;
    if (!ns || !nss->ap || !nss->ap->a_rt_i) return -1;

    if (argc != 1) {
	lerr(nss, "Usage:  s\n");
	return -1;
    }

    double bov = backout(nss);
    for (i = 0; i < 3; ++i) {
	nss->vals->orig[i] = nss->vals->orig[i] + (bov * -1*(nss->vals->dir[i]));
	nss->ap->a_ray.r_pt[i] = nss->vals->orig[i];
	nss->ap->a_ray.r_dir[i] = nss->vals->dir[i];
    }

    ldbg(nss, DEBUG_BACKOUT,
	    "Backing out %g units to (%g %g %g), shooting dir is (%g %g %g)\n",
	    bov * nss->base2local,
	    nss->ap->a_ray.r_pt[0] * nss->base2local,
	    nss->ap->a_ray.r_pt[1] * nss->base2local,
	    nss->ap->a_ray.r_pt[2] * nss->base2local,
	    V3ARGS(nss->ap->a_ray.r_dir));

    // TODO - any necessary initialization for data collection by callbacks
    init_ovlp(nss);
    (void)rt_shootray(nss->ap);

    // Undo backout
    for (i = 0; i < 3; ++i) {
	nss->vals->orig[i] = nss->vals->orig[i] - (bov * -1*(nss->vals->dir[i]));
    }

    return 0;
}

extern "C" int
backout(void *ns, int argc, const char *argv[])
{
    struct nirt_state *nss = (struct nirt_state *)ns;
    if (!ns) return -1;

    if (argc == 1) {
	lout(nss, "backout = %d\n", nss->backout);
	return 0;
    }

    if (argc == 2 && BU_STR_EQUAL(argv[1], "0")) {
	nss->backout = 0;
	return 0;
    }
    if (argc == 2 && BU_STR_EQUAL(argv[1], "1")) {
	nss->backout = 1;
	return 0;
    }

    lerr(nss, "backout must be set to 0 (off) or 1 (on)\n");

    return -1;
}

extern "C" int
use_air(void *ns, int argc, const char *argv[])
{
    struct nirt_state *nss = (struct nirt_state *)ns;
    struct bu_vls optparse_msg = BU_VLS_INIT_ZERO;
    if (!ns) return -1;
    if (argc == 1) {
	lout(nss, "use_air = %d\n", nss->use_air);
	return 0;
    }
    if (argc > 2) {
	lerr(nss, "Usage:  useair %s\n", get_desc_args("useair"));
	return -1;
    }
    if (bu_opt_int(&optparse_msg, 1, (const char **)&(argv[1]), (void *)&nss->use_air) == -1) {
	lerr(nss, "Error: bu_opt value read failure: %s\n\nUsage:  useair %s\n", bu_vls_addr(&optparse_msg), get_desc_args("useair"));
	return -1;
    }
    nss->ap->a_rt_i = get_rtip(nss);
    nss->ap->a_resource = get_resource(nss);
    return 0;
}

extern "C" int
nirt_units(void *ns, int argc, const char *argv[])
{
    double ncv = 0.0;
    struct nirt_state *nss = (struct nirt_state *)ns;
    if (!ns) return -1;

    if (argc == 1) {
	lout(nss, "units = %s\n", bu_units_string(nss->local2base));
	return 0;
    }

    if (argc > 2) {
	lerr(nss, "Usage:  units %s\n", get_desc_args("units"));
	return -1;
    }

    if (BU_STR_EQUAL(argv[1], "default")) {
	nss->base2local = nss->dbip->dbi_base2local;
	nss->local2base = nss->dbip->dbi_local2base;
	return 0;
    }

    ncv = bu_units_conversion(argv[1]);
    if (ncv <= 0.0) {
	lerr(nss, "Invalid unit specification: '%s'\n", argv[1]);
	return -1;
    }
    nss->base2local = 1.0/ncv;
    nss->local2base = ncv;

    return 0;
}

extern "C" int
do_overlap_claims(void *ns, int argc, const char *argv[])
{
    int valid_specification = 0;
    struct nirt_state *nss = (struct nirt_state *)ns;
    if (!ns) return -1;

    if (argc == 1) {
	switch (nss->overlap_claims) {
	    case OVLP_RESOLVE:
		lout(nss, "resolve (0)\n");
		return 0;
	    case OVLP_REBUILD_FASTGEN:
		lout(nss, "rebuild_fastgen (1)\n");
		return 0;
	    case OVLP_REBUILD_ALL:
		lout(nss, "rebuild_all (2)\n");
		return 0;
	    case OVLP_RETAIN:
		lout(nss, "retain (3)\n");
		return 0;
	    default:
		lerr(nss, "Error: overlap_clams is set to invalid value %d\n", nss->overlap_claims);
		return -1;
	}
    }

    if (argc > 2) {
	lerr(nss, "Usage:  overlap_claims %s\n", get_desc_args("overlap_claims"));
	return -1;
    }

    if (BU_STR_EQUAL(argv[1], "resolve") || BU_STR_EQUAL(argv[1], "0")) {
	nss->overlap_claims = OVLP_RESOLVE;
	valid_specification = 1;
    }
    if (BU_STR_EQUAL(argv[1], "rebuild_fastgen") || BU_STR_EQUAL(argv[1], "1")) {
	nss->overlap_claims = OVLP_REBUILD_FASTGEN;
	valid_specification = 1;
    }
    if (BU_STR_EQUAL(argv[1], "rebuild_all") || BU_STR_EQUAL(argv[1], "2")) {
	nss->overlap_claims = OVLP_REBUILD_ALL;
	valid_specification = 1;
    }
    if (BU_STR_EQUAL(argv[1], "retain") || BU_STR_EQUAL(argv[1], "3")) {
	nss->overlap_claims = OVLP_RETAIN;
	valid_specification = 1;
    }

    if (!valid_specification) {
	lerr(nss, "Error: Invalid overlap_claims specification: '%s'\n", argv[1]);
	return -1;
    }

    if (nss->rtip) nss->rtip->rti_save_overlaps = (nss->overlap_claims > 0);
    if (nss->rtip_air) nss->rtip_air->rti_save_overlaps = (nss->overlap_claims > 0);

    return 0;
}

extern "C" int
format_output(void *ns, int argc, const char **argv)
{
    struct nirt_state *nss = (struct nirt_state *)ns;
    char type;
    if (!ns) return -1;

    if (argc < 2 || strlen(argv[1]) != 1 || BU_STR_EQUAL(argv[1], "?")) {
	// TODO - need a *much* better help for fmt
	lerr(nss, "Usage:  fmt %s\n", get_desc_args("fmt"));
	return -1;
    }

    type = argv[1][0];
    if (!strchr(NIRT_OUTPUT_TYPE_SPECIFIERS, type)) {
	lerr(nss, "Error: unknown fmt type %c\n", type);
	return -1;
    }

    argc--; argv++;
    if (argc == 1) {
	// print out current fmt str for the specified type
	struct bu_vls ostr = BU_VLS_INIT_ZERO;
	switch (type) {
	    case 'r':
		print_fmt_str(&ostr, nss->fmt_ray);
		break;
	    case 'h':
		print_fmt_str(&ostr, nss->fmt_head);
		break;
	    case 'p':
		print_fmt_str(&ostr, nss->fmt_part);
		break;
	    case 'f':
		print_fmt_str(&ostr, nss->fmt_foot);
		break;
	    case 'm':
		print_fmt_str(&ostr, nss->fmt_miss);
		break;
	    case 'o':
		print_fmt_str(&ostr, nss->fmt_ovlp);
		break;
	    case 'g':
		print_fmt_str(&ostr, nss->fmt_gap);
		break;
	    default:
		lerr(nss, "Error: unknown fmt type %s\n", argv[1]);
		return -1;
		break;
	}
	lout(nss, "%s\n", bu_vls_addr(&ostr));
	bu_vls_free(&ostr);
	return 0;
    }

    argc--; argv++;
    if (argc) {

	int ac_prev = 0;
	int fmt_cnt = 0;
	int fmt_sp_count = 0;
	std::vector<std::pair<std::string,std::string> > fmt_tmp;
	const char *fmtstr = argv[0];
	char **fmts = NULL;

	/* Empty format strings are easy */
	if (strlen(argv[0]) == 0 || BU_STR_EQUAL(argv[0], "\"\"") || BU_STR_EQUAL(argv[0], "''")) {
	    goto set_fmt;
	}

	/* We now must take the format string and do a lot of validation on it so we
	 * can safely use it to dynamically format output.  This is a classic problem in C:
	 * CWE-134: Use of Externally-Controlled Format String. See
	 * http://cwe.mitre.org/data/definitions/134.html for more details. */

	if ((fmt_cnt = split_fmt(fmtstr, &fmts)) <= 0) {
	    lerr(nss, "Error: failure parsing format string \"%s\"\n", fmtstr);
	    return -1;
	}
	argc--; argv++;

	for (int i = 0; i < fmt_cnt; i++) {
	    unsigned int fc = fmt_sp_cnt(fmts[i]);
	    const char *key = NULL;
	    if (fc > 0) {
		std::string fs;
		if (!argc) {
		    lerr(nss, "Error: failure parsing format string \"%s\" - missing value for format specifier in substring \"%s\"\n", fmtstr, fmts[i]);
		    return -1;
		}
		key = argv[0];
		fmt_sp_count++;

		// Get fmt_sp substring and perform validation with the matching NIRT value key
		if (fmt_sp_get(nss, fmts[i], fs)) return -1;
		if (fmt_sp_validate(nss, fs)) return -1;
		if (fmt_sp_key_check(nss, key, fs)) return -1;

		argc--; argv++;
	    }
	    if (key) {
		fmt_tmp.push_back(std::make_pair(std::string(fmts[i]), std::string(key)));
	    } else {
		/* If there are no format specifiers, we don't need any key */
		fmt_tmp.push_back(std::make_pair(std::string(fmts[i]), ""));
	    }
	}

	/* we don't care about leftovers if they're empty (looks like
	 * bu_argv_from_string can empty argv strings...) */
	ac_prev = 0;
	while (argc > 0 && ac_prev != argc) {
	    if (!strlen(argv[0])) {
		ac_prev = argc;
		argc--; argv++;
	    } else {
		ac_prev = argc;
	    }
	}

	/* If we *do* have leftover we we care about, error out */
	if (argc) {
	    lerr(nss, "Error: fmt string \"%s\" has %d format %s, but has %d %s specified\n", fmtstr, fmt_sp_count, (fmt_sp_count == 1) ? "specifier" : "specifiers", fmt_sp_count + argc, ((fmt_sp_count + argc) == 1) ? "key" : "keys");
	    return -1;
	}

	/* OK, validation passed - we should be good.  Scrub and replace pre-existing
	 * formatting instructions with the new. */
set_fmt:
	switch (type) {
	    case 'r':
		nss->fmt_ray.clear();
		nss->fmt_ray = fmt_tmp;
		break;
	    case 'h':
		nss->fmt_head.clear();
		nss->fmt_head = fmt_tmp;
		break;
	    case 'p':
		nss->fmt_part.clear();
		nss->fmt_part = fmt_tmp;
		break;
	    case 'f':
		nss->fmt_foot.clear();
		nss->fmt_foot = fmt_tmp;
		break;
	    case 'm':
		nss->fmt_miss.clear();
		nss->fmt_miss = fmt_tmp;
		break;
	    case 'o':
		nss->fmt_ovlp.clear();
		nss->fmt_ovlp = fmt_tmp;
		break;
	    case 'g':
		nss->fmt_gap.clear();
		nss->fmt_gap = fmt_tmp;
		break;
	    default:
		lerr(nss, "Error: unknown fmt type %s\n", argv[1]);
		return -1;
		break;
	}
    }

    return 0;
}

extern "C" int
print_item(void *ns, int argc, const char **argv)
{
    int i = 0;
    struct bu_vls oval = BU_VLS_INIT_ZERO;
    struct nirt_state *nss = (struct nirt_state *)ns;
    if (!ns) return -1;

    if (argc == 1) {
	lerr(nss, "Usage:  print %s\n", get_desc_args("print"));
	return 0;
    }

    argc--; argv++;

    for (i = 0; i < argc; i++) {
	const char *val_type = bu_avs_get(nss->val_types, argv[i]);
	bu_vls_trunc(&oval, 0);
	if (!val_type) {
	    lerr(nss, "Error: item %s is not a valid NIRT value key\n", argv[i]);
	    bu_vls_free(&oval);
	    return -1;
	}
	if (BU_STR_EQUAL(val_type, "INT")) {
	    nirt_print_fmt_substr(&oval, "%d", argv[i], nss->vals, nss->base2local);
	    lout(nss, "%s\n", bu_vls_addr(&oval));
	    continue;
	}
	if (BU_STR_EQUAL(val_type, "FLOAT") || BU_STR_EQUAL(val_type, "FNOUNIT")) {
	    nirt_print_fmt_substr(&oval, "%g", argv[i], nss->vals, nss->base2local);
	    lout(nss, "%s\n", bu_vls_addr(&oval));
	    continue;
	}
	if (BU_STR_EQUAL(val_type, "STRING")) {
	    nirt_print_fmt_substr(&oval, "%s", argv[i], nss->vals, nss->base2local);
	    lout(nss, "%s\n", bu_vls_addr(&oval));
	    continue;
	}
	lerr(nss, "Error: item %s has unknown value type %s\n", argv[i], val_type);
	bu_vls_free(&oval);
	return -1;
    }

    bu_vls_free(&oval);
    return 0;
}

extern "C" int
bot_minpieces(void *ns, int argc, const char **argv)
{
    /* Ew - rt_bot_minpieces is a librt global.  Why isn't it
     * a part of the specific rtip? */
    int ret = 0;
    long minpieces = 0;
    struct bu_vls opt_msg = BU_VLS_INIT_ZERO;
    struct nirt_state *nss = (struct nirt_state *)ns;
    if (!ns) return -1;

    if (argc == 1) {
	lout(nss, "rt_bot_minpieces = %d\n", (unsigned int)rt_bot_minpieces);
	return 0;
    }

    argc--; argv++;

    if (argc > 1) {
	lerr(nss, "Usage:  bot_minpieces %s\n", get_desc_args("bot_minpieces"));
	return -1;
    }

    if ((ret = bu_opt_long(&opt_msg, 1, argv, (void *)&minpieces)) == -1) {
	lerr(nss, "Error: bu_opt value read failure reading minpieces value: %s\n", bu_vls_addr(&opt_msg));
	goto bot_minpieces_done;
    }

    if (minpieces < 0) {
	lerr(nss, "Error: rt_bot_minpieces cannot be less than 0\n");
	ret = -1;
	goto bot_minpieces_done;
    }

    if (rt_bot_minpieces != (size_t)minpieces) {
	rt_bot_minpieces = minpieces;
	bu_vls_free(&opt_msg);
	return raytrace_prep(nss);
    }

bot_minpieces_done:
    bu_vls_free(&opt_msg);
    return ret;
}

extern "C" int
librt_debug(void *ns, int argc, const char **argv)
{
    int ret = 0;
    long dflg = 0;
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    struct nirt_state *nss = (struct nirt_state *)ns;
    if (!ns) return -1;

    /* Sigh... another librt global, not rtip specific... */
    if (argc == 1) {
	bu_vls_printb(&msg, "librt debug ", RT_G_DEBUG, DEBUG_FMT);
	lout(nss, "%s\n", bu_vls_addr(&msg));
	goto librt_nirt_debug_done;
    }

    argc--; argv++;

    if (argc > 1) {
	lerr(nss, "Usage:  libdebug %s\n", get_desc_args("libdebug"));
	return -1;
    }

    if ((ret = bu_opt_long_hex(&msg, 1, argv, (void *)&dflg)) == -1) {
	lerr(nss, "%s\n", bu_vls_addr(&msg));
	goto librt_nirt_debug_done;
    }

    if (dflg < 0 || dflg > UINT32_MAX) {
	lerr(nss, "Error: LIBRT debug flag cannot be less than 0 or greater than %d\n", UINT32_MAX);
	ret = -1;
	goto librt_nirt_debug_done;
    }

    RT_G_DEBUG = (uint32_t)dflg;
    bu_vls_printb(&msg, "librt debug ", RT_G_DEBUG, DEBUG_FMT);
    lout(nss, "%s\n", bu_vls_addr(&msg));

librt_nirt_debug_done:
    bu_vls_free(&msg);
    return ret;
}

extern "C" int
nirt_debug(void *ns, int argc, const char **argv)
{
    int ret = 0;
    long dflg = 0;
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    struct nirt_state *nss = (struct nirt_state *)ns;
    if (!ns) return -1;

    if (argc == 1) {
	bu_vls_printb(&msg, "debug ", nss->nirt_debug, DEBUG_FMT);
	lout(nss, "%s\n", bu_vls_addr(&msg));
	goto nirt_debug_done;
    }

    argc--; argv++;

    if (argc > 1) {
	lerr(nss, "Usage:  debug %s\n", get_desc_args("debug"));
	return -1;
    }

    if ((ret = bu_opt_long_hex(&msg, 1, argv, (void *)&dflg)) == -1) {
	lerr(nss, "%s\n", bu_vls_addr(&msg));
	goto nirt_debug_done;
    }

    if (dflg < 0) {
	lerr(nss, "Error: NIRT debug flag cannot be less than 0\n");
	ret = -1;
	goto nirt_debug_done;
    }

    nss->nirt_debug = (unsigned int)dflg;
    bu_vls_printb(&msg, "debug ", nss->nirt_debug, DEBUG_FMT);
    lout(nss, "%s\n", bu_vls_addr(&msg));

nirt_debug_done:
    bu_vls_free(&msg);
    return ret;
}

extern "C" int
quit(void *ns, int UNUSED(argc), const char **UNUSED(argv))
{
    lout((struct nirt_state *)ns, "Quitting...\n");
    return 1;
}

extern "C" int
show_menu(void *ns, int UNUSED(argc), const char **UNUSED(argv))
{
    int longest = 0;
    const struct nirt_cmd_desc *d;
    struct nirt_state *nss = (struct nirt_state *)ns;
    if (!ns) return -1;
    for (d = nirt_descs; d->cmd != NULL; d++) {
	int l = strlen(d->cmd);
	if (l > longest) longest = l;
    }
    for (d = nirt_descs; d->cmd != NULL; d++) {
	lout(nss, "%*s %s\n", longest, d->cmd, d->desc);
    }
    return 0;
}

extern "C" int
draw_cmd(void *ns, int argc, const char *argv[])
{
    struct nirt_state *nss = (struct nirt_state *)ns;
    int i = 0;
    int ret = 0;
    if (!ns || argc < 2) return -1;
    for (i = 1; i < argc; i++) {
	if (std::find(nss->active_paths.begin(), nss->active_paths.end(), argv[i]) == nss->active_paths.end()) {
	    bu_log("draw_cmd: drawing %s\n", argv[i]);
	    nss->active_paths.push_back(argv[i]);
	}
    }
    ret = raytrace_gettrees(nss);
    if (ret) return ret;
    return raytrace_prep(nss);
}

extern "C" int
erase_cmd(void *ns, int UNUSED(argc), const char **UNUSED(argv))
{
    //struct nirt_state *nss = (struct nirt_state *)ns;
    if (!ns) return -1;
    bu_log("erase_cmd\n");
    return 0;
}

extern "C" int
state_cmd(void *ns, int argc, const char *argv[])
{
    struct nirt_state *nss = (struct nirt_state *)ns;
    if (!ns) return -1;
    struct rt_i *rtip = nss->ap->a_rt_i;
    double base2local = rtip->rti_dbip->dbi_base2local;
    if (argc == 1) {
	// TODO
	return 0;
    }
    if (BU_STR_EQUAL(argv[1], "out_accumulate")) {
	int setting = 0;
	if (argc == 2) {
	    lout(nss, "%d\n", nss->out_accumulate);
	    return 0;
	}
	(void)bu_opt_int(NULL, 1, (const char **)&(argv[2]), (void *)&setting);
	nss->out_accumulate = setting;
	return 0;
    }
    if (BU_STR_EQUAL(argv[1], "err_accumulate")) {
	int setting = 0;
	if (argc == 2) {
	    lout(nss, "%d\n", nss->err_accumulate);
	    return 0;
	}
	(void)bu_opt_int(NULL, 1, (const char **)&(argv[2]), (void *)&setting);
	nss->err_accumulate = setting;
	return 0;
    }
    if (BU_STR_EQUAL(argv[1], "model_bounds")) {
	if (argc != 2) {
	    lerr(nss, "TODO - state cmd help\n");
	    return -1;
	}
	lout(nss, "model_min = (%g, %g, %g)    model_max = (%g, %g, %g)\n",
		rtip->mdl_min[X] * base2local,
		rtip->mdl_min[Y] * base2local,
		rtip->mdl_min[Z] * base2local,
		rtip->mdl_max[X] * base2local,
		rtip->mdl_max[Y] * base2local,
		rtip->mdl_max[Z] * base2local);
	return 0;
    }
    return 0;
}

const struct bu_cmdtab nirt_cmds[] = {
    { "attr",           cm_attr},
    { "ae",             az_el},
    { "dir",            dir_vect},
    { "hv",             grid_coor},
    { "xyz",            target_coor},
    { "s",              shoot},
    { "backout",        backout},
    { "useair",         use_air},
    { "units",          nirt_units},
    { "overlap_claims", do_overlap_claims},
    { "fmt",            format_output},
    { "print",          print_item},
    { "bot_minpieces",  bot_minpieces},
    { "libdebug",       librt_debug},
    { "debug",          nirt_debug},
    { "q",              quit},
    { "?",              show_menu},
    // New commands...
    { "draw",           draw_cmd},
    { "erase",          erase_cmd},
    { "state",          state_cmd},
    { (char *)NULL,     NULL}
};


HIDDEN size_t
find_first_unescaped(std::string &s, const char *keys, int offset)
{
    int off = offset;
    int done = 0;
    size_t candidate = std::string::npos;
    while (!done) {
	candidate = s.find_first_of(keys, off);
	if (!candidate || candidate == std::string::npos) return candidate;
	if (s.at(candidate - 1) == '\\') {
	    off = candidate + 1;
	} else {
	    done = 1;
	}
    }
    return candidate;
}

HIDDEN size_t
find_first_unquoted(std::string &ts, const char *key, size_t offset)
{
    size_t q_start, q_end, pos;
    std::string s = ts;
    /* Start by initializing the position markers for quoted substrings. */
    q_start = find_first_unescaped(s, "\"", offset);
    q_end = (q_start != std::string::npos) ? find_first_unescaped(s, "\"", q_start + 1) : std::string::npos;

    pos = offset;
    while ((pos = s.find(key, pos)) != std::string::npos) {
	/* If we're inside matched quotes, only an un-escaped quote char means
	 * anything */
	if (q_end != std::string::npos && pos > q_start && pos < q_end) {
	    pos = q_end + 1;
	    q_start = find_first_unescaped(s, "\"", q_end + 1);
	    q_end = (q_start != std::string::npos) ? find_first_unescaped(s, "'\"", q_start + 1) : std::string::npos;
	    continue;
	}
	break;
    }
    return pos;
}


/* Parse command line command and execute */
HIDDEN int
nirt_exec_cmd(NIRT *ns, const char *cmdstr)
{
    int ac = 0;
    int ac_max = 0;
    int i = 0;
    int ret = 0;
    char **av = NULL;
    size_t q_start, q_end, pos;
    std::string entry;
    if (!ns || !cmdstr || strlen(cmdstr) > entry.max_size()) return -1;
    std::string s(cmdstr);
    std::stringstream ss(s);
    // get an upper limit on the size of argv
    while (std::getline(ss, entry, ' ')) ac_max++;
    ss.clear();
    ss.seekg(0, ss.beg);

    /* Start by initializing the position markers for quoted substrings. */
    q_start = find_first_unescaped(s, "\"", 0);
    q_end = (q_start != std::string::npos) ? find_first_unescaped(s, "\"", q_start + 1) : std::string::npos;

    /* get an argc/argv array for bu_cmd style command execution */
    av = (char **)bu_calloc(ac_max+1, sizeof(char *), "av");

    pos = 0; ac = 0;
    while ((pos = s.find_first_of(" \t", pos)) != std::string::npos) {
	/* If we're inside matched quotes, only an un-escaped quote char means
	 * anything */
	if (q_end != std::string::npos && pos > q_start && pos < q_end) {
	    pos = q_end + 1;
	    continue;
	}
	/* Extract and operate on the command specific subset string */
	std::string cmd = s.substr(0, pos);
	if (cmd.find_first_not_of(" \t") != std::string::npos) {
	    av[ac] = bu_strdup(cmd.c_str());
	    ac++;
	}

	/* Prepare the rest of the script string for processing */
	s.erase(0, pos + 1);
	q_start = find_first_unescaped(s, "\"", 0);
	q_end = (q_start != std::string::npos) ? find_first_unescaped(s, "\"", q_start + 1) : std::string::npos;
	pos = 0;
    }
    if (s.length() > 0) {
	av[ac] = bu_strdup(s.c_str());
	ac++;
    }
    av[ac] = NULL;

    if (bu_cmd(nirt_cmds, ac, (const char **)av, 0, (void *)ns, &ret) != BRLCAD_OK) ret = -1;

    for (i = 0; i < ac_max; i++) {
	if (av[i]) bu_free(av[i], "free av str");
    }
    bu_free(av, "free av array");
    return ret;
}

/* Parse command line script into individual commands, and run
 * the supplied callback on each command. The semicolon ';' char
 * denotes the end of one command and the beginning of another, unless
 * the semicolon is inside single or double quotes*/
HIDDEN int
nirt_parse_script(NIRT *ns, const char *script, int (*nc)(NIRT *ns, const char *cmdstr))
{
    if (!ns || !script || !nc) return -1;
    int ret = 0;
    std::string s(script);
    std::string cmd;
    struct bu_vls tcmd = BU_VLS_INIT_ZERO;
    size_t pos = 0;
    size_t q_start, q_end;

    /* TODO - eat leading whitespace, potentially masking a comment char */

    /* If this line is a comment, we're done */
    if (script[0] == '#') return 0;

    /* Start by initializing the position markers for quoted substrings. */
    q_start = find_first_unescaped(s, "\"", 0);
    q_end = (q_start != std::string::npos) ? find_first_unescaped(s, "\"", q_start + 1) : std::string::npos;

    /* Slice and dice to get individual commands. */
    while ((pos = find_first_unquoted(s, ";", pos)) != std::string::npos) {
	/* If we're inside matched quotes, only an un-escaped quote char means
	 * anything */
	if (q_end != std::string::npos && pos > q_start && pos < q_end) {
	    pos = q_end + 1;
	    continue;
	}
	/* Extract and operate on the command specific subset string */
	cmd = s.substr(0, pos);
	bu_vls_sprintf(&tcmd, "%s", cmd.c_str());
	bu_vls_trimspace(&tcmd);
	if (bu_vls_strlen(&tcmd) > 0) {
	    if (strchr(bu_vls_addr(&tcmd), ';')) {
		ret = nirt_parse_script(ns, bu_vls_addr(&tcmd), nc);
	    } else {
		ret = (*nc)(ns, bu_vls_addr(&tcmd));
	    }
	    if (ret) goto nps_done;
	}

	/* Prepare the rest of the script string for processing */
	s.erase(0, pos + 1);
	q_start = find_first_unescaped(s, "\"", 0);
	q_end = (q_start != std::string::npos) ? find_first_unescaped(s, "\"", q_start + 1) : std::string::npos;
	pos = 0;
    }

    /* If there's anything left over, it's a tailing command or a single command */
    bu_vls_sprintf(&tcmd, "%s", s.c_str());
    bu_vls_trimspace(&tcmd);
    if (bu_vls_strlen(&tcmd) > 0) {
	ret = (*nc)(ns, bu_vls_addr(&tcmd));
    }

nps_done:
    bu_vls_free(&tcmd);
    return ret;
}


//////////////////////////
/* Public API functions */
//////////////////////////

extern "C" int
nirt_alloc(NIRT **ns)
{
    unsigned int rgb[3] = {0, 0, 0};
    NIRT *n = NULL;

    if (!ns) return -1;

    /* Get memory */
    n = new nirt_state;
    BU_GET(n->hit_color, struct bu_color);
    BU_GET(n->miss_color, struct bu_color);
    BU_GET(n->overlap_color, struct bu_color);

    /* Strictly speaking these initializations are more
     * than an "alloc" function needs to do, but we want
     * a nirt state to be "functional" even though it
     * is not set up with a database (it's true/proper
     * initialization.)*/
    bu_color_from_rgb_chars(n->hit_color, (unsigned char *)&rgb);
    bu_color_from_rgb_chars(n->miss_color, (unsigned char *)&rgb);
    bu_color_from_rgb_chars(n->overlap_color, (unsigned char *)&rgb);
    n->print_header = 0;
    n->print_ident_flag = 0;
    n->silent_flag = SILENT_UNSET;
    n->rt_debug = 0;
    n->nirt_debug = 0;

    BU_GET(n->out, struct bu_vls);
    bu_vls_init(n->out);
    n->out_accumulate = 0;
    BU_GET(n->err, struct bu_vls);
    bu_vls_init(n->err);
    n->err_accumulate = 0;
    BU_LIST_INIT(&(n->s_vlist));
    n->segs = bn_vlblock_init(&(n->s_vlist), 32);
    n->ret = 0;

    n->h_state = NULL;
    n->h_out = NULL;
    n->h_err = NULL;
    n->h_segs = NULL;
    n->h_objs = NULL;
    n->h_frmts = NULL;
    n->h_view = NULL;

    n->base2local = 0.0;
    n->local2base = 0.0;
    n->use_air = 0;

    n->b_state = false;
    n->b_segs = false;
    n->b_objs = false;
    n->b_frmts = false;
    n->b_view = false;

    BU_GET(n->ap, struct application);
    n->dbip = DBI_NULL;
    BU_GET(n->res, struct resource);
    BU_GET(n->res_air, struct resource);
    n->rtip = RTI_NULL;
    n->rtip_air = RTI_NULL;

    BU_GET(n->val_types, struct bu_attribute_value_set);
    BU_GET(n->val_docs, struct bu_attribute_value_set);
    bu_avs_init_empty(n->val_types);
    bu_avs_init_empty(n->val_docs);

    BU_GET(n->vals, struct nout_record);
    BU_GET(n->vals->path_name, struct bu_vls);
    BU_GET(n->vals->reg_name, struct bu_vls);
    BU_GET(n->vals->ov_reg1_name, struct bu_vls);
    BU_GET(n->vals->ov_reg2_name, struct bu_vls);
    BU_GET(n->vals->ov_sol_in, struct bu_vls);
    BU_GET(n->vals->ov_sol_out, struct bu_vls);
    BU_GET(n->vals->claimant_list, struct bu_vls);
    BU_GET(n->vals->claimant_listn, struct bu_vls);
    BU_GET(n->vals->attributes, struct bu_vls);

    VSETALL(n->vals->orig, 0.0);
    n->vals->h = 0.0;
    n->vals->v = 0.0;
    n->vals->d_orig = 0.0;

    bu_vls_init(n->vals->path_name);
    bu_vls_init(n->vals->reg_name);
    bu_vls_init(n->vals->ov_reg1_name);
    bu_vls_init(n->vals->ov_reg2_name);
    bu_vls_init(n->vals->ov_sol_in);
    bu_vls_init(n->vals->ov_sol_out);
    bu_vls_init(n->vals->claimant_list);
    bu_vls_init(n->vals->claimant_listn);
    bu_vls_init(n->vals->attributes);

    /* Populate the output key and type information */
    {
	std::string s(ovals);
	std::string entry, key;
	std::stringstream ss(s);
	int place = 0;
	while (std::getline(ss, entry, ',')) {
	    std::string tentry;
	    size_t sb = entry.find_first_not_of(" \t");
	    size_t se = entry.find_last_not_of(" \t");
	    if (sb != std::string::npos && se != std::string::npos) {
		tentry = entry.substr(sb, se - sb + 1);
	    }
	    if (!place) { key = tentry; place++; continue; }
	    if (place == 1) {
		bu_avs_add(n->val_types, key.c_str(), tentry.c_str());
		place++;
		continue;
	    }
	    if (place == 2) {
		if (!tentry.empty()) {
		    bu_avs_add(n->val_docs, key.c_str(), tentry.c_str());
		}
		place = 0;
	    }
	}
    }

    n->u_data = NULL;

    (*ns) = n;
    return 0;
}

extern "C" int
nirt_init(NIRT *ns, struct db_i *dbip)
{
    if (!ns || !dbip) return -1;

    RT_CK_DBI(dbip);

    ns->dbip = dbip;

    /* initialize the application structure */
    RT_APPLICATION_INIT(ns->ap);
    ns->ap->a_hit = nirt_if_hit;        /* branch to if_hit routine */
    ns->ap->a_miss = nirt_if_miss;      /* branch to if_miss routine */
    ns->ap->a_overlap = nirt_if_overlap;/* branch to if_overlap routine */
    ns->ap->a_logoverlap = rt_silent_logoverlap;
    ns->ap->a_onehit = 0;               /* continue through shotline after hit */
    ns->ap->a_purpose = "NIRT ray";
    ns->ap->a_rt_i = get_rtip(ns);         /* rt_i pointer */
    ns->ap->a_resource = get_resource(ns); /* note: resource is initialized by get_rtip */
    ns->ap->a_zero1 = 0;           /* sanity check, sayth raytrace.h */
    ns->ap->a_zero2 = 0;           /* sanity check, sayth raytrace.h */
    ns->ap->a_uptr = (void *)ns;

    /* By default use the .g units */
    ns->base2local = dbip->dbi_base2local;
    ns->local2base = dbip->dbi_local2base;

    /* Initial direction is -x */
    VSET(ns->vals->dir, -1, 0, 0);

    /* Set up the default NIRT formatting output */
    std::ifstream fs;
    std::string fmtline;
    fs.open(bu_brlcad_data("nirt/default.nrt", 0), std::fstream::binary);
    if (!fs.is_open()) {
	lerr(ns, "Error - could not load default NIRT formatting file: %s\n", bu_brlcad_data("nirt/default.nrt", 0));
	return -1;
    }
    while (std::getline(fs, fmtline)) {
	if (nirt_exec(ns, fmtline.c_str()) < 0) {
	    lerr(ns, "Error - could not load default NIRT formatting file: %s\n", bu_brlcad_data("nirt/default.nrt", 0));
	    return -1;
	}
    }

    fs.close();

    return 0;
}

void
nirt_destroy(NIRT *ns)
{
    if (!ns) return;
    bu_vls_free(ns->err);
    bu_vls_free(ns->out);
    bn_vlist_cleanup(&(ns->s_vlist));
    bn_vlblock_free(ns->segs);

    if (ns->rtip != RTI_NULL) rt_free_rti(ns->rtip);
    if (ns->rtip_air != RTI_NULL) rt_free_rti(ns->rtip_air);

    bu_vls_free(ns->vals->path_name);
    bu_vls_free(ns->vals->reg_name);
    bu_vls_free(ns->vals->ov_reg1_name);
    bu_vls_free(ns->vals->ov_reg2_name);
    bu_vls_free(ns->vals->ov_sol_in);
    bu_vls_free(ns->vals->ov_sol_out);
    bu_vls_free(ns->vals->claimant_list);
    bu_vls_free(ns->vals->claimant_listn);
    bu_vls_free(ns->vals->attributes);

    BU_PUT(ns->vals->path_name, struct bu_vls);
    BU_PUT(ns->vals->reg_name, struct bu_vls);
    BU_PUT(ns->vals->ov_reg1_name, struct bu_vls);
    BU_PUT(ns->vals->ov_reg2_name, struct bu_vls);
    BU_PUT(ns->vals->ov_sol_in, struct bu_vls);
    BU_PUT(ns->vals->ov_sol_out, struct bu_vls);
    BU_PUT(ns->vals->claimant_list, struct bu_vls);
    BU_PUT(ns->vals->claimant_listn, struct bu_vls);
    BU_PUT(ns->vals->attributes, struct bu_vls);
    BU_PUT(ns->vals, struct nout_record);

    BU_PUT(ns->res, struct resource);
    BU_PUT(ns->res_air, struct resource);
    BU_PUT(ns->err, struct bu_vls);
    BU_PUT(ns->out, struct bu_vls);
    BU_PUT(ns->ap, struct application);
    BU_PUT(ns->hit_color, struct bu_color);
    BU_PUT(ns->miss_color, struct bu_color);
    BU_PUT(ns->overlap_color, struct bu_color);
    delete ns;
}

/* Note - defined inside the execute-once do-while loop to allow for putting a
 * semicolon at the end of a NIRT_HOOK() statement (makes code formatters
 * happier) */
#define NIRT_HOOK(btype,htype) \
    do { if (ns->btype && ns->htype){(*ns->htype)(ns, ns->u_data);} } \
    while (0)

int
nirt_exec(NIRT *ns, const char *script)
{
    ns->ret = 0;
    ns->b_state = false;
    ns->b_segs = false;
    ns->b_objs = false;
    ns->b_frmts = false;
    ns->b_view = false;

    /* Unless told otherwise, clear the textual outputs
     * before each nirt_exec call. */
    if (!ns->out_accumulate) bu_vls_trunc(ns->out, 0);
    if (!ns->err_accumulate) bu_vls_trunc(ns->err, 0);

    if (script) {
	ns->ret = nirt_parse_script(ns, script, &nirt_exec_cmd);
    } else {
	return 0;
    }

    NIRT_HOOK(b_state, h_state);
    NIRT_HOOK(b_segs, h_segs);
    NIRT_HOOK(b_objs, h_objs);
    NIRT_HOOK(b_frmts, h_frmts);
    NIRT_HOOK(b_view, h_view);

    return ns->ret;
}

void *
nirt_udata(NIRT *ns, void *u_data)
{
    if (!ns) return NULL;
    if (u_data) ns->u_data = u_data;
    return ns->u_data;
}

void
nirt_hook(NIRT *ns, nirt_hook_t hf, int flag)
{
    if (!ns || !hf) return;
    switch (flag) {
	case NIRT_ALL:
	    ns->h_state = hf;
	    break;
	case NIRT_OUT:
	    ns->h_out = hf;
	    break;
	case NIRT_ERR:
	    ns->h_err = hf;
	    break;
	case NIRT_SEGS:
	    ns->h_segs = hf;
	    break;
	case NIRT_OBJS:
	    ns->h_objs = hf;
	    break;
	case NIRT_FRMTS:
	    ns->h_frmts = hf;
	    break;
	case NIRT_VIEW:
	    ns->h_view = hf;
	    break;
	default:
	    return;
    }
}

#define NCFC(nf) ((flags & nf) && !all_set) || (all_set &&  !((flags & nf)))

void
nirt_clear(NIRT *ns, int flags)
{
    int all_set;
    if (!ns) return;
    all_set = (flags & NIRT_ALL) ? 1 : 0;

    if (NCFC(NIRT_OUT)) {
	bu_vls_trunc(ns->out, 0);
    }

    if (NCFC(NIRT_ERR)) {
	bu_vls_trunc(ns->err, 0);
    }

    if (NCFC(NIRT_SEGS)) {
    }

    if (NCFC(NIRT_OBJS)) {
    }

    if (NCFC(NIRT_FRMTS)) {
    }

    if (NCFC(NIRT_VIEW)) {
    }

    /* Standard outputs handled - now, if NIRT_ALL was
     * supplied, clear any internal state. */
    if (flags == NIRT_ALL) {
    }

}

void
nirt_log(struct bu_vls *o, NIRT *ns, int output_type)
{
    if (!o || !ns) return;
    switch (output_type) {
	case NIRT_ALL:
	    bu_vls_sprintf(o, "%s", "NIRT_ALL: TODO\n");
	    break;
	case NIRT_OUT:
	    bu_vls_strcpy(o, bu_vls_addr(ns->out));
	    break;
	case NIRT_ERR:
	    bu_vls_strcpy(o, bu_vls_addr(ns->err));
	    break;
	case NIRT_SEGS:
	    bu_vls_sprintf(o, "%s", "NIRT_SEGS: TODO\n");
	    break;
	case NIRT_OBJS:
	    bu_vls_sprintf(o, "%s", "NIRT_OBJS: TODO\n");
	    break;
	case NIRT_FRMTS:
	    bu_vls_sprintf(o, "%s", "NIRT_FRMTS: TODO\n");
	    break;
	case NIRT_VIEW:
	    bu_vls_sprintf(o, "%s", "NIRT_VIEW: TODO\n");
	    break;
	default:
	    return;
    }

}

int
nirt_help(struct bu_vls *h, NIRT *ns,  bu_opt_format_t UNUSED(output_type))
{
    if (!h || !ns) return -1;
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
