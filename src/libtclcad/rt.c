/*                              R T . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2020 United States Government as represented by
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
 *
 */
/**
 *
 * BRL-CAD's Tcl wrappers for librt.
 *
 */

#include "common.h"

#define RESOURCE_INCLUDED 1
#include <tcl.h>
#ifdef HAVE_TK
#  include <tk.h>
#endif

#include "string.h" /* for strchr */

#include "vmath.h"
#include "bu.h"
#include "bn.h"
#include "vmath.h"
#include "raytrace.h"
#include "tclcad.h"


/* Private headers */
#include "brlcad_version.h"
#include "./tclcad_private.h"


#define RT_FUNC_TCL_CAST(_func) ((int (*)(ClientData clientData, Tcl_Interp *interp, int argc, const char *const *argv))_func)

static int tclcad_rt_shootray(ClientData clientData, Tcl_Interp *interp, int argc, const char *const *argv);
static int tclcad_rt_onehit(ClientData clientData, Tcl_Interp *interp, int argc, const char *const *argv);
static int tclcad_rt_no_bool(ClientData clientData, Tcl_Interp *interp, int argc, const char *const *argv);
static int tclcad_rt_check(ClientData clientData, Tcl_Interp *interp, int argc, const char *const *argv);
static int tclcad_rt_prep(ClientData clientData, Tcl_Interp *interp, int argc, const char *const *argv);
static int tclcad_rt_set(ClientData clientData, Tcl_Interp *interp, int argc, const char *const*argv);


/************************************************************************
 *									*
 *		Tcl interface to Ray-tracing				*
 *									*
 ************************************************************************/

struct dbcmdstruct {
    char *cmdname;
    int (*cmdfunc)(ClientData clientData, Tcl_Interp *interp, int argc, const char *const *argv);
};


static struct dbcmdstruct tclcad_rt_cmds[] = {
    {"shootray",	RT_FUNC_TCL_CAST(tclcad_rt_shootray)},
    {"onehit",		RT_FUNC_TCL_CAST(tclcad_rt_onehit)},
    {"no_bool",		RT_FUNC_TCL_CAST(tclcad_rt_no_bool)},
    {"check",		RT_FUNC_TCL_CAST(tclcad_rt_check)},
    {"prep",		RT_FUNC_TCL_CAST(tclcad_rt_prep)},
    {"cutter",		RT_FUNC_TCL_CAST(tclcad_rt_cutter)},
    {"set",		RT_FUNC_TCL_CAST(tclcad_rt_set)},
    {(char *)0,		RT_FUNC_TCL_CAST(0)}
};


int
tclcad_rt_parse_ray(Tcl_Interp *interp, struct xray *rp, const char *const*argv)
{
    if (bn_decode_vect(rp->r_pt,  argv[0]) != 3) {
	Tcl_AppendResult(interp,
			 "badly formatted point: ", argv[0], (char *)NULL);
	return TCL_ERROR;
    }
    if (bn_decode_vect(rp->r_dir, argv[2]) != 3) {
	Tcl_AppendResult(interp,
			 "badly formatted vector: ", argv[2], (char *)NULL);
	return TCL_ERROR;
    }
    switch (argv[1][0]) {
	case 'd':
	    /* [2] is direction vector */
	    break;
	case 'a':
	    /* [2] is target point, build a vector from start pt */
	    VSUB2(rp->r_dir, rp->r_dir, rp->r_pt);
	    break;
	default:
	    Tcl_AppendResult(interp,
			     "wrong ray keyword: '", argv[1],
			     "', should be one of 'dir' or 'at'",
			     (char *)NULL);
	    return TCL_ERROR;
    }
    VUNITIZE(rp->r_dir);
    return TCL_OK;
}


