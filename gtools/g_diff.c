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
#include <sys/types.h>
#include <sys/stat.h>
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
#include "mater.h"

static struct mater *mater_hd1=MATER_NULL, *mater_hd2=MATER_NULL;

extern int optind;
extern int optopt;

#define HUMAN	1
#define MGED	2

/* type of adjustment, for do_compare() */
#define	PARAMS	1
#define	ATTRS	2

static int mode=HUMAN;
static Tcl_Interp *interp = NULL;
static int pre_5_vers=0;
static int use_floats=0;	/* flag to use floats for comparisons */
static int verify_region_attribs=0;	/* flag to verify region attributes */
static struct db_i *dbip1, *dbip2;
static int version2;

void
compare_colors(void)
{
	struct mater *mp1, *mp2;
	int found1=0, found2=0;
	int is_diff=0;

	for( mp1 = mater_hd1; mp1 != MATER_NULL; mp1 = mp1->mt_forw )  {
		found1 = 0;
		mp2 = mater_hd2;
		while( mp2 != MATER_NULL ) {
			if( mp1->mt_low == mp2->mt_low &&
			    mp1->mt_high == mp2->mt_high &&
			    mp1->mt_r == mp2->mt_r &&
			    mp1->mt_g == mp2->mt_g &&
			    mp1->mt_b == mp2->mt_b ) {
				found1 = 1;
				break;
			} else {
				mp2 = mp2->mt_forw;
			}
		}
		if( !found1 )
			break;
	}
	for( mp2 = mater_hd2; mp2 != MATER_NULL; mp2 = mp2->mt_forw )  {
		found1 = 0;
		mp1 = mater_hd1;
		while( mp1 != MATER_NULL ) {
			if( mp1->mt_low == mp2->mt_low &&
			    mp1->mt_high == mp2->mt_high &&
			    mp1->mt_r == mp2->mt_r &&
			    mp1->mt_g == mp2->mt_g &&
			    mp1->mt_b == mp2->mt_b ) {
				found2 = 1;
				break;
			} else {
				mp1 = mp1->mt_forw;
			}
		}
		if( !found2 )
			break;
	}
	if( !found1 && !found2 ) {
		return;
	} else if( !found1 || !found2 ) {
		is_diff = 1;
	} else {
		/* actually compare two color tables */
		mp1 = mater_hd1;
		mp2 = mater_hd2;
		while( mp1 != MATER_NULL && mp2 != MATER_NULL ) {
			if( mp1->mt_low != mp2->mt_low ) {
				is_diff = 1;
				break;
			}
			if( mp1->mt_high != mp2->mt_high ) {
				is_diff = 1;
				break;
			}
			if( mp1->mt_r != mp2->mt_r ) {
				is_diff = 1;
				break;
			}
			if( mp1->mt_g != mp2->mt_g ) {
				is_diff = 1;
				break;
			}
			if( mp1->mt_b != mp2->mt_b ) {
				is_diff = 1;
				break;
			}
			mp1 = mp1->mt_forw;
			mp2 = mp2->mt_forw;
		}
	}

	if( is_diff ) {
		if( mode == HUMAN ) {
			printf( "Color table has changed from:\n" );
			for( mp1 = mater_hd1; mp1 != MATER_NULL; mp1 = mp1->mt_forw )  {
				printf( "\t%d..%d %d %d %d\n", mp1->mt_low, mp1->mt_high,
					mp1->mt_r, mp1->mt_g, mp1->mt_b );
			}
			printf( "\t\tto:\n" );
			for( mp2 = mater_hd2; mp2 != MATER_NULL; mp2 = mp2->mt_forw )  {
				printf( "\t%d..%d %d %d %d\n", mp2->mt_low, mp2->mt_high,
					mp2->mt_r, mp2->mt_g, mp2->mt_b );
			}
		} else {
			if( version2 > 4 )
				printf( "attr rm _GLOBAL regionid_colortable\n" );
			for( mp2 = mater_hd2; mp2 != MATER_NULL; mp2 = mp2->mt_forw )  {
				printf( "color %d %d %d %d %d\n", mp2->mt_low, mp2->mt_high,
					mp2->mt_r, mp2->mt_g, mp2->mt_b );
			}
		}
	}
}

void
Usage(char *str)
{
	fprintf( stderr, "Usage: %s [-m] file1.g file2.g\n", str );
}

