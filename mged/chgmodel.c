/*
 *			C H G M O D E L
 *
 * This module contains functions which change particulars of the
 * model, generally on a single solid or combination.
 * Changes to the tree structure of the model are done in chgtree.c
 *
 * Functions -
 *	f_itemair	add/modify item and air codes of a region
 *	f_mater		modify material information
 *	f_mirror	mirror image
 *	f_edcomb	modify combination record info
 *	f_units		change local units of description
 *	f_title		change current title of description
 *	aexists		announce already exists
 *	f_make		create new solid of given type
 *	f_rot_obj	allow precise changes to object rotation
 *	f_sc_obj	allow precise changes to object scaling
 *	f_tr_obj	allow precise changes to object translation
 *
 *  Author -
 *	Michael John Muuss
 *	Keith A. Applin
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1985 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <math.h>
#include <signal.h>
#include <stdio.h>
#ifdef BSD
#include <strings.h>
#else
#include <string.h>
#endif

#include "machine.h"
#include "vmath.h"
#include "externs.h"
#include "db.h"
#include "rtstring.h"
#include "nmg.h"
#include "raytrace.h"
#include "./ged.h"
#include "./solid.h"
#include "./dm.h"
#include "./sedit.h"

void	aexists();

int		newedge;		/* new edge for arb editing */

/* Add/modify item and air codes of a region */
/* Format: I region item <air>	*/
void
f_itemair( argc, argv )
int	argc;
char	**argv;
{
	register struct directory *dp;
	int ident, air;
	union record record;

	if( (dp = db_lookup( dbip,  argv[1], LOOKUP_NOISY )) == DIR_NULL )
		return;

	air = ident = 0;
	ident = atoi( argv[2] );

	/* If <air> is not included, it is assumed to be zero */
	if( argc == 4 )  {
		air = atoi( argv[3] );
	}
	if( db_get( dbip,  dp, &record, 0 , 1) < 0 )  READ_ERR_return;
	if( record.u_id != ID_COMB ) {
		(void)printf("%s: not a combination\n", dp->d_namep );
		return;
	}
	if( record.c.c_flags != 'R' ) {
		(void)printf("%s: not a region\n", dp->d_namep );
		return;
	}
	record.c.c_regionid = ident;
	record.c.c_aircode = air;
	if( db_put( dbip, dp, &record, 0, 1 ) < 0 )  WRITE_ERR_return;
}

/* Modify material information */
/* Usage:  mater name */
void
f_mater( argc, argv )
int	argc;
char	**argv;
{
	register struct directory *dp;
	char line[80];
	union record record;
	int r=0, g=0, b=0;
	char	*nlp;

	if( (dp = db_lookup( dbip,  argv[1], LOOKUP_NOISY )) == DIR_NULL )
		return;

	if( db_get( dbip,  dp, &record, 0 , 1) < 0 )  READ_ERR_return;
	if( record.u_id != ID_COMB )  {
		(void)printf("%s: not a combination\n", dp->d_namep );
		return;
	}
	if( argc >= 3 )  {
		if( strncmp( argv[2], "del", 3 ) != 0 )  {
			(void)printf("Use 'mater name del' to delete\n");
			return;
		}
		(void)printf("Was %s %s\n", record.c.c_matname, record.c.c_matparm);
		record.c.c_matname[0] = '\0';
		record.c.c_override = 0;
		goto out;
	}

	/* Material */
	(void)printf("Material = %s\nMaterial?  ('del' to delete, CR to skip) ", record.c.c_matname);
	fflush(stdout);
	(void)fgets(line,sizeof(line),stdin);
	nlp = strchr( line, '\n' );
	if( strncmp(line, "del", 3) == 0 )  {
		record.c.c_matname[0] = '\0';
	} else if( line[0] != '\n' && line[0] != '\0' ) {
		if( nlp != NULL )  *nlp = '\0';
		strncpy( record.c.c_matname, line,
			sizeof(record.c.c_matname)-1);
	}

	/* Parameters */
	(void)printf("Param = %s\nParameter string? ('del' to delete, CR to skip) ", record.c.c_matparm);
	fflush(stdout);
	(void)fgets(line,sizeof(line),stdin);
	nlp = strchr( line, '\n' );
	if( strncmp(line, "del", 3) == 0  )  {
		record.c.c_matparm[0] = '\0';
	} else if( line[0] != '\n' && line[0] != '\0' ) {
		if( nlp != NULL )  *nlp = '\0';
		strncpy( record.c.c_matparm, line,
			sizeof(record.c.c_matparm)-1 );
	}

	/* Color */
	if( record.c.c_override )
		(void)printf("Color = %d %d %d\n", 
			record.c.c_rgb[0],
			record.c.c_rgb[1],
			record.c.c_rgb[2] );
	else
		(void)printf("Color = (No color specified)\n");
	(void)printf("Color R G B (0..255)? ('del' to delete, CR to skip) ");
	fflush(stdout);
	(void)fgets(line,sizeof(line),stdin);
	if( strncmp(line, "del", 3) == 0 ) {
		record.c.c_override = 0;
	} else if( sscanf(line, "%d %d %d", &r, &g, &b) >= 3 )  {
		record.c.c_rgb[0] = r;
		record.c.c_rgb[1] = g;
		record.c.c_rgb[2] = b;
		record.c.c_override = 1;
	} else {
		/* Else, leave it unchanged */
		printf(" (color unchanged)\n");
	}

	/* Inherit */
	switch( record.c.c_inherit )  {
	default:
		/* This is necessary to clean up old databases with grunge here */
		record.c.c_inherit = DB_INH_LOWER;
		/* Fall through */
	case DB_INH_LOWER:
		(void)printf("Inherit = 0:  lower nodes (towards leaves) override\n");
		break;
	case DB_INH_HIGHER:
		(void)printf("Inherit = 1:  higher nodes (towards root) override\n");
		break;
	}
	(void)printf("Inheritance (0|1)? (CR to skip) ");
	fflush(stdout);
	(void)fgets(line,sizeof(line),stdin);
	switch( line[0] )  {
	case '1':
		record.c.c_inherit = DB_INH_HIGHER;
		break;
	case '0':
		record.c.c_inherit = DB_INH_LOWER;
		break;
	case '\0':
	case '\n':
		break;
	default:
		(void)printf("Unknown response ignored\n");
		break;
	}		
out:
	if( db_put( dbip, dp, &record, 0, 1 ) < 0 )   WRITE_ERR_return;
}

