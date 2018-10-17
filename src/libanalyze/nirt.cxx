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

/* needed on mac in c90 mode */
#ifndef HAVE_DECL_FSEEKO
extern "C" int fseeko(FILE *, off_t, int);
extern "C" off_t ftello(FILE *);
#endif
#include <fstream>
#include <iomanip>
#include <set>
#include <vector>
#include <limits>

//#define NIRT_USE_UNICODE 1
#ifdef NIRT_USE_UNICODE
#  define delta_str "Δ"
#else
#  define delta_str "Delta"
#endif

#include "bu/app.h"
#include "bu/color.h"
#include "bu/cmd.h"
#include "bu/malloc.h"
#include "bu/path.h"
#include "bu/units.h"
#include "bu/str.h"
#include "bu/vls.h"
#include "analyze.h"


/* NIRT segment types */
#define NIRT_MISS_SEG      1    /**< @brief Ray segment representing a miss */
#define NIRT_PARTITION_SEG 2    /**< @brief Ray segment representing a solid region */
#define NIRT_OVERLAP_SEG   3    /**< @brief Ray segment representing an overlap region */
#define NIRT_GAP_SEG       4    /**< @brief Ray segment representing a gap */
#define NIRT_ALL_SEG       5    /**< @brief Enable all values (for print) */

#define NIRT_SILENT_UNSET    0
#define NIRT_SILENT_YES      1
#define NIRT_SILENT_NO       -1

// Used by front end applications, but library needs to store and report as well.
#define NIRT_DEBUG_INTERACT  0x001
#define NIRT_DEBUG_SCRIPTS   0x002
#define NIRT_DEBUG_MAT       0x004

#define NIRT_DEBUG_BACKOUT   0x008
#define NIRT_DEBUG_HITS      0x010
#define NIRT_DEBUG_FMT        "\020\5HITS\4BACKOUT\3MAT\2SCRIPTS\1INTERACT"

#define NIRT_OVLP_RESOLVE            0
#define NIRT_OVLP_REBUILD_FASTGEN    1
#define NIRT_OVLP_REBUILD_ALL        2
#define NIRT_OVLP_RETAIN             3

#define NIRT_PRINTF_SPECIFIERS "difeEgGs"
#define NIRT_OUTPUT_TYPE_SPECIFIERS "rhpfmog"

HIDDEN const char *nirt_cmd_tbl_defs =
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


struct nirt_overlap {
    struct application *ap;
    struct partition *pp;
    struct region *reg1;
    struct region *reg2;
    fastf_t in_dist;
    fastf_t out_dist;
    point_t in_point;
    point_t out_point;
    struct nirt_overlap *forw;
    struct nirt_overlap *backw;
};
#define NIRT_OVERLAP_NULL ((struct nirt_overlap *)0)

struct nirt_seg {
    int type;
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
};

void
_nirt_seg_init(struct nirt_seg **s)
{
    if (!s) return;

    BU_GET(*s, struct nirt_seg);
    (*s)->type = 0;
    BU_GET((*s)->path_name, struct bu_vls);
    BU_GET((*s)->reg_name, struct bu_vls);
    BU_GET((*s)->ov_reg1_name, struct bu_vls);
    BU_GET((*s)->ov_reg2_name, struct bu_vls);
    BU_GET((*s)->ov_sol_in, struct bu_vls);
    BU_GET((*s)->ov_sol_out, struct bu_vls);
    BU_GET((*s)->claimant_list, struct bu_vls);
    BU_GET((*s)->claimant_listn, struct bu_vls);
    BU_GET((*s)->attributes, struct bu_vls);
    bu_vls_init((*s)->path_name);
    bu_vls_init((*s)->reg_name);
    bu_vls_init((*s)->ov_reg1_name);
    bu_vls_init((*s)->ov_reg2_name);
    bu_vls_init((*s)->ov_sol_in);
    bu_vls_init((*s)->ov_sol_out);
    bu_vls_init((*s)->claimant_list);
    bu_vls_init((*s)->claimant_listn);
    bu_vls_init((*s)->attributes);
}

void
_nirt_seg_free(struct nirt_seg *s)
{
    bu_vls_free(s->path_name);
    bu_vls_free(s->reg_name);
    bu_vls_free(s->ov_reg1_name);
    bu_vls_free(s->ov_reg2_name);
    bu_vls_free(s->ov_sol_in);
    bu_vls_free(s->ov_sol_out);
    bu_vls_free(s->claimant_list);
    bu_vls_free(s->claimant_listn);
    bu_vls_free(s->attributes);

    BU_PUT(s->path_name, struct bu_vls);
    BU_PUT(s->reg_name, struct bu_vls);
    BU_PUT(s->ov_reg1_name, struct bu_vls);
    BU_PUT(s->ov_reg2_name, struct bu_vls);
    BU_PUT(s->ov_sol_in, struct bu_vls);
    BU_PUT(s->ov_sol_out, struct bu_vls);
    BU_PUT(s->claimant_list, struct bu_vls);
    BU_PUT(s->claimant_listn, struct bu_vls);
    BU_PUT(s->attributes, struct bu_vls);
    BU_PUT(s, struct nirt_seg);
}

struct nirt_seg *
_nirt_seg_cpy(struct nirt_seg *s)
{
    struct nirt_seg *n = NULL;
    if (!s) return NULL;

    _nirt_seg_init(&n);

    n->type = s->type;
    VMOVE(n->in, s->in);
    n->d_in = s->d_in;
    VMOVE(n->out, s->out);
    n->d_out = s->d_out;
    n->los = s->los;
    n->scaled_los = s->scaled_los;
    bu_vls_sprintf(n->path_name, "%s", bu_vls_addr(s->path_name));
    bu_vls_sprintf(n->reg_name, "%s", bu_vls_addr(s->reg_name));
    n->reg_id = s->reg_id;
    n->obliq_in = s->obliq_in;
    n->obliq_out = s->obliq_out;
    VMOVE(n->nm_in, s->nm_in);
    n->nm_d_in = s->nm_d_in;
    n->nm_h_in = s->nm_h_in;
    n->nm_v_in = s->nm_v_in;
    VMOVE(n->nm_out, s->nm_out);
    n->nm_d_out = s->nm_d_out;
    n->nm_h_out = s->nm_h_out;
    n->nm_v_out = s->nm_v_out;
    bu_vls_sprintf(n->ov_reg1_name, "%s", bu_vls_addr(s->ov_reg1_name));
    n->ov_reg1_id = s->ov_reg1_id;
    bu_vls_sprintf(n->ov_reg2_name, "%s", bu_vls_addr(s->ov_reg2_name));
    n->ov_reg2_id = s->ov_reg2_id;
    bu_vls_sprintf(n->ov_sol_in, "%s", bu_vls_addr(s->ov_sol_in));
    bu_vls_sprintf(n->ov_sol_out, "%s", bu_vls_addr(s->ov_sol_out));
    n->ov_los = s->ov_los;
    VMOVE(n->ov_in, s->ov_in);
    n->ov_d_in = s->ov_d_in;
    VMOVE(n->ov_out, s->ov_out);
    n->ov_d_out = s->ov_d_out;
    n->surf_num_in = s->surf_num_in;
    n->surf_num_out = s->surf_num_out;
    n->claimant_count = s->claimant_count;
    bu_vls_sprintf(n->claimant_list, "%s", bu_vls_addr(s->claimant_list));
    bu_vls_sprintf(n->claimant_listn, "%s", bu_vls_addr(s->claimant_listn));
    bu_vls_sprintf(n->attributes, "%s", bu_vls_addr(s->attributes));
    VMOVE(n->gap_in, s->gap_in);
    n->gap_los = s->gap_los;
    return n;
}


/* This record structure doesn't have to correspond exactly to the above list
 * of available values, but it needs to retain sufficient information to
 * support the ability to generate all of them upon request. */
struct nirt_output_record {
    point_t orig;
    fastf_t h;
    fastf_t v;
    fastf_t d_orig;
    vect_t dir;
    fastf_t a;
    fastf_t e;
    struct nirt_overlap ovlp_list;
    struct nirt_seg *seg;
};


/**
 * Design thoughts for nirt diffing:
 *
 * NIRT difference events come in two primary forms: "transition" differences
 * in the form of entry/exit hits, and differences in segments - the regions
 * between transitions.  In the context of a diff, a segment by definition
 * contains no transitions on *either* shotline path.  Even if a segment
 * contains no transitions along its own shotline, if a transition from the
 * other shot falls within its bounds the original segment will be treated as
 * two sequential segments of the same type for the purposes of comparison.
 *
 * Transitions are compared only to other transitions, and segments are
 * compared only to other segments.
 *
 * The comparison criteria are as follows:
 *
 * Transition points:
 *
 * 1.  DIST_PT_PT - if there is a transition on either path that does not align
 *     within the specified distance tolerance with a transition on the other
 *     path, the transition is unmatched and reported as a difference.
 * 2.  Obliquity delta - if two transition points align within the distance
 *     tolerance, the next test is the difference between their normals. If
 *     these match in direction and obliquity angle, the transition points
 *     are deemed to be matching.  Otherwise, a difference is reported on the
 *     transition point.
 *
 * Segments:
 *
 * 1.  The first comparison made between segments is their type. Type
 *     differences will always be reported as a difference.
 *
 * 2.  If types match, behavior will depend on options and the specific
 *     types being compared:
 *
 *     GAPS:       always match.
 *
 *     PARTITIONS: Match if all active criteria match.  If no criteria
 *                 are active, match.  Possible active criteria:
 *
 *                 Region name
 *                 Path name
 *                 Region ID
 *
 *     OVERLAPS:   Match if all active criteria match.  If no criteria
 *                 are active, match.  Possible active criteria:
 *                 
 *                 Overlap region set
 *                 Overlap path set
 *                 Overlap region id set
 *                 Selected "winning" partition in the overlap
 */


struct nirt_seg_diff {
    struct nirt_seg *left;
    struct nirt_seg *right;
    fastf_t in_delta;
    fastf_t out_delta;
    fastf_t los_delta;
    fastf_t scaled_los_delta;
    fastf_t obliq_in_delta;
    fastf_t obliq_out_delta;
    fastf_t ov_in_delta;
    fastf_t ov_out_delta;
    fastf_t ov_los_delta;
    fastf_t gap_in_delta;
    fastf_t gap_los_delta;
};

struct nirt_diff {
    point_t orig;
    vect_t dir;
    std::vector<struct nirt_seg *> old_segs;
    std::vector<struct nirt_seg *> new_segs;
    std::vector<struct nirt_seg_diff *> diffs;
};

struct nirt_diff_settings {
    int report_partitions;
    int report_misses;
    int report_gaps;
    int report_overlaps;
    int report_partition_reg_ids;
    int report_partition_reg_names;
    int report_partition_path_names;
    int report_partition_dists;
    int report_partition_obliq;
    int report_overlap_reg_names;
    int report_overlap_reg_ids;
    int report_overlap_dists;
    int report_overlap_obliq;
    int report_gap_dists;
    fastf_t dist_delta_tol;
    fastf_t obliq_delta_tol;
    fastf_t los_delta_tol;
    fastf_t scaled_los_delta_tol;
};

struct nirt_state_impl {
    /* Output options */
    struct bu_color *hit_odd_color;
    struct bu_color *hit_even_color;
    struct bu_color *void_color;
    struct bu_color *overlap_color;
    int print_header;
    int print_ident_flag;
    unsigned int rt_debug;
    unsigned int nirt_debug;

    /* Output */
    struct bu_vls *out;
    int out_accumulate;
    struct bu_vls *msg;
    struct bu_vls *err;
    int err_accumulate;
    struct bu_list s_vlist; /* used by the segs vlblock */
    struct bn_vlblock *segs;
    int plot_overlaps;
    //TODO - int segs_accumulate;
    int ret;  // return code to be returned by nirt_exec after execution

