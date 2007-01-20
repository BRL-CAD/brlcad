/*                           R E D . C
 * BRL-CAD
 *
 * Copyright (c) 1992-2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 */
/** @file red.c
 *
 *	These routines allow editing of a combination using the text editor
 *	of the users choice.
 *
 *  Author -
 *	John Anderson
 *
 *  Source -
 *	SECAD/VLD Computing Consortium
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif
#include <errno.h>
#include <ctype.h>
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "raytrace.h"
#include "db.h"

#include "./ged.h"
#include "./sedit.h"

extern int cmd_name(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);

static char	red_tmpfil[17];
#ifndef _WIN32
static char	*red_tmpfil_init = "/tmp/GED.aXXXXXX";
#else
static char	*red_tmpfil_init = "C:\\GED.aXXXXXX";
#endif
static char	red_tmpcomb[16];
static char	*red_tmpcomb_init = "red_tmp.aXXXXXX";
static char	delims[] = " \t/";	/* allowable delimiters */


HIDDEN char *
find_keyword(int i, char *line, char *word)
{
    char *ptr1;
    char *ptr2;
    int j;

    /* find the keyword */
    ptr1 = strstr( &line[i], word );
    if( !ptr1 )
	return( (char *)NULL );

    /* find the '=' */
    ptr2 = strchr( ptr1, '=' );
    if( !ptr2 )
	return( (char *)NULL );

    /* skip any white space before the value */
    while( isspace( *(++ptr2) ) );

    /* eliminate trailing white space */
    j = strlen( line );
    while( isspace( line[--j] ) );
    line[j+1] = '\0';

    /* return pointer to the value */
    return( ptr2 );
}

HIDDEN void
print_matrix(FILE *fp, matp_t matrix)
{
    int k;
    char buf[64];
    fastf_t tmp;

    if( !matrix )
	return;

    for( k=0 ; k<16 ; k++ ) {
	sprintf( buf, "%g", matrix[k] );
	tmp = atof( buf );
	if( tmp == matrix[k] )
	    fprintf( fp, " %g", matrix[k] );
	else
	    fprintf( fp, " %.12e", matrix[k] );
    }
}

HIDDEN void
vls_print_matrix(struct bu_vls *vls, matp_t matrix)
{
    int k;
    char buf[64];
    fastf_t tmp;

    if(!matrix)
	return;

    if(bn_mat_is_identity(matrix))
	return;

    for(k=0; k<16; k++){
	sprintf(buf, "%g", matrix[k]);
	tmp = atof(buf);
	if(tmp == matrix[k])
	    bu_vls_printf(vls, " %g", matrix[k]);
	else
	    bu_vls_printf(vls, " %.12e", matrix[k]);
    }
}

void
put_rgb_into_comb(struct rt_comb_internal *comb, char *str)
{
    int r, g, b;

    if(sscanf(str, "%d%d%d", &r, &g, &b) != 3){
	comb->rgb_valid = 0;
	return;
    }

    /* clamp the RGB values to [0,255] */
    if(r < 0)
	r = 0;
    else if(r > 255)
	r = 255;

    if(g < 0)
	g = 0;
    else if(g > 255)
	g = 255;

    if(b < 0)
	b = 0;
    else if(b > 255)
	b = 255;

    comb->rgb[0] = (unsigned char)r;
    comb->rgb[1] = (unsigned char)g;
    comb->rgb[2] = (unsigned char)b;
    comb->rgb_valid = 1;
}

struct line_list{
    struct bu_list l;
    char *line;
};

struct line_list HeadLines;

HIDDEN int
count_nodes(char *line)
{
    char *ptr;
    char *name;
    char relation;
    int node_count=0;

    /* sanity */
    if (line == NULL)
	return 0;

    ptr = strtok(line , delims);

    while (ptr) {
	/* First non-white is the relation operator */
	relation = (*ptr);

	if (relation != '+' && relation != 'u' && relation != '-') {
	    struct bu_vls tmp_vls;

	    bu_vls_init(&tmp_vls);
	    bu_vls_printf(&tmp_vls, " %c is not a legal operator\n" , relation );
	    Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
	    bu_vls_free(&tmp_vls);
	    return( -1 );
	}

	/* Next must be the member name */
	name = strtok((char *)NULL, delims);

	if (name == NULL) {
	    Tcl_AppendResult(interp, " operand name missing\n", (char *)NULL);
	    return( -1 );
	}

	ptr = strtok( (char *)NULL, delims );
	/*
	 * If this token is not a boolean operator, then it must be the start
	 * of a matrix which we will skip.
	 */
	if (ptr && !((*ptr == 'u' || *ptr == '-' || *ptr=='+') &&
		     *(ptr+1) == '\0')) {
	    int k;

	    /* skip past matrix, k=1 because we already have the first value */
	    for (k=1 ; k<16 ; k++) {
		ptr = strtok( (char *)NULL, delims );
		if (!ptr) {
		    Tcl_AppendResult(interp, "expecting a matrix\n", (char *)NULL);
		    return( -1 );
		}
	    }

	    /* get the next relational operator on the current line */
	    ptr = strtok( (char *)NULL, delims );
	}

	node_count++;
    }

    return node_count;
}

static int
make_tree(struct rt_comb_internal *comb, struct directory *dp, int node_count, char *old_name, char *new_name, struct rt_tree_array *rt_tree_array, int tree_index)
{
    struct rt_db_internal	intern;
    union tree		*final_tree;

    if (tree_index)
	final_tree = (union tree *)db_mkgift_tree( rt_tree_array, node_count, &rt_uniresource );
    else
	final_tree = (union tree *)NULL;

    RT_INIT_DB_INTERNAL(&intern);
    intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern.idb_type = ID_COMBINATION;
    intern.idb_meth = &rt_functab[ID_COMBINATION];
    intern.idb_ptr = (genptr_t)comb;
    comb->tree = final_tree;

    if (strcmp(new_name, old_name)) {
	int flags;

	if (comb->region_flag)
	    flags = DIR_COMB | DIR_REGION;
	else
	    flags = DIR_COMB;

	if (dp != DIR_NULL) {
	    if (db_delete(dbip, dp) || db_dirdelete(dbip, dp)) {
		Tcl_AppendResult(interp, "ERROR: Unable to delete directory entry for ",
				 old_name, "\n", (char *)NULL);
		rt_comb_ifree(&intern, &rt_uniresource);
		return(1);
	    }
	}

	if ((dp=db_diradd(dbip, new_name, -1L, 0, flags, (genptr_t)&intern.idb_type)) == DIR_NULL) {
	    Tcl_AppendResult(interp, "Cannot add ", new_name,
			     " to directory, no changes made\n", (char *)NULL);
	    rt_comb_ifree(&intern, &rt_uniresource);
	    return(1);
	}
    } else if( dp == DIR_NULL ) {
	int flags;

	if (comb->region_flag)
	    flags = DIR_COMB | DIR_REGION;
	else
	    flags = DIR_COMB;

	if ((dp=db_diradd(dbip, new_name, -1L, 0, flags, (genptr_t)&intern.idb_type)) == DIR_NULL) {
	    Tcl_AppendResult(interp, "Cannot add ", new_name,
			     " to directory, no changes made\n", (char *)NULL);
	    rt_comb_ifree( &intern, &rt_uniresource );
	    return(1);
	}
    } else {
	if (comb->region_flag)
	    dp->d_flags |= DIR_REGION;
	else
	    dp->d_flags &= ~DIR_REGION;
    }

    if (rt_db_put_internal(dp, dbip, &intern, &rt_uniresource) < 0) {
	Tcl_AppendResult(interp, "ERROR: Unable to write new combination into database.\n", (char *)NULL);
	return 1;
    }

    return(0);
}


