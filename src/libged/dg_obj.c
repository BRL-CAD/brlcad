/*                        D G _ O B J . C
 * BRL-CAD
 *
 * Copyright (c) 1997-2013 United States Government as represented by
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
/** @addtogroup libged */
/** @{ */
/** @file libged/dg_obj.c
 *
 * A drawable geometry object contains methods and attributes for
 * preparing geometry that is ready (i.e. vlists) for display.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <errno.h>

#ifdef HAVE_SYS_TYPES_H
#   include <sys/types.h>
#endif

#ifdef HAVE_SYS_WAIT_H
#   include <sys/wait.h>
#endif

#include "bio.h"

#include "tcl.h"
#include "bu.h"
#include "bn.h"
#include "cmd.h"
#include "vmath.h"
#include "mater.h"
#include "solid.h"
#include "plot3.h"
#include "dg.h"

#include "./ged_private.h"


#define DGO_CHECK_WDBP_NULL(_dgop, _interp)				\
    if (_dgop->dgo_wdbp == RT_WDB_NULL)	{				\
	Tcl_AppendResult(_interp, "Not associated with a database!\n", (char *)NULL); \
	return TCL_ERROR;						\
    }


struct dg_client_data {
    struct dg_obj *dgop;
    Tcl_Interp *interp;
    int wireframe_color_override;
    int wireframe_color[3];
    int draw_nmg_only;
    int nmg_triangulate;
    int draw_wireframes;
    int draw_normals;
    int draw_solid_lines_only;
    int draw_no_surfaces;
    int shade_per_vertex_normals;
    int draw_edge_uses;
    int fastpath_count;			/* statistics */
    int do_not_draw_nmg_solids_during_debugging;
    struct bn_vlblock *draw_edge_uses_vbp;
    int shaded_mode_override;
    fastf_t transparency;
    int dmode;
};


struct dg_rt_client_data {
    struct run_rt *rrtp;
    struct dg_obj *dgop;
    Tcl_Interp *interp;
};


#define DGO_WIREFRAME 0
#define DGO_SHADED_MODE_BOTS 1
#define DGO_SHADED_MODE_ALL 2
#define DGO_BOOL_EVAL 3


static int dgo_drawtrees(struct dg_obj *dgop, int argc, const char **argv, int kind, struct dg_client_data *_dgcdp);

/* declared in qray.c */
extern void dgo_init_qray(struct dg_obj *dgop);
extern void dgo_free_qray(struct dg_obj *dgop);

/* in wdb_obj.c */
void wdb_print_node(struct rt_wdb *wdbp, struct directory *dp, int pathpos, int indentSize, char prefix, int cflag, int displayDepth, int currdisplayDepth);

/* in view_obj.c */
extern struct view_obj HeadViewObj;		/**< @brief  head of view object list */
extern void vo_update(struct view_obj *vop, int oflag);


/**
 * count the total number of solids, phony or otherwise
 */
size_t
dgo_count_tops(const struct solid *headsp)
{
    struct solid *sp;
    size_t count = 0;
    FOR_ALL_SOLIDS(sp, &headsp->l) {
	count++;
    }
    return count;
}


/*
 * D G O _ B U I L D _ T O P S
 *
 * Build a command line vector of the tops of all objects in view.
 */
int
dgo_build_tops(struct solid *hsp,
	       const char **start,
	       const char **end)
{
    const char **vp = start;
    struct solid *sp;

    /*
     * Find all unique top-level entries.
     * Mark ones already done with s_flag == UP
     */
    FOR_ALL_SOLIDS(sp, &hsp->l)
	sp->s_flag = DOWN;
    FOR_ALL_SOLIDS(sp, &hsp->l) {
	struct solid *forw;
	struct directory *dp = FIRST_SOLID(sp);

	if (sp->s_flag == UP)
	    continue;
	if (dp->d_addr == RT_DIR_PHONY_ADDR)
	    continue;	/* Ignore overlays, predictor, etc. */
	if (vp < end)
	    *vp++ = dp->d_namep;
	else {
	    bu_log("INTERNAL ERROR: ran out of command vector space at %s\n", dp->d_namep);
	    break;
	}
	sp->s_flag = UP;
	for (BU_LIST_PFOR (forw, sp, solid, &hsp->l)) {
	    if (FIRST_SOLID(forw) == dp)
		forw->s_flag = UP;
	}
    }
    *vp = (const char *)NULL;
    return vp-start;
}


/*
 * Usage:
 * procname vdraw cmd arg(s)
 */
static int
dgo_vdraw_tcl(void *clientData,
	      int argc,
	      const char **argv)
{
    struct dg_obj *dgop = (struct dg_obj *)clientData;
    Tcl_Interp *interp = dgop->interp;

    DGO_CHECK_WDBP_NULL(dgop, interp);

    return vdraw_cmd_tcl(dgop, argc-1, argv+1);
}


/*
 * Create an command/object named "oname" in "interp".
 */
struct dg_obj *
dgo_open_cmd(const char *oname,
	     struct rt_wdb *wdbp)
{
    struct dg_obj *dgop;

    BU_ALLOC(dgop, struct dg_obj);

    /* initialize dg_obj */
    bu_vls_init(&dgop->dgo_name);
    bu_vls_strcpy(&dgop->dgo_name, oname);
    dgop->dgo_wdbp = wdbp;
    BU_LIST_INIT(&dgop->dgo_headSolid);
    BU_LIST_INIT(&dgop->dgo_headVDraw);
    BU_LIST_INIT(&dgop->dgo_observers.l);
    BU_LIST_INIT(&dgop->dgo_headRunRt.l);
    dgop->dgo_freeSolids = &_FreeSolid;
    dgop->dgo_uplotOutputMode = PL_OUTPUT_MODE_BINARY;

    dgo_init_qray(dgop);

    /* append to list of dg_obj's */
    BU_LIST_APPEND(&HeadDGObj.l, &dgop->l);

    return dgop;
}


/****************** Drawable Geometry Object Methods ********************/

/*
 *
 * Usage:
 * procname headSolid
 *
 * Returns: database object's headSolid.
 */
