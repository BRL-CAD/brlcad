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

#include "conf.h"

#include <math.h>
#include <signal.h>
#include <stdio.h>
#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "machine.h"
#include "vmath.h"
#include "externs.h"
#include "db.h"
#include "rtstring.h"
#include "nmg.h"
#include "raytrace.h"
#include "rtgeom.h"
#include "wdb.h"
#include "./ged.h"
#include "./solid.h"
#include "./dm.h"
#include "./sedit.h"

#ifdef XMGED
extern void (*tran_hook)();
extern void (*rot_hook)();
extern int irot_set;
extern double irot_x;
extern double irot_y;
extern double irot_z;
extern int tran_set;
extern double tran_x;
extern double tran_y;
extern double tran_z;
extern int      update_views;

void set_tran();
#endif

void	aexists();

int		newedge;		/* new edge for arb editing */

/* Add/modify item and air codes of a region */
/* Format: I region item <air>	*/
int
f_itemair( argc, argv )
int	argc;
char	**argv;
{
	register struct directory *dp;
	int ident, air;
	union record record;

	if( (dp = db_lookup( dbip,  argv[1], LOOKUP_NOISY )) == DIR_NULL )
		return CMD_BAD;

	air = ident = 0;
	ident = atoi( argv[2] );

	/* If <air> is not included, it is assumed to be zero */
	if( argc == 4 )  {
		air = atoi( argv[3] );
	}
	if( db_get( dbip,  dp, &record, 0 , 1) < 0 ) {
		READ_ERR;
		return CMD_BAD;
	}
	if( record.u_id != ID_COMB ) {
		rt_log("%s: not a combination\n", dp->d_namep );
		return CMD_BAD;
	}
	if( record.c.c_flags != 'R' ) {
		rt_log("%s: not a region\n", dp->d_namep );
		return CMD_BAD;
	}
	record.c.c_regionid = ident;
	record.c.c_aircode = air;
	if( db_put( dbip, dp, &record, 0, 1 ) < 0 ) {
		WRITE_ERR;
		return CMD_BAD;
	}

	return CMD_OK;
}

/* Modify material information */
/* Usage:  mater name */
int
f_mater( argc, argv )
int	argc;
char	**argv;
{
	register struct directory *dp;
	char line[80];
	union record record;
	int r=0, g=0, b=0;
	char	*nlp;
#ifdef XMGED
	int	num_params = 8;	/* number of parameters */
#endif

	if( (dp = db_lookup( dbip,  argv[1], LOOKUP_NOISY )) == DIR_NULL )
		return CMD_BAD;

	if( db_get( dbip,  dp, &record, 0 , 1) < 0 ) {
		READ_ERR;
		return CMD_BAD;
	}

	if( record.u_id != ID_COMB )  {
		rt_log("%s: not a combination\n", dp->d_namep );
		return CMD_BAD;
	}

#ifdef XMGED
	if( argc >= 3 )  {
		if( strncmp( argv[2], "del", 3 ) != 0 )  {
		/* Material */
			strcpy( record.c.c_matname, argv[2]);
		}else{
			(void)rt_log( "Was %s %s\n", record.c.c_matname,
						record.c.c_matparm);
			record.c.c_matname[0] = '\0';
			record.c.c_override = 0;
			goto out;
		}
	}else{
		/* Material */
		(void)rt_log( "Material = %s\nMaterial?  ('del' to delete, CR to skip) ", record.c.c_matname);
		return CMD_MORE;
	}

	if(argc >= 4){
		if( strncmp(argv[3],  "del", 3) == 0  )
			record.c.c_matparm[0] = '\0';
		else
			strcpy( record.c.c_matparm, argv[3]);
	}else{
		/* Parameters */
		(void)rt_log( "Param = %s\nParameter string? ('del' to delete, CR to skip) ", record.c.c_matparm);
		return CMD_MORE;
	}

	if(argc >= 5){
		if( strncmp(argv[4], "del", 3) == 0 ){
			/* leave color as is */
			record.c.c_override = 0;
			num_params = 6;
		}else if(argc < 7){	/* prompt for color */
			goto color_prompt;
		}else{	/* change color */
			sscanf(argv[4], "%d", &r);
			sscanf(argv[5], "%d", &g);
			sscanf(argv[6], "%d", &b);
			record.c.c_rgb[0] = r;
			record.c.c_rgb[1] = g;
			record.c.c_rgb[2] = b;
			record.c.c_override = 1;
		}
	}else{
	/* Color */
color_prompt:
	if( record.c.c_override )
		(void)rt_log( "Color = %d %d %d\n", 
			record.c.c_rgb[0],
			record.c.c_rgb[1],
			record.c.c_rgb[2] );
	else
		(void)rt_log( "Color = (No color specified)\n");
	(void)rt_log( "Color R G B (0..255)? ('del' to delete, CR to skip) ");
	return CMD_MORE;
	}

	/* Inherit */
	switch( record.c.c_inherit )  {
	default:
		/* This is necessary to clean up old databases with grunge here */
		record.c.c_inherit = DB_INH_LOWER;
		/* Fall through */
	case DB_INH_LOWER:
		(void)rt_log( "Inherit = 0:  lower nodes (towards leaves) override\n");
		break;
	case DB_INH_HIGHER:
		(void)rt_log( "Inherit = 1:  higher nodes (towards root) override\n");
		break;
	}

	if(argc >= num_params){
		sscanf(argv[num_params - 1], "%c", &record.c.c_inherit);
		line[0] = *argv[num_params - 1];
	}else{
		(void)rt_log( "Inheritance (0|1)? (CR to skip) ");
		return CMD_MORE;
	}
#else
	if( argc >= 3 )  {
		if( strncmp( argv[2], "del", 3 ) != 0 )  {
			rt_log("Use 'mater name del' to delete\n");
			return CMD_BAD;
		}
		rt_log("Was %s %s\n", record.c.c_matname, record.c.c_matparm);
		record.c.c_matname[0] = '\0';
		record.c.c_override = 0;
		goto out;
	}

	/* Material */
	rt_log("Material = %s\nMaterial?  ('del' to delete, CR to skip) ", record.c.c_matname);
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
	rt_log("Param = %s\nParameter string? ('del' to delete, CR to skip) ", record.c.c_matparm);
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
		rt_log("Color = %d %d %d\n", 
			record.c.c_rgb[0],
			record.c.c_rgb[1],
			record.c.c_rgb[2] );
	else
		rt_log("Color = (No color specified)\n");
	rt_log("Color R G B (0..255)? ('del' to delete, CR to skip) ");
	/* XXX This is bad!!!  Should use CMD_MORE return*/
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
		rt_log(" (color unchanged)\n");
	}

	/* Inherit */
	switch( record.c.c_inherit )  {
	default:
		/* This is necessary to clean up old databases with grunge here */
		record.c.c_inherit = DB_INH_LOWER;
		/* Fall through */
	case DB_INH_LOWER:
		rt_log("Inherit = 0:  lower nodes (towards leaves) override\n");
		break;
	case DB_INH_HIGHER:
		rt_log("Inherit = 1:  higher nodes (towards root) override\n");
		break;
	}
	rt_log("Inheritance (0|1)? (CR to skip) ");
	(void)fgets(line,sizeof(line),stdin);