HIDDEN int
put_tree_into_comb(struct rt_comb_internal *comb, struct directory *dp, char *old_name, char *new_name, char *str)
{
    int			i;
    int			done;
    char			*line;
    char			*ptr;
    char			relation;
    char			*name;
    struct rt_tree_array	*rt_tree_array;
    struct line_list	*llp;
    int			node_count = 0;
    int			tree_index = 0;
    union tree		*tp;
    matp_t			matrix;
    struct bu_vls		vls;
    int			result;

    if (str == (char *)NULL)
	return TCL_ERROR;

    BU_LIST_INIT(&HeadLines.l);

    /* break str into lines */
    line = str;
    ptr = strchr(str, '\n');
    if (ptr != NULL)
	*ptr = '\0';
    bu_vls_init(&vls);
    while (line != (char *)NULL) {
	int n;

	bu_vls_strcpy(&vls, line);

	if ((n = count_nodes(bu_vls_addr(&vls))) < 0) {
	    bu_vls_free(&vls);
	    bu_list_free(&HeadLines.l);
	    return TCL_ERROR;
	} else if (n > 0) {
	    BU_GETSTRUCT(llp, line_list);
	    BU_LIST_INSERT(&HeadLines.l, &llp->l);
	    llp->line = line;

	    node_count += n;
	} /* else blank line */

	if (ptr != NULL && *(ptr+1) != '\0') {
	    /* leap frog past EOS */
	    line = ptr + 1;

	    ptr = strchr(line, '\n');
	    if (ptr != NULL)
		*ptr = '\0';
	} else {
	    line = NULL;
	}
    }
    bu_vls_free(&vls);

    /* build tree list */
    if (node_count)
	rt_tree_array = (struct rt_tree_array *)bu_calloc(node_count, sizeof(struct rt_tree_array), "tree list" );
    else
	rt_tree_array = (struct rt_tree_array *)NULL;

    for (BU_LIST_FOR(llp, line_list, &HeadLines.l)) {
	done = 0;
	ptr = strtok(llp->line, delims);
	while (!done) {
	    if (!ptr)
		break;

	    /* First non-white is the relation operator */
	    relation = (*ptr);
	    if (relation == '\0')
		break;

	    /* Next must be the member name */
	    ptr = strtok((char *)NULL, delims);
	    if (ptr == (char *)NULL) {
		bu_list_free(&HeadLines.l);
		if (rt_tree_array)
		    bu_free((char *)rt_tree_array, "red: tree list");
		bu_log("no name specified\n");
		return TCL_ERROR;
	    }
	    name = ptr;

	    /* Eliminate trailing white space from name */
	    i = strlen( ptr );
	    while(isspace(name[--i]))
		name[i] = '\0';

	    /* Check for existence of member */
	    if ((db_lookup(dbip , name , LOOKUP_QUIET)) == DIR_NULL)
		bu_log("\tWARNING: ' %s ' does not exist\n", name);

	    /* get matrix */
	    ptr = strtok((char *)NULL, delims);
	    if (ptr == (char *)NULL) {
		matrix = (matp_t)NULL;
		done = 1;
	    } else if (*ptr == 'u' ||
		       (*ptr == '-' && *(ptr+1) == '\0') ||
		       (*ptr == '+' && *(ptr+1) == '\0')) {
		/* assume another relational operator */
		matrix = (matp_t)NULL;
	    } else {
		int k;

		matrix = (matp_t)bu_calloc(16, sizeof(fastf_t), "red: matrix");
		matrix[0] = atof(ptr);
		for (k=1 ; k<16 ; k++) {
		    ptr = strtok((char *)NULL, delims);
		    if (!ptr) {
			bu_log("incomplete matrix for member %s - No changes made\n", name);
			bu_free( (char *)matrix, "red: matrix" );
			if(rt_tree_array)
			    bu_free((char *)rt_tree_array, "red: tree list");
			bu_list_free(&HeadLines.l);
			return TCL_ERROR;
		    }
		    matrix[k] = atof( ptr );
		}
		if (bn_mat_is_identity( matrix )) {
		    bu_free((char *)matrix, "red: matrix");
		    matrix = (matp_t)NULL;
		}

		ptr = strtok((char *)NULL, delims);
		if (ptr == (char *)NULL)
		    done = 1;
	    }

	    /* Add it to the combination */
	    switch (relation) {
		case '+':
		    rt_tree_array[tree_index].tl_op = OP_INTERSECT;
		    break;
		case '-':
		    rt_tree_array[tree_index].tl_op = OP_SUBTRACT;
		    break;
		default:
		    bu_log("unrecognized relation (assume UNION)\n");
		case 'u':
		    rt_tree_array[tree_index].tl_op = OP_UNION;
		    break;
	    }

	    BU_GETUNION(tp, tree);
	    rt_tree_array[tree_index].tl_tree = tp;
	    tp->tr_l.magic = RT_TREE_MAGIC;
	    tp->tr_l.tl_op = OP_DB_LEAF;
	    tp->tr_l.tl_name = bu_strdup( name );
	    tp->tr_l.tl_mat = matrix;
	    tree_index++;
	}
    }

    bu_list_free(&HeadLines.l);
    result = make_tree(comb, dp, node_count, old_name, new_name, rt_tree_array, tree_index);
    if (result == 0)
	return TCL_OK;
    else
	return TCL_ERROR;
}