static int
dgo_headSolid_tcl(void *clientData,
		  int argc,
		  const char **argv)
{
    struct dg_obj *dgop = (struct dg_obj *)clientData;
    Tcl_Interp *interp = dgop->interp;
    struct bu_vls vls = BU_VLS_INIT_ZERO;

    if (argc != 2) {
	bu_vls_printf(&vls, "helplib_alias dgo_headSolid %s", argv[0]);
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    bu_vls_printf(&vls, "%p", (void *)(&dgop->dgo_headSolid));
    Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
    bu_vls_free(&vls);
    return TCL_OK;
}


int
dgo_illum_cmd(struct dg_obj *dgop,
	      int argc,
	      const char **argv)
{
    struct solid *sp;
    struct bu_vls vls = BU_VLS_INIT_ZERO;
    int found = 0;
    int illum = 1;

    if (argc == 3) {
	if (argv[1][0] == '-' && argv[1][1] == 'n')
	    illum = 0;
	else
	    goto bad;

	--argc;
	++argv;
    }

    if (argc != 2)
	goto bad;

    FOR_ALL_SOLIDS(sp, &dgop->dgo_headSolid) {
	size_t i;

	for (i = 0; i < sp->s_fullpath.fp_len; ++i) {
	    if (*argv[1] == *DB_FULL_PATH_GET(&sp->s_fullpath, i)->d_namep &&
		BU_STR_EQUAL(argv[1], DB_FULL_PATH_GET(&sp->s_fullpath, i)->d_namep)) {
		found = 1;
		if (illum)
		    sp->s_iflag = UP;
		else
		    sp->s_iflag = DOWN;
	    }
	}
    }

    if (!found) {
	bu_log("illum: %s not found", argv[1]);
	return TCL_ERROR;
    }

    return TCL_OK;

bad:
    bu_vls_printf(&vls, "helplib_alias dgo_illum %s", argv[0]);
    Tcl_Eval(dgop->interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
}


void
dgo_notify(struct dg_obj *dgop)
{
    bu_observer_notify(dgop->interp, &dgop->dgo_observers, bu_vls_addr(&dgop->dgo_name));
}


/*
 * Illuminate/highlight database object
 *
 * Usage:
 * procname illum [-n] obj
 *
 */
static int
dgo_illum_tcl(void *clientData,
	      int argc,
	      const char **argv)
{
    struct dg_obj *dgop = (struct dg_obj *)clientData;
    Tcl_Interp *interp = dgop->interp;
    int ret;

    DGO_CHECK_WDBP_NULL(dgop, interp);

    if ((ret = dgo_illum_cmd(dgop, argc-1, argv+1)) == TCL_OK)
	dgo_notify(dgop);

    return ret;
}


int
dgo_label_cmd(struct dg_obj *dgop,
	      int argc,
	      const char **argv)
{
    /* not yet implemented */
    if (!dgop)
	return TCL_ERROR;

    while (argc-- > 1)
	bu_log("Unexpected argument: %s", argv[argc]);

    return TCL_OK;
}


/*
 * Label database objects.
 *
 * Usage:
 * procname label [-n] obj
 *
 */
static int
dgo_label_tcl(void *clientData, int argc, const char **argv)
{
    struct dg_obj *dgop = (struct dg_obj *)clientData;
    Tcl_Interp *interp = dgop->interp;

    DGO_CHECK_WDBP_NULL(dgop, interp);

    return dgo_label_cmd(dgop, argc-1, argv+1);
}


/*
 * E R A S E O B J A L L
 *
 * This routine goes through the solid table and deletes all solids
 * from the solid list which contain the specified object anywhere in their 'path'
 */
static void
dgo_eraseobjall(struct dg_obj *dgop,
		struct directory **dpp)
{
    struct directory **tmp_dpp;
    struct solid *sp;
    struct solid *nsp;
    struct db_full_path subpath;

    if (dgop->dgo_wdbp->dbip == DBI_NULL)
	return;

    if (*dpp == RT_DIR_NULL)
	return;

    db_full_path_init(&subpath);
    for (tmp_dpp = dpp; *tmp_dpp != RT_DIR_NULL; ++tmp_dpp) {
	RT_CK_DIR(*tmp_dpp);
	db_add_node_to_full_path(&subpath, *tmp_dpp);
    }

    sp = BU_LIST_NEXT(solid, &dgop->dgo_headSolid);
    while (BU_LIST_NOT_HEAD(sp, &dgop->dgo_headSolid)) {
	nsp = BU_LIST_PNEXT(solid, sp);
	if (db_full_path_subset(&sp->s_fullpath, &subpath, 0)) {
	    BU_LIST_DEQUEUE(&sp->l);
	    FREE_SOLID(sp, &_FreeSolid.l);
	}
	sp = nsp;
    }

    if ((*dpp)->d_addr == RT_DIR_PHONY_ADDR) {
	if (db_dirdelete(dgop->dgo_wdbp->dbip, *dpp) < 0) {
	    bu_log("dgo_eraseobjall: db_dirdelete failed\n");
	}
    }
    db_free_full_path(&subpath);
}


/*
 * This looks for a drawable geometry object that has a matching "dbip"
 * and deletes the solids corresponding to "dp" from the solid list.
 * At the moment this is being called from wdb_obj.c/wdb_kill_tcl() if the
 * object is not phony.
 */
void
dgo_eraseobjall_callback(struct db_i *dbip,
			 struct directory *dp,
			 int notify)
{
    struct dg_obj *dgop;
    struct directory *dpp[2] = {RT_DIR_NULL, RT_DIR_NULL};

    dpp[0] = dp;
    for (BU_LIST_FOR (dgop, dg_obj, &HeadDGObj.l))
	/* drawable geometry objects associated database matches */
	if (dgop->dgo_wdbp->dbip == dbip) {
	    dgo_eraseobjall(dgop, dpp);

	    if (notify)
		dgo_notify(dgop);
	}
}


/*
 * E R A S E O B J
 *
 * This routine goes through the solid table and deletes all solids
 * from the solid list which contain the specified object at the
 * beginning of their 'path'
 */
static void
dgo_eraseobj(struct dg_obj *dgop,
	     struct directory **dpp)
{
    /*XXX
     * Temporarily put back the old behavior (as seen in Brlcad5.3),
     * as the behavior after the #else is identical to dgo_eraseobjall.
     */
    struct directory **tmp_dpp;
    struct solid *sp;
    struct solid *nsp;
    size_t i;

    if (dgop->dgo_wdbp->dbip == DBI_NULL)
	return;

    if (*dpp == RT_DIR_NULL)
	return;

    for (tmp_dpp = dpp; *tmp_dpp != RT_DIR_NULL; ++tmp_dpp)
	RT_CK_DIR(*tmp_dpp);

    sp = BU_LIST_FIRST(solid, &dgop->dgo_headSolid);
    while (BU_LIST_NOT_HEAD(sp, &dgop->dgo_headSolid)) {
	nsp = BU_LIST_PNEXT(solid, sp);
	for (i = 0, tmp_dpp = dpp;
	     i < sp->s_fullpath.fp_len && *tmp_dpp != RT_DIR_NULL;
	     ++i, ++tmp_dpp)
	    if (sp->s_fullpath.fp_names[i] != *tmp_dpp)
		goto end;

	if (*tmp_dpp != RT_DIR_NULL)
	    goto end;

	BU_LIST_DEQUEUE(&sp->l);
	FREE_SOLID(sp, &_FreeSolid.l);
    end:
	sp = nsp;
    }

    if ((*dpp)->d_addr == RT_DIR_PHONY_ADDR) {
	if (db_dirdelete(dgop->dgo_wdbp->dbip, *dpp) < 0) {
	    bu_log("dgo_eraseobj: db_dirdelete failed\n");
	}
    }
}


/*
 * Builds an array of directory pointers from argv and calls
 * either dgo_eraseobj or dgo_eraseobjall.
 */
void
dgo_eraseobjpath(struct dg_obj *dgop,
		 int argc,
		 const char **argv,
		 int noisy,
		 int all)
{
    struct directory *dp;
    int i;
    struct bu_vls vls = BU_VLS_INIT_ZERO;

    for (i = 0; i < argc; i++) {
	int j;
	char *list;
	int ac;
	char **av, **av_orig;
	struct directory **dpp = (struct directory **)0;

	{
	    char *begin;
	    char *end;
	    char *newstr = strdup(argv[i]);

	    begin = newstr;
	    bu_vls_trunc(&vls, 0);

	    while ((end = strchr(begin, '/')) != NULL) {
		*end = '\0';
		bu_vls_printf(&vls, "%s ", begin);
		begin = end + 1;
	    }
	    bu_vls_printf(&vls, "%s ", begin);
	    free((void *)newstr);
	}
	list = bu_vls_addr(&vls);

	if (Tcl_SplitList(dgop->interp, list, &ac, (const char ***)&av_orig) != TCL_OK)
	    continue;

	/* make sure we will not dereference null */
	if ((ac == 0) || (av_orig == 0) || (*av_orig == 0)) {
	    bu_log("WARNING: Asked to look up a null-named database object\n");
	    goto end;
	}

	/* skip first element if empty */
	av = av_orig;

	if (*av[0] == '\0') {
	    --ac;
	    ++av;
	}

	/* ignore last element if empty */
	if (*av[ac-1] == '\0')
	    --ac;

	dpp = bu_calloc(ac+1, sizeof(struct directory *), "eraseobjpath: directory pointers");
	for (j = 0; j < ac; ++j)
	    if ((dp = db_lookup(dgop->dgo_wdbp->dbip, av[j], noisy)) != RT_DIR_NULL)
		dpp[j] = dp;
	    else
		goto end;

	dpp[j] = RT_DIR_NULL;

	if (all)
	    dgo_eraseobjall(dgop, dpp);
	else
	    dgo_eraseobj(dgop, dpp);

    end:
	bu_free((genptr_t)dpp, "eraseobjpath: directory pointers");
	Tcl_Free((char *)av_orig);
    }
    bu_vls_free(&vls);

}


/*
 * C O L O R _ S O L T A B
 *
 * Pass through the solid table and set pointer to appropriate
 * mater structure.
 */
void
dgo_color_soltab(struct solid *hsp)
{
    struct solid *sp;
    const struct mater *mp;

    FOR_ALL_SOLIDS(sp, &hsp->l) {
	sp->s_cflag = 0;

	/* the user specified the color, so use it */
	if (sp->s_uflag) {
	    sp->s_color[0] = sp->s_basecolor[0];
	    sp->s_color[1] = sp->s_basecolor[1];
	    sp->s_color[2] = sp->s_basecolor[2];
	    continue;
	}

	for (mp = rt_material_head(); mp != MATER_NULL; mp = mp->mt_forw) {
	    if (sp->s_regionid <= mp->mt_high &&
		sp->s_regionid >= mp->mt_low) {
		sp->s_color[0] = mp->mt_r;
		sp->s_color[1] = mp->mt_g;
		sp->s_color[2] = mp->mt_b;
		goto done;
	    }
	}

	/*
	 * There is no region-id-based coloring entry in the
	 * table, so use the combination-record ("mater"
	 * command) based color if one was provided. Otherwise,
	 * use the default wireframe color.
	 * This is the "new way" of coloring things.
	 */

	/* use wireframe_default_color */
	if (sp->s_dflag)
	    sp->s_cflag = 1;
	/* Be conservative and copy color anyway, to avoid black */
	sp->s_color[0] = sp->s_basecolor[0];
	sp->s_color[1] = sp->s_basecolor[1];
	sp->s_color[2] = sp->s_basecolor[2];
    done: ;
    }
}


int
dgo_draw_cmd(struct dg_obj *dgop,
	     int argc,
	     const char **argv,
	     int kind)
{
    if (argc < 2) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	switch (kind) {
	    default:
	    case 1:
		bu_vls_printf(&vls, "helplib_alias dgo_draw %s", argv[0]);
		break;
	    case 2:
		bu_vls_printf(&vls, "helplib_alias dgo_E %s", argv[0]);
		break;
	    case 3:
		bu_vls_printf(&vls, "helplib_alias dgo_ev %s", argv[0]);
		break;
	}

	Tcl_Eval(dgop->interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);

	return TCL_ERROR;
    }

    /* skip past cmd */
    --argc;
    ++argv;

    /* First, delete any mention of these objects.
     * Silently skip any leading options (which start with minus signs).
     */
    dgo_eraseobjpath(dgop, argc, argv, LOOKUP_QUIET, 0);

    dgo_drawtrees(dgop, argc, argv, kind, (struct dg_client_data *)0);

    dgo_color_soltab((struct solid *)&dgop->dgo_headSolid);

    return TCL_OK;
}


/*
 * Prepare database objects for drawing.
 *
 * Usage:
 * procname draw|ev [args]
 *
 */
static int
dgo_draw_tcl(void *clientData, int argc, const char **argv)
{
    struct dg_obj *dgop = (struct dg_obj *)clientData;
    int ret;

    if ((ret = dgo_draw_cmd(dgop, argc-1, argv+1, 1)) == TCL_OK)
	dgo_notify(dgop);

    return ret;
}


/*
 *
 * Evaluated Edit something (add to visible display)
 *
 * Usage:
 * Usage: E object(s)
 */
int
dgo_E_tcl(void *clientData,
	  int argc,
	  const char **argv)
{
    struct dg_obj *dgop = (struct dg_obj *)clientData;

    return dgo_E_cmd(dgop, argc-1, argv+1);
}


/*
 * Prepare database objects for drawing.
 *
 * Usage:
 * procname ev [args]
 *
 */
static int
dgo_ev_tcl(void *clientData,
	   int argc,
	   const char **argv)
{
    struct dg_obj *dgop = (struct dg_obj *)clientData;
    Tcl_Interp *interp = dgop->interp;
    int ret;

    DGO_CHECK_WDBP_NULL(dgop, interp);

    if ((ret = dgo_draw_cmd(dgop, argc-1, argv+1, 3)) == TCL_OK)
	dgo_notify(dgop);

    return ret;
}


int
dgo_erase_cmd(struct dg_obj *dgop,
	      int argc,
	      const char **argv)
{

    if (argc < 2) {
	bu_log("ERROR: expecting only one argument\n");
	return TCL_ERROR;
    }

    dgo_eraseobjpath(dgop, argc-1, argv+1, LOOKUP_NOISY, 0);

    return TCL_OK;
}


/*
 * Erase database objects.
 *
 * Usage:
 * procname erase object(s)
 *
 */
static int
dgo_erase_tcl(void *clientData, int argc, const char **argv)
{
    struct dg_obj *dgop = (struct dg_obj *)clientData;
    Tcl_Interp *interp = dgop->interp;
    int ret;

    DGO_CHECK_WDBP_NULL(dgop, interp);

    if ((ret = dgo_erase_cmd(dgop, argc-1, argv+1)) == TCL_OK)
	dgo_notify(dgop);

    return ret;
}


int
dgo_erase_all_cmd(struct dg_obj *dgop,
		  int argc,
		  const char **argv)
{
    if (argc < 2) {
	bu_log("ERROR: expecting only one argument\n");
	return TCL_ERROR;
    }

    dgo_eraseobjpath(dgop, argc-1, argv+1, LOOKUP_NOISY, 1);

    return TCL_OK;
}


/*
 * Usage:
 * procname erase_all object(s)
 */
static int
dgo_erase_all_tcl(void *clientData, int argc, const char **argv)
{
    struct dg_obj *dgop = (struct dg_obj *)clientData;
    Tcl_Interp *interp = dgop->interp;
    int ret;

    DGO_CHECK_WDBP_NULL(dgop, interp);

    if ((ret = dgo_erase_all_cmd(dgop, argc-1, argv+1)) == TCL_OK)
	dgo_notify(dgop);

    return ret;
}


struct directory **
dgo_build_dpp(struct dg_obj *dgop,
	      const char *path) {
    struct directory *dp;
    struct directory **dpp;
    int i;
    char *begin;
    char *end;
    char *newstr;
    char *list;
    int ac;
    const char **av;
    const char **av_orig = NULL;
    struct bu_vls vls = BU_VLS_INIT_ZERO;

    /*
     * First, build an array of the object's path components.
     * We store the list in av_orig below.
     */
    newstr = strdup(path);
    begin = newstr;
    while ((end = strchr(begin, '/')) != NULL) {
	*end = '\0';
	bu_vls_printf(&vls, "%s ", begin);
	begin = end + 1;
    }
    bu_vls_printf(&vls, "%s ", begin);
    free((void *)newstr);

    list = bu_vls_addr(&vls);

    if (Tcl_SplitList(dgop->interp, list, &ac, &av_orig) != TCL_OK) {
	Tcl_AppendResult(dgop->interp, "-1", (char *)NULL);
	bu_vls_free(&vls);
	return (struct directory **)NULL;
    }

    /* skip first element if empty */
    av = av_orig;
    if (*av[0] == '\0') {
	--ac;
	++av;
    }

    /* ignore last element if empty */
    if (*av[ac-1] == '\0')
	--ac;

    /*
     * Next, we build an array of directory pointers that
     * correspond to the object's path.
     */
    dpp = bu_calloc(ac+1, sizeof(struct directory *), "dgo_build_dpp: directory pointers");
    for (i = 0; i < ac; ++i) {
	if ((dp = db_lookup(dgop->dgo_wdbp->dbip, av[i], 0)) != RT_DIR_NULL)
	    dpp[i] = dp;
	else {
	    /* object is not currently being displayed */
	    Tcl_AppendResult(dgop->interp, "-1", (char *)NULL);

	    bu_free((genptr_t)dpp, "dgo_build_dpp: directory pointers");
	    Tcl_Free((char *)av_orig);
	    bu_vls_free(&vls);
	    return (struct directory **)NULL;
	}
    }

    dpp[i] = RT_DIR_NULL;

    Tcl_Free((char *)av_orig);
    bu_vls_free(&vls);
    return dpp;
}


int
dgo_how_cmd(struct dg_obj *dgop,
	    int argc,
	    const char **argv)
{
    struct solid *sp;
    struct bu_vls vls = BU_VLS_INIT_ZERO;
    size_t i;
    struct directory **dpp;
    struct directory **tmp_dpp;
    int both = 0;

    if (argc < 2 || 3 < argc) {
	bu_vls_printf(&vls, "helplib_alias dgo_how %s", argv[0]);
	Tcl_Eval(dgop->interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    if (argc == 3 &&
	argv[1][0] == '-' &&
	argv[1][1] == 'b') {
	both = 1;

	if ((dpp = dgo_build_dpp(dgop, argv[2])) == NULL)
	    goto good;
    } else {
	if ((dpp = dgo_build_dpp(dgop, argv[1])) == NULL)
	    goto good;
    }

    FOR_ALL_SOLIDS(sp, &dgop->dgo_headSolid) {
	for (i = 0, tmp_dpp = dpp;
	     i < sp->s_fullpath.fp_len && *tmp_dpp != RT_DIR_NULL;
	     ++i, ++tmp_dpp) {
	    if (sp->s_fullpath.fp_names[i] != *tmp_dpp)
		break;
	}

	if (*tmp_dpp != RT_DIR_NULL)
	    continue;

	/* found a match */
	if (both)
	    bu_vls_printf(&vls, "%d %g", sp->s_dmode, sp->s_transparency);
	else
	    bu_vls_printf(&vls, "%d", sp->s_dmode);

	Tcl_AppendResult(dgop->interp, bu_vls_addr(&vls), (char *)NULL);
	goto good;
    }

    /* match NOT found */
    Tcl_AppendResult(dgop->interp, "-1", (char *)NULL);

good:
    if (dpp != (struct directory **)NULL)
	bu_free((genptr_t)dpp, "dgo_how_cmd: directory pointers");
    bu_vls_free(&vls);

    return TCL_OK;
}


/*
 * Returns "how" an object is being displayed.
 *
 * Usage:
 * procname how obj
 */
static int
dgo_how_tcl(void *clientData, int argc, const char **argv)
{
    struct dg_obj *dgop = (struct dg_obj *)clientData;
    Tcl_Interp *interp = dgop->interp;

    DGO_CHECK_WDBP_NULL(dgop, interp);
    return dgo_how_cmd(dgop, argc-1, argv+1);
}


int
dgo_who_cmd(struct dg_obj *dgop,
	    int argc,
	    const char **argv)
{
    struct solid *sp;
    int skip_real, skip_phony;

    if (argc < 1 || 2 < argc) {
	bu_log("ERROR: expecting no more than two arguments\n");
    }

    skip_real = 0;
    skip_phony = 1;
    if (argc == 2) {
	switch (argv[1][0]) {
	    case 'b':
		skip_real = 0;
		skip_phony = 0;
		break;
	    case 'p':
		skip_real = 1;
		skip_phony = 0;
		break;
	    case 'r':
		skip_real = 0;
		skip_phony = 1;
		break;
	    default:
		bu_log("dgo_who: argument not understood\n");
		return TCL_ERROR;
	}
    }


    /* Find all unique top-level entries.
     * Mark ones already done with s_flag == UP
     */
    FOR_ALL_SOLIDS(sp, &dgop->dgo_headSolid)
	sp->s_flag = DOWN;
    FOR_ALL_SOLIDS(sp, &dgop->dgo_headSolid) {
	struct solid *forw;	/* XXX */

	if (sp->s_flag == UP)
	    continue;
	if (FIRST_SOLID(sp)->d_addr == RT_DIR_PHONY_ADDR) {
	    if (skip_phony) continue;
	} else {
	    if (skip_real) continue;
	}
	Tcl_AppendResult(dgop->interp, FIRST_SOLID(sp)->d_namep, " ", (char *)NULL);
	sp->s_flag = UP;
	FOR_REST_OF_SOLIDS(forw, sp, &dgop->dgo_headSolid) {
	    if (FIRST_SOLID(forw) == FIRST_SOLID(sp))
		forw->s_flag = UP;
	}
    }
    FOR_ALL_SOLIDS(sp, &dgop->dgo_headSolid)
	sp->s_flag = DOWN;

    return TCL_OK;
}


/*
 * List the objects currently being drawn.
 *
 * Usage:
 * procname who [r(eal)|p(hony)|b(oth)]
 */
static int
dgo_who_tcl(void *clientData, int argc, const char **argv)
{
    struct dg_obj *dgop = (struct dg_obj *)clientData;
    Tcl_Interp *interp = dgop->interp;

    DGO_CHECK_WDBP_NULL(dgop, interp);
    return dgo_who_cmd(dgop, argc-1, argv+1);
}


/*
 * C V T _ V L B L O C K _ T O _ S O L I D S
 */
void
dgo_cvt_vlblock_to_solids(struct dg_obj *dgop, struct bn_vlblock *vbp, const char *name, int copy)
{
    size_t i;
    char shortname[32];
    char namebuf[64];

    bu_strlcpy(shortname, name, sizeof(shortname));

    for (i = 0; i < vbp->nused; i++) {
	if (BU_LIST_IS_EMPTY(&(vbp->head[i])))
	    continue;

	snprintf(namebuf, 64, "%s%lx", shortname, vbp->rgb[i]);
	dgo_invent_solid(dgop, namebuf, &vbp->head[i], vbp->rgb[i], copy, 0.0, 0);
    }
}


static void
dgo_overlay(struct dg_obj *dgop, FILE *fp, const char *name, double char_size)
{
    int ret;
    struct bn_vlblock*vbp;

    vbp = rt_vlblock_init();
    ret = rt_uplot_to_vlist(vbp, fp, char_size, dgop->dgo_uplotOutputMode);
    fclose(fp);

    if (ret < 0) {
	rt_vlblock_free(vbp);
	return;
    }

    dgo_cvt_vlblock_to_solids(dgop, vbp, name, 0);
    rt_vlblock_free(vbp);
}


int
dgo_overlay_cmd(struct dg_obj *dgop,
		int argc,
		const char **argv)
{
    FILE *fp;
    double char_size;
    const char *name;

    if (argc < 3 || 4 < argc) {
	bu_log("ERROR: expecting three or four arguments\n");
	return TCL_ERROR;
    }

    if (sscanf(argv[2], "%lf", &char_size) != 1) {
	bu_log("dgo_overlay: bad character size - %s\n", argv[2]);
	return TCL_ERROR;
    }

    if (argc == 3)
	name = "_PLOT_OVERLAY_";
    else
	name = argv[3];

    if ((fp = fopen(argv[1], "rb")) == NULL) {
	bu_log("dgo_overlay: failed to open file - %s\n", argv[1]);

	return TCL_ERROR;
    }

    dgo_overlay(dgop, fp, name, char_size);
    return TCL_OK;
}


/*
 * Usage:
 * procname overlay file.plot3 char_size [name]
 */
static int
dgo_overlay_tcl(void *clientData, int argc, const char **argv)
{
    struct dg_obj *dgop = (struct dg_obj *)clientData;
    Tcl_Interp *interp = dgop->interp;
    int ret;

    DGO_CHECK_WDBP_NULL(dgop, interp);

    if ((ret = dgo_overlay_cmd(dgop, argc-1, argv+1)) == TCL_OK)
	dgo_notify(dgop);

    return ret;
}


void
dgo_autoview(struct dg_obj *dgop,
	     struct view_obj *vop)
{
    struct solid *sp;
    vect_t min, max;
    vect_t minus, plus;
    vect_t center;
    vect_t radial;
    vect_t small;

    VSETALL(min,  INFINITY);
    VSETALL(max, -INFINITY);
    VSETALL(small, SQRT_SMALL_FASTF);

    FOR_ALL_SOLIDS(sp, &dgop->dgo_headSolid) {
	minus[X] = sp->s_center[X] - sp->s_size;
	minus[Y] = sp->s_center[Y] - sp->s_size;
	minus[Z] = sp->s_center[Z] - sp->s_size;
	VMIN(min, minus);
	plus[X] = sp->s_center[X] + sp->s_size;
	plus[Y] = sp->s_center[Y] + sp->s_size;
	plus[Z] = sp->s_center[Z] + sp->s_size;
	VMAX(max, plus);
    }

    if (BU_LIST_IS_EMPTY(&dgop->dgo_headSolid)) {
	/* Nothing is in view */
	VSETALL(center, 0.0);
	VSETALL(radial, 1000.0);	/* 1 meter */
    } else {
	VADD2SCALE(center, max, min, 0.5);
	VSUB2(radial, max, center);
    }

    /* make sure it's not inverted */
    VMAX(radial, small);

    /* make sure it's not too small */
    if (VNEAR_ZERO(radial, SQRT_SMALL_FASTF))
	VSETALL(radial, 1.0);

    MAT_IDN(vop->vo_center);
    MAT_DELTAS_VEC_NEG(vop->vo_center, center);
    vop->vo_scale = radial[X];
    V_MAX(vop->vo_scale, radial[Y]);
    V_MAX(vop->vo_scale, radial[Z]);

    vop->vo_size = 2.0 * vop->vo_scale;
    vop->vo_invSize = 1.0 / vop->vo_size;
    vo_update(vop, 1);
}


int
dgo_autoview_cmd(struct dg_obj *dgop,
		 struct view_obj *vop,
		 int argc,
		 const char **UNUSED(argv))
{
    if (argc != 2) {
	bu_log("ERROR: expecting only two arguments\n");
	return TCL_ERROR;
    }

    dgo_autoview(dgop, vop);

    return TCL_OK;
}


/*
 * Usage:
 * procname autoview view_obj
 */
static int
dgo_autoview_tcl(void *clientData,
		 int argc,
		 const char **argv)
{
    struct dg_obj *dgop = (struct dg_obj *)clientData;
    Tcl_Interp *interp = dgop->interp;
    struct view_obj *vop;

    if (argc != 3) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&vls, "helplib_alias dgo_autoview %s", argv[0]);
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    DGO_CHECK_WDBP_NULL(dgop, interp);

    /* search for view object */
    for (BU_LIST_FOR (vop, view_obj, &HeadViewObj.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&vop->vo_name), argv[2]))
	    break;
    }

    if (BU_LIST_IS_HEAD(vop, &HeadViewObj.l)) {
	Tcl_AppendResult(interp, "dgo_autoview: bad view object - ", argv[2],
			 "\n", (char *)NULL);
	return TCL_ERROR;
    }

    return dgo_autoview_cmd(dgop, vop, argc-1, argv+1);
}