#endif
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
		rt_log("Unknown response ignored\n");
		break;
	}		
out:
	if( db_put( dbip, dp, &record, 0, 1 ) < 0 ) {
		WRITE_ERR;
		return CMD_BAD;
	}

	return CMD_OK;
}

/*
 *			F _ C O M B _ C O L O R
 *
 *  Simple command-line way to set object color
 *  Usage: ocolor combination R G B
 */
int
f_comb_color( argc, argv )

int	argc;
char	**argv;

{
    int				i;
    int				val;
    register struct directory	*dp;
    union record		record;

    if ((dp = db_lookup(dbip,  argv[1], LOOKUP_NOISY)) == DIR_NULL)
	return CMD_BAD;

    if (db_get(dbip,  dp, &record, 0 , 1) < 0)
    {
	READ_ERR;
	return CMD_BAD;
    }

    if (record.u_id != ID_COMB)
    {
	rt_log("%s: not a combination\n", dp->d_namep);
	return CMD_BAD;
    }

    for (i = 0; i < 3; ++i)
	if (((val = atoi(argv[i + 2])) < 0) || (val > 255))
	{
	    rt_log("RGB value out of range: %d\n", val);
	    return CMD_BAD;
	}
	else
	    record.c.c_rgb[i] = val;
    record.c.c_override = 1;

    if (db_put( dbip, dp, &record, 0, 1) < 0)
    {
	WRITE_ERR;
	return CMD_BAD;
    }

    return CMD_OK;
}

/*
 *			F _ S H A D E R
 *
 *  Simpler, command-line version of 'mater' command.
 *  Usage: shader combination shader_material [shader_argument(s)]
 */
int
f_shader( argc, argv )
int	argc;
char	**argv;
{
	register struct directory *dp;
	union record		record;
	struct rt_vls		args;

	if( (dp = db_lookup( dbip,  argv[1], LOOKUP_NOISY )) == DIR_NULL )
		return CMD_BAD;

	if( db_get( dbip,  dp, &record, 0 , 1) < 0 ) {
		READ_ERR;
		return CMD_BAD;
	}

	if( record.u_id != ID_COMB )  {
		rt_log("%s: not a combination\n", dp->d_namep );
		return CMD_BAD;
	}

	strncpy( record.c.c_matname, argv[2], sizeof(record.c.c_matname)-1 );
	record.c.c_matname[sizeof(record.c.c_matname)-1] = '\0';

	/* Bunch up the rest of the args, space separated */
	rt_vls_init( &args );
	rt_vls_from_argv( &args, argc-3, argv+3 );
	rt_vls_trunc( &args, sizeof(record.c.c_matparm)-1 );
	strcpy( record.c.c_matparm, rt_vls_addr( &args ) );
	rt_vls_free( &args );

	if( db_put( dbip, dp, &record, 0, 1 ) < 0 ) {
		WRITE_ERR;
		return CMD_BAD;
	}

	return CMD_OK;
}


/* Mirror image */
/* Format: m oldobject newobject axis	*/
int
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
		return CMD_BAD;

	if( db_lookup( dbip,  argv[2], LOOKUP_QUIET ) != DIR_NULL )  {
		aexists( argv[2] );
		return CMD_BAD;
	}
	k = -1;
	if( strcmp( argv[3], "x" ) == 0 )
		k = 0;
	if( strcmp( argv[3], "y" ) == 0 )
		k = 1;
	if( strcmp( argv[3], "z" ) == 0 )
		k = 2;
	if( k < 0 ) {
		rt_log("axis must be x, y or z\n");
		return CMD_BAD;
	}

	if( (rec = db_getmrec( dbip, proto )) == (union record *)0 ) {
		READ_ERR;
		return CMD_BAD;
	}
	if( rec[0].u_id == ID_SOLID ||
		rec[0].u_id == ID_ARS_A
	)  {
		if( (dp = db_diradd( dbip,  argv[2], -1, proto->d_len, proto->d_flags )) == DIR_NULL ||
		    db_alloc( dbip, dp, proto->d_len ) < 0 )  {
		    	ALLOC_ERR;
			return CMD_BAD;
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
		if( db_put( dbip, dp, rec, 0, dp->d_len ) < 0 ) {
			WRITE_ERR;
			return CMD_BAD;
		}
		rt_free( (char *)rec, "record" );
	} else if( rec[0].u_id == ID_COMB ) {
		if( (dp = db_diradd( dbip, argv[2], -1, proto->d_len, proto->d_flags ) ) == DIR_NULL ||
		    db_alloc( dbip, dp, proto->d_len ) < 0 )  {
		    	ALLOC_ERR;
			return CMD_BAD;
		}
		NAMEMOVE(argv[2], rec[0].c.c_name);
		mat_idn( mirmat );
		mirmat[k*5] = -1.0;
		for( i=1; i < proto->d_len; i++) {
			mat_t	xmat;

			if(rec[i].u_id != ID_MEMB) {
				rt_log("f_mirror: bad db record\n");
				return CMD_BAD;
			}
			rt_mat_dbmat( xmat, rec[i].M.m_mat );
			mat_mul(temp, mirmat, xmat);
			rt_dbmat_mat( rec[i].M.m_mat, temp );
		}
		if( db_put( dbip, dp, rec, 0, dp->d_len ) < 0 ) {
			WRITE_ERR;
			return CMD_BAD;
		}
		rt_free( (char *)rec, "record" );
	} else {
		rt_log("%s: Cannot mirror\n",argv[2]);
		return CMD_BAD;
	}

	if( no_memory )  {
		rt_log(
		"Mirror image (%s) created but NO memory left to draw it\n",
			argv[2] );
		return CMD_BAD;
	}

	/* draw the "made" solid */
	return f_edit( 2, argv+1 ); /* depends on name being in argv[2] ! */
}

