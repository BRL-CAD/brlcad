/*                      J O I N T . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2014 United States Government as represented by
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
/** @file libged/joint.c
 *
 * Process all animation edit commands.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>


#include "bu/getopt.h"
#include "dg.h"
#include "solid.h"
#include "raytrace.h"

#include "./joint.h"
#include "./ged_private.h"

static unsigned int J_DEBUG = 0;
#define DEBUG_J_MESH	0x00000001
#define DEBUG_J_LOAD	0x00000002
#define DEBUG_J_MOVE	0x00000004
#define DEBUG_J_SOLVE	0x00000008
#define DEBUG_J_EVAL	0x00000010
#define DEBUG_J_SYSTEM	0x00000020
#define DEBUG_J_PARSE	0x00000040
#define DEBUG_J_LEX	0x00000080
#define J_DEBUG_FORMAT \
    "\020\10LEX\7PARSE\6SYSTEM\5EVAL\4SOLVE\3MOVE\2LOAD\1MESH"

/* max object name length we expect to encounter for each arc node */
#define MAX_OBJ_NAME 255

extern struct funtab joint_tab[];


struct artic_grips {
    struct bu_list l;
    vect_t vert;
    struct directory *dir;
};
#define MAGIC_A_GRIP 0x414752aa
struct artic_joints {
    struct bu_list l;
    struct bu_list head;
    struct joint *joint;
};
#define MAGIC_A_JOINT 0x414A4F55

struct bu_list artic_head = {
    BU_LIST_HEAD_MAGIC,
    &artic_head, &artic_head
};


struct bu_list joint_head = {
    BU_LIST_HEAD_MAGIC,
    &joint_head, &joint_head
};


struct bu_list hold_head = {
    BU_LIST_HEAD_MAGIC,
    &hold_head, &hold_head
};


struct joint *
findjoint(struct ged *gedp, const struct db_full_path *pathp)
{
    size_t i, j;
    struct joint *jp;
    int best;
    struct joint *bestjp = NULL;

    if (J_DEBUG & DEBUG_J_MESH) {
	char *sofar = db_path_to_string(pathp);

	bu_vls_printf(gedp->ged_result_str, "joint mesh: PATH = '%s'\n", sofar);
	bu_free(sofar, "path string");
    }

    best = -1;
    for (BU_LIST_FOR(jp, joint, &joint_head)) {
	for (i=0; i< pathp->fp_len; i++) {
	    int good=1;
	    if (jp->path.arc_last+i >= pathp->fp_len)
		break;
	    for (j=0; j<=(size_t)jp->path.arc_last;j++) {
		if ((*pathp->fp_names[i+j]->d_namep != *jp->path.arc[j]) ||
		    (!BU_STR_EQUAL(pathp->fp_names[i+j]->d_namep, jp->path.arc[j]))) {
		    good=0;
		    break;
		}
	    }

	    if (good && (long)j>best) {
		best = j;
		bestjp = jp;
	    }
	}
    }
    if (best > 0) {
	if (J_DEBUG & DEBUG_J_MESH) {
	    bu_vls_printf(gedp->ged_result_str, "joint mesh: returning joint '%s'\n", bestjp->name);
	}
	return bestjp;
    }

    if (J_DEBUG & DEBUG_J_MESH) {
	bu_vls_printf(gedp->ged_result_str, "joint mesh: returning joint 'NULL'\n");
    }
    return (struct joint *) 0;
}


HIDDEN union tree *
mesh_leaf(struct db_tree_state *UNUSED(tsp), const struct db_full_path *pathp, struct rt_db_internal *ip, void *client_data)
{
    struct ged *gedp = (struct ged *)client_data;
    struct rt_grip_internal *gip;
    struct artic_joints *newJoint;
    struct artic_grips *newGrip;
    struct joint *jp;
    union tree *curtree;
    struct directory *dp;

    RT_CK_FULL_PATH(pathp);
    RT_CK_DB_INTERNAL(ip);

    if (ip->idb_type != ID_GRIP) {
	return TREE_NULL;
    }

    BU_ALLOC(curtree, union tree);
    RT_TREE_INIT(curtree);
    curtree->tr_op = OP_SOLID;
    curtree->tr_op = OP_NOP;
    dp = pathp->fp_names[pathp->fp_len-1];
/*
 * get the grip information.
 */
    gip = (struct rt_grip_internal *) ip->idb_ptr;
/*
 * find the joint that this grip belongs to.
 */
    jp = findjoint(gedp, pathp);
/*
 * Get the grip structure.
 */
    BU_ALLOC(newGrip, struct artic_grips);
    newGrip->l.magic = MAGIC_A_GRIP;
    VMOVE(newGrip->vert, gip->center);
    newGrip->dir = dp;

    for (BU_LIST_FOR(newJoint, artic_joints, &artic_head)) {
	if (newJoint->joint == jp) {
	    BU_LIST_APPEND(&newJoint->head, &(newGrip->l));
	    return curtree;
	}
    }
/*
 * we need a new joint thingie.
 */
    BU_ALLOC(newJoint, struct artic_joints);
    newJoint->l.magic = MAGIC_A_JOINT;
    newJoint->joint = jp;

    BU_LIST_INIT(&newJoint->head);
    BU_LIST_APPEND(&artic_head, &(newJoint->l));
    BU_LIST_APPEND(&newJoint->head, &(newGrip->l));

    return curtree;
}


HIDDEN union tree *
mesh_end_region (struct db_tree_state *UNUSED(tsp), const struct db_full_path *UNUSED(pathp), union tree *curtree, void *UNUSED(client_data))
{
    return curtree;
}
static struct db_tree_state mesh_initial_tree_state = {
    RT_DBTS_MAGIC,		/* magic */
    0,			/* ts_dbip */
    0,			/* ts_sofar */
    0, 0, 0,			/* region, air, gmater */
    100,			/* GIFT los */
    {
	/* struct mater_info ts_mater */
	{1.0, 0.0, 0.0},	/* color, RGB */
	-1.0,		/* Temperature */
	0,		/* override */
	0,		/* color inherit */
	0,		/* mater inherit */
	(char *)NULL	/* shader */
    },
    MAT_INIT_IDN,
    REGION_NON_FASTGEN,		/* ts_is_fastgen */
    {
	/* attribute value set */
	BU_AVS_MAGIC,
	0,
	0,
	NULL,
	NULL,
	NULL
    }
    ,
    0,				/* ts_stop_at_regions */
    NULL,				/* ts_region_start_func */
    NULL,				/* ts_region_end_func */
    NULL,				/* ts_leaf_func */
    NULL,				/* ts_ttol */
    NULL,				/* ts_tol */
    NULL,				/* ts_m */
    NULL,				/* ts_rtip */
    NULL				/* ts_resp */
};


static int
joint_mesh(struct ged *gedp, int argc, const char *argv[])
{
    const char *name;
    struct bn_vlblock*vbp;
    struct bu_list *vhead;
    struct artic_joints *jp;
    struct artic_grips *gp, *gpp;
    int i;
    char *topv[2000];
    int topc;

    if (gedp->ged_wdbp->dbip == DBI_NULL)
	return GED_OK;

    if (argc <= 2) {
	name = "_ANIM_";
    } else {
	name = argv[2];
    }

    topc = ged_build_tops(gedp, topv, topv+2000);
    {
	struct ged_display_list *gdlp;
	struct ged_display_list *next_gdlp;
	struct solid *sp;

	gdlp = BU_LIST_NEXT(ged_display_list, gedp->ged_gdp->gd_headDisplay);
	while (BU_LIST_NOT_HEAD(gdlp, gedp->ged_gdp->gd_headDisplay)) {
	    next_gdlp = BU_LIST_PNEXT(ged_display_list, gdlp);

	    FOR_ALL_SOLIDS(sp, &gdlp->gdl_headSolid) {
		sp->s_iflag=DOWN;
	    }

	    gdlp = next_gdlp;
	}
    }

    i = db_walk_tree(gedp->ged_wdbp->dbip, topc, (const char **)topv,
		     1,			/* Number of cpus */
		     &mesh_initial_tree_state,
		     0,			/* Begin region */
		     mesh_end_region,	/* End region */
		     mesh_leaf,		/* node */
		     (void *)gedp);

    /*
     * Now we draw the overlays.  We do this by building a
     * mesh from each grip to every other grip in that list.
     */
    vbp = rt_vlblock_init();
    vhead = rt_vlblock_find(vbp, 0x00, 0xff, 0xff);

    for (BU_LIST_FOR(jp, artic_joints, &artic_head)) {
	i=0;
	for (BU_LIST_FOR(gp, artic_grips, &jp->head)) {
	    i++;
	    for (gpp=BU_LIST_NEXT(artic_grips, &(gp->l));
		 BU_LIST_NOT_HEAD(gpp, &(jp->head));
		 gpp=BU_LIST_NEXT(artic_grips, &(gpp->l))) {
		RT_ADD_VLIST(vhead, gp->vert, BN_VLIST_LINE_MOVE);
		RT_ADD_VLIST(vhead, gpp->vert, BN_VLIST_LINE_DRAW);
	    }
	}
	if (J_DEBUG & DEBUG_J_MESH) {
	    bu_vls_printf(gedp->ged_result_str, "joint mesh: %s has %d grips.\n",
			  (jp->joint) ? jp->joint->name: "UNGROUPED", i);
	}
    }

    _ged_cvt_vlblock_to_solids(gedp, vbp, name, 0);

    rt_vlblock_free(vbp);
    while (BU_LIST_WHILE(jp, artic_joints, &artic_head)) {
	while (BU_LIST_WHILE(gp, artic_grips, &jp->head)) {
	    BU_LIST_DEQUEUE(&gp->l);
	    bu_free((void *)gp, "artic_grip");
	}
	BU_LIST_DEQUEUE(&jp->l);
	bu_free((void *)jp, "Artic Joint");
    }
    return GED_OK;
}


static int
joint_debug(struct ged *gedp,
	    int argc,
	    const char *argv[])
{
    if (argc >= 2) {
	sscanf(argv[1], "%x", &J_DEBUG);
    } else {
	bu_vls_printb(gedp->ged_result_str, "possible flags", 0xffffffffL, J_DEBUG_FORMAT);
	bu_vls_printf(gedp->ged_result_str, "\n");
    }
    bu_vls_printb(gedp->ged_result_str, "J_DEBUG", J_DEBUG, J_DEBUG_FORMAT);
    bu_vls_printf(gedp->ged_result_str, "\n");

    return GED_OK;
}


/**
 * Common code for help commands
 */
HIDDEN int
helpcomm(struct ged *gedp, int argc, const char *argv[], struct funtab *functions)
{
    struct funtab *ftp;
    int i, bad;

    bad = 0;

    /* Help command(s) */
    for (i=1; i<argc; i++) {
	for (ftp = functions+1; ftp->ft_name; ftp++) {
	    if (!BU_STR_EQUAL(ftp->ft_name, argv[i]))
		continue;

	    bu_vls_printf(gedp->ged_result_str, "Usage: %s%s %s\n\t(%s)\n", functions->ft_name, ftp->ft_name, ftp->ft_parms, ftp->ft_comment);
	    break;
	}
	if (!ftp->ft_name) {
	    bu_vls_printf(gedp->ged_result_str, "%s%s : no such command, type '%s?' for help\n", functions->ft_name, argv[i], functions->ft_name);
	    bad = 1;
	}
    }

    return bad ? GED_ERROR : GED_OK;
}


/**
 * Print a help message, two lines for each command.  Or, help with
 * the indicated commands.
 */
static int
joint_usage(struct ged *gedp, int argc, const char *argv[], struct funtab *functions)
{
    struct funtab *ftp;

    if (argc <= 1) {
	bu_vls_printf(gedp->ged_result_str, "The following commands are available:\n");
	for (ftp = functions+1; ftp->ft_name; ftp++) {
	    bu_vls_printf(gedp->ged_result_str, "%s%s %s\n\t (%s)\n", functions->ft_name, ftp->ft_name, ftp->ft_parms, ftp->ft_comment);
	}
	return GED_OK;
    }
    return helpcomm(gedp, argc, argv, functions);
}


static int
joint_command_tab(struct ged *gedp, int argc, const char *argv[], struct funtab *functions)
{
    struct funtab *ftp;

    if (argc <= 1) {
	bu_vls_printf(gedp->ged_result_str, "The following %s subcommands are available:\n", functions->ft_name);
	for (ftp = functions+1; ftp->ft_name; ftp++) {
	    vls_col_item(gedp->ged_result_str, ftp->ft_name);
	}
	vls_col_eol(gedp->ged_result_str);
	bu_vls_printf(gedp->ged_result_str, "\n");
	return GED_OK;
    }
    return helpcomm(gedp, argc, argv, functions);
}


static int
joint_help_commands(struct ged *gedp, int argc, const char *argv[])
{
    int status;

    status = joint_command_tab(gedp, argc, argv, &joint_tab[0]);

    if (status == GED_OK)
	return GED_OK;
    else
	return GED_ERROR;
}


static int
joint_help(struct ged *gedp, int argc, const char *argv[])
{
    int status;

    status = joint_usage(gedp, argc, argv, &joint_tab[0]);

    if (status == GED_OK)
	return GED_OK;
    else
	return GED_ERROR;
}


static struct joint *
joint_lookup(const char *name)
{
    struct joint *jp;

    for (BU_LIST_FOR(jp, joint, &joint_head)) {
	if (BU_STR_EQUAL(jp->name, name)) {
	    return jp;
	}
    }
    return (struct joint *) 0;
}


static void
free_arc(struct arc *ap)
{
    int i;

    if (!ap || ap->type == ARC_UNSET)
	return;
    for (i=0; i<=ap->arc_last; i++) {
	bu_free((void *)ap->arc[i], "arc entry");
    }
    bu_free((void *)ap->arc, "arc table");
    ap->arc = (char **)0;
    if (ap->type & ARC_BOTH) {
	for (i=0; i<=ap->arc_last; i++) {
	    bu_free((void *)ap->original[i], "arc entry");
	}
	bu_free((void *)ap->original, "arc table");
    }
    ap->type=ARC_UNSET;
}


static void
free_joint(struct joint *jp)
{
    free_arc(&jp->path);
    if (jp->name)
	bu_free((void *)jp->name, "joint name");
    BU_PUT(jp, struct joint);
}


static void
free_hold(struct hold *hp)
{
    struct jointH *jh;

    if (!hp || hp->l.magic != MAGIC_HOLD_STRUCT)
	return;
    if (hp->objective.type != ID_FIXED) {
	if (hp->objective.path.fp_len) {
	    db_free_full_path(&hp->objective.path);
	}
	free_arc(&hp->objective.arc);
    }
    if (hp->effector.type != ID_FIXED) {
	if (hp->effector.path.fp_len) {
	    db_free_full_path(&hp->effector.path);
	}
	free_arc(&hp->effector.arc);
    }
    while (BU_LIST_WHILE(jh, jointH, &hp->j_head)) {
	jh->p->uses--;
	BU_LIST_DEQUEUE(&jh->l);
	BU_PUT(jh, struct jointH);
    }
    if (hp->joint)
	bu_free((void *)hp->joint, "hold joint name");
    if (hp->name)
	bu_free((void *)hp->name, "hold name");
    BU_PUT(hp, struct hold);
}