int
dgo_get_autoview_cmd(struct dg_obj *dgop,
		     int argc,
		     const char **argv)
{
    struct bu_vls vls = BU_VLS_INIT_ZERO;
    struct solid *sp;
    vect_t min, max;
    vect_t minus, plus;
    vect_t center;
    vect_t radial;
    int pflag = 0;
    int c;

    if (argc < 1 || 2 < argc) {
	bu_log("ERROR: expecting no more than two arguments\n");
	return TCL_ERROR;
    }

    /* Parse options. */
    bu_optind = 1;
    while ((c = bu_getopt(argc, (char * const *)argv, "p")) != -1) {
	switch (c) {
	    case 'p':
		pflag = 1;
		break;
	    default: {
		bu_log("ERROR: unexpected option\n");
		return TCL_ERROR;
	    }
	}
    }
    argc -= bu_optind;
    argv += bu_optind;

    VSETALL(min,  INFINITY);
    VSETALL(max, -INFINITY);

    FOR_ALL_SOLIDS(sp, &dgop->dgo_headSolid) {
	/* Skip pseudo-solids unless pflag is set */
	if (!pflag &&
	    sp->s_fullpath.fp_names != (struct directory **)0 &&
	    sp->s_fullpath.fp_names[0] != (struct directory *)0 &&
	    sp->s_fullpath.fp_names[0]->d_addr == RT_DIR_PHONY_ADDR)
	    continue;

	minus[X] = sp->s_center[X] - sp->s_size;
	minus[Y] = sp->s_center[Y] - sp->s_size;
	minus[Z] = sp->s_center[Z] - sp->s_size;
	VMIN(min, minus);
	plus[X] = sp->s_center[X] + sp->s_size;
	plus[Y] = sp->s_center[Y] + sp->s_size;
	plus[Z] = sp->s_center[Z] + sp->s_size;
	VMAX(max, plus);
    }

    if (BU_LIST_IS_EMPTY(&dgop->dgo_headSolid)) {
	/* Nothing is in view */
	VSETALL(center, 0.0);
	VSETALL(radial, 1000.0);	/* 1 meter */
    } else {
	VADD2SCALE(center, max, min, 0.5);
	VSUB2(radial, max, center);
    }

    if (VNEAR_ZERO(radial, SQRT_SMALL_FASTF))
	VSETALL(radial, 1.0);

    VSCALE(center, center, dgop->dgo_wdbp->dbip->dbi_base2local);
    radial[X] *= dgop->dgo_wdbp->dbip->dbi_base2local;

    bu_vls_printf(&vls, "center {%g %g %g} size %g", V3ARGS(center), radial[X] * 2.0);
    Tcl_AppendResult(dgop->interp, bu_vls_addr(&vls), (char *)NULL);
    bu_vls_free(&vls);

    return TCL_OK;
}


/*
 * Usage:
 * procname get_autoview
 */
static int
dgo_get_autoview_tcl(void *clientData,
		     int argc,
		     const char **argv)
{
    struct dg_obj *dgop = (struct dg_obj *)clientData;

    return dgo_get_autoview_cmd(dgop, argc-1, argv+1);
}


static void
dgo_rt_set_eye_model(struct dg_obj *dgop,
		     struct view_obj *vop,
		     vect_t eye_model)
{
    if (vop->vo_zclip || vop->vo_perspective > 0) {
	vect_t temp;

	VSET(temp, 0.0, 0.0, 1.0);
	MAT4X3PNT(eye_model, vop->vo_view2model, temp);
    } else {
	/* not doing zclipping, so back out of geometry */
	struct solid *sp;
	int i;
	vect_t direction;
	vect_t extremum[2];
	vect_t minus, plus;    /* vers of this solid's bounding box */

	VSET(eye_model, -vop->vo_center[MDX],
	     -vop->vo_center[MDY], -vop->vo_center[MDZ]);

	for (i = 0; i < 3; ++i) {
	    extremum[0][i] = INFINITY;
	    extremum[1][i] = -INFINITY;
	}

	FOR_ALL_SOLIDS (sp, &dgop->dgo_headSolid) {
	    minus[X] = sp->s_center[X] - sp->s_size;
	    minus[Y] = sp->s_center[Y] - sp->s_size;
	    minus[Z] = sp->s_center[Z] - sp->s_size;
	    VMIN(extremum[0], minus);
	    plus[X] = sp->s_center[X] + sp->s_size;
	    plus[Y] = sp->s_center[Y] + sp->s_size;
	    plus[Z] = sp->s_center[Z] + sp->s_size;
	    VMAX(extremum[1], plus);
	}
	VMOVEN(direction, vop->vo_rotation + 8, 3);
	for (i = 0; i < 3; ++i)
	    if (NEAR_ZERO(direction[i], 1e-10))
		direction[i] = 0.0;
	if ((eye_model[X] >= extremum[0][X]) &&
	    (eye_model[X] <= extremum[1][X]) &&
	    (eye_model[Y] >= extremum[0][Y]) &&
	    (eye_model[Y] <= extremum[1][Y]) &&
	    (eye_model[Z] >= extremum[0][Z]) &&
	    (eye_model[Z] <= extremum[1][Z])) {
	    double t_in;
	    vect_t diag;

	    VSUB2(diag, extremum[1], extremum[0]);
	    t_in = MAGNITUDE(diag);
	    VJOIN1(eye_model, eye_model, t_in, direction);
	}
    }
}


/*
 * support for get_eyemodel
 */
int
dgo_get_eyemodel_cmd(struct dg_obj *dgop,
		     int argc,
		     const char **argv)
{
    struct bu_vls vls;
    struct view_obj * vop;
    quat_t quat;
    vect_t eye_model;

    if (argc != 2) {
	bu_log("ERROR: expecting two arguments\n");
	return TCL_ERROR;
    }

    /*
     * Retrieve the view object
     */
    for (BU_LIST_FOR (vop, view_obj, &HeadViewObj.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&vop->vo_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(vop, &HeadViewObj.l)) {
	bu_log("dgo_get_eyemodel: bad view object - %s\n", argv[2]);
	return TCL_ERROR;
    }

    dgo_rt_set_eye_model(dgop, vop, eye_model);

    bu_vls_init(&vls);

    quat_mat2quat(quat, vop->vo_rotation);

    bu_vls_printf(&vls, "viewsize %.15e;\n", vop->vo_size);
    bu_vls_printf(&vls, "orientation %.15e %.15e %.15e %.15e;\n",
		  V4ARGS(quat));
    bu_vls_printf(&vls, "eye_pt %.15e %.15e %.15e;\n",
		  eye_model[X], eye_model[Y], eye_model[Z]);
    Tcl_AppendResult(dgop->interp, bu_vls_addr(&vls), NULL);

    bu_vls_free(&vls);

    return TCL_OK;
}


/*
 * D G O _ R T _ W R I T E
 *
 * Write out the information that RT's -M option needs to show current view.
 * Note that the model-space location of the eye is a parameter,
 * as it can be computed in different ways.
 */
static void
dgo_rt_write(struct dg_obj *dgop,
	     struct view_obj *vop,
	     FILE *fp,
	     vect_t eye_model)
{
    size_t i;
    quat_t quat;
    struct solid *sp;

    fprintf(fp, "viewsize %.15e;\n", vop->vo_size);
    quat_mat2quat(quat, vop->vo_rotation);
    fprintf(fp, "orientation %.15e %.15e %.15e %.15e;\n", V4ARGS(quat));
    fprintf(fp, "eye_pt %.15e %.15e %.15e;\n",
		  eye_model[X], eye_model[Y], eye_model[Z]);

    fprintf(fp, "start 0; clean;\n");
    FOR_ALL_SOLIDS (sp, &dgop->dgo_headSolid) {
	for (i = 0; i < sp->s_fullpath.fp_len; i++) {
	    DB_FULL_PATH_GET(&sp->s_fullpath, i)->d_flags &= ~RT_DIR_USED;
	}
    }
    FOR_ALL_SOLIDS(sp, &dgop->dgo_headSolid) {
	for (i = 0; i < sp->s_fullpath.fp_len; i++) {
	    if (!(DB_FULL_PATH_GET(&sp->s_fullpath, i)->d_flags & RT_DIR_USED)) {
		struct animate *anp;
		for (anp = DB_FULL_PATH_GET(&sp->s_fullpath, i)->d_animate; anp;
		     anp=anp->an_forw) {
		    db_write_anim(fp, anp);
		}
		DB_FULL_PATH_GET(&sp->s_fullpath, i)->d_flags |= RT_DIR_USED;
	    }
	}
    }

    FOR_ALL_SOLIDS(sp, &dgop->dgo_headSolid) {
	for (i = 0; i < sp->s_fullpath.fp_len; i++) {
	    DB_FULL_PATH_GET(&sp->s_fullpath, i)->d_flags &= ~RT_DIR_USED;
	}
    }
    fprintf(fp, "end;\n");
}


#ifndef _WIN32
static void
dgo_rt_output_handler(void *clientData, int UNUSED(mask))
{
    struct dg_rt_client_data *drcdp = (struct dg_rt_client_data *)clientData;
    struct run_rt *run_rtp;
    int count;
    char line[RT_MAXLINE+1];

    if (drcdp == (struct dg_rt_client_data *)NULL ||
	drcdp->dgop == (struct dg_obj *)NULL ||
	drcdp->rrtp == (struct run_rt *)NULL ||
	drcdp->interp == (Tcl_Interp *)NULL)
	return;

    run_rtp = drcdp->rrtp;

    /* Get data from rt */
    count = read((int)run_rtp->fd, line, RT_MAXLINE);
    if (count <= 0) {
	int retcode = 0;
	int rpid;
	int aborted;

	if (count < 0) {
	    perror("READ ERROR");
	}

	Tcl_DeleteFileHandler(run_rtp->fd);
	close(run_rtp->fd);

	/* wait for the forked process */
	while ((rpid = wait(&retcode)) != run_rtp->pid && rpid != -1);

	aborted = run_rtp->aborted;

	if (drcdp->dgop->dgo_outputHandler != NULL) {
	    struct bu_vls vls;

	    bu_vls_init(&vls);

	    if (aborted)
		bu_vls_printf(&vls, "%s \"Raytrace aborted.\n\"",
			      drcdp->dgop->dgo_outputHandler);
	    else if (retcode)
		bu_vls_printf(&vls, "%s \"Raytrace failed.\n\"",
			      drcdp->dgop->dgo_outputHandler);
	    else
		bu_vls_printf(&vls, "%s \"Raytrace complete.\n\"",
			      drcdp->dgop->dgo_outputHandler);

	    Tcl_Eval(drcdp->interp, bu_vls_addr(&vls));
	    bu_vls_free(&vls);
	} else {
	    if (aborted)
		bu_log("Raytrace aborted.\n");
	    else if (retcode)
		bu_log("Raytrace failed.\n");
	    else
		bu_log("Raytrace complete.\n");
	}

	if (drcdp->dgop->dgo_rtCmdNotify != (void (*)())0)
	    drcdp->dgop->dgo_rtCmdNotify();

	/* free run_rtp */
	BU_LIST_DEQUEUE(&run_rtp->l);
	bu_free((genptr_t)run_rtp, "dgo_rt_output_handler: run_rtp");

	bu_free((genptr_t)drcdp, "dgo_rt_output_handler: drcdp");

	return;
    }

    line[count] = '\0';

    /*XXX For now just blather to stderr */
    if (drcdp->dgop->dgo_outputHandler != NULL) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "%s \"%s\"", drcdp->dgop->dgo_outputHandler, line);
	Tcl_Eval(drcdp->interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
    } else
	bu_log("%s", line);
}

#else /* WIN32 */

static void
dgo_rt_output_handler(void *clientData, int UNUSED(mask))
{
    struct dg_rt_client_data *drcdp = (struct dg_rt_client_data *)clientData;
    struct run_rt *run_rtp;
    DWORD count;
    char line[10240+1] = {0};

    if (drcdp == (struct dg_rt_client_data *)NULL ||
	drcdp->dgop == (struct dg_obj *)NULL ||
	drcdp->rrtp == (struct run_rt *)NULL ||
	drcdp->interp == (Tcl_Interp *)NULL)
	return;

    run_rtp = drcdp->rrtp;

    /* Get data from rt */
    if (Tcl_Eof(run_rtp->chan) ||
	(!ReadFile(run_rtp->fd, line, 10240, &count, 0))) {
	int aborted;
	DWORD retcode;

	Tcl_DeleteChannelHandler(run_rtp->chan,
				 dgo_rt_output_handler,
				 (ClientData)drcdp);
	Tcl_Close(drcdp->interp, run_rtp->chan);

	/* wait for the forked process
	 * either EOF has been sent or there was a read error.
	 * there is no need to block indefinitely
	 */
	WaitForSingleObject(run_rtp->hProcess, 120);
	/* !!! need to observe implications of being non-infinite
	 * WaitForSingleObject(run_rtp->hProcess, INFINITE);
	 */

	if (GetLastError() == ERROR_PROCESS_ABORTED) {
	    run_rtp->aborted = 1;
	}

	GetExitCodeProcess(run_rtp->hProcess, &retcode);
	/* may be useful to try pr_wait_status() here */

	aborted = run_rtp->aborted;

	if (drcdp->dgop->dgo_outputHandler != NULL) {
	    struct bu_vls vls;

	    bu_vls_init(&vls);

	    if (aborted)
		bu_vls_printf(&vls, "%s \"Raytrace aborted.\n\"",
			      drcdp->dgop->dgo_outputHandler);
	    else if (retcode)
		bu_vls_printf(&vls, "%s \"Raytrace failed.\n\"",
			      drcdp->dgop->dgo_outputHandler);
	    else
		bu_vls_printf(&vls, "%s \"Raytrace complete.\n\"",
			      drcdp->dgop->dgo_outputHandler);

	    Tcl_Eval(drcdp->interp, bu_vls_addr(&vls));
	    bu_vls_free(&vls);
	} else {
	    if (aborted)
		bu_log("Raytrace aborted.\n");
	    else if (retcode)
		bu_log("Raytrace failed.\n");
	    else
		bu_log("Raytrace complete.\n");
	}

	if (drcdp->dgop->dgo_rtCmdNotify != (void (*)())0)
	    drcdp->dgop->dgo_rtCmdNotify();

	/* free run_rtp */
	BU_LIST_DEQUEUE(&run_rtp->l);
	bu_free((genptr_t)run_rtp, "dgo_rt_output_handler: run_rtp");

	bu_free((genptr_t)drcdp, "dgo_rt_output_handler: drcdp");

	return;
    }

    line[count] = '\0';

    /*XXX For now just blather to stderr */
    if (drcdp->dgop->dgo_outputHandler != NULL) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "%s \"%s\"", drcdp->dgop->dgo_outputHandler, line);
	Tcl_Eval(drcdp->interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
    } else
	bu_log("%s", line);
}


#endif


/*
 * Usage:
 * procname get_eyemodel
 */
static int
dgo_get_eyemodel_tcl(void *clientData,
		     int argc,
		     const char **argv)
{
    struct dg_obj *dgop = (struct dg_obj *)clientData;
    return dgo_get_eyemodel_cmd(dgop, argc-1, argv+1);
}


/*
 * D G O _ R U N _ R T
 */
static int
dgo_run_rt(struct dg_obj *dgop,
	   struct view_obj *vop)
{
    size_t i;
    FILE *fp_in;
#ifndef _WIN32
    int pipe_in[2];
    int pipe_err[2];
#else
    HANDLE pipe_in[2], pipe_inDup;
    HANDLE pipe_err[2], pipe_errDup;
    STARTUPINFO si = {0};
    PROCESS_INFORMATION pi = {0};
    SECURITY_ATTRIBUTES sa          = {0};
    char line[2048];
    char name[256];
#endif
    vect_t eye_model;
    struct run_rt *run_rtp;
    struct dg_rt_client_data *drcdp;
#ifndef _WIN32
    int pid;
    int ret;

    ret = pipe(pipe_in);
    if (ret != 0)
	perror("pipe");
    ret = pipe(pipe_err);
    if (ret != 0)
	perror("pipe");

    if ((pid = fork()) == 0) {
	/* make this a process group leader */
	setpgid(0, 0);

	/* Redirect stdin and stderr */
	(void)close(0);
	ret = dup(pipe_in[0]);
	if (ret < 0)
	    perror("dup");
	(void)close(2);
	ret = dup(pipe_err[1]);
	if (ret < 0)
	    perror("dup");

	/* close pipes */
	(void)close(pipe_in[0]);
	(void)close(pipe_in[1]);
	(void)close(pipe_err[0]);
	(void)close(pipe_err[1]);

	for (i = 3; i < 20; i++)
	    (void)close(i);

	(void)execvp(dgop->dgo_rt_cmd[0], dgop->dgo_rt_cmd);
	perror(dgop->dgo_rt_cmd[0]);
	exit(16);
    }

    /* As parent, send view information down pipe */
    (void)close(pipe_in[0]);
    fp_in = fdopen(pipe_in[1], "w");

    (void)close(pipe_err[1]);

    dgo_rt_set_eye_model(dgop, vop, eye_model);
    dgo_rt_write(dgop, vop, fp_in, eye_model);
    (void)fclose(fp_in);

    BU_ALLOC(run_rtp, struct run_rt);
    BU_LIST_INIT(&run_rtp->l);
    BU_LIST_APPEND(&dgop->dgo_headRunRt.l, &run_rtp->l);

    run_rtp->fd = pipe_err[0];
    run_rtp->pid = pid;

    BU_ALLOC(drcdp, struct dg_rt_client_data);
    drcdp->dgop = dgop;
    drcdp->rrtp = run_rtp;
    drcdp->interp = dgop->dgo_wdbp->wdb_interp;

    Tcl_CreateFileHandler(run_rtp->fd,
			  TCL_READABLE,
			  dgo_rt_output_handler,
			  (ClientData)drcdp);

    return 0;

#else
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    /* Create a pipe for the child process's STDOUT. */
    CreatePipe(&pipe_err[0], &pipe_err[1], &sa, 0);

    /* Create noninheritable read handle and close the inheritable read handle. */
    DuplicateHandle(GetCurrentProcess(), pipe_err[0],
		    GetCurrentProcess(),  &pipe_errDup ,
		    0,  FALSE,
		    DUPLICATE_SAME_ACCESS);
    CloseHandle(pipe_err[0]);

    /* Create a pipe for the child process's STDIN. */
    CreatePipe(&pipe_in[0], &pipe_in[1], &sa, 0);

    /* Duplicate the write handle to the pipe so it is not inherited. */
    DuplicateHandle(GetCurrentProcess(), pipe_in[1],
		    GetCurrentProcess(), &pipe_inDup,
		    0, FALSE,                  /* not inherited */
		    DUPLICATE_SAME_ACCESS);
    CloseHandle(pipe_in[1]);