int
cmd_get_comb(ClientData	clientData,
	     Tcl_Interp	*interp,
	     int	argc,
	     char	**argv)
{
    struct directory *dp;
    struct rt_db_internal	intern;
    struct rt_comb_internal *comb;
    struct rt_tree_array	*rt_tree_array;
    int i;
    int node_count;
    int actual_count;
    struct bu_vls vls;

    CHECK_DBI_NULL;
    CHECK_READ_ONLY;

    bu_vls_init(&vls);

    if (argc != 2) {
	bu_vls_printf(&vls, "helpdevel get_comb");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    dp = db_lookup(dbip , argv[1] , LOOKUP_QUIET);

    if (dp != DIR_NULL) {
	if (!(dp->d_flags & DIR_COMB)) {
	    Tcl_AppendResult(interp, argv[1],
			     " is not a combination, so cannot be edited this way\n", (char *)NULL);
	    return TCL_ERROR;
	}

	if (rt_db_get_internal(&intern, dp, dbip, (fastf_t *)NULL, &rt_uniresource) < 0)
	    TCL_READ_ERR_return;

	comb = (struct rt_comb_internal *)intern.idb_ptr;

	if (comb->tree && db_ck_v4gift_tree(comb->tree) < 0) {
	    db_non_union_push(comb->tree, &rt_uniresource);
	    if (db_ck_v4gift_tree(comb->tree) < 0) {
		Tcl_AppendResult(interp, "Cannot flatten tree for editing\n", (char *)NULL);
		return TCL_ERROR;
	    }
	}

	node_count = db_tree_nleaves(comb->tree);
	if (node_count > 0) {
	    rt_tree_array = (struct rt_tree_array *)bu_calloc(node_count,
							      sizeof(struct rt_tree_array),
							      "tree list");
	    actual_count = (struct rt_tree_array *)db_flatten_tree(rt_tree_array,
								   comb->tree,
								   OP_UNION,
								   1,
								   &rt_uniresource) - rt_tree_array;
	    BU_ASSERT_LONG(actual_count, ==, node_count);
	    comb->tree = TREE_NULL;
	} else {
	    rt_tree_array = (struct rt_tree_array *)NULL;
	    actual_count = 0;
	}

	Tcl_AppendElement(interp, dp->d_namep);                 /* NAME=name */
	if (comb->region_flag) {
	    Tcl_AppendElement(interp, "Yes");              /* REGION=Yes */
	    bu_vls_trunc(&vls, 0);
	    bu_vls_printf(&vls, "%d", comb->region_id );
	    Tcl_AppendElement(interp, bu_vls_addr(&vls));  /* REGION_ID=comb->region_id */
	    bu_vls_trunc(&vls, 0);
	    bu_vls_printf(&vls, "%d", comb->aircode);
	    Tcl_AppendElement(interp, bu_vls_addr(&vls));  /* AIRCODE=comb->aircode */
	    bu_vls_trunc(&vls, 0);
	    bu_vls_printf(&vls, "%d", comb->GIFTmater);
	    Tcl_AppendElement(interp, bu_vls_addr(&vls));  /* GIFT_MATERIAL=comb->GIFTmater */
	    bu_vls_trunc(&vls, 0);
	    bu_vls_printf(&vls, "%d", comb->los );
	    Tcl_AppendElement(interp, bu_vls_addr(&vls));  /* LOS=comb->los */
	} else {
	    Tcl_AppendElement(interp, "No");   /* REGION=No */
	}

	if (comb->rgb_valid) {
	    bu_vls_trunc(&vls, 0);
	    bu_vls_printf(&vls, "%d %d %d", V3ARGS(comb->rgb));
	    Tcl_AppendElement(interp, bu_vls_addr(&vls));  /* COLOR=comb->rgb */
	} else
	    Tcl_AppendElement(interp, "");                 /* COLOR="" */

	Tcl_AppendElement(interp, bu_vls_addr(&comb->shader)); /* SHADER=comb->shader */

	if (comb->inherit)
	    Tcl_AppendElement(interp, "Yes");  /* INHERIT=Yes */
	else
	    Tcl_AppendElement(interp, "No");   /* INHERIT=No  */


	bu_vls_trunc(&vls, 0);
	for (i = 0 ; i < actual_count ; i++) {
	    char op;

	    switch (rt_tree_array[i].tl_op) {
		case OP_UNION:
		    op = 'u';
		    break;
		case OP_INTERSECT:
		    op = '+';
		    break;
		case OP_SUBTRACT:
		    op = '-';
		    break;
		default:
		    Tcl_AppendResult(interp, "Illegal op code in tree\n",
				     (char *)NULL);
		    bu_vls_free(&vls);

		    return TCL_ERROR;
	    }

	    bu_vls_printf(&vls, " %c %s\t" , op , rt_tree_array[i].tl_tree->tr_l.tl_name);
	    vls_print_matrix(&vls, rt_tree_array[i].tl_tree->tr_l.tl_mat);
	    bu_vls_printf(&vls, "\n");
	    db_free_tree(rt_tree_array[i].tl_tree, &rt_uniresource);
	}

	Tcl_AppendElement(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);

	return TCL_OK;
    } else {
	Tcl_AppendElement(interp, argv[1]); /* NAME=argv[1] */
	Tcl_AppendElement(interp, "Yes");    /* REGION=Yes */
	bu_vls_trunc(&vls, 0);
	bu_vls_printf(&vls, "%d", item_default);
	Tcl_AppendElement(interp, bu_vls_addr(&vls)); /* REGION_ID=item_default */
	bu_vls_trunc(&vls, 0);
	bu_vls_printf(&vls, "%d", air_default);
	Tcl_AppendElement(interp, bu_vls_addr(&vls)); /* AIRCODE=air_default */
	bu_vls_trunc(&vls, 0);
	bu_vls_printf(&vls, "%d", mat_default);
	Tcl_AppendElement(interp, bu_vls_addr(&vls)); /* GIFT_MATERIAL=mat_default */
	bu_vls_trunc(&vls, 0);
	bu_vls_printf(&vls, "%d", los_default);
	Tcl_AppendElement(interp, bu_vls_addr(&vls)); /* LOS=los_default */
	Tcl_AppendElement(interp, "");      /* COLOR=""         */
	Tcl_AppendElement(interp, "");      /* SHADER=""        */
	Tcl_AppendElement(interp, "No");    /* INHERIT=No       */
	Tcl_AppendElement(interp, "");      /* COMBINATION:""   */
	bu_vls_free(&vls);

	return TCL_RETURN;
    }
}


int
writecomb( const struct rt_comb_internal *comb, const char *name )
{
    /*	Writes the file for later editing */
    struct rt_tree_array	*rt_tree_array;
    FILE			*fp;
    int			i;
    int			node_count;
    int			actual_count;

    if( comb )
	RT_CK_COMB( comb );

    /* open the file */
    if( (fp=fopen( red_tmpfil , "w" )) == NULL ) {
	Tcl_AppendResult(interp, "Cannot open create file for editing\n", (char *)NULL);
	perror( "MGED" );
	return(1);
    }

    if( !comb )	{
	fprintf( fp, "NAME=%s\n", name );
	fprintf( fp, "REGION=No\n" );
	fprintf( fp, "REGION_ID=\n" );
	fprintf( fp, "AIRCODE=\n" );
	fprintf( fp, "GIFT_MATERIAL=\n" );
	fprintf( fp, "LOS=\n" );
	fprintf( fp, "COLOR=\n" );
	fprintf( fp, "SHADER=\n" );
	fprintf( fp, "INHERIT=No\n" );
	fprintf( fp, "COMBINATION:\n" );
	fclose( fp );
	return( 0 );
    }

    if( comb->tree && db_ck_v4gift_tree( comb->tree ) < 0 ) {
	db_non_union_push( comb->tree, &rt_uniresource );
	if( db_ck_v4gift_tree( comb->tree ) < 0 ) {
	    Tcl_AppendResult(interp, "Cannot flatten tree for editing\n", (char *)NULL );
	    return( 1 );
	}
    }
    node_count = db_tree_nleaves( comb->tree );
    if( node_count > 0 ) {
	rt_tree_array = (struct rt_tree_array *)bu_calloc( node_count,
							   sizeof( struct rt_tree_array ), "tree list" );
	actual_count = (struct rt_tree_array *)db_flatten_tree(
							       rt_tree_array, comb->tree, OP_UNION,
							       0, &rt_uniresource ) - rt_tree_array;
	BU_ASSERT_LONG( actual_count, ==, node_count );
    } else {
	rt_tree_array = (struct rt_tree_array *)NULL;
	actual_count = 0;
    }

    fprintf( fp, "NAME=%s\n", name );
    if( comb->region_flag ) {
	fprintf( fp, "REGION=Yes\n" );
	fprintf( fp, "REGION_ID=%d\n", comb->region_id );
	fprintf( fp, "AIRCODE=%d\n", comb->aircode );
	fprintf( fp, "GIFT_MATERIAL=%d\n", comb->GIFTmater );
	fprintf( fp, "LOS=%d\n", comb->los );
    } else {
	fprintf( fp, "REGION=No\n" );
	fprintf( fp, "REGION_ID=\n" );
	fprintf( fp, "AIRCODE=\n" );
	fprintf( fp, "GIFT_MATERIAL=\n" );
	fprintf( fp, "LOS=\n" );
    }

    if( comb->rgb_valid )
	fprintf( fp, "COLOR= %d %d %d\n", V3ARGS( comb->rgb ) );
    else
	fprintf( fp, "COLOR=\n" );

    fprintf( fp, "SHADER=%s\n", bu_vls_addr( &comb->shader ) );
#if 0
    fprintf( fp, "MATERIAL=%s\n", bu_vls_addr( &comb->material ) );
#endif
    if( comb->inherit )
	fprintf( fp, "INHERIT=Yes\n" );
    else
	fprintf( fp, "INHERIT=No\n" );

    fprintf( fp, "COMBINATION:\n" );

    for( i=0 ; i<actual_count ; i++ ) {
	char op;

	switch( rt_tree_array[i].tl_op ) {
	    case OP_UNION:
		op = 'u';
		break;
	    case OP_INTERSECT:
		op = '+';
		break;
	    case OP_SUBTRACT:
		op = '-';
		break;
	    default:
		Tcl_AppendResult(interp, "Illegal op code in tree\n",
				 (char *)NULL );
		fclose( fp );
		return( 1 );
	}
	if( fprintf( fp , " %c %s" , op , rt_tree_array[i].tl_tree->tr_l.tl_name ) <= 0 ) {
	    Tcl_AppendResult(interp, "Cannot write to temp file (", red_tmpfil,
			     "). Aborting edit\n", (char *)NULL );
	    fclose( fp );
	    return( 1 );
	}
	print_matrix( fp, rt_tree_array[i].tl_tree->tr_l.tl_mat );
	fprintf( fp, "\n" );
    }
    fclose( fp );
    return( 0 );
}

int
checkcomb(void)
{
    /*	Do some minor checking of the edited file */

    FILE *fp;
    int node_count=0;
    int nonsubs=0;
    int i,j,done,ch;
    int done2,first;
    char relation;
    char name_v4[NAMESIZE+1];
    char *name_v5=NULL;
    char *name=NULL;
    char line[RT_MAXLINE] = {0};
    char lineCopy[RT_MAXLINE] = {0};
    char *ptr = (char *)NULL;
    int region=(-1);
    int id=0,air=0;
    int rgb_valid;

    if( (fp=fopen( red_tmpfil , "r" )) == NULL ) {
	Tcl_AppendResult(interp, "Cannot open create file for editing\n", (char *)NULL);
	perror( "MGED" );
	return(-1);
    }

    /* Read a line at a time */
    done = 0;
    while( !done ) {
	/* Read a line */
	i = (-1);

	while( (ch=getc( fp )) != EOF && ch != '\n' && i < RT_MAXLINE )
	    line[++i] = ch;

	if( ch == EOF ) {
	    /* We must be done */
	    done = 1;
	    if( i < 0 )
		break;
	}
	if( i == RT_MAXLINE ) {
	    line[RT_MAXLINE-1] = '\0';
	    Tcl_AppendResult(interp, "Line too long in edited file:\n",
			     line, "\n", (char *)NULL);
	    fclose( fp );
	    return( -1 );
	}

	line[++i] = '\0';
	strcpy( lineCopy, line );

	/* skip leading white space */
	i = (-1);
	while( isspace( line[++i] ));

	if( line[i] == '\0' )
	    continue;	/* blank line */

	if( (ptr=find_keyword(i, line, "NAME" ) ) ) {
	    if( dbip->dbi_version < 5 ) {
		int len;

		len = strlen( ptr );
		if( len >= NAMESIZE ) {
		    while( len > 1 && isspace( ptr[len-1] ) )
			len--;
		}
		if( len >= NAMESIZE ) {
		    Tcl_AppendResult(interp, "Name too long for v4 database: ",
				     ptr, "\n", lineCopy, "\n", (char *)NULL );
		    fclose( fp );
		    return( -1 );
		}
	    }
	    continue;
	} else if( (ptr=find_keyword( i, line, "REGION_ID" ) ) ) {
	    id = atoi( ptr );
	    continue;
	} else if( (ptr=find_keyword( i, line, "REGION" ) ) ) {
	    if( *ptr == 'y' || *ptr == 'Y' )
		region = 1;
	    else
		region = 0;
	    continue;
	} else if( (ptr=find_keyword( i, line, "AIRCODE" ) ) ) {
	    air = atoi( ptr );
	    continue;
	} else if( (ptr=find_keyword( i, line, "GIFT_MATERIAL" ) ) ) {
	    continue;
	} else if( (ptr=find_keyword( i, line, "LOS" ) ) ) {
	    continue;
	} else if( (ptr=find_keyword( i, line, "COLOR" ) ) ) {
	    char *ptr2;

	    rgb_valid = 1;
	    ptr2 = strtok( ptr, delims );
	    if( !ptr2 ) {
		continue;
	    } else {
		ptr2 = strtok( (char *)NULL, delims );
		if( !ptr2 ) {
		    rgb_valid = 0;
		} else {
		    ptr2 = strtok( (char *)NULL, delims );
		    if( !ptr2 ) {
			rgb_valid = 0;
		    }
		}
	    }
	    if( !rgb_valid ) {
		Tcl_AppendResult(interp, "WARNING: invalid color specification!!! Must be three integers, each 0-255\n", (char *)NULL );
	    }
	    continue;
	} else if( (ptr=find_keyword( i, line, "SHADER" ) ) )
	    continue;
#if 0
	else if( (ptr=find_keyword( i, line, "MATERIAL" ) ) )
	    continue;
#endif
	else if( (ptr=find_keyword( i, line, "INHERIT" ) ) )
	    continue;
	else if( !strncmp( &line[i], "COMBINATION:", 12 ) ) {
	    if( region < 0 ) {
		Tcl_AppendResult(interp, "Region flag not correctly set\n",
				 "\tMust be 'Yes' or 'No'\n", "\tNo Changes made\n",
				 (char *)NULL );
		fclose( fp );
		return( -1 );
	    } else if( region ) {
		if( id < 0 ) {
		    Tcl_AppendResult(interp, "invalid region ID\n",
				     "\tNo Changes made\n",
				     (char *)NULL );
		    fclose( fp );
		    return( -1 );
		}
		if( air < 0 ) {
		    Tcl_AppendResult(interp, "invalid Air code\n",
				     "\tNo Changes made\n",
				     (char *)NULL );
		    fclose( fp );
		    return( -1 );
		}
		if( air == 0 && id == 0 )
		    Tcl_AppendResult(interp, "Warning: both ID and Air codes are 0!!!\n", (char *)NULL );
		if( air && id )
		    Tcl_AppendResult(interp, "Warning: both ID and Air codes are non-zero!!!\n", (char *)NULL );
	    }
	    continue;
	}

	done2=0;
	first=1;
	ptr = strtok( line , delims );

	while (!done2) {
	    if( name_v5 ) {
		bu_free( name_v5, "name_v5" );
		name_v5 = NULL;
	    }
	    /* First non-white is the relation operator */
	    if( !ptr ) {
		done2 = 1;
		break;
	    }

	    relation = (*ptr);
	    if( relation == '\0' ) {
		if (first)
		    done = 1;

		done2 = 1;
		break;
	    }
	    first = 0;

	    /* Next must be the member name */
	    ptr = strtok( (char *)NULL, delims );
	    name = NULL;
	    if( ptr != NULL && *ptr != '\0' ) {
		if( dbip->dbi_version < 5 ) {
		    strncpy( name_v4 , ptr , NAMESIZE );
		    name_v4[NAMESIZE] = '\0';

		    /* Eliminate trailing white space from name */
		    j = NAMESIZE;
		    while( isspace( name_v4[--j] ) )
			name_v4[j] = '\0';
		    name = name_v4;
		} else {
		    int len;

		    len = strlen( ptr );
		    name_v5 = (char *)bu_malloc( len + 1, "name_v5" );
		    strcpy( name_v5, ptr );
		    while( isspace( name_v5[len-1] ) ) {
			len--;
			name_v5[len] = '\0';
		    }
		    name = name_v5;
		}
	    }

	    if( relation != '+' && relation != 'u' && relation != '-' ) {
		struct bu_vls tmp_vls;

		bu_vls_init(&tmp_vls);
		bu_vls_printf(&tmp_vls, " %c is not a legal operator\n" , relation );
		Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), lineCopy,
				 "\n", (char *)NULL);
		bu_vls_free(&tmp_vls);
		fclose( fp );
		if( dbip->dbi_version >= 5 && name_v5 )
		    bu_free( name_v5, "name_v5" );
		return( -1 );
	    }

	    if( relation != '-' )
		nonsubs++;

	    if( name == NULL || name[0] == '\0' ) {
		Tcl_AppendResult(interp, " operand name missing\n",
				 lineCopy, "\n", (char *)NULL);
		fclose( fp );
		if( dbip->dbi_version >= 5 && name_v5 )
		    bu_free( name_v5, "name_v5" );
		return( -1 );
	    }

	    ptr = strtok( (char *)NULL, delims );
	    if( !ptr )
		done2 = 1;
	    else if(*ptr != 'u' &&
		    (*ptr != '-' || *(ptr+1) != '\0') &&
		    (*ptr != '+' || *(ptr+1) != '\0')) {
		int k;

		/* skip past matrix */
		for( k=1 ; k<16 ; k++ ) {
		    ptr = strtok( (char *)NULL, delims );
		    if( !ptr) {
			Tcl_AppendResult(interp, "incomplete matrix\n",
					 lineCopy, "\n", (char *)NULL);
			fclose( fp );
			if( dbip->dbi_version >= 5 && name_v5 )
			    bu_free( name_v5, "name_v5" );
			return( -1 );
		    }
		}

		/* get the next relational operator on the current line */
		ptr = strtok( (char *)NULL, delims );
	    }

	    node_count++;
	}
    }

    if( dbip->dbi_version >= 5 && name_v5 )
	bu_free( name_v5, "name_v5" );

    fclose( fp );

    if( nonsubs == 0 && node_count ) {
	Tcl_AppendResult(interp, "Cannot create a combination with all subtraction operators\n",
			 (char *)NULL);
	return( -1 );
    }
    return( node_count );
}


