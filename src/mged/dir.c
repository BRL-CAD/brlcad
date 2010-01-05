/*                           D I R . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2010 United States Government as represented by
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
/** @file dir.c
 *
 */

#include "common.h"

#include <fcntl.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>

#include "bio.h"
#include "tcl.h"
#include "bu.h"
#include "vmath.h"
#include "raytrace.h"
#include "wdb.h"
#include "db.h"

#include "./mged.h"
#include "./mged_dm.h"


#define BAD_EOF	(-1L)			/* eof_addr not set yet */

void	killtree(struct db_i *dbip, struct directory *dp, genptr_t ptr);


/*
 *			F _ M E M P R I N T
 *
 *  Debugging aid:  dump memory maps
 */
int
f_memprint(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
    CHECK_DBI_NULL;

    if (argc < 1 || 1 < argc) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "help memprint");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

#if 0
    Tcl_AppendResult(interp, "Display manager free map:\n", (char *)NULL);
    rt_memprint( &(dmp->dm_map) );
#endif

    bu_log("Database free-storage map:\n");
    rt_memprint( &(dbip->dbi_freep) );

    return TCL_OK;
}

/*
 *			C M D _ G L O B
 *
 *  Assist routine for command processor.  If the current word in
 *  the argv[] array contains '*', '?', '[', or '\' then this word
 *  is potentially a regular expression, and we will tromp through the
 *  entire in-core directory searching for a match. If no match is
 *  found, the original word remains untouched and this routine was an
 *  expensive no-op.  If any match is found, it replaces the original
 *  word. Escape processing is also done done in this module.  If there
 *  are no matches, but there are escapes, the current word is modified.
 *  All matches are sought for, up to the limit of the argv[] array.
 *
 *  Returns 0 if no expansion happened, !0 if we matched something.
 */
int
cmd_glob(int *argcp, char **argv, int maxargs)
{
    static char word[64];
    char *pattern;
    struct directory	*dp;
    int i;
    int escaped = 0;
    int orig_numargs = *argcp;

    if (dbip == DBI_NULL)
	return 0;

    bu_strlcpy( word, argv[*argcp], sizeof(word) );

    /* If * ? [ or \ are present, this is a regular expression */
    pattern = word;
    do {
	if ( *pattern == '\0' )
	    return(0);		/* nothing to do */
	if ( *pattern == '*' ||
	     *pattern == '?' ||
	     *pattern == '[' ||
	     *pattern == '\\' )
	    break;
    } while ( *pattern++);

    /* Note if there are any escapes */
    for ( pattern = word; *pattern; pattern++)
	if ( *pattern == '\\') {
	    escaped++;
	    break;
	}

    /* Search for pattern matches.
     * First, save the pattern (in word), and remove it from
     * argv, as it will be overwritten by the expansions.
     * If any matches are found, we do not have to worry about
     * '\' escapes since the match coming from dp->d_namep is placed
     * into argv.  Only in the case of no matches do we have
     * to do escape crunching.
     */

    FOR_ALL_DIRECTORY_START(dp, dbip) {
	if ( !db_regexp_match( word, dp->d_namep ) )
	    continue;
	/* Successful match */
	/* See if already over the limit */
	if ( *argcp >= maxargs )  {
	    bu_log("%s: expansion stopped after %d matches (%d args)\n",
		   word, *argcp-orig_numargs, maxargs);
	    break;
	}
	argv[(*argcp)++] = dp->d_namep;
    } FOR_ALL_DIRECTORY_END;

    /* If one or matches occurred, decrement final argc,
     * otherwise, do escape processing if needed.
     */
    if ( *argcp > orig_numargs )  {
	(*argcp)--;
	return(1);
    } else if (escaped) {
	char *temp;
	temp = pattern = argv[*argcp];
	do {
	    if (*pattern != '\\') {
		*temp = *pattern;
		temp++;
	    } else if (*(pattern + 1) == '\\') {
		*temp = *pattern;
		pattern++;
		temp++;
	    }
	} while (*pattern++);

	/* Elide the rare pattern which becomes null ("\<NULL>") */
	if (*(argv[*argcp]) == '\0')
	    (*argcp)--;
    }
    return(0);		/* found nothing */
}