    si.cb = sizeof(STARTUPINFO);
    si.lpReserved = NULL;
    si.lpReserved2 = NULL;
    si.cbReserved2 = 0;
    si.lpDesktop = NULL;
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput   = pipe_in[0];
    si.hStdOutput  = pipe_err[1];
    si.hStdError   = pipe_err[1];

    snprintf(line, sizeof(line), "%s ", dgop->dgo_rt_cmd[0]);
    for (i = 1; i < dgop->dgo_rt_cmd_len; i++) {
	snprintf(name, sizeof(name), "%s ", dgop->dgo_rt_cmd[i]);
	bu_strlcat(line, name, sizeof(line));
    }

    CreateProcess(NULL, line, NULL, NULL, TRUE,
		  DETACHED_PROCESS, NULL, NULL,
		  &si, &pi);

    CloseHandle(pipe_in[0]);
    CloseHandle(pipe_err[1]);

    /* As parent, send view information down pipe */
    fp_in = _fdopen(_open_osfhandle((intptr_t)pipe_inDup, _O_TEXT), "wb");

    dgo_rt_set_eye_model(dgop, vop, eye_model);
    dgo_rt_write(dgop, vop, fp_in, eye_model);
    (void)fclose(fp_in);

    BU_ALLOC(run_rtp, struct run_rt);
    BU_LIST_INIT(&run_rtp->l);
    BU_LIST_APPEND(&dgop->dgo_headRunRt.l, &run_rtp->l);

    run_rtp->fd = pipe_errDup;
    run_rtp->hProcess = pi.hProcess;
    run_rtp->pid = pi.dwProcessId;
    run_rtp->aborted=0;
    run_rtp->chan = Tcl_MakeFileChannel(run_rtp->fd, TCL_READABLE);

    BU_ALLOC(drcdp, struct dg_rt_client_data);
    drcdp->dgop = dgop;
    drcdp->rrtp = run_rtp;
    drcdp->interp = dgop->dgo_wdbp->wdb_interp;

    Tcl_CreateChannelHandler(run_rtp->chan,
			     TCL_READABLE,
			     dgo_rt_output_handler,
			     (ClientData)drcdp);

    return 0;
#endif

}


int
dgo_rt_command(struct dg_obj *dgop,
	       struct view_obj *vop,
	       int argc,
	       const char **argv)
{
    const char **vp;
    int i;
    char pstring[32];
    size_t args;

    if (argc < 1) {
	bu_log("ERROR: expecting more than zero arguments\n");
	return TCL_ERROR;
    }

    args = argc + 2 + dgo_count_tops((struct solid *)&dgop->dgo_headSolid);
    dgop->dgo_rt_cmd = (char **)bu_calloc(args, sizeof(char *), "alloc dgo_rt_cmd");

    vp = (const char **)&dgop->dgo_rt_cmd[0];
    *vp++ = argv[0];
    *vp++ = "-M";

    if (vop->vo_perspective > 0) {
	(void)sprintf(pstring, "-p%g", vop->vo_perspective);
	*vp++ = pstring;
    }

    for (i = 1; i < argc; i++) {
	if (argv[i][0] == '-' && argv[i][1] == '-' &&
	    argv[i][2] == '\0') {
	    ++i;
	    break;
	}
	*vp++ = argv[i];
    }
    /* XXX why is this different for win32 only? */
#ifdef _WIN32
    {
	char buf[512];

	snprintf(buf, 512, "\"%s\"", dgop->dgo_wdbp->dbip->dbi_filename);
	*vp++ = buf;
    }
#else
    *vp++ = dgop->dgo_wdbp->dbip->dbi_filename;
#endif

    /*
     * Now that we've grabbed all the options, if no args remain,
     * append the names of all stuff currently displayed.
     * Otherwise, simply append the remaining args.
     */
    if (i == argc) {
	dgop->dgo_rt_cmd_len = (char **)vp - (char **)dgop->dgo_rt_cmd;
	dgop->dgo_rt_cmd_len += dgo_build_tops((struct solid *)&dgop->dgo_headSolid,
					       vp,
					       (const char **)&dgop->dgo_rt_cmd[args]);
    } else {
	while (i < argc)
	    *vp++ = argv[i++];
	*vp = 0;
	vp = (const char **)&dgop->dgo_rt_cmd[0];
	while (*vp)
	    Tcl_AppendResult(dgop->interp, *vp++, " ", (char *)NULL);

	Tcl_AppendResult(dgop->interp, "\n", (char *)NULL);
    }
    (void)dgo_run_rt(dgop, vop);

    bu_free(dgop->dgo_rt_cmd, "free dgo_rt_cmd");
    dgop->dgo_rt_cmd = NULL;

    return TCL_OK;
}


/*
 * Usage:
 * procname rt view_obj arg(s)
 */
static int
dgo_rt_tcl(void *clientData, int argc, const char **argv)
{
    struct dg_obj *dgop = (struct dg_obj *)clientData;
    Tcl_Interp *interp = dgop->interp;
    struct view_obj *vop;

    if (argc < 3) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib_alias dgo_%s %s", argv[0], argv[0]);
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    DGO_CHECK_WDBP_NULL(dgop, interp);

    /* search for view object */
    for (BU_LIST_FOR (vop, view_obj, &HeadViewObj.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&vop->vo_name), argv[2]))
	    break;
    }

    if (BU_LIST_IS_HEAD(vop, &HeadViewObj.l)) {
	Tcl_AppendResult(interp, "dgo_rt: bad view object - ", argv[2],
			 "\n", (char *)NULL);
	return TCL_ERROR;
    }

    /* copy command name into argv[2], could be rt or some other rt-style command */
    argv[2] = argv[1];
    return dgo_rt_command(dgop, vop, argc-2, argv+2);
}


void
dgo_zap_cmd(struct dg_obj *dgop)
{
    struct solid *sp;
    struct solid *nsp;
    struct directory *dp;

    sp = BU_LIST_NEXT(solid, &dgop->dgo_headSolid);
    while (BU_LIST_NOT_HEAD(sp, &dgop->dgo_headSolid)) {
	dp = FIRST_SOLID(sp);
	RT_CK_DIR(dp);
	if (dp->d_addr == RT_DIR_PHONY_ADDR) {
	    if (db_dirdelete(dgop->dgo_wdbp->dbip, dp) < 0) {
		bu_log("dgo_zap: db_dirdelete failed\n");
	    }
	}

	nsp = BU_LIST_PNEXT(solid, sp);
	BU_LIST_DEQUEUE(&sp->l);
	FREE_SOLID(sp, &_FreeSolid.l);
	sp = nsp;
    }
}


/*
 * Usage:
 * procname clear|zap
 */
static int
dgo_zap_tcl(void *clientData,
	    int argc,
	    const char **UNUSED(argv))
{
    struct dg_obj *dgop = (struct dg_obj *)clientData;

    if (argc != 2) {
	bu_log("ERROR: expecting two arguments\n");
	return TCL_ERROR;
    }

    dgo_zap_cmd(dgop);
    dgo_notify(dgop);

    return TCL_OK;
}


int
dgo_blast_cmd(struct dg_obj *dgop,
	      int argc,
	      const char **argv)
{
    /* First, clear the screen. */
    dgo_zap_cmd(dgop);

    /* Now, draw the new object(s). */
    return dgo_draw_cmd(dgop, argc, argv, 1);
}


/*
 * Usage:
 * procname blast object(s)
 */
static int
dgo_blast_tcl(void *clientData, int argc, const char **argv)
{
    struct dg_obj *dgop = (struct dg_obj *)clientData;
    Tcl_Interp *interp = dgop->interp;
    int ret;

    DGO_CHECK_WDBP_NULL(dgop, interp);

    if (argc < 3) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib_alias dgo_blast %s", argv[0]);
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);

	return TCL_ERROR;
    }

    if ((ret = dgo_blast_cmd(dgop, argc-1, argv+1)) == TCL_OK)
	dgo_notify(dgop);

    return ret;
}


struct rtcheck {
#ifdef _WIN32
    HANDLE fd;
    HANDLE hProcess;
    DWORD pid;
#ifdef TCL_OK
    Tcl_Channel chan;
#else
    genptr_t chan;
#endif
#else
    int fd;
    int pid;
#endif
    FILE *fp;
    struct bn_vlblock *vbp;
    struct bu_list *vhead;
    double csize;
    struct dg_obj *dgop;
    Tcl_Interp *interp;
};


struct rtcheck_output {
#ifdef _WIN32
    HANDLE fd;
    Tcl_Channel chan;
#else
    int fd;
#endif
    struct dg_obj *dgop;
    Tcl_Interp *interp;
};


/*
 * D G O _ W A I T _ S T A T U S
 *
 * Interpret the status return of a wait() system call,
 * for the edification of the watching luser.
 * Warning:  This may be somewhat system specific, most especially
 * on non-UNIX machines.
 */
static void
dgo_wait_status(Tcl_Interp *interp, int status)
{
    int sig = status & 0x7f;
    int core = status & 0x80;
    int ret = status >> 8;
    struct bu_vls tmp_vls;

    if (status == 0) {
	Tcl_AppendResult(interp, "Normal exit\n", (char *)NULL);
	return;
    }

    bu_vls_init(&tmp_vls);
    bu_vls_printf(&tmp_vls, "Abnormal exit x%x", status);

    if (core)
	bu_vls_printf(&tmp_vls, ", core dumped");

    if (sig)
	bu_vls_printf(&tmp_vls, ", terminating signal = %d", sig);
    else
	bu_vls_printf(&tmp_vls, ", return (exit) code = %d", ret);

    Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), "\n", (char *)NULL);
    bu_vls_free(&tmp_vls);
}


#ifndef _WIN32
static void
dgo_rtcheck_vector_handler(void *clientData, int UNUSED(mask))
{
    int value;
    struct solid *sp;
    struct rtcheck *rtcp = (struct rtcheck *)clientData;

    /* Get vector output from rtcheck */
    if ((value = getc(rtcp->fp)) == EOF) {
	int retcode;
	int rpid;

	Tcl_DeleteFileHandler(rtcp->fd);
	fclose(rtcp->fp);

	FOR_ALL_SOLIDS(sp, &rtcp->dgop->dgo_headSolid)
	    sp->s_flag = DOWN;

	/* Add overlay */
	dgo_cvt_vlblock_to_solids(rtcp->dgop, rtcp->vbp, "OVERLAPS", 0);
	rt_vlblock_free(rtcp->vbp);

	/* wait for the forked process */
	while ((rpid = wait(&retcode)) != rtcp->pid && rpid != -1)
	    dgo_wait_status(rtcp->interp, retcode);

	dgo_notify(rtcp->dgop);

	/* free rtcp */
	bu_free((genptr_t)rtcp, "dgo_rtcheck_vector_handler: rtcp");

	return;
    }

    (void)rt_process_uplot_value(&rtcp->vhead,
				 rtcp->vbp,
				 rtcp->fp,
				 value,
				 rtcp->csize,
				 rtcp->dgop->dgo_uplotOutputMode);
}


static void
dgo_rtcheck_output_handler(void *clientData, int UNUSED(mask))
{
    int count;
    char line[RT_MAXLINE] = {0};
    struct rtcheck_output *rtcop = (struct rtcheck_output *)clientData;

    /* Get textual output from rtcheck */
    count = read((int)rtcop->fd, line, RT_MAXLINE);
    if (count <= 0) {
	if (count < 0) {
	    perror("READ ERROR");
	}
	Tcl_DeleteFileHandler(rtcop->fd);
	close(rtcop->fd);

	if (rtcop->dgop->dgo_rtCmdNotify != (void (*)())0)
	    rtcop->dgop->dgo_rtCmdNotify();

	bu_free((genptr_t)rtcop, "dgo_rtcheck_output_handler: rtcop");
	return;
    }

    line[count] = '\0';
    if (rtcop->dgop->dgo_outputHandler != NULL) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "%s \"%s\"", rtcop->dgop->dgo_outputHandler, line);
	Tcl_Eval(rtcop->interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
    } else
	bu_log("%s", line);
}


#else

void
dgo_rtcheck_vector_handler(void *clientData, int UNUSED(mask))
{
    int value;
    struct solid *sp;
    struct rtcheck *rtcp = (struct rtcheck *)clientData;

    /* Get vector output from rtcheck */
    if (feof(rtcp->fp)) {
	Tcl_DeleteChannelHandler(rtcp->chan,
				 dgo_rtcheck_vector_handler,
				 (ClientData)rtcp);
	Tcl_Close(rtcp->interp, rtcp->chan);

	FOR_ALL_SOLIDS(sp, &rtcp->dgop->dgo_headSolid)
	    sp->s_flag = DOWN;

	/* Add overlay */
	dgo_cvt_vlblock_to_solids(rtcp->dgop, rtcp->vbp, "OVERLAPS", 0);
	rt_vlblock_free(rtcp->vbp);

	/* wait for the forked process */
	WaitForSingleObject(rtcp->hProcess, INFINITE);

	dgo_notify(rtcp->dgop);

	/* free rtcp */
	bu_free((genptr_t)rtcp, "dgo_rtcheck_vector_handler: rtcp");

	return;
    }

    value = getc(rtcp->fp);
    (void)rt_process_uplot_value(&rtcp->vhead,
				 rtcp->vbp,
				 rtcp->fp,
				 value,
				 rtcp->csize,
				 rtcp->dgop->dgo_uplotOutputMode);
}


void
dgo_rtcheck_output_handler(void *clientData, int UNUSED(mask))
{
    DWORD count;
    char line[RT_MAXLINE];
    struct rtcheck_output *rtcop = (struct rtcheck_output *)clientData;

    /* Get textual output from rtcheck */
    if (Tcl_Eof(rtcop->chan) ||
	(!ReadFile(rtcop->fd, line, RT_MAXLINE, &count, 0))) {

	Tcl_DeleteChannelHandler(rtcop->chan,
				 dgo_rtcheck_output_handler,
				 (ClientData)rtcop);
	Tcl_Close(rtcop->interp, rtcop->chan);

	if (rtcop->dgop->dgo_rtCmdNotify != (void (*)())0)
	    rtcop->dgop->dgo_rtCmdNotify();

	bu_free((genptr_t)rtcop, "dgo_rtcheck_output_handler: rtcop");

	return;
    }

    line[count] = '\0';
    if (rtcop->dgop->dgo_outputHandler != NULL) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "%s \"%s\"", rtcop->dgop->dgo_outputHandler, line);
	Tcl_Eval(rtcop->interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
    } else
	bu_log("%s", line);
}


#endif