    /* Callbacks */
    nirt_hook_t h_state;   // any state change
    nirt_hook_t h_out;     // output
    nirt_hook_t h_msg;     // messages (not errors) not part of command output
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
    struct nirt_output_record *vals;
    struct bu_vls *diff_file;
    struct nirt_diff *cdiff;
    std::vector<struct nirt_diff *> diffs;
    struct nirt_diff_settings *diff_settings;
    int diff_run;
    int diff_ready;

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
    int need_reprep;

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

HIDDEN void nmsg(struct nirt_state *nss, const char *fmt, ...) _BU_ATTR_PRINTF23;
HIDDEN void nmsg(struct nirt_state *nss, const char *fmt, ...)
{
    va_list ap;
    if (nss->i->h_msg) {
	struct bu_vls *vls = nss->i->msg;
	BU_CK_VLS(vls);
	va_start(ap, fmt);
	bu_vls_vprintf(vls, fmt, ap);
	(*nss->i->h_msg)(nss, nss->i->u_data);
	bu_vls_trunc(vls, 0);
    }
    va_end(ap);
}

HIDDEN void nout(struct nirt_state *nss, const char *fmt, ...) _BU_ATTR_PRINTF23;
HIDDEN void nout(struct nirt_state *nss, const char *fmt, ...)
{
    va_list ap;
    if (nss->i->h_out) {
	struct bu_vls *vls = nss->i->out;
	BU_CK_VLS(vls);
	va_start(ap, fmt);
	bu_vls_vprintf(vls, fmt, ap);
	(*nss->i->h_out)(nss, nss->i->u_data);
	bu_vls_trunc(vls, 0);
    }
    va_end(ap);
}

HIDDEN void nerr(struct nirt_state *nss, const char *fmt, ...) _BU_ATTR_PRINTF23;
HIDDEN void nerr(struct nirt_state *nss, const char *fmt, ...)
{
    va_list ap;
    if (nss->i->h_err){
	struct bu_vls *vls = nss->i->err;
	BU_CK_VLS(vls);
	va_start(ap, fmt);
	bu_vls_vprintf(vls, fmt, ap);
	(*nss->i->h_err)(nss, nss->i->u_data);
	bu_vls_trunc(vls, 0);
    }
    va_end(ap);
}

HIDDEN void ndbg(struct nirt_state *nss, int flag, const char *fmt, ...)
{
    va_list ap;
    if ((nss->i->nirt_debug & flag) && nss->i->h_err) {
	struct bu_vls *vls = nss->i->out;
	BU_CK_VLS(vls);
	va_start(ap, fmt);
	bu_vls_vprintf(vls, fmt, ap);
	(*nss->i->h_err)(nss, nss->i->u_data);
	bu_vls_trunc(vls, 0);
    }
    va_end(ap);
}

HIDDEN size_t
_nirt_find_first_unescaped(std::string &s, const char *keys, int offset)
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
_nirt_find_first_unquoted(std::string &ts, const char *key, size_t offset)
{
    size_t q_start, q_end, pos;
    std::string s = ts;
    /* Start by initializing the position markers for quoted substrings. */
    q_start = _nirt_find_first_unescaped(s, "\"", offset);
    q_end = (q_start != std::string::npos) ? _nirt_find_first_unescaped(s, "\"", q_start + 1) : std::string::npos;

    pos = offset;
    while ((pos = s.find(key, pos)) != std::string::npos) {
	/* If we're inside matched quotes, only an un-escaped quote char means
	 * anything */
	if (q_end != std::string::npos && pos > q_start && pos < q_end) {
	    pos = q_end + 1;
	    q_start = _nirt_find_first_unescaped(s, "\"", q_end + 1);
	    q_end = (q_start != std::string::npos) ? _nirt_find_first_unescaped(s, "'\"", q_start + 1) : std::string::npos;
	    continue;
	}
	break;
    }
    return pos;
}

HIDDEN void
_nirt_trim_whitespace(std::string &s)
{
    size_t ep = s.find_last_not_of(" \t\n\v\f\r");
    size_t sp = s.find_first_not_of(" \t\n\v\f\r");
    if (sp == std::string::npos) {
	s.clear();
	return;
    }
    s = s.substr(sp, ep-sp+1);
}

HIDDEN std::vector<std::string>
_nirt_string_split(std::string s)
{
    std::vector<std::string> substrs;
    std::string lstr = s;
    size_t pos = 0;
    while ((pos = _nirt_find_first_unquoted(lstr, ",", 0)) != std::string::npos) {
	std::string ss = lstr.substr(0, pos);
	_nirt_trim_whitespace(ss);
	substrs.push_back(ss);
	lstr.erase(0, pos + 1);
    }
    substrs.push_back(lstr);
    return substrs;
}

/* Based on tolerance, how many digits should we print? */
HIDDEN int
_nirt_digits(fastf_t ftol)
{
    int tol = 1;
    fastf_t tt = ftol;
    if (tt < SMALL_FASTF) return 1;
    while (tt < 1 && tol < 17) {
	tol++;
	tt = tt * 10;
    }
    return tol;
}

/*
 * References for string to-from floating point issues:
 * https://stackoverflow.com/a/22458961
 * http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2006/n2005.pdf
 * https://docs.oracle.com/cd/E19957-01/806-3568/ncg_goldberg.html
 */

HIDDEN std::string
_nirt_dbl_to_str(double d, size_t p)
{
    size_t prec = (p) ? p : std::numeric_limits<double>::max_digits10;
    bu_log("prec: %zu\n",prec);
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(prec) << d;
    std::string sd = ss.str();
    bu_log("sd: %s\n", sd.c_str());
    return sd;
}

HIDDEN double
_nirt_str_to_dbl(std::string s, size_t p)
{
    double d;
    size_t prec = (p) ? p : std::numeric_limits<double>::max_digits10;
    std::stringstream ss(s);
    ss >> std::setprecision(prec) >> std::fixed >> d;
    //bu_log("str   : %s\ndbl   : %.17f\n", s.c_str(), d);
    return d;
}

HIDDEN int
_nirt_str_to_int(std::string s)
{
    int i;
    std::stringstream ss(s);
    ss >> i;
    return i;
}

/********************************
 * Conversions and Calculations *
 ********************************/

HIDDEN fastf_t
d_calc(struct nirt_state *nss, point_t p)
{
    fastf_t ar = nss->i->vals->a * DEG2RAD;
    fastf_t er = nss->i->vals->e * DEG2RAD;
    return p[X] * cos(er) * cos(ar) + p[Y] * cos(er) * sin(ar) + p[Z] * sin(er);
}

HIDDEN fastf_t
h_calc(struct nirt_state *nss, point_t p)
{
    fastf_t ar = nss->i->vals->a * DEG2RAD;
    return p[X] * (-sin(ar)) + p[Y] * cos(ar);
}

HIDDEN fastf_t
v_calc(struct nirt_state *nss, point_t p)
{
    fastf_t ar = nss->i->vals->a * DEG2RAD;
    fastf_t er = nss->i->vals->e * DEG2RAD;
    return p[X] * (-sin(er)) * cos(ar) + p[Y] * (-sin(er)) * sin(ar) + p[Z] * cos(er);
}

HIDDEN void _nirt_grid2targ(struct nirt_state *nss)
{
    double ar = nss->i->vals->a * DEG2RAD;
    double er = nss->i->vals->e * DEG2RAD;
    nss->i->vals->orig[X] = - nss->i->vals->h * sin(ar) - nss->i->vals->v * cos(ar) * sin(er) + nss->i->vals->d_orig * cos(ar) * cos(er);
    nss->i->vals->orig[Y] =   nss->i->vals->h * cos(ar) - nss->i->vals->v * sin(ar) * sin(er) + nss->i->vals->d_orig * sin(ar) * cos(er);
    nss->i->vals->orig[Z] =   nss->i->vals->v * cos(er) + nss->i->vals->d_orig * sin(er);
}

HIDDEN void _nirt_targ2grid(struct nirt_state *nss)
{
    double ar = nss->i->vals->a * DEG2RAD;
    double er = nss->i->vals->e * DEG2RAD;
    nss->i->vals->h = - nss->i->vals->orig[X] * sin(ar) + nss->i->vals->orig[Y] * cos(ar);
    nss->i->vals->v = - nss->i->vals->orig[X] * cos(ar) * sin(er) - nss->i->vals->orig[Y] * sin(er) * sin(ar) + nss->i->vals->orig[Z] * cos(er);
    nss->i->vals->d_orig =   nss->i->vals->orig[X] * cos(er) * cos(ar) + nss->i->vals->orig[Y] * cos(er) * sin(ar) + nss->i->vals->orig[Z] * sin(er);
}

HIDDEN void _nirt_dir2ae(struct nirt_state *nss)
{
    int zeroes = ZERO(nss->i->vals->dir[Y]) && ZERO(nss->i->vals->dir[X]);
    double square = sqrt(nss->i->vals->dir[X] * nss->i->vals->dir[X] + nss->i->vals->dir[Y] * nss->i->vals->dir[Y]);

    nss->i->vals->a = zeroes ? 0.0 : atan2 (-(nss->i->vals->dir[Y]), -(nss->i->vals->dir[X])) / DEG2RAD;
    nss->i->vals->e = atan2(-(nss->i->vals->dir[Z]), square) / DEG2RAD;
}

HIDDEN void _nirt_ae2dir(struct nirt_state *nss)
{
    vect_t dir;
    double ar = nss->i->vals->a * DEG2RAD;
    double er = nss->i->vals->e * DEG2RAD;

    dir[X] = -cos(ar) * cos(er);
    dir[Y] = -sin(ar) * cos(er);
    dir[Z] = -sin(er);
    VUNITIZE(dir);
    VMOVE(nss->i->vals->dir, dir);
}

HIDDEN double _nirt_backout(struct nirt_state *nss)
{
    double bov;
    point_t ray_point;
    vect_t diag, dvec, ray_dir, center_bsphere;
    fastf_t bsphere_diameter, dist_to_target, delta;

    if (!nss || !nss->i->backout) return 0.0;

    VMOVE(ray_point, nss->i->vals->orig);
    VMOVE(ray_dir, nss->i->vals->dir);

    VSUB2(diag, nss->i->ap->a_rt_i->mdl_max, nss->i->ap->a_rt_i->mdl_min);
    bsphere_diameter = MAGNITUDE(diag);

    /*
     * calculate the distance from a plane normal to the ray direction through the center of
     * the bounding sphere and a plane normal to the ray direction through the aim point.
     */
    VADD2SCALE(center_bsphere, nss->i->ap->a_rt_i->mdl_max, nss->i->ap->a_rt_i->mdl_min, 0.5);

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

HIDDEN fastf_t
_nirt_get_obliq(fastf_t *ray, fastf_t *normal)
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

/**********************
 * Diff functionality *
 **********************/

#define SD_INIT(sd, l, r) \
    do {BU_GET(sd, struct nirt_seg_diff); sd->left = left; sd->right = right;} \
    while (0)

#define SD_FREE(sd) \
    do {BU_PUT(sd, struct nirt_seg_diff);} \
    while (0)


HIDDEN struct nirt_seg_diff *
_nirt_partition_diff(struct nirt_state *nss, struct nirt_seg *left, struct nirt_seg *right)
{
    int have_diff = 0;
    struct nirt_seg_diff *sd;
    if (!nss || !nss->i->diff_settings) return NULL;
    fastf_t in_delta = DIST_PT_PT(left->in, right->in);
    fastf_t out_delta = DIST_PT_PT(left->in, right->in);
    fastf_t los_delta = fabs(left->los - right->los);
    fastf_t scaled_los_delta = fabs(left->scaled_los - right->scaled_los);
    fastf_t obliq_in_delta = fabs(left->obliq_in - right->obliq_in);
    fastf_t obliq_out_delta = fabs(left->obliq_out - right->obliq_out);
    if (bu_vls_strcmp(left->reg_name, right->reg_name)) have_diff = 1;
    if (bu_vls_strcmp(left->path_name, right->path_name)) have_diff = 1;
    if (left->reg_id != right->reg_id) have_diff = 1;
    if (in_delta > nss->i->diff_settings->dist_delta_tol) have_diff = 1;
    if (out_delta > nss->i->diff_settings->dist_delta_tol) have_diff = 1;
    if (los_delta > nss->i->diff_settings->los_delta_tol) have_diff = 1;
    if (scaled_los_delta > nss->i->diff_settings->scaled_los_delta_tol) have_diff = 1;
    if (obliq_in_delta > nss->i->diff_settings->obliq_delta_tol) have_diff = 1;
    if (obliq_out_delta > nss->i->diff_settings->obliq_delta_tol) have_diff = 1;
    if (have_diff) {
	SD_INIT(sd, left, right);
	sd->in_delta = in_delta;
	sd->out_delta = out_delta;
	sd->los_delta = los_delta;
	sd->scaled_los_delta = scaled_los_delta;
	sd->obliq_in_delta = obliq_in_delta;
	sd->obliq_out_delta = obliq_out_delta;
	return sd;
    }
    return NULL; 
}

HIDDEN struct nirt_seg_diff *
_nirt_overlap_diff(struct nirt_state *nss, struct nirt_seg *left, struct nirt_seg *right)
{
    int have_diff = 0;
    struct nirt_seg_diff *sd;
    if (!nss || !nss->i->diff_settings) return NULL;
    fastf_t ov_in_delta = DIST_PT_PT(left->ov_in, right->ov_in);
    fastf_t ov_out_delta = DIST_PT_PT(left->ov_out, right->ov_out);
    fastf_t ov_los_delta = fabs(left->ov_los - right->ov_los);
    if (bu_vls_strcmp(left->ov_reg1_name, right->ov_reg1_name)) have_diff = 1;
    if (bu_vls_strcmp(left->ov_reg2_name, right->ov_reg2_name)) have_diff = 1;
    if (left->ov_reg1_id != right->ov_reg1_id) have_diff = 1;
    if (left->ov_reg2_id != right->ov_reg2_id) have_diff = 1;
    if (ov_in_delta > nss->i->diff_settings->dist_delta_tol) have_diff = 1;
    if (ov_out_delta > nss->i->diff_settings->dist_delta_tol) have_diff = 1;
    if (ov_los_delta > nss->i->diff_settings->los_delta_tol) have_diff = 1;
    if (have_diff) {
	SD_INIT(sd, left, right);
	sd->ov_in_delta = ov_in_delta;
	sd->ov_out_delta = ov_out_delta;
	sd->ov_los_delta = ov_los_delta;
	return sd;
    }
    return NULL; 
}

HIDDEN struct nirt_seg_diff *
_nirt_gap_diff(struct nirt_state *nss, struct nirt_seg *left, struct nirt_seg *right)
{
    int have_diff = 0;
    struct nirt_seg_diff *sd;
    if (!nss || !nss->i->diff_settings) return NULL;
    fastf_t gap_in_delta = DIST_PT_PT(left->gap_in, right->gap_in);
    fastf_t gap_los_delta = fabs(left->gap_los - right->gap_los);
    if (gap_in_delta > nss->i->diff_settings->dist_delta_tol) have_diff = 1;
    if (gap_los_delta > nss->i->diff_settings->los_delta_tol) have_diff = 1;
    if (have_diff) {
	SD_INIT(sd, left, right);
	sd->gap_in_delta = gap_in_delta;
	sd->gap_los_delta = gap_los_delta;
	return sd;
    }
    return 0;
}

HIDDEN struct nirt_seg_diff *
_nirt_segs_diff(struct nirt_state *nss, struct nirt_seg *left, struct nirt_seg *right)
{
    struct nirt_seg_diff *sd;
    if (!nss) return NULL;
    if (!left || !right || left->type != right->type) {
	/* Fundamental segment difference - no point going further, they're different */
	SD_INIT(sd, left, right);
	return sd;
    }
    switch(left->type) {
	case NIRT_MISS_SEG:
	    /* Types don't differ and they're both misses - we're good */
	    return NULL;
	case NIRT_PARTITION_SEG:
	    return _nirt_partition_diff(nss, left, right);
	case NIRT_GAP_SEG:
	    return _nirt_gap_diff(nss, left, right);
	case NIRT_OVERLAP_SEG:
	    return _nirt_overlap_diff(nss, left, right);
	default:
	    nerr(nss, "NIRT diff error: unknown segment type: %d\n", left->type);
	    return 0;
    }
}

HIDDEN const char *
_nirt_seg_string(int type) {
    static const char *p = "Partition";
    static const char *g = "Gap";
    static const char *o = "Overlap";
    static const char *m = "Miss";
    switch (type) {
	case NIRT_MISS_SEG:
	    return m;
	case NIRT_PARTITION_SEG:
	    return p;
	case NIRT_GAP_SEG:
	    return g;
	case NIRT_OVERLAP_SEG:
	    return o;
	default:
	    return NULL;
    }
}




HIDDEN int
_nirt_diff_report(struct nirt_state *nss)
 {
    int reporting_diff = 0;
    int did_header = 0;
    struct bu_vls dreport = BU_VLS_INIT_ZERO;

    if (!nss || nss->i->diffs.size() == 0) return 0;
    std::vector<struct nirt_diff *> *dfs = &(nss->i->diffs); 
    for (size_t i = 0; i < dfs->size(); i++) {
	struct nirt_diff *d = (*dfs)[i];
	// Calculate diffs according to settings.  TODO - need to be more sophisticated about this - if a
	// segment is "missing" but the rays otherwise match, don't propagate the "offset" and report all
	// of the subsequent segments as different...
	size_t seg_max = (d->old_segs.size() > d->new_segs.size()) ? d->new_segs.size() : d->old_segs.size();
	for (size_t j = 0; j < seg_max; j++) {
	    struct nirt_seg_diff *sd = _nirt_segs_diff(nss, d->old_segs[j], d->new_segs[j]);
	    if (sd) d->diffs.push_back(sd);
	}
	if (d->diffs.size() > 0 && !did_header) {
	    bu_vls_printf(&dreport, "Diff Results (%s):\n", bu_vls_addr(nss->i->diff_file));
	    did_header = 1;
	}

	if (d->diffs.size() > 0) bu_vls_printf(&dreport, "Found differences along Ray:\n  xyz %.17f %.17f %.17f\n  dir %.17f %.17f %.17f\n \n", V3ARGS(d->orig), V3ARGS(d->dir));
	for (size_t j = 0; j < d->diffs.size(); j++) {
	    struct nirt_seg_diff *sd = d->diffs[j];
	    struct nirt_seg *left = sd->left;
	    struct nirt_seg *right = sd->right;
	    if (left->type != right->type) {
		bu_vls_printf(&dreport, "  Segment %zu type mismatch : Original %s, Current %s\n", j, _nirt_seg_string(sd->left->type), _nirt_seg_string(sd->right->type));
		nout(nss, "%s", bu_vls_addr(&dreport));
		bu_vls_free(&dreport);
		return 1;
	    }
	    switch (sd->left->type) {
		case NIRT_MISS_SEG:
		    if (!nss->i->diff_settings->report_misses) continue;
		    /* TODO */
		    break;
		case NIRT_PARTITION_SEG:
		    if (!nss->i->diff_settings->report_partitions) continue;
		    bu_vls_printf(&dreport, "  Segment difference(%s)[%zu]:\n", _nirt_seg_string(sd->left->type), j);
		    if (bu_vls_strcmp(left->reg_name, right->reg_name) && nss->i->diff_settings->report_partition_reg_names) {
			bu_vls_printf(&dreport, "    Region Name: '%s' -> '%s'\n", bu_vls_addr(left->reg_name), bu_vls_addr(right->reg_name));
			reporting_diff = 1;
		    }
		    if (bu_vls_strcmp(left->path_name, right->path_name) && nss->i->diff_settings->report_partition_path_names) {
			bu_vls_printf(&dreport, "    Path Name: '%s' -> '%s'\n", bu_vls_addr(left->path_name), bu_vls_addr(right->path_name));
			reporting_diff = 1;
		    }
		    if (left->reg_id != right->reg_id && nss->i->diff_settings->report_partition_reg_ids) {
			bu_vls_printf(&dreport, "    Region ID: %d -> %d\n", left->reg_id, right->reg_id);
			reporting_diff = 1;
		    }
		    if (sd->in_delta > nss->i->diff_settings->dist_delta_tol && nss->i->diff_settings->report_partition_dists) {
			std::string oval = _nirt_dbl_to_str(sd->in_delta, _nirt_digits(nss->i->diff_settings->dist_delta_tol));
			bu_vls_printf(&dreport, "    DIST_PT_PT(in_old,in_new): %s\n", oval.c_str());
			reporting_diff = 1;
		    }
		    if (sd->out_delta > nss->i->diff_settings->dist_delta_tol && nss->i->diff_settings->report_partition_dists) {
			std::string oval = _nirt_dbl_to_str(sd->out_delta, _nirt_digits(nss->i->diff_settings->dist_delta_tol));
			bu_vls_printf(&dreport, "    DIST_PT_PT(out_old,out_new): %s\n", oval.c_str());
			reporting_diff = 1;
		    }
		    if (sd->los_delta > nss->i->diff_settings->los_delta_tol && nss->i->diff_settings->report_partition_dists) {
			std::string oval = _nirt_dbl_to_str(sd->los_delta, _nirt_digits(nss->i->diff_settings->los_delta_tol));
			bu_vls_printf(&dreport, "    Line-Of-Sight(LOS) %s: %s\n", delta_str, oval.c_str());
			reporting_diff = 1;
		    }
		    if (sd->scaled_los_delta > nss->i->diff_settings->scaled_los_delta_tol && nss->i->diff_settings->report_partition_dists) {
			std::string oval = _nirt_dbl_to_str(sd->scaled_los_delta, _nirt_digits(nss->i->diff_settings->los_delta_tol));
			bu_vls_printf(&dreport, "    Scaled Line-Of-Sight %s: %s\n", delta_str, oval.c_str());
			reporting_diff = 1;
		    }
		    if (sd->obliq_in_delta > nss->i->diff_settings->obliq_delta_tol && nss->i->diff_settings->report_partition_obliq) {
			std::string oval = _nirt_dbl_to_str(sd->obliq_in_delta, _nirt_digits(nss->i->diff_settings->obliq_delta_tol));
			bu_vls_printf(&dreport, "    Input Normal Obliquity %s: %s\n", delta_str, oval.c_str());
			reporting_diff = 1;
		    }
		    if (sd->obliq_out_delta > nss->i->diff_settings->obliq_delta_tol && nss->i->diff_settings->report_partition_obliq) {
			std::string oval = _nirt_dbl_to_str(sd->obliq_out_delta, _nirt_digits(nss->i->diff_settings->obliq_delta_tol));
			bu_vls_printf(&dreport, "    Output Normal Obliquity %s: %s\n", delta_str, oval.c_str());
			reporting_diff = 1;
		    }
		    break;
		case NIRT_GAP_SEG:
		    if (!nss->i->diff_settings->report_gaps) continue;
		    bu_vls_printf(&dreport, "  Segment difference(%s)[%zu]:\n", _nirt_seg_string(sd->left->type), j);
		    if (sd->gap_in_delta > nss->i->diff_settings->dist_delta_tol && nss->i->diff_settings->report_gap_dists) {
			std::string oval = _nirt_dbl_to_str(sd->gap_in_delta, _nirt_digits(nss->i->diff_settings->dist_delta_tol));
			bu_vls_printf(&dreport, "    DIST_PT_PT(gap_in_old,gap_in_new): %s\n", oval.c_str());
			reporting_diff = 1;
		    }
		    if (sd->gap_los_delta > nss->i->diff_settings->los_delta_tol && nss->i->diff_settings->report_gap_dists) {
			std::string oval = _nirt_dbl_to_str(sd->gap_los_delta, _nirt_digits(nss->i->diff_settings->los_delta_tol));
			bu_vls_printf(&dreport, "    Gap Line-Of-Sight (LOS) %s: %s\n", delta_str, oval.c_str());
			reporting_diff = 1;
		    }
		    break;
		case NIRT_OVERLAP_SEG:
		    if (!nss->i->diff_settings->report_overlaps) continue;
		    bu_vls_printf(&dreport, "  Segment difference(%s)[%zu]:\n", _nirt_seg_string(sd->left->type), j);
		    if (bu_vls_strcmp(left->ov_reg1_name, right->ov_reg1_name) && nss->i->diff_settings->report_overlap_reg_names) {
			bu_vls_printf(&dreport, "    Region 1 Name: '%s' -> '%s'\n", bu_vls_addr(left->ov_reg1_name), bu_vls_addr(right->ov_reg1_name));
			reporting_diff = 1;
		    }
		    if (bu_vls_strcmp(left->ov_reg2_name, right->ov_reg2_name) && nss->i->diff_settings->report_overlap_reg_names) {
			bu_vls_printf(&dreport, "    Region 2 Name: '%s' -> '%s'\n", bu_vls_addr(left->ov_reg2_name), bu_vls_addr(right->ov_reg2_name));
			reporting_diff = 1;
		    }
		    if (left->ov_reg1_id != right->ov_reg1_id && nss->i->diff_settings->report_overlap_reg_ids) {
			bu_vls_printf(&dreport, "    Region 1 ID: %d -> %d\n", left->ov_reg1_id, right->ov_reg1_id);
			reporting_diff = 1;
		    }
		    if (left->ov_reg2_id != right->ov_reg2_id && nss->i->diff_settings->report_overlap_reg_ids) {
			bu_vls_printf(&dreport, "    Region 2 ID: %d -> %d\n", left->ov_reg2_id, right->ov_reg2_id);
			reporting_diff = 1;
		    }
		    if (sd->ov_in_delta > nss->i->diff_settings->dist_delta_tol && nss->i->diff_settings->report_overlap_dists) {
			std::string oval = _nirt_dbl_to_str(sd->ov_in_delta, _nirt_digits(nss->i->diff_settings->dist_delta_tol));
			bu_vls_printf(&dreport, "    DIST_PT_PT(ov_in_old, ov_in_new): %s\n", oval.c_str());
			reporting_diff = 1;
		    }
		    if (sd->ov_out_delta > nss->i->diff_settings->dist_delta_tol && nss->i->diff_settings->report_overlap_dists) {
			std::string oval = _nirt_dbl_to_str(sd->ov_out_delta, _nirt_digits(nss->i->diff_settings->dist_delta_tol));
			bu_vls_printf(&dreport, "    DIST_PT_PT(ov_out_old, ov_out_new): %s\n", oval.c_str());
			reporting_diff = 1;
		    }
		    if (sd->ov_los_delta > nss->i->diff_settings->los_delta_tol && nss->i->diff_settings->report_overlap_dists) {
			std::string oval = _nirt_dbl_to_str(sd->ov_los_delta, _nirt_digits(nss->i->diff_settings->los_delta_tol));
			bu_vls_printf(&dreport, "    Overlap Line-Of-Sight (LOS) %s: %s\n", delta_str, oval.c_str());
			reporting_diff = 1;
		    }
		    break;
		default:
		    nerr(nss, "NIRT diff error: unknown segment type: %d\n", left->type);
		    return 0;
	    }
	} 
	for (size_t j = 0; j < d->diffs.size(); j++) {
	    SD_FREE(d->diffs[j]);
	}
	d->diffs.clear();
    }

    if (reporting_diff) {
	nout(nss, "%s", bu_vls_addr(&dreport));
    } else {
	nout(nss, "No differences found\n");
    }
    bu_vls_free(&dreport);

    return reporting_diff;
}

/********************
 * Raytracing setup *
 ********************/
HIDDEN struct rt_i *
_nirt_get_rtip(struct nirt_state *nss)
{
    if (!nss || nss->i->dbip == DBI_NULL || nss->i->active_paths.size() == 0) return NULL;

    if (nss->i->use_air) {
	if (nss->i->rtip_air == RTI_NULL) {
	    nss->i->rtip_air = rt_new_rti(nss->i->dbip); /* clones dbip, so we can operate on the copy */
	    nss->i->rtip_air->rti_dbip->dbi_fp = fopen(nss->i->dbip->dbi_filename, "rb"); /* get read-only fp */
	    if (nss->i->rtip_air->rti_dbip->dbi_fp == NULL) {
		rt_free_rti(nss->i->rtip_air);
		nss->i->rtip_air = RTI_NULL;
		return RTI_NULL;
	    }
	    nss->i->rtip_air->rti_dbip->dbi_read_only = 1;
	    rt_init_resource(nss->i->res_air, 0, nss->i->rtip_air);
	    nss->i->rtip_air->useair = 1;
	}
	return nss->i->rtip_air;
    }

    if (nss->i->rtip == RTI_NULL) {
	nss->i->rtip = rt_new_rti(nss->i->dbip); /* clones dbip, so we can operate on the copy */
	nss->i->rtip->rti_dbip->dbi_fp = fopen(nss->i->dbip->dbi_filename, "rb"); /* get read-only fp */
	if (nss->i->rtip->rti_dbip->dbi_fp == NULL) {
	    rt_free_rti(nss->i->rtip);
	    nss->i->rtip = RTI_NULL;
	    return RTI_NULL;
	}
	nss->i->rtip->rti_dbip->dbi_read_only = 1;
	rt_init_resource(nss->i->res, 0, nss->i->rtip);
    }
    return nss->i->rtip;
}

HIDDEN struct resource *
_nirt_get_resource(struct nirt_state *nss)
{
    if (!nss || nss->i->dbip == DBI_NULL || nss->i->active_paths.size() == 0) return NULL;
    if (nss->i->use_air) return nss->i->res_air;
    return nss->i->res;
}

HIDDEN int
_nirt_raytrace_prep(struct nirt_state *nss)
{
    int ocnt = 0;
    int acnt = 0;
    std::set<std::string>::iterator s_it;

    /* Based on current settings, pick the particular rtip */
    nss->i->ap->a_rt_i = _nirt_get_rtip(nss);
    nss->i->ap->a_resource = _nirt_get_resource(nss);

    /* Don't have enough info to prep yet - can happen if we're in a pre "nirt_init" state */
    if (!nss->i->ap->a_rt_i) return 0;

    // Prepare C-style arrays for rt prep
    const char **objs = (const char **)bu_calloc(nss->i->active_paths.size() + 1, sizeof(char *), "objs");
    const char **attrs = (const char **)bu_calloc(nss->i->attrs.size() + 1, sizeof(char *), "attrs");
    for (size_t ui = 0; ui < nss->i->active_paths.size(); ui++) {
	const char *o = nss->i->active_paths[ui].c_str();
	objs[ocnt] = o;
	ocnt++;
    }
    objs[ocnt] = NULL;
    for (s_it = nss->i->attrs.begin(); s_it != nss->i->attrs.end(); s_it++) {
	const char *a = (*s_it).c_str();
	attrs[acnt] = a;
	acnt++;
    }
    attrs[acnt] = NULL;
    nmsg(nss, "Get trees...\n");
    rt_clean(nss->i->ap->a_rt_i);
    if (rt_gettrees_and_attrs(nss->i->ap->a_rt_i, attrs, ocnt, objs, 1)) {
	nerr(nss, "rt_gettrees() failed\n");
	bu_free(objs, "objs");
	bu_free(attrs, "objs");
	return -1;
    }
    bu_free(objs, "objs");
    bu_free(attrs, "objs");

    nmsg(nss, "Prepping the geometry...\n");
    rt_prep(nss->i->ap->a_rt_i);
    nmsg(nss, "%s ", (nss->i->active_paths.size() == 1) ? "Object" : "Objects");
    for (size_t i = 0; i < nss->i->active_paths.size(); i++) {
	if (i == 0) {
	    nmsg(nss, "'%s'", nss->i->active_paths[i].c_str());
	} else {
	    nmsg(nss, " '%s'", nss->i->active_paths[i].c_str());
	}
    }
    nmsg(nss, " processed\n");
    nss->i->need_reprep = 0;
    return 0;
}

/*****************************************
 * Routines to support output formatting *
 *****************************************/

/* Count how many format specifiers we might find in a format string */
HIDDEN unsigned int
_nirt_fmt_sp_cnt(const char *fmt) {
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
HIDDEN int
_nirt_fmt_sp_flags_check(struct nirt_state *nss, std::string &fmt_sp)
{
    size_t sp = fmt_sp.find_first_of(NIRT_PRINTF_FLAGS);
    size_t ep = fmt_sp.find_last_of(NIRT_PRINTF_FLAGS);
    if (sp == std::string::npos) return 0; // no flags is acceptable
    std::string flags = fmt_sp.substr(sp, ep - sp + 1);
    size_t invalid_char = flags.find_first_not_of(NIRT_PRINTF_FLAGS);
    if (invalid_char != std::string::npos) {
	nerr(nss, "Error: invalid characters in flags substring (\"%s\") of format specifier \"%s\"\n", flags.c_str(), fmt_sp.c_str());
	return -1;
    }
    return 0;
}

/* Validate the precision specification portion of the format specifier against
 * the system limits */
#define NIRT_PRINTF_PRECISION "0123456789."
#define NIRT_PRINTF_MAXWIDTH 1000 //Arbitrary sanity boundary for width specification
HIDDEN int
_nirt_fmt_sp_width_precision_check(struct nirt_state *nss, std::string &fmt_sp)
{
    size_t sp = fmt_sp.find_first_of(NIRT_PRINTF_PRECISION);
    size_t ep = fmt_sp.find_last_of(NIRT_PRINTF_PRECISION);
    if (sp == std::string::npos) return 0; // no width/precision settings is acceptable
    std::string p = fmt_sp.substr(sp, ep - sp + 1);
    size_t invalid_char = p.find_first_not_of(NIRT_PRINTF_PRECISION);
    if (invalid_char != std::string::npos) {
	nerr(nss, "Error: invalid characters in precision substring (\"%s\") of format specifier \"%s\"\n", p.c_str(), fmt_sp.c_str());
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
	    nerr(nss, "Error: bu_opt value read failure reading format specifier width from substring \"%s\" of specifier \"%s\": %s\n", wn.c_str(), fmt_sp.c_str(), bu_vls_addr(&optparse_msg));
	    bu_vls_free(&optparse_msg);
	    return -1;
	}
	bu_vls_free(&optparse_msg);
	if (w > NIRT_PRINTF_MAXWIDTH) {
	    nerr(nss, "Error: width specification in format specifier substring \"%s\" of specifier \"%s\" exceeds allowed max width (%d)\n", wn.c_str(), fmt_sp.c_str(), NIRT_PRINTF_MAXWIDTH);
	    return -1;
	} 
    }
    if (pn.length() > 0) {
	// Precision check
	struct bu_vls optparse_msg = BU_VLS_INIT_ZERO;
	int p_num = 0;
	const char *pn_cstr = pn.c_str();
	if (bu_opt_int(&optparse_msg, 1, (const char **)&pn_cstr, (void *)&p_num) == -1) {
	    nerr(nss, "Error: bu_opt value read failure reading format specifier precision from substring \"%s\" of specifier \"%s\": %s\n", pn.c_str(), fmt_sp.c_str(), bu_vls_addr(&optparse_msg));
	    bu_vls_free(&optparse_msg);
	    return -1;
	}
	bu_vls_free(&optparse_msg);

	switch (fmt_sp.c_str()[fmt_sp.length()-1]) {
	    case 'd':
	    case 'i':
		if (p_num > std::numeric_limits<int>::digits10) {
		    nerr(nss, "Error: precision specification in format specifier substring \"%s\" of specifier \"%s\" exceeds allowed maximum (%d)\n", pn.c_str(), fmt_sp.c_str(), std::numeric_limits<int>::digits10);
		    return -1;
		} 
		break;
	    case 'f':
	    case 'e':
	    case 'E':
	    case 'g':
	    case 'G':
		if (p_num > std::numeric_limits<fastf_t>::max_digits10) {
		    nerr(nss, "Error: precision specification in format specifier substring \"%s\" of specifier \"%s\" exceeds allowed maximum (%d)\n", pn.c_str(), fmt_sp.c_str(), std::numeric_limits<fastf_t>::max_digits10);
		    return -1;
		}
		break;
	    case 's':
		if ((size_t)p_num > pn.max_size()) {
		    nerr(nss, "Error: precision specification in format specifier substring \"%s\" of specifier \"%s\" exceeds allowed maximum (%zu)\n", pn.c_str(), fmt_sp.c_str(), pn.max_size());
		    return -1;
		}
		break;
	    default:
		nerr(nss, "Error: NIRT internal error in format width/precision validation.\n");
		return -1;
		break;
	}
    }
    return 0;
}

HIDDEN int
_nirt_fmt_sp_validate(struct nirt_state *nss, std::string &fmt_sp)
{
    // Sanity check any sub-specifiers
    if (_nirt_fmt_sp_flags_check(nss, fmt_sp)) return -1;
    if (_nirt_fmt_sp_width_precision_check(nss, fmt_sp)) return -1;
    return 0;
}

/* Processes the first format specifier.  (We use _nirt_split_fmt to break up a format string into
 * substrings with one format specifier per string - _nirt_fmt_sp_get's job is to extract the actual
 * format specifier from those substrings.) */
HIDDEN int
_nirt_fmt_sp_get(struct nirt_state *nss, const char *fmt, std::string &fmt_sp)
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
	nerr(nss, "Error: could not find format specifier in fmt substring \"%s\"\n", fmt);
	return -1;
    }