HIDDEN void
Do_prefix(struct db_i *dbip, struct rt_comb_internal *comb, union tree *comb_leaf, genptr_t prefix_ptr, genptr_t obj_ptr, genptr_t user_ptr3)
{
    char *prefix, *obj;
    char tempstring_v4[NAMESIZE+1];
    int len = NAMESIZE+1;

    RT_CK_DBI( dbip );
    RT_CK_TREE( comb_leaf );

    prefix = (char *)prefix_ptr;
    obj = (char *)obj_ptr;

    if ( strcmp( comb_leaf->tr_l.tl_name, obj ) )
	return;

    bu_free( comb_leaf->tr_l.tl_name, "comb_leaf->tr_l.tl_name" );
    if ( dbip->dbi_version < 5 ) {
	bu_strlcpy( tempstring_v4, prefix, len);
	bu_strlcat( tempstring_v4, obj, len);
	comb_leaf->tr_l.tl_name = bu_strdup( tempstring_v4 );
    } else {
	len = strlen(prefix)+strlen(obj)+1;
	comb_leaf->tr_l.tl_name = (char *)bu_malloc( len, "Adding prefix" );

	bu_strlcpy( comb_leaf->tr_l.tl_name , prefix, len);
	bu_strlcat( comb_leaf->tr_l.tl_name , obj, len );
    }
}

/*
 *			F _ P R E F I X
 *
 *  Prefix each occurence of a specified object name, both
 *  when defining the object, and when referencing it.
 */
int
f_prefix(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
    int	i, k;
    struct directory *dp;
    struct rt_db_internal	intern;
    struct rt_comb_internal *comb;
    char tempstring_v4[NAMESIZE+1];
    struct bu_vls tempstring_v5;
    char *tempstring;
    int len = NAMESIZE+1;

    CHECK_DBI_NULL;
    CHECK_READ_ONLY;

    if (argc < 3) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "help prefix");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    bu_vls_init( &tempstring_v5 );

    /* First, check validity, and change node names */
    for ( i = 2; i < argc; i++) {
	if ( (dp = db_lookup( dbip, argv[i], LOOKUP_NOISY )) == DIR_NULL) {
	    argv[i] = "";
	    continue;
	}

	if ( dbip->dbi_version < 5 && (int)(strlen(argv[1]) + strlen(argv[i])) > NAMESIZE) {
	    struct bu_vls tmp_vls;

	    bu_vls_init(&tmp_vls);
	    bu_vls_printf(&tmp_vls, "'%s%s' too long, must be %d characters or less.\n",
			  argv[1], argv[i], NAMESIZE);
	    Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
	    bu_vls_free(&tmp_vls);

	    argv[i] = "";
	    continue;
	}

	if ( dbip->dbi_version < 5 ) {
	    bu_strlcpy( tempstring_v4, argv[1], len);
	    bu_strlcat( tempstring_v4, argv[i], len);
	    tempstring = tempstring_v4;
	} else {
	    bu_vls_trunc( &tempstring_v5, 0 );
	    bu_vls_strcpy( &tempstring_v5, argv[1] );
	    bu_vls_strcat( &tempstring_v5, argv[i] );
	    tempstring = bu_vls_addr( &tempstring_v5 );
	}

	if ( db_lookup( dbip, tempstring, LOOKUP_QUIET ) != DIR_NULL ) {
	    aexists( tempstring );
	    argv[i] = "";
	    continue;
	}
	/*  Change object name in the directory. */
	if ( db_rename( dbip, dp, tempstring ) < 0 )  {
	    bu_vls_free( &tempstring_v5 );
	    Tcl_AppendResult(interp, "error in rename to ", tempstring,
			     ", aborting\n", (char *)NULL);
	    TCL_ERROR_RECOVERY_SUGGESTION;
	    return TCL_ERROR;
	}
    }

    bu_vls_free( &tempstring_v5 );

    /* Examine all COMB nodes */
    FOR_ALL_DIRECTORY_START(dp, dbip) {
	if ( !(dp->d_flags & DIR_COMB) )
	    continue;

	if ( rt_db_get_internal( &intern, dp, dbip, (fastf_t *)NULL, &rt_uniresource ) < 0 )
	    TCL_READ_ERR_return;
	comb = (struct rt_comb_internal *)intern.idb_ptr;

	for ( k=2; k<argc; k++ )
	    db_tree_funcleaf( dbip, comb, comb->tree, Do_prefix,
			      (genptr_t)argv[1], (genptr_t)argv[k], (genptr_t)NULL );
	if ( rt_db_put_internal( dp, dbip, &intern, &rt_uniresource ) )
	    TCL_WRITE_ERR_return;
    } FOR_ALL_DIRECTORY_END;
    return TCL_OK;
}

HIDDEN void
Change_name(struct db_i *dbip, struct rt_comb_internal *comb, union tree *comb_leaf, genptr_t old_ptr, genptr_t new_ptr)
{
    char	*old_name, *new_name;

    RT_CK_DBI( dbip );
    RT_CK_TREE( comb_leaf );

    old_name = (char *)old_ptr;
    new_name = (char *)new_ptr;

    if ( strcmp( comb_leaf->tr_l.tl_name, old_name ) )
	return;

    bu_free( comb_leaf->tr_l.tl_name, "comb_leaf->tr_l.tl_name" );
    comb_leaf->tr_l.tl_name = bu_strdup( new_name );
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
