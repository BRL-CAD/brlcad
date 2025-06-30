/*                      U T I L I T Y 1 . C
 * BRL-CAD
 *
 * Copyright (c) 1990-2025 United States Government as represented by
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

#include "common.h"

#include <stdlib.h>
#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#ifdef HAVE_PWD_H
#  include <pwd.h>
#endif
#include <signal.h>
#include <math.h>
#include <time.h>
#include <string.h>

#include "vmath.h"
#include "bn.h"
#include "nmg.h"
#include "raytrace.h"
#include "rt/db4.h"

#include "./mged.h"
#include "./sedit.h"
#include "./cmd.h"


#define ABORTED -99
#define OLDSOLID 0
#define NEWSOLID 1
#define SOL_TABLE 1
#define REG_TABLE 2
#define ID_TABLE 3

#define LINELEN 256
#define MAX_LEVELS 12
struct directory *path[MAX_LEVELS];

/* structure to distinguish new solids from existing (old) solids */
struct identt {
    int i_index;
    char i_name[NAMESIZE+1];
    mat_t i_mat;
};
struct identt identt, idbuf;

char operate;
int regflag, lastmemb, old_or_new, oper_ok;
long numsol;
long numreg;
int idfd, rd_idfd;
FILE *tabptr;

char ctemp[7];


/*
 *
 * No-frills edit - opens an editor on the supplied
 * file name.
 *
 */
int
editit(struct mged_state *s, const char *command, const char *tempfile) {
    int argc = 3;
    const char *av[6] = {NULL, NULL, NULL, NULL, NULL, NULL};

    CHECK_DBI_NULL;

    if (!get_editor(s))
	return TCL_ERROR;

    av[0] = command;
    av[1] = "-f";
    av[2] = tempfile;
    av[3] = NULL;

    ged_exec(s->gedp, argc, (const char **)av);

    clear_editor(s);
    return TCL_OK;
}


/*
 *
 * control routine for editing color
 */
int
f_edcolor(ClientData clientData, Tcl_Interp *UNUSED(interpreter), int argc, const char *argv[])
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;

    CHECK_DBI_NULL;

    if (!get_editor(s))
	return TCL_ERROR;

    ged_exec(s->gedp, argc, argv);

    clear_editor(s);
    return TCL_OK;
}


/*
 * control routine for editing region ident codes
 */
int
f_edcodes(ClientData clientData, Tcl_Interp *interpreter, int argc, const char *argv[])
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;

    CHECK_DBI_NULL;

    if (argc < 2) {
	Tcl_Eval(interpreter, "help edcodes");
	return TCL_ERROR;
    }

    if (!get_editor(s))
	return TCL_ERROR;

    ged_exec(s->gedp, argc, argv);

    clear_editor(s);
    return TCL_OK;
}


/*
 *
 * control routine for editing mater information
 */
int
f_edmater(ClientData clientData, Tcl_Interp *interpreter, int argc, const char *argv[])
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;

    CHECK_DBI_NULL;

    if (argc < 2) {
	Tcl_Eval(interpreter, "help edmater");
	return TCL_ERROR;
    }

    if (!get_editor(s))
	return TCL_ERROR;

    ged_exec(s->gedp, argc, argv);

    clear_editor(s);
    return TCL_OK;
}


/*
 *
 * Get editing string and call ged_red
 */
int
f_red(ClientData clientData, Tcl_Interp *interpreter, int argc, const char *argv[])
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;

    CHECK_DBI_NULL;

    if (argc != 2) {
	Tcl_Eval(interpreter, "help red");
	return TCL_ERROR;
    }

    get_editor(s);

    if (ged_exec(s->gedp, argc, argv) & BRLCAD_ERROR) {
	mged_pr_output(interpreter);
	Tcl_AppendResult(interpreter, "Error: ", bu_vls_addr(s->gedp->ged_result_str), (char *)NULL);
    } else {
	mged_pr_output(interpreter);
	Tcl_AppendResult(interpreter, bu_vls_addr(s->gedp->ged_result_str), (char *)NULL);
    }

    clear_editor(s);
    return TCL_OK;
}


/* cyclic, for db_tree_funcleaf in printcodes() */
static void Do_printnode(struct db_i *dbip2, struct rt_comb_internal *comb, union tree *comb_leaf, void *user_ptr1, void *user_ptr2, void *user_ptr3, void *UNUSED(user_ptr4));


static int
printcodes(FILE *fp, struct directory *dp, int pathpos)
{
    struct mged_state *s = MGED_STATE;
    int i;
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;
    int id;

    CHECK_DBI_NULL;

    if (pathpos >= MAX_LEVELS) {
	regflag = ABORTED;
	return TCL_ERROR;
    }

    if (!(dp->d_flags & RT_DIR_COMB))
	return 0;

    if ((id=rt_db_get_internal(&intern, dp, s->dbip, (matp_t)NULL, &rt_uniresource)) < 0) {
	Tcl_AppendResult(s->interp, "printcodes: Cannot get records for ",
			 dp->d_namep, "\n", (char *)NULL);
	return TCL_ERROR;
    }

    if (id != ID_COMBINATION)
	return TCL_OK;

    comb = (struct rt_comb_internal *)intern.idb_ptr;
    RT_CK_COMB(comb);

    if (comb->region_flag) {
	fprintf(fp, "%-6ld %-3ld %-3ld %-4ld  ",
		comb->region_id,
		comb->aircode,
		comb->GIFTmater,
		comb->los);
	for (i=0; i < pathpos; i++)
	    fprintf(fp, "/%s", path[i]->d_namep);
	fprintf(fp, "/%s\n", dp->d_namep);
	intern.idb_meth->ft_ifree(&intern);
	return TCL_OK;
    }

    if (comb->tree) {
	path[pathpos] = dp;
	db_tree_funcleaf(s->dbip, comb, comb->tree, Do_printnode,
			 (void *)fp, (void *)&pathpos, (void *)NULL, (void *)NULL);
    }

    intern.idb_meth->ft_ifree(&intern);
    return TCL_OK;
}


static void
Do_printnode(struct db_i *dbip2, struct rt_comb_internal *UNUSED(comb), union tree *comb_leaf, void *user_ptr1, void *user_ptr2, void *UNUSED(user_ptr3), void *UNUSED(user_ptr4))
{
    FILE *fp;
    int *pathpos;
    struct directory *nextdp;

    RT_CK_DBI(dbip2);
    RT_CK_TREE(comb_leaf);

    if ((nextdp=db_lookup(dbip2, comb_leaf->tr_l.tl_name, LOOKUP_NOISY)) == RT_DIR_NULL)
	return;

    fp = (FILE *)user_ptr1;
    pathpos = (int *)user_ptr2;

    /* recurse on combinations */
    if (nextdp->d_flags & RT_DIR_COMB)
	(void)printcodes(fp, nextdp, (*pathpos)+1);
}


/*
 * compares solids returns 1 if they match or  0 otherwise
 */
int
check(char *a, char *b)
{

    int c= sizeof(struct identt);

    while (c--) if (*a++ != *b++) return 0;	/* no match */
    return 1;	/* match */

}


struct id_names {
    struct bu_list l;
    struct bu_vls name;		/* name associated with region id */
};


struct id_to_names {
    struct bu_list l;
    int id;				/* starting id (i.e. region id or air code) */
    struct id_names headName;	/* head of list of names */
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
