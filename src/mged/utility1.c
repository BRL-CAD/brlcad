/*                      U T I L I T Y 1 . C
 * BRL-CAD
 *
 * Copyright (c) 1990-2010 United States Government as represented by
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
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "help edcodes");
	Tcl_Eval(interpreter, bu_vls_addr(&vls));
	bu_vls_free(&vls);
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
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "help edmater");
	Tcl_Eval(interpreter, bu_vls_addr(&vls));
	bu_vls_free(&vls);
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
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "help red");
	Tcl_Eval(interpreter, bu_vls_addr(&vls));
	bu_vls_free(&vls);
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

    if (!(dp->d_flags & DIR_COMB))
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

    if ((nextdp=db_lookup(dbip2, comb_leaf->tr_l.tl_name, LOOKUP_NOISY)) == DIR_NULL)
	return;

    fp = (FILE *)user_ptr1;
    pathpos = (int *)user_ptr2;

    /* recurse on combinations */
    if (nextdp->d_flags & DIR_COMB)
	(void)printcodes(fp, nextdp, (*pathpos)+1);
}


/* write codes to a file */
int
f_wcodes(ClientData UNUSED(clientData), Tcl_Interp *interpreter, int argc, const char *argv[])
{
    int i;
    int status;
    FILE *fp;
    struct directory *dp;

    CHECK_DBI_NULL;

    if (argc < 3) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "help wcodes");
	Tcl_Eval(interpreter, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    if ((fp = fopen(argv[1], "w")) == NULL) {
	Tcl_AppendResult(interpreter, "f_wcodes: Failed to open file - ", argv[1], (char *)NULL);
	return TCL_ERROR;
    }

    regflag = lastmemb = 0;
    for (i = 2; i < argc; ++i) {
	if ((dp = db_lookup(dbip, argv[i], LOOKUP_NOISY)) != DIR_NULL) {
	    status = printcodes(fp, dp, 0);

	    if (status == TCL_ERROR) {
		(void)fclose(fp);
		return TCL_ERROR;
	    }
	}
    }

    (void)fclose(fp);
    return TCL_OK;
}