int
dgo_rtcheck_command(struct dg_obj *dgop,
		    struct view_obj *vop,
		    int argc,
		    const char **argv)
{
    const char **vp;
    size_t i;
    size_t args;
    int ret;

#ifndef _WIN32
    int pid;
    int i_pipe[2];	/* object reads results for building vectors */
    int o_pipe[2];	/* object writes view parameters */
    int e_pipe[2];	/* object reads textual results */
#else
    HANDLE i_pipe[2], pipe_iDup;	/* READS results for building vectors */
    HANDLE o_pipe[2], pipe_oDup;	/* WRITES view parameters */
    HANDLE e_pipe[2], pipe_eDup;	/* READS textual results */
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    SECURITY_ATTRIBUTES sa;
    char line[2048];
    char name[256];
#endif
    FILE *fp;
    struct rtcheck *rtcp;
    struct rtcheck_output *rtcop;
    vect_t temp;
    vect_t eye_model;

    args = argc + 2 + dgo_count_tops((struct solid *)&dgop->dgo_headSolid);
    dgop->dgo_rt_cmd = (char **)bu_calloc(args, sizeof(char *), "alloc dgo_rt_cmd");

#ifndef _WIN32
    vp = (const char **)&dgop->dgo_rt_cmd[0];
    *vp++ = argv[0];
    *vp++ = "-M";
    for (i = 1; i < (size_t)argc; i++)
	*vp++ = argv[i];
    *vp++ = dgop->dgo_wdbp->dbip->dbi_filename;

    /*
     * Now that we've grabbed all the options, if no args remain,
     * append the names of all stuff currently displayed.
     * Otherwise, simply append the remaining args.
     */
    if (i == (size_t)argc) {
	dgop->dgo_rt_cmd_len = (char **)vp - (char **)dgop->dgo_rt_cmd;
	dgop->dgo_rt_cmd_len += dgo_build_tops((struct solid *)&dgop->dgo_headSolid,
					       vp,
					       (const char **)&dgop->dgo_rt_cmd[args]);
    } else {
	while (i < (size_t)argc)
	    *vp++ = argv[i++];
	*vp = 0;
	vp = (const char **)&dgop->dgo_rt_cmd[0];
	while (*vp)
	    Tcl_AppendResult(dgop->interp, *vp++, " ", (char *)NULL);

	Tcl_AppendResult(dgop->interp, "\n", (char *)NULL);
    }

    ret = pipe(i_pipe);
    if (ret != 0)
	perror("pipe");
    ret = pipe(o_pipe);
    if (ret != 0)
	perror("pipe");
    ret = pipe(e_pipe);
    if (ret != 0)
	perror("pipe");

    if ((pid = fork()) == 0) {
	/* Redirect stdin, stdout and stderr */
	(void)close(0);
	ret = dup(o_pipe[0]);
	if (ret < 0)
	    perror("dup");
	(void)close(1);
	ret = dup(i_pipe[1]);
	if (ret < 0)
	    perror("dup");
	(void)close(2);
	ret = dup(e_pipe[1]);
	if (ret < 0)
	    perror("dup");

	/* close pipes */
	(void)close(i_pipe[0]);
	(void)close(i_pipe[1]);
	(void)close(o_pipe[0]);
	(void)close(o_pipe[1]);
	(void)close(e_pipe[0]);
	(void)close(e_pipe[1]);

	for (i = 3; i < 20; i++)
	    (void)close(i);

	(void)execvp(dgop->dgo_rt_cmd[0], dgop->dgo_rt_cmd);
	perror(dgop->dgo_rt_cmd[0]);
	exit(16);
    }

    /* As parent, send view information down pipe */
    (void)close(o_pipe[0]);
    fp = fdopen(o_pipe[1], "w");
    VSET(temp, 0.0, 0.0, 1.0);
    MAT4X3PNT(eye_model, vop->vo_view2model, temp);
    dgo_rt_write(dgop, vop, fp, eye_model);

    (void)fclose(fp);

    /* close write end of pipes */
    (void)close(i_pipe[1]);
    (void)close(e_pipe[1]);

    BU_ALLOC(rtcp, struct rtcheck);

    /* initialize the rtcheck struct */
    rtcp->fd = i_pipe[0];
    rtcp->fp = fdopen(i_pipe[0], "r");
    rtcp->pid = pid;
    rtcp->vbp = rt_vlblock_init();
    rtcp->vhead = rt_vlblock_find(rtcp->vbp, 0xFF, 0xFF, 0x00);
    rtcp->csize = vop->vo_scale * 0.01;
    rtcp->dgop = dgop;
    rtcp->interp = dgop->interp;

    /* file handlers */
    Tcl_CreateFileHandler(i_pipe[0], TCL_READABLE,
			  dgo_rtcheck_vector_handler, (ClientData)rtcp);

    BU_ALLOC(rtcop, struct rtcheck_output);
    rtcop->fd = e_pipe[0];
    rtcop->dgop = dgop;
    rtcop->interp = dgop->interp;
    Tcl_CreateFileHandler(rtcop->fd,
			  TCL_READABLE,
			  dgo_rtcheck_output_handler,
			  (ClientData)rtcop);

#else

    /* _WIN32 */

    vp = &dgop->dgo_rt_cmd[0];
    *vp++ = "rtcheck";
    *vp++ = "-M";
    for (i = 1; i < argc; i++)
	*vp++ = argv[i];

    {
	char buf[512];

	snprintf(buf, 512, "\"%s\"", dgop->dgo_wdbp->dbip->dbi_filename);
	*vp++ = buf;
    }

    /*
     * Now that we've grabbed all the options, if no args remain,
     * append the names of all stuff currently displayed.
     * Otherwise, simply append the remaining args.
     */
    if (i == argc) {
	dgop->dgo_rt_cmd_len = vp - dgop->dgo_rt_cmd;
	dgop->dgo_rt_cmd_len += dgo_build_tops((struct solid *)&dgop->dgo_headSolid,
					       vp,
					       &dgop->dgo_rt_cmd[args]);
    } else {
	while (i < argc)
	    *vp++ = argv[i++];
	*vp = 0;
	vp = &dgop->dgo_rt_cmd[0];
	while (*vp)
	    Tcl_AppendResult(dgop->interp, *vp++, " ", (char *)NULL);

	Tcl_AppendResult(dgop->interp, "\n", (char *)NULL);
    }


    memset((void *)&si, 0, sizeof(STARTUPINFO));
    memset((void *)&pi, 0, sizeof(PROCESS_INFORMATION));
    memset((void *)&sa, 0, sizeof(SECURITY_ATTRIBUTES));

    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    /* Create a pipe for the child process's STDERR. */
    CreatePipe(&e_pipe[0], &e_pipe[1], &sa, 0);

    /* Create noninheritable read handle and close the inheritable read handle. */
    DuplicateHandle(GetCurrentProcess(), e_pipe[0],
		    GetCurrentProcess(),  &pipe_eDup,
		    0,  FALSE,
		    DUPLICATE_SAME_ACCESS);
    CloseHandle(e_pipe[0]);

    /* Create a pipe for the child process's STDOUT. */
    CreatePipe(&o_pipe[0], &o_pipe[1], &sa, 0);

    /* Create noninheritable write handle and close the inheritable writehandle. */
    DuplicateHandle(GetCurrentProcess(), o_pipe[1],
		    GetCurrentProcess(),  &pipe_oDup ,
		    0,  FALSE,
		    DUPLICATE_SAME_ACCESS);
    CloseHandle(o_pipe[1]);

    /* Create a pipe for the child process's STDIN. */
    CreatePipe(&i_pipe[0], &i_pipe[1], &sa, 0);

    /* Duplicate the read handle to the pipe so it is not inherited. */
    DuplicateHandle(GetCurrentProcess(), i_pipe[0],
		    GetCurrentProcess(), &pipe_iDup,
		    0, FALSE,                  /* not inherited */
		    DUPLICATE_SAME_ACCESS);
    CloseHandle(i_pipe[0]);


    si.cb = sizeof(STARTUPINFO);
    si.lpReserved = NULL;
    si.lpReserved2 = NULL;
    si.cbReserved2 = 0;
    si.lpDesktop = NULL;
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdInput   = o_pipe[0];
    si.hStdOutput  = i_pipe[1];
    si.hStdError   = e_pipe[1];
    si.wShowWindow = SW_HIDE;

    snprintf(line, sizeof(line), "%s ", dgop->dgo_rt_cmd[0]);
    for (i = 1; i < dgop->dgo_rt_cmd_len; i++) {
	snprintf(name, sizeof(name), "%s ", dgop->dgo_rt_cmd[i]);
	bu_strlcat(line, name, sizeof(line));
    }

    CreateProcess(NULL, line, NULL, NULL, TRUE,
		  DETACHED_PROCESS, NULL, NULL,
		  &si, &pi);

    /* close read end of pipe */
    CloseHandle(o_pipe[0]);

    /* close write end of pipes */
    (void)CloseHandle(i_pipe[1]);
    (void)CloseHandle(e_pipe[1]);

    /* As parent, send view information down pipe */
    fp = _fdopen(_open_osfhandle((intptr_t)pipe_oDup, _O_TEXT), "wb");
    setmode(_fileno(fp), O_BINARY);

    VSET(temp, 0.0, 0.0, 1.0);
    MAT4X3PNT(eye_model, vop->vo_view2model, temp);
    dgo_rt_write(dgop, vop, fp, eye_model);
    (void)fclose(fp);

    BU_ALLOC(rtcp, struct rtcheck);

    /* initialize the rtcheck struct */
    rtcp->fd = pipe_iDup;
    rtcp->fp = _fdopen(_open_osfhandle((intptr_t)pipe_iDup, _O_TEXT), "rb");
    setmode(_fileno(rtcp->fp), O_BINARY);
    rtcp->hProcess = pi.hProcess;
    rtcp->pid = pi.dwProcessId;
    rtcp->vbp = rt_vlblock_init();
    rtcp->vhead = rt_vlblock_find(rtcp->vbp, 0xFF, 0xFF, 0x00);
    rtcp->csize = vop->vo_scale * 0.01;
    rtcp->dgop = dgop;
    rtcp->interp = dgop->interp;

    rtcp->chan = Tcl_MakeFileChannel(pipe_iDup, TCL_READABLE);
    Tcl_CreateChannelHandler(rtcp->chan, TCL_READABLE,
			     dgo_rtcheck_vector_handler,
			     (ClientData)rtcp);

    BU_ALLOC(rtcop, struct rtcheck_output);
    rtcop->fd = pipe_eDup;
    rtcop->chan = Tcl_MakeFileChannel(pipe_eDup, TCL_READABLE);
    rtcop->dgop = dgop;
    rtcop->interp = dgop->interp;
    Tcl_CreateChannelHandler(rtcop->chan,
			     TCL_READABLE,
			     dgo_rtcheck_output_handler,
			     (ClientData)rtcop);
#endif

    bu_free(dgop->dgo_rt_cmd, "free dgo_rt_cmd");
    dgop->dgo_rt_cmd = NULL;

    return TCL_OK;
}


/*
 * Usage:
 * procname rtcheck view_obj [args]
 */
static int
dgo_rtcheck_tcl(void *clientData, int argc, const char **argv)
{
    struct dg_obj *dgop = (struct dg_obj *)clientData;
    Tcl_Interp *interp = dgop->interp;
    struct view_obj *vop;

    DGO_CHECK_WDBP_NULL(dgop, interp);

    if (argc < 3) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib_alias dgo_rtcheck %s", argv[0]);
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);

	return TCL_ERROR;
    }

    /* search for view object */
    for (BU_LIST_FOR (vop, view_obj, &HeadViewObj.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&vop->vo_name), argv[2]))
	    break;
    }

    if (BU_LIST_IS_HEAD(vop, &HeadViewObj.l)) {
	Tcl_AppendResult(interp, "dgo_rtcheck: bad view object - ", argv[2],
			 "\n", (char *)NULL);
	return TCL_ERROR;
    }

    return dgo_rtcheck_command(dgop, vop, argc-2, argv+2);
}


/*
 * Associate this drawable geometry object with a database object.
 *
 * Usage:
 * procname assoc [wdb_obj]
 */
static int
dgo_assoc_tcl(void *clientData, int argc, const char **argv)
{
    struct dg_obj *dgop = (struct dg_obj *)clientData;
    Tcl_Interp *interp = dgop->interp;
    struct rt_wdb *wdbp;
    struct bu_vls vls;

    /* Get associated database object */
    if (argc == 2) {
	if (dgop->dgo_wdbp == RT_WDB_NULL)
	    Tcl_AppendResult(interp, (char *)NULL);
	else
	    Tcl_AppendResult(interp, bu_vls_addr(&dgop->dgo_wdbp->wdb_name), (char *)NULL);
	return TCL_OK;
    }

    /* Set associated database object */
    if (argc == 3) {
	/* search for database object */
	for (BU_LIST_FOR (wdbp, rt_wdb, &RTG.rtg_headwdb.l)) {
	    if (BU_STR_EQUAL(bu_vls_addr(&wdbp->wdb_name), argv[2]))
		break;
	}

	if (BU_LIST_IS_HEAD(wdbp, &RTG.rtg_headwdb.l))
	    wdbp = RT_WDB_NULL;

	if (dgop->dgo_wdbp != RT_WDB_NULL)
	    dgo_zap_cmd(dgop);

	dgop->dgo_wdbp = wdbp;
	dgo_notify(dgop);

	return TCL_OK;
    }

    /* return help message */
    bu_vls_init(&vls);
    bu_vls_printf(&vls, "helplib_alias dgo_assoc %s", argv[0]);
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);

    return TCL_ERROR;
}


int
dgo_observer_cmd(struct dg_obj *dgop,
		 int argc,
		 const char **argv)
{
    if (argc < 2) {
	bu_log("ERROR: expecting at least two arguments\n");
	return TCL_ERROR;
    }

    return bu_observer_cmd((ClientData)&dgop->dgo_observers, argc-1, (const char **)argv+1);
}


/*
 * Attach/detach observers to/from list.
 *
 * Usage:
 * procname observer cmd [args]
 *
 */
static int
dgo_observer_tcl(void *clientData,
		 int argc,
		 const char **argv) {
    struct dg_obj *dgop = (struct dg_obj *)clientData;

    return dgo_observer_cmd(dgop, argc-1, argv+1);
}


/*
 * D G O _ P R _ S C H A I N
 *
 * Given a pointer to a member of the circularly linked list of solids
 * (typically the head), chase the list and print out the information
 * about each solid structure.
 */
static void
dgo_print_schain(struct dg_obj *dgop, int lvl)


/* debug level */
{
    struct solid *sp;
    struct bn_vlist *vp;
    int nvlist;
    int npts;
    struct bu_vls vls;

    if (dgop->dgo_wdbp->dbip == DBI_NULL)
	return;

    bu_vls_init(&vls);

    FOR_ALL_SOLIDS(sp, &dgop->dgo_headSolid) {
	if (lvl <= -2) {
	    /* print only leaves */
	    bu_vls_printf(&vls, "%s ", LAST_SOLID(sp)->d_namep);
	    continue;
	}

	db_path_to_vls(&vls, &sp->s_fullpath);

	if ((lvl != -1) && (sp->s_iflag == UP))
	    bu_vls_printf(&vls, " ILLUM");

	bu_vls_printf(&vls, "\n");

	if (lvl <= 0)
	    continue;

	/* convert to the local unit for printing */
	bu_vls_printf(&vls, "  cent=(%.3f, %.3f, %.3f) sz=%g ",
		      sp->s_center[X]*dgop->dgo_wdbp->dbip->dbi_base2local,
		      sp->s_center[Y]*dgop->dgo_wdbp->dbip->dbi_base2local,
		      sp->s_center[Z]*dgop->dgo_wdbp->dbip->dbi_base2local,
		      sp->s_size*dgop->dgo_wdbp->dbip->dbi_base2local);
	bu_vls_printf(&vls, "reg=%d\n", sp->s_regionid);
	bu_vls_printf(&vls, "  basecolor=(%d, %d, %d) color=(%d, %d, %d)%s%s%s\n",
		      sp->s_basecolor[0],
		      sp->s_basecolor[1],
		      sp->s_basecolor[2],
		      sp->s_color[0],
		      sp->s_color[1],
		      sp->s_color[2],
		      sp->s_uflag?" U":"",
		      sp->s_dflag?" D":"",
		      sp->s_cflag?" C":"");

	if (lvl <= 1)
	    continue;

	/* Print the actual vector list */
	nvlist = 0;
	npts = 0;
	for (BU_LIST_FOR (vp, bn_vlist, &(sp->s_vlist))) {
	    int i;
	    int nused = vp->nused;
	    int *cmd = vp->cmd;
	    point_t *pt = vp->pt;

	    BN_CK_VLIST(vp);
	    nvlist++;
	    npts += nused;

	    if (lvl <= 2)
		continue;

	    for (i = 0; i < nused; i++, cmd++, pt++) {
		bu_vls_printf(&vls, "  %s (%g, %g, %g)\n",
			      rt_vlist_cmd_descriptions[*cmd],
			      V3ARGS(*pt));
	    }
	}

	bu_vls_printf(&vls, "  %d vlist structures, %d pts\n", nvlist, npts);
	bu_vls_printf(&vls, "  %d pts (via rt_ck_vlist)\n", rt_ck_vlist(&(sp->s_vlist)));
    }

    Tcl_AppendResult(dgop->interp, bu_vls_addr(&vls), (char *)NULL);
    bu_vls_free(&vls);
}


/*
 * D G O _ P R _ S C H A I N _ V L C M D S
 *
 * Given a pointer to a member of the circularly linked list of solids
 * (typically the head), chase the list and print out the vlist cmds
 * for each structure.
 */
static void
dgo_print_schain_vlcmds(struct dg_obj *dgop)
{
    struct solid *sp;
    struct bn_vlist *vp;
    struct bu_vls vls;

    if (dgop->dgo_wdbp->dbip == DBI_NULL)
	return;

    bu_vls_init(&vls);

    FOR_ALL_SOLIDS(sp, &dgop->dgo_headSolid) {
	bu_vls_printf(&vls, "-1 %d %d %d\n",
		      sp->s_color[0],
		      sp->s_color[1],
		      sp->s_color[2]);

	/* Print the actual vector list */
	for (BU_LIST_FOR (vp, bn_vlist, &(sp->s_vlist))) {
	    int i;
	    int nused = vp->nused;
	    int *cmd = vp->cmd;
	    point_t *pt = vp->pt;

	    BN_CK_VLIST(vp);

	    for (i = 0; i < nused; i++, cmd++, pt++)
		bu_vls_printf(&vls, "%d %g %g %g\n", *cmd, V3ARGS(*pt));
	}
    }

    Tcl_AppendResult(dgop->interp, bu_vls_addr(&vls), (char *)NULL);
    bu_vls_free(&vls);
}


int
dgo_report_cmd(struct dg_obj *dgop,
	       int argc,
	       const char **argv)
{
    int lvl = 0;

    if (argc < 1 || 2 < argc) {
	bu_log("ERROR: expecting one or two arguments\n");
	return TCL_ERROR;
    }

    if (argc == 2)
	lvl = atoi(argv[1]);

    if (lvl <= 3)
	dgo_print_schain(dgop, lvl);
    else
	dgo_print_schain_vlcmds(dgop);

    return TCL_OK;
}


/*
 * Report information about solid table, and per-solid VLS
 */
static int
dgo_report_tcl(void *clientData, int argc, const char **argv)
{
    struct dg_obj *dgop = (struct dg_obj *)clientData;
    Tcl_Interp *interp = dgop->interp;

    DGO_CHECK_WDBP_NULL(dgop, interp);

    return dgo_report_cmd(dgop, argc-1, argv+1);
}


int
dgo_rtabort_cmd(struct dg_obj *dgop,
		int argc,
		const char **argv)
{
    struct run_rt *rrp;

    while (argc-- > 1) {
	bu_log("Unexpected argument: %s", argv[argc]);
    }

    for (BU_LIST_FOR (rrp, run_rt, &dgop->dgo_headRunRt.l)) {
	bu_terminate(rrp->pid);
	rrp->aborted = 1;
    }

    return TCL_OK;
}


static int
dgo_rtabort_tcl(void *clientData,
		int argc,
		const char **argv)
{
    struct dg_obj *dgop = (struct dg_obj *)clientData;

    return dgo_rtabort_cmd(dgop, argc-1, argv+1);
}


static int
dgo_qray_tcl(void *clientData,
	     int argc,
	     const char **argv)
{
    struct dg_obj *dgop = (struct dg_obj *)clientData;

    return dgo_qray_cmd(dgop, argc-1, argv+1);
}