void
tclcad_rt_pr_cutter(Tcl_Interp *interp, const union cutter *cutp)
{
    static const char xyz[4] = "XYZ";
    struct bu_vls str = BU_VLS_INIT_ZERO;
    size_t i;

    switch (cutp->cut_type) {
	case CUT_CUTNODE:
	    bu_vls_printf(&str,
			  "type cutnode axis %c point %.25G",
			  xyz[cutp->cn.cn_axis], cutp->cn.cn_point);
	    break;
	case CUT_BOXNODE:
	    bu_vls_printf(&str,
			  "type boxnode min {%.25G %.25G %.25G}",
			  V3ARGS(cutp->bn.bn_min));
	    bu_vls_printf(&str,
			  " max {%.25G %.25G %.25G}",
			  V3ARGS(cutp->bn.bn_max));
	    bu_vls_printf(&str, " solids {");
	    for (i=0; i < cutp->bn.bn_len; i++) {
		bu_vls_strcat(&str, cutp->bn.bn_list[i]->st_name);
		bu_vls_putc(&str, ' ');
	    }
	    bu_vls_printf(&str, "} pieces {");
	    for (i = 0; i < cutp->bn.bn_piecelen; i++) {
		struct rt_piecelist *plp = &cutp->bn.bn_piecelist[i];
		size_t j;
		RT_CK_PIECELIST(plp);
		/* These can be taken by user positionally */
		bu_vls_printf(&str, "{%s {", plp->stp->st_name);
		for (j=0; j < plp->npieces; j++) {
		    bu_vls_printf(&str, "%ld ", plp->pieces[j]);
		}
		bu_vls_strcat(&str, "} } ");
	    }
	    bu_vls_strcat(&str, "}");
	    break;
	default:
	    bu_vls_printf(&str, "tclcad_rt_pr_cutter() bad pointer cutp=%p",
			  (void *)cutp);
	    break;
    }
    Tcl_AppendResult(interp, bu_vls_addr(&str), (char *)NULL);
    bu_vls_free(&str);
}


/**
 * Obtain the 'n'th space partitioning cell along the given ray, and
 * return that to the user.
 *
 * Example -
 *	.rt cutter 7 {0 0 0} dir {0 0 -1}
 */
int
tclcad_rt_cutter(ClientData clientData, Tcl_Interp *interp, int argc, const char *const*argv)
{
    struct application *ap = (struct application *)clientData;
    struct rt_i *rtip;
    const union cutter *cutp;
    int n;

    if (argc != 6) {
	Tcl_AppendResult(interp,
			 "wrong # args: should be \"",
			 argv[0], " ", argv[1], "cutnum {P} dir|at {V}\"",
			 (char *)NULL);
	return TCL_ERROR;
    }

    RT_CK_APPLICATION(ap);
    rtip = ap->a_rt_i;
    RT_CK_RTI(rtip);

    n = atoi(argv[2]);
    if (tclcad_rt_parse_ray(interp, &ap->a_ray, &argv[3]) == TCL_ERROR)
	return TCL_ERROR;

    cutp = rt_cell_n_on_ray(ap, n);
    if (cutp == CUTTER_NULL) {
	Tcl_AppendResult(interp, "rt_cell_n_on_ray() failed to find cell ", argv[2], (char *)NULL);
	return TCL_ERROR;
    }
    tclcad_rt_pr_cutter(interp, cutp);
    return TCL_OK;
}


void
tclcad_rt_pr_hit(Tcl_Interp *interp, struct hit *hitp, const struct seg *segp, int flipflag)
{
    struct bu_vls str = BU_VLS_INIT_ZERO;
    vect_t norm;
    struct soltab *stp;
    const struct directory *dp;
    struct curvature crv = {{0.0, 0.0, 0.0}, 0.0, 0.0};

    RT_CK_SEG(segp);
    stp = segp->seg_stp;
    RT_CK_SOLTAB(stp);
    dp = stp->st_dp;
    RT_CK_DIR(dp);

    RT_HIT_NORMAL(norm, hitp, stp, rayp, flipflag);
    RT_CURVATURE(&crv, hitp, flipflag, stp);

    bu_vls_printf(&str, " {dist %g point {", hitp->hit_dist);
    bn_encode_vect(&str, hitp->hit_point, 1);
    bu_vls_printf(&str, "} normal {");
    bn_encode_vect(&str, norm, 1);
    bu_vls_printf(&str, "} c1 %g c2 %g pdir {",
		  crv.crv_c1, crv.crv_c2);
    bn_encode_vect(&str, crv.crv_pdir, 1);
    bu_vls_printf(&str, "} surfno %d", hitp->hit_surfno);
    if (stp->st_path.magic == DB_FULL_PATH_MAGIC) {
	/* Magic is left 0 if the path is not filled in. */
	char *sofar = db_path_to_string(&stp->st_path);
	bu_vls_printf(&str, " path ");
	bu_vls_strcat(&str, sofar);
	bu_free((void *)sofar, "path string");
    }
    bu_vls_printf(&str, " solid %s}", dp->d_namep);

    Tcl_AppendResult(interp, bu_vls_addr(&str), (char *)NULL);
    bu_vls_free(&str);
}