static int
hold_point_location(struct ged *gedp, fastf_t *loc, struct hold_point *hp)
{
    mat_t mat;
    struct joint *jp;
    struct rt_grip_internal *gip;
    struct rt_db_internal intern;

    if (gedp->ged_wdbp->dbip == DBI_NULL)
	return 1;

    VSETALL(loc, 0.0);	/* default is the origin. */
    switch (hp->type) {
	case ID_FIXED:
	    VMOVE(loc, hp->point);
	    return 1;
	case ID_GRIP:
	    if (hp->flag & HOLD_PT_GOOD) {
		db_path_to_mat(gedp->ged_wdbp->dbip, &hp->path, mat, hp->path.fp_len-2, &rt_uniresource);
		MAT4X3PNT(loc, mat, hp->point);
		return 1;
	    }
	    if (!hp->path.fp_names) {
		bu_vls_printf(gedp->ged_result_str, "hold_point_location: null pointer! '%s' not found!\n", "hp->path.fp_names");
		bu_bomb("this shouldn't happen\n");
	    }
	    if (rt_db_get_internal(&intern, hp->path.fp_names[hp->path.fp_len-1], gedp->ged_wdbp->dbip, NULL, &rt_uniresource) < 0)
		return 0;

	    RT_CK_DB_INTERNAL(&intern);
	    if (intern.idb_type != ID_GRIP)
		return 0;
	    gip = (struct rt_grip_internal *)intern.idb_ptr;
	    VMOVE(hp->point, gip->center);
	    hp->flag |= HOLD_PT_GOOD;
	    rt_db_free_internal(&intern);

	    db_path_to_mat(gedp->ged_wdbp->dbip, &hp->path, mat, hp->path.fp_len-2, &rt_uniresource);
	    MAT4X3PNT(loc, mat, hp->point);
	    return 1;
	case ID_JOINT:
	    db_path_to_mat(gedp->ged_wdbp->dbip, &hp->path, mat, hp->path.fp_len-3, &rt_uniresource);
	    if (hp->flag & HOLD_PT_GOOD) {
		MAT4X3VEC(loc, mat, hp->point);
		return 1;
	    }
	    jp = joint_lookup(hp->arc.arc[hp->arc.arc_last]);
	    if (!jp) {
		bu_vls_printf(gedp->ged_result_str, "hold_point_location: Lost joint! %s not found!\n",
			      hp->arc.arc[hp->arc.arc_last]);
		return 0;
	    }
	    VMOVE(hp->point, jp->location);
	    hp->flag |= HOLD_PT_GOOD;
	    MAT4X3VEC(loc, mat, hp->point);
	    return 1;
    }
    /* NEVER REACHED */
    return 1;	/* For the picky compilers */
}


static char *
hold_point_to_string(struct ged *gedp, struct hold_point *hp)
{
#define HOLD_POINT_TO_STRING_LEN 1024
    char *text = (char *)bu_malloc(HOLD_POINT_TO_STRING_LEN, "hold_point_to_string");
    char *path;
    vect_t loc = VINIT_ZERO;

    switch (hp->type) {
	case ID_FIXED:
	    sprintf(text, "(%g %g %g)", hp->point[X],
		    hp->point[Y], hp->point[Z]);
	    break;
	case ID_GRIP:
	case ID_JOINT:
	    (void)hold_point_location(gedp, loc, hp);
	    path = db_path_to_string(&hp->path);
	    snprintf(text, HOLD_POINT_TO_STRING_LEN, "%s (%g %g %g)", path, loc[X], loc[Y], loc[Z]);
	    bu_free(path, "full path");
	    break;
    }
    return text;
}


static double
hold_eval(struct ged *gedp, struct hold *hp)
{
    int i;
    vect_t e_loc = VINIT_ZERO;
    vect_t o_loc = VINIT_ZERO;
    double value;
    struct directory *dp;

    if (!hp->effector.path.fp_names) {
	db_free_full_path(&hp->effector.path); /* sanity */
	for (i=0; i<= hp->effector.arc.arc_last; i++) {
	    dp = db_lookup(gedp->ged_wdbp->dbip, hp->effector.arc.arc[i], LOOKUP_NOISY);
	    if (!dp) {
		continue;
	    }
	    db_add_node_to_full_path(&hp->effector.path, dp);
	}
    }
    if (!hp->objective.path.fp_names) {
	db_free_full_path(&hp->objective.path); /* sanity */
	for (i=0; i<= hp->objective.arc.arc_last; i++) {
	    dp = db_lookup(gedp->ged_wdbp->dbip, hp->objective.arc.arc[i], LOOKUP_NOISY);
	    if (!dp) {
		continue;
	    }
	    db_add_node_to_full_path(&hp->objective.path, dp);
	}
    }

    /*
     * get the current location of the effector.
     */
    if (!hold_point_location(gedp, e_loc, &hp->effector)) {
	if (J_DEBUG & DEBUG_J_EVAL) {
	    bu_vls_printf(gedp->ged_result_str, "hold_eval: unable to find location of effector for %s.\n",
			  hp->name);
	}
	return 0.0;
    }
    if (!hold_point_location(gedp, o_loc, &hp->objective)) {
	if (J_DEBUG & DEBUG_J_EVAL) {
	    bu_vls_printf(gedp->ged_result_str, "hold_eval: unable to find location of objective for %s.\n",
			  hp->name);
	}
	return 0.0;
    }
    value = hp->weight * DIST_PT_PT(e_loc, o_loc);
    if (J_DEBUG & DEBUG_J_EVAL) {
	bu_vls_printf(gedp->ged_result_str, "hold_eval: PT->PT of %s is %g\n", hp->name, value);
    }
    return value;
}


static void
print_hold(struct ged *gedp, struct hold *hp)
{
    char *t1, *t2;

    t1 = hold_point_to_string(gedp, &hp->effector);
    t2 = hold_point_to_string(gedp, &hp->objective);

    bu_vls_printf(gedp->ged_result_str, "holds:\t%s with %s\n\tfrom:%s\n\tto:%s\n", (hp->name) ? hp->name : "UNNAMED", hp->joint, t1, t2);
    bu_free(t1, "hold_point_to_string");
    bu_free(t2, "hold_point_to_string");

    {
	bu_vls_printf(gedp->ged_result_str, "\n\twith a weight: %g, pull %g\n",
		      hp->weight, hold_eval(gedp, hp));
    }
}


static void
hold_clear_flags(struct hold *hp)
{
    hp->effector.flag = hp->objective.flag = 0;
}


static int
joint_unload(struct ged *gedp, int argc, const char *argv[])
{
    struct joint *jp;
    struct hold *hp;
    int joints, holds;

    if (gedp->ged_wdbp->dbip == DBI_NULL) {
	bu_vls_printf(gedp->ged_result_str, "A database is not open!\n");
	return GED_ERROR;
    }

    if (argc > 1) {
	bu_vls_printf(gedp->ged_result_str, "Unexpected parameter [%s]\n", argv[1]);
    }

    db_free_anim(gedp->ged_wdbp->dbip);
    holds = 0;
    while (BU_LIST_WHILE(hp, hold, &hold_head)) {
	holds++;
	BU_LIST_DEQUEUE(&hp->l);
	if (J_DEBUG & DEBUG_J_LOAD) {
	    bu_vls_printf(gedp->ged_result_str, "joint: unloading '%s' constraint\n", hp->name);
	    bu_vls_printf(gedp->ged_result_str, "===begin %s===\n", hp->name);
	    print_hold(gedp, hp);
	    bu_vls_printf(gedp->ged_result_str, "===end %s===\n", hp->name);
	}
	free_hold(hp);
    }
    joints = 0;
    while (BU_LIST_WHILE(jp, joint, &joint_head)) {
	joints++;
	BU_LIST_DEQUEUE(&(jp->l));
	if (J_DEBUG & DEBUG_J_LOAD) {
	    bu_vls_printf(gedp->ged_result_str, "joint: unloading '%s' joint\n", jp->name);
	}
	free_joint(jp);
    }
    if (J_DEBUG & DEBUG_J_LOAD) {
	bu_vls_printf(gedp->ged_result_str, "joint: unloaded %d joints, %d constraints.\n",
		      joints, holds);
    }

    return GED_OK;
}


#define KEY_JOINT	1
#define KEY_CON		2
#define KEY_ARC		3
#define KEY_LOC		4
#define KEY_TRANS	5
#define KEY_ROT		6
#define KEY_LIMIT	7
#define KEY_UP		8
#define KEY_LOW		9
#define KEY_CUR		10
#define KEY_ACC		11
#define KEY_DIR		12
#define KEY_UNITS	13
#define KEY_JOINTS	14
#define KEY_START	15
#define KEY_PATH	16
#define KEY_WEIGHT	17
#define KEY_PRI		18
#define KEY_EFF		19
#define KEY_POINT	20
#define KEY_EXCEPT	21
#define KEY_INF		22
#define KEY_VERTEX	23

static struct bu_lex_key animkeys[] = {
    {KEY_JOINT, "joint"},
    {KEY_CON,   "constraint"},
    {KEY_ARC,   "arc"},
    {KEY_LOC,   "location"},
    {KEY_TRANS, "translate"},
    {KEY_ROT,   "rotate"},
    {KEY_LIMIT, "limits"},
    {KEY_UP,    "upper"},
    {KEY_LOW,   "lower"},
    {KEY_CUR,   "current"},
    {KEY_ACC,   "accepted"},
    {KEY_DIR,   "direction"},
    {KEY_UNITS, "units"},
    {KEY_JOINTS, "joints"},
    {KEY_START, "start"},
    {KEY_PATH,  "path"},
    {KEY_WEIGHT, "weight"},
    {KEY_PRI,   "priority"},
    {KEY_EFF,   "effector"},
    {KEY_POINT, "point"},
    {KEY_EXCEPT, "except"},
    {KEY_INF,   "INF"},
    {KEY_VERTEX, "vertex"},
    {0, 0}};

#define UNIT_INCH	1
#define UNIT_METER	2
#define UNIT_FEET	3
#define UNIT_CM		4
#define UNIT_MM		5

static struct bu_lex_key units[] = {
    {UNIT_INCH,	"inches"},
    {UNIT_INCH,	"in"},
    {UNIT_METER,	"m"},
    {UNIT_METER,	"meters"},
    {UNIT_FEET,	"ft"},
    {UNIT_FEET,	"feet"},
    {UNIT_CM,	"cm"},
    {UNIT_MM,	"mm"},
    {0, 0}};

#define ID_FIXED	-1
static struct bu_lex_key lex_solids[] = {
    {ID_FIXED,	"fixed"},
    {ID_GRIP,	"grip"},
    {ID_SPH,	"sphere"},
    {ID_JOINT,	"joint"},
    {0, 0}};

#define SYM_OP_GROUP	1
#define SYM_CL_GROUP	2
#define SYM_OP_PT	3
#define SYM_CL_PT	4
#define SYM_EQ		5
#define SYM_ARC		6
#define SYM_END		7
#define SYM_COMMA	8
#define SYM_MINUS	9
#define SYM_PLUS	10

static struct bu_lex_key animsyms[] = {
    {SYM_OP_GROUP,	"{"},
    {SYM_CL_GROUP,	"}"},
    {SYM_OP_PT,	"("},
    {SYM_CL_PT,	")"},
    {SYM_EQ,	"="},
    {SYM_ARC,	"/"},
    {SYM_END,	";"},
    {SYM_COMMA,	", "},
    {SYM_MINUS,	"-"},
    {SYM_PLUS,	"+"},
    {0, 0}};

static int lex_line;
static const char *lex_name;
static double mm2base, base2mm;

static void
parse_error(struct ged *gedp, struct bu_vls *vlsp, char *error)
{
    char *text;
    size_t i;
    size_t len;
    const char *str = bu_vls_addr(vlsp);

    len = bu_vls_strlen(vlsp);
    if (!len) {
	bu_vls_printf(gedp->ged_result_str, "%s:%d %s\n", lex_name, lex_line, error);
	return;
    }
    text = (char *)bu_malloc(len+2, "error pointer");
    for (i=0; i<len; i++) {
	text[i]=(str[i] == '\t')? '\t' : '-';
    }
    text[len] = '^';
    text[len+1] = '\0';

    {
	bu_vls_printf(gedp->ged_result_str, "%s:%d %s\n%s\n%s\n", lex_name, lex_line, error, str, text);
    }

    bu_free(text, "error pointer");
}


int
get_token(struct ged *gedp, union bu_lex_token *token, FILE *fip, struct bu_vls *str, struct bu_lex_key *keys, struct bu_lex_key *syms)
{
    int used;
    for (;;) {
	used = bu_lex(token, str, keys, syms);
	if (used)
	    break;
	bu_vls_free(str);
	lex_line++;
	used = bu_vls_gets(str, fip);
	if (used == EOF)
	    return used;
    }

    bu_vls_nibble(str, used);

    {
	if (J_DEBUG & DEBUG_J_LEX) {
	    int i;
	    switch (token->type) {
		case BU_LEX_KEYWORD:
		    for (i=0; keys[i].tok_val != token->t_key.value; i++)
			/* skip */;
		    bu_vls_printf(gedp->ged_result_str, "lex: key(%d)='%s'\n", token->t_key.value,
				  keys[i].string);
		    break;
		case BU_LEX_SYMBOL:
		    for (i=0; syms[i].tok_val != token->t_key.value; i++)
			/* skip */;
		    bu_vls_printf(gedp->ged_result_str, "lex: symbol(%d)='%c'\n", token->t_key.value,
				  *(syms[i].string));
		    break;
		case BU_LEX_DOUBLE:
		    bu_vls_printf(gedp->ged_result_str, "lex: double(%g)\n", token->t_dbl.value);
		    break;
		case BU_LEX_INT:
		    bu_vls_printf(gedp->ged_result_str, "lex: int(%d)\n", token->t_int.value);
		    break;
		case BU_LEX_IDENT:
		    bu_vls_printf(gedp->ged_result_str, "lex: id(%s)\n", token->t_id.value);
		    break;
	    }
	}
    }
    return used;
}


static int
gobble_token(struct ged *gedp, int type_wanted, int value_wanted, FILE *fip, struct bu_vls *str)
{
    static char *types[] = {
	"any",
	"integer",
	"double",
	"symbol",
	"keyword",
	"identifier",
	"number"};
    union bu_lex_token token;
    char error[160];

    if (get_token(gedp, &token, fip, str, animkeys, animsyms) == EOF) {
	snprintf(error, 160, "parse: Unexpected EOF while getting %s",
		 types[type_wanted]);
	parse_error(gedp, str, error);
	return 0;
    }

    if (token.type == BU_LEX_IDENT)
	bu_free(token.t_id.value, "unit token");

    switch (type_wanted) {
	case BU_LEX_ANY:
	    return 1;
	case BU_LEX_INT:
	    return token.type == BU_LEX_INT;
	case BU_LEX_DOUBLE:
	    return token.type == BU_LEX_DOUBLE;
	case BU_LEX_NUMBER:
	    return token.type == BU_LEX_INT || token.type == BU_LEX_DOUBLE;
	case BU_LEX_SYMBOL:
	    return (token.type == BU_LEX_SYMBOL &&
		    value_wanted == token.t_key.value);
	case BU_LEX_KEYWORD:
	    return (token.type == BU_LEX_KEYWORD &&
		    value_wanted == token.t_key.value);
	case BU_LEX_IDENT:
	    return token.type == BU_LEX_IDENT;
    }
    return 0;
}