int build_comb(struct rt_comb_internal *comb, struct directory *dp, int node_count, char *old_name)
{
    /*	Build the new combination by adding to the recently emptied combination
	This keeps combo info associated with this combo intact */

    FILE *fp;
    char relation;
    char *name=NULL, *new_name;
    char name_v4[NAMESIZE+1];
    char new_name_v4[NAMESIZE+1];
    char line[RT_MAXLINE] = {0};
    char *ptr;
    int ch;
    int i;
    int done=0;
    int done2;
    struct rt_tree_array *rt_tree_array;
    int tree_index=0;
    union tree *tp;
    matp_t matrix;
    int ret=0;

    if(dbip == DBI_NULL)
	return 0;

    if( comb ) {
	RT_CK_COMB( comb );
	RT_CK_DIR( dp );
    }

    if( (fp=fopen( red_tmpfil , "r" )) == NULL ) {
	Tcl_AppendResult(interp, " Cannot open edited file: ",
			 red_tmpfil, "\n", (char *)NULL);
	return( 1 );
    }

    /* empty the existing combination */
    if( comb && comb->tree ) {
	db_free_tree( comb->tree, &rt_uniresource );
	comb->tree = NULL;
    } else {
	/* make an empty combination structure */
	BU_GETSTRUCT( comb, rt_comb_internal );
	comb->magic = RT_COMB_MAGIC;
	comb->tree = TREE_NULL;
	bu_vls_init( &comb->shader );
	bu_vls_init( &comb->material );
    }

    /* build tree list */
    if( node_count )
	rt_tree_array = (struct rt_tree_array *)bu_calloc( node_count , sizeof( struct rt_tree_array ) , "tree list" );
    else
	rt_tree_array = (struct rt_tree_array *)NULL;

    if( dbip->dbi_version < 5 ) {
	if( dp == DIR_NULL )
	    NAMEMOVE( old_name, new_name_v4 );
	else
	    NAMEMOVE( dp->d_namep, new_name_v4 );
	new_name = new_name_v4;
    } else {
	if( dp == DIR_NULL )
	    new_name = bu_strdup( old_name );
	else
	    new_name = bu_strdup( dp->d_namep );
    }

    /* Read edited file */
    while( !done ) {
	/* Read a line */
	i = (-1);

	while( (ch=getc( fp )) != EOF && ch != '\n' && i < RT_MAXLINE )
	    line[++i] = ch;

	if( ch == EOF ) {
	    /* We must be done */
	    done = 1;
	    if( i < 0 )
		break;
	}

	line[++i] = '\0';

	/* skip leading white space */
	i = (-1);
	while( isspace( line[++i] ));

	if( line[i] == '\0' )
	    continue;	/* blank line */

	if( (ptr=find_keyword(i, line, "NAME" ) ) ) {
	    if( dbip->dbi_version < 5 )
		NAMEMOVE( ptr, new_name_v4 );
	    else {
		bu_free( new_name, "new_name" );
		new_name = bu_strdup( ptr );
	    }
	    continue;
	} else if( (ptr=find_keyword( i, line, "REGION_ID" ) ) ) {
	    comb->region_id = atoi( ptr );
	    continue;
	} else if( (ptr=find_keyword( i, line, "REGION" ) ) ) {
	    if( *ptr == 'y' || *ptr == 'Y' )
		comb->region_flag = 1;
	    else
		comb->region_flag = 0;
	    continue;
	} else if( (ptr=find_keyword( i, line, "AIRCODE" ) ) ) {
	    comb->aircode = atoi( ptr );
	    continue;
	} else if( (ptr=find_keyword( i, line, "GIFT_MATERIAL" ) ) ) {
	    comb->GIFTmater = atoi( ptr );
	    continue;
	} else if( (ptr=find_keyword( i, line, "LOS" ) ) ) {
	    comb->los = atoi( ptr );
	    continue;
	} else if( (ptr=find_keyword( i, line, "COLOR" ) ) ) {
	    char *ptr2;
	    int value;

	    ptr2 = strtok( ptr, delims );
	    if( !ptr2 ) {
		comb->rgb_valid = 0;
		continue;
	    }
	    value = atoi( ptr2 );
	    if( value < 0 ) {
		Tcl_AppendResult(interp, "Red value less than 0, assuming 0\n", (char *)NULL );
		value = 0;
	    }
	    if( value > 255 ) {
		Tcl_AppendResult(interp, "Red value greater than 255, assuming 255\n", (char *)NULL );
		value = 255;
	    }
	    comb->rgb[0] = value;
	    ptr2 = strtok( (char *)NULL, delims );
	    if( !ptr2 ) {
		Tcl_AppendResult(interp, "Invalid RGB value\n", (char *)NULL );
		comb->rgb_valid = 0;
		continue;
	    }
	    value = atoi( ptr2 );
	    if( value < 0 ) {
		Tcl_AppendResult(interp, "Green value less than 0, assuming 0\n", (char *)NULL );
		value = 0;
	    }
	    if( value > 255 ) {
		Tcl_AppendResult(interp, "Green value greater than 255, assuming 255\n", (char *)NULL );
		value = 255;
	    }
	    comb->rgb[1] = value;
	    ptr2 = strtok( (char *)NULL, delims );
	    if( !ptr2 ) {
		Tcl_AppendResult(interp, "Invalid RGB value\n", (char *)NULL );
		comb->rgb_valid = 0;
		continue;
	    }
	    value = atoi( ptr2 );
	    if( value < 0 ) {
		Tcl_AppendResult(interp, "Blue value less than 0, assuming 0\n", (char *)NULL );
		value = 0;
	    }
	    if( value > 255 ) {
		Tcl_AppendResult(interp, "Blue value greater than 255, assuming 255\n", (char *)NULL );
		value = 255;
	    }
	    comb->rgb[2] = value;
	    comb->rgb_valid = 1;
	    continue;
	} else if( (ptr=find_keyword( i, line, "SHADER" ) ) ) {
	    bu_vls_strcpy( &comb->shader,  ptr );
	    continue;
	}
#if 0
	else if( (ptr=find_keyword( i, line, "MATERIAL" ) ) ) {
	    bu_vls_strcpy( &comb->material,  ptr );
	    continue;
	}
#endif
	else if( (ptr=find_keyword( i, line, "INHERIT" ) ) ) {
	    if( *ptr == 'y' || *ptr == 'Y' )
		comb->inherit = 1;
	    else
		comb->inherit = 0;
	    continue;
	} else if( !strncmp( &line[i], "COMBINATION:", 12 ) )
	    continue;

	done2=0;
	ptr = strtok( line, delims );
	while (!done2) {
	    if ( !ptr )
		break;

	    /* First non-white is the relation operator */
	    relation = (*ptr);
	    if( relation == '\0' )
		break;

	    /* Next must be the member name */
	    ptr = strtok( (char *)NULL, delims );
	    if( dbip->dbi_version < 5 ) {
		strncpy( name_v4 , ptr, NAMESIZE );
		name_v4[NAMESIZE] = '\0';
		name = name_v4;
	    } else {
		if( name )
		    bu_free( name, "name" );
		name = bu_strdup( ptr );
	    }

	    /* Eliminate trailing white space from name */
	    if( dbip->dbi_version < 5 )
		i = NAMESIZE;
	    else
		i = strlen( name );
	    while( isspace( name[--i] ) )
		name[i] = '\0';

	    /* Check for existence of member */
	    if( (db_lookup( dbip , name , LOOKUP_QUIET )) == DIR_NULL )
		Tcl_AppendResult(interp, "\tWARNING: '", name, "' does not exist\n", (char *)NULL);
	    /* get matrix */
	    ptr = strtok( (char *)NULL, delims );
	    if( !ptr ){
		matrix = (matp_t)NULL;
		done2 = 1;
	    }else if(*ptr == 'u' ||
		     (*ptr == '-' && *(ptr+1) == '\0') ||
		     (*ptr == '+' && *(ptr+1) == '\0')) {
		/* assume another relational operator */
		matrix = (matp_t)NULL;
	    }else {
		int k;

		matrix = (matp_t)bu_calloc( 16, sizeof( fastf_t ), "red: matrix" );
		matrix[0] = atof( ptr );
		for( k=1 ; k<16 ; k++ ) {
		    ptr = strtok( (char *)NULL, delims );
		    if( !ptr ) {
			Tcl_AppendResult(interp, "incomplete matrix for member ",
					 name, " No changes made\n", (char *)NULL );
			bu_free( (char *)matrix, "red: matrix" );
			if( rt_tree_array )
			    bu_free( (char *)rt_tree_array, "red: tree list" );
			fclose( fp );
			return( 1 );
		    }
		    matrix[k] = atof( ptr );
		}
		if( bn_mat_is_identity( matrix ) ) {
		    bu_free( (char *)matrix, "red: matrix" );
		    matrix = (matp_t)NULL;
		}

		ptr = strtok( (char *)NULL, delims );
		if (ptr == (char *)NULL)
		    done2 = 1;
	    }

	    /* Add it to the combination */
	    switch( relation ) {
		case '+':
		    rt_tree_array[tree_index].tl_op = OP_INTERSECT;
		    break;
		case '-':
		    rt_tree_array[tree_index].tl_op = OP_SUBTRACT;
		    break;
		default:
		    Tcl_AppendResult(interp, "unrecognized relation (assume UNION)\n",
				     (char *)NULL );
		case 'u':
		    rt_tree_array[tree_index].tl_op = OP_UNION;
		    break;
	    }
	    BU_GETUNION( tp, tree );
	    rt_tree_array[tree_index].tl_tree = tp;
	    tp->tr_l.magic = RT_TREE_MAGIC;
	    tp->tr_l.tl_op = OP_DB_LEAF;
	    tp->tr_l.tl_name = bu_strdup( name );
	    tp->tr_l.tl_mat = matrix;
	    tree_index++;
	}
    }

    fclose( fp );

    ret = make_tree(comb, dp, node_count, old_name, new_name, rt_tree_array, tree_index);

    if( dbip->dbi_version >= 5 ) {
	if( name )
	    bu_free( name, "name " );
	bu_free( new_name, "new_name" );
    }

    return( ret );
}