int
tclcad_rt_a_hit(struct application *ap,
	     struct partition *PartHeadp,
	     struct seg *UNUSED(segHeadp))
{
    Tcl_Interp *interp = (Tcl_Interp *)ap->a_uptr;
    register struct partition *pp;

    RT_CK_PT_HD(PartHeadp);

    for (pp=PartHeadp->pt_forw; pp != PartHeadp; pp = pp->pt_forw) {
	RT_CK_PT(pp);
	Tcl_AppendResult(interp, "{in", (char *)NULL);
	tclcad_rt_pr_hit(interp, pp->pt_inhit, pp->pt_inseg, pp->pt_inflip);
	Tcl_AppendResult(interp, "\nout", (char *)NULL);
	tclcad_rt_pr_hit(interp, pp->pt_outhit, pp->pt_outseg, pp->pt_outflip);
	Tcl_AppendResult(interp,
			 "\nregion ",
			 pp->pt_regionp->reg_name,
			 (char *)NULL);
	Tcl_AppendResult(interp, "}\n", (char *)NULL);
    }

    return 1;
}


int
tclcad_rt_a_miss(struct application *ap)
{
    if (ap) RT_CK_APPLICATION(ap);
    return 0;
}


/**
 * Usage -
 * procname shootray [-R] {P} dir|at {V}
 * -R option specifies no overlap reporting
 *
 * Example -
 * set glob_compat_mode 0
 *	.inmem rt_gettrees .rt all.g
 *	.rt shootray -R {0 0 0} dir {0 0 -1}
 *
 * set tgt [bu_get_value_by_keyword V [concat type [.inmem get LIGHT]]]
 *	.rt shootray {20 -13.5 20} at $tgt
 *
 *
 * Returns -
 * This "shootray" operation returns a nested set of lists. It
 * returns a list of zero or more partitions. Inside each
 * partition is a list containing an in, out, and region keyword,
 * each with an associated value. The associated value for each
 * "inhit" and "outhit" is itself a list containing a dist,
 * point, normal, surfno, and solid keyword, each with an
 * associated value.
 */
static int
tclcad_rt_shootray(ClientData clientData, Tcl_Interp *interp, int argc, const char *const *argv)
{
    struct application *ap = (struct application *)clientData;
    struct rt_i *rtip;
    int idx;

    if ((argc != 5 && argc != 6) || (argc == 6 && !BU_STR_EQUAL(argv[2], "-R"))) {
	Tcl_AppendResult(interp,
			 "wrong # args: should be \"",
			 argv[0], " ", argv[1], " [-R] {P} dir|at {V}\"",
			 (char *)NULL);
	return TCL_ERROR;
    }

    if (argc == 6) {
	ap->a_logoverlap = rt_silent_logoverlap;
	idx = 3;
    } else {
	idx = 2;
    }

    RT_CK_APPLICATION(ap);
    rtip = ap->a_rt_i;
    RT_CK_RTI(rtip);

    if (tclcad_rt_parse_ray(interp, &ap->a_ray, &argv[idx]) == TCL_ERROR)
	return TCL_ERROR;
    ap->a_hit = tclcad_rt_a_hit;
    ap->a_miss = tclcad_rt_a_miss;
    ap->a_uptr = (void *)interp;

    (void)rt_shootray(ap);

    return TCL_OK;
}