static void
skip_group(struct ged *gedp, FILE *fip, struct bu_vls *str)
{
    union bu_lex_token tok;
    int count = 1;

    if (J_DEBUG & DEBUG_J_PARSE) {
	bu_vls_printf(gedp->ged_result_str, "skip_group: Skipping....\n");
    }

    while (count) {
	if (get_token(gedp, &tok, fip, str, animkeys, animsyms) == EOF) {
	    parse_error(gedp, str, "skip_group: Unexpected EOF while searching for group end.");
	    return;
	}
	if (tok.type == BU_LEX_IDENT) {
	    bu_free(tok.t_id.value, "unit token");
	}
	if (tok.type != BU_LEX_SYMBOL) {
	    continue;
	}
	if (tok.t_key.value == SYM_OP_GROUP) {
	    count++;
	} else if (tok.t_key.value == SYM_CL_GROUP) {
	    count--;
	}
    }
    if (J_DEBUG & DEBUG_J_PARSE) {
	bu_vls_printf(gedp->ged_result_str, "skip_group: Done....\n");
    }

}


static int
parse_units(struct ged *gedp, FILE *fip, struct bu_vls *str)
{
    union bu_lex_token token;

    if (get_token(gedp, &token, fip, str, units, animsyms) == EOF) {
	parse_error(gedp, str, "parse_units: Unexpected EOF reading units.");
	return 0;
    }
    if (token.type == BU_LEX_IDENT)
	bu_free(token.t_id.value, "unit token");
    if (token.type != BU_LEX_KEYWORD) {
	parse_error(gedp, str, "parse_units: syntax error getting unit type.");
	return 0;
    }

    switch (token.t_key.value) {
	case UNIT_INCH:
	    base2mm = 25.4; break;
	case UNIT_METER:
	    base2mm = 1000.0; break;
	case UNIT_FEET:
	    base2mm = 304.8; break;
	case UNIT_CM:
	    base2mm = 10.0; break;
	case UNIT_MM:
	    base2mm = 1; break;
    }
    mm2base = 1.0 / base2mm;
    (void) gobble_token(gedp, BU_LEX_SYMBOL, SYM_END, fip, str);
    return 1;
}


static int
parse_path(struct ged *gedp, struct arc *ap, FILE *fip, struct bu_vls *str)
{
    union bu_lex_token token;
    int max;

    if (J_DEBUG & DEBUG_J_PARSE) {
	bu_vls_printf(gedp->ged_result_str, "parse_path: open.\n");
    }
    /*
     * clear the arc if there is anything there.
     */
    free_arc(ap);
    if (!gobble_token(gedp, BU_LEX_SYMBOL, SYM_EQ, fip, str))
	return 0;
    max = MAX_OBJ_NAME;
    ap->arc = (char **)bu_malloc(sizeof(char *)*max, "arc table");
    ap->arc_last = -1;
    ap->type = ARC_PATH;
    for (;;) {
	if (get_token(gedp, &token, fip, str, (struct bu_lex_key *)NULL, animsyms) == EOF) {
	    parse_error(gedp, str, "parse_path: Unexpected EOF.");
	    free_arc(ap);
	    return 0;
	}
	if (token.type != BU_LEX_IDENT) {
	    parse_error(gedp, str, "parse_path: syntax error. Missing identifier.");
	    free_arc(ap);
	    return 0;
	}
	if (++ap->arc_last >= max) {
	    max += MAX_OBJ_NAME + 1;
	    ap->arc = (char **) bu_realloc((char *) ap->arc, sizeof(char *)*max, "arc table");
	}
	ap->arc[ap->arc_last] = token.t_id.value;
	if (get_token(gedp, &token, fip, str, (struct bu_lex_key *)NULL, animsyms) == EOF) {
	    parse_error(gedp, str, "parse_path: Unexpected EOF while getting '/' or '-'");
	    free_arc(ap);
	    return 0;
	}
	if (token.type == BU_LEX_IDENT)
	    bu_free(token.t_id.value, "unit token");
	if (token.type != BU_LEX_SYMBOL) {
	    parse_error(gedp, str, "parse_path: syntax error.");
	    free_arc(ap);
	    return 0;
	}
	if (token.t_key.value == SYM_ARC) {
	    continue;
	} else if (token.t_key.value == SYM_MINUS) {
	    break;
	} else {
	    parse_error(gedp, str, "parse_path: syntax error.");
	    free_arc(ap);
	    return 0;
	}
    }
    /*
     * Just got the '-' so this is the "destination" part.
     */
    if (get_token(gedp, &token, fip, str, (struct bu_lex_key *)NULL, animsyms) == EOF) {
	parse_error(gedp, str, "parse_path: Unexpected EOF while getting destination.");
	free_arc(ap);
	return 0;
    }
    if (token.type != BU_LEX_IDENT) {
	parse_error(gedp, str, "parse_path: syntax error, expecting destination.");
	free_arc(ap);
	return 0;
    }
    if (!gobble_token(gedp, BU_LEX_SYMBOL, SYM_END, fip, str)) {
	free_arc(ap);
	return 0;
    }
    return 1;
}


static int
parse_list(struct ged *gedp, struct arc *ap, FILE *fip, struct bu_vls *str)
{
    union bu_lex_token token;
    int max;

    if (J_DEBUG & DEBUG_J_PARSE) {
	bu_vls_printf(gedp->ged_result_str, "parse_path: open.\n");
    }
    /*
     * clear the arc if there is anything there.
     */
    free_arc(ap);

    if (!gobble_token(gedp, BU_LEX_SYMBOL, SYM_EQ, fip, str))
	return 0;
    max = MAX_OBJ_NAME;
    ap->arc = (char **)bu_malloc(sizeof(char *)*max, "arc table");
    ap->arc_last = -1;
    ap->type = ARC_LIST;
    for (;;) {
	if (get_token(gedp, &token, fip, str, (struct bu_lex_key *)NULL, animsyms) == EOF) {
	    parse_error(gedp, str, "parse_path: Unexpected EOF.");
	    free_arc(ap);
	    return 0;
	}
	if (token.type != BU_LEX_IDENT) {
	    parse_error(gedp, str, "parse_path: syntax error. Missing identifier.");
	    free_arc(ap);
	    return 0;
	}
	if (++ap->arc_last >= max) {
	    max += MAX_OBJ_NAME + 1;
	    ap->arc = (char **) bu_realloc((char *) ap->arc, sizeof(char *)*max, "arc table");
	}
	ap->arc[ap->arc_last] = token.t_id.value;
	if (get_token(gedp, &token, fip, str, (struct bu_lex_key *)NULL, animsyms) == EOF) {
	    parse_error(gedp, str, "parse_path: Unexpected EOF while getting ', ' or ';'");
	    free_arc(ap);
	    return 0;
	}
	if (token.type == BU_LEX_IDENT)
	    bu_free(token.t_id.value, "unit token");
	if (token.type != BU_LEX_SYMBOL) {
	    parse_error(gedp, str, "parse_path: syntax error.");
	    free_arc(ap);
	    return 0;
	}
	if (token.t_key.value == SYM_COMMA) {
	    continue;
	} else if (token.t_key.value == SYM_END) {
	    break;
	} else {
	    parse_error(gedp, str, "parse_path: syntax error.");
	    free_arc(ap);
	    return 0;
	}
    }
    return 1;
}


static int
parse_ARC(struct ged *gedp, struct arc *ap, FILE *fip, struct bu_vls *str)
{
    union bu_lex_token token;
    int max;
    char *error;

    if (J_DEBUG & DEBUG_J_PARSE) {
	bu_vls_printf(gedp->ged_result_str, "parse_ARC: open.\n");
    }

    free_arc(ap);
    max = MAX_OBJ_NAME;
    if (!gobble_token(gedp, BU_LEX_SYMBOL, SYM_EQ, fip, str))
	return 0;

    ap->arc = (char **) bu_malloc(sizeof(char *)*max, "arc table");
    ap->arc_last = -1;

    error = "parse_ARC: Unexpected EOF while getting arc.";
    while (get_token(gedp, &token, fip, str, (struct bu_lex_key *)NULL, animsyms) != EOF) {
	if (token.type != BU_LEX_IDENT) {
	    error = "parse_ARC: syntax error. Missing identifier.";
	    break;
	}
	if (++ap->arc_last >= max) {
	    max += MAX_OBJ_NAME + 1;
	    ap->arc = (char **) bu_realloc((char *)ap->arc, sizeof(char *)*max, "arc table");
	}
	ap->arc[ap->arc_last] = token.t_id.value;
	if (get_token(gedp, &token, fip, str, (struct bu_lex_key *)NULL, animsyms) == EOF) {
	    error = "parse_ARC: Unexpected EOF while getting '/' or ';'.";
	    break;
	}
	if (token.type != BU_LEX_SYMBOL) {
	    if (token.type == BU_LEX_IDENT) {
		bu_free(token.t_id.value, "unit token");
	    }
	    error = "parse_ARC: syntax error.  Expected '/' or ';'";
	    break;
	}
	if (token.t_key.value == SYM_END) {
	    if (J_DEBUG & DEBUG_J_PARSE) {
		bu_vls_printf(gedp->ged_result_str, "parse_ARC: close.\n");
	    }

	    return 1;
	}
	if (token.t_key.value != SYM_ARC) {
	    error = "parse_ARC: Syntax error.  Expecting ';' or '/'";
	    break;
	}
	error = "parse_ARC: Unexpected EOF while getting arc.";
    }
    parse_error(gedp, str, error);
    free_arc(ap);
    return 0;
}


static int
parse_double(struct ged *gedp, double *dbl, FILE *fip, struct bu_vls *str)
{
    union bu_lex_token token;
    double sign;
    sign = 1.0;

    if (J_DEBUG & DEBUG_J_PARSE) {
	bu_vls_printf(gedp->ged_result_str, "parse_double: open\n");
    }

    if (get_token(gedp, &token, fip, str, animkeys, animsyms) == EOF) {
	parse_error(gedp, str, "parse_double: Unexpected EOF while getting number.");
	return 0;
    }
    if (token.type == BU_LEX_SYMBOL && token.t_key.value == SYM_MINUS) {
	sign = -1;
	if (get_token(gedp, &token, fip, str, animkeys, animsyms) == EOF) {
	    parse_error(gedp, str, "parse_double: Unexpected EOF while getting number.");
	    return 0;
	}
    }
    if (token.type == BU_LEX_IDENT)
	bu_free(token.t_id.value, "unit token");

    if (token.type == BU_LEX_INT) {
	*dbl = token.t_int.value * sign;
    } else if (token.type == BU_LEX_DOUBLE) {
	*dbl = token.t_dbl.value * sign;
    } else if (token.type == BU_LEX_KEYWORD && token.t_key.value == KEY_INF) {
	*dbl = MAX_FASTF * sign;
    } else {
	parse_error(gedp, str, "parse_double: syntax error.  Expecting number.");
	return 0;
    }

    if (J_DEBUG & DEBUG_J_PARSE)
	bu_vls_printf(gedp->ged_result_str, "parse_double: %lf\n", *dbl);

    return 1;
}


static int
parse_assign(struct ged *gedp, double *dbl, FILE *fip, struct bu_vls *str)
{
    if (!gobble_token(gedp, BU_LEX_SYMBOL, SYM_EQ, fip, str)) {
	skip_group(gedp, fip, str);
	return 0;
    }
    if (!parse_double(gedp, dbl, fip, str)) {
	skip_group(gedp, fip, str);
	return 0;
    }
    if (!gobble_token(gedp, BU_LEX_SYMBOL, SYM_END, fip, str)) {
	skip_group(gedp, fip, str);
	return 0;
    }
    return 1;
}


static int
parse_vect(struct ged *gedp, fastf_t *vect, FILE *fip, struct bu_vls *str)
{
    int i;
    double scan[3];

    if (J_DEBUG & DEBUG_J_PARSE) {
	bu_vls_printf(gedp->ged_result_str, "parse_vect: open.\n");
    }

    /* convert to double for parsing */
    VMOVE(scan, vect);

    if (!gobble_token(gedp, BU_LEX_SYMBOL, SYM_OP_PT, fip, str)) {
	return 0;
    }

    for (i=0; i < 3; i++) {
	if (!parse_double(gedp, &scan[i], fip, str)) {
	    return 0;
	}
	if (i < 2) {
	    if (!gobble_token(gedp, BU_LEX_SYMBOL, SYM_COMMA, fip, str)) {
		return 0;
	    }
	} else {
	    if (!gobble_token(gedp, BU_LEX_SYMBOL, SYM_CL_PT, fip, str)) {
		return 0;
	    }
	}
    }

    /* convert to fastf_t back to double for return */
    VMOVE(vect, scan);

    return 1;
}