/* Modify Combination record information */
/* Format: edcomb combname Regionflag regionid air los GIFTmater */
int
f_edcomb( argc, argv )
int	argc;
char	**argv;
{
	register struct directory *dp;
	union record record;
	int regionid, air, mat, los;

	if( (dp = db_lookup( dbip,  argv[1], LOOKUP_NOISY )) == DIR_NULL )
		return CMD_BAD;

	regionid = atoi( argv[3] );
	air = atoi( argv[4] );
	los = atoi( argv[5] );
	mat = atoi( argv[6] );

	if( db_get( dbip,  dp, &record, 0 , 1) < 0 ) {
		READ_ERR;
		return CMD_BAD;
	}
	if( record.u_id != ID_COMB ) {
		rt_log("%s: not a combination\n", dp->d_namep );
		return CMD_BAD;
	}

	if( argv[2][0] == 'R' )
		record.c.c_flags = 'R';
	else
		record.c.c_flags =' ';
	record.c.c_regionid = regionid;
	record.c.c_aircode = air;
	record.c.c_los = los;
	record.c.c_material = mat;
	if( db_put( dbip, dp, &record, 0, 1 ) < 0 ) {
		WRITE_ERR;
		return CMD_BAD;
	}

	return CMD_OK;
}

/*
 *			F _ U N I T S
 *
 * Change the local units of the description.
 * Base unit is fixed in mm, so this just changes the current local unit
 * that the user works in.
 */
int
f_units( argc, argv )
int	argc;
char	**argv;
{
	double	loc2mm;
	int	new_unit = 0;
	CONST char	*str;

	if( argc < 2 )  {
		str = rt_units_string(dbip->dbi_local2base);
		rt_log("You are currently editing in '%s'.  1%s = %gmm \n",
			str, str, dbip->dbi_local2base );
		return CMD_OK;
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
			rt_log("Warning: unable to stash working units into database\n");

	} else if( (loc2mm = rt_units_conversion(argv[1]) ) <= 0 )  {
		rt_log("%s: unrecognized unit\n", argv[1]);
		rt_log("valid units: <mm|cm|m|in|ft|meters|inches|feet>\n");
		return CMD_BAD;
	} else {
		/*
		 *  Can't stash requested units into the database for next session,
		 *  but there is no problem with the user editing in these units.
		 */
		dbip->dbi_localunit = ID_MM_UNIT;
		dbip->dbi_local2base = loc2mm;
		dbip->dbi_base2local = 1.0 / loc2mm;
		rt_log("\
Due to a database restriction in the current format of .g files,\n\
this choice of units will not be remembered on your next editing session.\n");
	}
	rt_log("New editing units = '%s'\n",
		rt_units_string(dbip->dbi_local2base) );
	dmaflag = 1;

	return CMD_OK;
}

/*
 *	Change the current title of the description
 */
int
f_title( argc, argv )
int	argc;
char	**argv;
{
	struct rt_vls	title;
	int bad = 0;

	if( argc < 2 )  {
		rt_log("%s\n", dbip->dbi_title);
		return CMD_OK;
	}

	rt_vls_init( &title );
	rt_vls_from_argv( &title, argc-1, argv+1 );

	if( db_ident( dbip, rt_vls_addr(&title), dbip->dbi_localunit ) < 0 ) {
		rt_log("Error: unable to change database title\n");
		bad = 1;
	}

	rt_vls_free( &title );
	dmaflag = 1;

	return bad ? CMD_BAD : CMD_OK;
}

/* tell him it already exists */
void
aexists( name )
char	*name;
{
	rt_log( "%s:  already exists\n", name );
}

/*
 *  			F _ M A K E
 *  
 *  Create a new solid of a given type
 *  (Generic, or explicit)
 */