/**
 * Usage -
 * procname onehit
 * procname onehit #
 */
static int
tclcad_rt_onehit(ClientData clientData, Tcl_Interp *interp, int argc, const char *const *argv)
{
    struct application *ap = (struct application *)clientData;
    struct rt_i *rtip;
    char buf[64];

    if (argc < 2 || argc > 3) {
	Tcl_AppendResult(interp,
			 "wrong # args: should be \"",
			 argv[0], " ", argv[1], " [#]\"",
			 (char *)NULL);
	return TCL_ERROR;
    }

    RT_CK_APPLICATION(ap);
    rtip = ap->a_rt_i;
    RT_CK_RTI(rtip);

    if (argc == 3) {
	ap->a_onehit = atoi(argv[2]);
    }
    sprintf(buf, "%d", ap->a_onehit);
    Tcl_AppendResult(interp, buf, (char *)NULL);
    return TCL_OK;
}


/**
 * Usage -
 * procname no_bool
 * procname no_bool #
 */
int
tclcad_rt_no_bool(ClientData clientData, Tcl_Interp *interp, int argc, const char *const *argv)
{
    struct application *ap = (struct application *)clientData;
    struct rt_i *rtip;
    char buf[64];

    if (argc < 2 || argc > 3) {
	Tcl_AppendResult(interp,
			 "wrong # args: should be \"",
			 argv[0], " ", argv[1], " [#]\"",
			 (char *)NULL);
	return TCL_ERROR;
    }

    RT_CK_APPLICATION(ap);
    rtip = ap->a_rt_i;
    RT_CK_RTI(rtip);

    if (argc == 3) {
	ap->a_no_booleans = atoi(argv[2]);
    }
    sprintf(buf, "%d", ap->a_no_booleans);
    Tcl_AppendResult(interp, buf, (char *)NULL);
    return TCL_OK;
}


/**
 * Run some of the internal consistency checkers over the data
 * structures.
 *
 * Usage -
 * procname check
 */
int
tclcad_rt_check(ClientData clientData, Tcl_Interp *interp, int argc, const char *const *argv)
{
    struct application *ap = (struct application *)clientData;
    struct rt_i *rtip;

    if (argc != 2) {
	Tcl_AppendResult(interp,
			 "wrong # args: should be \"",
			 argv[0], " ", argv[1], "\"",
			 (char *)NULL);
	return TCL_ERROR;
    }

    RT_CK_APPLICATION(ap);
    rtip = ap->a_rt_i;
    RT_CK_RTI(rtip);

    rt_ck(rtip);

    return TCL_OK;
}


/**
 * When run with no args, just prints current status of prepping.
 *
 * Usage -
 * procname prep
 * procname prep use_air [hasty_prep]
 */