    // Find the terminating character of the specifier and build
    // the substring.
    std::string wfmt(uos);
    size_t ep = wfmt.find_first_of(NIRT_PRINTF_SPECIFIERS);
    if (ep == std::string::npos) {
	nerr(nss, "Error: could not find valid format specifier terminator in fmt substring \"%s\" - candidates are \"%s\"\n", fmt, NIRT_PRINTF_SPECIFIERS);
	return -1;
    }
    fmt_sp = wfmt.substr(0, ep+1);

    // Check for unsupported fmt features
    size_t invalid_char;
    invalid_char = fmt_sp.find_first_of("*");
    if (invalid_char != std::string::npos) {
	nerr(nss, "Error: NIRT format specifiers do not support wildcard ('*') characters for width or precision: \"%s\"\n", fmt_sp.c_str());
	fmt_sp.clear();
	return -1;
    }
    invalid_char = fmt_sp.find_first_of("hljztL");
    if (invalid_char != std::string::npos) {
	nerr(nss, "Error: NIRT format specifiers do not support length sub-specifiers: \"%s\"\n", fmt_sp.c_str());
	fmt_sp.clear();
	return -1;
    }

    return 0;
}

/* Given a key and a format specifier string, use the NIRT type to check that the
 * supplied key is an appropriate input for that specifier. */
HIDDEN int
_nirt_fmt_sp_key_check(struct nirt_state *nss, const char *key, std::string &fmt_sp)
{
    int key_ok = 1;
    const char *type = NULL;

    if (!nss || !nss->i->val_types || fmt_sp.length() == 0) {
	nerr(nss, "Internal NIRT format processing error.\n");
	return -1;
    }

    if (!key || strlen(key) == 0) {
	nerr(nss, "Format specifier \"%s\" has no matching NIRT value key\n", fmt_sp.c_str());
	return -1;
    }

    type = bu_avs_get(nss->i->val_types, key);
    if (!type) {
	nerr(nss, "Key \"%s\" supplied to format specifier \"%s\" is not a valid NIRT value key\n", key, fmt_sp.c_str());
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
	    nerr(nss, "Error: format specifier terminator \"%s\" is invalid/unsupported: \"%c\" \n", fmt_sp.c_str(), fmt_sp.c_str()[fmt_sp.length()-1]);
	    return -1;
	    break;
    }

    if (!key_ok) {
	nerr(nss, "Error: NIRT value key \"%s\" has type %s, which does not match the type expected by format specifier \"%s\" \n", key, bu_avs_get(nss->i->val_types, key), fmt_sp.c_str());
	return -1;
    }

    return 0;
}

/* Given a format string, produce an array of substrings each of which contain
 * at most one format specifier */
HIDDEN int
_nirt_split_fmt(const char *ofmt, char ***breakout)
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
    fcnt = _nirt_fmt_sp_cnt(fmt);
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
HIDDEN void
_nirt_print_fmt_str(struct bu_vls *ostr, std::vector<std::pair<std::string,std::string> > &fmt_vect)
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

/* Given a vector breakout of a format string, generate the NIRT fmt command that
 * created it */
HIDDEN void
_nirt_print_fmt_cmd(struct bu_vls *ostr, char f, std::vector<std::pair<std::string,std::string> > &fmt_vect)
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
    bu_vls_sprintf(ostr, "fmt %c \"%s\"%s\n", f, fmt_str.c_str(), fmt_keys.c_str());
}

/* Translate NIRT fmt substrings into fully evaluated printf output, while also
 * handling units on key values.  This should never be called using any fmt
 * string that hasn't been validated by _nirt_fmt_sp_validate and _nirt_fmt_sp_key_check */
#define nirt_print_key(keystr,val) \
    do {if (BU_STR_EQUAL(key, keystr)) { bu_vls_printf(ostr, fmt, val); return; } } \
    while (0)