int
f_make( argc, argv )
int	argc;
char	**argv;
{
	register struct directory *dp;
	int ngran;
	int i;
	struct rt_db_internal	internal;
	struct rt_external	external;
	struct rt_arb_internal	*arb_ip;
	struct rt_tgc_internal	*tgc_ip;
	struct rt_ell_internal	*ell_ip;
	struct rt_tor_internal	*tor_ip;
	struct rt_grip_internal	*grp_ip;
	struct rt_half_internal *half_ip;
	struct rt_rpc_internal *rpc_ip;
	struct rt_rhc_internal *rhc_ip;
	struct rt_epa_internal *epa_ip;
	struct rt_ehy_internal *ehy_ip;
	struct rt_eto_internal *eto_ip;
	struct rt_part_internal *part_ip;
	struct rt_pipe_internal *pipe_ip;

	if( db_lookup( dbip,  argv[1], LOOKUP_QUIET ) != DIR_NULL )  {
		aexists( argv[1] );
		return CMD_BAD;
	}

	RT_INIT_DB_INTERNAL( &internal );

	/* make name <arb8 | arb7 | arb6 | arb5 | arb4 | ellg | ell |
	 * sph | tor | tgc | rec | trc | rcc | grp | half | nmg> */
	if( strcmp( argv[2], "arb8" ) == 0 )  {
		internal.idb_type = ID_ARB8;
		internal.idb_ptr = (genptr_t)rt_malloc( sizeof(struct rt_arb_internal) , "rt_arb_internal" );
		arb_ip = (struct rt_arb_internal *)internal.idb_ptr;
		arb_ip->magic = RT_ARB_INTERNAL_MAGIC;
		VSET( arb_ip->pt[0] ,
			-toViewcenter[MDX] +Viewscale,
			-toViewcenter[MDY] -Viewscale,
			-toViewcenter[MDZ] -Viewscale );
		for( i=1 ; i<8 ; i++ )			VMOVE( arb_ip->pt[i] , arb_ip->pt[0] );
		arb_ip->pt[1][Y] += Viewscale*2.0;
		arb_ip->pt[2][Y] += Viewscale*2.0;
		arb_ip->pt[2][Z] += Viewscale*2.0;
		arb_ip->pt[3][Z] += Viewscale*2.0;
		for( i=4 ; i<8 ; i++ )
			arb_ip->pt[i][X] -= Viewscale*2.0;
		arb_ip->pt[5][Y] += Viewscale*2.0;
		arb_ip->pt[6][Y] += Viewscale*2.0;
		arb_ip->pt[6][Z] += Viewscale*2.0;
		arb_ip->pt[7][Z] += Viewscale*2.0;
	} else if( strcmp( argv[2], "arb7" ) == 0 )  {
		internal.idb_type = ID_ARB8;
		internal.idb_ptr = (genptr_t)rt_malloc( sizeof(struct rt_arb_internal) , "rt_arb_internal" );
		arb_ip = (struct rt_arb_internal *)internal.idb_ptr;
		arb_ip->magic = RT_ARB_INTERNAL_MAGIC;
		VSET( arb_ip->pt[0] ,
			-toViewcenter[MDX] +Viewscale,
			-toViewcenter[MDY] -Viewscale,
			-toViewcenter[MDZ] -(0.5*Viewscale) );
		for( i=1 ; i<8 ; i++ )
			VMOVE( arb_ip->pt[i] , arb_ip->pt[0] );
		arb_ip->pt[1][Y] += Viewscale*2.0;
		arb_ip->pt[2][Y] += Viewscale*2.0;
		arb_ip->pt[2][Z] += Viewscale*2.0;
		arb_ip->pt[3][Z] += Viewscale;
		for( i=4 ; i<8 ; i++ )
			arb_ip->pt[i][X] -= Viewscale*2.0;
		arb_ip->pt[5][Y] += Viewscale*2.0;
		arb_ip->pt[6][Y] += Viewscale*2.0;
		arb_ip->pt[6][Z] += Viewscale;
	} else if( strcmp( argv[2], "arb6" ) == 0 )  {
		internal.idb_type = ID_ARB8;
		internal.idb_ptr = (genptr_t)rt_malloc( sizeof(struct rt_arb_internal) , "rt_arb_internal" );
		arb_ip = (struct rt_arb_internal *)internal.idb_ptr;
		arb_ip->magic = RT_ARB_INTERNAL_MAGIC;
		VSET( arb_ip->pt[0],
			-toViewcenter[MDX] +Viewscale,
			-toViewcenter[MDY] -Viewscale,
			-toViewcenter[MDZ] -Viewscale );
		for( i=1 ; i<8 ; i++ )
			VMOVE( arb_ip->pt[i] , arb_ip->pt[0] );
		arb_ip->pt[1][Y] += Viewscale*2.0;
		arb_ip->pt[2][Y] += Viewscale*2.0;
		arb_ip->pt[2][Z] += Viewscale*2.0;
		arb_ip->pt[3][Z] += Viewscale*2.0;
		for( i=4 ; i<8 ; i++ )
			arb_ip->pt[i][X] -= Viewscale*2.0;
		arb_ip->pt[4][Y] += Viewscale;
		arb_ip->pt[5][Y] += Viewscale;
		arb_ip->pt[6][Y] += Viewscale;
		arb_ip->pt[6][Z] += Viewscale*2.0;
		arb_ip->pt[7][Y] += Viewscale;
		arb_ip->pt[7][Z] += Viewscale*2.0;
	} else if( strcmp( argv[2], "arb5" ) == 0 )  {
		internal.idb_type = ID_ARB8;
		internal.idb_ptr = (genptr_t)rt_malloc( sizeof(struct rt_arb_internal) , "rt_arb_internal" );
		arb_ip = (struct rt_arb_internal *)internal.idb_ptr;
		arb_ip->magic = RT_ARB_INTERNAL_MAGIC;
		VSET( arb_ip->pt[0] ,
			-toViewcenter[MDX] +Viewscale,
			-toViewcenter[MDY] -Viewscale,
			-toViewcenter[MDZ] -Viewscale );
		for( i=1 ; i<8 ; i++ )
			VMOVE( arb_ip->pt[i] , arb_ip->pt[0] );
		arb_ip->pt[1][Y] += Viewscale*2.0;
		arb_ip->pt[2][Y] += Viewscale*2.0;
		arb_ip->pt[2][Z] += Viewscale*2.0;
		arb_ip->pt[3][Z] += Viewscale*2.0;
		for( i=4 ; i<8 ; i++ )
		{
			arb_ip->pt[i][X] -= Viewscale*2.0;
			arb_ip->pt[i][Y] += Viewscale;
			arb_ip->pt[i][Z] += Viewscale;
		}
	} else if( strcmp( argv[2], "arb4" ) == 0 )  {
		internal.idb_type = ID_ARB8;
		internal.idb_ptr = (genptr_t)rt_malloc( sizeof(struct rt_arb_internal) , "rt_arb_internal" );
		arb_ip = (struct rt_arb_internal *)internal.idb_ptr;
		arb_ip->magic = RT_ARB_INTERNAL_MAGIC;
		VSET( arb_ip->pt[0] ,
			-toViewcenter[MDX] +Viewscale,
			-toViewcenter[MDY] -Viewscale,
			-toViewcenter[MDZ] -Viewscale );
		for( i=1 ; i<8 ; i++ )
			VMOVE( arb_ip->pt[i] , arb_ip->pt[0] );
		arb_ip->pt[1][Y] += Viewscale*2.0;
		arb_ip->pt[2][Y] += Viewscale*2.0;
		arb_ip->pt[2][Z] += Viewscale*2.0;
		arb_ip->pt[3][Y] += Viewscale*2.0;
		arb_ip->pt[3][Z] += Viewscale*2.0;
		for( i=4 ; i<8 ; i++ )
		{
			arb_ip->pt[i][X] -= Viewscale*2.0;
			arb_ip->pt[i][Y] += Viewscale*2.0;
		}
	} else if( strcmp( argv[2], "sph" ) == 0 )  {
		internal.idb_type = ID_ELL;
		internal.idb_ptr = (genptr_t)rt_malloc( sizeof(struct rt_ell_internal) , "rt_ell_internal" );
		ell_ip = (struct rt_ell_internal *)internal.idb_ptr;
		ell_ip->magic = RT_ELL_INTERNAL_MAGIC;
		VSET( ell_ip->v , -toViewcenter[MDX] , -toViewcenter[MDY] , -toViewcenter[MDZ] );
		VSET( ell_ip->a, (0.5*Viewscale), 0, 0 );	/* A */
		VSET( ell_ip->b, 0, (0.5*Viewscale), 0 );	/* B */
		VSET( ell_ip->c, 0, 0, (0.5*Viewscale) );	/* C */
	} else if(( strcmp( argv[2], "grp" ) == 0 ) ||
		  ( strcmp( argv[2], "grip") == 0 )) {
		internal.idb_type = ID_GRIP;
		internal.idb_ptr = (genptr_t)rt_malloc( sizeof(struct rt_grip_internal), "rt_grp_internal" );
		grp_ip = (struct rt_grip_internal *) internal.idb_ptr;
		grp_ip->magic = RT_GRIP_INTERNAL_MAGIC;
		VSET( grp_ip->center, -toViewcenter[MDX], -toViewcenter[MDY],
		    -toViewcenter[MDZ]);
		VSET( grp_ip->normal, 1.0, 0.0, 0.0);
		grp_ip->mag = Viewscale*0.75;
	} else if( strcmp( argv[2], "ell" ) == 0 )  {
		internal.idb_type = ID_ELL;
		internal.idb_ptr = (genptr_t)rt_malloc( sizeof(struct rt_ell_internal) , "rt_ell_internal" );
		ell_ip = (struct rt_ell_internal *)internal.idb_ptr;
		ell_ip->magic = RT_ELL_INTERNAL_MAGIC;
		VSET( ell_ip->v , -toViewcenter[MDX] , -toViewcenter[MDY] , -toViewcenter[MDZ] );
		VSET( ell_ip->a, (0.5*Viewscale), 0, 0 );	/* A */
		VSET( ell_ip->b, 0, (0.25*Viewscale), 0 );	/* B */
		VSET( ell_ip->c, 0, 0, (0.25*Viewscale) );	/* C */
	} else if( strcmp( argv[2], "ellg" ) == 0 )  {
		internal.idb_type = ID_ELL;
		internal.idb_ptr = (genptr_t)rt_malloc( sizeof(struct rt_ell_internal) , "rt_ell_internal" );
		ell_ip = (struct rt_ell_internal *)internal.idb_ptr;
		ell_ip->magic = RT_ELL_INTERNAL_MAGIC;
		VSET( ell_ip->v , -toViewcenter[MDX] , -toViewcenter[MDY] , -toViewcenter[MDZ] );
		VSET( ell_ip->a, Viewscale, 0, 0 );		/* A */
		VSET( ell_ip->b, 0, (0.5*Viewscale), 0 );	/* B */
		VSET( ell_ip->c, 0, 0, (0.25*Viewscale) );	/* C */
	} else if( strcmp( argv[2], "tor" ) == 0 )  {
		internal.idb_type = ID_TOR;
		internal.idb_ptr = (genptr_t)rt_malloc( sizeof(struct rt_tor_internal) , "rt_tor_internal" );
		tor_ip = (struct rt_tor_internal *)internal.idb_ptr;
		tor_ip->magic = RT_TOR_INTERNAL_MAGIC;
		VSET( tor_ip->v , -toViewcenter[MDX] , -toViewcenter[MDY] , -toViewcenter[MDZ] );
		VSET( tor_ip->h , 1.0 , 0.0 , 0.0 );	/* unit normal */
		tor_ip->r_h = 0.5*Viewscale;
		tor_ip->r_a = Viewscale;
		tor_ip->r_b = Viewscale;
		VSET( tor_ip->a , 0.0 , Viewscale , 0.0 );
		VSET( tor_ip->b , 0.0 , 0.0 , Viewscale );
	} else if( strcmp( argv[2], "tgc" ) == 0 )  {
		internal.idb_type = ID_TGC;
		internal.idb_ptr = (genptr_t)rt_malloc( sizeof(struct rt_tgc_internal) , "rt_tgc_internal" );
		tgc_ip = (struct rt_tgc_internal *)internal.idb_ptr;
		tgc_ip->magic = RT_TGC_INTERNAL_MAGIC;
		VSET( tgc_ip->v , -toViewcenter[MDX] , -toViewcenter[MDY] , -toViewcenter[MDZ]-Viewscale );
		VSET( tgc_ip->h,  0, 0, (Viewscale*2) );
		VSET( tgc_ip->a,  (0.5*Viewscale), 0, 0 );
		VSET( tgc_ip->b,  0, (0.25*Viewscale), 0 );
		VSET( tgc_ip->c,  (0.25*Viewscale), 0, 0 );
		VSET( tgc_ip->d,  0, (0.5*Viewscale), 0 );
	} else if( strcmp( argv[2], "tec" ) == 0 )  {
		internal.idb_type = ID_TGC;
		internal.idb_ptr = (genptr_t)rt_malloc( sizeof(struct rt_tgc_internal) , "rt_tgc_internal" );
		tgc_ip = (struct rt_tgc_internal *)internal.idb_ptr;
		tgc_ip->magic = RT_TGC_INTERNAL_MAGIC;
		VSET( tgc_ip->v , -toViewcenter[MDX] , -toViewcenter[MDY] , -toViewcenter[MDZ]-Viewscale );
		VSET( tgc_ip->h,  0, 0, (Viewscale*2) );
		VSET( tgc_ip->a,  (0.5*Viewscale), 0, 0 );
		VSET( tgc_ip->b,  0, (0.25*Viewscale), 0 );
		VSET( tgc_ip->c,  (0.25*Viewscale), 0, 0 );
		VSET( tgc_ip->d,  0, (0.125*Viewscale), 0 );
	} else if( strcmp( argv[2], "rec" ) == 0 )  {
		internal.idb_type = ID_TGC;
		internal.idb_ptr = (genptr_t)rt_malloc( sizeof(struct rt_tgc_internal) , "rt_tgc_internal" );
		tgc_ip = (struct rt_tgc_internal *)internal.idb_ptr;
		tgc_ip->magic = RT_TGC_INTERNAL_MAGIC;
		VSET( tgc_ip->v , -toViewcenter[MDX] , -toViewcenter[MDY] , -toViewcenter[MDZ]-Viewscale );
		VSET( tgc_ip->h,  0, 0, (Viewscale*2) );
		VSET( tgc_ip->a,  (0.5*Viewscale), 0, 0 );
		VSET( tgc_ip->b,  0, (0.25*Viewscale), 0 );
		VSET( tgc_ip->c,  (0.5*Viewscale), 0, 0 );
		VSET( tgc_ip->d,  0, (0.25*Viewscale), 0 );
	} else if( strcmp( argv[2], "trc" ) == 0 )  {
		internal.idb_type = ID_TGC;
		internal.idb_ptr = (genptr_t)rt_malloc( sizeof(struct rt_tgc_internal) , "rt_tgc_internal" );
		tgc_ip = (struct rt_tgc_internal *)internal.idb_ptr;
		tgc_ip->magic = RT_TGC_INTERNAL_MAGIC;
		VSET( tgc_ip->v , -toViewcenter[MDX] , -toViewcenter[MDY] , -toViewcenter[MDZ]-Viewscale );
		VSET( tgc_ip->h,  0, 0, (Viewscale*2) );
		VSET( tgc_ip->a,  (0.5*Viewscale), 0, 0 );
		VSET( tgc_ip->b,  0, (0.5*Viewscale), 0 );
		VSET( tgc_ip->c,  (0.25*Viewscale), 0, 0 );
		VSET( tgc_ip->d,  0, (0.25*Viewscale), 0 );
	} else if( strcmp( argv[2], "rcc" ) == 0 )  {
		internal.idb_type = ID_TGC;
		internal.idb_ptr = (genptr_t)rt_malloc( sizeof(struct rt_tgc_internal) , "rt_tgc_internal" );
		tgc_ip = (struct rt_tgc_internal *)internal.idb_ptr;
		tgc_ip->magic = RT_TGC_INTERNAL_MAGIC;
		VSET( tgc_ip->v , -toViewcenter[MDX] , -toViewcenter[MDY] , -toViewcenter[MDZ]-Viewscale );
		VSET( tgc_ip->h,  0, 0, (Viewscale*2) );
		VSET( tgc_ip->a,  (0.5*Viewscale), 0, 0 );
		VSET( tgc_ip->b,  0, (0.5*Viewscale), 0 );
		VSET( tgc_ip->c,  (0.5*Viewscale), 0, 0 );
		VSET( tgc_ip->d,  0, (0.5*Viewscale), 0 );
	} else if( strcmp( argv[2], "half" ) == 0 ) {
		internal.idb_type = ID_HALF;
		internal.idb_ptr = (genptr_t)rt_malloc( sizeof(struct rt_half_internal) , "rt_half_internal" );
		half_ip = (struct rt_half_internal *)internal.idb_ptr;
		half_ip->magic = RT_HALF_INTERNAL_MAGIC;
		VSET( half_ip->eqn , 0 , 0 , 1 );
		half_ip->eqn[3] = (-toViewcenter[MDZ]);
	} else if( strcmp( argv[2], "rpc" ) == 0 ) {
		internal.idb_type = ID_RPC;
		internal.idb_ptr = (genptr_t)rt_malloc( sizeof(struct rt_rpc_internal) , "rt_rpc_internal" );
		rpc_ip = (struct rt_rpc_internal *)internal.idb_ptr;
		rpc_ip->rpc_magic = RT_RPC_INTERNAL_MAGIC;
		VSET( rpc_ip->rpc_V, -toViewcenter[MDX] , -toViewcenter[MDY] , -toViewcenter[MDZ]-Viewscale*0.5 );
		VSET( rpc_ip->rpc_H, 0, 0, Viewscale );
		VSET( rpc_ip->rpc_B, 0, (Viewscale*0.5), 0 );
		rpc_ip->rpc_r = Viewscale*0.25;
	} else if( strcmp( argv[2], "rhc" ) == 0 ) {
		internal.idb_type = ID_RHC;
		internal.idb_ptr = (genptr_t)rt_malloc( sizeof(struct rt_rhc_internal) , "rt_rhc_internal" );
		rhc_ip = (struct rt_rhc_internal *)internal.idb_ptr;
		rhc_ip->rhc_magic = RT_RHC_INTERNAL_MAGIC;
		VSET( rhc_ip->rhc_V, -toViewcenter[MDX] , -toViewcenter[MDY] , -toViewcenter[MDZ]-Viewscale*0.5 );
		VSET( rhc_ip->rhc_H, 0, 0, Viewscale );
		VSET( rhc_ip->rhc_B, 0, (Viewscale*0.5), 0 );
		rhc_ip->rhc_r = Viewscale*0.25;
		rhc_ip->rhc_c = Viewscale*0.10;
	} else if( strcmp( argv[2], "epa" ) == 0 ) {
		internal.idb_type = ID_EPA;
		internal.idb_ptr = (genptr_t)rt_malloc( sizeof(struct rt_epa_internal) , "rt_epa_internal" );
		epa_ip = (struct rt_epa_internal *)internal.idb_ptr;
		epa_ip->epa_magic = RT_EPA_INTERNAL_MAGIC;
		VSET( epa_ip->epa_V, -toViewcenter[MDX] , -toViewcenter[MDY] , -toViewcenter[MDZ]-Viewscale*0.5 );
		VSET( epa_ip->epa_H, 0, 0, Viewscale );
		VSET( epa_ip->epa_Au, 0, 1, 0 );
		epa_ip->epa_r1 = Viewscale*0.5;
		epa_ip->epa_r2 = Viewscale*0.25;
	} else if( strcmp( argv[2], "ehy" ) == 0 ) {
		internal.idb_type = ID_EHY;
		internal.idb_ptr = (genptr_t)rt_malloc( sizeof(struct rt_ehy_internal) , "rt_ehy_internal" );
		ehy_ip = (struct rt_ehy_internal *)internal.idb_ptr;
		ehy_ip->ehy_magic = RT_EHY_INTERNAL_MAGIC;
		VSET( ehy_ip->ehy_V, -toViewcenter[MDX] , -toViewcenter[MDY] , -toViewcenter[MDZ]-Viewscale*0.5 );
		VSET( ehy_ip->ehy_H, 0, 0, Viewscale );
		VSET( ehy_ip->ehy_Au, 0, 1, 0 );
		ehy_ip->ehy_r1 = Viewscale*0.5;
		ehy_ip->ehy_r2 = Viewscale*0.25;
		ehy_ip->ehy_c = ehy_ip->ehy_r2;
	} else if( strcmp( argv[2], "eto" ) == 0 ) {
		internal.idb_type = ID_ETO;
		internal.idb_ptr = (genptr_t)rt_malloc( sizeof(struct rt_eto_internal) , "rt_eto_internal" );
		eto_ip = (struct rt_eto_internal *)internal.idb_ptr;
		eto_ip->eto_magic = RT_ETO_INTERNAL_MAGIC;
		VSET( eto_ip->eto_V, -toViewcenter[MDX] , -toViewcenter[MDY] , -toViewcenter[MDZ] );
		VSET( eto_ip->eto_N, 0, 0, 1 );
		VSET( eto_ip->eto_C, Viewscale*0.1, 0, Viewscale*0.1 );
		eto_ip->eto_r = Viewscale*0.5;
		eto_ip->eto_rd = Viewscale*0.05;
	} else if( strcmp( argv[2], "part" ) == 0 ) {
		internal.idb_type = ID_PARTICLE;
		internal.idb_ptr = (genptr_t)rt_malloc( sizeof(struct rt_part_internal) , "rt_part_internal" );
		part_ip = (struct rt_part_internal *)internal.idb_ptr;
		part_ip->part_magic = RT_PART_INTERNAL_MAGIC;
		VSET( part_ip->part_V, -toViewcenter[MDX] , -toViewcenter[MDY] , -toViewcenter[MDZ]-Viewscale*0.5 );
		VSET( part_ip->part_H, 0, 0, Viewscale );
		part_ip->part_vrad = Viewscale*0.5;
		part_ip->part_hrad = Viewscale*0.25;
		part_ip->part_type = RT_PARTICLE_TYPE_CONE;
	} else if( strcmp( argv[2], "nmg" ) == 0 ) {
		struct model *m;
		struct nmgregion *r;
		struct shell *s;

		m = nmg_mm();
		r = nmg_mrsv( m );
		s = RT_LIST_FIRST( shell , &r->s_hd );
		nmg_vertex_g( s->vu_p->v_p, -toViewcenter[MDX], -toViewcenter[MDY], -toViewcenter[MDZ]);
		(void)nmg_meonvu( s->vu_p );
		(void)nmg_ml( s );
		internal.idb_type = ID_NMG;
		internal.idb_ptr = (genptr_t)m;
	} else if( strcmp( argv[2], "pipe" ) == 0 ) {
		struct wdb_pipept *ps;

		internal.idb_type = ID_PIPE;
		internal.idb_ptr = (genptr_t)rt_malloc( sizeof(struct rt_pipe_internal), "rt_pipe_internal" );
		pipe_ip = (struct rt_pipe_internal *)internal.idb_ptr;
		pipe_ip->pipe_magic = RT_PIPE_INTERNAL_MAGIC;
		RT_LIST_INIT( &pipe_ip->pipe_segs_head );
		GETSTRUCT( ps, wdb_pipept );
		ps->l.magic = WDB_PIPESEG_MAGIC;
		VSET( ps->pp_coord, -toViewcenter[MDX] , -toViewcenter[MDY] , -toViewcenter[MDZ]-Viewscale );
		ps->pp_od = 0.5*Viewscale;
		ps->pp_id = 0.5*ps->pp_od;
		ps->pp_bendradius = ps->pp_od;
		RT_LIST_INSERT( &pipe_ip->pipe_segs_head, &ps->l );
		GETSTRUCT( ps, wdb_pipept );
		ps->l.magic = WDB_PIPESEG_MAGIC;
		VSET( ps->pp_coord, -toViewcenter[MDX] , -toViewcenter[MDY] , -toViewcenter[MDZ]+Viewscale );
		ps->pp_od = 0.5*Viewscale;
		ps->pp_id = 0.5*ps->pp_od;
		ps->pp_bendradius = ps->pp_od;
		RT_LIST_INSERT( &pipe_ip->pipe_segs_head, &ps->l );
	} else if( strcmp( argv[2], "ars" ) == 0 ||
		   strcmp( argv[2], "poly" ) == 0 ||
		   strcmp( argv[2], "ebm" ) == 0 ||
		   strcmp( argv[2], "vol" ) == 0 ||
		   strcmp( argv[2], "arbn" ) == 0 ||
		   strcmp( argv[2], "nurb" ) == 0 ||
		   strcmp( argv[2], "spline" ) == 0 )  {
		rt_log("make %s not implimented yet\n", argv[2]);
		return CMD_BAD;
	} else {
		rt_log("make:  %s is not a known primitive\n", argv[2]);
		rt_log("\tchoices are: arb8, arb7, arb6, arb5, arb4, sph, ell, ellg, grip, tor,\n" );
		rt_log("\t\ttgc, tec, rec, trc, rcc, half, rpc, rhc, epa, ehy, eto, part\n" );
		return CMD_BAD;
	}

	if( rt_functab[internal.idb_type].ft_export( &external, &internal, 1.0 ) < 0 )
	{
		rt_log( "f_make: export failure\n" );
		rt_functab[internal.idb_type].ft_ifree( &internal );
		return CMD_BAD;
	}
	rt_functab[internal.idb_type].ft_ifree( &internal );   /* free internal rep */

	/* no interuprts */
	(void)signal( SIGINT, SIG_IGN );

	ngran = (external.ext_nbytes+sizeof(union record)-1) / sizeof(union record);
	if( (dp = db_diradd( dbip, argv[1], -1L, ngran, DIR_SOLID)) == DIR_NULL ||
	    db_alloc( dbip, dp, 1 ) < 0 )
	    {
	    	db_free_external( &external );
	    	ALLOC_ERR;
		return CMD_BAD;
	    }

	if (db_put_external( &external, dp, dbip ) < 0 )
	{
		db_free_external( &external );
		WRITE_ERR;
		return CMD_BAD;
	}
	db_free_external( &external );

	/* draw the "made" solid */
	return f_edit( 2, argv );	/* depends on name being in argv[1] */
}