int
tclcad_rt_prep(ClientData clientData, Tcl_Interp *interp, int argc, const char *const *argv)
{
    struct application *ap = (struct application *)clientData;
    struct rt_i *rtip;
    struct bu_vls str = BU_VLS_INIT_ZERO;

    if (argc < 2 || argc > 4) {
	Tcl_AppendResult(interp,
			 "wrong # args: should be \"",
			 argv[0], " ", argv[1],
			 " [hasty_prep]\"",
			 (char *)NULL);
	return TCL_ERROR;
    }

    RT_CK_APPLICATION(ap);
    rtip = ap->a_rt_i;
    RT_CK_RTI(rtip);

    if (argc >= 3 && !rtip->needprep) {
	Tcl_AppendResult(interp,
			 argv[0], " ", argv[1],
			 " invoked when model has already been prepped.\n",
			 (char *)NULL);
	return TCL_ERROR;
    }

    if (argc == 4) rtip->rti_hasty_prep = atoi(argv[3]);

    /* If args were given, prep now. */
    if (argc >= 3) rt_prep_parallel(rtip, 1);

    /* Now, describe the current state */
    bu_vls_printf(&str, "hasty_prep %d dont_instance %d useair %d needprep %d",
		  rtip->rti_hasty_prep,
		  rtip->rti_dont_instance,
		  rtip->useair,
		  rtip->needprep
	);

    bu_vls_printf(&str, " space_partition_type %s n_cutnode %d n_boxnode %d n_empty %ld",
		  rtip->rti_space_partition == RT_PART_NUBSPT ?
		  "NUBSP" : "unknown",
		  rtip->rti_ncut_by_type[CUT_CUTNODE],
		  rtip->rti_ncut_by_type[CUT_BOXNODE],
		  rtip->nempty_cells);
    bu_vls_printf(&str, " maxdepth %d maxlen %d",
		  rtip->rti_cut_maxdepth,
		  rtip->rti_cut_maxlen);
    if (rtip->rti_ncut_by_type[CUT_BOXNODE]) bu_vls_printf(&str, " avglen %g",
							   ((double)rtip->rti_cut_totobj) /
							   rtip->rti_ncut_by_type[CUT_BOXNODE]);

    Tcl_AppendResult(interp, bu_vls_addr(&str), (char *)NULL);
    bu_vls_free(&str);
    return TCL_OK;
}


/**
 * Set/get the rt object's settable variables.
 *
 * This replaces onehit and no_bool.
 *
 * Usage -
 * procname set
 * procname set vname
 * procname set vname val
 */
int
tclcad_rt_set(ClientData clientData, Tcl_Interp *interp, int argc, const char *const *argv)
{
    struct application *ap = (struct application *)clientData;
    struct rt_i *rtip;
    struct bu_vls str = BU_VLS_INIT_ZERO;
    int val;
    const char *usage = "[vname [val]]";

    if (argc < 2 || argc > 4) {
	bu_vls_printf(&str, "%s %s: %s", argv[0], argv[1], usage);
	Tcl_AppendResult(interp, bu_vls_addr(&str), (char *)NULL);
	bu_vls_free(&str);

	return TCL_ERROR;
    }

    RT_CK_APPLICATION(ap);
    rtip = ap->a_rt_i;
    RT_CK_RTI(rtip);

    /* Return a list of the settable variables and their values */
    if (argc == 2) {
	bu_vls_printf(&str, "{onehit %d} ", ap->a_onehit);
	bu_vls_printf(&str, "{no_bool %d} ", ap->a_no_booleans);
	bu_vls_printf(&str, "{bot_reverse_normal_disabled %d}", ap->a_bot_reverse_normal_disabled);
	Tcl_AppendResult(interp, bu_vls_addr(&str), (char *)NULL);
	bu_vls_free(&str);

	return TCL_OK;
    }

    if (argc == 4 && sscanf(argv[3], "%d", &val) != 1) {
	bu_vls_printf(&str, "%s %s: bad val - %s, must be an integer", argv[0], argv[1], argv[3]);
	Tcl_AppendResult(interp, bu_vls_addr(&str), (char *)NULL);
	bu_vls_free(&str);

	return TCL_ERROR;
    }

    if (argv[2][0] == 'o' && !bu_strncmp(argv[2], "onehit", 6)) {
	if (argc == 3)
	    val = ap->a_onehit;
	else
	    ap->a_onehit = val;
    } else if (argv[2][0] == 'n' && !bu_strncmp(argv[2], "no_bool", 7)) {
	if (argc == 3)
	    val = ap->a_no_booleans;
	else
	    ap->a_no_booleans = val;
    } else if (argv[2][0] == 'b' && !bu_strncmp(argv[2], "bot_reverse_normal_disabled", 27)) {
	if (argc == 3)
	    val = ap->a_bot_reverse_normal_disabled;
	else
	    ap->a_bot_reverse_normal_disabled = val;
    } else {
	bu_vls_printf(&str, "%s %s: bad val - %s, must be one of the following: onehit, no_bool, or bot_reverse_normal_disabled",
		      argv[0], argv[1], argv[2]);
	Tcl_AppendResult(interp, bu_vls_addr(&str), (char *)NULL);
	bu_vls_free(&str);

	return TCL_ERROR;
    }

    bu_vls_printf(&str, "%d", val);
    Tcl_AppendResult(interp, bu_vls_addr(&str), (char *)NULL);
    bu_vls_free(&str);
    return TCL_OK;
}