static int
parse_trans(struct ged *gedp, struct joint *jp, int idx, FILE *fip, struct bu_vls *str)
{
    union bu_lex_token token;
    int dirfound, upfound, lowfound, curfound;

    if (J_DEBUG & DEBUG_J_PARSE) {
	bu_vls_printf(gedp->ged_result_str, "parse_trans: open\n");
    }

    if (idx >= 3) {
	parse_error(gedp, str, "parse_trans: Too many translations for this joint.");
	if (!gobble_token(gedp, BU_LEX_SYMBOL, SYM_OP_GROUP, fip, str))
	    return 0;
	skip_group(gedp, fip, str);
	return 0;
    }
    if (!gobble_token(gedp, BU_LEX_SYMBOL, SYM_OP_GROUP, fip, str))
	return 0;

    dirfound = upfound = lowfound = curfound = 0;
    while (get_token(gedp, &token, fip, str, animkeys, animsyms) != EOF) {
	if (token.type == BU_LEX_IDENT) {
	    bu_free(token.t_id.value, "unit token");
	}
	if (token.type == BU_LEX_SYMBOL && token.t_key.value == SYM_CL_GROUP) {
	    if (J_DEBUG & DEBUG_J_PARSE) {
		bu_vls_printf(gedp->ged_result_str, "parse_trans: closing.\n");
	    }

	    if (!dirfound) {
		parse_error(gedp, str, "parse_trans: Direction vector not given.");
		return 0;
	    }
	    VUNITIZE(jp->dirs[idx].unitvec);
	    if (!lowfound) {
		parse_error(gedp, str, "parse_trans: lower bound not given.");
		return 0;
	    }
	    if (!upfound) {
		parse_error(gedp, str, "parse_trans: upper bound not given.");
		return 0;
	    }
	    if (jp->dirs[idx].lower > jp->dirs[idx].upper) {
		double tmp;
		tmp = jp->dirs[idx].lower;
		jp->dirs[idx].lower = jp->dirs[idx].upper;
		jp->dirs[idx].upper = tmp;
		parse_error(gedp, str, "parse_trans: lower > upper, exchanging.");
	    }
	    jp->dirs[idx].accepted = 0.0;
	    if (!curfound)
		jp->dirs[idx].current = 0.0;
	    jp->dirs[idx].lower *= base2mm;
	    jp->dirs[idx].upper *= base2mm;
	    jp->dirs[idx].current *= base2mm;
	    jp->dirs[idx].accepted *= base2mm;

	    if (jp->dirs[idx].accepted < jp->dirs[idx].lower) {
		jp->dirs[idx].accepted = jp->dirs[idx].lower;
	    }
	    if (jp->dirs[idx].accepted > jp->dirs[idx].upper) {
		jp->dirs[idx].accepted = jp->dirs[idx].upper;
	    }
	    if (jp->dirs[idx].current < jp->dirs[idx].lower) {
		jp->dirs[idx].current = jp->dirs[idx].lower;
	    }
	    if (jp->dirs[idx].current > jp->dirs[idx].upper) {
		jp->dirs[idx].current = jp->dirs[idx].upper;
	    }
	    return 1;
	}

	if (token.type != BU_LEX_KEYWORD) {
	    parse_error(gedp, str, "parse_trans: Syntax error.");
	    skip_group(gedp, fip, str);
	    return 0;
	}
	switch (token.t_key.value) {
	    case KEY_LIMIT:
		if (!gobble_token(gedp, BU_LEX_SYMBOL, SYM_EQ, fip, str)) {
		    skip_group(gedp, fip, str);
		    return 0;
		}
		if (!parse_double(gedp, &jp->dirs[idx].lower, fip, str)) {
		    skip_group(gedp, fip, str);
		    return 0;
		}
		lowfound = 1;
		if (!gobble_token(gedp, BU_LEX_SYMBOL, SYM_COMMA, fip, str)) {
		    skip_group(gedp, fip, str);
		    return 0;
		}
		if (!parse_double(gedp, &jp->dirs[idx].upper, fip, str)) {
		    skip_group(gedp, fip, str);
		    return 0;
		}
		upfound = 1;
		if (!gobble_token(gedp, BU_LEX_SYMBOL, SYM_COMMA, fip, str)) {
		    skip_group(gedp, fip, str);
		    return 0;
		}
		if (!parse_double(gedp, &jp->dirs[idx].current, fip, str)) {
		    skip_group(gedp, fip, str);
		    return 0;
		}
		curfound = 1;
		(void) gobble_token(gedp, BU_LEX_SYMBOL, SYM_END, fip, str);
		break;
	    case KEY_UP:
		if (!parse_assign(gedp, &jp->dirs[idx].upper, fip, str)) {
		    skip_group(gedp, fip, str);
		    return 0;
		}
		upfound = 1;
		break;
	    case KEY_LOW:
		if (!parse_assign(gedp, &jp->dirs[idx].lower, fip, str)) {
		    skip_group(gedp, fip, str);
		    return 0;
		}
		lowfound = 1;
		break;
	    case KEY_CUR:
		if (!parse_assign(gedp, &jp->dirs[idx].current, fip, str)) {
		    skip_group(gedp, fip, str);
		    return 0;
		}
		curfound = 1;
		break;
	    case KEY_ACC:
		if (!parse_assign(gedp, &jp->dirs[idx].accepted, fip, str)) {
		    skip_group(gedp, fip, str);
		    return 0;
		}
		curfound = 1;
		break;
	    case KEY_DIR:
		if (!gobble_token(gedp, BU_LEX_SYMBOL, SYM_EQ, fip, str)) {
		    skip_group(gedp, fip, str);
		    return 0;
		}
		if (!parse_vect(gedp, jp->dirs[idx].unitvec, fip, str)) {
		    skip_group(gedp, fip, str);
		    return 0;
		}
		if (!gobble_token(gedp, BU_LEX_SYMBOL, SYM_END, fip, str)) {
		    skip_group(gedp, fip, str);
		    return 0;
		}
		dirfound = 1;
		break;
	    default:
		parse_error(gedp, str, "parse_trans: syntax error.");
		skip_group(gedp, fip, str);
		return 0;
	}
    }
    parse_error(gedp, str, "parse_trans:Unexpected EOF.");
    return 0;
}


static int
parse_rots(struct ged *gedp, struct joint *jp, int idx, FILE *fip, struct bu_vls *str)
{
    union bu_lex_token token;
    int dirfound, upfound, lowfound, curfound;

    if (J_DEBUG & DEBUG_J_PARSE) {
	bu_vls_printf(gedp->ged_result_str, "parse_rots: open\n");
    }

    if (idx >= 3) {
	parse_error(gedp, str, "parse_rot: To many rotations for this joint.");
	if (!gobble_token(gedp, BU_LEX_SYMBOL, SYM_OP_GROUP, fip, str))
	    return 0;
	skip_group(gedp, fip, str);
	return 0;
    }
    if (!gobble_token(gedp, BU_LEX_SYMBOL, SYM_OP_GROUP, fip, str))
	return 0;

    dirfound = upfound = lowfound = curfound = 0;
    while (get_token(gedp, &token, fip, str, animkeys, animsyms) != EOF) {
	if (token.type == BU_LEX_IDENT) {
	    bu_free(token.t_id.value, "unit token");
	}
	if (token.type == BU_LEX_SYMBOL && token.t_key.value == SYM_CL_GROUP) {
	    if (J_DEBUG & DEBUG_J_PARSE) {
		bu_vls_printf(gedp->ged_result_str, "parse_rots: closing.\n");
	    }

	    if (!dirfound) {
		parse_error(gedp, str, "parse_rots: Direction vector not given.");
		return 0;
	    }
	    VUNITIZE(jp->rots[idx].quat);
	    jp->rots[idx].quat[W] = 0.0;
	    if (!lowfound) {
		parse_error(gedp, str, "parse_rots: lower bound not given.");
		return 0;
	    }
	    if (!upfound) {
		parse_error(gedp, str, "parse_rots: upper bound not given.");
		return 0;
	    }
	    if (jp->rots[idx].lower > jp->rots[idx].upper) {
		double tmp;
		tmp = jp->rots[idx].lower;
		jp->rots[idx].lower = jp->rots[idx].upper;
		jp->rots[idx].upper = tmp;
		parse_error(gedp, str, "parse_rots: lower > upper, exchanging.");
	    }

	    jp->rots[idx].accepted = 0.0;
	    if (jp->rots[idx].accepted < jp->rots[idx].lower) {
		jp->rots[idx].accepted = jp->rots[idx].lower;
	    }
	    if (jp->rots[idx].accepted > jp->rots[idx].upper) {
		jp->rots[idx].accepted = jp->rots[idx].upper;
	    }
	    if (!curfound) {
		jp->rots[idx].current = 0.0;
	    }
	    if (jp->rots[idx].current < jp->rots[idx].lower) {
		jp->rots[idx].current = jp->rots[idx].lower;
	    }
	    if (jp->rots[idx].current > jp->rots[idx].upper) {
		jp->rots[idx].current = jp->rots[idx].upper;
	    }
	    return 1;
	}

	if (token.type != BU_LEX_KEYWORD) {
	    parse_error(gedp, str, "parse_rots: Syntax error.");
	    skip_group(gedp, fip, str);
	    return 0;
	}
	switch (token.t_key.value) {
	    case KEY_LIMIT:
		if (!gobble_token(gedp, BU_LEX_SYMBOL, SYM_EQ, fip, str)) {
		    skip_group(gedp, fip, str);
		    return 0;
		}
		if (!parse_double(gedp, &jp->rots[idx].lower, fip, str)) {
		    skip_group(gedp, fip, str);
		    return 0;
		}
		lowfound = 1;
		if (!gobble_token(gedp, BU_LEX_SYMBOL, SYM_COMMA, fip, str)) {
		    skip_group(gedp, fip, str);
		    return 0;
		}
		if (!parse_double(gedp, &jp->rots[idx].upper, fip, str)) {
		    skip_group(gedp, fip, str);
		    return 0;
		}
		upfound = 1;
		if (!gobble_token(gedp, BU_LEX_SYMBOL, SYM_COMMA, fip, str)) {
		    skip_group(gedp, fip, str);
		    return 0;
		}
		if (!parse_double(gedp, &jp->rots[idx].current, fip, str)) {
		    skip_group(gedp, fip, str);
		    return 0;
		}
		curfound = 1;
		(void) gobble_token(gedp, BU_LEX_SYMBOL, SYM_END, fip, str);
		break;
	    case KEY_UP:
		if (!parse_assign(gedp, &jp->rots[idx].upper, fip, str)) {
		    skip_group(gedp, fip, str);
		    return 0;
		}
		upfound = 1;
		break;
	    case KEY_LOW:
		if (!parse_assign(gedp, &jp->rots[idx].lower, fip, str)) {
		    skip_group(gedp, fip, str);
		    return 0;
		}
		lowfound = 1;
		break;
	    case KEY_CUR:
		if (!parse_assign(gedp, &jp->rots[idx].current, fip, str)) {
		    skip_group(gedp, fip, str);
		    return 0;
		}
		curfound = 1;
		break;
	    case KEY_ACC:
		if (!parse_assign(gedp, &jp->rots[idx].accepted, fip, str)) {
		    skip_group(gedp, fip, str);
		    return 0;
		}
		curfound = 1;
		break;
	    case KEY_DIR:
		if (!gobble_token(gedp, BU_LEX_SYMBOL, SYM_EQ, fip, str)) {
		    skip_group(gedp, fip, str);
		    return 0;
		}
		if (!parse_vect(gedp, jp->rots[idx].quat, fip, str)) {
		    skip_group(gedp, fip, str);
		    return 0;
		}
		if (!gobble_token(gedp, BU_LEX_SYMBOL, SYM_END, fip, str)) {
		    skip_group(gedp, fip, str);
		    return 0;
		}
		dirfound = 1;
		break;
	    default:
		parse_error(gedp, str, "parse_rots: syntax error.");
		skip_group(gedp, fip, str);
		return 0;
	}
    }
    parse_error(gedp, str, "parse_rots:Unexpected EOF.");
    return 0;
}


static int
parse_joint(struct ged *gedp, FILE *fip, struct bu_vls *str)
{
    union bu_lex_token token;
    struct joint *jp;
    int trans;
    int rots;
    int arcfound, locfound;

    if (J_DEBUG & DEBUG_J_PARSE) {
	bu_vls_printf(gedp->ged_result_str, "parse_joint: reading joint.\n");
    }

    BU_GET(jp, struct joint);
    jp->l.magic = MAGIC_JOINT_STRUCT;
    jp->anim = (struct animate *) 0;
    jp->path.type = ARC_UNSET;
    jp->name = NULL;

    if (get_token(gedp, &token, fip, str, (struct bu_lex_key *)NULL, animsyms) == EOF) {
	parse_error(gedp, str, "parse_joint: Unexpected EOF getting name.");
	free_joint(jp);
	return 0;
    }
    jp->name = token.t_id.value;	/* Name */
    if (!gobble_token(gedp, BU_LEX_SYMBOL, SYM_OP_GROUP, fip, str)) {
	free_joint(jp);
	return 0;
    }
    /*
     * With in the group, we need at least one rotate or translate,
     * a location and an arc or path.
     */
    arcfound = 0;
    locfound = 0;
    rots = trans = 0;
    for (;;) {
	if (get_token(gedp, &token, fip, str, animkeys, animsyms) == EOF) {
	    parse_error(gedp, str, "parse_joint: Unexpected EOF getting joint contents.");
	    skip_group(gedp, fip, str);
	    free_joint(jp);
	    return 0;
	}
	if (token.type == BU_LEX_SYMBOL && token.t_key.value == SYM_CL_GROUP) {
	    if (J_DEBUG & DEBUG_J_PARSE) {
		bu_vls_printf(gedp->ged_result_str, "parse_joint: closing.\n");
	    }
	    if (!arcfound) {
		parse_error(gedp, str, "parse_joint: Arc not defined.");
		free_joint(jp);
		return 0;
	    }
	    if (!locfound) {
		parse_error(gedp, str, "parse_joint: location not defined.");
		free_joint(jp);
		return 0;
	    }
	    if (trans + rots == 0) {
		parse_error(gedp, str, "parse_joint: no translations or rotations defined.");
		free_joint(jp);
		return 0;
	    }
	    for (;trans<3;trans++) {
		jp->dirs[trans].lower = -1.0;
		jp->dirs[trans].upper = -2.0;
		jp->dirs[trans].current = 0.0;
	    }
	    for (;rots<3;rots++) {
		jp->rots[rots].lower = -1.0;
		jp->rots[rots].upper = -2.0;
		jp->rots[rots].current = 0.0;
	    }
	    jp->location[X] *= base2mm;
	    jp->location[Y] *= base2mm;
	    jp->location[Z] *= base2mm;

	    BU_LIST_INSERT(&joint_head, &(jp->l));
	    gobble_token(gedp, BU_LEX_SYMBOL, SYM_END, fip, str);
	    return 1;
	}
	if (token.type == BU_LEX_IDENT)
	    bu_free(token.t_id.value, "unit token");

	if (token.type != BU_LEX_KEYWORD) {
	    parse_error(gedp, str, "parse_joint: syntax error.");
	    skip_group(gedp, fip, str);
	    free_joint(jp);
	    return 0;
	}
	switch (token.t_key.value) {
	    case KEY_ARC:
		if (arcfound) {
		    parse_error(gedp, str, "parse_joint: more than one arc or path given");
		}
		if (!parse_ARC(gedp, &jp->path, fip, str)) {
		    skip_group(gedp, fip, str);
		    free_joint(jp);
		    return 0;
		}
		arcfound = 1;
		break;
	    case KEY_PATH:
		if (arcfound) {
		    parse_error(gedp, str, "parse_joint: more than one arc or path given.");
		}
		if (!parse_path(gedp, &jp->path, fip, str)) {
		    skip_group(gedp, fip, str);
		    free_joint(jp);
		    return 0;
		}
		arcfound = 1;
		break;
	    case KEY_LOC:
		if (locfound) {
		    parse_error(gedp, str, "parse_joint: more than one location given.");
		}
		if (!gobble_token(gedp, BU_LEX_SYMBOL, SYM_EQ, fip, str)) {
		    skip_group(gedp, fip, str);
		    free_joint(jp);
		    return 0;
		}
		if (!parse_vect(gedp, &jp->location[0], fip, str)) {
		    skip_group(gedp, fip, str);
		    free_joint(jp);
		    return 0;
		}
		if (!gobble_token(gedp, BU_LEX_SYMBOL, SYM_END, fip, str)) {
		    skip_group(gedp, fip, str);
		    free_joint(jp);
		    return 0;
		}
		locfound=1;
		break;
	    case KEY_TRANS:
		if (!parse_trans(gedp, jp, trans, fip, str)) {
		    skip_group(gedp, fip, str);
		    free_joint(jp);
		    return 0;
		}
		trans++;
		break;
	    case KEY_ROT:
		if (!parse_rots(gedp, jp, rots, fip, str)) {
		    skip_group(gedp, fip, str);
		    free_joint(jp);
		    return 0;
		}
		rots++;
		break;
	    default:
		parse_error(gedp, str, "parse_joint: syntax error.");
		skip_group(gedp, fip, str);
		free_joint(jp);
		return 0;
	}
    }
    /* NOTREACHED */
}