/* allow precise changes to object rotation */
int
f_rot_obj( argc, argv )
int	argc;
char	**argv;
{
	mat_t temp;
	vect_t s_point, point, v_work, model_pt;

	if( not_state( ST_O_EDIT, "Object Rotation" ) )
		return CMD_BAD;

#ifdef XMGED
	if(!irot_set){
	  irot_x = atof(argv[1]);
	  irot_y = atof(argv[2]);
	  irot_z = atof(argv[3]);
	}

	update_views = 1;
#endif

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

#ifdef XMGED
/*XXX*/ mat_copy(acc_rot_sol, temp); /* used to rotate solid/object axis */
#endif

	/* Record the new rotation matrix into the revised
	 *	modelchanges matrix wrt "point"
	 */
	wrt_point(modelchanges, temp, modelchanges, point);

	new_mats();
	dmaflag = 1;

#ifdef XMGED
	if(rot_hook)
          (*rot_hook)();
#endif

	return CMD_OK;
}

/* allow precise changes to object scaling, both local & global */
int
f_sc_obj( argc, argv )
int	argc;
char	**argv;
{
	mat_t incr;
	vect_t point, temp;

	if( not_state( ST_O_EDIT, "Object Scaling" ) )
		return CMD_BAD;

	if( atof(argv[1]) <= 0.0 ) {
		rt_log("ERROR: scale factor <=  0\n");
		return CMD_BAD;
	}

#ifdef XMGED
	update_views = 1;
#endif

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

	return CMD_OK;
}

