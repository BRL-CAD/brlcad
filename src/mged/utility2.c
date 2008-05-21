/*                      U T I L I T Y 2 . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2008 United States Government as represented by
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
/** @file utility2.c
 *
 *
 *	f_pathsum()	gives various path summaries
 *	f_copyeval()	copy an evaluated solid
 *	f_push()	control routine to push transformations to bottom of paths
 *	f_showmats()	shows matrices along a path
 *
 */

#include "common.h"

#include <stdlib.h>
#include <signal.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#include "bio.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "rtgeom.h"
#include "nmg.h"
#include "raytrace.h"
#include "wdb.h"

#include "./mged.h"
#include "./sedit.h"
#include "./cmd.h"


int
f_eac(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
    int j;
    int item;
    struct directory *dp;
    struct bu_vls v;
    int new_argc;
    int lim;

    CHECK_DBI_NULL;

    if (argc < 2) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "help eac");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    bu_vls_init( &v );

    if ( setjmp( jmp_env ) == 0 )
	(void)signal( SIGINT, sig3);  /* allow interupts */
    else {
	bu_vls_free( &v );
	return TCL_OK;
    }

    bu_vls_strcat( &v, "e" );
    lim = 1;

    for ( j=1; j<argc; j++)
    {
	item = atoi( argv[j] );
	if ( item < 1 )
	    continue;

	FOR_ALL_DIRECTORY_START(dp, dbip) {
	    struct rt_db_internal intern;
	    struct rt_comb_internal *comb;

	    if ( !(dp->d_flags & DIR_REGION) )
		continue;

	    if ( rt_db_get_internal( &intern, dp, dbip, (fastf_t *)NULL, &rt_uniresource ) < 0 )
		TCL_READ_ERR_return;
	    comb = (struct rt_comb_internal *)intern.idb_ptr;
	    if ( comb->region_id != 0 ||
		 comb->aircode != item )
	    {
		rt_comb_ifree( &intern, &rt_uniresource );
		continue;
	    }
	    rt_comb_ifree( &intern, &rt_uniresource );

	    bu_vls_strcat( &v, " " );
	    bu_vls_strcat( &v, dp->d_namep );
	    lim++;
	} FOR_ALL_DIRECTORY_END;
    }

    if ( lim > 1 )
    {
	int retval;
	const char **new_argv;

	new_argv = (char **)bu_calloc( lim+1, sizeof( char *), "f_eac: new_argv" );
	new_argc = bu_argv_from_string( new_argv, lim, bu_vls_addr( &v ) );
	retval = cmd_draw( clientData, interp, new_argc, new_argv );
	bu_free( (genptr_t)new_argv, "f_eac: new_argv" );
	bu_vls_free( &v );
	(void)signal( SIGINT, SIG_IGN );
	return retval;
    }
    else
    {
	bu_vls_free( &v );
	(void)signal( SIGINT, SIG_IGN );
	return TCL_OK;
    }
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