void
mktemp_comb(char *str)
{
    /* Make a temporary name for a combination
       a template name is expected as in "mk_temp()" with
       5 trailing X's */

    int counter,done;
    char *ptr;


    if(dbip == DBI_NULL)
	return;

    /* Set "ptr" to start of X's */

    ptr = str;
    while( *ptr != '\0' )
	ptr++;

    while( *(--ptr) == 'X' );
    ptr++;


    counter = 1;
    done = 0;
    while( !done && counter < 99999 ) {
	sprintf( ptr , "%d" , counter );
	if( db_lookup( dbip , str , LOOKUP_QUIET ) == DIR_NULL )
	    done = 1;
	else
	    counter++;
    }
}

int save_comb(struct directory *dpold)
{
    /* Save a combination under a temporory name */

    register struct directory	*dp;
    struct rt_db_internal		intern;

    if(dbip == DBI_NULL)
	return 0;

    /* Make a new name */
    mktemp_comb( red_tmpcomb );

    if( rt_db_get_internal( &intern, dpold, dbip, (fastf_t *)NULL, &rt_uniresource ) < 0 )
	TCL_READ_ERR_return;

    if( (dp=db_diradd( dbip, red_tmpcomb, -1L, 0, dpold->d_flags, (genptr_t)&intern.idb_type)) == DIR_NULL )  {
	Tcl_AppendResult(interp, "Cannot save copy of ", dpold->d_namep,
			 ", no changes made\n", (char *)NULL);
	return( 1 );
    }

    if( rt_db_put_internal(	dp, dbip, &intern, &rt_uniresource ) < 0 ) {
	Tcl_AppendResult(interp, "Cannot save copy of ", dpold->d_namep,
			 ", no changes made\n", (char *)NULL);
	return( 1 );
    }

    return( 0 );
}