int
tclcad_rt(ClientData clientData, Tcl_Interp *interp, int argc, const char **argv)
{
    struct dbcmdstruct *dbcmd;

    if (argc < 2) {
	Tcl_AppendResult(interp,
			 "wrong # args: should be \"", argv[0],
			 " command [args...]\"",
			 (char *)NULL);
	return TCL_ERROR;
    }

    for (dbcmd = tclcad_rt_cmds; dbcmd->cmdname != NULL; dbcmd++) {
	if (BU_STR_EQUAL(dbcmd->cmdname, argv[1])) {
	    /* need proper cmd func pointer for actual call */
	    int (*_cmdfunc)(void*, Tcl_Interp*, int, const char* const*);
	    /* cast to the actual caller */
	    _cmdfunc = (int (*)(void*, Tcl_Interp*, int, const char* const*))(*dbcmd->cmdfunc);
	    return _cmdfunc(clientData, interp, argc, argv);
	}
    }


    Tcl_AppendResult(interp, "unknown LIBRT command '",
		     argv[1], "'; must be one of:",
		     (char *)NULL);
    for (dbcmd = tclcad_rt_cmds; dbcmd->cmdname != NULL; dbcmd++) {
	Tcl_AppendResult(interp, " ", dbcmd->cmdname, (char *)NULL);
    }
    return TCL_ERROR;
}


/************************************************************************
 *									*
 *		Tcl interface to Combination management			*
 *									*
 ************************************************************************/

int
tclcad_rt_import_from_path(Tcl_Interp *interp, struct rt_db_internal *ip, const char *path, struct rt_wdb *wdb)
{
    struct db_i *dbip;
    int status;

    /* Can't run RT_CK_DB_INTERNAL(ip), it hasn't been filled in yet */
    RT_CK_WDB(wdb);
    dbip = wdb->dbip;
    RT_CK_DBI(dbip);

    if (strchr(path, '/')) {
	/* This is a path */
	struct db_tree_state ts;
	struct db_full_path old_path;
	struct db_full_path new_path;
	struct directory *dp_curr;
	int ret;

	db_init_db_tree_state(&ts, dbip, &rt_uniresource);
	db_full_path_init(&old_path);
	db_full_path_init(&new_path);

	if (db_string_to_path(&new_path, dbip, path) < 0) {
	    Tcl_AppendResult(interp, "tclcad_rt_import_from_path: '",
			     path, "' contains unknown object names\n", (char *)NULL);
	    return TCL_ERROR;
	}

	dp_curr = DB_FULL_PATH_CUR_DIR(&new_path);
	if (!dp_curr)
	    return TCL_ERROR;

	ret = db_follow_path(&ts, &old_path, &new_path, LOOKUP_NOISY, 0);
	db_free_full_path(&old_path);
	db_free_full_path(&new_path);

	if (ret < 0) {
	    Tcl_AppendResult(interp, "tclcad_rt_import_from_path: '",
			     path, "' is a bad path\n", (char *)NULL);
	    return TCL_ERROR;
	}

	status = wdb_import(wdb, ip, dp_curr->d_namep, ts.ts_mat);
	if (status == -4) {
	    Tcl_AppendResult(interp, dp_curr->d_namep,
			     " not found in path ", path, "\n",
			     (char *)NULL);
	    return TCL_ERROR;
	}
	if (status < 0) {
	    Tcl_AppendResult(interp, "wdb_import failure: ",
			     dp_curr->d_namep, (char *)NULL);
	    return TCL_ERROR;
	}
    } else {
	status = wdb_import(wdb, ip, path, (matp_t)NULL);
	if (status == -4) {
	    Tcl_AppendResult(interp, path, ": not found\n",
			     (char *)NULL);
	    return TCL_ERROR;
	}
	if (status < 0) {
	    Tcl_AppendResult(interp, "wdb_import failure: ",
			     path, (char *)NULL);
	    return TCL_ERROR;
	}
    }
    return TCL_OK;
}


