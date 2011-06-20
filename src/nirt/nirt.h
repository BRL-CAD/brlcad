/*                          N I R T . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file nirt/nirt.h
 *
 * Common defines and declarations used by nirt.
 *
 * Author:
 * Natalie L. Barker
 *
 * Date:
 * Jan 90
 *
 */

#ifndef __NIRT_H__
#define __NIRT_H__

#ifdef __NetBSD__
#define fmax(x, y) nbsd_fmax(x, y)
static double nbsd_fmax(double x, double y)
{
    return x > y ? x : y;
}
#endif /* __NetBSD__ */


#include "common.h"

#include "raytrace.h" /* for DEBUG_FORMAT */
#include "vmath.h"

#include "./usrfmt.h" /* Needed for some struct definitions */


/** CONSTANTS */
#define VAR_NULL	((struct VarTable *) 0)
#define CT_NULL		((com_table *) 0)
#define SILENT_UNSET	0
#define SILENT_YES	1
#define SILENT_NO	-1
#define NIRT_PROMPT	"nirt>  "
#define TITLE_LEN	80

#define OFF		0
#define ON		1
#define YES		1
#define NO		0
#define HIT		1     /* HIT the target */
#define MISS		0     /* MISS the target */
#define END		2
#define HORZ		0
#define VERT		1
#define DIST		2
#define POS		1
#define NEG		0
#define AIR		1
#define NO_AIR		0
#define READING_FILE	1
#define READING_STRING	2

/** FLAG VALUES FOR overlap_claims */
#define OVLP_RESOLVE		0
#define OVLP_REBUILD_FASTGEN	1
#define OVLP_REBUILD_ALL	2
#define OVLP_RETAIN		3

/** FLAG VALUES FOR nirt_debug */
#define DEBUG_INTERACT	0x001
#define DEBUG_SCRIPTS	0x002
#define DEBUG_MAT	0x004
#define DEBUG_BACKOUT	0x008
#define DEBUG_HITS	0x010

#define RT_DEBUG_FMT	DEBUG_FORMAT
#define DEBUG_FMT	"\020\5HITS\4BACKOUT\3MAT\2SCRIPTS\1INTERACT"

/** STRING FOR USE WITH GETOPT(3) */
#define OPT_STRING      "A:bB:Ee:f:LMO:su:vx:X:?"

#define made_it()	bu_log("Made it to %s:%d\n", __FILE__, __LINE__)

/** MACROS WITH ARGUMENTS */
#define com_usage(c)	fprintf (stderr, "Usage:  %s %s\n", \
				c->com_name, c->com_args);

/** DATA STRUCTURES */
typedef struct {
    char *com_name;		/* for invoking */
    void (*com_func)();          /* what to do?      	         */
    char *com_desc;		/* Help description */
    char *com_args;		/* Command arguments for usage */
} com_table;

struct VarTable {
    double azimuth;
    double elevation;
    vect_t direct;
    vect_t target;
    vect_t grid;
};

typedef struct attributes {
    int attrib_use;
    int attrib_cnt;
    char **attrib;
} attr_table;

extern attr_table a_tab;

extern void ae2dir(void);
extern void attrib_add(char *a, int *prep);
extern void attrib_flush(void);
extern void attrib_print(void);
extern void az_el(char *buffer, com_table *ctp, struct rt_i *rtip);
extern void backout(char *buffer, com_table *ctp, struct rt_i *rtip);
extern void bot_minpieces(char *buffer, com_table *ctp, struct rt_i *rtip);
extern void bot_mintie(char *buffer, com_table *ctp, struct rt_i *rtip);
extern int check_conv_spec(outitem *oip);
extern void default_ospec(void);
extern void dir2ae(void);
extern void dir_vect(char *buffer, com_table *ctp, struct rt_i *rtip);
extern void direct_output(const char* buffer, com_table* ctp, struct rt_i *rtip);
extern void do_overlap_claims();
extern void do_rt_gettrees(struct rt_i *, char **object_name, int nm_objects, int *do_prep);
extern void dump_state(const char* buffer, com_table* ctp, struct rt_i *rtip);
extern void format_output(const char* buffer, com_table* ctp, struct rt_i *rtip);
extern com_table *get_comtab_ent(char *pattern, int pat_len);
extern void grid2targ(void);
extern void grid_coor(char *buffer, com_table *ctp, struct rt_i *rtip);
extern void interact(int input_source, void *sPtr, struct rt_i *rtip);
extern void load_state(char *buffer, com_table *ctp, struct rt_i *rtip);
extern void nirt_units();
extern void print_item(char *buffer, com_table *ctp, struct rt_i *rtip);
extern void printusage(void);
extern void quit();
extern void read_mat(struct rt_i *rtip);
extern void report(int outcom_type);
extern void set_diameter(struct rt_i *);
extern void sh_esc(char *buffer);
extern void shoot(char *buffer, com_table *ctp, struct rt_i *rtip);
extern void show_menu();
extern void state_file(const char* buffer, com_table* ctp, struct rt_i *rtip);
extern void targ2grid(void);
extern void target_coor(char *buffer, com_table *ctp, struct rt_i *rtip);
extern void use_air(char *buffer, com_table *ctp, struct rt_i *rtip);

/* main driver needs to get at these, even though they're command-specific */
extern void cm_libdebug();
extern void cm_debug();
extern void cm_attr();


#endif /* __NIRT_H__ */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