/* read codes from a file and load them into the database */
int
f_rcodes(ClientData UNUSED(clientData), Tcl_Interp *interpreter, int argc, const char *argv[])
{
    int item, air, mat, los;
    char name[256];
    char line[LINELEN];
    char *cp;
    FILE *fp;
    struct directory *dp;
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;

    CHECK_DBI_NULL;
    CHECK_READ_ONLY;

    if (argc < 2 || 2 < argc) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "help rcodes");
	Tcl_Eval(interpreter, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    if ((fp = fopen(argv[1], "r")) == NULL) {
	Tcl_AppendResult(interpreter, "f_rcodes: Failed to read file - ", argv[1], (char *)NULL);
	return TCL_ERROR;
    }

    while (bu_fgets(line, LINELEN, fp) != NULL) {
	int changed;

	if (sscanf(line, "%d%d%d%d%256s", &item, &air, &mat, &los, name) != 5)
	    continue; /* not useful */

	/* skip over the path */
	if ((cp = strrchr(name, (int)'/')) == NULL)
	    cp = name;
	else
	    ++cp;

	if (*cp == '\0')
	    continue;

	if ((dp = db_lookup(dbip, cp, LOOKUP_NOISY)) == DIR_NULL) {
	    Tcl_AppendResult(interpreter, "f_rcodes: Warning - ", cp, " not found in database.\n",
			     (char *)NULL);
	    continue;
	}

	if (!(dp->d_flags & DIR_REGION)) {
	    Tcl_AppendResult(interpreter, "f_rcodes: Warning ", cp, " not a region\n", (char *)NULL);
	    continue;
	}

	if (rt_db_get_internal(&intern, dp, dbip, (matp_t)NULL, &rt_uniresource) != ID_COMBINATION) {
	    Tcl_AppendResult(interpreter, "f_rcodes: Warning ", cp, " not a region\n", (char *)NULL);
	    continue;
	}

	comb = (struct rt_comb_internal *)intern.idb_ptr;

	/* make the changes */
	changed = 0;
	if (comb->region_id != item) {
	    comb->region_id = item;
	    changed = 1;
	}
	if (comb->aircode != air) {
	    comb->aircode = air;
	    changed = 1;
	}
	if (comb->GIFTmater != mat) {
	    comb->GIFTmater = mat;
	    changed = 1;
	}
	if (comb->los != los) {
	    comb->los = los;
	    changed = 1;
	}

	if (changed) {
	    /* write out all changes */
	    if (rt_db_put_internal(dp, dbip, &intern, &rt_uniresource)) {
		Tcl_AppendResult(interpreter, "Database write error, aborting.\n",
				 (char *)NULL);
		TCL_ERROR_RECOVERY_SUGGESTION;
		rt_db_free_internal(&intern);
		return TCL_ERROR;
	    }
	}

    }

    return TCL_OK;
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


HIDDEN int
sol_number(const matp_t matrix, char *name, int *old)
{
    int i;
    struct identt idbuf1, idbuf2;
    int readval;

    memset(&idbuf1, 0, sizeof(struct identt));
    bu_strlcpy(idbuf1.i_name, name, sizeof(idbuf1.i_name));
    MAT_COPY(idbuf1.i_mat, matrix);

    for (i=0; i<numsol; i++) {
	(void)lseek(rd_idfd, i*(long)sizeof identt, 0);
	readval = read(rd_idfd, &idbuf2, sizeof identt);

	if (readval < 0) {
	    perror("READ ERROR");
	}

	idbuf1.i_index = i + 1;

	if (check((char *)&idbuf1, (char *)&idbuf2) == 1) {
	    *old = 1;
	    return idbuf2.i_index;
	}
    }
    numsol++;
    idbuf1.i_index = numsol;

    (void)lseek(idfd, (off_t)0L, 2);
    (void)write(idfd, &idbuf1, sizeof identt);

    *old = 0;
    return idbuf1.i_index;
}


HIDDEN void
new_tables(struct directory *dp, struct bu_ptbl *cur_path, const matp_t old_mat, int flag)
{
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;
    struct rt_tree_array *tree_list;
    int node_count;
    int actual_count;
    int i, k;

    if (dbip == DBI_NULL)
	return;

    RT_CK_DIR(dp);
    BU_CK_PTBL(cur_path);

    if (!(dp->d_flags & DIR_COMB))
	return;

    if (rt_db_get_internal(&intern, dp, dbip, (fastf_t *)NULL, &rt_uniresource) < 0)
	READ_ERR_return;

    comb = (struct rt_comb_internal *)intern.idb_ptr;
    RT_CK_COMB(comb);

    if (comb->tree && db_ck_v4gift_tree(comb->tree) < 0) {
	db_non_union_push(comb->tree, &rt_uniresource);
	if (db_ck_v4gift_tree(comb->tree) < 0) {
	    Tcl_AppendResult(INTERP, "Cannot flatten tree for editing\n", (char *)NULL);
	    intern.idb_meth->ft_ifree(&intern);
	    return;
	}
    }

    if (!comb->tree) {
	/* empty combination */
	intern.idb_meth->ft_ifree(&intern);
	return;
    }

    node_count = db_tree_nleaves(comb->tree);
    tree_list = (struct rt_tree_array *)bu_calloc(node_count,
						  sizeof(struct rt_tree_array), "tree list");

    /* flatten tree */
    actual_count = (struct rt_tree_array *)db_flatten_tree(tree_list,
							   comb->tree, OP_UNION, 0, &rt_uniresource) - tree_list;
    BU_ASSERT_LONG(actual_count, ==, node_count);

    if (dp->d_flags & DIR_REGION) {
	numreg++;
	(void)fprintf(tabptr, " %-4ld %4ld %4ld %4ld %4ld  ",
		      numreg, comb->region_id, comb->aircode, comb->GIFTmater,
		      comb->los);
	for (k=0; k<BU_PTBL_END(cur_path); k++) {
	    struct directory *path_dp;

	    path_dp = (struct directory *)BU_PTBL_GET(cur_path, k);
	    RT_CK_DIR(path_dp);
	    (void)fprintf(tabptr, "/%s", path_dp->d_namep);
	}
	(void)fprintf(tabptr, "/%s:\n", dp->d_namep);

	if (flag == ID_TABLE)
	    goto out;

	for (i=0; i<actual_count; i++) {
	    char op;
	    int nsoltemp=0;
	    struct rt_db_internal sol_intern;
	    struct directory *sol_dp;
	    mat_t temp_mat;
	    struct bu_vls tmp_vls;
	    int old;

	    switch (tree_list[i].tl_op) {
		case OP_UNION:
		    op = 'u';
		    break;
		case OP_SUBTRACT:
		    op = '-';
		    break;
		case OP_INTERSECT:
		    op = '+';
		    break;
		default:
		    bu_log("unrecognized operation in region %s\n", dp->d_namep);
		    op = '?';
		    break;
	    }

	    if ((sol_dp=db_lookup(dbip, tree_list[i].tl_tree->tr_l.tl_name, LOOKUP_QUIET)) != DIR_NULL) {
		if (sol_dp->d_flags & DIR_COMB) {
		    (void)fprintf(tabptr, "   RG %c %s\n",
				  op, sol_dp->d_namep);
		    continue;
		} else if (!(sol_dp->d_flags & DIR_SOLID)) {
		    (void)fprintf(tabptr, "   ?? %c %s\n",
				  op, sol_dp->d_namep);
		    continue;
		} else {
		    if (tree_list[i].tl_tree->tr_l.tl_mat) {
			bn_mat_mul(temp_mat, old_mat,
				   tree_list[i].tl_tree->tr_l.tl_mat);
		    } else {
			MAT_COPY(temp_mat, old_mat);
		    }
		    if (rt_db_get_internal(&sol_intern, sol_dp, dbip, temp_mat, &rt_uniresource) < 0) {
			bu_log("Could not import %s\n", tree_list[i].tl_tree->tr_l.tl_name);
			nsoltemp = 0;
		    }
		    nsoltemp = sol_number(temp_mat, tree_list[i].tl_tree->tr_l.tl_name, &old);
		    (void)fprintf(tabptr, "   %c [%d] ", op, nsoltemp);
		}
	    } else {
		nsoltemp = sol_number(old_mat, tree_list[i].tl_tree->tr_l.tl_name, &old);
		(void)fprintf(tabptr, "   %c [%d] ", op, nsoltemp);
		continue;
	    }

	    if (flag == REG_TABLE || old) {
		(void) fprintf(tabptr, "%s\n", tree_list[i].tl_tree->tr_l.tl_name);
		continue;
	    } else
		(void) fprintf(tabptr, "%s:  ", tree_list[i].tl_tree->tr_l.tl_name);

	    if (!old && (sol_dp->d_flags & DIR_SOLID)) {
		/* if we get here, we must be looking for a solid table */
		bu_vls_init_if_uninit(&tmp_vls);
		if (rt_functab[sol_intern.idb_type].ft_describe(&tmp_vls, &sol_intern, 1, base2local, &rt_uniresource, dbip) < 0) {
		    Tcl_AppendResult(INTERP, tree_list[i].tl_tree->tr_l.tl_name,
				     "describe error\n", (char *)NULL);
		}
		fprintf(tabptr, "%s", bu_vls_addr(&tmp_vls));
		bu_vls_free(&tmp_vls);
	    }
	    if (nsoltemp && (sol_dp->d_flags & DIR_SOLID))
		rt_db_free_internal(&sol_intern);
	}
    } else if (dp->d_flags & DIR_COMB) {
	int cur_length;

	bu_ptbl_ins(cur_path, (long *)dp);
	cur_length = BU_PTBL_END(cur_path);

	for (i=0; i<actual_count; i++) {
	    struct directory *nextdp;
	    mat_t new_mat;

	    nextdp = db_lookup(dbip, tree_list[i].tl_tree->tr_l.tl_name, LOOKUP_NOISY);
	    if (nextdp == DIR_NULL) {
		Tcl_AppendResult(INTERP, "\tskipping this object\n", (char *)NULL);
		continue;
	    }

	    /* recurse */
	    if (tree_list[i].tl_tree->tr_l.tl_mat) {
		bn_mat_mul(new_mat, old_mat, tree_list[i].tl_tree->tr_l.tl_mat);
	    } else {
		MAT_COPY(new_mat, old_mat);
	    }
	    new_tables(nextdp, cur_path, new_mat, flag);
	    bu_ptbl_trunc(cur_path, cur_length);
	}
    } else {
	Tcl_AppendResult(INTERP, "Illegal flags for ", dp->d_namep,
			 "skipping\n", (char *)NULL);
	return;
    }

 out:
    bu_free((char *)tree_list, "new_tables: tree_list");
    intern.idb_meth->ft_ifree(&intern);
    return;
}


/*
 * control routine for building ascii tables
 */
int
f_tables(ClientData UNUSED(clientData), Tcl_Interp *interpreter, int argc, const char *argv[])
{
    static const char sortcmd[] = "sort -n +1 -2 -o /tmp/ord_id ";
    static const char catcmd[] = "cat /tmp/ord_id >> ";

    int flag; /* which type of table to make */
    struct bu_vls tmp_vls;
    struct bu_vls cmd;
    struct bu_ptbl cur_path;
    int status;
    char *timep;
    time_t now;
    int i;

    CHECK_DBI_NULL;

    if (argc < 3) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "help %s", argv[0]);
	Tcl_Eval(interpreter, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    bu_vls_init(&tmp_vls);
    bu_vls_init(&cmd);
    bu_ptbl_init(&cur_path, 8, "f_tables: cur_path");
    numreg = 0;
    numsol = 0;

    if (setjmp(jmp_env) == 0) {
	/* allow interrupts */
	(void)signal(SIGINT, sig3);  
    } else {
	bu_vls_free(&cmd);
	bu_vls_free(&tmp_vls);
	bu_ptbl_free(&cur_path);
	return TCL_OK;
    }
    status = TCL_OK;

    /* find out which ascii table is desired */
    if (strcmp(argv[0], "solids") == 0) {
	/* complete summary - down to solids/paremeters */
	flag = SOL_TABLE;
    } else if (strcmp(argv[0], "regions") == 0) {
	/* summary down to solids as members of regions */
	flag = REG_TABLE;
    } else if (strcmp(argv[0], "idents") == 0) {
	/* summary down to regions */
	flag = ID_TABLE;
    } else {
	/* should never reach here */
	Tcl_AppendResult(interpreter, "tables:  input error\n", (char *)NULL);
	status = TCL_ERROR;
	goto end;
    }

    /* open the file */
    if ((tabptr=fopen(argv[1], "w+")) == NULL) {
	Tcl_AppendResult(interpreter, "Can't open ", argv[1], "\n", (char *)NULL);
	status = TCL_ERROR;
	goto end;
    }

    if (flag == SOL_TABLE || flag == REG_TABLE) {
	/* temp file for discrimination of solids */
	if ((idfd = creat("/tmp/mged_discr", 0600)) < 0) {
	    perror("/tmp/mged_discr");
	    status = TCL_ERROR;
	    goto end;
	}
	rd_idfd = open("/tmp/mged_discr", 2);
    }

    (void)time(&now);
    timep = ctime(&now);
    timep[24] = '\0';
    (void)fprintf(tabptr, "1 -8    Summary Table {%s}  (written: %s)\n", argv[0], timep);
    (void)fprintf(tabptr, "2 -7         file name    : %s\n", dbip->dbi_filename);
    (void)fprintf(tabptr, "3 -6         \n");
    (void)fprintf(tabptr, "4 -5         \n");
#ifndef _WIN32
    (void)fprintf(tabptr, "5 -4         user         : %s\n", getpwuid(getuid())->pw_gecos);
#else
    {
	char uname[256];
	DWORD dwNumBytes = 256;
	if (GetUserName(uname, &dwNumBytes))
	    (void)fprintf(tabptr, "5 -4         user         : %s\n", uname);
	else
	    (void)fprintf(tabptr, "5 -4         user         : UNKNOWN\n");
    }
#endif
    (void)fprintf(tabptr, "6 -3         target title : %s\n", cur_title);
    (void)fprintf(tabptr, "7 -2         target units : %s\n",
		  bu_units_string(dbip->dbi_local2base));
    (void)fprintf(tabptr, "8 -1         objects      :");
    for (i=2; i<argc; i++) {
	if ((i%8) == 0)
	    (void)fprintf(tabptr, "\n                           ");
	(void)fprintf(tabptr, " %s", argv[i]);
    }
    (void)fprintf(tabptr, "\n\n");

    /* make the tables */
    for (i=2; i<argc; i++) {
	struct directory *dp;

	bu_ptbl_reset(&cur_path);
	if ((dp = db_lookup(dbip, argv[i], LOOKUP_NOISY)) != DIR_NULL)
	    new_tables(dp, &cur_path, (const matp_t)bn_mat_identity, flag);
	else
	    Tcl_AppendResult(interpreter, " skip this object\n", (char *)NULL);
    }

    Tcl_AppendResult(interpreter, "Summary written in: ", argv[1], "\n", (char *)NULL);

    if (flag == SOL_TABLE || flag == REG_TABLE) {
	(void)unlink("/tmp/mged_discr\0");
	(void)fprintf(tabptr, "\n\nNumber Primitives = %ld  Number Regions = %ld\n",
		      numsol, numreg);

	bu_vls_printf(&tmp_vls, "Processed %ld Primitives and %ld Regions\n",
		      numsol, numreg);
	Tcl_AppendResult(interpreter, bu_vls_addr(&tmp_vls), (char *)NULL);

	(void)fclose(tabptr);
    } else {
	(void)fprintf(tabptr, "* 9999999\n* 9999999\n* 9999999\n* 9999999\n* 9999999\n");
	(void)fclose(tabptr);

	bu_vls_printf(&tmp_vls, "Processed %ld Regions\n", numreg);
	Tcl_AppendResult(interpreter, bu_vls_addr(&tmp_vls), (char *)NULL);

	/* make ordered idents */
	bu_vls_strcpy(&cmd, sortcmd);
	bu_vls_strcat(&cmd, argv[1]);
	Tcl_AppendResult(interpreter, bu_vls_addr(&cmd), "\n", (char *)NULL);
	(void)system(bu_vls_addr(&cmd));

	bu_vls_trunc(&cmd, 0);
	bu_vls_strcpy(&cmd, catcmd);
	bu_vls_strcat(&cmd, argv[1]);
	Tcl_AppendResult(interpreter, bu_vls_addr(&cmd), "\n", (char *)NULL);
	(void)system(bu_vls_addr(&cmd));

	(void)unlink("/tmp/ord_id\0");
    }

 end:
    bu_vls_free(&cmd);
    bu_vls_free(&tmp_vls);
    bu_ptbl_free(&cur_path);
    (void)signal(SIGINT, SIG_IGN);
    return status;
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
