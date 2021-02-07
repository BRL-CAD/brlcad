/*                          N I R T . H
 * BRL-CAD
 *
 * Copyright (c) 2020-2021 United States Government as represented by
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
/** @file nirt.h
 *
 * Header containing internal definitions for Natalie's Interactive Raytracer
 * implementation.
 *
 */

/* BRL-CAD includes */
#include "common.h"

#include <algorithm>
#include <string>
#include <sstream>

/* needed on mac in c90 mode */
#ifndef HAVE_DECL_FSEEKO
#include "bio.h" /* for b_off_t */
extern "C" int fseeko(FILE *, b_off_t, int);
extern "C" b_off_t ftello(FILE *);
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

#include "bu/debug.h"
#include "nmg/debug.h"
#include "rt/debug.h"

/* NIRT segment types */
#define NIRT_MISS_SEG      1    /**< @brief Ray segment representing a miss */
#define NIRT_PARTITION_SEG 2    /**< @brief Ray segment representing a solid region */
#define NIRT_OVERLAP_SEG   3    /**< @brief Ray segment representing an overlap region */
#define NIRT_GAP_SEG       4    /**< @brief Ray segment representing a gap */
#define NIRT_ALL_SEG       5    /**< @brief Enable all values (for print) */

#define NIRT_SILENT_UNSET    0
#define NIRT_SILENT_YES      1
#define NIRT_SILENT_NO       -1

#define NIRT_OVLP_RESOLVE            0
#define NIRT_OVLP_REBUILD_FASTGEN    1
#define NIRT_OVLP_REBUILD_ALL        2
#define NIRT_OVLP_RETAIN             3

#define NIRT_PRINTF_SPECIFIERS "difeEgGs"
#define NIRT_OUTPUT_TYPE_SPECIFIERS "rhpfmog"

#define HELPFLAG "--print-help"
#define PURPOSEFLAG "--print-purpose"

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
    int type = 0;
    point_t in;
    fastf_t d_in;
    point_t out;
    fastf_t d_out;
    fastf_t los;
    fastf_t scaled_los;
    std::string path_name;
    std::string reg_name;
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
    std::string ov_reg1_name;
    int ov_reg1_id;
    std::string ov_reg2_name;
    int ov_reg2_id;
    std::string ov_sol_in;
    std::string ov_sol_out;
    fastf_t ov_los;
    point_t ov_in;
    fastf_t ov_d_in;
    point_t ov_out;
    fastf_t ov_d_out;
    int surf_num_in;
    int surf_num_out;
    int claimant_count;
    std::string claimant_list;
    std::string claimant_listn; /* uses \n instead of space to separate claimants */
    std::string attributes;
    point_t gap_in;
    fastf_t gap_los;
};

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


struct nirt_diff_state;

class nirt_fmt_state {
    public:
	std::vector<std::pair<std::string,std::string> > ray;
	std::vector<std::pair<std::string,std::string> > head;
	std::vector<std::pair<std::string,std::string> > part;
	std::vector<std::pair<std::string,std::string> > foot;
	std::vector<std::pair<std::string,std::string> > miss;
	std::vector<std::pair<std::string,std::string> > ovlp;
	std::vector<std::pair<std::string,std::string> > gap;

	void clear() {
	    ray.clear();
	    head.clear();
	    part.clear();
	    foot.clear();
	    miss.clear();
	    ovlp.clear();
	    gap.clear();
	};
};

struct nirt_state_impl {
    /* Output options */
    struct bu_color *hit_odd_color;
    struct bu_color *hit_even_color;
    struct bu_color *void_color;
    struct bu_color *overlap_color;
    int print_header;
    int print_ident_flag;

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
    struct nirt_diff_state *diff_state;

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
    nirt_fmt_state fmt;

    void *u_data; // user data
};

/**************************
 * Internal functionality *
 **************************/

void nmsg(struct nirt_state *nss, const char *fmt, ...) _BU_ATTR_PRINTF23;
void nout(struct nirt_state *nss, const char *fmt, ...) _BU_ATTR_PRINTF23;
void nerr(struct nirt_state *nss, const char *fmt, ...) _BU_ATTR_PRINTF23;
void ndbg(struct nirt_state *nss, int flag, const char *fmt, ...);

size_t _nirt_find_first_unescaped(std::string &s, const char *keys, int offset);
size_t _nirt_find_first_unquoted(std::string &ts, const char *key, size_t offset);
void _nirt_trim_whitespace(std::string &s);
std::vector<std::string> _nirt_string_split(std::string s);

/* Based on tolerance, how many digits should we print? */
int _nirt_digits(fastf_t ftol);

/*
 * References for string to-from floating point issues:
 * https://stackoverflow.com/a/22458961
 * http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2006/n2005.pdf
 * https://docs.oracle.com/cd/E19957-01/806-3568/ncg_goldberg.html
 */
std::string _nirt_dbl_to_str(double d, size_t p);
double _nirt_str_to_dbl(std::string s, size_t p);

int _nirt_str_to_int(std::string s);

void _nirt_targ2grid(struct nirt_state *nss);

void _nirt_dir2ae(struct nirt_state *nss);

struct rt_i * _nirt_get_rtip(struct nirt_state *nss);
struct resource * _nirt_get_resource(struct nirt_state *nss);
void _nirt_init_ovlp(struct nirt_state *nss);
int _nirt_raytrace_prep(struct nirt_state *nss);


void _nirt_diff_create(struct nirt_state *nss);
void _nirt_diff_destroy(struct nirt_state *nss);
void _nirt_diff_add_seg(struct nirt_state *nss, struct nirt_seg *nseg);
extern "C" int _nirt_cmd_diff(void *ns, int argc, const char *argv[]);


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