static int
parse_jset(struct ged *gedp, struct hold *hp, FILE *fip, struct bu_vls *str)
{
    union bu_lex_token token;
    int jointfound, listfound, arcfound, pathfound;

    if (J_DEBUG & DEBUG_J_PARSE) {
	bu_vls_printf(gedp->ged_result_str, "parse_jset: open\n");
    }

    if (!gobble_token(gedp, BU_LEX_SYMBOL, SYM_OP_GROUP, fip, str))
	return 0;

    jointfound = listfound = arcfound = pathfound = 0;
    for (;;) {
	if (get_token(gedp, &token, fip, str, animkeys, animsyms) == EOF) {
	    parse_error(gedp, str, "parse_jset: Unexpected EOF getting contents of joint set");
	    return 0;
	}
	if (token.type == BU_LEX_IDENT)
	    bu_free(token.t_id.value, "unit token");
	if (token.type == BU_LEX_SYMBOL && token.t_key.value == SYM_CL_GROUP) {
	    if (!jointfound) hp->j_set.joint = 0;
	    if (!listfound && !arcfound && !pathfound) {
		parse_error(gedp, str, "parse_jset: no list/arc/path given.");
		return 0;
	    }
	    if (J_DEBUG & DEBUG_J_PARSE) {
		bu_vls_printf(gedp->ged_result_str, "parse_jset: close\n");
	    }
	    return 1;
	}
	if (token.type != BU_LEX_KEYWORD) {
	    parse_error(gedp, str, "parse_jset: syntax error.");
	    return 0;
	}
	switch (token.t_key.value) {
	    case KEY_START:
		if (!gobble_token(gedp, BU_LEX_SYMBOL, SYM_EQ, fip, str)) {
		    skip_group(gedp, fip, str);
		    return 0;
		}
		if (get_token(gedp, &token, fip, str, animkeys, animsyms) == EOF) {
		    parse_error(gedp, str, "parse_jset: Unexpected EOF getting '='");
		    return 0;
		}
		if (token.type != BU_LEX_IDENT) {
		    parse_error(gedp, str, "parse_jset: syntax error, expecting joint name.");
		    skip_group(gedp, fip, str);
		    return 0;
		}
		hp->j_set.joint = token.t_id.value;
		if (!gobble_token(gedp, BU_LEX_SYMBOL, SYM_END, fip, str)) {
		    skip_group(gedp, fip, str);
		    return 0;
		}
		jointfound = 1;
		break;
	    case KEY_ARC:
		if (!parse_ARC(gedp, &hp->j_set.path, fip, str)) {
		    skip_group(gedp, fip, str);
		    return 0;
		}
		arcfound = 1;
		break;
	    case KEY_PATH:
		if (!parse_path(gedp, &hp->j_set.path, fip, str)) {
		    skip_group(gedp, fip, str);
		    return 0;
		}
		pathfound = 1;
		break;
	    case KEY_JOINTS:
		if (!parse_list(gedp, &hp->j_set.path, fip, str)) {
		    skip_group(gedp, fip, str);
		    return 0;
		}
		listfound=1;
		break;
	    case KEY_EXCEPT:
		if (!parse_list(gedp, &hp->j_set.exclude, fip, str)) {
		    skip_group(gedp, fip, str);
		    return 0;
		}
		break;
	    default:
		parse_error(gedp, str, "parse_jset: syntax error.");
		skip_group(gedp, fip, str);
		return 0;
	}
    }
}


static int
parse_solid(struct ged *gedp, struct hold_point *pp, FILE *fip, struct bu_vls *str)
{
    union bu_lex_token token;
    int vertexfound = 0, arcfound = 0;
    double vertex;

    if (J_DEBUG & DEBUG_J_PARSE) {
	bu_vls_printf(gedp->ged_result_str, "parse_solid: open\n");
    }

    if (!gobble_token(gedp, BU_LEX_SYMBOL, SYM_OP_GROUP, fip, str))
	return 0;

    for (;;) {
	if (get_token(gedp, &token, fip, str, animkeys, animsyms) == EOF) {
	    parse_error(gedp, str, "parse_solid: Unexpected EOF.");
	    return 0;
	}
	if (token.type == BU_LEX_IDENT)
	    bu_free(token.t_id.value, "unit token");
	if (token.type == BU_LEX_SYMBOL && token.t_key.value == SYM_CL_GROUP) {
	    if (!arcfound) {
		parse_error(gedp, str, "parse_solid: path/arc missing.");
		return 0;
	    }
	    if (!vertexfound)
		pp->vertex_number = 1;

	    if (J_DEBUG & DEBUG_J_PARSE) {
		bu_vls_printf(gedp->ged_result_str, "parse_solid: close\n");
	    }

	    return 1;
	}
	if (token.type != BU_LEX_KEYWORD) {
	    parse_error(gedp, str, "parse_solid: syntax error getting solid information.");
	    skip_group(gedp, fip, str);
	    return 0;
	}
	switch (token.t_key.value) {
	    case KEY_VERTEX:
		if (!parse_assign(gedp, &vertex, fip, str)) {
		    skip_group(gedp, fip, str);
		    return 0;
		}
		pp->vertex_number = vertex;	/* double to int */
		vertexfound = 1;
		break;
	    case KEY_PATH:
		if (!parse_path(gedp, &pp->arc, fip, str)) {
		    skip_group(gedp, fip, str);
		    return 0;
		}
		arcfound = 1;
		break;
	    case KEY_ARC:
		if (!parse_ARC(gedp, &pp->arc, fip, str)) {
		    skip_group(gedp, fip, str);
		    return 0;
		}
		arcfound =1;
		break;
	    default:
		parse_error(gedp, str, "parse_solid: syntax error.");
	}
    }
}


static int
parse_point(struct ged *gedp, struct hold_point *pp, FILE *fip, struct bu_vls *str)
{
    union bu_lex_token token;

    if (get_token(gedp, &token, fip, str, lex_solids, animsyms) == EOF) {
	parse_error(gedp, str, "parse_point: Unexpected EOF getting solid type.");
	return 0;
    }
    if (token.type == BU_LEX_IDENT)
	bu_free(token.t_id.value, "unit token");
    if (token.type != BU_LEX_KEYWORD) {
	parse_error(gedp, str, "parse_point: syntax error getting solid type.");
	return 0;
    }
    switch (token.t_key.value) {
	case ID_FIXED:
	    pp->type = ID_FIXED;
	    if (!parse_vect(gedp, &pp->point[0], fip, str))
		return 0;
	    return gobble_token(gedp, BU_LEX_SYMBOL, SYM_END, fip, str);
	case ID_SPH:
	    pp->type = ID_SPH;
	    break;
	case ID_GRIP:
	    pp->type = ID_GRIP;
	    break;
	case ID_JOINT:
	    pp->type = ID_JOINT;
	    break;
	default:
	    parse_error(gedp, str, "parse_point: Syntax error-XXX.");
	    skip_group(gedp, fip, str);
	    return 0;
    }
    if (!parse_solid(gedp, pp, fip, str)) {
	skip_group(gedp, fip, str);
	return 0;
    }
    if (J_DEBUG & DEBUG_J_PARSE) {
	bu_vls_printf(gedp->ged_result_str, "parse_point: close.\n");
    }
    return 1;
}


static int
parse_hold(struct ged *gedp, FILE *fip, struct bu_vls *str)
{
    struct hold *hp;
    union bu_lex_token token;
    int jsetfound = 0, efffound=0, goalfound=0, weightfound=0, prifound=0;

    if (J_DEBUG & DEBUG_J_PARSE) {
	bu_vls_printf(gedp->ged_result_str, "parse_hold: reading constraint\n");
    }

    BU_GET(hp, struct hold);
    hp->l.magic = MAGIC_HOLD_STRUCT;
    hp->name = NULL;
    hp->joint = NULL;
    BU_LIST_INIT(&hp->j_head);
    hp->effector.type = ID_FIXED;
    hp->effector.arc.type = ARC_UNSET;
    db_full_path_init(&hp->effector.path);
    hp->effector.flag = 0;
    hp->objective.type = ID_FIXED;
    hp->objective.arc.type = ARC_UNSET;
    db_full_path_init(&hp->objective.path);
    hp->objective.flag = 0;
    hp->j_set.joint = NULL;
    hp->j_set.path.type = ARC_UNSET;
    hp->j_set.exclude.type = ARC_UNSET;

    /* read constraint name */
    if (get_token(gedp, &token, fip, str, (struct bu_lex_key *)NULL, animsyms) == EOF) {
	parse_error(gedp, str, "parse_hold: Unexpected EOF getting name.");
	free_hold(hp);
	return 0;
    }
    /* read constraint group label */
    if (token.type == BU_LEX_IDENT) {
	hp->name = token.t_id.value;
	if (get_token(gedp, &token, fip, str, (struct bu_lex_key *)NULL, animsyms) == EOF) {
	    parse_error(gedp, str, "parse_hold: Unexpected EOF getting open group.");
	    free_hold(hp);
	    return 0;
	}
    }

    /* sanity */
    if (token.type == BU_LEX_IDENT)
	bu_free(token.t_id.value, "unit token");
    if (token.type != BU_LEX_SYMBOL || token.t_key.value != SYM_OP_GROUP) {
	parse_error(gedp, str, "parse_hold: syntax error, expecting open group.");
	free_hold(hp);
	return 0;
    }

    /* read in the constraint details */
    for (;;) {
	if (get_token(gedp, &token, fip, str, animkeys, animsyms) == EOF) {
	    parse_error(gedp, str, "parse_hold: Unexpected EOF getting constraint contents.");
	    skip_group(gedp, fip, str);
	    free_hold(hp);
	}
	if (token.type == BU_LEX_IDENT)
	    bu_free(token.t_id.value, "unit token");

	if (token.type == BU_LEX_SYMBOL && token.t_key.value == SYM_CL_GROUP) {
	    if (J_DEBUG & DEBUG_J_PARSE) {
		bu_vls_printf(gedp->ged_result_str, "parse_hold: closing.\n");
	    }

	    if (!jsetfound) {
		parse_error(gedp, str, "parse_hold: no joint set given.");
		free_hold(hp);
		skip_group(gedp, fip, str);
		return 0;
	    }
	    if (!efffound) {
		parse_error(gedp, str, "parse_hold: no effector given.");
		free_hold(hp);
		skip_group(gedp, fip, str);
		return 0;
	    }
	    if (!goalfound) {
		parse_error(gedp, str, "parse_hold: no goal given.");
		free_hold(hp);
		skip_group(gedp, fip, str);
		return 0;
	    }
	    if (!weightfound) {
		hp->weight = 1.0;
	    }
	    if (!prifound) {
		hp->priority = 50;
	    }
	    BU_LIST_INSERT(&hold_head, &(hp->l));

	    gobble_token(gedp, BU_LEX_SYMBOL, SYM_END, fip, str);
	    return 1;
	}
	if (token.type != BU_LEX_KEYWORD) {
	    parse_error(gedp, str, "parse_hold: syntax error");
	    skip_group(gedp, fip, str);
	    free_hold(hp);
	    return 0;
	}

	switch (token.t_key.value) {
	    /* effector, goal */
	    case KEY_WEIGHT:
		if (!parse_assign(gedp, &hp->weight, fip, str)) {
		    free_hold(hp);
		    skip_group(gedp, fip, str);
		    return 0;
		}
		weightfound = 1;
		break;
	    case KEY_PRI:
		if (!parse_assign(gedp, (double *)&hp->priority, fip, str)) {
		    free_hold(hp);
		    skip_group(gedp, fip, str);
		    return 0;
		}
		prifound=1;
		break;
	    case KEY_JOINTS:
		if (jsetfound) {
		    parse_error(gedp, str, "parse_hold: joint set redefined.");
		    free_hold(hp);
		    skip_group(gedp, fip, str);
		    return 0;
		}
		if (!parse_jset(gedp, hp, fip, str)) {
		    free_hold(hp);
		    skip_group(gedp, fip, str);
		    return 0;
		}
		jsetfound = 1;
		break;
	    case KEY_EFF:
		if (!gobble_token(gedp, BU_LEX_SYMBOL, SYM_EQ, fip, str)) {
		    skip_group(gedp, fip, str);
		    free_hold(hp);
		    return 0;
		}
		if (!parse_point(gedp, &hp->effector, fip, str)) {
		    skip_group(gedp, fip, str);
		    free_hold(hp);
		    return 0;
		}
		efffound = 1;
		break;
	    case KEY_POINT:
		if (!gobble_token(gedp, BU_LEX_SYMBOL, SYM_EQ, fip, str)) {
		    skip_group(gedp, fip, str);
		    free_hold(hp);
		    return 0;
		}
		if (!parse_point(gedp, &hp->objective, fip, str)) {
		    skip_group(gedp, fip, str);
		    free_hold(hp);
		    return 0;
		}
		goalfound=1;
		break;
	    default:
		parse_error(gedp, str, "parse_hold: syntax error.");
		break;
	}
    }
    /* NOTREACHED */
}


static void
joint_adjust(struct ged *gedp, struct joint *jp)
{
    struct animate *anp;
    double tmp;
    mat_t m1, m2;
    quat_t q1;
    int i;

    if (gedp->ged_wdbp->dbip == DBI_NULL)
	return;

    /*
     * If no animate structure, cons one up.
     */
    anp=jp->anim;
    if (!anp || anp->magic != ANIMATE_MAGIC) {
	char *sofar;
	struct directory *dp = NULL;
	BU_ALLOC(anp, struct animate); /* may be free'd by librt */
	anp->magic = ANIMATE_MAGIC;
	db_full_path_init(&anp->an_path);

	for (i=0; i<= jp->path.arc_last; i++) {
	    dp = db_lookup(gedp->ged_wdbp->dbip, jp->path.arc[i], LOOKUP_NOISY);
	    if (!dp) {
		continue;
	    }
	    db_add_node_to_full_path(&anp->an_path, dp);
	}
	jp->anim=anp;
	db_add_anim(gedp->ged_wdbp->dbip, anp, 0);

	if (J_DEBUG & DEBUG_J_MOVE) {
	    sofar = db_path_to_string(&jp->anim->an_path);
	    bu_vls_printf(gedp->ged_result_str, "joint move: %s added animate %s to %s(%p)\n",
			  jp->name, sofar, dp->d_namep, (void *)dp);
	}
    }


#define ANIM_MAT (anp->an_u.anu_m.anm_mat)

    anp->an_type = RT_AN_MATRIX;
    anp->an_u.anu_m.anm_op = ANM_RMUL;

    /*
     * Build the base matrix.  Ident with translate back to origin.
     */
    MAT_IDN(ANIM_MAT);
    MAT_DELTAS_VEC_NEG(ANIM_MAT, jp->location);

    /*
     * Do rotations.
     */
    for (i=0; i<3; i++) {
	if (jp->rots[i].upper < jp->rots[i].lower)
	    break;
	/*
	 * Build a quat from that.
	 */
	tmp = (jp->rots[i].current * DEG2RAD)/2.0;
	VMOVE(q1, jp->rots[i].quat);
	if (J_DEBUG & DEBUG_J_MOVE) {
	    bu_vls_printf(gedp->ged_result_str, "joint move: rotating %g around (%g %g %g)\n",
			  tmp*2*RAD2DEG, q1[X], q1[Y], q1[Z]);
	}
	{double srot = sin(tmp);
	    q1[X] *= srot;
	    q1[Y] *= srot;
	    q1[Z] *= srot;
	}
	q1[W] = cos(tmp);

	/*
	 * Build matrix.
	 */
	quat_quat2mat(m2, q1);
	MAT_COPY(m1, ANIM_MAT);
	bn_mat_mul(ANIM_MAT, m2, m1);
	/*
	 * rmult matrix into the mat we are building.
	 */
    }
    /*
     * do the translations.
     */
    for (i=0; i<3; i++) {
	if (jp->dirs[i].upper < jp->dirs[i].lower)
	    break;
	/*
	 * build matrix.
	 */
	tmp = jp->dirs[i].current;
	MAT_IDN(m2);
	MAT_DELTAS(m2, jp->dirs[i].unitvec[X]*tmp,
		   jp->dirs[i].unitvec[Y]*tmp,
		   jp->dirs[i].unitvec[Z]*tmp);

	if (J_DEBUG & DEBUG_J_MOVE) {
	    bu_vls_printf(gedp->ged_result_str, "joint move: moving %g along (%g %g %g)\n",
			  tmp*gedp->ged_wdbp->dbip->dbi_base2local, m2[3], m2[7], m2[11]);
	}
	MAT_COPY(m1, ANIM_MAT);
	bn_mat_mul(ANIM_MAT, m2, m1);
    }
    /*
     * Now move the whole thing back to original location.
     */
    MAT_IDN(m2);
    MAT_DELTAS_VEC(m2, jp->location);
    MAT_COPY(m1, ANIM_MAT);
    bn_mat_mul(ANIM_MAT, m2, m1);
    if (J_DEBUG & DEBUG_J_MOVE) {
	bn_mat_print("joint move: ANIM_MAT", ANIM_MAT);
    }
}


