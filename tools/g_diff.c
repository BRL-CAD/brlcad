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
		bu_log( "%s has been killed\n", name );
	else
		bu_log( "kill %s\n", name );

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
			bu_log( "kill %s and import it from %s\n", dp1->d_namep, dbip1->dbi_filename );
		else
			bu_log( "kill %s\n# IMPORT %s from %s", dp1->d_namep, dp2->d_namep, dbip2->dbi_filename );
	}
}

void
compare_tcl_solids( str1, dp1, dbip1, str2, dp2, dbip2 )
char *str1, *str2;
struct directory *dp1, *dp2;
struct db_i *dbip1, *dbip2;
{
	char *c1, *c2;

	/* check if same solid type */
	c1 = str1;
	c2 = str2;
	while( *c1 != ' ' && *c2 != ' ' && *c1++ == *c2++ );

	if( *c1 != *c2 )
	{
		/* different solid types */
		if( mode == HUMAN )
			bu_log( "solid %s:\n\twas: %s\n\tis now: %s\n", dp1->d_namep, str1, str2 );
		else
			bu_log( "kill %s\ndb put %s %s\n", dp1->d_namep, dp1->d_namep, str2 );
	}
	else if( strcmp( str1, str2 ) )
	{
		/* same solid type */
		/* XXXX can add more code to limit number of args to 'db adjust' */
		if( mode == HUMAN )
			bu_log( "solid %s:\n\twas: %s\n\tis now: %s\n", dp1->d_namep, str1, str2 );
		else
			bu_log( "db adjust %s %s\n", dp1->d_namep, c2 );
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

			/* check if this object exists in the other database */
			if( (dp2 = db_lookup( dbip2, dp1->d_namep, 0 )) == DIR_NULL )
			{
				kill_obj( dp1->d_namep );
				continue;
			}

			/* try to get the TCL version of this object */
			argv[2] = dp1->d_namep;
			if( rt_db_get( (ClientData)(wdb1), interp, 3, argv ) == TCL_ERROR || !strncmp( interp->result, "invalid", 7 ) )
			{
				/* cannot get TCL version, use bu_external */
				Tcl_ResetResult( interp );
				compare_external( dp1, dbip1, dp2, dbip2 );
				continue;
			}

			bu_vls_trunc( &s1_tcl, 0 );
			bu_vls_trunc( &s2_tcl, 0 );

			bu_vls_strcpy( &s1_tcl, Tcl_GetStringResult( interp ) );
			str1 = bu_vls_addr( &s1_tcl );
			Tcl_ResetResult( interp );

			/* try to get TCL version of object from the other database */				
			if( rt_db_get( (ClientData)(wdb2), interp, 3, argv ) == TCL_ERROR || !strncmp( interp->result, "invalid", 7 ) )
			{
				Tcl_ResetResult( interp );

				/* cannot get it, they MUST be different */
				if( mode == HUMAN )
					bu_log( "Replace %s with the same object from %s\n",
						dp1->d_namep, dbip2->dbi_filename );
				else
					bu_log( "kill %s\n# IMPORT %s from %s\n",
						dp1->d_namep, dp2->d_namep, dbip2->dbi_filename );
				continue;
			}
			bu_vls_strcpy( &s2_tcl , Tcl_GetStringResult( interp ) );
			str2 = bu_vls_addr( &s2_tcl );
			Tcl_ResetResult( interp );

			/* got TCL versions of both */
			if( (dp1->d_flags & DIR_SOLID) && (dp2->d_flags & DIR_SOLID) )
			{
				/* both are solids */
				compare_tcl_solids( str1, dp1, dbip1, str2, dp2, dbip2 );
				continue;
			}

			/* at least one is a combination */
			if( strcmp( str1, str2 ) )
			{
				if( mode == HUMAN )
					bu_log( "%s:\n\twas: %s\n\tis now: %s\n",
						dp1->d_namep, str1, str2 );
				else
					bu_log( "kill %s\ndb put %s %s\n",
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
				if( rt_db_get( (ClientData)(wdb2), interp, 3, argv ) == TCL_ERROR || !strncmp( interp->result, "invalid", 7 ) )
				{
					/* could not get TCL version */
					if( mode == HUMAN )
						bu_log( "Import %s from %s\n",
							dp2->d_namep, dbip2->dbi_filename );
					else
						bu_log( "# IMPORT %s from %s\n",
							dp2->d_namep, dbip2->dbi_filename );
				}
				else
				{
					if( mode == HUMAN )
						bu_log( "%s does not exist in %s\n",
							dp2->d_namep, dbip1->dbi_filename );
					else
						bu_log( "db put %s %s\n",
							dp2->d_namep, Tcl_GetStringResult( interp ) );
				}
				Tcl_ResetResult( interp );
				
			}
		}
	}
}

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

	bu_log( "# " );
	if( (wdb1 = wdb_dbopen( dbip1, RT_WDB_TYPE_DB_DISK )) == RT_WDB_NULL )
	{
		bu_log( "wdb_dbopen failed for %s\n", file1 );
		exit( 1 );
	}

	if( db_scan( dbip1, (int (*)())db_diradd, 1 ) < 0 )
	{
		db_close( dbip1 );
		bu_log( "db_scan failed on %s\n", file1 );
		exit( 1 );
	}

	if( (dbip2 = db_open( file2, "r" )) == DBI_NULL )
	{
		bu_log( "Cannot open %s\n", file2 );
		perror( argv[0] );
		exit( 1 );
	}

	RT_CK_DBI(dbip2);

	if( db_scan( dbip2, (int (*)())db_diradd, 1 ) < 0 )
	{
		db_close( dbip1 );
		db_close( dbip2 );
		bu_log( "db_scan failed on %s\n", file2 );
		exit( 1 );
	}

	bu_log( "# " );
	if( (wdb2 = wdb_dbopen( dbip2, RT_WDB_TYPE_DB_DISK )) == RT_WDB_NULL )
	{
		db_close( dbip2 );
		wdb_close( wdb1 );
		bu_log( "wdb_dbopen failed for %s\n", file2 );
		exit( 1 );
	}

	/* compare titles */
	if( strcmp( dbip1->dbi_title, dbip2->dbi_title ) )
	{
		if( mode == HUMAN )
			bu_log( "Title has changed from: \"%s\" to: \"%s\"\n", dbip1->dbi_title, dbip2->dbi_title );
		else
			bu_log( "title %s\n", dbip2->dbi_title );
	}

	/* and units */
	if( dbip1->dbi_local2base != dbip2->dbi_local2base )
	{
		if( mode == HUMAN )
			bu_log( "Units changed from %s to %s\n", rt_units_string(dbip1->dbi_local2base), rt_units_string(dbip2->dbi_local2base) );
		else
			bu_log( "units %s\n", rt_units_string(dbip2->dbi_local2base) );
	}

	/* next compare objects */
	diff_objs( wdb1, wdb2 );
}