/* Mirror image */
/* Format: m oldobject newobject axis	*/
void
f_mirror( argc, argv )
int	argc;
char	**argv;
{
	register struct directory *proto;
	register struct directory *dp;
	register int i, j, k;
	union record	*rec;
	mat_t mirmat;
	mat_t temp;

	if( (proto = db_lookup( dbip,  argv[1], LOOKUP_NOISY )) == DIR_NULL )
		return;

	if( db_lookup( dbip,  argv[2], LOOKUP_QUIET ) != DIR_NULL )  {
		aexists( argv[2] );
		return;
	}
	k = -1;
	if( strcmp( argv[3], "x" ) == 0 )
		k = 0;
	if( strcmp( argv[3], "y" ) == 0 )
		k = 1;
	if( strcmp( argv[3], "z" ) == 0 )
		k = 2;
	if( k < 0 ) {
		(void)printf("axis must be x, y or z\n");
		return;
	}

	if( (rec = db_getmrec( dbip, proto )) == (union record *)0 )
		READ_ERR_return;
	if( rec[0].u_id == ID_SOLID ||
		rec[0].u_id == ID_ARS_A
	)  {
		if( (dp = db_diradd( dbip,  argv[2], -1, proto->d_len, proto->d_flags )) == DIR_NULL ||
		    db_alloc( dbip, dp, proto->d_len ) < 0 )  {
		    	ALLOC_ERR_return;
		}

		/* create mirror image */
		if( rec[0].u_id == ID_ARS_A )  {
			NAMEMOVE( argv[2], rec[0].a.a_name );
			for( i = 1; i < proto->d_len; i++ )  {
				for( j = k; j < 24; j += 3 )
					rec[i].b.b_values[j] *= -1.0;
			}
		} else  {
			for( i = k; i < 24; i += 3 )
				rec[0].s.s_values[i] *= -1.0;
			NAMEMOVE( argv[2], rec[0].s.s_name );
		}
		if( db_put( dbip, dp, rec, 0, dp->d_len ) < 0 )  WRITE_ERR_return;
		rt_free( (char *)rec, "record" );
	} else if( rec[0].u_id == ID_COMB ) {
		if( (dp = db_diradd( dbip, argv[2], -1, proto->d_len, proto->d_flags ) ) == DIR_NULL ||
		    db_alloc( dbip, dp, proto->d_len ) < 0 )  {
		    	ALLOC_ERR_return;
		}
		NAMEMOVE(argv[2], rec[0].c.c_name);
		mat_idn( mirmat );
		mirmat[k*5] = -1.0;
		for( i=1; i < proto->d_len; i++) {
			mat_t	xmat;

			if(rec[i].u_id != ID_MEMB) {
				(void)printf("f_mirror: bad db record\n");
				return;
			}
			rt_mat_dbmat( xmat, rec[i].M.m_mat );
			mat_mul(temp, mirmat, xmat);
			rt_dbmat_mat( rec[i].M.m_mat, temp );
		}
		if( db_put( dbip, dp, rec, 0, dp->d_len ) < 0 )  WRITE_ERR_return;
		rt_free( (char *)rec, "record" );
	} else {
		(void)printf("%s: Cannot mirror\n",argv[2]);
		return;
	}

	if( no_memory )  {
		(void)printf(
		"Mirror image (%s) created but NO memory left to draw it\n",
			argv[2] );
		return;
	}

	/* draw the "made" solid */
	f_edit( 2, argv+1 );	/* depends on name being in argv[2] ! */
}