/* allow precise changes to object translation */
int
f_tr_obj( argc, argv )
int	argc;
char	**argv;
{
	register int i;
	mat_t incr, old;
	vect_t model_sol_pt, model_incr, ed_sol_pt, new_vertex;

	if( not_state( ST_O_EDIT, "Object Translation") )
		return CMD_BAD;

#ifdef XMGED
	update_views = 1;
#endif

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

#ifdef XMGED
	if(!tran_set) /*   not calling from f_tran()   */
	  set_tran(new_vertex[0], new_vertex[1], new_vertex[2]);

	if(tran_hook)
	  (*tran_hook)();
#endif

	return CMD_OK;
}

/* Change the default region ident codes: item air los mat
 */
int
f_regdef( argc, argv )
int	argc;
char	**argv;
{

	dmaflag = 1;
	item_default = atoi(argv[1]);

	if(argc == 2)
		return CMD_OK;

	air_default = atoi(argv[2]);
	if(air_default) 
		item_default = 0;

	if(argc == 3)
		return CMD_OK;

	los_default = atoi(argv[3]);

	if(argc == 4)
		return CMD_OK;

	mat_default = atoi(argv[4]);

	return CMD_OK;
}

static int frac_stat;
void
mged_add_nmg_part(newname, m)
char *newname;
struct model *m;
{
	struct rt_db_internal	new_intern;
	struct directory *new_dp;
	struct nmgregion *r;
	extern struct rt_tol mged_tol;

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