static int
dgo_nirt_tcl(void *clientData,
	     int argc,
	     const char **argv)
{
    struct dg_obj *dgop = (struct dg_obj *)clientData;
    Tcl_Interp *interp = dgop->interp;
    struct view_obj *vop;

    if (argc < 3) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib_alias dgo_nirt %s", argv[0]);
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    DGO_CHECK_WDBP_NULL(dgop, interp);

    /* search for view object */
    for (BU_LIST_FOR (vop, view_obj, &HeadViewObj.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&vop->vo_name), argv[2]))
	    break;
    }

    if (BU_LIST_IS_HEAD(vop, &HeadViewObj.l)) {
	Tcl_AppendResult(interp, "dgo_nirt: bad view object - ", argv[2],
			 "\n", (char *)NULL);
	return TCL_ERROR;
    }

    return dgo_nirt_cmd(dgop, vop, argc-2, argv+2);
}


static int
dgo_vnirt_tcl(void *clientData,
	      int argc,
	      const char **argv)
{
    struct dg_obj *dgop = (struct dg_obj *)clientData;
    Tcl_Interp *interp = dgop->interp;
    struct view_obj *vop;

    if (argc < 5) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib_alias dgo_vnirt %s", argv[0]);
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    DGO_CHECK_WDBP_NULL(dgop, interp);

    /* search for view object */
    for (BU_LIST_FOR (vop, view_obj, &HeadViewObj.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&vop->vo_name), argv[2]))
	    break;
    }

    if (BU_LIST_IS_HEAD(vop, &HeadViewObj.l)) {
	Tcl_AppendResult(interp, "dgo_vnirt: bad view object - ", argv[2],
			 "\n", (char *)NULL);
	return TCL_ERROR;
    }

    return dgo_vnirt_cmd(dgop, vop, argc-2, argv+2);
}


int
dgo_set_outputHandler_cmd(struct dg_obj *dgop,
			  int argc,
			  const char **argv)
{
    struct bu_vls vls;

    if (argc < 1 || 2 < argc) {
	bu_log("ERROR: expecting one or two arguments\n");
	return TCL_ERROR;
    }

    /* Get the output handler script */
    if (argc == 1) {
	bu_vls_init(&vls);
	if (dgop->dgo_outputHandler != NULL)
	    bu_vls_strcat(&vls, dgop->dgo_outputHandler);
	Tcl_SetResult(dgop->interp, bu_vls_addr(&vls), TCL_VOLATILE);
	bu_vls_free(&vls);

	return TCL_OK;
    }

    /* We're now going to set the output handler script */
    /* First, we zap any previous script */
    if (dgop->dgo_outputHandler != NULL) {
	bu_free((genptr_t)dgop->dgo_outputHandler, "dgo_set_outputHandler: zap");
	dgop->dgo_outputHandler = NULL;
    }

    if (argv[1] != NULL && argv[1][0] != '\0')
	dgop->dgo_outputHandler = bu_strdup(argv[1]);

    return TCL_OK;
}


/*
 * Sets/gets the output handler.
 *
 * Usage:
 * procname set_outputHandler [script]
 */
static int
dgo_set_outputHandler_tcl(void *clientData,
			  int argc,
			  const char **argv)
{
    struct dg_obj *dgop = (struct dg_obj *)clientData;

    return dgo_set_outputHandler_cmd(dgop, argc-1, argv+1);
}


int
dgo_set_uplotOutputMode_cmd(struct dg_obj *dgop,
			    int argc,
			    const char **argv)
{
    struct bu_vls vls;

    if (argc < 1 || 2 < argc) {
	bu_log("ERROR: expecting one or two arguments\n");
	return TCL_ERROR;
    }

    /* Get the plot output mode */
    if (argc == 1) {
	bu_vls_init(&vls);

	if (dgop->dgo_uplotOutputMode == PL_OUTPUT_MODE_BINARY)
	    bu_vls_strcat(&vls, "binary");
	else
	    bu_vls_strcat(&vls, "text");
	Tcl_SetResult(dgop->interp, bu_vls_addr(&vls), TCL_VOLATILE);
	bu_vls_free(&vls);

	return TCL_OK;
    }

    if (argv[1][0] == 'b' &&
	BU_STR_EQUAL("binary", argv[1]))
	dgop->dgo_uplotOutputMode = PL_OUTPUT_MODE_BINARY;
    else if (argv[1][0] == 't' &&
	     BU_STR_EQUAL("text", argv[1]))
	dgop->dgo_uplotOutputMode = PL_OUTPUT_MODE_TEXT;
    else {
	bu_log("ERROR: unexpected plot output mode\n");
	return TCL_ERROR;
    }

    return TCL_OK;
}


/*
 * Sets/gets the plot output mode.
 *
 * Usage:
 * procname set_uplotOutput mode [omode]
 */
static int
dgo_set_uplotOutputMode_tcl(void *clientData,
			    int argc,
			    const char **argv)
{
    struct dg_obj *dgop = (struct dg_obj *)clientData;

    return dgo_set_uplotOutputMode_cmd(dgop, argc-1, argv+1);
}


int
dgo_set_transparency_cmd(struct dg_obj *dgop,
			 int argc,
			 const char **argv)
{
    struct solid *sp;
    size_t i;
    struct directory **dpp;
    struct directory **tmp_dpp;
    double transparency;


    if (argc != 3) {
	bu_log("ERROR: expecting three arguments\n");
	return TCL_ERROR;
    }

    if (sscanf(argv[2], "%lf", &transparency) != 1) {
	bu_log("dgo_set_transparency: bad transparency - %s\n", argv[2]);
	return TCL_ERROR;
    }

    if ((dpp = dgo_build_dpp(dgop, argv[1])) == NULL) {
	return TCL_OK;
    }

    FOR_ALL_SOLIDS(sp, &dgop->dgo_headSolid) {
	for (i = 0, tmp_dpp = dpp;
	     i < sp->s_fullpath.fp_len && *tmp_dpp != RT_DIR_NULL;
	     ++i, ++tmp_dpp) {
	    if (sp->s_fullpath.fp_names[i] != *tmp_dpp)
		break;
	}

	if (*tmp_dpp != RT_DIR_NULL)
	    continue;

	/* found a match */
	sp->s_transparency = transparency;
    }

    if (dpp != (struct directory **)NULL)
	bu_free((genptr_t)dpp, "dgo_set_transparency_cmd: directory pointers");

    return TCL_OK;
}


/*
 * Sets the transparency of obj.
 *
 * Usage:
 * procname set_transparency obj t
 */
static int
dgo_set_transparency_tcl(void *clientData,
			 int argc,
			 const char **argv)
{
    struct dg_obj *dgop = (struct dg_obj *)clientData;
    int ret;

    if ((ret = dgo_set_transparency_cmd(dgop, argc-1, argv+1)) == TCL_OK)
	dgo_notify(dgop);

    return ret;
}


int
dgo_shaded_mode_cmd(struct dg_obj *dgop,
		    int argc,
		    const char **argv)
{
    struct bu_vls vls = BU_VLS_INIT_ZERO;

    /* get shaded mode */
    if (argc == 1) {
	bu_vls_printf(&vls, "%d", dgop->dgo_shaded_mode);
	Tcl_AppendResult(dgop->interp, bu_vls_addr(&vls), (char *)0);
	bu_vls_free(&vls);
	return TCL_OK;
    }

    /* set shaded mode */
    if (argc == 2) {
	int shaded_mode;

	if (sscanf(argv[1], "%d", &shaded_mode) != 1)
	    goto bad;

	if (shaded_mode < 0 || 2 < shaded_mode)
	    goto bad;

	dgop->dgo_shaded_mode = shaded_mode;
	return TCL_OK;
    }

bad:
    bu_log("ERROR: bad shaded mode value\n");
    return TCL_ERROR;
}


/*
 * Usage:
 * procname shaded_mode [m]
 */
static int
dgo_shaded_mode_tcl(void *clientData,
		    int argc,
		    const char **argv)
{
    struct dg_obj *dgop = (struct dg_obj *)clientData;

    return dgo_shaded_mode_cmd(dgop, argc-1, argv+1);
}


/****************** Utility Routines ********************/

static union tree *
dgo_wireframe_region_end(struct db_tree_state *UNUSED(tsp), const struct db_full_path *UNUSED(pathp), union tree *curtree, genptr_t UNUSED(client_data))
{
    return curtree;
}


/**
 * Compute the min, max, and center points of the solid.  Also finds
 * s_vlen.
 *
 * XXX Should split out a separate bn_vlist_rpp() routine, for
 * librt/vlist.c
 */
static void
dgo_bound_solid(Tcl_Interp *interp, struct solid *sp)
{
    struct bn_vlist *vp;
    double xmax, ymax, zmax;
    double xmin, ymin, zmin;

    xmax = ymax = zmax = -INFINITY;
    xmin = ymin = zmin =  INFINITY;
    sp->s_vlen = 0;
    for (BU_LIST_FOR (vp, bn_vlist, &(sp->s_vlist))) {
	int j;
	int nused = vp->nused;
	int *cmd = vp->cmd;
	point_t *pt = vp->pt;
	for (j = 0; j < nused; j++, cmd++, pt++) {
	    switch (*cmd) {
		case BN_VLIST_POLY_START:
		case BN_VLIST_POLY_VERTNORM:
		case BN_VLIST_TRI_START:
		case BN_VLIST_TRI_VERTNORM:
		case BN_VLIST_POINT_SIZE:
		case BN_VLIST_LINE_WIDTH:
		    /* attribute, not location */
		    break;
		case BN_VLIST_LINE_MOVE:
		case BN_VLIST_LINE_DRAW:
		case BN_VLIST_POLY_MOVE:
		case BN_VLIST_POLY_DRAW:
		case BN_VLIST_POLY_END:
		case BN_VLIST_TRI_MOVE:
		case BN_VLIST_TRI_DRAW:
		case BN_VLIST_TRI_END:
		    V_MIN(xmin, (*pt)[X]);
		    V_MAX(xmax, (*pt)[X]);
		    V_MIN(ymin, (*pt)[Y]);
		    V_MAX(ymax, (*pt)[Y]);
		    V_MIN(zmin, (*pt)[Z]);
		    V_MAX(zmax, (*pt)[Z]);
		    break;
		case BN_VLIST_POINT_DRAW:
		    V_MIN(xmin, (*pt)[X]-1.0);
		    V_MAX(xmax, (*pt)[X]+1.0);
		    V_MIN(ymin, (*pt)[Y]-1.0);
		    V_MAX(ymax, (*pt)[Y]+1.0);
		    V_MIN(zmin, (*pt)[Z]-1.0);
		    V_MAX(zmax, (*pt)[Z]+1.0);
		    break;
		default: {
		    struct bu_vls tmp_vls;

		    bu_vls_init(&tmp_vls);
		    bu_vls_printf(&tmp_vls, "unknown vlist op %d\n", *cmd);
		    Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
		    bu_vls_free(&tmp_vls);
		}
	    }
	}
	sp->s_vlen += nused;
    }

    sp->s_center[X] = (xmin + xmax) * 0.5;
    sp->s_center[Y] = (ymin + ymax) * 0.5;
    sp->s_center[Z] = (zmin + zmax) * 0.5;

    sp->s_size = xmax - xmin;
    V_MAX(sp->s_size, ymax - ymin);
    V_MAX(sp->s_size, zmax - zmin);
}


/*
 * D M O _ D R A W h _ P A R T 2
 *
 * Once the vlist has been created, perform the common tasks
 * in handling the drawn solid.
 *
 * This routine must be prepared to run in parallel.
 */
void
dgo_drawH_part2(int dashflag, struct bu_list *vhead, const struct db_full_path *pathp, struct db_tree_state *tsp, struct solid *existing_sp, struct dg_client_data *dgcdp)
{
    struct solid *sp;

    if (!existing_sp) {
	/* Handling a new solid */
	GET_SOLID(sp, &_FreeSolid.l);
	/* NOTICE:  The structure is dirty & not initialized for you! */

	if (BU_LIST_NON_EMPTY(&dgcdp->dgop->dgo_headSolid)) {
	    sp->s_dlist = BU_LIST_LAST(solid, &dgcdp->dgop->dgo_headSolid)->s_dlist + 1;
	} else
	    sp->s_dlist = 1;
    } else {
	/* Just updating an existing solid.
	 * 'tsp' and 'pathpos' will not be used
	 */
	sp = existing_sp;
    }


    /*
     * Compute the min, max, and center points.
     */
    BU_LIST_APPEND_LIST(&(sp->s_vlist), vhead);
    dgo_bound_solid(dgcdp->interp, sp);

    /*
     * If this solid is new, fill in its information.
     * Otherwise, don't touch what is already there.
     */
    if (!existing_sp) {
	/* Take note of the base color */
	if (dgcdp->wireframe_color_override) {
	    /* a user specified the color, so arrange to use it */
	    sp->s_uflag = 1;
	    sp->s_dflag = 0;
	    sp->s_basecolor[0] = dgcdp->wireframe_color[0];
	    sp->s_basecolor[1] = dgcdp->wireframe_color[1];
	    sp->s_basecolor[2] = dgcdp->wireframe_color[2];
	} else {
	    sp->s_uflag = 0;
	    if (tsp) {
		if (tsp->ts_mater.ma_color_valid) {
		    sp->s_dflag = 0;	/* color specified in db */
		    sp->s_basecolor[0] = tsp->ts_mater.ma_color[0] * 255.0;
		    sp->s_basecolor[1] = tsp->ts_mater.ma_color[1] * 255.0;
		    sp->s_basecolor[2] = tsp->ts_mater.ma_color[2] * 255.0;
		} else {
		    sp->s_dflag = 1;	/* default color */
		    sp->s_basecolor[0] = 255;
		    sp->s_basecolor[1] = 0;
		    sp->s_basecolor[2] = 0;
		}
	    }
	}
	sp->s_cflag = 0;
	sp->s_flag = DOWN;
	sp->s_iflag = DOWN;
	sp->s_soldash = dashflag;
	sp->s_Eflag = 0;	/* This is a solid */
	db_dup_full_path(&sp->s_fullpath, pathp);
	if (tsp)
	   sp->s_regionid = tsp->ts_regionid;
	sp->s_transparency = dgcdp->transparency;
	sp->s_dmode = dgcdp->dmode;

	/* Add to linked list of solid structs */
	bu_semaphore_acquire(RT_SEM_MODEL);
	BU_LIST_APPEND(dgcdp->dgop->dgo_headSolid.back, &sp->l);
	bu_semaphore_release(RT_SEM_MODEL);
    }
}


/*
 * D G O _ W I R E F R A M E _ L E A F
 *
 * This routine must be prepared to run in parallel.
 */
static union tree *
dgo_wireframe_leaf(struct db_tree_state *tsp, const struct db_full_path *pathp, struct rt_db_internal *ip, genptr_t client_data)
{
    union tree *curtree;
    int dashflag;		/* draw with dashed lines */
    struct bu_list vhead;
    struct dg_client_data *dgcdp = (struct dg_client_data *)client_data;

    RT_CK_TESS_TOL(tsp->ts_ttol);
    BN_CK_TOL(tsp->ts_tol);
    RT_CK_RESOURCE(tsp->ts_resp);

    BU_LIST_INIT(&vhead);

    if (RT_G_DEBUG&DEBUG_TREEWALK) {
	char *sofar = db_path_to_string(pathp);

	Tcl_AppendResult(dgcdp->interp, "dgo_wireframe_leaf(",
			 ip->idb_meth->ft_name,
			 ") path='", sofar, "'\n", (char *)NULL);
	bu_free((genptr_t)sofar, "path string");
    }

    if (dgcdp->draw_solid_lines_only)
	dashflag = 0;
    else
	dashflag = (tsp->ts_sofar & (TS_SOFAR_MINUS|TS_SOFAR_INTER));

    RT_CK_DB_INTERNAL(ip);

    if (ip->idb_meth->ft_plot(&vhead, ip,
			      tsp->ts_ttol,
			      tsp->ts_tol,
			      NULL) < 0) {
	Tcl_AppendResult(dgcdp->interp, DB_FULL_PATH_CUR_DIR(pathp)->d_namep,
			 ": plot failure\n", (char *)NULL);
	return TREE_NULL;		/* ERROR */
    }

    /*
     * XXX HACK CTJ - dgo_drawH_part2 sets the default color of a
     * solid by looking in tps->ts_mater.ma_color, for pseudo
     * solids, this needs to be something different and drawH
     * has no idea or need to know what type of solid this is.
     */
    if (ip->idb_type == ID_GRIP) {
	int r, g, b;
	r= tsp->ts_mater.ma_color[0];
	g= tsp->ts_mater.ma_color[1];
	b= tsp->ts_mater.ma_color[2];
	tsp->ts_mater.ma_color[0] = 0;
	tsp->ts_mater.ma_color[1] = 128;
	tsp->ts_mater.ma_color[2] = 128;
	dgo_drawH_part2(dashflag, &vhead, pathp, tsp, SOLID_NULL, dgcdp);
	tsp->ts_mater.ma_color[0] = r;
	tsp->ts_mater.ma_color[1] = g;
	tsp->ts_mater.ma_color[2] = b;
    } else {
	dgo_drawH_part2(dashflag, &vhead, pathp, tsp, SOLID_NULL, dgcdp);
    }

    /* Indicate success by returning something other than TREE_NULL */
    RT_GET_TREE(curtree, tsp->ts_resp);
    curtree->tr_op = OP_NOP;

    return curtree;
}


/*
 * D G O _ N M G _ R E G I O N _ S T A R T
 *
 * When performing "ev" on a region, consider whether to process
 * the whole subtree recursively.
 * Normally, say "yes" to all regions by returning 0.
 *
 * Check for special case:  a region of one solid, which can be
 * directly drawn as polygons without going through NMGs.
 * If we draw it here, then return -1 to signal caller to ignore
 * further processing of this region.
 * A hack to view polygonal models (converted from FASTGEN) more rapidly.
 */