/* Modify Combination record information */
/* Format: edcomb combname Regionflag regionid air los GIFTmater */
void
f_edcomb( argc, argv )
int	argc;
char	**argv;
{
	register struct directory *dp;
	union record record;
	int regionid, air, mat, los;

	if( (dp = db_lookup( dbip,  argv[1], LOOKUP_NOISY )) == DIR_NULL )
		return;

	regionid = atoi( argv[3] );
	air = atoi( argv[4] );
	los = atoi( argv[5] );
	mat = atoi( argv[6] );

	if( db_get( dbip,  dp, &record, 0 , 1) < 0 )  READ_ERR_return;
	if( record.u_id != ID_COMB ) {
		(void)printf("%s: not a combination\n", dp->d_namep );
		return;
	}

	if( argv[2][0] == 'R' )
		record.c.c_flags = 'R';
	else
		record.c.c_flags =' ';
	record.c.c_regionid = regionid;
	record.c.c_aircode = air;
	record.c.c_los = los;
	record.c.c_material = mat;
	if( db_put( dbip, dp, &record, 0, 1 ) < 0 )  WRITE_ERR_return;
}

/*
 *			F _ U N I T S
 *
 * Change the local units of the description.
 * Base unit is fixed in mm, so this just changes the current local unit
 * that the user works in.
 */
void
f_units( argc, argv )
int	argc;
char	**argv;
{
	double	loc2mm;
	int	new_unit = 0;
	CONST char	*str;

	if( argc < 2 )  {
		str = rt_units_string(dbip->dbi_local2base);
		(void)printf("You are currently editing in '%s'.  1%s = %gmm \n",
			str, str, dbip->dbi_local2base );
		return;
	}

	if( strcmp(argv[1], "mm") == 0 ) 
		new_unit = ID_MM_UNIT;
	else
	if( strcmp(argv[1], "cm") == 0 ) 
		new_unit = ID_CM_UNIT;
	else
	if( strcmp(argv[1],"m")==0 || strcmp(argv[1],"meters")==0 ) 
		new_unit = ID_M_UNIT;
	else
	if( strcmp(argv[1],"in")==0 || strcmp(argv[1],"inches")==0 ) 
		new_unit = ID_IN_UNIT;
	else
	if( strcmp(argv[1],"ft")==0 || strcmp(argv[1],"feet")==0 ) 
		new_unit = ID_FT_UNIT;

	if( new_unit ) {
		/* One of the recognized db.h units */
		/* change to the new local unit */
		db_conversions( dbip, new_unit );

		if( db_ident( dbip, dbip->dbi_title, new_unit ) < 0 )
			printf("Warning: unable to stash working units into database\n");

	} else if( (loc2mm = rt_units_conversion(argv[1]) ) <= 0 )  {
		(void)printf("%s: unrecognized unit\n", argv[1]);
		(void)printf("valid units: <mm|cm|m|in|ft|meters|inches|feet>\n");
		return;
	} else {
		/*
		 *  Can't stash requested units into the database for next session,
		 *  but there is no problem with the user editing in these units.
		 */
		dbip->dbi_localunit = ID_MM_UNIT;
		dbip->dbi_local2base = loc2mm;
		dbip->dbi_base2local = 1.0 / loc2mm;
		(void)printf("\
Due to a database restriction in the current format of .g files,\n\
this choice of units will not be remembered on your next editing session.\n");
	}
	(void)printf("New editing units = '%s'\n",
		rt_units_string(dbip->dbi_local2base) );
	dmaflag = 1;
}

/*
 *	Change the current title of the description
 */
void
f_title( argc, argv )
int	argc;
char	**argv;
{
	struct rt_vls	title;

	if( argc < 2 )  {
		(void)printf("%s\n", dbip->dbi_title);
		return;
	}

	rt_vls_init( &title );
	rt_vls_from_argv( &title, argc-1, argv+1 );

	if( db_ident( dbip, rt_vls_addr(&title), dbip->dbi_localunit ) < 0 )
		printf("Error: unable to change database title\n");

	rt_vls_free( &title );
	dmaflag = 1;
}

/* tell him it already exists */
void
aexists( name )
char	*name;
{
	(void)printf( "%s:  already exists\n", name );
}

/*
 *  			F _ M A K E
 *  
 *  Create a new solid of a given type
 *  (Generic, or explicit)
 */