	/* make sure the geometry/bounding boxes are up to date */
	for (RT_LIST_FOR(r, nmgregion, &m->r_hd))
		nmg_region_a(r, &mged_tol);


	/* Export NMG as a new solid */
	RT_INIT_DB_INTERNAL(&new_intern);
	new_intern.idb_type = ID_NMG;
	new_intern.idb_ptr = (genptr_t)m;

	if( rt_db_put_internal( new_dp, dbip, &new_intern ) < 0 )  {
		/* Free memory */
		nmg_km(m);
		rt_log("rt_db_put_internal() failure\n");
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
int
f_fracture( argc, argv )
int	argc;
char	**argv;
{
	register int i;
	struct directory *old_dp;
	struct rt_db_internal	old_intern;
	struct model	*m, *new_model;
	char		newname[32];
	char		prefix[32];
	int	maxdigits;
	struct nmgregion *r, *new_r;
	struct shell *s, *new_s;
	struct faceuse *fu;
	struct vertex *v_new, *v;
	unsigned long tw, tf, tp;

	rt_log("fracture:");
	for (i=0 ; i < argc ; i++)
		rt_log(" %s", argv[i]);
	rt_log("\n");

	if( (old_dp = db_lookup( dbip,  argv[1], LOOKUP_NOISY )) == DIR_NULL )
		return CMD_BAD;

	if( rt_db_get_internal( &old_intern, old_dp, dbip, rt_identity ) < 0 )  {
		rt_log("rt_db_get_internal() error\n");
		return CMD_BAD;
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

/*	nmg_start_dup(m); */
				new_model = nmg_mm();
				new_r = nmg_mrsv(new_model);
				new_s = RT_LIST_FIRST(shell, &r->s_hd);
				v_new = new_s->vu_p->v_p;
				if (v->vg_p) {
					nmg_vertex_gv(v_new, v->vg_p->coord);
				}
/*	nmg_end_dup(); */

				sprintf(newname, "%s%0*d", prefix, maxdigits, i++);

				mged_add_nmg_part(newname, new_model);
				if (frac_stat) return CMD_BAD;
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
/*	nmg_start_dup(m); */
				NMG_CK_SHELL(new_s);
				nmg_dup_face(fu, new_s);
/*	nmg_end_dup(); */

				sprintf(newname, "%s%0*d", prefix, maxdigits, i++);
				mged_add_nmg_part(newname, new_model);
				if (frac_stat) return CMD_BAD;
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
				mged_add_nmg_part(newname, new_model);
				if (frac_stat) return CMD_BAD;
			}
			while (RT_LIST_NON_EMPTY(&s->eu_hd)) {
				eu = RT_LIST_FIRST(edgeuse, &s->eu_hd);
				new_model = nmg_mm();
				r = nmg_mrsv(new_model);
				new_s = RT_LIST_FIRST(shell, &r->s_hd);

				nmg_dup_edge(eu, new_s);
				nmg_keu(eu);

				sprintf(newname, "%s%0*d", prefix, maxdigits, i++);

				mged_add_nmg_part(newname, new_model);
				if (frac_stat) return CMD_BAD;
			}
#endif
		}
	}
	return CMD_OK;

}
/*
 *			F _ Q O R O T
 *
 * Usage: qorot x y z dx dy dz theta
 *
 *	rotate an object through a specified angle
 *	about a specified ray.
 */