void
_nirt_print_fmt_substr(struct nirt_state *nss, struct bu_vls *ostr, const char *fmt, const char *key, struct nirt_output_record *r, fastf_t base2local)
{
    if (!ostr || !fmt || !r) return;
    if (!key || !strlen(key)) {
	bu_vls_printf(ostr, "%s", fmt);
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

    if (!r->seg || r->seg->type == 0) return;

    if (r->seg->type == NIRT_PARTITION_SEG || r->seg->type == NIRT_ALL_SEG) {
	nirt_print_key("x_in", r->seg->in[X] * base2local);
	nirt_print_key("y_in", r->seg->in[Y] * base2local);
	nirt_print_key("z_in", r->seg->in[Z] * base2local);
	nirt_print_key("d_in", r->seg->d_in * base2local);
	nirt_print_key("x_out", r->seg->out[X] * base2local);
	nirt_print_key("y_out", r->seg->out[Y] * base2local);
	nirt_print_key("z_out", r->seg->out[Z] * base2local);
	nirt_print_key("d_out", r->seg->d_out * base2local);
	nirt_print_key("los", r->seg->los * base2local);
	nirt_print_key("scaled_los", r->seg->scaled_los * base2local);
	nirt_print_key("path_name", bu_vls_addr(r->seg->path_name));
	nirt_print_key("reg_name", bu_vls_addr(r->seg->reg_name));
	nirt_print_key("reg_id", r->seg->reg_id);
	nirt_print_key("obliq_in", r->seg->obliq_in);
	nirt_print_key("obliq_out", r->seg->obliq_out);
	nirt_print_key("nm_x_in", r->seg->nm_in[X]);
	nirt_print_key("nm_y_in", r->seg->nm_in[Y]);
	nirt_print_key("nm_z_in", r->seg->nm_in[Z]);
	nirt_print_key("nm_d_in", r->seg->nm_d_in);
	nirt_print_key("nm_h_in", r->seg->nm_h_in);
	nirt_print_key("nm_v_in", r->seg->nm_v_in);
	nirt_print_key("nm_x_out", r->seg->nm_out[X]);
	nirt_print_key("nm_y_out", r->seg->nm_out[Y]);
	nirt_print_key("nm_z_out", r->seg->nm_out[Z]);
	nirt_print_key("nm_d_out", r->seg->nm_d_out);
	nirt_print_key("nm_h_out", r->seg->nm_h_out);
	nirt_print_key("nm_v_out", r->seg->nm_v_out);
	nirt_print_key("surf_num_in", r->seg->surf_num_in);
	nirt_print_key("surf_num_out", r->seg->surf_num_out);
	nirt_print_key("claimant_count", r->seg->claimant_count);
	nirt_print_key("claimant_list", bu_vls_addr(r->seg->claimant_list));
	nirt_print_key("claimant_listn", bu_vls_addr(r->seg->claimant_listn));
	nirt_print_key("attributes", bu_vls_addr(r->seg->attributes));
    }

    if (r->seg->type == NIRT_OVERLAP_SEG || r->seg->type == NIRT_ALL_SEG) {
	nirt_print_key("ov_reg1_name", bu_vls_addr(r->seg->ov_reg1_name));
	nirt_print_key("ov_reg1_id", r->seg->ov_reg1_id);
	nirt_print_key("ov_reg2_name", bu_vls_addr(r->seg->ov_reg2_name));
	nirt_print_key("ov_reg2_id", r->seg->ov_reg2_id);
	nirt_print_key("ov_sol_in", bu_vls_addr(r->seg->ov_sol_in));
	nirt_print_key("ov_sol_out", bu_vls_addr(r->seg->ov_sol_out));
	nirt_print_key("ov_los", r->seg->ov_los * base2local);
	nirt_print_key("ov_x_in", r->seg->ov_in[X] * base2local);
	nirt_print_key("ov_y_in", r->seg->ov_in[Y] * base2local);
	nirt_print_key("ov_z_in", r->seg->ov_in[Z] * base2local);
	nirt_print_key("ov_d_in", r->seg->ov_d_in * base2local);
	nirt_print_key("ov_x_out", r->seg->ov_out[X] * base2local);
	nirt_print_key("ov_y_out", r->seg->ov_out[Y] * base2local);
	nirt_print_key("ov_z_out", r->seg->ov_out[Z] * base2local);
	nirt_print_key("ov_d_out", r->seg->ov_d_out * base2local);
    }

    if (r->seg->type == NIRT_GAP_SEG || r->seg->type == NIRT_ALL_SEG) {
	nirt_print_key("x_in", r->seg->in[X] * base2local);
	nirt_print_key("y_in", r->seg->in[Y] * base2local);
	nirt_print_key("z_in", r->seg->in[Z] * base2local);
	
	nirt_print_key("x_gap_in", r->seg->gap_in[X] * base2local);
	nirt_print_key("y_gap_in", r->seg->gap_in[Y] * base2local);
	nirt_print_key("z_gap_in", r->seg->gap_in[Z] * base2local);
	nirt_print_key("gap_los", r->seg->gap_los * base2local);
    }

    /* TODO - the fmt command should ideally do checking on format definition
     * to preclude the possibility of needing these error checks... */
    switch (r->seg->type) {
	case NIRT_MISS_SEG:
	    nerr(nss, "Key %s is not supported in MISS reporting\n", key);
	    break;
	case NIRT_PARTITION_SEG:
	    nerr(nss, "Key %s is not supported in PARTITION reporting\n", key);
	    break;
	case NIRT_OVERLAP_SEG:
	    nerr(nss, "Key %s is not supported in OVERLAP reporting\n", key);
	    break;
	case NIRT_GAP_SEG:
	    nerr(nss, "Key %s is not supported in GAP reporting\n", key);
	    break;
    }
}

/* Generate the full report string defined by the array of fmt,key pairs
 * associated with the supplied type, based on current values */
HIDDEN void
_nirt_report(struct nirt_state *nss, char type, struct nirt_output_record *r)
{
    struct bu_vls rstr = BU_VLS_INIT_ZERO;
    std::vector<std::pair<std::string,std::string> > *fmt_vect = NULL;
    std::vector<std::pair<std::string,std::string> >::iterator f_it;

    switch (type) {
	case 'r':
	    fmt_vect = &nss->i->fmt_ray;
	    break;
	case 'h':
	    fmt_vect = &nss->i->fmt_head;
	    break;
	case 'p':
	    fmt_vect = &nss->i->fmt_part;
	    break;
	case 'f':
	    fmt_vect = &nss->i->fmt_foot;
	    break;
	case 'm':
	    fmt_vect = &nss->i->fmt_miss;
	    break;
	case 'o':
	    fmt_vect = &nss->i->fmt_ovlp;
	    break;
	case 'g':
	    fmt_vect = &nss->i->fmt_gap;
	    break;
	default:
	    nerr(nss, "Error: NIRT internal error, attempt to report using unknown fmt type %c\n", type);
	    return;
    }
    for(f_it = fmt_vect->begin(); f_it != fmt_vect->end(); f_it++) {
	const char *key = (*f_it).second.c_str();
	const char *fmt = (*f_it).first.c_str();
	_nirt_print_fmt_substr(nss, &rstr, fmt, key, r, nss->i->base2local);
    }
    nout(nss, "%s", bu_vls_addr(&rstr));
    bu_vls_free(&rstr);
}


/************************
 * Raytracing Callbacks *
 ************************/

HIDDEN struct nirt_overlap *
_nirt_find_ovlp(struct nirt_state *nss, struct partition *pp)
{
    struct nirt_overlap *op;

    for (op = nss->i->vals->ovlp_list.forw; op != &(nss->i->vals->ovlp_list); op = op->forw) {
	if (((pp->pt_inhit->hit_dist <= op->in_dist)
		    && (op->in_dist <= pp->pt_outhit->hit_dist)) ||
		((pp->pt_inhit->hit_dist <= op->out_dist)
		 && (op->in_dist <= pp->pt_outhit->hit_dist)))
	    break;
    }
    return (op == &(nss->i->vals->ovlp_list)) ? NIRT_OVERLAP_NULL : op;
}


HIDDEN void
_nirt_del_ovlp(struct nirt_overlap *op)
{
    op->forw->backw = op->backw;
    op->backw->forw = op->forw;
    bu_free((char *)op, "free op in del_ovlp");
}

HIDDEN void
_nirt_init_ovlp(struct nirt_state *nss)
{
    nss->i->vals->ovlp_list.forw = nss->i->vals->ovlp_list.backw = &(nss->i->vals->ovlp_list);
}


extern "C" int
_nirt_if_hit(struct application *ap, struct partition *part_head, struct seg *UNUSED(finished_segs))
{
    struct bu_list *vhead;
    struct nirt_state *nss = (struct nirt_state *)ap->a_uptr;
    struct nirt_output_record *vals = nss->i->vals;
    int part_nm = 0;
    struct nirt_overlap *ovp;
    struct partition *part;
    int ev_odd = 1; /* first partition is colored as "odd" */
    point_t out_old = VINIT_ZERO;
    double d_out_old = 0.0;
    struct nirt_seg *s;
    _nirt_seg_init(&s);
    if (vals->seg) {
	_nirt_seg_free(vals->seg);
	vals->seg = NULL;
    }
    vals->seg = s;

    if (!nss->i->diff_run) _nirt_report(nss, 'r', vals);
    if (!nss->i->diff_run) _nirt_report(nss, 'h', vals);

    if (nss->i->overlap_claims == NIRT_OVLP_REBUILD_FASTGEN) {
	rt_rebuild_overlaps(part_head, ap, 1);
    } else if (nss->i->overlap_claims == NIRT_OVLP_REBUILD_ALL) {
	rt_rebuild_overlaps(part_head, ap, 0);
    }

    for (part = part_head->pt_forw; part != part_head; part = part->pt_forw) {
	s->type = NIRT_PARTITION_SEG;

	++part_nm;

	/* Update the output values */
	RT_HIT_NORMAL(s->nm_in, part->pt_inhit, part->pt_inseg->seg_stp,
		&ap->a_ray, part->pt_inflip);
	RT_HIT_NORMAL(s->nm_out, part->pt_outhit, part->pt_outseg->seg_stp,
		&ap->a_ray, part->pt_outflip);
	VMOVE(s->in, part->pt_inhit->hit_point);
	VMOVE(s->out, part->pt_outhit->hit_point);
	if (part_nm > 1) VMOVE(s->gap_in, out_old);

	ndbg(nss, NIRT_DEBUG_HITS, "Partition %d entry: (%g, %g, %g) exit: (%g, %g, %g)\n",
		part_nm, V3ARGS(s->in), V3ARGS(s->out));

	s->d_in = d_calc(nss, s->in);
	s->d_out = d_calc(nss, s->out);
	s->nm_d_in = d_calc(nss, s->nm_in);
	s->nm_h_in = h_calc(nss, s->nm_in);
	s->nm_v_in = v_calc(nss, s->nm_in);

	s->nm_d_out = d_calc(nss, s->nm_out);
	s->nm_h_out = h_calc(nss, s->nm_out);
	s->nm_v_out = v_calc(nss, s->nm_out);

	s->los = s->d_in - s->d_out;
	s->scaled_los = 0.01 * s->los * part->pt_regionp->reg_los;

	if (part_nm > 1) {
	    /* old d_out - new d_in */
	    s->gap_los = d_out_old - s->d_in;

	    if (s->gap_los > 0) {
		s->type = NIRT_GAP_SEG;
		if (!nss->i->diff_run) _nirt_report(nss, 'g', vals);
		if (nss->i->cdiff) {
		    nss->i->cdiff->new_segs.push_back(_nirt_seg_cpy(s));
		}
		/* vlist segment for gap */
		vhead = bn_vlblock_find(nss->i->segs, nss->i->void_color->buc_rgb[RED], nss->i->void_color->buc_rgb[GRN], nss->i->void_color->buc_rgb[BLU]);
		BN_ADD_VLIST(nss->i->segs->free_vlist_hd, vhead, s->gap_in, BN_VLIST_LINE_MOVE);
		BN_ADD_VLIST(nss->i->segs->free_vlist_hd, vhead, s->in, BN_VLIST_LINE_DRAW);
		nss->i->b_segs = true;
		s->type = NIRT_PARTITION_SEG;
	    }
	}
	VMOVE(out_old, s->out); // Stash the out value for gap_los calculation in the next partition
	d_out_old = s->d_out; // Stash the d_out value for gap_los calculation in the next partition

	bu_vls_sprintf(s->path_name, "%s", part->pt_regionp->reg_name);
	{
	    char *tmp_regname = (char *)bu_calloc(bu_vls_strlen(s->path_name) + 1, sizeof(char), "tmp reg_name");
	    bu_path_basename(part->pt_regionp->reg_name, tmp_regname);
	    bu_vls_sprintf(s->reg_name, "%s", tmp_regname);
	    bu_free(tmp_regname, "tmp reg_name");
	}

	s->reg_id = part->pt_regionp->reg_regionid;
	s->surf_num_in = part->pt_inhit->hit_surfno;
	s->surf_num_out = part->pt_outhit->hit_surfno;
	s->obliq_in = _nirt_get_obliq(ap->a_ray.r_dir, s->nm_in);
	s->obliq_out = _nirt_get_obliq(ap->a_ray.r_dir, s->nm_out);

	bu_vls_trunc(s->claimant_list, 0);
	bu_vls_trunc(s->claimant_listn, 0);
	if (part->pt_overlap_reg == 0) {
	    s->claimant_count = 1;
	} else {
	    struct region **rpp;
	    struct bu_vls tmpcp = BU_VLS_INIT_ZERO;
	    s->claimant_count = 0;
	    for (rpp = part->pt_overlap_reg; *rpp != REGION_NULL; ++rpp) {
		char *base = NULL;
		if (s->claimant_count) bu_vls_strcat(s->claimant_list, " ");
		s->claimant_count++;
		bu_vls_sprintf(&tmpcp, "%s", (*rpp)->reg_name);
		base = bu_path_basename(bu_vls_addr(&tmpcp), NULL);
		bu_vls_strcat(s->claimant_list, base);
		bu_free(base, "bu_path_basename");
	    }
	    bu_vls_free(&tmpcp);

	    /* insert newlines instead of spaces for listn */
	    bu_vls_sprintf(s->claimant_listn, "%s", bu_vls_addr(s->claimant_list));
	    for (char *cp = bu_vls_addr(s->claimant_listn); *cp != '\0'; ++cp) {
		if (*cp == ' ') *cp = '\n';
	    }
	}

	bu_vls_trunc(s->attributes, 0);
	std::set<std::string>::iterator a_it;
	for (a_it = nss->i->attrs.begin(); a_it != nss->i->attrs.end(); a_it++) {
	    const char *key = (*a_it).c_str();
	    const char *val = bu_avs_get(&part->pt_regionp->attr_values, key);
	    if (val != NULL) {
		bu_vls_printf(s->attributes, "%s=%s ", key, val);
	    }
	}

	if (!nss->i->diff_run) _nirt_report(nss, 'p', vals);

	/* vlist segment for hit */
	if (ev_odd % 2) {
	    vhead = bn_vlblock_find(nss->i->segs, nss->i->hit_odd_color->buc_rgb[RED], nss->i->hit_odd_color->buc_rgb[GRN], nss->i->hit_odd_color->buc_rgb[BLU]);
	} else {
	    vhead = bn_vlblock_find(nss->i->segs, nss->i->hit_even_color->buc_rgb[RED], nss->i->hit_even_color->buc_rgb[GRN], nss->i->hit_even_color->buc_rgb[BLU]);
	}
	BN_ADD_VLIST(nss->i->segs->free_vlist_hd, vhead, s->in, BN_VLIST_LINE_MOVE);
	BN_ADD_VLIST(nss->i->segs->free_vlist_hd, vhead, s->out, BN_VLIST_LINE_DRAW);
	nss->i->b_segs = true;

	/* done with hit portion - if diff, stash */
	if (nss->i->cdiff && nss->i->diff_run) {
	    nss->i->cdiff->new_segs.push_back(_nirt_seg_cpy(s));
	}

	while ((ovp = _nirt_find_ovlp(nss, part)) != NIRT_OVERLAP_NULL) {

	    s->type = NIRT_OVERLAP_SEG;

	    // TODO - do this more cleanly
	    char *copy_ovlp_reg1 = bu_strdup(ovp->reg1->reg_name);
	    char *copy_ovlp_reg2 = bu_strdup(ovp->reg2->reg_name);
	    char *t1 = (char *)bu_calloc(strlen(copy_ovlp_reg1), sizeof(char), "if_hit sval2");
	    bu_path_basename(copy_ovlp_reg1, t1);
	    bu_vls_sprintf(s->ov_reg1_name, "%s", t1);
	    bu_free(t1, "t1");
	    bu_free(copy_ovlp_reg1, "cp1");
	    char *t2 = (char *)bu_calloc(strlen(copy_ovlp_reg2), sizeof(char), "if_hit sval3");
	    bu_path_basename(copy_ovlp_reg2, t2);
	    bu_vls_sprintf(s->ov_reg2_name, "%s", t2);
	    bu_free(t2, "t2");
	    bu_free(copy_ovlp_reg2, "cp2");


	    s->ov_reg1_id = ovp->reg1->reg_regionid;
	    s->ov_reg2_id = ovp->reg2->reg_regionid;
	    bu_vls_sprintf(s->ov_sol_in, "%s", part->pt_inseg->seg_stp->st_dp->d_namep);
	    bu_vls_sprintf(s->ov_sol_out, "%s", part->pt_outseg->seg_stp->st_dp->d_namep);
	    VMOVE(s->ov_in, ovp->in_point);
	    VMOVE(s->ov_out, ovp->out_point);

	    s->ov_d_in = vals->d_orig - ovp->in_dist; // TODO looks sketchy in NIRT - did they really mean target(D) ?? -> (VTI_XORIG + 3 -> VTI_H)
	    s->ov_d_out = vals->d_orig - ovp->out_dist; // TODO looks sketchy in NIRT - did they really mean target(D) ?? -> (VTI_XORIG + 3 -> VTI_H)
	    s->ov_los = s->ov_d_in - s->ov_d_out;

	    if (!nss->i->diff_run) _nirt_report(nss, 'o', vals);

	    /* vlist segment for overlap */
	    if (nss->i->plot_overlaps) {
		vhead = bn_vlblock_find(nss->i->segs, nss->i->overlap_color->buc_rgb[RED], nss->i->overlap_color->buc_rgb[GRN], nss->i->overlap_color->buc_rgb[BLU]);
		BN_ADD_VLIST(nss->i->segs->free_vlist_hd, vhead, s->ov_in, BN_VLIST_LINE_MOVE);
		BN_ADD_VLIST(nss->i->segs->free_vlist_hd, vhead, s->ov_out, BN_VLIST_LINE_DRAW);
		nss->i->b_segs = true;
	    }

	    /* Diff */
	    if (nss->i->cdiff && nss->i->diff_run) {
		nss->i->cdiff->new_segs.push_back(_nirt_seg_cpy(s));
	    }

	    _nirt_del_ovlp(ovp);
	}


    }

    if (!nss->i->diff_run) _nirt_report(nss, 'f', vals);

    if (vals->ovlp_list.forw != &(vals->ovlp_list)) {
	nerr(nss, "Previously unreported overlaps.  Shouldn't happen\n");
	ovp = vals->ovlp_list.forw;
	while (ovp != &(vals->ovlp_list)) {
	    nerr(nss, " OVERLAP:\n\t%s %s (%g %g %g) %g\n", ovp->reg1->reg_name, ovp->reg2->reg_name, V3ARGS(ovp->in_point), ovp->out_dist - ovp->in_dist);
	    ovp = ovp->forw;
	}
    }

    /* We're done reporting - let print get at everything */
    vals->seg->type = NIRT_ALL_SEG;

    return HIT;
}

extern "C" int
_nirt_if_miss(struct application *ap)
{
    struct nirt_state *nss = (struct nirt_state *)ap->a_uptr;
    if (!nss->i->diff_run) _nirt_report(nss, 'r', nss->i->vals);
    if (!nss->i->diff_run) _nirt_report(nss, 'm', nss->i->vals);

    // TODO - handle miss diffing...

    return MISS;
}

extern "C" int
_nirt_if_overlap(struct application *ap, struct partition *pp, struct region *reg1, struct region *reg2, struct partition *InputHdp)
{
    struct nirt_state *nss = (struct nirt_state *)ap->a_uptr;
    struct nirt_overlap *o;
    BU_ALLOC(o, struct nirt_overlap);
    o->ap = ap;
    o->pp = pp;
    o->reg1 = reg1;
    o->reg2 = reg2;
    o->in_dist = pp->pt_inhit->hit_dist;
    o->out_dist = pp->pt_outhit->hit_dist;
    VJOIN1(o->in_point, ap->a_ray.r_pt, pp->pt_inhit->hit_dist, ap->a_ray.r_dir);
    VJOIN1(o->out_point, ap->a_ray.r_pt, pp->pt_outhit->hit_dist, ap->a_ray.r_dir);

    /* Insert the new overlap into the list of overlaps */
    o->forw = nss->i->vals->ovlp_list.forw;
    o->backw = &(nss->i->vals->ovlp_list);
    o->forw->backw = o;
    nss->i->vals->ovlp_list.forw = o;

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

static const struct nirt_cmd_desc nirt_descs[] = {
    { "attr",           "select attributes",                             NULL },
    { "ae",             "set/query azimuth and elevation",               "azimuth elevation" },
    { "dir",            "set/query direction vector",                    "x-component y-component z-component" },
    { "diff",           "test a ray result against a supplied default",  "[-t tol] partition_info" },
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

HIDDEN const char *
_nirt_get_desc_args(const char *key)
{
    const struct nirt_cmd_desc *d;
    for (d = nirt_descs; d->cmd; d++) {
	if (BU_STR_EQUAL(d->cmd, key)) return d->args;
    }
    return NULL;
}

extern "C" int
_nirt_cmd_attr(void *ns, int argc, const char *argv[])
{
    if (!ns) return -1;
    struct nirt_state *nss = (struct nirt_state *)ns;
    int i = 0;
    int ac = 0;
    int print_help = 0;
    int attr_print = 0;
    int attr_flush = 0;
    int val_attrs = 0;
    size_t orig_size = nss->i->attrs.size();
    struct bu_vls optparse_msg = BU_VLS_INIT_ZERO;
    struct bu_opt_desc d[5];
    BU_OPT(d[0],  "h", "help",  "",  NULL,   &print_help, "print help and exit");
    BU_OPT(d[1],  "p", "print", "",  NULL,   &attr_print, "print active attributes");
    BU_OPT(d[2],  "f", "flush", "",  NULL,   &attr_flush, "clear attribute list");
    BU_OPT(d[3],  "v", "",      "",  NULL,   &val_attrs,  "validate attributes - only accept attributes used by one or more objects in the active database");
    BU_OPT_NULL(d[4]);
    const char *ustr = "Usage: attr <opts> <attribute_name_1> <attribute_name_2> ...\nNote: attr with no options and no attributes to add will print the list of active attributes.\nOptions:";

    argv++; argc--;

    if ((ac = bu_opt_parse(&optparse_msg, argc, (const char **)argv, d)) == -1) {
	char *help = bu_opt_describe(d, NULL);
	nerr(nss, "Error: bu_opt value read failure: %s\n\n%s\n%s\n", bu_vls_addr(&optparse_msg), ustr, help);
	if (help) bu_free(help, "help str");
	bu_vls_free(&optparse_msg);
	return -1;
    }
    bu_vls_free(&optparse_msg);

    if (attr_flush) nss->i->attrs.clear();

    if (print_help || (attr_print && ac > 0)) {
	char *help = bu_opt_describe(d, NULL);
	nerr(nss, "%s\n%s", ustr, help);
	if (help) bu_free(help, "help str");
	return -1;
    }
    if (attr_print || ac == 0) {
	std::set<std::string>::iterator a_it;
	for (a_it = nss->i->attrs.begin(); a_it != nss->i->attrs.end(); a_it++) {
	    nout(nss, "\"%s\"\n", (*a_it).c_str());
	}
	return 0;
    }

    for (i = 0; i < ac; i++) {
	const char *sattr = db5_standard_attribute(db5_standardize_attribute(argv[i]));
	if (val_attrs) {
	    //TODO - check against the .g for any usage of the attribute
	}
	/* If there is a standardized version of this attribute that is
	 * different from the supplied version, activate it as well. */
	if (sattr) {
	    nss->i->attrs.insert(sattr);
	} else {
	    nss->i->attrs.insert(argv[i]);
	}
    }

    if (nss->i->attrs.size() != orig_size) {
	nss->i->need_reprep = 1;
    }
    return 0;
}

extern "C" int
_nirt_cmd_az_el(void *ns, int argc, const char *argv[])
{
    double az = 0.0;
    double el = 0.0;
    int ret = 0;
    struct bu_vls opt_msg = BU_VLS_INIT_ZERO;
    struct nirt_state *nss = (struct nirt_state *)ns;
    if (!ns) return -1;

    if (argc == 1) {
	nout(nss, "(az, el) = (%4.2f, %4.2f)\n", nss->i->vals->a, nss->i->vals->e);
	return 0;
    }

    argv++; argc--;

    if (argc != 2) {
	nerr(nss, "Usage:  ae %s\n", _nirt_get_desc_args("ae"));
	return -1;
    }

    if ((ret = bu_opt_fastf_t(&opt_msg, 1, argv, (void *)&az)) == -1) {
	nerr(nss, "Error: bu_opt value read failure: %s\n", bu_vls_addr(&opt_msg));
	goto azel_done;
    }

    if (fabs(az) > 360) {
	nerr(nss, "Error:  |azimuth| <= 360\n");
	ret = -1;
	goto azel_done;
    }

    argc--; argv++;
    if ((ret = bu_opt_fastf_t(&opt_msg, 1, argv, (void *)&el)) == -1) {
	nerr(nss, "%s\n", bu_vls_addr(&opt_msg));
	goto azel_done;
    }

    if (fabs(el) > 90) {
	nerr(nss, "Error:  |elevation| <= 90\n");
	ret = -1;
	goto azel_done;
    }

    nss->i->vals->a = az;
    nss->i->vals->e = el;
    _nirt_ae2dir(nss);
    ret = 0;

azel_done:
    bu_vls_free(&opt_msg);
    return ret;
}

//#define NIRT_DIFF_DEBUG 1

extern "C" int
_nirt_cmd_diff(void *ns, int argc, const char *argv[])
{
    if (!ns) return -1;
    struct nirt_state *nss = (struct nirt_state *)ns;
    int ac = 0;
    int print_help = 0;
    double tol = 0;
    int have_ray = 0;
    size_t cmt = std::string::npos;
    struct bu_vls optparse_msg = BU_VLS_INIT_ZERO;
    struct bu_opt_desc d[3];
    // TODO - add reporting options for enabling/disabling region/path, partition length, normal, and overlap ordering diffs.
    // For example, if r1 and r2 have the same shape in space, we may want to suppress name based differences and look at
    // other aspects of the shotline.  Also need sorting options for output - max partition diff first, max normal diff first,
    // max numerical delta, etc...
    BU_OPT(d[0],  "h", "help",       "",             NULL,   &print_help, "print help and exit");
    BU_OPT(d[1],  "t", "tol",   "<val>",  &bu_opt_fastf_t,   &tol,        "set diff tolerance");
    BU_OPT_NULL(d[2]);
    const char *ustr = "Usage: diff [opts] shotfile";

    argv++; argc--;

    if ((ac = bu_opt_parse(&optparse_msg, argc, (const char **)argv, d)) == -1) {
	char *help = bu_opt_describe(d, NULL);
	nerr(nss, "Error: bu_opt value read failure: %s\n\n%s\n%s\n", bu_vls_addr(&optparse_msg), ustr, help);
	if (help) bu_free(help, "help str");
	bu_vls_free(&optparse_msg);
	return -1;
    }
    bu_vls_free(&optparse_msg);

    if (print_help || !argv[0]) {
	char *help = bu_opt_describe(d, NULL);
	nerr(nss, "%s\n%s", ustr, help);
	if (help) bu_free(help, "help str");
	return -1;
    }

    if (BU_STR_EQUAL(argv[0], "load")) {
	ac--; argv++;
	if (ac != 1) {
	    nerr(nss, "Please specify a NIRT diff file to load.\n");
	    return -1;
	}

	if (nss->i->diff_ready) {
	    nerr(nss, "Diff file already loaded.  To reset, run \"diff clear\"\n");
	    return -1;
	}

	std::string line;
	std::ifstream ifs;
	ifs.open(argv[0]);
	if (!ifs.is_open()) {
	    nerr(nss, "Error: could not open file %s\n", argv[0]);
	    return -1;
	}

	if (nss->i->need_reprep) {
	    /* If we need to (re)prep, do it now. Failure is an error. */
	    if (_nirt_raytrace_prep(nss)) {
		nerr(nss, "Error: raytrace prep failed!\n");
		return -1;
	    }
	} else {
	    /* Based on current settings, tell the ap which rtip to use */
	    nss->i->ap->a_rt_i = _nirt_get_rtip(nss);
	    nss->i->ap->a_resource = _nirt_get_resource(nss);
	}

	bu_vls_sprintf(nss->i->diff_file, "%s", argv[0]);
	nss->i->diff_run = 1;
	have_ray = 0;
	while (std::getline(ifs, line)) {
	    struct nirt_diff *df;

	    /* If part of the line is commented, skip that part */
	    cmt = _nirt_find_first_unescaped(line, "#", 0);
	    if (cmt != std::string::npos) {
		line.erase(cmt);
	    }
	    _nirt_trim_whitespace(line);
	    if (!line.length()) continue;

	    if (!line.compare(0, 4, "RAY ")) {
		if (have_ray) {
#ifdef NIRT_DIFF_DEBUG
		    bu_log("\n\nanother ray!\n\n\n");
#endif
		    // Have ray already - execute current ray, store results in
		    // diff database (if any diffs were found), then clear expected
		    // results and old ray
		    for (int i = 0; i < 3; ++i) {
			nss->i->ap->a_ray.r_pt[i] = nss->i->vals->orig[i];
			nss->i->ap->a_ray.r_dir[i] = nss->i->vals->dir[i];
		    }
		    // TODO - rethink this container...
		    _nirt_init_ovlp(nss);
		    (void)rt_shootray(nss->i->ap);
		    nss->i->diffs.push_back(nss->i->cdiff);
		}
		// Read ray
		df = new struct nirt_diff;
		// TODO - once we go to C++11, used std::regex_search and std::smatch to more flexibly get a substring
		std::string rstr = line.substr(7);
		have_ray = 1;
		std::vector<std::string> substrs = _nirt_string_split(rstr);
		if (substrs.size() != 6) {
		    nerr(nss, "Error processing ray line \"%s\"!\nExpected 6 elements, found %zu\n", line.c_str(), substrs.size());
		    delete df;
		    return -1;
		}
		VSET(df->orig, _nirt_str_to_dbl(substrs[0], 0), _nirt_str_to_dbl(substrs[1], 0), _nirt_str_to_dbl(substrs[2], 0));
		VSET(df->dir, _nirt_str_to_dbl(substrs[3], 0), _nirt_str_to_dbl(substrs[4], 0), _nirt_str_to_dbl(substrs[5], 0));
		VMOVE(nss->i->vals->dir, df->dir);
		VMOVE(nss->i->vals->orig, df->orig);
#ifdef NIRT_DIFF_DEBUG
		bu_log("Found RAY:\n");
		bu_log("origin   : %0.17f, %0.17f, %0.17f\n", V3ARGS(df->orig));
		bu_log("direction: %0.17f, %0.17f, %0.17f\n", V3ARGS(df->dir));
#endif
		_nirt_targ2grid(nss);
		_nirt_dir2ae(nss);
		nss->i->cdiff = df;
		continue;
	    } else {
		if (!line.compare(0, 4, "HIT ") || !line.compare(0, 4, "GAP ") ||
			!line.compare(0, 5, "MISS ") || !line.compare(0, 8, "OVERLAP ")) {
		    if (!have_ray) {
			nerr(nss, "Error: Result line found but no ray set.\n");
			return -1;
		    }
		}
	    }
	    if (!line.compare(0, 4, "HIT ")) {
		// TODO - once we go to C++11, used std::regex_search and std::smatch to more flexibly get a substring
		std::string hstr = line.substr(7);
		std::vector<std::string> substrs = _nirt_string_split(hstr);
		if (substrs.size() != 15) {
		    nerr(nss, "Error processing hit line \"%s\"!\nExpected 15 elements, found %zu\n", hstr.c_str(), substrs.size());
		    return -1;
		}
		struct nirt_seg *seg;
		_nirt_seg_init(&seg);
		seg->type = NIRT_PARTITION_SEG;
		bu_vls_decode(seg->reg_name, substrs[0].c_str());
		//bu_vls_printf(seg->reg_name, "%s", substrs[0].c_str());
		bu_vls_decode(seg->path_name, substrs[1].c_str());
		seg->reg_id = _nirt_str_to_int(substrs[2]);
		VSET(seg->in, _nirt_str_to_dbl(substrs[3], 0), _nirt_str_to_dbl(substrs[4], 0), _nirt_str_to_dbl(substrs[5], 0));
		seg->d_in = _nirt_str_to_dbl(substrs[6], 0);
		VSET(seg->out, _nirt_str_to_dbl(substrs[7], 0), _nirt_str_to_dbl(substrs[8], 0), _nirt_str_to_dbl(substrs[9], 0));
		seg->d_out = _nirt_str_to_dbl(substrs[10], 0);
		seg->los = _nirt_str_to_dbl(substrs[11], 0);
		seg->scaled_los = _nirt_str_to_dbl(substrs[12], 0);
		seg->obliq_in = _nirt_str_to_dbl(substrs[13], 0);
		seg->obliq_out = _nirt_str_to_dbl(substrs[14], 0);
#ifdef NIRT_DIFF_DEBUG
		bu_log("Found %s:\n", line.c_str());
		bu_log("  reg_name: %s\n", bu_vls_addr(seg->reg_name));
		bu_log("  path_name: %s\n", bu_vls_addr(seg->path_name));
		bu_log("  reg_id: %d\n", seg->reg_id);
		bu_log("  in: %0.17f, %0.17f, %0.17f\n", V3ARGS(seg->in));
		bu_log("  out: %0.17f, %0.17f, %0.17f\n", V3ARGS(seg->out));
		bu_log("  d_in: %0.17f d_out: %0.17f\n", seg->d_in, seg->d_out);
		bu_log("  los: %0.17f  scaled_los: %0.17f\n", seg->los, seg->scaled_los);
		bu_log("  obliq_in: %0.17f  obliq_out: %0.17f\n", seg->obliq_in, seg->obliq_out);
#endif
		nss->i->cdiff->old_segs.push_back(seg);
		continue;
	    }
	    if (!line.compare(0, 4, "GAP ")) {
		// TODO - once we go to C++11, used std::regex_search and std::smatch to more flexibly get a substring
		std::string gstr = line.substr(7);
		std::vector<std::string> substrs = _nirt_string_split(gstr);
		if (substrs.size() != 7) {
		    nerr(nss, "Error processing gap line \"%s\"!\nExpected 7 elements, found %zu\n", gstr.c_str(), substrs.size());
		    return -1;
		}
		struct nirt_seg *seg;
		_nirt_seg_init(&seg);
		seg->type = NIRT_GAP_SEG;
		VSET(seg->gap_in, _nirt_str_to_dbl(substrs[0], 0), _nirt_str_to_dbl(substrs[1], 0), _nirt_str_to_dbl(substrs[2], 0));
		VSET(seg->in, _nirt_str_to_dbl(substrs[3], 0), _nirt_str_to_dbl(substrs[4], 0), _nirt_str_to_dbl(substrs[5], 0));
		seg->gap_los = _nirt_str_to_dbl(substrs[6], 0);
#ifdef NIRT_DIFF_DEBUG
		bu_log("Found %s:\n", line.c_str());
		bu_log("  in: %0.17f, %0.17f, %0.17f\n", V3ARGS(seg->gap_in));
		bu_log("  out: %0.17f, %0.17f, %0.17f\n", V3ARGS(seg->in));
		bu_log("  gap_los: %0.17f\n", seg->gap_los);
#endif
		nss->i->cdiff->old_segs.push_back(seg);
		continue;
	    }
	    if (!line.compare(0, 5, "MISS ")) {
		struct nirt_seg *seg;
		_nirt_seg_init(&seg);
		seg->type = NIRT_MISS_SEG;
#ifdef NIRT_DIFF_DEBUG
		bu_log("Found MISS\n");
#endif
		have_ray = 0;
		nss->i->cdiff->old_segs.push_back(seg);
		continue;
	    }
	    if (!line.compare(0, 8, "OVERLAP ")) {
		// TODO - once we go to C++11, used std::regex_search and std::smatch to more flexibly get a substring
		std::string ostr = line.substr(11);
		std::vector<std::string> substrs = _nirt_string_split(ostr);
		if (substrs.size() != 11) {
		    nerr(nss, "Error processing overlap line \"%s\"!\nExpected 11 elements, found %zu\n", ostr.c_str(), substrs.size());
		    return -1;
		}
		struct nirt_seg *seg;
		_nirt_seg_init(&seg);
		seg->type = NIRT_OVERLAP_SEG;
		bu_vls_decode(seg->ov_reg1_name, substrs[0].c_str());
		bu_vls_decode(seg->ov_reg2_name, substrs[1].c_str());
		seg->ov_reg1_id = _nirt_str_to_int(substrs[2]);
		seg->ov_reg2_id = _nirt_str_to_int(substrs[3]);
		VSET(seg->ov_in, _nirt_str_to_dbl(substrs[4], 0), _nirt_str_to_dbl(substrs[5], 0), _nirt_str_to_dbl(substrs[6], 0));
		VSET(seg->ov_out, _nirt_str_to_dbl(substrs[7], 0), _nirt_str_to_dbl(substrs[8], 0), _nirt_str_to_dbl(substrs[9], 0));
		seg->ov_los = _nirt_str_to_dbl(substrs[10], 0);
#ifdef NIRT_DIFF_DEBUG
		bu_log("Found %s:\n", line.c_str());
		bu_log("  ov_reg1_name: %s\n", bu_vls_addr(seg->ov_reg1_name));
		bu_log("  ov_reg2_name: %s\n", bu_vls_addr(seg->ov_reg2_name));
		bu_log("  ov_reg1_id: %d\n", seg->ov_reg1_id);
		bu_log("  ov_reg2_id: %d\n", seg->ov_reg2_id);
		bu_log("  ov_in: %0.17f, %0.17f, %0.17f\n", V3ARGS(seg->ov_in));
		bu_log("  ov_out: %0.17f, %0.17f, %0.17f\n", V3ARGS(seg->ov_out));
		bu_log("  ov_los: %0.17f\n", seg->ov_los);
#endif
		nss->i->cdiff->old_segs.push_back(seg);
		continue;
	    }
#ifdef NIRT_DIFF_DEBUG
	    nerr(nss, "Warning - unknown line type, skipping: %s\n", line.c_str());
#endif
	}

	if (have_ray) {
	    // Execute work.
	    for (int i = 0; i < 3; ++i) {
		nss->i->ap->a_ray.r_pt[i] = nss->i->vals->orig[i];
		nss->i->ap->a_ray.r_dir[i] = nss->i->vals->dir[i];
	    }
	    // TODO - rethink this container...
	    _nirt_init_ovlp(nss);
	    (void)rt_shootray(nss->i->ap);
	    nss->i->diffs.push_back(nss->i->cdiff);

	    // Thought - if we have rays but no pre-defined output, write out the
	    // expected output to stdout - in this mode diff will generate a diff
	    // input file from a supplied list of rays.
	}

	ifs.close();

	// Done with if_hit and friends
	nss->i->cdiff = NULL;
	nss->i->diff_run = 0;
	nss->i->diff_ready = 1;
	return 0;
    }

    if (BU_STR_EQUAL(argv[0], "report")) {
	// Report diff results according to the NIRT diff settings.
	if (!nss->i->diff_ready) {
	    nerr(nss, "No diff file loaded - please load a diff file with \"diff load <filename>\"\n");
	    return -1;
	} else {
	    return (_nirt_diff_report(nss) >= 0) ? 0 : -1;
	}
    }

    if (BU_STR_EQUAL(argv[0], "clear")) {
	// need clear command to scrub old nss->i->diffs
	for (size_t i = 0; i < nss->i->diffs.size(); i++) {
	    struct nirt_diff *df = nss->i->diffs[i];
	    for (size_t j = 0; j < df->old_segs.size(); j++) {
		_nirt_seg_free(df->old_segs[j]);
	    }
	    for (size_t j = 0; j < df->new_segs.size(); j++) {
		_nirt_seg_free(df->new_segs[j]);
	    }
	    delete nss->i->diffs[i];
	}
	nss->i->diffs.clear();
	nss->i->diff_ready = 0;
	return 0;
    }

    if (BU_STR_EQUAL(argv[0], "settings")) {
	ac--; argv++;
	if (!ac) {
	    //print current settings
	    nout(nss, "report_partitions:           %d\n", nss->i->diff_settings->report_partitions);
	    nout(nss, "report_misses:               %d\n", nss->i->diff_settings->report_misses);
	    nout(nss, "report_gaps:                 %d\n", nss->i->diff_settings->report_gaps);
	    nout(nss, "report_overlaps:             %d\n", nss->i->diff_settings->report_overlaps);
	    nout(nss, "report_partition_reg_ids:    %d\n", nss->i->diff_settings->report_partition_reg_ids);
	    nout(nss, "report_partition_reg_names:  %d\n", nss->i->diff_settings->report_partition_reg_names);
	    nout(nss, "report_partition_path_names: %d\n", nss->i->diff_settings->report_partition_path_names);
	    nout(nss, "report_partition_dists:      %d\n", nss->i->diff_settings->report_partition_dists);
	    nout(nss, "report_partition_obliq:      %d\n", nss->i->diff_settings->report_partition_obliq);
	    nout(nss, "report_overlap_reg_names:    %d\n", nss->i->diff_settings->report_overlap_reg_names);
	    nout(nss, "report_overlap_reg_ids:      %d\n", nss->i->diff_settings->report_overlap_reg_ids);
	    nout(nss, "report_overlap_dists:        %d\n", nss->i->diff_settings->report_overlap_dists);
	    nout(nss, "report_overlap_obliq:        %d\n", nss->i->diff_settings->report_overlap_obliq);
	    nout(nss, "report_gap_dists:            %d\n", nss->i->diff_settings->report_gap_dists);
	    nout(nss, "dist_delta_tol:              %g\n", nss->i->diff_settings->dist_delta_tol);
	    nout(nss, "obliq_delta_tol:             %g\n", nss->i->diff_settings->obliq_delta_tol);
	    nout(nss, "los_delta_tol:               %g\n", nss->i->diff_settings->los_delta_tol);
	    nout(nss, "scaled_los_delta_tol:        %g\n", nss->i->diff_settings->scaled_los_delta_tol);
	    return 0;
	}
	if (ac == 1) {
	    //print specific setting
	    if (BU_STR_EQUAL(argv[0], "report_partitions"))           nout(nss, "%d\n", nss->i->diff_settings->report_partitions);
	    if (BU_STR_EQUAL(argv[0], "report_misses"))               nout(nss, "%d\n", nss->i->diff_settings->report_misses);
	    if (BU_STR_EQUAL(argv[0], "report_gaps"))                 nout(nss, "%d\n", nss->i->diff_settings->report_gaps);
	    if (BU_STR_EQUAL(argv[0], "report_overlaps"))             nout(nss, "%d\n", nss->i->diff_settings->report_overlaps);
	    if (BU_STR_EQUAL(argv[0], "report_partition_reg_ids"))    nout(nss, "%d\n", nss->i->diff_settings->report_partition_reg_ids);
	    if (BU_STR_EQUAL(argv[0], "report_partition_reg_names"))  nout(nss, "%d\n", nss->i->diff_settings->report_partition_reg_names);
	    if (BU_STR_EQUAL(argv[0], "report_partition_path_names")) nout(nss, "%d\n", nss->i->diff_settings->report_partition_path_names);
	    if (BU_STR_EQUAL(argv[0], "report_partition_dists"))      nout(nss, "%d\n", nss->i->diff_settings->report_partition_dists);
	    if (BU_STR_EQUAL(argv[0], "report_partition_obliq"))      nout(nss, "%d\n", nss->i->diff_settings->report_partition_obliq);
	    if (BU_STR_EQUAL(argv[0], "report_overlap_reg_names"))    nout(nss, "%d\n", nss->i->diff_settings->report_overlap_reg_names);
	    if (BU_STR_EQUAL(argv[0], "report_overlap_reg_ids"))      nout(nss, "%d\n", nss->i->diff_settings->report_overlap_reg_ids);
	    if (BU_STR_EQUAL(argv[0], "report_overlap_dists"))        nout(nss, "%d\n", nss->i->diff_settings->report_overlap_dists);
	    if (BU_STR_EQUAL(argv[0], "report_overlap_obliq"))        nout(nss, "%d\n", nss->i->diff_settings->report_overlap_obliq);
	    if (BU_STR_EQUAL(argv[0], "report_gap_dists"))            nout(nss, "%d\n", nss->i->diff_settings->report_gap_dists);
	    if (BU_STR_EQUAL(argv[0], "dist_delta_tol"))              nout(nss, "%g\n", nss->i->diff_settings->dist_delta_tol);
	    if (BU_STR_EQUAL(argv[0], "obliq_delta_tol"))             nout(nss, "%g\n", nss->i->diff_settings->obliq_delta_tol);
	    if (BU_STR_EQUAL(argv[0], "los_delta_tol"))               nout(nss, "%g\n", nss->i->diff_settings->los_delta_tol);
	    if (BU_STR_EQUAL(argv[0], "scaled_los_delta_tol"))        nout(nss, "%g\n", nss->i->diff_settings->scaled_los_delta_tol);
	    return 0;
	}
	if (ac == 2) {
	    //set setting
	    struct bu_vls opt_msg = BU_VLS_INIT_ZERO;
	    int *setting_int = NULL;
	    fastf_t *setting_fastf_t = NULL;
	    if (BU_STR_EQUAL(argv[0], "report_partitions"))           setting_int = &(nss->i->diff_settings->report_partitions);
	    if (BU_STR_EQUAL(argv[0], "report_misses"))               setting_int = &(nss->i->diff_settings->report_misses);
	    if (BU_STR_EQUAL(argv[0], "report_gaps"))                 setting_int = &(nss->i->diff_settings->report_gaps);
	    if (BU_STR_EQUAL(argv[0], "report_overlaps"))             setting_int = &(nss->i->diff_settings->report_overlaps);
	    if (BU_STR_EQUAL(argv[0], "report_partition_reg_ids"))    setting_int = &(nss->i->diff_settings->report_partition_reg_ids);
	    if (BU_STR_EQUAL(argv[0], "report_partition_reg_names"))  setting_int = &(nss->i->diff_settings->report_partition_reg_names);
	    if (BU_STR_EQUAL(argv[0], "report_partition_path_names")) setting_int = &(nss->i->diff_settings->report_partition_path_names);
	    if (BU_STR_EQUAL(argv[0], "report_partition_dists"))      setting_int = &(nss->i->diff_settings->report_partition_dists);
	    if (BU_STR_EQUAL(argv[0], "report_partition_obliq"))      setting_int = &(nss->i->diff_settings->report_partition_obliq);
	    if (BU_STR_EQUAL(argv[0], "report_overlap_reg_names"))    setting_int = &(nss->i->diff_settings->report_overlap_reg_names);
	    if (BU_STR_EQUAL(argv[0], "report_overlap_reg_ids"))      setting_int = &(nss->i->diff_settings->report_overlap_reg_ids);
	    if (BU_STR_EQUAL(argv[0], "report_overlap_dists"))        setting_int = &(nss->i->diff_settings->report_overlap_dists);
	    if (BU_STR_EQUAL(argv[0], "report_overlap_obliq"))        setting_int = &(nss->i->diff_settings->report_overlap_obliq);
	    if (BU_STR_EQUAL(argv[0], "report_gap_dists"))            setting_int = &(nss->i->diff_settings->report_gap_dists);
	    if (BU_STR_EQUAL(argv[0], "dist_delta_tol"))              setting_fastf_t = &(nss->i->diff_settings->dist_delta_tol);
	    if (BU_STR_EQUAL(argv[0], "obliq_delta_tol"))             setting_fastf_t = &(nss->i->diff_settings->obliq_delta_tol);
	    if (BU_STR_EQUAL(argv[0], "los_delta_tol"))               setting_fastf_t = &(nss->i->diff_settings->los_delta_tol);
	    if (BU_STR_EQUAL(argv[0], "scaled_los_delta_tol"))        setting_fastf_t = &(nss->i->diff_settings->scaled_los_delta_tol);

	    if (setting_int) {
		if (bu_opt_int(&opt_msg, 1, (const char **)&argv[1], (void *)setting_int) == -1) {
		    nerr(nss, "Error: bu_opt value read failure: %s\n", bu_vls_addr(&opt_msg));
		    bu_vls_free(&opt_msg);
		    return -1;
		} else {
		    return 0;
		}
	    }
	    if (setting_fastf_t) {
		if (BU_STR_EQUAL(argv[1], "BN_TOL_DIST")) {
		    *setting_fastf_t = BN_TOL_DIST;
		    return 0;
		} else {
		    if (bu_opt_fastf_t(&opt_msg, 1, (const char **)&argv[1], (void *)setting_fastf_t) == -1) {
			nerr(nss, "Error: bu_opt value read failure: %s\n", bu_vls_addr(&opt_msg));
			bu_vls_free(&opt_msg);
			return -1;
		    } else {
			return 0;
		    }
		}
	    }

	    // anything else is an error
	    return -1;
	}
    }

    nerr(nss, "Unknown diff subcommand: \"%s\"\n", argv[0]);

    return -1;
}


extern "C" int
_nirt_cmd_dir_vect(void *ns, int argc, const char *argv[])
{
    vect_t dir = VINIT_ZERO;
    struct bu_vls opt_msg = BU_VLS_INIT_ZERO;
    struct nirt_state *nss = (struct nirt_state *)ns;
    if (!ns) return -1;

    if (argc == 1) {
	nout(nss, "(x, y, z) = (%4.2f, %4.2f, %4.2f)\n", V3ARGS(nss->i->vals->dir));
	return 0;
    }

    argc--; argv++;
    if (argc != 3) {
	nerr(nss, "Usage:  dir %s\n", _nirt_get_desc_args("dir"));
	return -1;
    }

    if (bu_opt_vect_t(&opt_msg, 3, argv, (void *)&dir) == -1) {
	nerr(nss, "%s\n", bu_vls_addr(&opt_msg));
	bu_vls_free(&opt_msg);
	return -1;
    }

    VUNITIZE(dir);
    VMOVE(nss->i->vals->dir, dir);
    _nirt_dir2ae(nss);
    return 0;
}

extern "C" int
_nirt_cmd_grid_coor(void *ns, int argc, const char *argv[])
{
    vect_t grid = VINIT_ZERO;
    struct bu_vls opt_msg = BU_VLS_INIT_ZERO;
    struct nirt_state *nss = (struct nirt_state *)ns;
    if (!ns) return -1;

    if (argc == 1) {
	VSET(grid, nss->i->vals->h, nss->i->vals->v, nss->i->vals->d_orig);
	VSCALE(grid, grid, nss->i->base2local);
	nout(nss, "(h, v, d) = (%4.2f, %4.2f, %4.2f)\n", V3ARGS(grid));
	return 0;
    }

    argc--; argv++;
    if (argc != 2 && argc != 3) {
	nerr(nss, "Usage:  hv %s\n", _nirt_get_desc_args("hv"));
	return -1;
    }
    if (bu_opt_fastf_t(&opt_msg, 1, argv, (void *)&grid[0]) == -1) {
	nerr(nss, "%s\n", bu_vls_addr(&opt_msg));
	bu_vls_free(&opt_msg);
	return -1;
    }
    argc--; argv++;
    if (bu_opt_fastf_t(&opt_msg, 1, argv, (void *)&grid[1]) == -1) {
	nerr(nss, "%s\n", bu_vls_addr(&opt_msg));
	bu_vls_free(&opt_msg);
	return -1;
    }
    argc--; argv++;
    if (argc) {
	if (bu_opt_fastf_t(&opt_msg, 1, argv, (void *)&grid[2]) == -1) {
	    nerr(nss, "%s\n", bu_vls_addr(&opt_msg));
	    bu_vls_free(&opt_msg);
	    return -1;
	}
    } else {
	grid[2] = nss->i->vals->d_orig;
    }

    VSCALE(grid, grid, nss->i->local2base);
    nss->i->vals->h = grid[0];
    nss->i->vals->v = grid[1];
    nss->i->vals->d_orig = grid[2];
    _nirt_grid2targ(nss);

    return 0;
}

extern "C" int
_nirt_cmd_target_coor(void *ns, int argc, const char *argv[])
{
    vect_t target = VINIT_ZERO;
    struct bu_vls opt_msg = BU_VLS_INIT_ZERO;
    struct nirt_state *nss = (struct nirt_state *)ns;
    if (!ns) return -1;

    if (argc == 1) {
	VMOVE(target, nss->i->vals->orig);
	VSCALE(target, target, nss->i->base2local);
	nout(nss, "(x, y, z) = (%4.2f, %4.2f, %4.2f)\n", V3ARGS(target));
	return 0;
    }

    argc--; argv++;
    if (argc != 3) {
	nerr(nss, "Usage:  xyz %s\n", _nirt_get_desc_args("xyz"));
	return -1;
    }

    if (bu_opt_vect_t(&opt_msg, 3, argv, (void *)&target) == -1) {
	nerr(nss, "%s\n", bu_vls_addr(&opt_msg));
	bu_vls_free(&opt_msg);
	return -1;
    }

    VSCALE(target, target, nss->i->local2base);
    VMOVE(nss->i->vals->orig, target);
    _nirt_targ2grid(nss);

    return 0;
}

extern "C" int
_nirt_cmd_shoot(void *ns, int argc, const char **UNUSED(argv))
{
    int i;
    struct nirt_state *nss = (struct nirt_state *)ns;
    if (!ns || !nss->i->ap) return -1;

    if (argc != 1) {
	nerr(nss, "Usage:  s\n");
	return -1;
    }

    /* If we have no active rtip, we don't need to prep or shoot */
    if (!_nirt_get_rtip(nss)) {
	return 0;
    } else {
	if (nss->i->need_reprep) {
	    /* If we need to (re)prep, do it now. Failure is an error. */
	    if (_nirt_raytrace_prep(nss)) {
		nerr(nss, "Error: raytrace prep failed!\n");
		return -1;
	    }
	} else {
	    /* Based on current settings, tell the ap which rtip to use */
	    nss->i->ap->a_rt_i = _nirt_get_rtip(nss);
	    nss->i->ap->a_resource = _nirt_get_resource(nss);
	}
    }

    double bov = _nirt_backout(nss);
    for (i = 0; i < 3; ++i) {
	nss->i->vals->orig[i] = nss->i->vals->orig[i] + (bov * -1*(nss->i->vals->dir[i]));
	nss->i->ap->a_ray.r_pt[i] = nss->i->vals->orig[i];
	nss->i->ap->a_ray.r_dir[i] = nss->i->vals->dir[i];
    }

    ndbg(nss, NIRT_DEBUG_BACKOUT,
	    "Backing out %g units to (%g %g %g), shooting dir is (%g %g %g)\n",
	    bov * nss->i->base2local,
	    nss->i->ap->a_ray.r_pt[0] * nss->i->base2local,
	    nss->i->ap->a_ray.r_pt[1] * nss->i->base2local,
	    nss->i->ap->a_ray.r_pt[2] * nss->i->base2local,
	    V3ARGS(nss->i->ap->a_ray.r_dir));

    // TODO - any necessary initialization for data collection by callbacks
    _nirt_init_ovlp(nss);
    (void)rt_shootray(nss->i->ap);

    // Undo backout
    for (i = 0; i < 3; ++i) {
	nss->i->vals->orig[i] = nss->i->vals->orig[i] - (bov * -1*(nss->i->vals->dir[i]));
    }

    return 0;
}

extern "C" int
_nirt_cmd_backout(void *ns, int argc, const char *argv[])
{
    struct nirt_state *nss = (struct nirt_state *)ns;
    if (!ns) return -1;

    if (argc == 1) {
	nout(nss, "backout = %d\n", nss->i->backout);
	return 0;
    }

    if (argc == 2 && BU_STR_EQUAL(argv[1], "0")) {
	nss->i->backout = 0;
	return 0;
    }
    if (argc == 2 && BU_STR_EQUAL(argv[1], "1")) {
	nss->i->backout = 1;
	return 0;
    }

    nerr(nss, "backout must be set to 0 (off) or 1 (on)\n");

    return -1;
}

extern "C" int
_nirt_cmd_use_air(void *ns, int argc, const char *argv[])
{
    struct nirt_state *nss = (struct nirt_state *)ns;
    struct bu_vls optparse_msg = BU_VLS_INIT_ZERO;
    if (!ns) return -1;
    if (argc == 1) {
	nout(nss, "use_air = %d\n", nss->i->use_air);
	return 0;
    }
    if (argc > 2) {
	nerr(nss, "Usage:  useair %s\n", _nirt_get_desc_args("useair"));
	return -1;
    }
    if (bu_opt_int(&optparse_msg, 1, (const char **)&(argv[1]), (void *)&nss->i->use_air) == -1) {
	nerr(nss, "Error: bu_opt value read failure: %s\n\nUsage:  useair %s\n", bu_vls_addr(&optparse_msg), _nirt_get_desc_args("useair"));
	return -1;
    }
    return 0;
}

extern "C" int
_nirt_cmd_units(void *ns, int argc, const char *argv[])
{
    double ncv = 0.0;
    struct nirt_state *nss = (struct nirt_state *)ns;
    if (!ns) return -1;

    if (argc == 1) {
	nout(nss, "units = '%s'\n", bu_units_string(nss->i->local2base));
	return 0;
    }

    if (argc > 2) {
	nerr(nss, "Usage:  units %s\n", _nirt_get_desc_args("units"));
	return -1;
    }

    if (BU_STR_EQUAL(argv[1], "default")) {
	nss->i->base2local = nss->i->dbip->dbi_base2local;
	nss->i->local2base = nss->i->dbip->dbi_local2base;
	return 0;
    }

    ncv = bu_units_conversion(argv[1]);
    if (ncv <= 0.0) {
	nerr(nss, "Invalid unit specification: '%s'\n", argv[1]);
	return -1;
    }
    nss->i->base2local = 1.0/ncv;
    nss->i->local2base = ncv;

    return 0;
}

extern "C" int
_nirt_cmd_do_overlap_claims(void *ns, int argc, const char *argv[])
{
    int valid_specification = 0;
    struct nirt_state *nss = (struct nirt_state *)ns;
    if (!ns) return -1;

    if (argc == 1) {
	switch (nss->i->overlap_claims) {
	    case NIRT_OVLP_RESOLVE:
		nout(nss, "resolve (0)\n");
		return 0;
	    case NIRT_OVLP_REBUILD_FASTGEN:
		nout(nss, "rebuild_fastgen (1)\n");
		return 0;
	    case NIRT_OVLP_REBUILD_ALL:
		nout(nss, "rebuild_all (2)\n");
		return 0;
	    case NIRT_OVLP_RETAIN:
		nout(nss, "retain (3)\n");
		return 0;
	    default:
		nerr(nss, "Error: overlap_clams is set to invalid value %d\n", nss->i->overlap_claims);
		return -1;
	}
    }

    if (argc > 2) {
	nerr(nss, "Usage:  overlap_claims %s\n", _nirt_get_desc_args("overlap_claims"));
	return -1;
    }

    if (BU_STR_EQUAL(argv[1], "resolve") || BU_STR_EQUAL(argv[1], "0")) {
	nss->i->overlap_claims = NIRT_OVLP_RESOLVE;
	valid_specification = 1;
    }
    if (BU_STR_EQUAL(argv[1], "rebuild_fastgen") || BU_STR_EQUAL(argv[1], "1")) {
	nss->i->overlap_claims = NIRT_OVLP_REBUILD_FASTGEN;
	valid_specification = 1;
    }
    if (BU_STR_EQUAL(argv[1], "rebuild_all") || BU_STR_EQUAL(argv[1], "2")) {
	nss->i->overlap_claims = NIRT_OVLP_REBUILD_ALL;
	valid_specification = 1;
    }
    if (BU_STR_EQUAL(argv[1], "retain") || BU_STR_EQUAL(argv[1], "3")) {
	nss->i->overlap_claims = NIRT_OVLP_RETAIN;
	valid_specification = 1;
    }

    if (!valid_specification) {
	nerr(nss, "Error: Invalid overlap_claims specification: '%s'\n", argv[1]);
	return -1;
    }

    if (nss->i->rtip) nss->i->rtip->rti_save_overlaps = (nss->i->overlap_claims > 0);
    if (nss->i->rtip_air) nss->i->rtip_air->rti_save_overlaps = (nss->i->overlap_claims > 0);

    return 0;
}

extern "C" int
_nirt_cmd_format_output(void *ns, int argc, const char **argv)
{
    struct nirt_state *nss = (struct nirt_state *)ns;
    char type;
    if (!ns) return -1;

    if (argc < 2 || strlen(argv[1]) != 1 || BU_STR_EQUAL(argv[1], "?")) {
	// TODO - need a *much* better help for fmt
	nerr(nss, "Usage:  fmt %s\n", _nirt_get_desc_args("fmt"));
	return -1;
    }

    type = argv[1][0];
    if (!strchr(NIRT_OUTPUT_TYPE_SPECIFIERS, type)) {
	nerr(nss, "Error: unknown fmt type %c\n", type);
	return -1;
    }

    argc--; argv++;
    if (argc == 1) {
	// print out current fmt str for the specified type
	struct bu_vls ostr = BU_VLS_INIT_ZERO;
	switch (type) {
	    case 'r':
		_nirt_print_fmt_str(&ostr, nss->i->fmt_ray);
		break;
	    case 'h':
		_nirt_print_fmt_str(&ostr, nss->i->fmt_head);
		break;
	    case 'p':
		_nirt_print_fmt_str(&ostr, nss->i->fmt_part);
		break;
	    case 'f':
		_nirt_print_fmt_str(&ostr, nss->i->fmt_foot);
		break;
	    case 'm':
		_nirt_print_fmt_str(&ostr, nss->i->fmt_miss);
		break;
	    case 'o':
		_nirt_print_fmt_str(&ostr, nss->i->fmt_ovlp);
		break;
	    case 'g':
		_nirt_print_fmt_str(&ostr, nss->i->fmt_gap);
		break;
	    default:
		nerr(nss, "Error: unknown fmt type %s\n", argv[1]);
		return -1;
		break;
	}
	nout(nss, "%s\n", bu_vls_addr(&ostr));
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

	if ((fmt_cnt = _nirt_split_fmt(fmtstr, &fmts)) <= 0) {
	    nerr(nss, "Error: failure parsing format string \"%s\"\n", fmtstr);
	    return -1;
	}
	argc--; argv++;

	for (int i = 0; i < fmt_cnt; i++) {
	    unsigned int fc = _nirt_fmt_sp_cnt(fmts[i]);
	    const char *key = NULL;
	    if (fc > 0) {
		std::string fs;
		if (!argc) {
		    nerr(nss, "Error: failure parsing format string \"%s\" - missing value for format specifier in substring \"%s\"\n", fmtstr, fmts[i]);
		    return -1;
		}
		key = argv[0];
		fmt_sp_count++;

		// Get fmt_sp substring and perform validation with the matching NIRT value key
		if (_nirt_fmt_sp_get(nss, fmts[i], fs)) return -1;
		if (_nirt_fmt_sp_validate(nss, fs)) return -1;
		if (_nirt_fmt_sp_key_check(nss, key, fs)) return -1;

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
	    nerr(nss, "Error: fmt string \"%s\" has %d format %s, but has %d %s specified\n", fmtstr, fmt_sp_count, (fmt_sp_count == 1) ? "specifier" : "specifiers", fmt_sp_count + argc, ((fmt_sp_count + argc) == 1) ? "key" : "keys");
	    return -1;
	}

	/* OK, validation passed - we should be good.  Scrub and replace pre-existing
	 * formatting instructions with the new. */
set_fmt:
	switch (type) {
	    case 'r':
		nss->i->fmt_ray.clear();
		nss->i->fmt_ray = fmt_tmp;
		break;
	    case 'h':
		nss->i->fmt_head.clear();
		nss->i->fmt_head = fmt_tmp;
		break;
	    case 'p':
		nss->i->fmt_part.clear();
		nss->i->fmt_part = fmt_tmp;
		break;
	    case 'f':
		nss->i->fmt_foot.clear();
		nss->i->fmt_foot = fmt_tmp;
		break;
	    case 'm':
		nss->i->fmt_miss.clear();
		nss->i->fmt_miss = fmt_tmp;
		break;
	    case 'o':
		nss->i->fmt_ovlp.clear();
		nss->i->fmt_ovlp = fmt_tmp;
		break;
	    case 'g':
		nss->i->fmt_gap.clear();
		nss->i->fmt_gap = fmt_tmp;
		break;
	    default:
		nerr(nss, "Error: unknown fmt type %s\n", argv[1]);
		return -1;
		break;
	}
    }

    return 0;
}

extern "C" int
_nirt_cmd_print_item(void *ns, int argc, const char **argv)
{
    int i = 0;
    struct bu_vls oval = BU_VLS_INIT_ZERO;
    struct nirt_state *nss = (struct nirt_state *)ns;
    if (!ns) return -1;

    if (argc == 1) {
	nerr(nss, "Usage:  print %s\n", _nirt_get_desc_args("print"));
	return 0;
    }

    argc--; argv++;

    for (i = 0; i < argc; i++) {
	const char *val_type = bu_avs_get(nss->i->val_types, argv[i]);
	bu_vls_trunc(&oval, 0);
	if (!val_type) {
	    nerr(nss, "Error: item %s is not a valid NIRT value key\n", argv[i]);
	    bu_vls_free(&oval);
	    return -1;
	}
	if (BU_STR_EQUAL(val_type, "INT")) {
	    _nirt_print_fmt_substr(nss, &oval, "%d", argv[i], nss->i->vals, nss->i->base2local);
	    nout(nss, "%s\n", bu_vls_addr(&oval));
	    continue;
	}
	if (BU_STR_EQUAL(val_type, "FLOAT") || BU_STR_EQUAL(val_type, "FNOUNIT")) {
	    _nirt_print_fmt_substr(nss, &oval, "%g", argv[i], nss->i->vals, nss->i->base2local);
	    nout(nss, "%s\n", bu_vls_addr(&oval));
	    continue;
	}
	if (BU_STR_EQUAL(val_type, "STRING")) {
	    _nirt_print_fmt_substr(nss, &oval, "%s", argv[i], nss->i->vals, nss->i->base2local);
	    nout(nss, "%s\n", bu_vls_addr(&oval));
	    continue;
	}
	nerr(nss, "Error: item %s has unknown value type %s\n", argv[i], val_type);
	bu_vls_free(&oval);
	return -1;
    }

    bu_vls_free(&oval);
    return 0;
}

extern "C" int
_nirt_cmd_bot_minpieces(void *ns, int argc, const char **argv)
{
    /* Ew - rt_bot_minpieces is a librt global.  Why isn't it
     * a part of the specific rtip? */
    int ret = 0;
    long minpieces = 0;
    struct bu_vls opt_msg = BU_VLS_INIT_ZERO;
    struct nirt_state *nss = (struct nirt_state *)ns;
    if (!ns) return -1;

    if (argc == 1) {
	nout(nss, "rt_bot_minpieces = %d\n", (unsigned int)rt_bot_minpieces);
	return 0;
    }

    argc--; argv++;

    if (argc > 1) {
	nerr(nss, "Usage:  bot_minpieces %s\n", _nirt_get_desc_args("bot_minpieces"));
	return -1;
    }

    if ((ret = bu_opt_long(&opt_msg, 1, argv, (void *)&minpieces)) == -1) {
	nerr(nss, "Error: bu_opt value read failure reading minpieces value: %s\n", bu_vls_addr(&opt_msg));
	goto bot_minpieces_done;
    }

    if (minpieces < 0) {
	nerr(nss, "Error: rt_bot_minpieces cannot be less than 0\n");
	ret = -1;
	goto bot_minpieces_done;
    }

    if (rt_bot_minpieces != (size_t)minpieces) {
	rt_bot_minpieces = minpieces;
	bu_vls_free(&opt_msg);
	nss->i->need_reprep = 1;
    }

bot_minpieces_done:
    bu_vls_free(&opt_msg);
    return ret;
}

extern "C" int
_nirt_cmd_librt_debug(void *ns, int argc, const char **argv)
{
    int ret = 0;
    long dflg = 0;
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    struct nirt_state *nss = (struct nirt_state *)ns;
    if (!ns) return -1;

    /* Sigh... another librt global, not rtip specific... */
    if (argc == 1) {
	bu_vls_printb(&msg, "librt debug ", RT_G_DEBUG, NIRT_DEBUG_FMT);
	nout(nss, "%s\n", bu_vls_addr(&msg));
	goto librt_nirt_debug_done;
    }

    argc--; argv++;

    if (argc > 1) {
	nerr(nss, "Usage:  libdebug %s\n", _nirt_get_desc_args("libdebug"));
	return -1;
    }

    if ((ret = bu_opt_long_hex(&msg, 1, argv, (void *)&dflg)) == -1) {
	nerr(nss, "%s\n", bu_vls_addr(&msg));
	goto librt_nirt_debug_done;
    } else {
	ret = 0;
    }

    if (dflg < 0 || dflg > (long)UINT32_MAX) {
	nerr(nss, "Error: LIBRT debug flag cannot be less than 0 or greater than %d\n", UINT32_MAX);
	ret = -1;
	goto librt_nirt_debug_done;
    }

    RTG.debug = (uint32_t)dflg;
    bu_vls_printb(&msg, "librt debug ", RTG.debug, NIRT_DEBUG_FMT);
    nout(nss, "%s\n", bu_vls_addr(&msg));

librt_nirt_debug_done:
    bu_vls_free(&msg);
    return ret;
}

extern "C" int
_nirt_cmd_debug(void *ns, int argc, const char **argv)
{
    int ret = 0;
    long dflg = 0;
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    struct nirt_state *nss = (struct nirt_state *)ns;
    if (!ns) return -1;

    if (argc == 1) {
	bu_vls_printb(&msg, "debug ", nss->i->nirt_debug, NIRT_DEBUG_FMT);
	nout(nss, "%s\n", bu_vls_addr(&msg));
	goto nirt_debug_done;
    }

    argc--; argv++;

    if (argc > 1) {
	nerr(nss, "Usage:  debug %s\n", _nirt_get_desc_args("debug"));
	return -1;
    }

    if ((ret = bu_opt_long_hex(&msg, 1, argv, (void *)&dflg)) == -1) {
	nerr(nss, "%s\n", bu_vls_addr(&msg));
	goto nirt_debug_done;
    } else {
	ret = 0;
    }

    if (dflg < 0) {
	nerr(nss, "Error: NIRT debug flag cannot be less than 0\n");
	ret = -1;
	goto nirt_debug_done;
    }

    nss->i->nirt_debug = (unsigned int)dflg;
    bu_vls_printb(&msg, "debug ", nss->i->nirt_debug, NIRT_DEBUG_FMT);
    nout(nss, "%s\n", bu_vls_addr(&msg));

nirt_debug_done:
    bu_vls_free(&msg);
    return ret;
}

extern "C" int
_nirt_cmd_quit(void *ns, int UNUSED(argc), const char **UNUSED(argv))
{
    nmsg((struct nirt_state *)ns, "Quitting...\n");
    return 1;
}

extern "C" int
_nirt_cmd_show_menu(void *ns, int UNUSED(argc), const char **UNUSED(argv))
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
	nout(nss, "%*s %s\n", longest, d->cmd, d->desc);
    }
    return 0;
}

extern "C" int
_nirt_cmd_draw(void *ns, int argc, const char *argv[])
{
    struct nirt_state *nss = (struct nirt_state *)ns;
    if (!ns || argc < 2) return -1;
    size_t orig_size = nss->i->active_paths.size();
    for (int i = 1; i < argc; i++) {
	if (std::find(nss->i->active_paths.begin(), nss->i->active_paths.end(), argv[i]) == nss->i->active_paths.end()) {
	    nss->i->active_paths.push_back(argv[i]);
	}
    }
    if (nss->i->active_paths.size() != orig_size) nss->i->need_reprep = 1;
    return 0;
}

extern "C" int
_nirt_cmd_erase(void *ns, int argc, const char **argv)
{
    struct nirt_state *nss = (struct nirt_state *)ns;
    if (!ns || argc < 2) return -1;
    size_t orig_size = nss->i->active_paths.size();
    for (int i = 1; i < argc; i++) {
	std::vector<std::string>::iterator v_it = std::find(nss->i->active_paths.begin(), nss->i->active_paths.end(), argv[i]);
	if (v_it != nss->i->active_paths.end()) {
	    bu_log("erase_cmd: erasing %s\n", argv[i]);
	    nss->i->active_paths.erase(v_it);
	}
    }
    if (nss->i->active_paths.size() > 0 && nss->i->active_paths.size() != orig_size) nss->i->need_reprep = 1;
    return 0;
}

extern "C" int
_nirt_cmd_state(void *ns, int argc, const char *argv[])
{
    unsigned char rgb[3];
    struct bu_vls optmsg = BU_VLS_INIT_ZERO;
    struct nirt_state *nss = (struct nirt_state *)ns;
    if (!ns) return -1;
    if (argc == 1) {
	// TODO
	return 0;
    }
    argc--; argv++;
    if (BU_STR_EQUAL(argv[0], "oddcolor")) {
	if (argc == 1) {
	    if (bu_color_to_rgb_chars(nss->i->hit_odd_color, (unsigned char *)rgb)) return -1;
	    nout(nss, "%c %c %c", rgb[0], rgb[1], rgb[2]);
	    return 0;
	}
	if (bu_opt_color(&optmsg, argc, argv, (void *)nss->i->hit_odd_color) == -1) {
	    nerr(nss, "Error: bu_opt color read failure reading parsing");
	    for (int i = 0; i < argc; i++) {nerr(nss, " %s", argv[i]);}
	    nerr(nss, ": %s\n", bu_vls_addr(&optmsg));
	    bu_vls_free(&optmsg);
	    return -1;
	}
    }
    if (BU_STR_EQUAL(argv[0], "evencolor")) {
	if (argc == 1) {
	    if (bu_color_to_rgb_chars(nss->i->hit_even_color, (unsigned char *)rgb)) return -1;
	    nout(nss, "%c %c %c", rgb[0], rgb[1], rgb[2]);
	    return 0;
	}
	if (bu_opt_color(&optmsg, argc, argv, (void *)nss->i->hit_even_color) == -1) {
	    nerr(nss, "Error: bu_opt color read failure reading parsing");
	    for (int i = 0; i < argc; i++) {nerr(nss, " %s", argv[i]);}
	    nerr(nss, ": %s\n", bu_vls_addr(&optmsg));
	    bu_vls_free(&optmsg);
	    return -1;
	}

    }
    if (BU_STR_EQUAL(argv[0], "voidcolor")) {
	if (argc == 1) {
	    if (bu_color_to_rgb_chars(nss->i->void_color, (unsigned char *)rgb)) return -1;
	    nout(nss, "%c %c %c", rgb[0], rgb[1], rgb[2]);
	    return 0;
	}
	if (bu_opt_color(&optmsg, argc, argv, (void *)nss->i->void_color) == -1) {
	    nerr(nss, "Error: bu_opt color read failure reading parsing");
	    for (int i = 0; i < argc; i++) {nerr(nss, " %s", argv[i]);}
	    nerr(nss, ": %s\n", bu_vls_addr(&optmsg));
	    bu_vls_free(&optmsg);
	    return -1;
	}

    }
    if (BU_STR_EQUAL(argv[0], "overlapcolor")) {
	if (argc == 1) {
	    if (bu_color_to_rgb_chars(nss->i->overlap_color, (unsigned char *)rgb)) return -1;
	    nout(nss, "%c %c %c", rgb[0], rgb[1], rgb[2]);
	    return 0;
	}
	if (bu_opt_color(&optmsg, argc, argv, (void *)nss->i->overlap_color) == -1) {
	    nerr(nss, "Error: bu_opt color read failure reading parsing");
	    for (int i = 0; i < argc; i++) {nerr(nss, " %s", argv[i]);}
	    nerr(nss, ": %s\n", bu_vls_addr(&optmsg));
	    bu_vls_free(&optmsg);
	    return -1;
	}
    }


    if (BU_STR_EQUAL(argv[0], "plot_overlaps")) {
	if (argc == 1) {
	    nout(nss, "%d", nss->i->plot_overlaps);
	    return 0;
	}
	if (argc == 2 && BU_STR_EQUAL(argv[1], "0")) {
	    nss->i->plot_overlaps = 0;
	    return 0;
	}
	if (argc == 2 && BU_STR_EQUAL(argv[1], "1")) {
	    nss->i->plot_overlaps = 1;
	    return 0;
	}
	nerr(nss, "Error - valid plot_overlaps values are 0 or 1");
	return -1;
    }

    if (BU_STR_EQUAL(argv[0], "out_accumulate")) {
	int setting = 0;
	if (argc == 2) {
	    nout(nss, "%d\n", nss->i->out_accumulate);
	    return 0;
	}
	(void)bu_opt_int(NULL, 1, (const char **)&(argv[2]), (void *)&setting);
	nss->i->out_accumulate = setting;
	return 0;
    }
    if (BU_STR_EQUAL(argv[0], "err_accumulate")) {
	int setting = 0;
	if (argc == 2) {
	    nout(nss, "%d\n", nss->i->err_accumulate);
	    return 0;
	}
	(void)bu_opt_int(NULL, 1, (const char **)&(argv[2]), (void *)&setting);
	nss->i->err_accumulate = setting;
	return 0;
    }
    if (BU_STR_EQUAL(argv[0], "model_bounds")) {
	if (argc != 1) {
	    nerr(nss, "TODO - state cmd help\n");
	    return -1;
	}
	if (nss->i->need_reprep) {
	    if (_nirt_raytrace_prep(nss)) {
		nerr(nss, "Error: raytrace prep failed!\n");
		return -1;
	    }
	}
	if (nss->i->ap->a_rt_i) {
	    double base2local = nss->i->ap->a_rt_i->rti_dbip->dbi_base2local;
	    nout(nss, "model_min = (%g, %g, %g)    model_max = (%g, %g, %g)\n",
		    nss->i->ap->a_rt_i->mdl_min[X] * base2local,
		    nss->i->ap->a_rt_i->mdl_min[Y] * base2local,
		    nss->i->ap->a_rt_i->mdl_min[Z] * base2local,
		    nss->i->ap->a_rt_i->mdl_max[X] * base2local,
		    nss->i->ap->a_rt_i->mdl_max[Y] * base2local,
		    nss->i->ap->a_rt_i->mdl_max[Z] * base2local);
	} else {
	    nout(nss, "model_min = (0, 0, 0)    model_max = (0, 0, 0)\n");
	}
	return 0;
    }
    if (BU_STR_EQUAL(argv[0], "-d")) {
	struct bu_vls dumpstr = BU_VLS_INIT_ZERO;
	bu_vls_printf(&dumpstr, "#  file created by the dump command of nirt\n");
	bu_vls_printf(&dumpstr, "xyz %g %g %g\n", V3ARGS(nss->i->vals->orig));
	bu_vls_printf(&dumpstr, "dir %g %g %g\n", V3ARGS(nss->i->vals->dir));
	bu_vls_printf(&dumpstr, "useair %d\n", nss->i->use_air);
	bu_vls_printf(&dumpstr, "units %s\n", bu_units_string(nss->i->local2base));
	bu_vls_printf(&dumpstr, "overlap_claims ");
	switch (nss->i->overlap_claims) {
	    case NIRT_OVLP_RESOLVE:
		bu_vls_printf(&dumpstr, "resolve\n");
		break;
	    case NIRT_OVLP_REBUILD_FASTGEN:
		bu_vls_printf(&dumpstr, "rebuild_fastgen\n");
		break;
	    case NIRT_OVLP_REBUILD_ALL:
		bu_vls_printf(&dumpstr, "rebuild_all\n");
		break;
	    case NIRT_OVLP_RETAIN:
		bu_vls_printf(&dumpstr, "retain\n");
		break;
	    default:
		bu_vls_printf(&dumpstr, "error: invalid overlap_clams value %d\n", nss->i->overlap_claims);
		break;
	}
	struct bu_vls ostr = BU_VLS_INIT_ZERO;
	_nirt_print_fmt_cmd(&ostr, 'r', nss->i->fmt_ray);
	bu_vls_printf(&dumpstr, "%s", bu_vls_addr(&ostr));
	_nirt_print_fmt_cmd(&ostr, 'h', nss->i->fmt_head);
	bu_vls_printf(&dumpstr, "%s", bu_vls_addr(&ostr));
	_nirt_print_fmt_cmd(&ostr, 'p', nss->i->fmt_part);
	bu_vls_printf(&dumpstr, "%s", bu_vls_addr(&ostr));
	_nirt_print_fmt_cmd(&ostr, 'f', nss->i->fmt_foot);
	bu_vls_printf(&dumpstr, "%s", bu_vls_addr(&ostr));
	_nirt_print_fmt_cmd(&ostr, 'm', nss->i->fmt_miss);
	bu_vls_printf(&dumpstr, "%s", bu_vls_addr(&ostr));
	_nirt_print_fmt_cmd(&ostr, 'o', nss->i->fmt_ovlp);
	bu_vls_printf(&dumpstr, "%s", bu_vls_addr(&ostr));
	_nirt_print_fmt_cmd(&ostr, 'g', nss->i->fmt_gap);
	bu_vls_printf(&dumpstr, "%s", bu_vls_addr(&ostr));
	nout(nss, "%s", bu_vls_addr(&dumpstr));
	bu_vls_free(&dumpstr);
	bu_vls_free(&ostr);
    }

    return 0;
}

const struct bu_cmdtab _libanalyze_nirt_cmds[] = {
    { "attr",           _nirt_cmd_attr},
    { "ae",             _nirt_cmd_az_el},
    { "diff",           _nirt_cmd_diff},
    { "dir",            _nirt_cmd_dir_vect},
    { "hv",             _nirt_cmd_grid_coor},
    { "xyz",            _nirt_cmd_target_coor},
    { "s",              _nirt_cmd_shoot},
    { "backout",        _nirt_cmd_backout},
    { "useair",         _nirt_cmd_use_air},
    { "units",          _nirt_cmd_units},
    { "overlap_claims", _nirt_cmd_do_overlap_claims},
    { "fmt",            _nirt_cmd_format_output},
    { "print",          _nirt_cmd_print_item},
    { "bot_minpieces",  _nirt_cmd_bot_minpieces},
    { "libdebug",       _nirt_cmd_librt_debug},
    { "debug",          _nirt_cmd_debug},
    { "q",              _nirt_cmd_quit},
    { "?",              _nirt_cmd_show_menu},
    // New commands...
    { "draw",           _nirt_cmd_draw},
    { "erase",          _nirt_cmd_erase},
    { "state",          _nirt_cmd_state},
    { (char *)NULL,     NULL}
};




/* Parse command line command and execute */
HIDDEN int
_nirt_exec_cmd(struct nirt_state *ns, const char *cmdstr)
{
    int ac = 0;
    int ac_max = 0;
    int i = 0;
    int ret = 0;
    char **av = NULL;
    size_t q_start, q_end, pos, cmt;
    std::string entry;
    if (!ns || !cmdstr || strlen(cmdstr) > entry.max_size()) return -1;
    std::string s(cmdstr);
    std::stringstream ss(s);
    // get an upper limit on the size of argv
    while (std::getline(ss, entry, ' ')) ac_max++;
    ss.clear();
    ss.seekg(0, ss.beg);

    /* If part of the line is commented, skip that part */
    cmt = _nirt_find_first_unescaped(s, "#", 0);
    if (cmt != std::string::npos) {
	s.erase(cmt);
    }

    _nirt_trim_whitespace(s);
    if (!s.length()) return 0;

    /* Start by initializing the position markers for quoted substrings. */
    q_start = _nirt_find_first_unescaped(s, "\"", 0);
    q_end = (q_start != std::string::npos) ? _nirt_find_first_unescaped(s, "\"", q_start + 1) : std::string::npos;

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
	q_start = _nirt_find_first_unescaped(s, "\"", 0);
	q_end = (q_start != std::string::npos) ? _nirt_find_first_unescaped(s, "\"", q_start + 1) : std::string::npos;
	pos = 0;
    }
    if (s.length() > 0) {
	av[ac] = bu_strdup(s.c_str());
	ac++;
    }
    av[ac] = NULL;

    if (bu_cmd(_libanalyze_nirt_cmds, ac, (const char **)av, 0, (void *)ns, &ret) != BRLCAD_OK) ret = -1;

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
_nirt_parse_script(struct nirt_state *ns, const char *script)
{
    if (!ns || !script) return -1;
    int ret = 0;
    std::string s(script);
    std::string cmd;
    struct bu_vls tcmd = BU_VLS_INIT_ZERO;
    size_t pos = 0;
    size_t q_start, q_end, cmt;

    /* If part of the line is commented, skip that part */
    cmt = _nirt_find_first_unescaped(s, "#", 0);
    if (cmt != std::string::npos) {
	s.erase(cmt);
    }

    _nirt_trim_whitespace(s);
    if (!s.length()) return 0;

    /* Start by initializing the position markers for quoted substrings. */
    q_start = _nirt_find_first_unescaped(s, "\"", 0);
    q_end = (q_start != std::string::npos) ? _nirt_find_first_unescaped(s, "\"", q_start + 1) : std::string::npos;

    /* Slice and dice to get individual commands. */
    while ((pos = _nirt_find_first_unquoted(s, ";", pos)) != std::string::npos) {
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
		ret = _nirt_parse_script(ns, bu_vls_addr(&tcmd));
	    } else {
		ret = _nirt_exec_cmd(ns, bu_vls_addr(&tcmd));
	    }
	    if (ret) goto nps_done;
	}

	/* Prepare the rest of the script string for processing */
	s.erase(0, pos + 1);
	q_start = _nirt_find_first_unescaped(s, "\"", 0);
	q_end = (q_start != std::string::npos) ? _nirt_find_first_unescaped(s, "\"", q_start + 1) : std::string::npos;
	pos = 0;
    }

    /* If there's anything left over, it's a tailing command or a single command */
    bu_vls_sprintf(&tcmd, "%s", s.c_str());
    bu_vls_trimspace(&tcmd);
    if (bu_vls_strlen(&tcmd) > 0) {
	ret = _nirt_exec_cmd(ns, bu_vls_addr(&tcmd));
    }

nps_done:
    bu_vls_free(&tcmd);
    return ret;
}