void
tclcad_rt_setup(Tcl_Interp *interp)
{
    Tcl_LinkVar(interp, "rt_bot_minpieces", (char *)&rt_bot_minpieces, TCL_LINK_WIDE_INT);

    Tcl_LinkVar(interp, "rt_bot_tri_per_piece",
		(char *)&rt_bot_tri_per_piece, TCL_LINK_WIDE_INT);
}


int
Rt_Init(Tcl_Interp *interp)
{
    /*XXX how much will this break? */
    if (!BU_LIST_IS_INITIALIZED(&RTG.rtg_vlfree)) {
	if (bu_avail_cpus() > 1) {
	    RTG.rtg_parallel = 1;
	}

	/* initialize RT's global state */
	BU_LIST_INIT(&RTG.rtg_vlfree);
	BU_LIST_INIT(&RTG.rtg_headwdb.l);
	if (rt_uniresource.re_magic != RESOURCE_MAGIC) {
	    rt_init_resource(&rt_uniresource, 0, NULL);
	}
    }

    tclcad_rt_setup(interp);

    Tcl_PkgProvide(interp,  "Rt", brlcad_version());

    return TCL_OK;
}


/* ====================================================================== */

/* TCL-oriented C support for LIBRT */


void
db_full_path_appendresult(Tcl_Interp *interp, const struct db_full_path *pp)
{
    size_t i;

    RT_CK_FULL_PATH(pp);

    for (i=0; i<pp->fp_len; i++) {
	Tcl_AppendResult(interp, "/", pp->fp_names[i]->d_namep, (char *)NULL);
    }
}


int
tcl_obj_to_int_array(Tcl_Interp *interp, Tcl_Obj *list, int **array, int *array_len)
{
    Tcl_Obj **obj_array;
    int len, i;

    if (Tcl_ListObjGetElements(interp, list, &len, &obj_array) != TCL_OK)
	return 0;

    if (len < 1)
	return 0;

    if (*array_len < 1) {
	*array = (int *)bu_calloc(len, sizeof(int), "array");
	*array_len = len;
    }

    for (i=0; i<len && i<*array_len; i++) {
	(*array)[i] = atoi(Tcl_GetStringFromObj(obj_array[i], NULL));
	Tcl_DecrRefCount(obj_array[i]);
    }

    return len < *array_len ? len : *array_len;
}


int
tcl_list_to_int_array(Tcl_Interp *interp, char *char_list, int **array, int *array_len)
{
    Tcl_Obj *obj;
    int ret;

    obj = Tcl_NewStringObj(char_list, -1);

    ret = tcl_obj_to_int_array(interp, obj, array, array_len);

    return ret;
}


int
tcl_obj_to_fastf_array(Tcl_Interp *interp, Tcl_Obj *list, fastf_t **array, int *array_len)
{
    Tcl_Obj **obj_array;
    int len, i;
    int ret;

    if ((ret=Tcl_ListObjGetElements(interp, list, &len, &obj_array)) != TCL_OK)
	return ret;

    if (len < 1)
	return 0;

    if (*array_len < 1) {
	*array = (fastf_t *)bu_calloc(len, sizeof(fastf_t), "array");
	*array_len = len;
    }

    for (i=0; i<len && i<*array_len; i++) {
	(*array)[i] = atof(Tcl_GetStringFromObj(obj_array[i], NULL));
	Tcl_DecrRefCount(obj_array[i]);
    }

    return len < *array_len ? len : *array_len;
}


int
tcl_list_to_fastf_array(Tcl_Interp *interp, const char *char_list, fastf_t **array, int *array_len)
{
    Tcl_Obj *obj;
    int ret;

    obj = Tcl_NewStringObj(char_list, -1);

    ret = tcl_obj_to_fastf_array(interp, obj, array, array_len);

    return ret;
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
