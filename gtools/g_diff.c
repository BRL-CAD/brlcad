/*
 *			G _ D I F F . C
 *
 *	Routine to determine the differences between two BRL-CAD databases (".g" files).
 *	With no options, the output to stdout is an MGED script that may be fed to
 *	MGED to convert the first database to the match the second.
 *	The script uses the MGED "db" command to make the changes. Some solid types
 *	do not yet have support in the "db" command. Such solids that change from
 *	one database to the next, will be noted by a comment in the database as:
 *	"#IMPORT solid_name from database_name"
 *
 *  Author -
 *      John R. Anderson
 *
 *  Source -
 *      The U. S. Army Research Laboratory
 *      Aberdeen Proving Ground, Maryland  21005-5066
 *
 *  Distribution Notice -
 *      Re-distribution of this software is restricted, as described in
 *      your "Statement of Terms and Conditions for the Release of
 *      The BRL-CAD Pacakge" agreement.
 *
 *  Copyright Notice -
 *      This software is Copyright (C) 1998 by the United States Army
 *      in all countries except the USA.  All rights reserved.
 */

#ifndef lint
static const char RCSid[] = "$Header$";
#endif

#include "conf.h"

#include <stdio.h>
#include <math.h>
#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "machine.h"
#include "externs.h"
#include "vmath.h"
#include "nmg.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "wdb.h"
#include "../librt/debug.h"
#include "tcl.h"

extern int optind;
extern int optopt;

#define HUMAN	1
#define MGED	2

static int mode=HUMAN;
static Tcl_Interp *interp = NULL;

void
Usage( str )
char *str;
{
	bu_log( "Usage: %s [-m] file1.g file2.g\n", str );
}

void
kill_obj( name )
char *name;
{
	if( mode == HUMAN )
		printf( "%s has been killed\n", name );
	else
		printf( "kill %s\n", name );

}

void
compare_external( dp1, dbip1, dp2, dbip2 )
struct directory *dp1, *dp2;
struct db_i *dbip1, *dbip2;
{
	struct bu_external ext1, ext2;

	if( db_get_external( &ext1, dp1, dbip1 ) )
	{
		bu_log( "ERROR: db_get_external failed on solid %s in %s\n", dp1->d_namep, dbip1->dbi_filename );
		exit( 1 );
	}
	if( db_get_external( &ext2, dp2, dbip2 ) )
	{
		bu_log( "ERROR: db_get_external failed on solid %s in %s\n", dp2->d_namep, dbip2->dbi_filename );
		exit( 1 );
	}

	if( ext1.ext_nbytes != ext2.ext_nbytes ||
		bcmp( (void *)ext1.ext_buf, (void *)ext2.ext_buf, ext1.ext_nbytes ) )
	{
		if( mode == HUMAN )
			printf( "kill %s and import it from %s\n", dp1->d_namep, dbip1->dbi_filename );
		else
			printf( "kill %s\n# IMPORT %s from %s\n", dp1->d_namep, dp2->d_namep, dbip2->dbi_filename );
	}
}

void
do_adjusts( adjust, obj1, obj2 )
Tcl_Obj *obj1, *obj2;
struct bu_vls *adjust;
{
	Tcl_Obj *key1, *val1, *key2, *val2;
	int len1, len2, found, junk;
	int i, j;

	if( Tcl_ListObjLength( interp, obj1, &len1 ) == TCL_ERROR )
	{
		bu_log( "Error getting length of TCL object!!!\n" );
		bu_log( "%s\n", Tcl_GetStringResult( interp ) );
		exit ( 1 );
	}
	if( Tcl_ListObjLength( interp, obj2, &len2 ) == TCL_ERROR )
	{
		bu_log( "Error getting length of TCL object!!!\n" );
		bu_log( "%s\n", Tcl_GetStringResult( interp ) );
		exit ( 1 );
	}

	if( Tcl_ListObjIndex( interp, obj1, 0, &key1 ) == TCL_ERROR )
	{
		bu_log( "Error getting word #%d in TCL object (%s)!!!\n", 0, Tcl_GetStringFromObj( obj1, &junk ) );
		bu_log( "%s\n", Tcl_GetStringResult( interp ) );
		exit ( 1 );
	}