int
f_qorot( argc, argv )
int	argc;
char	**argv;
{
	mat_t temp;
	vect_t s_point, point, v_work, model_pt;
	vect_t	specified_pt, direc;

	if( not_state( ST_O_EDIT, "Object Rotation" ) )
		return CMD_BAD;

	if(movedir != ROTARROW) {
		/* NOT in object rotate mode - put it in obj rot */
		dmp->dmr_light( LIGHT_ON, BE_O_ROTATE );
		dmp->dmr_light( LIGHT_OFF, BE_O_SCALE );
		dmp->dmr_light( LIGHT_OFF, BE_O_XY );
		movedir = ROTARROW;
	}
	VSET(specified_pt, atof(argv[1]), atof(argv[2]), atof(argv[3]));
	VSET(direc, atof(argv[4]), atof(argv[5]), atof(argv[6]));

	/* find point for rotation to take place wrt */
	MAT4X3PNT(model_pt, es_mat, specified_pt);
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
	buildHrot(temp, 0.0, 0.0, atof(argv[7])*degtorad);

	/* Record the new rotation matrix into the revised
	 *	modelchanges matrix wrt "point"
	 */
	wrt_point_direc(modelchanges, temp, modelchanges, point, direc);

	new_mats();
	dmaflag = 1;

	return CMD_OK;
}