void
f_make( argc, argv )
int	argc;
char	**argv;
{
	register struct directory *dp;
	union record record;
	int i;

	if( db_lookup( dbip,  argv[1], LOOKUP_QUIET ) != DIR_NULL )  {
		aexists( argv[1] );
		return;
	}
	/* Position this solid at view center */
	record.s.s_values[0] = -toViewcenter[MDX];
	record.s.s_values[1] = -toViewcenter[MDY];
	record.s.s_values[2] = -toViewcenter[MDZ];
	record.s.s_id = ID_SOLID;

	/* Zero out record.s.s_values[] */
	for( i = 3; i < 24; i++ )  {
		record.s.s_values[i] = 0.0;
	}

	/* make name <arb8|arb7|arb6|arb5|arb4|ellg|ell|sph|tor|tgc|rec|trc|rcc> */
	if( strcmp( argv[2], "arb8" ) == 0 )  {
		record.s.s_type = GENARB8;
		record.s.s_cgtype = ARB8;
		VSET( &record.s.s_values[0*3],
			-toViewcenter[MDX] +Viewscale,
			-toViewcenter[MDY] -Viewscale,
			-toViewcenter[MDZ] -Viewscale );
		VSET( &record.s.s_values[1*3],  0, (Viewscale*2), 0 );
		VSET( &record.s.s_values[2*3],  0, (Viewscale*2), (Viewscale*2) );
		VSET( &record.s.s_values[3*3],  0, 0, (Viewscale*2) );
		VSET( &record.s.s_values[4*3],  -(Viewscale*2), 0, 0 );
		VSET( &record.s.s_values[5*3],  -(Viewscale*2), (Viewscale*2), 0 );
		VSET( &record.s.s_values[6*3],  -(Viewscale*2), (Viewscale*2), (Viewscale*2) );
		VSET( &record.s.s_values[7*3],  -(Viewscale*2), 0, (Viewscale*2)  );
	} else if( strcmp( argv[2], "arb7" ) == 0 )  {
		record.s.s_type = GENARB8;
		record.s.s_cgtype = ARB7;
		VSET( &record.s.s_values[0*3],
			-toViewcenter[MDX] +Viewscale,
			-toViewcenter[MDY] -Viewscale,
			-toViewcenter[MDZ] -(0.5*Viewscale) );
		VSET( &record.s.s_values[1*3],  0, (Viewscale*2), 0 );
		VSET( &record.s.s_values[2*3],  0, (Viewscale*2), (Viewscale*2) );
		VSET( &record.s.s_values[3*3],  0, 0, Viewscale );
		VSET( &record.s.s_values[4*3],  -(Viewscale*2), 0, 0 );
		VSET( &record.s.s_values[5*3],  -(Viewscale*2), (Viewscale*2), 0 );
		VSET( &record.s.s_values[6*3],  -(Viewscale*2), (Viewscale*2), Viewscale );
		VSET( &record.s.s_values[7*3],  -(Viewscale*2), 0, 0  );
	} else if( strcmp( argv[2], "arb6" ) == 0 )  {
		record.s.s_type = GENARB8;
		record.s.s_cgtype = ARB6;
		VSET( &record.s.s_values[0*3],
			-toViewcenter[MDX] +Viewscale,
			-toViewcenter[MDY] -Viewscale,
			-toViewcenter[MDZ] -Viewscale );
		VSET( &record.s.s_values[1*3],  0, (Viewscale*2), 0 );
		VSET( &record.s.s_values[2*3],  0, (Viewscale*2), (Viewscale*2) );
		VSET( &record.s.s_values[3*3],  0, 0, (Viewscale*2) );
		VSET( &record.s.s_values[4*3],  -(Viewscale*2), Viewscale, 0 );
		VSET( &record.s.s_values[5*3],  -(Viewscale*2), Viewscale, 0 );
		VSET( &record.s.s_values[6*3],  -(Viewscale*2), Viewscale, (Viewscale*2) );
		VSET( &record.s.s_values[7*3],  -(Viewscale*2), Viewscale, (Viewscale*2)  );
	} else if( strcmp( argv[2], "arb5" ) == 0 )  {
		record.s.s_type = GENARB8;
		record.s.s_cgtype = ARB5;
		VSET( &record.s.s_values[0*3],
			-toViewcenter[MDX] +Viewscale,
			-toViewcenter[MDY] -Viewscale,
			-toViewcenter[MDZ] -Viewscale );
		VSET( &record.s.s_values[1*3],  0, (Viewscale*2), 0 );
		VSET( &record.s.s_values[2*3],  0, (Viewscale*2), (Viewscale*2) );
		VSET( &record.s.s_values[3*3],  0, 0, (Viewscale*2) );
		VSET( &record.s.s_values[4*3],  -(Viewscale*2), Viewscale, Viewscale );
		VSET( &record.s.s_values[5*3],  -(Viewscale*2), Viewscale, Viewscale );
		VSET( &record.s.s_values[6*3],  -(Viewscale*2), Viewscale, Viewscale );
		VSET( &record.s.s_values[7*3],  -(Viewscale*2), Viewscale, Viewscale  );
	} else if( strcmp( argv[2], "arb4" ) == 0 )  {
		record.s.s_type = GENARB8;
		record.s.s_cgtype = ARB4;
		VSET( &record.s.s_values[0*3],
			-toViewcenter[MDX] +Viewscale,
			-toViewcenter[MDY] -Viewscale,
			-toViewcenter[MDZ] -Viewscale );
		VSET( &record.s.s_values[1*3],  0, (Viewscale*2), 0 );
		VSET( &record.s.s_values[2*3],  0, (Viewscale*2), (Viewscale*2) );
		VSET( &record.s.s_values[3*3],  0, (Viewscale*2), (Viewscale*2) );
		VSET( &record.s.s_values[4*3],  -(Viewscale*2), (Viewscale*2), 0 );
		VSET( &record.s.s_values[5*3],  -(Viewscale*2), (Viewscale*2), 0 );
		VSET( &record.s.s_values[6*3],  -(Viewscale*2), (Viewscale*2), 0 );
		VSET( &record.s.s_values[7*3],  -(Viewscale*2), (Viewscale*2), 0  );
	} else if( strcmp( argv[2], "sph" ) == 0 )  {
		record.s.s_type = GENELL;
		record.s.s_cgtype = SPH;
		VSET( &record.s.s_values[1*3], (0.5*Viewscale), 0, 0 );	/* A */
		VSET( &record.s.s_values[2*3], 0, (0.5*Viewscale), 0 );	/* B */
		VSET( &record.s.s_values[3*3], 0, 0, (0.5*Viewscale) );	/* C */
	} else if( strcmp( argv[2], "ell" ) == 0 )  {
		record.s.s_type = GENELL;
		record.s.s_cgtype = ELL;
		VSET( &record.s.s_values[1*3], (0.5*Viewscale), 0, 0 );	/* A */
		VSET( &record.s.s_values[2*3], 0, (0.25*Viewscale), 0 );	/* B */
		VSET( &record.s.s_values[3*3], 0, 0, (0.25*Viewscale) );	/* C */
	} else if( strcmp( argv[2], "ellg" ) == 0 )  {
		record.s.s_type = GENELL;
		record.s.s_cgtype = ELL;
		VSET( &record.s.s_values[1*3], Viewscale, 0, 0 );	/* A */
		VSET( &record.s.s_values[2*3], 0, (0.5*Viewscale), 0 );	/* B */
		VSET( &record.s.s_values[3*3], 0, 0, (0.25*Viewscale) );	/* C */
	} else if( strcmp( argv[2], "tor" ) == 0 )  {
		record.s.s_type = TOR;
		record.s.s_cgtype = TOR;
		VSET( &record.s.s_values[1*3], (0.5*Viewscale), 0, 0 );	/* N with mag = r2 */
		VSET( &record.s.s_values[2*3], 0, Viewscale, 0 );	/* A == r1 */
		VSET( &record.s.s_values[3*3], 0, 0, Viewscale );	/* B == r1 */
		VSET( &record.s.s_values[4*3], 0, (0.5*Viewscale), 0 );	/* A == r1-r2 */
		VSET( &record.s.s_values[5*3], 0, 0, (0.5*Viewscale) );	/* B == r1-r2 */
		VSET( &record.s.s_values[6*3], 0, (1.5*Viewscale), 0 );	/* A == r1+r2 */
		VSET( &record.s.s_values[7*3], 0, 0, (1.5*Viewscale) );	/* B == r1+r2 */
	} else if( strcmp( argv[2], "tgc" ) == 0 )  {
		record.s.s_type = GENTGC;
		record.s.s_cgtype = TGC;
		VSET( &record.s.s_values[1*3],  0, 0, (Viewscale*2) );
		VSET( &record.s.s_values[2*3],  (0.5*Viewscale), 0, 0 );
		VSET( &record.s.s_values[3*3],  0, (0.25*Viewscale), 0 );
		VSET( &record.s.s_values[4*3],  (0.25*Viewscale), 0, 0 );
		VSET( &record.s.s_values[5*3],  0, (0.5*Viewscale), 0 );
	} else if( strcmp( argv[2], "tec" ) == 0 )  {
		record.s.s_type = GENTGC;
		record.s.s_cgtype = TEC;
		VSET( &record.s.s_values[1*3],  0, 0, (Viewscale*2) );
		VSET( &record.s.s_values[2*3],  (0.5*Viewscale), 0, 0 );
		VSET( &record.s.s_values[3*3],  0, (0.25*Viewscale), 0 );
		VSET( &record.s.s_values[4*3],  (0.25*Viewscale), 0, 0 );
		VSET( &record.s.s_values[5*3],  0, 31.75, 0 );
	} else if( strcmp( argv[2], "rec" ) == 0 )  {
		record.s.s_type = GENTGC;
		record.s.s_cgtype = REC;
		VSET( &record.s.s_values[1*3],  0, 0, (Viewscale*2) );
		VSET( &record.s.s_values[2*3],  (0.5*Viewscale), 0, 0 );
		VSET( &record.s.s_values[3*3],  0, (0.25*Viewscale), 0 );
		VSET( &record.s.s_values[4*3],  (0.5*Viewscale), 0, 0 );
		VSET( &record.s.s_values[5*3],  0, (0.25*Viewscale), 0 );
	} else if( strcmp( argv[2], "trc" ) == 0 )  {
		record.s.s_type = GENTGC;
		record.s.s_cgtype = TRC;
		VSET( &record.s.s_values[1*3],  0, 0, (Viewscale*2) );
		VSET( &record.s.s_values[2*3],  (0.5*Viewscale), 0, 0 );
		VSET( &record.s.s_values[3*3],  0, (0.5*Viewscale), 0 );
		VSET( &record.s.s_values[4*3],  (0.25*Viewscale), 0, 0 );
		VSET( &record.s.s_values[5*3],  0, (0.25*Viewscale), 0 );
	} else if( strcmp( argv[2], "rcc" ) == 0 )  {
		record.s.s_type = GENTGC;
		record.s.s_cgtype = RCC;
		VSET( &record.s.s_values[1*3],  0, 0, (Viewscale*2) );
		VSET( &record.s.s_values[2*3],  (0.5*Viewscale), 0, 0 );
		VSET( &record.s.s_values[3*3],  0, (0.5*Viewscale), 0 );
		VSET( &record.s.s_values[4*3],  (0.5*Viewscale), 0, 0 );
		VSET( &record.s.s_values[5*3],  0, (0.5*Viewscale), 0 );
	} else if( strcmp( argv[2], "ars" ) == 0 )  {
		(void)printf("make ars not implimented yet\n");
		return;
	} else {
		(void)printf("make:  %s is not a known primitive\n", argv[2]);
		return;
	}

	/* Add to in-core directory */
	if( (dp = db_diradd( dbip,  argv[1], -1, 0, DIR_SOLID )) == DIR_NULL ||
	    db_alloc( dbip, dp, 1 ) < 0 )  {
	    	ALLOC_ERR_return;
	}

	NAMEMOVE( argv[1], record.s.s_name );
	if( db_put( dbip, dp, &record, 0, 1 ) < 0 )  WRITE_ERR_return;

	/* draw the "made" solid */
	f_edit( 2, argv );	/* depends on name being in argv[1] */
}