	/* check for changed values from object 1 to object2 */
	for( i=1 ; i<len1 ; i+=2 )
	{
		if( Tcl_ListObjIndex( interp, obj1, i, &key1 ) == TCL_ERROR )
		{
			bu_log( "Error getting word #%d in TCL object!!! (%s)\n", i, Tcl_GetStringFromObj( obj1, &junk ) );
			bu_log( "%s\n", Tcl_GetStringResult( interp ) );
			exit ( 1 );
		}

		if( Tcl_ListObjIndex( interp, obj1, i+1, &val1 ) == TCL_ERROR )
		{
			bu_log( "Error getting word #%d in TCL object!!! (%s)\n", i+1, Tcl_GetStringFromObj( obj1, &junk ) );
			bu_log( "%s\n", Tcl_GetStringResult( interp ) );
			exit ( 1 );
		}

		found = 0;
		for( j=1 ; j<len2 ; j += 2 )
		{
			if( Tcl_ListObjIndex( interp, obj2, j, &key2 ) == TCL_ERROR )
			{
				bu_log( "Error getting word #%d in TCL object!!! (%s)\n", j, Tcl_GetStringFromObj( obj2, &junk ) );
				bu_log( "%s\n", Tcl_GetStringResult( interp ) );
				exit ( 1 );
			}
			if( !strcmp( Tcl_GetStringFromObj( key1, &junk ), Tcl_GetStringFromObj( key2, &junk ) ) )
			{
				found = 1;
				if( Tcl_ListObjIndex( interp, obj2, j+1, &val2 ) == TCL_ERROR )
				{
					bu_log( "Error getting word #%d in TCL object!!! (%s)\n", j+1, Tcl_GetStringFromObj( obj2, &junk ) );
					bu_log( "%s\n", Tcl_GetStringResult( interp ) );
					exit ( 1 );
				}

				/* check if this value has changed */
				if( strcmp( Tcl_GetStringFromObj( val1, &junk ), Tcl_GetStringFromObj( val2, &junk ) ) )
				{
					if( mode == HUMAN )
					{
						printf( "\t%s has changed from:\n\t\t%s\n\tto:\n\t\t%s\n",
							Tcl_GetStringFromObj( key1, &junk ),
							Tcl_GetStringFromObj( val1, &junk ),
							Tcl_GetStringFromObj( val2, &junk ) );
					}
					else
					{
						int val_len;

						bu_vls_strcat( adjust, " " );
						bu_vls_strcat( adjust, Tcl_GetStringFromObj( key1, &junk ) );
						bu_vls_strcat( adjust, " " );
						if( Tcl_ListObjLength( interp, val2, &val_len ) == TCL_ERROR )
						{
							bu_log( "Error getting length of TCL object!!\n" );
							bu_log( "%s\n", Tcl_GetStringResult( interp ) );
							exit( 1 );
						}
						if( val_len > 1 )
							bu_vls_putc( adjust, '{' );
						bu_vls_strcat( adjust, Tcl_GetStringFromObj( val2, &junk ) );
						if( val_len > 1 )
							bu_vls_putc( adjust, '}' );
					}
				}
				break;
			}
		}
		if( !found )
		{
			/* this keyword value pair has been eliminated */
			if( mode == HUMAN )
			{
				printf( "\t%s has been eliminated\n",
					Tcl_GetStringFromObj( key1, &junk ) );
			}
			else
			{
				bu_vls_strcat( adjust, " " );
				bu_vls_strcat( adjust, Tcl_GetStringFromObj( key1, &junk ) );
				bu_vls_strcat( adjust, " none" );
			}
		}
	}