static int
dgo_nmg_region_start(struct db_tree_state *tsp, const struct db_full_path *pathp, const struct rt_comb_internal *combp, genptr_t client_data)
{
    union tree *tp;
    struct directory *dp;
    struct rt_db_internal intern;
    mat_t xform;
    matp_t matp;
    struct bu_list vhead;
    struct dg_client_data *dgcdp = (struct dg_client_data *)client_data;

    if (RT_G_DEBUG&DEBUG_TREEWALK) {
	char *sofar = db_path_to_string(pathp);
	bu_log("dgo_nmg_region_start(%s)\n", sofar);
	bu_free((genptr_t)sofar, "path string");
	rt_pr_tree(combp->tree, 1);
	db_pr_tree_state(tsp);
    }

    RT_CK_DBI(tsp->ts_dbip);
    RT_CK_RESOURCE(tsp->ts_resp);

    BU_LIST_INIT(&vhead);

    RT_CK_COMB(combp);
    tp = combp->tree;
    if (!tp)
	return -1;
    RT_CK_TREE(tp);
    if (tp->tr_l.tl_op != OP_DB_LEAF)
	return 0;	/* proceed as usual */

    /* The subtree is a single node.  It may be a combination, though */

    /* Fetch by name, check to see if it's an easy type */
    dp = db_lookup(tsp->ts_dbip, tp->tr_l.tl_name, LOOKUP_NOISY);
    if (!dp)
	return 0;	/* proceed as usual */
    if (!bn_mat_is_identity(tsp->ts_mat)) {
	if (tp->tr_l.tl_mat) {
	    matp = xform;
	    bn_mat_mul(xform, tsp->ts_mat, tp->tr_l.tl_mat);
	} else {
	    matp = tsp->ts_mat;
	}
    } else {
	if (tp->tr_l.tl_mat) {
	    matp = tp->tr_l.tl_mat;
	} else {
	    matp = (matp_t)NULL;
	}
    }
    if (rt_db_get_internal(&intern, dp, tsp->ts_dbip, matp, &rt_uniresource) < 0)
	return 0;	/* proceed as usual */

    switch (intern.idb_type) {
	case ID_POLY: {
	    if (RT_G_DEBUG&DEBUG_TREEWALK) {
		bu_log("fastpath draw ID_POLY %s\n", dp->d_namep);
	    }
	    if (dgcdp->draw_wireframes) {
		(void)rt_pg_plot(&vhead, &intern, tsp->ts_ttol, tsp->ts_tol, NULL);
	    } else {
		(void)rt_pg_plot_poly(&vhead, &intern, tsp->ts_ttol, tsp->ts_tol);
	    }
	}
	    goto out;
	case ID_BOT: {
	    if (RT_G_DEBUG&DEBUG_TREEWALK) {
		bu_log("fastpath draw ID_BOT %s\n", dp->d_namep);
	    }
	    if (dgcdp->draw_wireframes) {
		(void)rt_bot_plot(&vhead, &intern, tsp->ts_ttol, tsp->ts_tol, NULL);
	    } else {
		(void)rt_bot_plot_poly(&vhead, &intern, tsp->ts_ttol, tsp->ts_tol);
	    }
	}
	    goto out;
	case ID_COMBINATION:
	default:
	    break;
    }
    rt_db_free_internal(&intern);
    return 0;

out:
    {
	struct db_full_path pp;
	db_full_path_init(&pp);
	db_dup_full_path(&pp, pathp);

	/* Successful fastpath drawing of this solid */
	db_add_node_to_full_path(&pp, dp);
	dgo_drawH_part2(0, &vhead, &pp, tsp, SOLID_NULL, dgcdp);

	db_free_full_path(&pp);
    }

    rt_db_free_internal(&intern);
    dgcdp->fastpath_count++;
    return -1;	/* SKIP THIS REGION */
}


static int
process_boolean(union tree *curtree, struct db_tree_state *tsp, const struct db_full_path *pathp, Tcl_Interp *interp)
{
    int result = 1;

    if (!BU_SETJUMP) {
	/* try */

	result = nmg_boolean(curtree, *tsp->ts_m, tsp->ts_tol, tsp->ts_resp);

    } else {
	/* catch */
	char *sofar = db_path_to_string(pathp);

	Tcl_AppendResult(interp, "WARNING: Boolean evaluation of ", sofar,
			 " failed!!!\n", (char *)NULL);
	bu_free((genptr_t)sofar, "path string");
    } BU_UNSETJUMP;

    return result;
}


static int
process_triangulation(struct db_tree_state *tsp, const struct db_full_path *pathp, Tcl_Interp *interp)
{
    int result = 1;

    if (!BU_SETJUMP) {
	/* try */

	nmg_triangulate_model(*tsp->ts_m, tsp->ts_tol);
	result = 0;

    } else {
	/* catch */

	char *sofar = db_path_to_string(pathp);

	Tcl_AppendResult(interp, "WARNING: Triangulation of ", sofar,
			 " failed!!!\n", (char *)NULL);
	bu_free((genptr_t)sofar, "path string");

    } BU_UNSETJUMP;

    return result;
}


/*
 * D G O _ N M G _ R E G I O N _ E N D
 *
 * This routine must be prepared to run in parallel.
 */
static union tree *
dgo_nmg_region_end(struct db_tree_state *tsp, const struct db_full_path *pathp, union tree *curtree, genptr_t client_data)
{
    struct nmgregion *r;
    struct bu_list vhead;
    int failed;
    struct dg_client_data *dgcdp = (struct dg_client_data *)client_data;

    RT_CK_TESS_TOL(tsp->ts_ttol);
    BN_CK_TOL(tsp->ts_tol);
    NMG_CK_MODEL(*tsp->ts_m);
    RT_CK_RESOURCE(tsp->ts_resp);

    BU_LIST_INIT(&vhead);

    if (RT_G_DEBUG&DEBUG_TREEWALK) {
	char *sofar = db_path_to_string(pathp);

	Tcl_AppendResult(dgcdp->interp, "dgo_nmg_region_end() path='", sofar,
			 "'\n", (char *)NULL);
	bu_free((genptr_t)sofar, "path string");
    } else {
	char *sofar = db_path_to_string(pathp);

	bu_log("%s:\n", sofar);
	bu_free((genptr_t)sofar, "path string");
    }

    if (curtree->tr_op == OP_NOP) return curtree;

    if (!dgcdp->draw_nmg_only) {

	failed = process_boolean(curtree, tsp, pathp, dgcdp->interp);
	if (failed) {
	    db_free_tree(curtree, tsp->ts_resp);
	    return (union tree *)NULL;
	}

    } else if (curtree->tr_op != OP_NMG_TESS) {
	Tcl_AppendResult(dgcdp->interp, "Cannot use '-d' option when Boolean evaluation is required\n", (char *)NULL);
	db_free_tree(curtree, tsp->ts_resp);
	return (union tree *)NULL;
    }
    r = curtree->tr_d.td_r;
    NMG_CK_REGION(r);

    if (dgcdp->do_not_draw_nmg_solids_during_debugging && r) {
	db_free_tree(curtree, tsp->ts_resp);
	return (union tree *)NULL;
    }

    if (dgcdp->nmg_triangulate) {
	failed = process_triangulation(tsp, pathp, dgcdp->interp);
	if (failed) {
	    db_free_tree(curtree, tsp->ts_resp);
	    return (union tree *)NULL;
	}
    }

    if (r != 0) {
	int style;
	/* Convert NMG to vlist */
	NMG_CK_REGION(r);

	if (dgcdp->draw_wireframes) {
	    /* Draw in vector form */
	    style = NMG_VLIST_STYLE_VECTOR;
	} else {
	    /* Default -- draw polygons */
	    style = NMG_VLIST_STYLE_POLYGON;
	}
	if (dgcdp->draw_normals) {
	    style |= NMG_VLIST_STYLE_VISUALIZE_NORMALS;
	}
	if (dgcdp->shade_per_vertex_normals) {
	    style |= NMG_VLIST_STYLE_USE_VU_NORMALS;
	}
	if (dgcdp->draw_no_surfaces) {
	    style |= NMG_VLIST_STYLE_NO_SURFACES;
	}
	nmg_r_to_vlist(&vhead, r, style);

	dgo_drawH_part2(0, &vhead, pathp, tsp, SOLID_NULL, dgcdp);

	if (dgcdp->draw_edge_uses) {
	    nmg_vlblock_r(dgcdp->draw_edge_uses_vbp, r, 1);
	}
	/* NMG region is no longer necessary, only vlist remains */
	db_free_tree(curtree, tsp->ts_resp);
	return (union tree *)NULL;
    }

    /* Return tree -- it needs to be freed (by caller) */
    return curtree;
}


static union tree *
dgo_bot_check_region_end(struct db_tree_state *UNUSED(tsp),
			 const struct db_full_path *UNUSED(pathp),
			 union tree *curtree,
			 genptr_t UNUSED(client_data))
{
    return curtree;
}


static union tree *
dgo_bot_check_leaf(struct db_tree_state *tsp,
		   const struct db_full_path *pathp,
		   struct rt_db_internal *ip,
		   genptr_t client_data)
{
    union tree *curtree;
    int ac = 1;
    const char *av[2];
    struct dg_client_data *dgcdp = (struct dg_client_data *)client_data;

    av[0] = db_path_to_string(pathp);
    av[1] = (char *)0;

    /* Indicate success by returning something other than TREE_NULL */
    RT_GET_TREE(curtree, tsp->ts_resp);
    curtree->tr_op = OP_NOP;

    /*
     * Use dgop->dgo_shaded_mode if set and not being overridden. Otherwise use dgcdp->shaded_mode_override.
     */

    switch (dgcdp->dmode) {
	case DGO_SHADED_MODE_BOTS:
	    if (ip->idb_major_type == DB5_MAJORTYPE_BRLCAD &&
		ip->idb_minor_type == DB5_MINORTYPE_BRLCAD_BOT) {
		struct bu_list vhead;

		BU_LIST_INIT(&vhead);

		(void)rt_bot_plot_poly(&vhead, ip, tsp->ts_ttol, tsp->ts_tol);
		dgo_drawH_part2(0, &vhead, pathp, tsp, SOLID_NULL, dgcdp);
	    } else if (ip->idb_major_type == DB5_MAJORTYPE_BRLCAD &&
		       ip->idb_minor_type == DB5_MINORTYPE_BRLCAD_POLY) {
		struct bu_list vhead;

		BU_LIST_INIT(&vhead);

		(void)rt_pg_plot_poly(&vhead, ip, tsp->ts_ttol, tsp->ts_tol);
		dgo_drawH_part2(0, &vhead, pathp, tsp, SOLID_NULL, dgcdp);
	    } else {
		/* save shaded mode states */
		int save_dgo_shaded_mode = dgcdp->dgop->dgo_shaded_mode;
		int save_shaded_mode_override = dgcdp->shaded_mode_override;
		int save_dmode = dgcdp->dmode;

		/* turn shaded mode off for this non-bot/non-poly object */
		dgcdp->dgop->dgo_shaded_mode = 0;
		dgcdp->shaded_mode_override = -1;
		dgcdp->dmode = DGO_WIREFRAME;

		dgo_drawtrees(dgcdp->dgop, ac, av, 1, client_data);

		/* restore shaded mode states */
		dgcdp->dgop->dgo_shaded_mode = save_dgo_shaded_mode;
		dgcdp->shaded_mode_override = save_shaded_mode_override;
		dgcdp->dmode = save_dmode;
	    }

	    break;
	case DGO_SHADED_MODE_ALL:
	    if (ip->idb_major_type == DB5_MAJORTYPE_BRLCAD &&
		ip->idb_minor_type != DB5_MINORTYPE_BRLCAD_PIPE) {
		if (ip->idb_minor_type == DB5_MINORTYPE_BRLCAD_BOT) {
		    struct bu_list vhead;

		    BU_LIST_INIT(&vhead);

		    (void)rt_bot_plot_poly(&vhead, ip, tsp->ts_ttol, tsp->ts_tol);
		    dgo_drawH_part2(0, &vhead, pathp, tsp, SOLID_NULL, dgcdp);
		} else if (ip->idb_minor_type == DB5_MINORTYPE_BRLCAD_POLY) {
		    struct bu_list vhead;

		    BU_LIST_INIT(&vhead);

		    (void)rt_pg_plot_poly(&vhead, ip, tsp->ts_ttol, tsp->ts_tol);
		    dgo_drawH_part2(0, &vhead, pathp, tsp, SOLID_NULL, dgcdp);
		} else
		    dgo_drawtrees(dgcdp->dgop, ac, av, 3, client_data);
	    } else {
		/* save shaded mode states */
		int save_dgo_shaded_mode = dgcdp->dgop->dgo_shaded_mode;
		int save_shaded_mode_override = dgcdp->shaded_mode_override;
		int save_dmode = dgcdp->dmode;

		/* turn shaded mode off for this pipe object */
		dgcdp->dgop->dgo_shaded_mode = 0;
		dgcdp->shaded_mode_override = -1;
		dgcdp->dmode = DGO_WIREFRAME;

		dgo_drawtrees(dgcdp->dgop, ac, av, 1, client_data);

		/* restore shaded mode states */
		dgcdp->dgop->dgo_shaded_mode = save_dgo_shaded_mode;
		dgcdp->shaded_mode_override = save_shaded_mode_override;
		dgcdp->dmode = save_dmode;
	    }

	    break;
    }

    bu_free((genptr_t)av[0], "dgo_bot_check_leaf: av[0]");

    return curtree;
}


/*
 * D G O _ D R A W T R E E S
 *
 * This routine is the drawable geometry object's analog of rt_gettrees().
 * Add a set of tree hierarchies to the active set.
 * Note that argv[0] should be ignored, it has the command name in it.
 *
 * Kind =
 * 1 regular wireframes
 * 2 big-E
 * 3 NMG polygons
 *
 * Returns -
 * 0 Ordinarily
 * -1 On major error
 */
