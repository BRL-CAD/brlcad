/*                      U T I L I T Y 2 . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2006 United States Government as represented by
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
#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "rtgeom.h"
#include "nmg.h"
#include "raytrace.h"
#include "wdb.h"

#include "./ged.h"
#include "./sedit.h"
#include "./cmd.h"
#include "../librt/debug.h"	/* XXX */


int
f_eac(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
	int i,j;
	int item;
	struct directory *dp;
	struct bu_vls v;
	int new_argc;
	int lim;

	CHECK_DBI_NULL;

	if(argc < 2){
	  struct bu_vls vls;

	  bu_vls_init(&vls);
	  bu_vls_printf(&vls, "help eac");
	  Tcl_Eval(interp, bu_vls_addr(&vls));
	  bu_vls_free(&vls);
	  return TCL_ERROR;
	}

	bu_vls_init( &v );

	if( setjmp( jmp_env ) == 0 )
	  (void)signal( SIGINT, sig3);  /* allow interupts */
        else{
	  bu_vls_free( &v );
	  return TCL_OK;
	}

	bu_vls_strcat( &v, "e" );
	lim = 1;

	for( j=1; j<argc; j++)
	{
		item = atoi( argv[j] );
		if( item < 1 )
			continue;

		FOR_ALL_DIRECTORY_START(dp, dbip) {
			struct rt_db_internal intern;
			struct rt_comb_internal *comb;

			if( !(dp->d_flags & DIR_REGION) )
				continue;

			if( rt_db_get_internal( &intern, dp, dbip, (fastf_t *)NULL, &rt_uniresource ) < 0 )
				TCL_READ_ERR_return;
			comb = (struct rt_comb_internal *)intern.idb_ptr;
			if( comb->region_id != 0 ||
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

	if( lim > 1 )
	{
		int retval;
		char **new_argv;

		new_argv = (char **)bu_calloc( lim+1, sizeof( char *), "f_eac: new_argv" );
		new_argc = rt_split_cmd( new_argv, lim+1, bu_vls_addr( &v ) );
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
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