static int
joint_load(struct ged *gedp, int argc, const char *argv[])
{
    static struct bu_list path_head;

    FILE *fip;
    struct bu_vls instring = BU_VLS_INIT_ZERO;
    union bu_lex_token token;
    int no_unload = 0, no_apply=0, no_mesh=0;
    int c;
    struct joint *jp;
    struct hold *hp;

    if (gedp->ged_wdbp->dbip == DBI_NULL) {
	bu_vls_printf(gedp->ged_result_str, "A database is not open!\n");
	return GED_ERROR;
    }

    bu_optind = 1;
    while ((c=bu_getopt(argc, (char * const *)argv, "uam")) != -1) {
	switch (c) {
	    case 'u': no_unload = 1;break;
	    case 'a': no_apply = 1; break;
	    case 'm': no_mesh = 1; break;
	    default:
		bu_vls_printf(gedp->ged_result_str, "Usage: joint load [-uam] file_name [files]\n");
		break;
	}
    }
    argv += bu_optind;
    argc -= bu_optind;
    if (!no_unload) joint_unload(gedp, 0, NULL);

    base2mm = gedp->ged_wdbp->dbip->dbi_base2local;
    mm2base = gedp->ged_wdbp->dbip->dbi_local2base;

    while (argc) {
	fip = fopen(*argv, "rb");
	if (fip == NULL) {
	    bu_vls_printf(gedp->ged_result_str, "joint load: unable to open '%s'.\n", *argv);
	    ++argv;
	    --argc;
	    continue;
	}
	if (J_DEBUG & DEBUG_J_LOAD) {
	    bu_vls_printf(gedp->ged_result_str, "joint load: loading from '%s'", *argv);
	}
	lex_line = 0;
	lex_name = *argv;

	while (get_token(gedp, &token, fip, &instring, animkeys, animsyms) != EOF) {
	    if (token.type == BU_LEX_KEYWORD) {
		if (token.t_key.value == KEY_JOINT) {
		    if (parse_joint(gedp, fip, &instring)) {
			jp = BU_LIST_LAST(joint, &joint_head);
			if (!no_apply) joint_adjust(gedp, jp);
		    }
		} else if (token.t_key.value == KEY_CON) {
		    (void)parse_hold(gedp, fip, &instring);
		} else if (token.t_key.value == KEY_UNITS) {
		    (void)parse_units(gedp, fip, &instring);
		} else {
		    parse_error(gedp, &instring, "joint load: syntax error.");
		}
	    } else {
		parse_error(gedp, &instring, "joint load: syntax error.");
	    }
	    if (token.type == BU_LEX_IDENT) {
		bu_free(token.t_id.value, "unit token");
	    }
	}
	fclose(fip);
	argc--;
	argv++;
    }
/* CTJ */
    /*
     * For each "struct arc" in joints or constraints, build a linked
     * list of all ARC_PATHs and a control list of all unique tops.
     */
    BU_LIST_INIT(&path_head);
    for (BU_LIST_FOR(jp, joint, &joint_head)) {
	if (jp->path.type == ARC_PATH) {
	    BU_LIST_INSERT(&path_head, &(jp->path.l));
	}
    }
    for (BU_LIST_FOR(hp, hold, &hold_head)) {
	if (hp->j_set.path.type == ARC_PATH) {
	    BU_LIST_INSERT(&path_head, &(hp->j_set.path.l));
	}
	if (hp->effector.arc.type == ARC_PATH) {
	    BU_LIST_INSERT(&path_head, &(hp->effector.arc.l));
	}
	if (hp->objective.arc.type == ARC_PATH) {
	    BU_LIST_INSERT(&path_head, &(hp->objective.arc.l));
	}
    }
    /*
     * call the tree walker to search for these paths.
     */
    /*
     * All ARC_PATHS have been translated into ARC_ARC.
     *
     * Constraints need to have ARC_ARCs translated to ARC_LISTS, this
     * can be done at a latter time, such as when the constraint is
     * evaluated. ??? XXX
     */
    for (BU_LIST_FOR(hp, hold, &hold_head)) {
	struct directory *dp;
	int i;

	if (hp->effector.arc.type == ARC_ARC) {
	    db_full_path_init(&hp->effector.path);

	    for (i=0; i<= hp->effector.arc.arc_last; i++) {
		dp = db_lookup(gedp->ged_wdbp->dbip, hp->effector.arc.arc[i], LOOKUP_NOISY);
		if (!dp) {
		    continue;
		}
		db_add_node_to_full_path(&hp->effector.path, dp);
	    }
	}
	if (hp->objective.arc.type == ARC_ARC) {
	    db_full_path_init(&hp->objective.path);

	    for (i=0; i<= hp->objective.arc.arc_last; i++) {
		dp = db_lookup(gedp->ged_wdbp->dbip, hp->objective.arc.arc[i], LOOKUP_NOISY);
		if (!dp) {
		    break;
		}
		db_add_node_to_full_path(&hp->objective.path, dp);
	    }
	}
    }
    if (!no_mesh) (void) joint_mesh(gedp, 0, 0);
    return GED_OK;
}


static int
joint_save(struct ged *gedp, int argc, const char *argv[])
{
    struct joint *jp;
    int i;
    FILE *fop;

    if (gedp->ged_wdbp->dbip == DBI_NULL) {
	bu_vls_printf(gedp->ged_result_str, "A database is not open!\n");
	return GED_ERROR;
    }

    --argc;
    ++argv;

    if (argc <1) {
	bu_vls_printf(gedp->ged_result_str, "joint save: missing file name");
	return GED_ERROR;
    }
    fop = fopen(*argv, "wb");
    if (!fop) {
	bu_vls_printf(gedp->ged_result_str, "joint save: unable to open '%s' for writing.\n", *argv);
	return GED_ERROR;
    }
    fprintf(fop, "# joints and constraints for '%s'\n",
	    gedp->ged_wdbp->dbip->dbi_title);

    /* Output the current editing units */
    fprintf(fop, "units %gmm;\n", gedp->ged_wdbp->dbip->dbi_local2base);

    mm2base = gedp->ged_wdbp->dbip->dbi_local2base;
    base2mm = gedp->ged_wdbp->dbip->dbi_base2local;

    for (BU_LIST_FOR(jp, joint, &joint_head)) {
	fprintf(fop, "joint %s {\n", jp->name);
	if (jp->path.type == ARC_PATH) {
	    fprintf(fop, "\tpath = %s", jp->path.arc[0]);
	    for (i=1;i<jp->path.arc_last;i++) {
		fprintf(fop, "/%s", jp->path.arc[i]);
	    }
	    fprintf(fop, "-%s;\n", jp->path.arc[i]);
	} else if (jp->path.type & ARC_BOTH) {
	    fprintf(fop, "\tpath = %s", jp->path.original[0]);
	    for (i=1; i < jp->path.org_last; i++) {
		fprintf(fop, "/%s", jp->path.original[i]);
	    }
	    fprintf(fop, "-%s;\n", jp->path.original[i]);
	} else {
	    /* ARC_ARC */
	    fprintf(fop, "\tarc = %s", jp->path.arc[0]);
	    for (i=1;i<jp->path.arc_last;i++) {
		fprintf(fop, "/%s", jp->path.arc[i]);
	    }
	    fprintf(fop, "/%s;\n", jp->path.arc[i]);
	}
	fprintf(fop, "\tlocation = (%.15e, %.15e, %.15e);\n",
		jp->location[X]*mm2base, jp->location[Y]*mm2base,
		jp->location[Z]*mm2base);

	for (i=0;i<3;i++) {
	    if (jp->rots[i].upper < jp->rots[i].lower)
		break;
	    fprintf(fop,
		    "\trotate {\n\t\tdirection = (%.15e, %.15e, %.15e);\n\t\tlimits = %.15e, %.15e, %.15e;\n\t}\n",
		    jp->rots[i].quat[X], jp->rots[i].quat[Y],
		    jp->rots[i].quat[Z],
		    jp->rots[i].lower, jp->rots[i].upper,
		    jp->rots[i].current);
	}
	for (i=0;i<3;i++) {
	    if (jp->dirs[i].upper < jp->dirs[i].lower)
		break;
	    fprintf(fop,
		    "\ttranslate {\n\t\tdirection = (%.15e, %.15e, %.15e);\n\t\tlimits = %.15e, %.15e, %.15e;\n\t}\n",
		    jp->dirs[i].unitvec[X], jp->dirs[i].unitvec[Y],
		    jp->dirs[i].unitvec[Z], jp->dirs[i].lower*mm2base,
		    jp->dirs[i].upper*mm2base, jp->dirs[i].current*mm2base);
	}
	fprintf(fop, "};\n");
    }
    fclose(fop);
    return GED_OK;
}


static int
joint_accept(struct ged *gedp, int argc, const char *argv[])
{
    struct joint *jp;
    int i;
    int c;
    int no_mesh = 0;

    bu_optind=1;
    while ((c=bu_getopt(argc, (char * const *)argv, "m")) != -1) {
	switch (c) {
	    case 'm': no_mesh=1;break;
	    default:
		bu_vls_printf(gedp->ged_result_str, "Usage: joint accept [-m] [joint_names]\n");
		break;
	}
    }
    argc -= bu_optind;
    argv += bu_optind;

    for (BU_LIST_FOR(jp, joint, &joint_head)) {
	if (argc) {
	    for (i=0; i<argc; i++) {
		if (BU_STR_EQUAL(argv[i], jp->name))
		    break;
	    }
	    if (i>=argc)
		continue;
	}
	for (i=0; i<3; i++) {
	    jp->dirs[i].accepted = jp->dirs[i].current;
	    jp->rots[i].accepted = jp->rots[i].current;
	}
    }
    if (!no_mesh) joint_mesh(gedp, 0, 0);
    return GED_OK;
}


static int
joint_reject(struct ged *gedp, int argc, const char *argv[])
{
    struct joint *jp;
    int i;
    int c;
    int no_mesh = 0;

    bu_optind=1;
    while ((c=bu_getopt(argc, (char * const *)argv, "m")) != -1) {
	switch (c) {
	    case 'm': no_mesh=1;break;
	    default:
		bu_vls_printf(gedp->ged_result_str, "Usage: joint accept [-m] [joint_names]\n");
		break;
	}
    }
    argc -= bu_optind;
    argv += bu_optind;

    for (BU_LIST_FOR(jp, joint, &joint_head)) {
	if (argc) {
	    for (i=0; i<argc; i++) {
		if (BU_STR_EQUAL(argv[i], jp->name))
		    break;
	    }
	    if (i>=argc)
		continue;
	}

	for (i=0; i<3; i++) {
	    jp->rots[i].current = jp->rots[i].accepted;
	    jp->dirs[i].current = jp->dirs[i].accepted;
	}
	joint_adjust(gedp, jp);
    }
    if (!no_mesh) joint_mesh(gedp, 0, 0);
    return GED_OK;
}


struct solve_stack {
    struct bu_list l;
    struct joint *jp;
    int freedom;
    double oldval;
    double newval;
};
#define SOLVE_STACK_MAGIC 0x76766767
struct bu_list solve_head = {
    BU_LIST_HEAD_MAGIC,
    &solve_head,
    &solve_head
};


static void
joint_clear(void)
{
    struct stack_solve *ssp;
    BU_LIST_POP(stack_solve, &solve_head, ssp);
    while (ssp) {
	bu_free((void *)ssp, "struct stack_solve");
	BU_LIST_POP(stack_solve, &solve_head, ssp);
    }
}