/* allow precise changes to object rotation */
void
f_rot_obj( argc, argv )
int	argc;
char	**argv;
{
	mat_t temp;
	vect_t s_point, point, v_work, model_pt;

	if( not_state( ST_O_EDIT, "Object Rotation" ) )
		return;

	if(movedir != ROTARROW) {
		/* NOT in object rotate mode - put it in obj rot */
		dmp->dmr_light( LIGHT_ON, BE_O_ROTATE );
		dmp->dmr_light( LIGHT_OFF, BE_O_SCALE );
		dmp->dmr_light( LIGHT_OFF, BE_O_XY );
		movedir = ROTARROW;
	}

	/* find point for rotation to take place wrt */
	MAT4X3PNT(model_pt, es_mat, es_keypoint);
	MAT4X3PNT(point, modelchanges, model_pt);

	/* Find absolute translation vector to go from "model_pt" to
	 * 	"point" without any of the rotations in "modelchanges"
	 */
	VSCALE(s_point, point, modelchanges[15]);
	VSUB2(v_work, s_point, model_pt);

	/* REDO "modelchanges" such that:
	 *	1. NO rotations (identity)
	 *	2. trans == v_work
	 *	3. same scale factor
	 */
	mat_idn(temp);
	MAT_DELTAS(temp, v_work[X], v_work[Y], v_work[Z]);
	temp[15] = modelchanges[15];
	mat_copy(modelchanges, temp);

	/* build new rotation matrix */
	mat_idn(temp);
	buildHrot(temp, atof(argv[1])*degtorad,
			atof(argv[2])*degtorad,
			atof(argv[3])*degtorad );

	/* Record the new rotation matrix into the revised
	 *	modelchanges matrix wrt "point"
	 */
	wrt_point(modelchanges, temp, modelchanges, point);

	new_mats();
	dmaflag = 1;
}