	/* check for keyword value pairs in object 2 that don't appear in object 1 */
	for( i=1 ; i<len2 ; i+= 2 )
	{
		/* get keyword/value pairs from object 2 */
		if( Tcl_ListObjIndex( interp, obj2, i, &key2 ) == TCL_ERROR )
		{
			bu_log( "Error getting word #%d in TCL object!!! (%s)\n", i, Tcl_GetStringFromObj( obj2, &junk ) );
			bu_log( "%s\n", Tcl_GetStringResult( interp ) );
			exit ( 1 );
		}

		if( Tcl_ListObjIndex( interp, obj2, i+1, &val2 ) == TCL_ERROR )
		{
			bu_log( "Error getting word #%d in TCL object!!! (%s)\n", i+1, Tcl_GetStringFromObj( obj2, &junk ) );
			bu_log( "%s\n", Tcl_GetStringResult( interp ) );
			exit ( 1 );
		}

		found = 0;
		/* look for this keyword in object 1 */
		for( j=1 ; j<len1 ; j += 2 )
		{
			if( Tcl_ListObjIndex( interp, obj1, j, &key1 ) == TCL_ERROR )
			{
				bu_log( "Error getting word #%d in TCL object!!! (%s)\n", i, Tcl_GetStringFromObj( obj1, &junk ) );
				bu_log( "%s\n", Tcl_GetStringResult( interp ) );
				exit ( 1 );
			}
			if( !strcmp( Tcl_GetStringFromObj( key1, &junk ), Tcl_GetStringFromObj( key2, &junk ) ) )
			{
				found = 1;
				break;
			}
		}
		if( found )
			continue;

		/* This keyword/value pair in object 2 is not in object 1 */
		if( mode == HUMAN )
			printf( "\tadd %s %s\n", Tcl_GetStringFromObj( key2, &junk ), Tcl_GetStringFromObj( val2, &junk ) );
		else
		{
			int val_len;

			bu_vls_strcat( adjust, " " );
			bu_vls_strcat( adjust, Tcl_GetStringFromObj( key2, &junk ) );
			bu_vls_strcat( adjust, " " );
			if( Tcl_ListObjLength( interp, val2, &val_len ) == TCL_ERROR )
			{
				bu_log( "Error getting length of TCL object!!\n" );
				bu_log( "%s\n", Tcl_GetStringResult( interp ) );
				exit( 1 );
			}
			if( val_len > 1 )
				bu_vls_putc( adjust, '{' );
			bu_vls_strcat( adjust, Tcl_GetStringFromObj( val2, &junk ) );
			if( val_len > 1 )
				bu_vls_putc( adjust, '}' );
		}
	}
}

void
compare_tcl_solids( str1, obj1, dp1, dbip1, str2, obj2, dp2, dbip2 )
char *str1, *str2;
Tcl_Obj *obj1, *obj2;
struct directory *dp1, *dp2;
struct db_i *dbip1, *dbip2;
{
	char *c1, *c2;
	struct bu_vls adjust;

	/* check if same solid type */
	c1 = str1;
	c2 = str2;
	while( *c1 != ' ' && *c2 != ' ' && *c1++ == *c2++ );

	if( *c1 != *c2 )
	{
		/* different solid types */
		if( mode == HUMAN )
			printf( "solid %s:\n\twas: %s\n\tis now: %s\n\n", dp1->d_namep, str1, str2 );
		else
			printf( "kill %s\ndb put %s %s\n", dp1->d_namep, dp1->d_namep, str2 );

		return;
	}
	else if( !strcmp( str1, str2 ) )
		return;		/* no difference */

	/* same solid type, can use "db adjust" */

	if( mode == HUMAN )
		printf( "%s has changed:\n", dp1->d_namep );
	else
	{
		bu_vls_init( &adjust );
		bu_vls_printf( &adjust, "db adjust %s", dp1->d_namep );
	}

	do_adjusts( &adjust, obj1, obj2 );

	if( mode != HUMAN )
	{
		printf( "%s\n", bu_vls_addr( &adjust ) );
		bu_vls_free( &adjust );
	}
}

void
compare_tcl_combs( obj1, dp1, dbip1, obj2, dp2, dbip2 )
Tcl_Obj *obj1, *obj2;
struct directory *dp1, *dp2;
struct db_i *dbip1, *dbip2;
{
	int junk;
	struct bu_vls adjust;

	/* first check if there is any difference */
	if( !strcmp( Tcl_GetStringFromObj( obj1, &junk ), Tcl_GetStringFromObj( obj2, &junk ) ) )
		return;

	if( mode != HUMAN )
	{
		bu_vls_init( &adjust );
		bu_vls_printf( &adjust, "db adjust %s", dp1->d_namep );
	}
	else
		printf( "%s has changed:\n", dp1->d_namep );

	do_adjusts( &adjust, obj1, obj2 );

	if( mode != HUMAN )
	{
		printf( "%s\n", bu_vls_addr( &adjust ) );
		bu_vls_free( &adjust );
	}
}

void
diff_objs( wdb1, wdb2 )
struct rt_wdb *wdb1, *wdb2;
{
	int i;
	struct directory *dp1, *dp2;
	struct db_i *dbip1, *dbip2;
	char *argv[4]={NULL, NULL, NULL, NULL};
	struct bu_vls s1_tcl, s2_tcl;