static int
part_solve(struct ged *gedp, struct hold *hp, double limits, double tol)
{
    struct joint *jp;
    double f0, f1, f2;
    double ax, bx, cx;
    double x0, x1, x2, x3;
    double besteval, bestvalue = 0, origvalue;
    int bestfreedom = -1;
    struct joint *bestjoint;
    struct jointH *jh;

    if (J_DEBUG & DEBUG_J_SOLVE) {
	bu_vls_printf(gedp->ged_result_str, "part_solve: solving for %s.\n", hp->name);
    }

    if (BU_LIST_IS_EMPTY(&hp->j_head)) {
	size_t i, j;
	int startjoint;
	startjoint = -1;
	if (J_DEBUG & DEBUG_J_SOLVE) {
	    bu_vls_printf(gedp->ged_result_str, "part_solve: looking for joints on arc.\n");
	}
	for (BU_LIST_FOR(jp, joint, &joint_head)) {
	    if (hp->j_set.path.type == ARC_LIST) {
		for (i=0; i <= (size_t)hp->j_set.path.arc_last; i++) {
		    if (BU_STR_EQUAL(jp->name, hp->j_set.path.arc[i])) {
			BU_GET(jh, struct jointH);
			jh->l.magic = MAGIC_JOINT_HANDLE;
			jh->p = jp;
			jp->uses++;
			jh->arc_loc = -1;
			jh->flag = 0;
			BU_LIST_APPEND(&hp->j_head, &jh->l);
			break;
		    }
		}
		continue;
	    }
	    for (i=0;i<hp->effector.path.fp_len; i++) {
		if (!BU_STR_EQUAL(jp->path.arc[0],
				  hp->effector.path.fp_names[i]->d_namep)==0)
		    break;
	    }
	    if (i+jp->path.arc_last >= hp->effector.path.fp_len)
		continue;
	    for (j=1; j <= (size_t)jp->path.arc_last;j++) {
		if (!BU_STR_EQUAL(jp->path.arc[j],
				  hp->effector.path.fp_names[i+j]->d_namep)
		    != 0)
		    break;
	    }
	    if (j>(size_t)jp->path.arc_last) {
		if (J_DEBUG & DEBUG_J_SOLVE) {
		    bu_vls_printf(gedp->ged_result_str, "part_solve: found %s\n", jp->name);
		}
		BU_GET(jh, struct jointH);
		jh->l.magic = MAGIC_JOINT_HANDLE;
		jh->p = jp;
		jp->uses++;
		jh->arc_loc = i+j-1;
		jh->flag = 0;
		BU_LIST_APPEND(&hp->j_head, &jh->l);
		if (BU_STR_EQUAL(hp->joint, jp->name)) {
		    startjoint = jh->arc_loc;
		}
	    }
	}
	if (startjoint < 0) {
	    bu_vls_printf(gedp->ged_result_str, "part_solve: %s, joint %s not on arc.\n", hp->name, hp->joint);
	}
	for (BU_LIST_FOR(jh, jointH, &hp->j_head)) {
	    /*
	     * XXX - Coming to a source module near you RSN.
	     * Not only joint location, but drop joints that
	     * are "locked"
	     */
	    if (jh->arc_loc < startjoint) {
		struct jointH *hold;
		if (J_DEBUG & DEBUG_J_SOLVE) {
		    bu_vls_printf(gedp->ged_result_str, "part_solve: dequeuing %s from %s", jh->p->name, hp->name);
		}
		hold=(struct jointH *)jh->l.back;
		BU_LIST_DEQUEUE(&jh->l);
		jh->p->uses--;
		jh=hold;
	    }
	}
    }
    origvalue = besteval = hold_eval(gedp, hp);
    if (fabs(origvalue) < tol) {
	if (J_DEBUG & DEBUG_J_SOLVE) {
	    bu_vls_printf(gedp->ged_result_str, "part_solve: solved, original(%g) < tol(%g)\n",
			  origvalue, tol);
	}
	return 0;
    }
    bestjoint = (struct joint *)0;
    /*
     * From here, we try each joint to try and find the best movement
     * if any.
     */
    for (BU_LIST_FOR(jh, jointH, &hp->j_head)) {
	int i;
	double hold;
	jp= jh->p;
	for (i=0;i<3;i++) {
	    if ((jh->flag & (1<<i)) ||
		jp->rots[i].upper < jp->rots[i].lower) {
		jh->flag |= (1<<i);
		continue;
	    }
	    hold = bx =jp->rots[i].current;
#define EPSI 1e-6
#define R 0.61803399
#define C (1.0-R)
	    /*
	     * find the min in the range ax-bx-cx where ax is
	     * bx-limits-0.001 or lower and cx = bx+limits+0.001
	     * or upper.
	     */
	    ax=bx-limits-EPSI;
	    if (ax < jp->rots[i].lower)
		ax=jp->rots[i].lower;
	    cx=bx+limits+EPSI;
	    if (cx > jp->rots[i].upper)
		cx=jp->rots[i].upper;
	    x0=ax;
	    x3=cx;
	    if (fabs(cx-bx) > fabs(bx-ax)) {
		x1=bx;
		x2=bx+C*(cx-bx);
	    } else {
		x2=bx;
		x1=bx-C*(bx-ax);
	    }
	    jp->rots[i].current = x1;
	    joint_adjust(gedp, jp);
	    f1=hold_eval(gedp, hp);
	    jp->rots[i].current = x2;
	    joint_adjust(gedp, jp);
	    f2=hold_eval(gedp, hp);
	    while (fabs(x3-x0) > EPSI*(fabs(x1)+fabs(x2))) {
		if (f2 < f1) {
		    x0 = x1;
		    x1 = x2;
		    x2 = R*x1+C*x3;
		    f1=f2;
		    jp->rots[i].current = x2;
		    joint_adjust(gedp, jp);
		    f2=hold_eval(gedp, hp);
		} else {
		    x3=x2;
		    x2=x1;
		    x1=R*x2+C*x0;
		    f2=f1;
		    jp->rots[i].current = x1;
		    joint_adjust(gedp, jp);
		    f1=hold_eval(gedp, hp);
		}
	    }
	    if (f1 < f2) {
		x0=x1;
		f0=f1;
	    } else {
		x0=x2;
		f0=f2;
	    }
	    jp->rots[i].current = hold;
	    joint_adjust(gedp, jp);
	    if (f0 < besteval) {
		if (J_DEBUG & DEBUG_J_SOLVE) {
		    bu_vls_printf(gedp->ged_result_str, "part_solve: NEW min %s(%d, %g) %g <%g\n",
				  jp->name, i, x0, f0, besteval);
		}
		besteval = f0;
		bestjoint = jp;
		bestfreedom = i;
		bestvalue = x0;
	    } else if (J_DEBUG & DEBUG_J_SOLVE) {
		bu_vls_printf(gedp->ged_result_str, "part_solve: OLD min %s(%d, %g)%g >= %g\n",
			      jp->name, i, x0, f0, besteval);
	    }
	}
	/*
	 * Now we do the same thing but for directional movements.
	 */
	for (i=0;i<3;i++) {
	    if ((jh->flag & (1<<(i+3))) ||
		(jp->dirs[i].upper < jp->dirs[i].lower)) {
		jh->flag |= (1<<(i+3));
		continue;
	    }
	    hold = bx =jp->dirs[i].current;
	    /*
	     * find the min in the range ax-bx-cx where ax is
	     * bx-limits-0.001 or lower and cx = bx+limits+0.001
	     * or upper.
	     */
	    ax=bx-limits-EPSI;
	    if (ax < jp->dirs[i].lower) ax=jp->dirs[i].lower;
	    cx=bx+limits+EPSI;
	    if (cx > jp->dirs[i].upper) cx=jp->dirs[i].upper;
	    x0=ax;
	    x3=cx;
	    if (fabs(cx-bx) > fabs(bx-ax)) {
		x1=bx;
		x2=bx+C*(cx-bx);
	    } else {
		x2=bx;
		x1=bx-C*(bx-ax);
	    }
	    jp->dirs[i].current = x1;
	    joint_adjust(gedp, jp);
	    f1=hold_eval(gedp, hp);
	    jp->dirs[i].current = x2;
	    joint_adjust(gedp, jp);
	    f2=hold_eval(gedp, hp);
	    while (fabs(x3-x0) > EPSI*(fabs(x1)+fabs(x2))) {
		if (f2 < f1) {
		    x0 = x1;
		    x1 = x2;
		    x2 = R*x1+C*x3;
		    f1=f2;
		    jp->dirs[i].current = x2;
		    joint_adjust(gedp, jp);
		    f2=hold_eval(gedp, hp);
		} else {
		    x3=x2;
		    x2=x1;
		    x1=R*x2+C*x0;
		    f2=f1;
		    jp->dirs[i].current = x1;
		    joint_adjust(gedp, jp);
		    f1=hold_eval(gedp, hp);
		}
	    }
	    if (f1 < f2) {
		x0=x1;
		f0=f1;
	    } else {
		x0=x2;
		f0=f2;
	    }
	    jp->dirs[i].current = hold;
	    joint_adjust(gedp, jp);
	    if (f0 < besteval-SQRT_SMALL_FASTF) {
		if (J_DEBUG & DEBUG_J_SOLVE) {
		    bu_vls_printf(gedp->ged_result_str, "part_solve: NEW min %s(%d, %g) %g <%g delta=%g\n",
				  jp->name, i+3, x0, f0, besteval, besteval-f0);
		}
		besteval = f0;
		bestjoint = jp;
		bestfreedom = i + 3;
		bestvalue = x0;
	    } else if (J_DEBUG & DEBUG_J_SOLVE) {
		bu_vls_printf(gedp->ged_result_str, "part_solve: OLD min %s(%d, %g)%g >= %g\n",
			      jp->name, i, x0, f0, besteval);
	    }

	}
    }
    /*
     * Did we find a better joint?
     */
    if (!bestjoint) {
	if (J_DEBUG & DEBUG_J_SOLVE) {
	    bu_vls_printf(gedp->ged_result_str, "part_solve: No joint configuration found to be better.\n");
	}
	return 0;
    }
    if (origvalue - besteval < (tol/100.0)) {
	if (J_DEBUG & DEBUG_J_SOLVE) {
	    bu_vls_printf(gedp->ged_result_str, "part_solve: No reasonable improvement found.\n");
	}
	return 0;
    }
    {
	struct solve_stack *ssp;
	BU_GET(ssp, struct solve_stack);
	ssp->jp = bestjoint;
	ssp->freedom = bestfreedom;
	ssp->oldval = (bestfreedom<3) ? bestjoint->rots[bestfreedom].current :
	    bestjoint->dirs[bestfreedom-3].current;
	ssp->newval = bestvalue;
	BU_LIST_PUSH(&solve_head, ssp);
    }
    if (bestfreedom < 3) {
	bestjoint->rots[bestfreedom].current = bestvalue;
    } else {
	bestjoint->dirs[bestfreedom-3].current = bestvalue;
    }
    joint_adjust(gedp, bestjoint);
    return 1;
}


static void
reject_move(struct ged *gedp)
{
    struct solve_stack *ssp;
    BU_LIST_POP(solve_stack, &solve_head, ssp);
    if (!ssp)
	return;

    if (J_DEBUG & DEBUG_J_SYSTEM) {
	bu_vls_printf(gedp->ged_result_str, "reject_move: rejecting %s(%d, %g)->%g\n", ssp->jp->name,
		      ssp->freedom, ssp->newval, ssp->oldval);
    }
    if (ssp->freedom<3) {
	ssp->jp->rots[ssp->freedom].current = ssp->oldval;
    } else {
	ssp->jp->dirs[ssp->freedom-3].current = ssp->oldval;
    }
    joint_adjust(gedp, ssp->jp);
    BU_PUT(ssp, struct solve_stack);
}


/* Constraint system solver.
 *
 * The basic idea is that we are called with some priority level.  We
 * will attempt to solve all constraints nodes at that level without
 * breaking a joint solution for a node of a higher priority.
 *
 * Returns:
 *
 * -1: This system could not be made better without worsening a higher
 * priority node.
 *
 * 0: All nodes at a higher priority stayed stable or got better and
 * this priority level got better.
 *
 * 1: All systems at a higher priority stayed stable or got better and
 * this system is at at min.
 *
 * Method:
 *	while all constraints at this level are not solved:
 *		try to solve the selected constraint
 *		if joint changed and this priority level got better then
 *			result = system_solve(current_priority - 1);
 *		else if (result == worse) then
 *			reject this joint change
 *		else
 *			mark this constraint as "solved"
 *		fi
 *	endwhile
 */
static int
system_solve(struct ged *gedp, int pri, double delta, double epsilon)
{
#define SOLVE_MAX_PRIORITY 100
    double pri_weights[SOLVE_MAX_PRIORITY+1];
    double new_weights[SOLVE_MAX_PRIORITY+1];
    double new_eval;
    int i;
    int j;
    struct hold *hp;
    struct jointH *jh;
    struct solve_stack *ssp;
    struct hold *test_hold = NULL;

    if (pri < 0)
	return 1;

    for (i=0; i<=pri; i++)
	pri_weights[i]=0.0;

    for (BU_LIST_FOR(hp, hold, &hold_head)) {
	hp->eval = hold_eval(gedp, hp);
	pri_weights[hp->priority] += hp->eval;
    }

    if (J_DEBUG & DEBUG_J_SYSTEM) {
	for (i=0; i <= pri; i++) {
	    if (pri_weights[i] > 0.0) {
		bu_vls_printf(gedp->ged_result_str, "system_solve: priority %d has system weight of %g.\n",
			      i, pri_weights[i]);
	    }
	}
    }
    /*
     * sort constraints by priority then weight from the evaluation
     * we just did.
     */
    for (hp=(struct hold *)hold_head.forw; hp->l.forw != &hold_head;) {
	struct hold *tmp;
	tmp = (struct hold *)hp->l.forw;

	if ((tmp->priority < hp->priority) ||
	    ((tmp->priority == hp->priority) &&
	     (tmp->eval > hp->eval))) {
	    BU_LIST_DEQUEUE(&tmp->l);
	    BU_LIST_INSERT(&hp->l, &tmp->l);
	    if (tmp->l.back != &hold_head) {
		hp = (struct hold*)tmp->l.back;
	    }
	} else {
	    hp = (struct hold*)hp->l.forw;
	}
    }
Middle:
    /*
     * now we find the constraint(s) we will be working with.
     */
    for (; pri>=0 && pri_weights[pri] < epsilon; pri--)
	;
    if (pri <0) {
	if (J_DEBUG & DEBUG_J_SYSTEM) {
	    bu_vls_printf(gedp->ged_result_str, "system_solve: returning 1\n");
	}
	return 1;	/* solved */
    }
    for (BU_LIST_FOR(hp, hold, &hold_head)) {
	if (hp->priority != pri)
	    continue;
	if (hp->flag & HOLD_FLAG_TRIED)
	    continue;
	if (part_solve(gedp, hp, delta, epsilon)==0)
	    continue;
	test_hold = hp;
	break;
    }
    /*
     * Now check to see if a) anything happened, b) that it was good
     * for the entire system.
     */
    if (hp==(struct hold*)&hold_head) {
	/*
	 * There was nothing we could do at this level.  Try
	 * again at a higher level.
	 */
	pri--;
	goto Middle;
    }
    /*
     * We did something so lets re-evaluated and see if it got any
     * better at THIS level only.  breaking things of lower priority
     * does not bother us.  If things got worse at a lower priority
     * we'll know that in a little bit.
     */
    new_eval = 0.0;
    for (BU_LIST_FOR(hp, hold, &hold_head)) {
	if (hp->priority != pri)
	    continue;
	new_eval += hold_eval(gedp, hp);
    }
    if (J_DEBUG & DEBUG_J_SYSTEM) {
	bu_vls_printf(gedp->ged_result_str, "system_solve: old eval = %g, new eval = %g\n",
		      pri_weights[pri], new_eval);
    }
    /*
     * if the new evaluation is worse than the original, back off
     * this modification, set the constraint such that this freedom
     * of this joint won't be used next time through part_solve.
     */
    if (new_eval > pri_weights[pri]+epsilon) {
	/*
	 * now we see if there is anything we can do with this
	 * constraint.
	 */
	ssp = (struct solve_stack *) solve_head.forw;

	i = (2<<6) - 1;		/* Six degrees of freedom */
	if (test_hold) {
	    /* make sure we've got test_hold */
	    for (BU_LIST_FOR(jh, jointH, &test_hold->j_head)) {
		if (ssp->jp != jh->p) {
		    i &= jh->flag;
		    continue;
		}
		jh->flag |= (1 << ssp->freedom);
		i &= jh->flag;
	    }
	    if (i == ((2<<6)-1)) {
		/* All joints, all freedoms */
		test_hold->flag |= HOLD_FLAG_TRIED;
	    }
	}
	reject_move(gedp);
	goto Middle;
    }
    /*
     * OK, we've got a constraint that makes this priority system
     * better, now we've got to make sure all the constraints below
     * this are better or solve also.
     */
    ssp = (struct solve_stack *) solve_head.forw;
    for (j=0; (i = system_solve(gedp, pri-1, delta, epsilon)) == 0; j++)
	;
    /*
     * All constraints at a higher priority are stabilized.
     *
     * If system_solve returns "1" then every thing higher is happy
     * and we only have to worry about his one.  If -1 was returned
     * then all higher priorities have to be check to make sure they
     * did not get any worse.
     */
    for (j=0; j<=pri; j++)
	new_weights[j] = 0.0;
    for (BU_LIST_FOR(hp, hold, &hold_head)) {
	new_weights[hp->priority] += hold_eval(gedp, hp);
    }
    for (j=0; j<=pri; j++) {
	if (new_weights[j] > pri_weights[j] + epsilon) {
	    break;
	}
    }
    /*
     * if j <= pri, then that priority got worse.  Since it is
     * worse, we need to clean up what's been done before and
     * exit out of here.
     */
    if (j <= pri) {
	while (ssp != (struct solve_stack *) solve_head.forw) {
	    reject_move(gedp);
	}
	i = (2 << 6) - 1;
	if (test_hold) {
	    /* again, make sure we've got test_hold */
	    for (BU_LIST_FOR(jh, jointH, &test_hold->j_head)) {
		if (ssp->jp != jh->p) {
		    i &= jh->flag;
		    continue;
		}
		jh->flag |= (1 << ssp->freedom);
		i &= jh->flag;
	    }
	    if (i == ((2<<6) - 1)) {
		test_hold->flag |= HOLD_FLAG_TRIED;
	    }
	}
	reject_move(gedp);
	if (J_DEBUG & DEBUG_J_SYSTEM) {
	    bu_vls_printf(gedp->ged_result_str, "system_solve: returning -1\n");
	}
	return -1;
    }
    if (J_DEBUG & DEBUG_J_SYSTEM) {
	bu_vls_printf(gedp->ged_result_str, "system_solve: new_weights[%d] = %g, returning ", pri,
		      new_weights[pri]);
    }
    if (new_weights[pri] < epsilon) {
	if (J_DEBUG & DEBUG_J_SYSTEM) {
	    bu_vls_printf(gedp->ged_result_str, "1\n");
	}
	return 1;
    }
    if (J_DEBUG & DEBUG_J_SYSTEM) {
	bu_vls_printf(gedp->ged_result_str, "0\n");    }
    return 0;

}