/* allow precise changes to object scaling, both local & global */
void
f_sc_obj( argc, argv )
int	argc;
char	**argv;
{
	mat_t incr;
	vect_t point, temp;

	if( not_state( ST_O_EDIT, "Object Scaling" ) )
		return;

	if( atof(argv[1]) <= 0.0 ) {
		(void)printf("ERROR: scale factor <=  0\n");
		return;
	}

	if(movedir != SARROW) {
		/* Put in global object scale mode */
		dmp->dmr_light( LIGHT_OFF, BE_O_ROTATE );
		dmp->dmr_light( LIGHT_OFF, BE_O_XY );
		if( edobj == 0 )
			edobj = BE_O_SCALE;	/* default is global scaling */
		dmp->dmr_light( LIGHT_ON, edobj );
		movedir = SARROW;
	}

	mat_idn(incr);

	/* switch depending on type of scaling to do */
	switch( edobj ) {

		case BE_O_SCALE:
			/* global scaling */
			incr[15] = 1.0 / (atof(argv[1]) * modelchanges[15]);
		break;

		case BE_O_XSCALE:
			/* local scaling ... X-axis */
			incr[0] = atof(argv[1]) / acc_sc[0];
			acc_sc[0] = atof(argv[1]);
		break;

		case BE_O_YSCALE:
			/* local scaling ... Y-axis */
			incr[5] = atof(argv[1]) / acc_sc[1];
			acc_sc[1] = atof(argv[1]);
		break;

		case BE_O_ZSCALE:
			/* local scaling ... Z-axis */
			incr[10] = atof(argv[1]) / acc_sc[2];
			acc_sc[2] = atof(argv[1]);
		break;

	}

	/* find point the scaling is to take place wrt */
	MAT4X3PNT(temp, es_mat, es_keypoint);
	MAT4X3PNT(point, modelchanges, temp);

	wrt_point(modelchanges, incr, modelchanges, point);
	new_mats();
}