/* restore a combination that was saved in "red_tmpcomb" */
void
restore_comb(struct directory *dp)
{
    char *av[4];
    char *name;

    /* Save name of original combo */
    name = bu_strdup( dp->d_namep );

    av[0] = "kill";
    av[1] = name;
    av[2] = NULL;
    av[3] = NULL;
    (void)cmd_kill((ClientData)NULL, interp, 2, av);

    av[0] = "mv";
    av[1] = red_tmpcomb;
    av[2] = name;

    (void)cmd_name((ClientData)NULL, interp, 3, av);

    bu_free( name, "bu_strdup'd name" );
}


/*
 *  Usage:  put_comb comb_name is_Region id air material los color
 *			shader inherit boolean_expr
 */
int
cmd_put_comb(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
    struct directory *dp;
    struct rt_db_internal	intern;
    struct rt_comb_internal *comb;
    char new_name_v4[NAMESIZE+1];
    char *new_name;
    int offset;
    int save_comb_flag = 0;

    CHECK_DBI_NULL;
    CHECK_READ_ONLY;

    if(argc < 7 || 11 < argc){
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helpdevel put_comb");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    strcpy(red_tmpfil, red_tmpfil_init);
    strcpy(red_tmpcomb, red_tmpcomb_init);
    dp = db_lookup( dbip , argv[1] , LOOKUP_QUIET );
    if(dp != DIR_NULL){
	if( !(dp->d_flags & DIR_COMB) ){
	    Tcl_AppendResult(interp, argv[1],
			     " is not a combination, so cannot be edited this way\n", (char *)NULL);
	    return TCL_ERROR;
	}

	if( rt_db_get_internal( &intern, dp, dbip, (fastf_t *)NULL, &rt_uniresource ) < 0 )
	    TCL_READ_ERR_return;

	comb = (struct rt_comb_internal *)intern.idb_ptr;
	save_comb(dp); /* Save combination to a temp name */
	save_comb_flag = 1;
    }else{
	comb = (struct rt_comb_internal *)NULL;
    }

    /* empty the existing combination */
    if( comb && comb->tree ){
	db_free_tree( comb->tree, &rt_uniresource );
	comb->tree = NULL;
    }else{
	/* make an empty combination structure */
	BU_GETSTRUCT( comb, rt_comb_internal );
	comb->magic = RT_COMB_MAGIC;
	comb->tree = TREE_NULL;
	bu_vls_init( &comb->shader );
	bu_vls_init( &comb->material );
    }

    if( dbip->dbi_version < 5 )	{
	new_name = new_name_v4;
	if(dp == DIR_NULL)
	    NAMEMOVE(argv[1], new_name_v4);
	else
	    NAMEMOVE(dp->d_namep, new_name_v4);
    } else {
	if( dp == DIR_NULL )
	    new_name = argv[1];
	else
	    new_name = dp->d_namep;
    }

    if(*argv[2] == 'y' || *argv[2] == 'Y')
	comb->region_flag = 1;
    else
	comb->region_flag = 0;

    if(comb->region_flag){
	if(argc != 11){
	    struct bu_vls vls;

	    bu_vls_init(&vls);
	    bu_vls_printf(&vls, "help put_comb");
	    Tcl_Eval(interp, bu_vls_addr(&vls));
	    bu_vls_free(&vls);
	    return TCL_ERROR;
	}

	comb->region_id = atoi(argv[3]);
	comb->aircode = atoi(argv[4]);
	comb->GIFTmater = atoi(argv[5]);
	comb->los = atoi(argv[6]);

	/* use the new values for defaults */
	item_default = comb->region_id + 1;
	air_default = comb->aircode;
	mat_default = comb->GIFTmater;
	los_default = comb->los;
	offset = 6;
    }else{
	if(argc != 7){
	    struct bu_vls vls;

	    bu_vls_init(&vls);
	    bu_vls_printf(&vls, "help put_comb");
	    Tcl_Eval(interp, bu_vls_addr(&vls));
	    bu_vls_free(&vls);
	    return TCL_ERROR;
	}
	offset = 2;
    }

    put_rgb_into_comb(comb, argv[offset + 1]);
    bu_vls_strcpy(&comb->shader, argv[offset +2]);

    if(*argv[offset + 3] == 'y' || *argv[offset + 3] == 'Y')
	comb->inherit = 1;
    else
	comb->inherit = 0;

    if(put_tree_into_comb(comb, dp, argv[1], new_name, argv[offset + 4]) == TCL_ERROR){
	if(comb){
	    restore_comb(dp);
	    Tcl_AppendResult(interp, "\toriginal restored\n", (char *)NULL);
	}
	(void)unlink(red_tmpfil);
	return TCL_ERROR;
    }else if(save_comb_flag){
	/* eliminate the temporary combination */
	char *av[3];

	av[0] = "kill";
	av[1] = red_tmpcomb;
	av[2] = NULL;
	(void)cmd_kill(clientData, interp, 2, av);
    }

    (void)unlink(red_tmpfil);
    return TCL_OK;
}


int
f_red(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
    struct directory *dp;
    struct rt_db_internal	intern;
    struct rt_comb_internal	*comb;
    int node_count;
    int fd;

    CHECK_DBI_NULL;

    if(argc != 2){
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "help red");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    strcpy(red_tmpfil, red_tmpfil_init);
    strcpy(red_tmpcomb, red_tmpcomb_init);

    dp = db_lookup( dbip , argv[1] , LOOKUP_QUIET );

    if( dp != DIR_NULL ) {
	if( !(dp->d_flags & DIR_COMB ) ) {
	    Tcl_AppendResult(interp, argv[1],
			     " is not a combination, so cannot be edited this way\n", (char *)NULL);
	    return TCL_ERROR;
	}

	if( rt_db_get_internal( &intern, dp, dbip, (fastf_t *)NULL, &rt_uniresource ) < 0 )
	    TCL_READ_ERR_return;

	comb = (struct rt_comb_internal *)intern.idb_ptr;

	/* Make a file for the text editor */
#ifdef _WIN32
	(void)mktemp( red_tmpfil );
#else
	if ((fd = mkstemp(red_tmpfil)) < 0) {
	    perror(red_tmpfil);
	    return TCL_ERROR;;
	}
	(void)close(fd);
#endif

	/* Write the combination components to the file */
	if( writecomb( comb, dp->d_namep ) ) {
	    Tcl_AppendResult(interp, "Unable to edit ", argv[1], "\n", (char *)NULL);
	    unlink( red_tmpfil );
	    return TCL_ERROR;
	}
    } else {
	comb = (struct rt_comb_internal *)NULL;
	/* Make a file for the text editor */
#ifdef _WIN32
	(void)mktemp( red_tmpfil );
#else
	if ((fd = mkstemp(red_tmpfil)) < 0) {
	    perror(red_tmpfil);
	    return TCL_ERROR;;
	}
	(void)close(fd);
#endif

	/* Write the combination components to the file */
	if( writecomb( comb, argv[1] ) ) {
	    Tcl_AppendResult(interp, "Unable to edit ", argv[1], "\n", (char *)NULL);
	    unlink( red_tmpfil );
	    return TCL_ERROR;
	}
    }

    /* Edit the file */
    if( editit( red_tmpfil ) ){

	/* specifically avoid CHECK_READ_ONLY; above so that
	 * we can delay checking if the geometry is read-only
	 * until here so that red may be used to view objects.
	 */
	if (!dbip->dbi_read_only) {
	    if( (node_count = checkcomb()) < 0 ){ /* Do some quick checking on the edited file */
		Tcl_AppendResult(interp, "Error in edited region, no changes made\n", (char *)NULL);
		if( comb )
		    rt_comb_ifree( &intern, &rt_uniresource );
		(void)unlink( red_tmpfil );
		return TCL_ERROR;
	    }

	    if( comb ){
		if( save_comb( dp ) ){ /* Save combination to a temp name */
		    Tcl_AppendResult(interp, "No changes made\n", (char *)NULL);
		    rt_comb_ifree( &intern, &rt_uniresource );
		    (void)unlink( red_tmpfil );
		    return TCL_OK;
		}
	    }

	    if( build_comb( comb, dp, node_count, argv[1] ) ){
		Tcl_AppendResult(interp, "Unable to construct new ", dp->d_namep,
				 (char *)NULL);
		if( comb ){
		    restore_comb( dp );
		    Tcl_AppendResult(interp, "\toriginal restored\n", (char *)NULL );
		    rt_comb_ifree( &intern, &rt_uniresource );
		}

		(void)unlink( red_tmpfil );
		return TCL_ERROR;
	    }else if( comb ){
		/* eliminate the temporary combination */
		char *av[3];

		av[0] = "kill";
		av[1] = red_tmpcomb;
		av[2] = NULL;
		(void)cmd_kill(clientData, interp, 2, av);
	    }
	} else {
	    Tcl_AppendResult(interp, "Because the database is READ-ONLY no changes were made.\n", (char *)NULL);
	}
    }

    (void)unlink( red_tmpfil );
    return TCL_OK;
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