	RT_CK_WDB(wdb1);
	RT_CK_WDB(wdb2);
	dbip1 = wdb1->dbip;
	dbip2 = wdb2->dbip;

	bu_vls_init( &s1_tcl );
	bu_vls_init( &s2_tcl );

	/* look at all objects in this database */
	for( i = 0; i < RT_DBNHASH; i++)
	{
		for( dp1 = dbip1->dbi_Head[i]; dp1 != DIR_NULL; dp1 = dp1->d_forw )
		{
			char *str1, *str2;
			Tcl_Obj *obj1, *obj2;

			/* check if this object exists in the other database */
			if( (dp2 = db_lookup( dbip2, dp1->d_namep, 0 )) == DIR_NULL )
			{
				kill_obj( dp1->d_namep );
				continue;
			}

			/* skip the _GLOBAL object */
			if( dp1->d_major_type == DB5_MAJORTYPE_ATTRIBUTE_ONLY )
				continue;

			/* try to get the TCL version of this object */
			argv[2] = dp1->d_namep;
/* XXX Dangerous downcall.  Should invoke Tcl_Eval("db1 get name") */
			if( wdb_get_tcl( (ClientData)(wdb1), interp, 3, argv ) == TCL_ERROR || !strncmp( interp->result, "invalid", 7 ) )
			{
				/* cannot get TCL version, use bu_external */
				Tcl_ResetResult( interp );
				compare_external( dp1, dbip1, dp2, dbip2 );
				continue;
			}

			obj1 = Tcl_NewListObj( 0, NULL );
			Tcl_AppendToObj( obj1, interp->result, -1 );

			bu_vls_trunc( &s1_tcl, 0 );
			bu_vls_trunc( &s2_tcl, 0 );

			bu_vls_strcpy( &s1_tcl, Tcl_GetStringResult( interp ) );
			str1 = bu_vls_addr( &s1_tcl );
			Tcl_ResetResult( interp );

			/* try to get TCL version of object from the other database */				
/* XXX Dangerous downcall.  Should invoke Tcl_Eval("db1 get name") */
			if( wdb_get_tcl( (ClientData)(wdb2), interp, 3, argv ) == TCL_ERROR || !strncmp( interp->result, "invalid", 7 ) )
			{
				Tcl_ResetResult( interp );

				/* cannot get it, they MUST be different */
				if( mode == HUMAN )
					printf( "Replace %s with the same object from %s\n",
						dp1->d_namep, dbip2->dbi_filename );
				else
					printf( "kill %s\n# IMPORT %s from %s\n",
						dp1->d_namep, dp2->d_namep, dbip2->dbi_filename );
				continue;
			}

			obj2 = Tcl_NewListObj( 0, NULL );
			Tcl_AppendToObj( obj2, interp->result, -1 );

			bu_vls_strcpy( &s2_tcl , Tcl_GetStringResult( interp ) );
			str2 = bu_vls_addr( &s2_tcl );
			Tcl_ResetResult( interp );

			/* got TCL versions of both */
			if( (dp1->d_flags & DIR_SOLID) && (dp2->d_flags & DIR_SOLID) )
			{
				/* both are solids */
				compare_tcl_solids( str1, obj1, dp1, dbip1, str2, obj2, dp2, dbip2 );
				continue;
			}

			if( (dp1->d_flags & DIR_COMB) && (dp2->d_flags & DIR_COMB ) )
			{
				/* both are combinations */
				compare_tcl_combs( obj1, dp1, dbip1, obj2, dp2, dbip2 );
				continue;
			}

			/* the two objects are different types */
			if( strcmp( str1, str2 ) )
			{
				if( mode == HUMAN )
					printf( "%s:\n\twas: %s\n\tis now: %s\n\n",
						dp1->d_namep, str1, str2 );
				else
					printf( "kill %s\ndb put %s %s\n",
						dp1->d_namep, dp2->d_namep, str2 );
			}
		}
	}

	bu_vls_free( &s1_tcl );
	bu_vls_free( &s2_tcl );