//////////////////////////
/* Public API functions */
//////////////////////////

extern "C" int
nirt_init(struct nirt_state *ns)
{
    unsigned int rgb[3] = {0, 0, 0};
    struct nirt_state_impl *n = NULL;

    if (!ns) return -1;

    /* Get memory */
    n = new nirt_state_impl;
    ns->i = n;
    n->overlap_claims = NIRT_OVLP_RESOLVE;
    BU_GET(n->hit_odd_color, struct bu_color);
    BU_GET(n->hit_even_color, struct bu_color);
    BU_GET(n->void_color, struct bu_color);
    BU_GET(n->overlap_color, struct bu_color);

    /* Strictly speaking these initializations are more
     * than an "alloc" function needs to do, but we want
     * a nirt state to be "functional" even though it
     * is not set up with a database (it's true/proper
     * initialization.)*/
    bu_color_from_rgb_chars(n->hit_odd_color, (unsigned char *)&rgb);
    bu_color_from_rgb_chars(n->hit_even_color, (unsigned char *)&rgb);
    bu_color_from_rgb_chars(n->void_color, (unsigned char *)&rgb);
    bu_color_from_rgb_chars(n->overlap_color, (unsigned char *)&rgb);
    n->print_header = 0;
    n->print_ident_flag = 0;
    n->rt_debug = 0;
    n->nirt_debug = 0;

    BU_GET(n->out, struct bu_vls);
    bu_vls_init(n->out);
    n->out_accumulate = 0;
    BU_GET(n->msg, struct bu_vls);
    bu_vls_init(n->msg);
    BU_GET(n->err, struct bu_vls);
    bu_vls_init(n->err);
    n->err_accumulate = 0;
    BU_LIST_INIT(&(n->s_vlist));
    n->segs = bn_vlblock_init(&(n->s_vlist), 32);
    n->plot_overlaps = 1;
    n->ret = 0;

    n->h_state = NULL;
    n->h_out = NULL;
    n->h_msg = NULL;
    n->h_err = NULL;
    n->h_segs = NULL;
    n->h_objs = NULL;
    n->h_frmts = NULL;
    n->h_view = NULL;

    n->base2local = 0.0;
    n->local2base = 0.0;
    n->use_air = 0;
    n->backout = 0;

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
    n->need_reprep = 1;

    BU_GET(n->val_types, struct bu_attribute_value_set);
    BU_GET(n->val_docs, struct bu_attribute_value_set);
    bu_avs_init_empty(n->val_types);
    bu_avs_init_empty(n->val_docs);

    BU_GET(n->diff_file, struct bu_vls);
    bu_vls_init(n->diff_file);
    n->cdiff = NULL;
    n->diff_run = 0;
    n->diff_ready = 0;
    BU_GET(n->diff_settings, struct nirt_diff_settings);
    n->diff_settings->report_partitions = 1;
    n->diff_settings->report_misses = 1;
    n->diff_settings->report_gaps = 1;
    n->diff_settings->report_overlaps = 1;
    n->diff_settings->report_partition_reg_ids = 1;
    n->diff_settings->report_partition_reg_names = 1;
    n->diff_settings->report_partition_path_names = 1;
    n->diff_settings->report_partition_dists = 1;
    n->diff_settings->report_partition_obliq = 1;
    n->diff_settings->report_overlap_reg_names = 1;
    n->diff_settings->report_overlap_reg_ids = 1;
    n->diff_settings->report_overlap_dists = 1;
    n->diff_settings->report_overlap_obliq = 1;
    n->diff_settings->report_gap_dists = 1;
    n->diff_settings->dist_delta_tol = BN_TOL_DIST;
    n->diff_settings->obliq_delta_tol = BN_TOL_DIST;
    n->diff_settings->los_delta_tol = BN_TOL_DIST;
    n->diff_settings->scaled_los_delta_tol = BN_TOL_DIST;

    BU_GET(n->vals, struct nirt_output_record);

    VSETALL(n->vals->orig, 0.0);
    n->vals->h = 0.0;
    n->vals->v = 0.0;
    n->vals->d_orig = 0.0;
    n->vals->seg = NULL;


    /* Populate the output key and type information */
    {
	std::string s(nirt_cmd_tbl_defs);
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

    /* Initial direction is -x */
    VSET(n->vals->dir, -1, 0, 0);

    /* Set up the default NIRT formatting output */
    std::ifstream fs;
    std::string fmtline;
    fs.open(bu_brlcad_root("share/nirt/default.nrt", 0), std::fstream::binary);
    if (!fs.is_open()) {
	bu_log("Error - could not load default NIRT formatting file: %s\n", bu_brlcad_root("share/nirt/default.nrt", 0));
	return -1;
    }
    while (std::getline(fs, fmtline)) {
	if (nirt_exec(ns, fmtline.c_str()) < 0) {
	    bu_log("Error - could not load default NIRT formatting file: %s\n", bu_brlcad_root("share/nirt/default.nrt", 0));
	    return -1;
	}
    }

    fs.close();

    return 0;
}

extern "C" int
nirt_init_dbip(struct nirt_state *ns, struct db_i *dbip)
{
    if (!ns || !dbip) return -1;

    RT_CK_DBI(dbip);

    ns->i->dbip = db_clone_dbi(dbip, NULL);

    /* initialize the application structure */
    RT_APPLICATION_INIT(ns->i->ap);
    ns->i->ap->a_hit = _nirt_if_hit;        /* branch to if_hit routine */
    ns->i->ap->a_miss = _nirt_if_miss;      /* branch to if_miss routine */
    ns->i->ap->a_overlap = _nirt_if_overlap;/* branch to if_overlap routine */
    ns->i->ap->a_logoverlap = rt_silent_logoverlap;
    ns->i->ap->a_onehit = 0;               /* continue through shotline after hit */
    ns->i->ap->a_purpose = "NIRT ray";
    ns->i->ap->a_rt_i = _nirt_get_rtip(ns);         /* rt_i pointer */
    ns->i->ap->a_resource = _nirt_get_resource(ns); /* note: resource is initialized by get_rtip */
    ns->i->ap->a_zero1 = 0;           /* sanity check, sayth raytrace.h */
    ns->i->ap->a_zero2 = 0;           /* sanity check, sayth raytrace.h */
    ns->i->ap->a_uptr = (void *)ns;

    /* If we've already got something, go ahead and prep */
    if (ns->i->need_reprep && ns->i->active_paths.size() > 0) {
	if (_nirt_raytrace_prep(ns)) {
	    nerr(ns, "Error - raytrace prep failed during initialization!\n");
	    return -1;
	}
    }

    /* By default use the .g units */
    ns->i->base2local = dbip->dbi_base2local;
    ns->i->local2base = dbip->dbi_local2base;
    return 0;
}


extern "C" int
nirt_clear_dbip(struct nirt_state *ns)
{
    if (!ns) return -1;

    db_close(ns->i->dbip);
    ns->i->dbip = NULL;

    /* clear relevant parts of the application structure */
    if (ns->i->rtip) rt_clean(ns->i->rtip);
    if (ns->i->rtip_air) rt_clean(ns->i->rtip_air);
    ns->i->ap->a_rt_i = NULL;
    ns->i->ap->a_resource = NULL;

    ns->i->active_paths.clear();

    ns->i->base2local = 0.0;
    ns->i->local2base = 0.0;
    return 0;
}

void
nirt_destroy(struct nirt_state *ns)
{
    if (!ns) return;
    bu_vls_free(ns->i->err);
    bu_vls_free(ns->i->msg);
    bu_vls_free(ns->i->out);
    bu_vls_free(ns->i->diff_file);
    bn_vlist_cleanup(&(ns->i->s_vlist));
    bn_vlblock_free(ns->i->segs);

    if (ns->i->rtip != RTI_NULL) rt_free_rti(ns->i->rtip);
    if (ns->i->rtip_air != RTI_NULL) rt_free_rti(ns->i->rtip_air);

    db_close(ns->i->dbip);

    BU_PUT(ns->i->diff_settings, struct nirt_diff_settings);
    BU_PUT(ns->i->vals, struct nirt_output_record);

    BU_PUT(ns->i->res, struct resource);
    BU_PUT(ns->i->res_air, struct resource);
    BU_PUT(ns->i->err, struct bu_vls);
    BU_PUT(ns->i->msg, struct bu_vls);
    BU_PUT(ns->i->out, struct bu_vls);
    BU_PUT(ns->i->diff_file, struct bu_vls);
    BU_PUT(ns->i->ap, struct application);
    BU_PUT(ns->i->hit_odd_color, struct bu_color);
    BU_PUT(ns->i->hit_even_color, struct bu_color);
    BU_PUT(ns->i->void_color, struct bu_color);
    BU_PUT(ns->i->overlap_color, struct bu_color);
    delete ns->i;
}

/* Note - defined inside the execute-once do-while loop to allow for putting a
 * semicolon at the end of a NIRT_HOOK() statement (makes code formatters
 * happier) */
#define NIRT_HOOK(btype,htype) \
    do { if (ns->i->btype && ns->i->htype){(*ns->i->htype)(ns, ns->i->u_data);} } \
    while (0)

int
nirt_exec(struct nirt_state *ns, const char *script)
{
    ns->i->ret = 0;
    ns->i->b_state = false;
    ns->i->b_segs = false;
    ns->i->b_objs = false;
    ns->i->b_frmts = false;
    ns->i->b_view = false;

    /* Unless told otherwise, clear the textual outputs
     * before each nirt_exec call. */
    if (!ns->i->out_accumulate) bu_vls_trunc(ns->i->out, 0);
    if (!ns->i->err_accumulate) bu_vls_trunc(ns->i->err, 0);

    if (script) {
	ns->i->ret = _nirt_parse_script(ns, script);
    } else {
	return 0;
    }

    NIRT_HOOK(b_state, h_state);
    NIRT_HOOK(b_segs, h_segs);
    NIRT_HOOK(b_objs, h_objs);
    NIRT_HOOK(b_frmts, h_frmts);
    NIRT_HOOK(b_view, h_view);

    return ns->i->ret;
}

void *
nirt_udata(struct nirt_state *ns, void *u_data)
{
    if (!ns) return NULL;
    if (u_data) ns->i->u_data = u_data;
    return ns->i->u_data;
}

void
nirt_hook(struct nirt_state *ns, nirt_hook_t hf, int flag)
{
    if (!ns || !hf) return;
    switch (flag) {
	case NIRT_ALL:
	    ns->i->h_state = hf;
	    break;
	case NIRT_OUT:
	    ns->i->h_out = hf;
	    break;
	case NIRT_MSG:
	    ns->i->h_msg = hf;
	    break;
	case NIRT_ERR:
	    ns->i->h_err = hf;
	    break;
	case NIRT_SEGS:
	    ns->i->h_segs = hf;
	    break;
	case NIRT_OBJS:
	    ns->i->h_objs = hf;
	    break;
	case NIRT_FRMTS:
	    ns->i->h_frmts = hf;
	    break;
	case NIRT_VIEW:
	    ns->i->h_view = hf;
	    break;
	default:
	    bu_log("Error: unknown hook type %d\n", flag);
	    return;
    }
}

#define NCFC(nf) ((flags & nf) && !all_set) || (all_set &&  !((flags & nf)))

void
nirt_clear(struct nirt_state *ns, int flags)
{
    int all_set;
    if (!ns) return;
    all_set = (flags & NIRT_ALL) ? 1 : 0;

    if (NCFC(NIRT_OUT)) {
	bu_vls_trunc(ns->i->out, 0);
    }

    if (NCFC(NIRT_ERR)) {
	bu_vls_trunc(ns->i->err, 0);
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
nirt_log(struct bu_vls *o, struct nirt_state *ns, int output_type)
{
    if (!o || !ns) return;
    switch (output_type) {
	case NIRT_ALL:
	    bu_vls_sprintf(o, "%s", "NIRT_ALL: TODO\n");
	    break;
	case NIRT_OUT:
	    bu_vls_strcpy(o, bu_vls_addr(ns->i->out));
	    break;
	case NIRT_MSG:
	    bu_vls_strcpy(o, bu_vls_addr(ns->i->msg));
	    break;
	case NIRT_ERR:
	    bu_vls_strcpy(o, bu_vls_addr(ns->i->err));
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
	    bu_log("Error: unknown output type %d\n", output_type);
	    return;
    }

}

int
nirt_help(struct bu_vls *h, struct nirt_state *ns,  bu_opt_format_t UNUSED(output_type))
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