/* allow precise changes to object translation */
void
f_tr_obj( argc, argv )
int	argc;
char	**argv;
{
	register int i;
	mat_t incr, old;
	vect_t model_sol_pt, model_incr, ed_sol_pt, new_vertex;

	if( not_state( ST_O_EDIT, "Object Translation") )
		return;

	mat_idn(incr);
	mat_idn(old);

	if( (movedir & (RARROW|UARROW)) == 0 ) {
		/* put in object trans mode */
		dmp->dmr_light( LIGHT_ON, BE_O_XY );
		dmp->dmr_light( LIGHT_OFF, BE_O_SCALE );
		dmp->dmr_light( LIGHT_OFF, BE_O_ROTATE );
		movedir = UARROW | RARROW;
	}

	for(i=0; i<3; i++) {
		new_vertex[i] = atof(argv[i+1]) * local2base;
	}
	MAT4X3PNT(model_sol_pt, es_mat, es_keypoint);
	MAT4X3PNT(ed_sol_pt, modelchanges, model_sol_pt);
	VSUB2(model_incr, new_vertex, ed_sol_pt);
	MAT_DELTAS(incr, model_incr[0], model_incr[1], model_incr[2]);
	mat_copy(old,modelchanges);
	mat_mul(modelchanges, incr, old);
	new_mats();
	return;
}

/* Change the default region ident codes: item air los mat
 */
void
f_regdef( argc, argv )
int	argc;
char	**argv;
{

	dmaflag = 1;
	item_default = atoi(argv[1]);

	if(argc == 2)
		return;

	air_default = atoi(argv[2]);
	if(air_default) 
		item_default = 0;

	if(argc == 3)
		return;

	los_default = atoi(argv[3]);

	if(argc == 4)
		return;

	mat_default = atoi(argv[4]);
}

static int frac_stat;
void
mged_add_nmg_part(newname, m, old_dp)
char *newname;
struct model *m;
struct directory *old_dp;
{
	struct rt_db_internal	new_intern;
	struct directory *new_dp;

	if( db_lookup( dbip,  newname, LOOKUP_QUIET ) != DIR_NULL )  {
		aexists( newname );
		/* Free memory here */
		nmg_km(m);
		frac_stat = 1;
		return;
	}

	if( (new_dp=db_diradd( dbip, newname, -1, 0, DIR_SOLID)) == DIR_NULL )  {
	    	ALLOC_ERR_return;
	}

	/* Export NMG as a new solid */
	RT_INIT_DB_INTERNAL(&new_intern);
	new_intern.idb_type = ID_NMG;
	new_intern.idb_ptr = (genptr_t)m;

	if( rt_db_put_internal( new_dp, dbip, &new_intern ) < 0 )  {
		/* Free memory */
		nmg_km(m);
		printf("rt_db_put_internal() failure\n");
		frac_stat = 1;
		return;
	}
	/* Internal representation has been freed by rt_db_put_internal */
	new_intern.idb_ptr = (genptr_t)NULL;
	frac_stat = 0;
}
/*
 *			F _ F R A C T U R E
 *
 * Usage: fracture nmgsolid [prefix]
 *
 *	given an NMG solid, break it up into several NMG solids, each
 *	containing a single shell with a single sub-element.
 */