void
kill_obj(char *name)
{
	if( mode == HUMAN ) {
		printf( "%s has been killed\n", name );
	} else {
		printf( "kill %s\n", name );
	}
}

void
compare_external(struct directory *dp1, struct directory *dp2)
{
	struct bu_external ext1, ext2;

	if( db_get_external( &ext1, dp1, dbip1 ) )
	{
		fprintf( stderr, "ERROR: db_get_external failed on solid %s in %s\n", dp1->d_namep, dbip1->dbi_filename );
		exit( 1 );
	}
	if( db_get_external( &ext2, dp2, dbip2 ) )
	{
		fprintf( stderr, "ERROR: db_get_external failed on solid %s in %s\n", dp2->d_namep, dbip2->dbi_filename );
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

int
compare_values( int type, Tcl_Obj *val1, Tcl_Obj *val2 )
{
	int len1, len2;
	int i;
	int str_ret;
	float a, b;
	Tcl_Obj *obj1, *obj2;

	str_ret = strcmp( Tcl_GetStringFromObj( val1, NULL ), Tcl_GetStringFromObj( val2, NULL ) );

	if( str_ret == 0 || type == ATTRS || !use_floats )
		return( str_ret );

	if( Tcl_ListObjLength( interp, val1, &len1 ) == TCL_ERROR )
	{
		fprintf( stderr, "Error getting length of TCL object!!!\n" );
		fprintf( stderr, "%s\n", Tcl_GetStringResult( interp ) );
		exit ( 1 );
	}

	if( Tcl_ListObjLength( interp, val2, &len2 ) == TCL_ERROR )
	{
		fprintf( stderr, "Error getting length of TCL object!!!\n" );
		fprintf( stderr, "%s\n", Tcl_GetStringResult( interp ) );
		exit ( 1 );
	}

	if( len1 != len2 )
		return 1;

	for( i=0 ; i<len1 ; i++ ) {
		if( Tcl_ListObjIndex( interp, val1, i, &obj1 ) == TCL_ERROR )
		{
			fprintf( stderr, "Error getting word #%d in TCL object!!! (%s)\n", i, Tcl_GetStringFromObj( val1, NULL ) );
			fprintf( stderr, "%s\n", Tcl_GetStringResult( interp ) );
			exit ( 1 );
		}
		if( Tcl_ListObjIndex( interp, val2, i, &obj2 ) == TCL_ERROR )
		{
			fprintf( stderr, "Error getting word #%d in TCL object!!! (%s)\n", i, Tcl_GetStringFromObj( val2, NULL ) );
			fprintf( stderr, "%s\n", Tcl_GetStringResult( interp ) );
			exit ( 1 );
		}
		a = atof( Tcl_GetStringFromObj( obj1, NULL ) );
		b = atof( Tcl_GetStringFromObj( obj2, NULL ) );

		if( a != b ) {
			return 1;
		}
	}

	return 0;
}

void
do_compare(int type, struct bu_vls *vls, Tcl_Obj *obj1, Tcl_Obj *obj2, char *obj_name)
{
	Tcl_Obj *key1, *val1, *key2, *val2;
	int len1, len2, found, junk;
	int i, j;
	int start_index;
	int found_diffs=0;

	if( Tcl_ListObjLength( interp, obj1, &len1 ) == TCL_ERROR )
	{
		fprintf( stderr, "Error getting length of TCL object!!!\n" );
		fprintf( stderr, "%s\n", Tcl_GetStringResult( interp ) );
		exit ( 1 );
	}
	if( Tcl_ListObjLength( interp, obj2, &len2 ) == TCL_ERROR )
	{
		fprintf( stderr, "Error getting length of TCL object!!!\n" );
		fprintf( stderr, "%s\n", Tcl_GetStringResult( interp ) );
		exit ( 1 );
	}

	if( !len1 && !len2 )
		return;

	if( type == ATTRS ) {
		start_index = 0;
	} else {
		start_index = 1;
	}

	/* check for changed values from object 1 to object2 */
	for( i=start_index ; i<len1 ; i+=2 )
	{
		if( Tcl_ListObjIndex( interp, obj1, i, &key1 ) == TCL_ERROR )
		{
			fprintf( stderr, "Error getting word #%d in TCL object!!! (%s)\n", i, Tcl_GetStringFromObj( obj1, &junk ) );
			fprintf( stderr, "%s\n", Tcl_GetStringResult( interp ) );
			exit ( 1 );
		}

		if( Tcl_ListObjIndex( interp, obj1, i+1, &val1 ) == TCL_ERROR )
		{
			fprintf( stderr, "Error getting word #%d in TCL object!!! (%s)\n", i+1, Tcl_GetStringFromObj( obj1, &junk ) );
			fprintf( stderr, "%s\n", Tcl_GetStringResult( interp ) );
			exit ( 1 );
		}

		found = 0;
		for( j=start_index ; j<len2 ; j += 2 )
		{
			if( Tcl_ListObjIndex( interp, obj2, j, &key2 ) == TCL_ERROR )
			{
				fprintf( stderr, "Error getting word #%d in TCL object!!! (%s)\n", j, Tcl_GetStringFromObj( obj2, &junk ) );
				fprintf( stderr, "%s\n", Tcl_GetStringResult( interp ) );
				exit ( 1 );
			}
			if( !strcmp( Tcl_GetStringFromObj( key1, &junk ), Tcl_GetStringFromObj( key2, &junk ) ) )
			{
				found = 1;
				if( Tcl_ListObjIndex( interp, obj2, j+1, &val2 ) == TCL_ERROR )
				{
					fprintf( stderr, "Error getting word #%d in TCL object!!! (%s)\n", j+1, Tcl_GetStringFromObj( obj2, &junk ) );
					fprintf( stderr, "%s\n", Tcl_GetStringResult( interp ) );
					exit ( 1 );
				}

				/* check if this value has changed */
				if( compare_values( type, val1, val2 ) )
				{
					if( !found_diffs++ ) {
						if( mode == HUMAN ) {
							printf( "%s has changed:\n", obj_name );
						}
					}
					if( mode == HUMAN )
					{
						if( type == PARAMS ) {
							printf( "\tparameter %s has changed from:\n\t\t%s\n\tto:\n\t\t%s\n",
								Tcl_GetStringFromObj( key1, &junk ),
								Tcl_GetStringFromObj( val1, &junk ),
								Tcl_GetStringFromObj( val2, &junk ) );
						} else {
							printf( "\t%s attribute \"%s\" has changed from:\n\t\t%s\n\tto:\n\t\t%s\n",
								obj_name,
								Tcl_GetStringFromObj( key1, &junk ),
								Tcl_GetStringFromObj( val1, &junk ),
								Tcl_GetStringFromObj( val2, &junk ) );
						}
					}
					else
					{
						int val_len;

						if( type == ATTRS ) {
							bu_vls_printf( vls, "attr set %s ", obj_name );
						} else {
							bu_vls_strcat( vls, " " );
						}
						bu_vls_strcat( vls, Tcl_GetStringFromObj( key1, &junk ) );
						bu_vls_strcat( vls, " " );
						if( Tcl_ListObjLength( interp, val2, &val_len ) == TCL_ERROR )
						{
							fprintf( stderr, "Error getting length of TCL object!!\n" );
							fprintf( stderr, "%s\n", Tcl_GetStringResult( interp ) );
							exit( 1 );
						}
						if( val_len > 1 )
							bu_vls_putc( vls, '{' );
						bu_vls_strcat( vls, Tcl_GetStringFromObj( val2, &junk ) );
						if( val_len > 1 )
							bu_vls_putc( vls, '}' );
						if( type == ATTRS ) {
							bu_vls_putc( vls, '\n' );
						}
					}
				}
				break;
			}
		}
		if( !found )
		{
			/* this keyword value pair has been eliminated */
			if( !found_diffs++ ) {
				if( mode == HUMAN ) {
					printf( "%s has changed:\n", obj_name );
				}
			}
			if( mode == HUMAN )
			{
				if( type == PARAMS ) {
					printf( "\tparameter %s has been eliminated\n",
						Tcl_GetStringFromObj( key1, &junk ) );
				} else {	
					printf( "\tattribute \"%s\" has been eliminated from %s\n",
						Tcl_GetStringFromObj( key1, &junk ), obj_name );
				}
			}
			else
			{
				if( type == ATTRS ) {
					bu_vls_printf( vls, "attr rm %s %s\n", obj_name,
						       Tcl_GetStringFromObj( key1, &junk ) );
				} else {
					bu_vls_strcat( vls, " " );
					bu_vls_strcat( vls, Tcl_GetStringFromObj( key1, &junk ) );
					bu_vls_strcat( vls, " none" );
				}
			}
		}
	}

	/* check for keyword value pairs in object 2 that don't appear in object 1 */
	for( i=start_index ; i<len2 ; i+= 2 )
	{
		/* get keyword/value pairs from object 2 */
		if( Tcl_ListObjIndex( interp, obj2, i, &key2 ) == TCL_ERROR )
		{
			fprintf( stderr, "Error getting word #%d in TCL object!!! (%s)\n", i, Tcl_GetStringFromObj( obj2, &junk ) );
			fprintf( stderr, "%s\n", Tcl_GetStringResult( interp ) );
			exit ( 1 );
		}

		if( Tcl_ListObjIndex( interp, obj2, i+1, &val2 ) == TCL_ERROR )
		{
			fprintf( stderr, "Error getting word #%d in TCL object!!! (%s)\n", i+1, Tcl_GetStringFromObj( obj2, &junk ) );
			fprintf( stderr, "%s\n", Tcl_GetStringResult( interp ) );
			exit ( 1 );
		}

		found = 0;
		/* look for this keyword in object 1 */
		for( j=start_index ; j<len1 ; j += 2 )
		{
			if( Tcl_ListObjIndex( interp, obj1, j, &key1 ) == TCL_ERROR )
			{
				fprintf( stderr, "Error getting word #%d in TCL object!!! (%s)\n", i, Tcl_GetStringFromObj( obj1, &junk ) );
				fprintf( stderr, "%s\n", Tcl_GetStringResult( interp ) );
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
		if( !found_diffs++ ) {
			if( mode == HUMAN ) {
				printf( "%s has changed:\n", obj_name );
			}
		}
		if( mode == HUMAN ) {
			if( type == PARAMS ) {
				printf( "\t%s has new parameter \"%s\" with value %s\n",
					obj_name,
					Tcl_GetStringFromObj( key2, &junk ),
					Tcl_GetStringFromObj( val2, &junk ) );
			} else {
				printf( "\t%s has new attribute \"%s\" with value {%s}\n",
					obj_name,
					Tcl_GetStringFromObj( key2, &junk ),
					Tcl_GetStringFromObj( val2, &junk ) );
			}
		}
		else
		{
			int val_len;

			if( type == ATTRS ) {
				bu_vls_printf( vls, "attr set %s ", obj_name );
			} else {
				bu_vls_strcat( vls, " " );
			}
			bu_vls_strcat( vls, Tcl_GetStringFromObj( key2, &junk ) );
			bu_vls_strcat( vls, " " );
			if( Tcl_ListObjLength( interp, val2, &val_len ) == TCL_ERROR )
			{
				fprintf( stderr, "Error getting length of TCL object!!\n" );
				fprintf( stderr, "%s\n", Tcl_GetStringResult( interp ) );
				exit( 1 );
			}
			if( val_len > 1 )
				bu_vls_putc( vls, '{' );
			bu_vls_strcat( vls, Tcl_GetStringFromObj( val2, &junk ) );
			if( val_len > 1 )
				bu_vls_putc( vls, '}' );

			if( type == ATTRS )
				bu_vls_putc( vls, '\n' );
		}
	}
}

void
compare_tcl_solids(char *str1, Tcl_Obj *obj1, struct directory *dp1, char *str2, Tcl_Obj *obj2, struct directory *dp2)
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

	if( mode == MGED )
	{
		bu_vls_init( &adjust );
		bu_vls_printf( &adjust, "db adjust %s", dp1->d_namep );
	}

	do_compare( PARAMS, &adjust, obj1, obj2, dp1->d_namep );

	if( mode != HUMAN )
	{
		printf( "%s\n", bu_vls_addr( &adjust ) );
		bu_vls_free( &adjust );
	}
}

void
compare_tcl_combs(Tcl_Obj *obj1, struct directory *dp1, Tcl_Obj *obj2, struct directory *dp2)
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

	do_compare( PARAMS, &adjust, obj1, obj2, dp1->d_namep );

	if( mode != HUMAN )
	{
		printf( "%s\n", bu_vls_addr( &adjust ) );
		bu_vls_free( &adjust );
	}
}

void
verify_region_attrs( struct directory *dp, struct db_i *dbip, Tcl_Obj *obj )
{
	Tcl_Obj **objs;
	int len=0;
	int i;
	struct rt_db_internal intern;
	struct rt_comb_internal *comb;

	if( rt_db_get_internal( &intern, dp, dbip, NULL, &rt_uniresource ) < 0 ) {
		fprintf( stderr, "Cannot import %s\n", dp->d_namep );
		exit( 1 );
	}

	comb = (struct rt_comb_internal *)intern.idb_ptr;
	RT_CHECK_COMB( comb );

	if( Tcl_ListObjGetElements( interp, obj, &len, &objs ) != TCL_OK ) {
		fprintf( stderr, "Cannot get length of attributes for %s\n", dp->d_namep );
		exit( 1 );
	}

	for( i=1 ; i<len ; i += 2 ) {
		char *key, *value;

		key = Tcl_GetStringFromObj( objs[i-1], NULL );
		value = Tcl_GetStringFromObj( objs[i], NULL );
		if( !strcmp( key, "region_id" ) ) {
			short id;

			id = atoi( value );
			if( id != comb->region_id ) {
				fprintf( stderr, "WARNING: %s in %s: \"region_id\" attribute says %d, while region says %d\n",
					 dp->d_namep, dbip->dbi_filename, id, comb->region_id );
			}
		} else if( !strcmp( key, "giftmater" ) ) {
			short GIFTmater;

			GIFTmater = atoi( value );
			if( GIFTmater != comb->GIFTmater ) {
				fprintf( stderr, "WARNING: %s in %s: \"giftmater\" attribute says %d, while region says %d\n",
					 dp->d_namep, dbip->dbi_filename, GIFTmater, comb->GIFTmater );
			}
		} else if( !strcmp( key, "los" ) ) {
			short los;

			los = atoi( value );
			if( los != comb->los ) {
				fprintf( stderr, "WARNING: %s in %s: \"los\" attribute says %d, while region says %d\n",
					 dp->d_namep, dbip->dbi_filename, los, comb->los );
			}
		} else if( !strcmp( key, "material" ) ) {
			if( !strncmp( value, "gift", 4 ) ) {
				short GIFTmater;

				GIFTmater = atoi( &value[4] );
				if( GIFTmater != comb->GIFTmater ) {
					fprintf( stderr, "WARNING: %s in %s: \"material\" attribute says %s, while region says %d\n",
						 dp->d_namep, dbip->dbi_filename, value, comb->GIFTmater );
				}
			}
		} else if( !strcmp( key, "aircode" ) ) {
			short aircode;

			aircode = atoi( value );
			if( aircode != comb->aircode ) {
				fprintf( stderr, "WARNING: %s in %s: \"aircode\" attribute says %d, while region says %d\n",
					 dp->d_namep, dbip->dbi_filename, aircode, comb->aircode );
			}
		}
	}
	rt_db_free_internal( &intern, &rt_uniresource );
}

static char *region_attrs[] = { "region",
			      "region_id",
			      "giftmater",
			      "los",
			      "aircode",
			      NULL };
void
remove_region_attrs( Tcl_Obj *obj )
{
	int len=0;
	Tcl_Obj **objs;
	Tcl_Obj *new_obj;
	char *key;
	int i,j;
	int found_material=0;

	if( Tcl_ListObjGetElements( interp, obj, &len, &objs ) != TCL_OK ) {
		fprintf( stderr, "Cannot get length of attributes for %s\n",
			 Tcl_GetStringFromObj( obj, NULL ) );
		exit( 1 );
	}

	if( len == 0 )
		return;

	new_obj = Tcl_NewObj();

	for( i=len-1 ; i>0 ; i -= 2 ) {

		key = Tcl_GetStringFromObj( objs[i-1], NULL );
		j = 0;
		while( region_attrs[j] ) {
			if( !strcmp( key, region_attrs[j] ) ) {
				Tcl_ListObjReplace(interp, obj, i-1, 2, 0, NULL);
				break;
			}
			j++;
		}
		if( !found_material && !strcmp( key, "material" ) ) {
			found_material = 1;
			if( !strncmp( Tcl_GetStringFromObj( objs[i], NULL ), "gift", 4 ) ) {
				Tcl_ListObjReplace(interp, obj, i-1, 2, 0, NULL);
			}
		}
	}
}


void
compare_attrs( struct directory *dp1, struct directory *dp2 )
{
	struct bu_vls vls;
	Tcl_Obj *obj1, *obj2;

	bu_vls_init( &vls );

	if( dbip1->dbi_version > 4 ) {
		bu_vls_printf( &vls, "db1 attr get %s", dp1->d_namep );
		if( Tcl_Eval( interp, bu_vls_addr( &vls ) ) != TCL_OK ) {
			fprintf( stderr, "Cannot get attributes for %s\n", dp1->d_namep );
			fprintf( stderr, "%s\n", Tcl_GetStringResult( interp ) );
			exit( 1 );
		}

		obj1 = Tcl_DuplicateObj( Tcl_GetObjResult( interp ) );
		Tcl_ResetResult( interp );
		if( dp1->d_flags & DIR_REGION && verify_region_attribs ) {
			verify_region_attrs( dp1, dbip1, obj1 );
		}
	} else {
		obj1 = Tcl_NewObj();
	}

	if( dbip2->dbi_version > 4 ) {
		bu_vls_trunc( &vls, 0 );
		bu_vls_printf( &vls, "db2 attr get %s", dp1->d_namep );
		if( Tcl_Eval( interp, bu_vls_addr( &vls ) ) != TCL_OK ) {
			fprintf( stderr, "Cannot get attributes for %s\n", dp1->d_namep );
			fprintf( stderr, "%s\n", Tcl_GetStringResult( interp ) );
			exit( 1 );
		}

		obj2 = Tcl_DuplicateObj( Tcl_GetObjResult( interp ) );
		Tcl_ResetResult( interp );
		if( dp1->d_flags & DIR_REGION && verify_region_attribs ) {
			verify_region_attrs( dp2, dbip2, obj2 );
		}
	} else {
		obj2 = Tcl_NewObj();
	}

	if( (dp1->d_flags & DIR_REGION) && (dp2->d_flags & DIR_REGION) ) {
		/* don't complain about "region" attributes */
		remove_region_attrs( obj1 );
		remove_region_attrs( obj2 );
	}

	bu_vls_trunc( &vls, 0 );
	do_compare( ATTRS, &vls, obj1, obj2, dp1->d_namep );

	printf( "%s", bu_vls_addr( &vls ) );
	bu_vls_free( &vls );
}

void
diff_objs(struct rt_wdb *wdb1, struct rt_wdb *wdb2)
{
	int i;
	struct directory *dp1, *dp2;
	char *argv[4]={NULL, NULL, NULL, NULL};
	struct bu_vls s1_tcl, s2_tcl;
	struct bu_vls vls;

	RT_CK_WDB(wdb1);
	RT_CK_WDB(wdb2);

	bu_vls_init( &s1_tcl );
	bu_vls_init( &s2_tcl );
	bu_vls_init( &vls );

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
			bu_vls_trunc( &vls, 0 );
			bu_vls_printf( &vls, "db1 get %s", dp1->d_namep );
			if( Tcl_Eval( interp, bu_vls_addr( &vls ) ) != TCL_OK )
			{
				/* cannot get TCL version, use bu_external */
				Tcl_ResetResult( interp );
				compare_external( dp1, dp2 );
				continue;
			}

			obj1 = Tcl_NewListObj( 0, NULL );
			Tcl_AppendObjToObj( obj1, Tcl_GetObjResult( interp ) );

			bu_vls_trunc( &s1_tcl, 0 );
			bu_vls_trunc( &s2_tcl, 0 );

			bu_vls_strcpy( &s1_tcl, Tcl_GetStringResult( interp ) );
			str1 = bu_vls_addr( &s1_tcl );
			Tcl_ResetResult( interp );

			/* try to get TCL version of object from the other database */				
			bu_vls_trunc( &vls, 0 );
			bu_vls_printf( &vls, "db2 get %s", dp1->d_namep );
			if( Tcl_Eval( interp, bu_vls_addr( &vls ) ) != TCL_OK )
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
			Tcl_AppendObjToObj( obj2, Tcl_GetObjResult( interp ) );

			bu_vls_strcpy( &s2_tcl , Tcl_GetStringResult( interp ) );
			str2 = bu_vls_addr( &s2_tcl );
			Tcl_ResetResult( interp );

			/* got TCL versions of both */
			if( (dp1->d_flags & DIR_SOLID) && (dp2->d_flags & DIR_SOLID) )
			{
				/* both are solids */
				compare_tcl_solids( str1, obj1, dp1, str2, obj2, dp2 );
				if( pre_5_vers != 2 )
					compare_attrs( dp1, dp2 );
				continue;
			}

			if( (dp1->d_flags & DIR_COMB) && (dp2->d_flags & DIR_COMB ) )
			{
				/* both are combinations */
				compare_tcl_combs( obj1, dp1, obj2, dp2 );
				if( !pre_5_vers != 2 )
					compare_attrs( dp1, dp2 );
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
			/* skip the _GLOBAL object */
			if( dp2->d_major_type == DB5_MAJORTYPE_ATTRIBUTE_ONLY )
				continue;

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
main(int argc, char **argv)
{
	char *invoked_as;
	char *file1, *file2;
	struct rt_wdb *wdb1, *wdb2;
	struct stat stat1, stat2;
	int c;

	invoked_as = argv[0];

	while ((c = getopt(argc, argv, "mfv")) != EOF)
	{
	 	switch( c )
		{
			case 'm':	/* mged readable */
				mode = MGED;
				break;
			case 'f':
				use_floats = 1;
				break;
			case 'v':	/* verify region attributes */
				verify_region_attribs = 1;
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

	if( stat( file1, &stat1 ) ) {
		fprintf( stderr, "Cannot stat file %s\n", file1 );
		perror( file1 );
		exit( 1 );
	}

	if( stat( file2, &stat2 ) ) {
		fprintf( stderr, "Cannot stat file %s\n", file2 );
		perror( file2 );
		exit( 1 );
	}

	if( stat1.st_dev == stat2.st_dev && stat1.st_ino == stat2.st_ino ) {
		fprintf( stderr, "%s and %s are the same file\n", file1, file2 );
		fprintf( stderr, "Cannot compare a file to itself!!\n" );
		exit( 1 );
	}

	interp = Tcl_CreateInterp();
	if( Tcl_Init(interp) == TCL_ERROR )
	{
		fprintf( stderr, "Tcl_Init error %s\n", interp->result);
		exit( 1 );
	}

	Rt_Init( interp );

	if( (dbip1 = db_open( file1, "r" )) == DBI_NULL )
	{
		fprintf( stderr, "Cannot open %s\n", file1 );
		perror( argv[0] );
		exit( 1 );
	}

	RT_CK_DBI(dbip1);

	if( (wdb1 = wdb_dbopen( dbip1, RT_WDB_TYPE_DB_DISK )) == RT_WDB_NULL )
	{
		fprintf( stderr, "wdb_dbopen failed for %s\n", file1 );
		exit( 1 );
	}

	if( db_dirbuild( dbip1 ) < 0 )
	{
		db_close( dbip1 );
		fprintf( stderr, "db_dirbuild failed on %s\n", file1 );
		exit( 1 );
	}

	if( wdb_init_obj( interp, wdb1, "db1") != TCL_OK ) {
		wdb_close( wdb1 );
		fprintf( stderr, "wdb_init_obj failed on %s\n", file1 );
		exit( 1 );
	}

	/* save regionid colortable */
	mater_hd1 = rt_material_head;
	rt_material_head = MATER_NULL;

	if( dbip1->dbi_version < 5 ) {
		pre_5_vers++;
	}

	if( (dbip2 = db_open( file2, "r" )) == DBI_NULL )
	{
		fprintf( stderr, "Cannot open %s\n", file2 );
		perror( argv[0] );
		exit( 1 );
	}

	RT_CK_DBI(dbip2);

	if( db_dirbuild( dbip2 ) < 0 )
	{
		db_close( dbip1 );
		db_close( dbip2 );
		fprintf( stderr, "db_dirbuild failed on %s\n", file2 );
		exit( 1 );
	}

	if( (wdb2 = wdb_dbopen( dbip2, RT_WDB_TYPE_DB_DISK )) == RT_WDB_NULL )
	{
		db_close( dbip2 );
		wdb_close( wdb1 );
		fprintf( stderr, "wdb_dbopen failed for %s\n", file2 );
		exit( 1 );
	}

	if( wdb_init_obj( interp, wdb2, "db2") != TCL_OK ) {
		wdb_close( wdb1 );
		wdb_close( wdb2 );
		fprintf( stderr, "wdb_init_obj failed on %s\n", file2 );
		exit( 1 );
	}

	/* save regionid colortable */
	mater_hd2 = rt_material_head;
	rt_material_head = MATER_NULL;

	if( dbip2->dbi_version < 5 ) {
		pre_5_vers++;
		version2 = 4;
	} else {
		version2 = 5;
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

	/* and color table */
	compare_colors();

	/* next compare objects */
	diff_objs( wdb1, wdb2 );

	return( 0 );
}
