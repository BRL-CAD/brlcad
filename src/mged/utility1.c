/*                      U T I L I T Y 1 . C
 * BRL-CAD
 *
 * Copyright (c) 1990-2011 United States Government as represented by
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
#include "bio.h"

#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "nmg.h"
#include "raytrace.h"
#include "db.h"

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
 * E D I T I T
 *
 * No-frills edit - opens an editor on the supplied
 * file name.
 *
 */
int
editit(const char *command, const char *tempfile) {
    int argc = 5;
    const char *av[6] = {NULL, NULL, NULL, NULL, NULL, NULL};
    struct bu_vls editstring;

    CHECK_DBI_NULL;

    bu_vls_init(&editstring);
    get_editor_string(&editstring);

    av[0] = command;
    av[1] = "-e"; 
    av[2] = bu_vls_addr(&editstring);
    av[3] = "-f";
    av[4] = tempfile;
    av[5] = NULL;

    ged_editit(gedp, argc, (const char **)av);

    bu_vls_free(&editstring);
    return TCL_OK;
}


/*
 *
 * F _ E D C O L O R ()
 *
 * control routine for editing color
 */
int
f_edcolor(ClientData UNUSED(clientData), Tcl_Interp *UNUSED(interpreter), int argc, const char *argv[])
{
    const char **av;
    int i;
    struct bu_vls editstring;

    CHECK_DBI_NULL;

    bu_vls_init(&editstring);
    get_editor_string(&editstring);

    av = (const char **)bu_malloc(sizeof(char *)*(argc + 3), "f_edcolor: av");
    av[0] = argv[0];
    av[1] = "-E";
    av[2] = bu_vls_addr(&editstring);
    argc += 2;
    for (i = 3; i < argc; ++i) {
	av[i] = argv[i-2];
    }
    av[argc] = NULL;
  
    ged_edcolor(gedp, argc, (const char **)av);
    
    bu_vls_free(&editstring); 
    bu_free((genptr_t)av, "f_edcolor: av");
    return TCL_OK;
}


/*
 * control routine for editing region ident codes
 */
int
f_edcodes(ClientData UNUSED(clientData), Tcl_Interp *interpreter, int argc, const char *argv[])
{
    const char **av;
    struct bu_vls editstring;
    int i;

    CHECK_DBI_NULL;

    if (argc < 2) {
	Tcl_Eval(interpreter, "help edcodes");
	return TCL_ERROR;
    }

    bu_vls_init(&editstring);
    get_editor_string(&editstring);

    av = (const char **)bu_malloc(sizeof(char *)*(argc + 3), "f_edcodes: av");
    av[0] = argv[0];
    av[1] = "-E";
    av[2] = bu_vls_addr(&editstring);
    argc += 2;
    for (i = 3; i < argc; ++i) {
	av[i] = argv[i-2];
    }
    av[argc] = NULL;
  
    ged_edcodes(gedp, argc + 1, (const char **)av);
   
    bu_vls_free(&editstring); 
    bu_free((genptr_t)av, "f_edcodes: av");
    return TCL_OK;
}


/*
 *
 * F _ E D M A T E R ()
 *
 * control routine for editing mater information
 */
int
f_edmater(ClientData UNUSED(clientData), Tcl_Interp *interpreter, int argc, const char *argv[])
{
    const char **av;
    struct bu_vls editstring;
    int i;

    CHECK_DBI_NULL;

    if (argc < 2) {
	Tcl_Eval(interpreter, "help edmater");
	return TCL_ERROR;
    }

    bu_vls_init(&editstring);
    get_editor_string(&editstring);

    av = (const char **)bu_malloc(sizeof(char *)*(argc + 3), "f_edmater: av");
    av[0] = (const char *)argv[0];
    av[1] = "-E";
    av[2] = bu_vls_addr(&editstring);
    argc += 2;
    for (i = 3; i < argc; ++i) {
	av[i] = (const char *)argv[i-2];
    }
    av[argc] = NULL;

    ged_edmater(gedp, argc + 1, (const char **)av);
   
    bu_vls_free(&editstring); 
    bu_free((genptr_t)av, "f_edmater: av");
    return TCL_OK;
}


/*
 *
 * F _ R E D ()
 *
 * Get editing string and call ged_red
 */
int
f_red(ClientData UNUSED(clientData), Tcl_Interp *interpreter, int argc, const char *argv[])
{
    const char **av;
    struct bu_vls editstring;
    int i;

    CHECK_DBI_NULL;

    if (argc < 2) {
	Tcl_Eval(interpreter, "help red");
	return TCL_ERROR;
    }

    bu_vls_init(&editstring);
    get_editor_string(&editstring);

    av = (const char **)bu_malloc(sizeof(char *)*(argc + 3), "f_red: av");
    av[0] = argv[0];
    av[1] = "-E";
    av[2] = bu_vls_addr(&editstring);
    argc += 2;
    for (i = 3; i < argc; ++i) {
	av[i] = argv[i-2];
    }
    av[argc] = NULL;

    ged_red(gedp, argc + 1, (const char **)av);
   
    bu_vls_free(&editstring); 
    bu_free((genptr_t)av, "f_red: av");
    return TCL_OK;
}


/* cyclic, for db_tree_funcleaf in printcodes() */
HIDDEN void Do_printnode(struct db_i *dbip2, struct rt_comb_internal *comb, union tree *comb_leaf, genptr_t user_ptr1, genptr_t user_ptr2, genptr_t user_ptr3);


HIDDEN int
printcodes(FILE *fp, struct directory *dp, int pathpos)
{
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

    if ((id=rt_db_get_internal(&intern, dp, dbip, (matp_t)NULL, &rt_uniresource)) < 0) {
	Tcl_AppendResult(INTERP, "printcodes: Cannot get records for ",
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
	db_tree_funcleaf(dbip, comb, comb->tree, Do_printnode,
			 (genptr_t)fp, (genptr_t)&pathpos, (genptr_t)NULL);
    }

    intern.idb_meth->ft_ifree(&intern);
    return TCL_OK;
}


HIDDEN void
Do_printnode(struct db_i *dbip2, struct rt_comb_internal *UNUSED(comb), union tree *comb_leaf, genptr_t user_ptr1, genptr_t user_ptr2, genptr_t UNUSED(user_ptr3))
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