void
f_fracture( argc, argv )
int	argc;
char	**argv;
{
	register int i;
	struct directory *old_dp;
	struct rt_db_internal	old_intern;
	struct rt_db_internal	new_intern;
	struct model	*m, *new_model;
	char		newname[32];
	char		prefix[32];
	int	maxdigits;
	struct nmgregion *r, *new_r;
	struct shell *s, *new_s;
	struct faceuse *fu;
	struct loopuse *lu;
	struct edgeuse *eu;
	struct vertex *v_new, *v;
	unsigned long tw, tf, tp;
	double log10();

	rt_log("fracture:");
	for (i=0 ; i < argc ; i++)
		rt_log(" %s", argv[i]);
	rt_log("\n");

	if( (old_dp = db_lookup( dbip,  argv[1], LOOKUP_NOISY )) == DIR_NULL )
		return;

	if( rt_db_get_internal( &old_intern, old_dp, dbip, rt_identity ) < 0 )  {
		(void)printf("rt_db_get_internal() error\n");
		return;
	}

	m = (struct model *)old_intern.idb_ptr;
	NMG_CK_MODEL(m);


	/* how many characters of the solid names do we reserve for digits? */
	nmg_count_shell_kids(m, &tf, &tw, &tp);
	
	maxdigits = (int)(log10((double)(tf+tw+tp)) + 1.0);

	rt_log("%d = %d digits\n", tf+tw+tp, maxdigits);

	/*	for(maxdigits=1,i=tf+tw+tp ; i > 0 ; i /= 10)
	 *	maxdigits++;
	 */

	/* get the prefix for the solids to be created. */
	bzero(prefix, sizeof(prefix));
	strncpy(prefix, argv[argc-1], sizeof(prefix)-2-maxdigits);
	strcat(prefix, "_");

	/* Bust it up here */

	i = 1;
	for (RT_LIST_FOR(r, nmgregion, &m->r_hd)) {
		NMG_CK_REGION(r);
		for (RT_LIST_FOR(s, shell, &r->s_hd)) {
			NMG_CK_SHELL(s);
			if (s->vu_p) {
				NMG_CK_VERTEXUSE(s->vu_p);
				NMG_CK_VERTEX(s->vu_p->v_p);
				v = s->vu_p->v_p;

	nmg_start_dup(m);
				new_model = nmg_mm();
				new_r = nmg_mrsv(new_model);
				new_s = RT_LIST_FIRST(shell, &r->s_hd);
				v_new = new_s->vu_p->v_p;
				if (v->vg_p) {
					nmg_vertex_gv(v_new, v->vg_p->coord);
				}
	nmg_end_dup();

				sprintf(newname, "%s%0*d", prefix, maxdigits, i++);

				mged_add_nmg_part(newname, new_model,
							dbip, old_dp);
				if (frac_stat) return;
				continue;
			}
			for (RT_LIST_FOR(fu, faceuse, &s->fu_hd)) {
				if (fu->orientation != OT_SAME)
					continue;

				NMG_CK_FACEUSE(fu);

				new_model = nmg_mm();
				NMG_CK_MODEL(new_model);
				new_r = nmg_mrsv(new_model);
				NMG_CK_REGION(new_r);
				new_s = RT_LIST_FIRST(shell, &new_r->s_hd);
				NMG_CK_SHELL(new_s);
	nmg_start_dup(m);
				NMG_CK_SHELL(new_s);
				nmg_dup_face(fu, new_s);
	nmg_end_dup();

				sprintf(newname, "%s%0*d", prefix, maxdigits, i++);
				mged_add_nmg_part(newname, new_model,
							dbip, old_dp);
				if (frac_stat) return;
			}
#if 0
			while (RT_LIST_NON_EMPTY(&s->lu_hd)) {
				lu = RT_LIST_FIRST(loopuse, &s->lu_hd);
				new_model = nmg_mm();
				r = nmg_mrsv(new_model);
				new_s = RT_LIST_FIRST(shell, &r->s_hd);

				nmg_dup_loop(lu, new_s);
				nmg_klu(lu);

				sprintf(newname, "%s%0*d", prefix, maxdigits, i++);
				mged_add_nmg_part(newname, new_model,
							dbip, old_dp);
				if (frac_stat) return;
			}
			while (RT_LIST_NON_EMPTY(&s->eu_hd)) {
				eu = RT_LIST_FIRST(edgeuse, &s->eu_hd);
				new_model = nmg_mm();
				r = nmg_mrsv(new_model);
				new_s = RT_LIST_FIRST(shell, &r->s_hd);

				nmg_dup_edge(eu, new_s);
				nmg_keu(eu);

				sprintf(newname, "%s%0*d", prefix, maxdigits, i++);

				mged_add_nmg_part(newname, new_model,
							dbip, old_dp);
				if (frac_stat) return;
			}
#endif
		}
	}

}