static int
dgo_drawtrees(struct dg_obj *dgop, int argc, const char **argv, int kind, struct dg_client_data *_dgcdp)
{
    int ret = 0;
    int c;
    int ncpu = 1;
    int dgo_nmg_use_tnurbs = 0;
    int dgo_enable_fastpath = 0;
    struct model *dgo_nmg_model;
    struct dg_client_data *dgcdp;
    RT_CHECK_DBI(dgop->dgo_wdbp->dbip);

    if (argc <= 0)
	return -1;	/* FAIL */

    /* options are already parsed into _dgcdp */
    if (_dgcdp != (struct dg_client_data *)0) {
	BU_ALLOC(dgcdp, struct dg_client_data);
	*dgcdp = *_dgcdp;            /* struct copy */
    } else {

	BU_ALLOC(dgcdp, struct dg_client_data);
	dgcdp->dgop = dgop;
	dgcdp->interp = dgop->interp;

	/* Initial values for options, must be reset each time */
	dgcdp->draw_nmg_only = 0;	/* no booleans */
	dgcdp->nmg_triangulate = 1;
	dgcdp->draw_wireframes = 0;
	dgcdp->draw_normals = 0;
	dgcdp->draw_solid_lines_only = 0;
	dgcdp->draw_no_surfaces = 0;
	dgcdp->shade_per_vertex_normals = 0;
	dgcdp->draw_edge_uses = 0;
	dgcdp->wireframe_color_override = 0;
	dgcdp->fastpath_count = 0;

	/* default color - red */
	dgcdp->wireframe_color[0] = 255;
	dgcdp->wireframe_color[1] = 0;
	dgcdp->wireframe_color[2] = 0;

	/* default transparency - opaque */
	dgcdp->transparency = 1.0;

	/* -1 indicates flag not set */
	dgcdp->shaded_mode_override = -1;

	dgo_enable_fastpath = 0;

	/* Parse options. */
	bu_optind = 0;		/* re-init bu_getopt() */
	while ((c = bu_getopt(argc, (char * const *)argv, "dfm:nqstuvwx:C:STP:")) != -1) {
	    switch (c) {
		case 'u':
		    dgcdp->draw_edge_uses = 1;
		    break;
		case 's':
		    dgcdp->draw_solid_lines_only = 1;
		    break;
		case 't':
		    dgo_nmg_use_tnurbs = 1;
		    break;
		case 'v':
		    dgcdp->shade_per_vertex_normals = 1;
		    break;
		case 'w':
		    dgcdp->draw_wireframes = 1;
		    break;
		case 'S':
		    dgcdp->draw_no_surfaces = 1;
		    break;
		case 'T':
		    dgcdp->nmg_triangulate = 0;
		    break;
		case 'n':
		    dgcdp->draw_normals = 1;
		    break;
		case 'P':
		    ncpu = atoi(bu_optarg);
		    break;
		case 'q':
		    dgcdp->do_not_draw_nmg_solids_during_debugging = 1;
		    break;
		case 'd':
		    dgcdp->draw_nmg_only = 1;
		    break;
		case 'f':
		    dgo_enable_fastpath = 1;
		    break;
		case 'C': {
		    int r, g, b;
		    char *cp = bu_optarg;

		    r = atoi(cp);
		    while ((*cp >= '0' && *cp <= '9')) cp++;
		    while (*cp && (*cp < '0' || *cp > '9')) cp++;
		    g = atoi(cp);
		    while ((*cp >= '0' && *cp <= '9')) cp++;
		    while (*cp && (*cp < '0' || *cp > '9')) cp++;
		    b = atoi(cp);

		    if (r < 0 || r > 255) r = 255;
		    if (g < 0 || g > 255) g = 255;
		    if (b < 0 || b > 255) b = 255;

		    dgcdp->wireframe_color_override = 1;
		    dgcdp->wireframe_color[0] = r;
		    dgcdp->wireframe_color[1] = g;
		    dgcdp->wireframe_color[2] = b;
		}
		    break;
		case 'm':
		    /* clamp it to [-infinity, 2] */
		    dgcdp->shaded_mode_override = atoi(bu_optarg);
		    if (2 < dgcdp->shaded_mode_override)
			dgcdp->shaded_mode_override = 2;

		    break;
		case 'x':
		    dgcdp->transparency = atof(bu_optarg);

		    /* clamp it to [0, 1] */
		    if (dgcdp->transparency < 0.0)
			dgcdp->transparency = 0.0;

		    if (1.0 < dgcdp->transparency)
			dgcdp->transparency = 1.0;

		    break;
		default: {
		    bu_log("ERROR: unexpected tree drawing option\n");
		    bu_free((genptr_t)dgcdp, "dgo_drawtrees: dgcdp");

		    return TCL_ERROR;
		}
	    }
	}
	argc -= bu_optind;
	argv += bu_optind;

	switch (kind) {
	    case 1:
		if (dgop->dgo_shaded_mode && dgcdp->shaded_mode_override < 0) {
		    dgcdp->dmode = dgop->dgo_shaded_mode;
		} else if (0 <= dgcdp->shaded_mode_override)
		    dgcdp->dmode = dgcdp->shaded_mode_override;
		else
		    dgcdp->dmode = DGO_WIREFRAME;

		break;
	    case 2:
	    case 3:
		dgcdp->dmode = DGO_BOOL_EVAL;
		break;
	}

    }

    switch (kind) {
	default:
	    bu_log("ERROR: bad kind\n");
	    bu_free((genptr_t)dgcdp, "dgo_drawtrees: dgcdp");
	    return -1;
	case 1:		/* Wireframes */
	    /*
	     * If asking for wireframe and in shaded_mode and no shaded mode override,
	     * or asking for wireframe and shaded mode is being overridden with a value
	     * greater than 0, then draw shaded polygons for each object's primitives if possible.
	     *
	     * Note -
	     * If shaded_mode is DGO_SHADED_MODE_BOTS, only BOTS and polysolids
	     * will be shaded. The rest is drawn as wireframe.
	     * If shaded_mode is DGO_SHADED_MODE_ALL, everything except pipe solids
	     * are drawn as shaded polygons.
	     */
	    if (DGO_SHADED_MODE_BOTS <= dgcdp->dmode && dgcdp->dmode <= DGO_SHADED_MODE_ALL) {
		int i;
		int ac = 1;
		const char *av[2];

		av[1] = (char *)0;

		for (i = 0; i < argc; ++i) {

		    av[0] = argv[i];

		    ret = db_walk_tree(dgop->dgo_wdbp->dbip,
				       ac,
				       (const char **)av,
				       ncpu,
				       &dgop->dgo_wdbp->wdb_initial_tree_state,
				       0,
				       dgo_bot_check_region_end,
				       dgo_bot_check_leaf,
				       (genptr_t)dgcdp);
		}
	    } else
		ret = db_walk_tree(dgop->dgo_wdbp->dbip,
				   argc,
				   (const char **)argv,
				   ncpu,
				   &dgop->dgo_wdbp->wdb_initial_tree_state,
				   0,			/* take all regions */
				   dgo_wireframe_region_end,
				   dgo_wireframe_leaf,
				   (genptr_t)dgcdp);
	    break;
	case 2:		/* Big-E */
	    Tcl_AppendResult(dgop->interp, "drawtrees:  can't do big-E here\n", (char *)NULL);
	    bu_free((genptr_t)dgcdp, "dgo_drawtrees: dgcdp");
	    return -1;
	case 3: {
	    /* NMG */
	    dgo_nmg_model = nmg_mm();
	    dgop->dgo_wdbp->wdb_initial_tree_state.ts_m = &dgo_nmg_model;
	    if (dgcdp->draw_edge_uses) {
		Tcl_AppendResult(dgop->interp, "Doing the edgeuse thang (-u)\n", (char *)NULL);
		dgcdp->draw_edge_uses_vbp = rt_vlblock_init();
	    }

	    ret = db_walk_tree(dgop->dgo_wdbp->dbip, argc, (const char **)argv,
			       ncpu,
			       &dgop->dgo_wdbp->wdb_initial_tree_state,
			       dgo_enable_fastpath ? dgo_nmg_region_start : 0,
			       dgo_nmg_region_end,
			       dgo_nmg_use_tnurbs ? nmg_booltree_leaf_tnurb : nmg_booltree_leaf_tess,
			       (genptr_t)dgcdp);

	    if (dgcdp->draw_edge_uses) {
		dgo_cvt_vlblock_to_solids(dgop, dgcdp->draw_edge_uses_vbp, "_EDGEUSES_", 0);
		rt_vlblock_free(dgcdp->draw_edge_uses_vbp);
		dgcdp->draw_edge_uses_vbp = (struct bn_vlblock *)NULL;
	    }

	    /* Destroy NMG */
	    nmg_km(dgo_nmg_model);
	    break;
	}
    }
    if (dgcdp->fastpath_count) {
	bu_log("%d region%s rendered through polygon fastpath\n",
	       dgcdp->fastpath_count, dgcdp->fastpath_count == 1 ? "" : "s");
    }

    bu_free((genptr_t)dgcdp, "dgo_drawtrees: dgcdp");

    if (ret < 0)
	return -1;

    return 0;	/* OK */
}


/*
 * I N V E N T _ S O L I D
 *
 * Invent a solid by adding a fake entry in the database table,
 * adding an entry to the solid table, and populating it with
 * the given vector list.
 *
 * This parallels much of the code in dodraw.c
 */
int
dgo_invent_solid(struct dg_obj *dgop,
		 char *name,
		 struct bu_list *vhead,
		 long int rgb,
		 int copy,
		 fastf_t transparency,
		 int dmode)
{
    struct directory *dp;
    struct directory *dpp[2] = {RT_DIR_NULL, RT_DIR_NULL};
    struct solid *sp;
    unsigned char type='0';

    if (dgop->dgo_wdbp->dbip == DBI_NULL)
	return 0;

    if ((dp = db_lookup(dgop->dgo_wdbp->dbip, name, LOOKUP_QUIET)) != RT_DIR_NULL) {
	if (dp->d_addr != RT_DIR_PHONY_ADDR) {
	    bu_log("dgo_invent_solid(%s) would clobber existing database entry, ignored\n", name);
	    return -1;
	}

	/*
	 * Name exists from some other overlay,
	 * zap any associated solids
	 */
	dpp[0] = dp;
	dgo_eraseobjall(dgop, dpp);
    }
    /* Need to enter phony name in directory structure */
    dp = db_diradd(dgop->dgo_wdbp->dbip,  name, RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (genptr_t)&type);

    /* Obtain a fresh solid structure, and fill it in */
    GET_SOLID(sp, &_FreeSolid.l);

    if (copy) {
	BU_LIST_INIT(&(sp->s_vlist));
	rt_vlist_copy(&(sp->s_vlist), vhead);
    } else {
	/* For efficiency, just swipe the vlist */
	BU_LIST_APPEND_LIST(&(sp->s_vlist), vhead);
	BU_LIST_INIT(vhead);
    }
    dgo_bound_solid(dgop->interp, sp);

    /* set path information -- this is a top level node */
    db_add_node_to_full_path(&sp->s_fullpath, dp);

    sp->s_iflag = DOWN;
    sp->s_soldash = 0;
    sp->s_Eflag = 1;		/* Can't be solid edited! */
    sp->s_color[0] = sp->s_basecolor[0] = (rgb>>16) & 0xFF;
    sp->s_color[1] = sp->s_basecolor[1] = (rgb>> 8) & 0xFF;
    sp->s_color[2] = sp->s_basecolor[2] = (rgb) & 0xFF;
    sp->s_regionid = 0;

    if (BU_LIST_NON_EMPTY(&dgop->dgo_headSolid)) {
	sp->s_dlist = BU_LIST_LAST(solid, &dgop->dgo_headSolid)->s_dlist + 1;
    } else
	sp->s_dlist = 1;

    sp->s_uflag = 0;
    sp->s_dflag = 0;
    sp->s_cflag = 0;
    sp->s_wflag = 0;

    sp->s_transparency = transparency;
    sp->s_dmode = dmode;

    /* Solid successfully drawn, add to linked list of solid structs */
    BU_LIST_APPEND(dgop->dgo_headSolid.back, &sp->l);

    return 0;		/* OK */
}


void
dgo_notifyWdb(struct rt_wdb *wdbp)
{
    struct dg_obj *dgop;

    for (BU_LIST_FOR (dgop, dg_obj, &HeadDGObj.l))
	if (dgop->dgo_wdbp == wdbp)
	    dgo_notify(dgop);
}


void
dgo_impending_wdb_close(struct rt_wdb *wdbp)
{
    struct dg_obj *dgop;

    for (BU_LIST_FOR (dgop, dg_obj, &HeadDGObj.l))
	if (dgop->dgo_wdbp == wdbp) {
	    dgo_zap_cmd(dgop);
	    dgop->dgo_wdbp = RT_WDB_NULL;
	    dgo_notify(dgop);
	}
}


void
dgo_zapall(struct rt_wdb *wdbp)
{
    struct dg_obj *dgop;

    for (BU_LIST_FOR (dgop, dg_obj, &HeadDGObj.l))
	if (dgop->dgo_wdbp == wdbp) {
	    dgo_zap_cmd(dgop);
	    dgo_notify(dgop);
	}
}


/**
 *
 *
 */
int
dgo_tree_cmd(struct dg_obj *dgop,
	     int argc,
	     const char **argv)
{
    struct directory *dp;
    int j;
    int cflag = 0;
    int indentSize = -1;
    int displayDepth = INT_MAX;
    int c;
    FILE *fdout = NULL;
    char *buffer = NULL;
#define WHOARGVMAX 256
    char *whoargv[WHOARGVMAX+1] = {0};

    if (!dgop || !dgop->dgo_wdbp || dgop->dgo_wdbp->dbip == DBI_NULL)
	return TCL_ERROR;

    if (argc < 1) {
	bu_log("ERROR: expecting one or more arguments\n");
	return TCL_ERROR;
    }

    /* Parse options */
    bu_optind = 1;	/* re-init bu_getopt() */
    while ((c=bu_getopt(argc, (char * const *)argv, "d:i:o:c")) != -1) {
	switch (c) {
	    case 'i':
		indentSize = atoi(bu_optarg);
		break;
	    case 'c':
		cflag = 1;
		break;
	    case 'o':
		if (fdout) {
		    bu_log("Warning - output file already specified");
		    break;
		}
		if ((fdout = fopen(bu_optarg, "w+b")) == NULL) {
		    Tcl_SetErrno(errno);
		    bu_log("Failed to open output file, %s", strerror(errno));
		    return TCL_ERROR;
		}
		break;
	    case 'd':
		displayDepth = atoi(bu_optarg);
		if (displayDepth < 0) {
		    bu_log("Negative number supplied as depth - unsupported.");
		    if (fdout)
			fclose(fdout);
		    return TCL_ERROR;
		}
		break;
	    case '?':
	    default:
		bu_log("ERROR: unexpected tree option\n");
		if (fdout)
		    fclose(fdout);
		return TCL_ERROR;
		break;
	}
    }

    argc -= (bu_optind - 1);
    argv += (bu_optind - 1);

    /* tree of all displayed objects */
    if (argc == 1) {
	const char *whocmd[2] = {"who", NULL};
	if (dgo_who_cmd(dgop, 1, whocmd) == TCL_OK) {
	    const char *result = Tcl_GetStringResult(dgop->interp);
	    buffer = bu_strdup(result);
	    Tcl_ResetResult(dgop->interp);

	    argc += bu_argv_from_string(whoargv, WHOARGVMAX, buffer);
	}
    }

    for (j = 1; j < argc; j++) {
	const char *next = argv[j];
	if (buffer) {
	    next = whoargv[j-1];
	}

	if (j > 1)
	    Tcl_AppendResult(dgop->interp, "\n", (char *)NULL);
	if ((dp = db_lookup(dgop->dgo_wdbp->dbip, next, LOOKUP_NOISY)) == RT_DIR_NULL)
	    continue;
	wdb_print_node(dgop->dgo_wdbp, dp, 0, indentSize, 0, cflag, displayDepth, 0);
    }

    if (buffer) {
	bu_free(buffer, "free who buffer");
	buffer = NULL;
    }

    if (fdout) {
	fprintf(fdout, "%s", Tcl_GetStringResult(dgop->interp));
	Tcl_ResetResult(dgop->interp);
	fclose(fdout);
    }

    return TCL_OK;
}


/**
 * Usage:
 * procname tree object(s)
 */
static int
dgo_tree_tcl(void *clientData, int argc, const char **argv)
{
    struct dg_obj *dgop = (struct dg_obj *)clientData;

    return dgo_tree_cmd(dgop, argc-1, argv+1);
}


/*
 * D G O _ C M D
 *
 * Generic interface for drawable geometry objects.
 * Usage:
 * procname cmd ?args?
 *
 * Returns: result of dgo command.
 */
int
dgo_cmd(void *clientData, Tcl_Interp *UNUSED(interp), int argc, const char **argv)
{
    int ret;

    static struct bu_cmdtab dgo_cmds[] = {
	{"assoc",		dgo_assoc_tcl},
	{"autoview",		dgo_autoview_tcl},
	{"blast",		dgo_blast_tcl},
	{"clear",		dgo_zap_tcl},
	{"draw",		dgo_draw_tcl},
	{"E",			dgo_E_tcl},
	{"erase",		dgo_erase_tcl},
	{"erase_all",		dgo_erase_all_tcl},
	{"ev",			dgo_ev_tcl},
	{"get_autoview",	dgo_get_autoview_tcl},
	{"get_eyemodel",	dgo_get_eyemodel_tcl},
	{"headSolid",		dgo_headSolid_tcl},
	{"how",			dgo_how_tcl},
	{"illum",		dgo_illum_tcl},
	{"label",		dgo_label_tcl},
	{"nirt",		dgo_nirt_tcl},
	{"observer",		dgo_observer_tcl},
	{"overlay",		dgo_overlay_tcl},
	{"qray",		dgo_qray_tcl},
	{"report",		dgo_report_tcl},
	{"rt",			dgo_rt_tcl},
	{"rtabort",		dgo_rtabort_tcl},
	{"rtcheck",		dgo_rtcheck_tcl},
	{"rtedge",		dgo_rt_tcl},
	{"set_outputHandler",	dgo_set_outputHandler_tcl},
	{"set_uplotOutputMode",	dgo_set_uplotOutputMode_tcl},
	{"set_transparency",	dgo_set_transparency_tcl},
	{"shaded_mode",		dgo_shaded_mode_tcl},
	{"tree",		dgo_tree_tcl},
	{"vdraw",		dgo_vdraw_tcl},
	{"vnirt",		dgo_vnirt_tcl},
	{"who",			dgo_who_tcl},
	{"zap",			dgo_zap_tcl},
	{(char *)0,		(int (*)())0}
    };

    if (bu_cmd(dgo_cmds, argc, argv, 1, clientData, &ret) == BRLCAD_OK)
	return ret;

    bu_log("ERROR: '%s' command not found\n", argv[1]);
    return BRLCAD_ERROR;
}


/*
 * Called by Tcl when the object is destroyed.
 */
void
dgo_deleteProc(void *clientData)
{
    struct dg_obj *dgop = (struct dg_obj *)clientData;

    /* free observers */
    bu_observer_free(&dgop->dgo_observers);

    /*
     * XXX Do something for the case where the drawable geometry
     * XXX object is deleted and the object has forked rt processes.
     * XXX This will create a memory leak.
     */


    bu_vls_free(&dgop->dgo_name);
    dgo_free_qray(dgop);

    BU_LIST_DEQUEUE(&dgop->l);
    bu_free((genptr_t)dgop, "dgo_deleteProc: dgop");
}


/*
 * Open/create a drawable geometry object that's associated with the
 * database object "rt_wdb".
 *
 * USAGE:
 * dgo_open [name rt_wdb]
 */
static int
dgo_open_tcl(ClientData UNUSED(clientData),
	     Tcl_Interp *interp,
	     int argc,
	     const char **argv)
{
    struct dg_obj *dgop;
    struct rt_wdb *wdbp;
    struct bu_vls vls;

    if (argc == 1) {
	/* get list of drawable geometry objects */
	for (BU_LIST_FOR (dgop, dg_obj, &HeadDGObj.l))
	    Tcl_AppendResult(interp, bu_vls_addr(&dgop->dgo_name), " ", (char *)NULL);

	return TCL_OK;
    }

    if (argc != 3) {
	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib dgo_open");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    /* search for database object */
    for (BU_LIST_FOR (wdbp, rt_wdb, &RTG.rtg_headwdb.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&wdbp->wdb_name), argv[2]))
	    break;
    }

    if (BU_LIST_IS_HEAD(wdbp, &RTG.rtg_headwdb.l))
	wdbp = RT_WDB_NULL;

    /* first, delete any commands by this name */
    (void)Tcl_DeleteCommand(interp, argv[1]);

    dgop = dgo_open_cmd(argv[1], wdbp);
    dgop->interp = interp;
    (void)Tcl_CreateCommand(interp,
			    bu_vls_addr(&dgop->dgo_name),
			    (Tcl_CmdProc *)dgo_cmd,
			    (ClientData)dgop,
			    dgo_deleteProc);

    /* Return new function name as result */
    Tcl_ResetResult(interp);
    Tcl_AppendResult(interp, bu_vls_addr(&dgop->dgo_name), (char *)NULL);

    return TCL_OK;
}


int
Dgo_Init(Tcl_Interp *interp)
{
    BU_LIST_INIT(&HeadDGObj.l);
    BU_LIST_INIT(&_FreeSolid.l);

    (void)Tcl_CreateCommand(interp, "dg_open", (Tcl_CmdProc *)dgo_open_tcl, (ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);

    return TCL_OK;
}


/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