	/* now look for objects in the other database that aren't here */
	for( i = 0; i < RT_DBNHASH; i++)
	{
		for( dp2 = dbip2->dbi_Head[i]; dp2 != DIR_NULL; dp2 = dp2->d_forw )
		{
			/* check if this object exists in the other database */
			if( (dp1 = db_lookup( dbip1, dp2->d_namep, 0 )) == DIR_NULL )
			{
				/* need to add this object */
				argv[2] = dp2->d_namep;
				if( wdb_get_tcl( (ClientData)(wdb2), interp, 3, argv ) == TCL_ERROR || !strncmp( interp->result, "invalid", 7 ) )
				{
					/* could not get TCL version */
					if( mode == HUMAN )
						printf( "Import %s from %s\n",
							dp2->d_namep, dbip2->dbi_filename );
					else
						printf( "# IMPORT %s from %s\n",
							dp2->d_namep, dbip2->dbi_filename );
				}
				else
				{
					if( mode == HUMAN )
						printf( "%s does not exist in %s\n",
							dp2->d_namep, dbip1->dbi_filename );
					else
						printf( "db put %s %s\n",
							dp2->d_namep, Tcl_GetStringResult( interp ) );
				}
				Tcl_ResetResult( interp );
				
			}
		}
	}
}

int
main( argc, argv )
int argc;
char *argv[];
{
	char *invoked_as;
	char *file1, *file2;
	struct rt_wdb *wdb1, *wdb2;
	struct db_i *dbip1, *dbip2;
	int c;

	invoked_as = argv[0];

	while ((c = getopt(argc, argv, "m")) != EOF)
	{
	 	switch( c )
		{
			case 'm':	/* mged readable */
				mode = MGED;
				break;
		}
	}

	argc -= optind;
	argv+= optind;

	if( argc != 2 )
	{
		Usage( invoked_as );
		exit( 1 );
	}

	file1 = *argv++;
	file2 = *argv;

	interp = Tcl_CreateInterp();
	if( Tcl_Init(interp) == TCL_ERROR )
	{
		bu_log( "Tcl_Init error %s\n", interp->result);
		exit( 1 );
	}

	if( (dbip1 = db_open( file1, "r" )) == DBI_NULL )
	{
		bu_log( "Cannot open %s\n", file1 );
		perror( argv[0] );
		exit( 1 );
	}

	RT_CK_DBI(dbip1);

	if( (wdb1 = wdb_dbopen( dbip1, RT_WDB_TYPE_DB_DISK )) == RT_WDB_NULL )
	{
		bu_log( "wdb_dbopen failed for %s\n", file1 );
		exit( 1 );
	}

	if( db_dirbuild( dbip1 ) < 0 )
	{
		db_close( dbip1 );
		bu_log( "db_dirbuild failed on %s\n", file1 );
		exit( 1 );
	}

	if( (dbip2 = db_open( file2, "r" )) == DBI_NULL )
	{
		bu_log( "Cannot open %s\n", file2 );
		perror( argv[0] );
		exit( 1 );
	}

	RT_CK_DBI(dbip2);

	if( db_dirbuild( dbip2 ) < 0 )
	{
		db_close( dbip1 );
		db_close( dbip2 );
		bu_log( "db_dirbuild failed on %s\n", file2 );
		exit( 1 );
	}

	if( (wdb2 = wdb_dbopen( dbip2, RT_WDB_TYPE_DB_DISK )) == RT_WDB_NULL )
	{
		db_close( dbip2 );
		wdb_close( wdb1 );
		bu_log( "wdb_dbopen failed for %s\n", file2 );
		exit( 1 );
	}

	if( mode == HUMAN)
		printf( "\nChanges from %s to %s\n\n", dbip1->dbi_filename, dbip2->dbi_filename );

	/* compare titles */
	if( strcmp( dbip1->dbi_title, dbip2->dbi_title ) )
	{
		if( mode == HUMAN )
			printf( "Title has changed from: \"%s\" to: \"%s\"\n\n", dbip1->dbi_title, dbip2->dbi_title );
		else
			printf( "title %s\n", dbip2->dbi_title );
	}

	/* and units */
	if( dbip1->dbi_local2base != dbip2->dbi_local2base )
	{
		if( mode == HUMAN )
			printf( "Units changed from %s to %s\n", rt_units_string(dbip1->dbi_local2base), rt_units_string(dbip2->dbi_local2base) );
		else
			printf( "units %s\n", rt_units_string(dbip2->dbi_local2base) );
	}

	/* next compare objects */
	diff_objs( wdb1, wdb2 );

	return( 0 );
}