static int
joint_solve(struct ged *gedp, int argc, char *argv[])
{
    struct hold *hp;
    int loops, count;
    double delta, epsilon;
    int domesh;
    int found;
    char **myargv;
    int myargc;
    int result = 0;

    /*
     * because this routine calls "mesh" in the middle, the command
     * arguments can be reused.  We cons up a new argv vector and
     * copy all of the arguments before we do any processing.
     */
    myargc = argc;
    myargv = (char **)bu_malloc(sizeof(char *)*argc, "param pointers");

    for (count=0; count<myargc; count++) {
	myargv[count] = (char *)bu_malloc(strlen(argv[count])+1, "param");
	bu_strlcpy(myargv[count], argv[count], strlen(argv[count])+1);
    }

    argv=myargv;
    /* argc = myargc; */

    /*
     * these are the defaults.  Domesh will change to not at a later
     * time.
     */
    loops = 1000;
    delta = 16.0;
    epsilon = 0.1;
    domesh = 1;

    /*
     * reset bu_getopt.
     */
    bu_optind=1;
    while ((count=bu_getopt(argc, (char * const *)argv, "l:e:d:m")) != -1) {
	switch (count) {
	    case 'l': loops = atoi(bu_optarg);break;
	    case 'e': epsilon = atof(bu_optarg);break;
	    case 'd': delta =  atof(bu_optarg);break;
	    case 'm': domesh = 1-domesh;
	}
    }

    /*
     * skip the command and any options that bu_getopt ate.
     */
    argc -= bu_optind;
    argv += bu_optind;

    for (BU_LIST_FOR(hp, hold, &hold_head))
	hold_clear_flags(hp);

    found = -1;
    while (argc) {
	found = 0;
	for (BU_LIST_FOR(hp, hold, &hold_head)) {
	    if (BU_STR_EQUAL(*argv, hp->name)) {
		found = 1;
		for (count=0; count<loops; count++) {
		    if (!part_solve(gedp, hp, delta, epsilon))
			break;
		    if (domesh) {
			joint_mesh(gedp, 0, 0);
			/* refreshing the screen */
			if (gedp->ged_refresh_handler != GED_REFRESH_CALLBACK_PTR_NULL)
			    (*gedp->ged_refresh_handler)(gedp->ged_refresh_clientdata);
		    }
		    joint_clear();
		}

		{
		    bu_vls_printf(gedp->ged_result_str, "joint solve: finished %d loops of %s.\n",
				  count, hp->name);
		}

		continue;
	    }
	}
	if (!found) {
	    bu_vls_printf(gedp->ged_result_str, "joint solve: constraint %s not found.\n", *argv);
	}
	--argc;
	++argv;
    }

    for (count=0; count<myargc; count++) {
	bu_free(myargv[count], "params");
    }
    bu_free((void *)myargv, "param pointers");

    if (found >= 0)
	return GED_ERROR;

    /*
     * solve the whole system of constraints.
     */

    joint_clear();	/* make sure the system is empty. */

    for (count=0; count < loops; count++) {
	/*
	 * Clear all constraint flags.
	 */
	for (BU_LIST_FOR(hp, hold, &hold_head)) {
	    struct jointH *jh;
	    hp->flag &= ~HOLD_FLAG_TRIED;
	    hp->eval = hold_eval(gedp, hp);
	    for (BU_LIST_FOR(jh, jointH, &hp->j_head)) {
		jh->flag = 0;
	    }
	}
	result = system_solve(gedp, 0, delta, epsilon);
	if (result == 1) {
	    break;
	} else if (result == -1) {
	    delta /= 2.0;
	    if (J_DEBUG & DEBUG_J_SYSTEM) {
		bu_vls_printf(gedp->ged_result_str, "joint solve: splitting delta (%g)\n",
			      delta);
	    }
	    if (delta < epsilon)
		break;
	}
	joint_clear();
	if (domesh) {
	    joint_mesh(gedp, 0, 0);
	    /* refreshing the screen */
	    if (gedp->ged_refresh_handler != GED_REFRESH_CALLBACK_PTR_NULL)
		(*gedp->ged_refresh_handler)(gedp->ged_refresh_clientdata);
	}
    }
    if (count < loops) {
	for (count = 0; count < loops; count++) {
	    /*
	     * Clear all constraint flags.
	     */
	    for (BU_LIST_FOR(hp, hold, &hold_head)) {
		struct jointH *jh;
		hp->flag &= ~HOLD_FLAG_TRIED;
		hp->eval = hold_eval(gedp, hp);
		for (BU_LIST_FOR(jh, jointH, &hp->j_head)) {
		    jh->flag = 0;
		}
	    }
	    result = system_solve(gedp, SOLVE_MAX_PRIORITY, delta, epsilon);
	    if (result == 1) {
		break;
	    } else if (result == -1) {
		delta /= 2.0;
		if (J_DEBUG & DEBUG_J_SYSTEM) {
		    bu_vls_printf(gedp->ged_result_str, "joint solve: splitting delta (%g)\n",
				  delta);
		}
		if (delta < epsilon)
		    break;
	    }
	    joint_clear();
	    if (domesh) {
		joint_mesh(gedp, 0, 0);
		/* refreshing the screen */
		if (gedp->ged_refresh_handler != GED_REFRESH_CALLBACK_PTR_NULL)
		    (*gedp->ged_refresh_handler)(gedp->ged_refresh_clientdata);
	    }
	}
    }
    if (result == 1) {
	bu_vls_printf(gedp->ged_result_str, "joint solve: system has converged to a result (after %d iterations).\n", count+1);
    } else if (result == 0) {
	bu_vls_printf(gedp->ged_result_str, "joint solve: system has not converged after %d iterations.\n", count+1);
    } else {
	bu_vls_printf(gedp->ged_result_str, "joint solve: system will not converge.\n");
    }
    joint_clear();
    if (domesh) {
	joint_mesh(gedp, 0, 0);
	/* refreshing the screen */
	if (gedp->ged_refresh_handler != GED_REFRESH_CALLBACK_PTR_NULL)
	    (*gedp->ged_refresh_handler)(gedp->ged_refresh_clientdata);
    }
    return GED_OK;
}


static int
joint_hold(struct ged *gedp, int argc, const char *argv[])
{
    struct hold *hp;
    ++argv;
    --argc;
    for (BU_LIST_FOR(hp, hold, &hold_head)) {
	if (argc) {
	    int i;
	    for (i=0; i<argc; i++) {
		if (BU_STR_EQUAL(argv[i], hp->name))
		    break;
	    }
	    if (i>=argc)
		continue;
	}
	hold_clear_flags(hp);
	print_hold(gedp, hp);
    }
    return GED_OK;
}


static int
joint_list(struct ged *gedp, int UNUSED(argc), const char *UNUSED(argv[]))
{
    struct joint *jp;

    for (BU_LIST_FOR(jp, joint, &joint_head)) {
	vls_col_item(gedp->ged_result_str, jp->name);
    }
    vls_col_eol(gedp->ged_result_str);

    return GED_OK;
}


static int
joint_move(struct ged *gedp, int argc, const char *argv[])
{
    struct joint *jp;
    int i;
    double tmp;

    if (gedp->ged_wdbp->dbip == DBI_NULL)
	return GED_OK;

    /*
     * find the joint.
     */
    argv++;
    argc--;

    jp = joint_lookup(*argv);
    if (!jp) {
	bu_vls_printf(gedp->ged_result_str, "joint move: %s not found\n", *argv);
	return GED_ERROR;
    }

    argv++;
    argc--;
    for (i=0; i<3 && argc; i++) {
	if (jp->rots[i].upper < jp->rots[i].lower)
	    break;
	/*
	 * Eat a parameter, translate it from degrees to rads.
	 */
	if ((*argv)[0] == '-' && (*argv)[1] == '\0') {
	    ++argv;
	    --argc;
	    continue;
	}
	tmp = atof(*argv);
	if (J_DEBUG & DEBUG_J_MOVE) {
	    bu_vls_printf(gedp->ged_result_str, "joint move: %s rotate (%g %g %g) %g degrees.\n",
			  jp->name, jp->rots[i].quat[X],
			  jp->rots[i].quat[Y], jp->rots[i].quat[Z],
			  tmp);
	    bu_vls_printf(gedp->ged_result_str, "joint move: %s lower=%g, upper=%g\n",
			  jp->name, jp->rots[i].lower, jp->rots[i].upper);
	}
	if (tmp <= jp->rots[i].upper && tmp >= jp->rots[i].lower) {
	    jp->rots[i].current = tmp;
	} else {
	    bu_vls_printf(gedp->ged_result_str, "joint move: %s, rotation %d, %s out of range.\n",
			  jp->name, i, *argv);
	}
	argv++;
	argc--;
    }
    for (i=0; i<3 && argc; i++) {
	if (jp->dirs[i].upper < jp->dirs[i].lower)
	    break;
	/*
	 * eat a parameter.
	 */
	if ((*argv)[0] == '-' && (*argv)[1] == '\0') {
	    ++argv;
	    --argc;
	    continue;
	}
	tmp = atof(*argv) * gedp->ged_wdbp->dbip->dbi_local2base;
	if (tmp <= jp->dirs[i].upper &&
	    tmp >= jp->dirs[i].lower) {
	    jp->dirs[i].current = tmp;
	} else {
	    bu_vls_printf(gedp->ged_result_str, "joint move: %s, vector %d, %s out of range.\n",
			  jp->name, i, *argv);
	}
    }
    joint_adjust(gedp, jp);
    joint_mesh(gedp, 0, 0);

    /* refreshing the screen */
    if (gedp->ged_refresh_handler != GED_REFRESH_CALLBACK_PTR_NULL)
	(*gedp->ged_refresh_handler)(gedp->ged_refresh_clientdata);

    return GED_OK;
}


/**
 * Check a table for the command, check for the correct minimum and
 * maximum number of arguments, and pass control to the proper
 * function.  If the number of arguments is incorrect, print out a
 * short help message.
 */
static int
joint_cmd(struct ged *gedp,
	  int argc,
	  const char *argv[],
	  struct funtab functions[])
{
    struct funtab *ftp;

    if (argc == 0) {
	bu_vls_printf(gedp->ged_result_str, "Usage: joint {command} [command_options]\n\n");
	(void)joint_usage(gedp, argc, argv, functions);
	return GED_HELP;	/* No command entered */
    }

    for (ftp = &functions[1]; ftp->ft_name; ftp++) {
	if (!BU_STR_EQUAL(ftp->ft_name, argv[0]))
	    continue;
	/* We have a match */
	if ((ftp->ft_min <= argc) && (ftp->ft_max < 0 || argc <= ftp->ft_max)) {
	    /* Input has the right number of args.  Call function
	     * listed in table, with main(argc, argv) style args
	     */

	    switch (ftp->ft_func(gedp, argc, argv)) {
		case GED_OK:
		    return GED_OK;
		case GED_ERROR:
		    return GED_ERROR;
		default:
		    bu_vls_printf(gedp->ged_result_str, "joint_cmd: Invalid return from %s\n", ftp->ft_name);
		    return GED_ERROR;
	    }
	}

	bu_vls_printf(gedp->ged_result_str, "Usage: %s%s %s\n\t(%s)\n", functions[0].ft_name, ftp->ft_name, ftp->ft_parms, ftp->ft_comment);
	return GED_ERROR;
    }

    bu_vls_printf(gedp->ged_result_str, "%s%s : no such command, type '%s?' for help\n", functions[0].ft_name, argv[0], functions[0].ft_name);
    return GED_ERROR;
}


int
ged_joint(struct ged *gedp, int argc, const char *argv[])
{
    int status;

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* Skip the command name */
    argc--;
    argv++;

    status = joint_cmd(gedp, argc, argv, &joint_tab[0]);

    if (status == GED_OK)
	return GED_OK;

    return GED_ERROR;
}


struct funtab joint_tab[] = {
    {"joint ", "", "Joint command table",
     0, 0, 0, FALSE},
    {"?", "[commands]", "summary of available joint commands",
     joint_help_commands, 0, FUNTAB_UNLIMITED, FALSE},
    {"accept", "[joints]", "accept a series of moves",
     joint_accept, 1, FUNTAB_UNLIMITED, FALSE},
    {"debug", "[hex code]", "Show/set debugging bit vector for joints",
     joint_debug, 1, 2, FALSE},
    {"help", "[commands]", "give usage message for given joint commands",
     joint_help, 0, FUNTAB_UNLIMITED, FALSE},
    {"holds", "[names]", "list constraints",
     joint_hold, 1, FUNTAB_UNLIMITED, FALSE},
    {"list", "[names]", "list joints.",
     joint_list, 1, FUNTAB_UNLIMITED, FALSE},
    {"load", "file_name", "load a joint/constraint file",
     joint_load, 2, FUNTAB_UNLIMITED, FALSE},
    {"mesh", "", "Build the grip mesh",
     joint_mesh, 0, 1, FALSE},
    {"move", "joint_name p1 [p2...p6]", "Manual adjust a joint",
     joint_move, 3, 8, FALSE},
    {"reject", "[joint_names]", "reject joint motions",
     joint_reject, 1, FUNTAB_UNLIMITED, FALSE},
    {"save",	"file_name", "Save joints and constraints to disk",
     joint_save, 2, 2, FALSE},
    {"solve", "constraint", "Solve a or all constraints",
     joint_solve, 1, FUNTAB_UNLIMITED, FALSE},
    {"unload", "", "Unload any joint/constraints that have been loaded",
     joint_unload, 1, 1, FALSE},
    {NULL, NULL, NULL,
     NULL, 0, 0, FALSE}
};


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
