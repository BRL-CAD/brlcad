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
 *	f_extrude	"extrude" command -- project an ARB face
 *	f_arbdef	define ARB8 using rot fb angles to define face
 *	f_edcomb	modify combination record info
 *	f_mirface	mirror an ARB face
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
#include "db.h"
#include "./sedit.h"
#include "raytrace.h"
#include "./ged.h"
#include "externs.h"
#include "./solid.h"
#include "./dm.h"

void	aexists();
void	ext4to6();

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

/* Extrude command - project an arb face */
/* Format: extrude face distance	*/
void
f_extrude( argc, argv )
int	argc;
char	**argv;
{
	register int i, j;
	static int face;
	static int pt[4];
	static int prod;
	static fastf_t dist;
	static struct solidrec lsolid;	/* local copy of solid */

	if( not_state( ST_S_EDIT, "Extrude" ) )
		return;

	if( es_gentype != GENARB8 )  {
		(void)printf("Extrude: solid type must be ARB\n");
		return;
	}

	if(es_type != ARB8 && es_type != ARB6 && es_type != ARB4) {
		(void)printf("ARB%d: extrusion of faces not allowed\n",es_type);
		return;
	}

	face = atoi( argv[1] );

	/* get distance to project face */
	dist = atof( argv[2] );
	/* apply es_mat[15] to get to real model space */
	/* convert from the local unit (as input) to the base unit */
	dist = dist * es_mat[15] * local2base;

	/* convert to point notation in temporary buffer */
	VMOVE( &lsolid.s_values[0], &es_rec.s.s_values[0] );
	for( i = 3; i <= 21; i += 3 )  {  
		VADD2(&lsolid.s_values[i], &es_rec.s.s_values[i], &lsolid.s_values[0]);
	}

	if( (es_type == ARB6 || es_type == ARB4) && face < 1000 ) {
		/* 3 point face */
		pt[0] = face / 100;
		i = face - (pt[0]*100);
		pt[1] = i / 10;
		pt[2] = i - (pt[1]*10);
		pt[3] = 1;
	}
	else {
		pt[0] = face / 1000;
		i = face - (pt[0]*1000);
		pt[1] = i / 100;
		i = i - (pt[1]*100);
		pt[2] = i / 10;
		pt[3] = i - (pt[2]*10);
	}

	/* user can input face in any order - will use product of
	 * face points to distinguish faces:
	 *    product       face
	 *       24         1234 for ARB8
	 *     1680         5678 for ARB8
	 *      252         2367 for ARB8
	 *      160         1548 for ARB8
	 *      672         4378 for ARB8
	 *       60         1256 for ARB8
	 *	 10	    125 for ARB6
	 *	 72	    346 for ARB6
	 * --- special case to make ARB6 from ARB4
	 * ---   provides easy way to build ARB6's
	 *        6	    123 for ARB4
	 *	  8	    124 for ARB4
 	 *	 12	    134 for ARB4
	 *	 24	    234 for ARB4
	 */
	prod = 1;
	for( i = 0; i <= 3; i++ )  {
		prod *= pt[i];
		if(es_type == ARB6 && pt[i] == 6)
			pt[i]++;
		if(es_type == ARB4 && pt[i] == 4)
			pt[i]++;
		pt[i]--;
		if( pt[i] > 7 )  {
			(void)printf("bad face: %d\n",face);
			return;
		}
	}

	/* find plane containing this face */
	if( planeqn(6, pt[0], pt[1], pt[2], &lsolid) ) {
		(void)printf("face: %d is not a plane\n",face);
		return;
	}
	/* get normal vector of length == dist */
	for( i = 0; i < 3; i++ )
		es_peqn[6][i] *= dist;

	/* protrude the selected face */
	switch( prod )  {

	case 24:   /* protrude face 1234 */
		if(es_type == ARB6) {
			(void)printf("ARB6: extrusion of face %d not allowed\n",face);
			return;
		}
		if(es_type == ARB4)
			goto a4toa6;	/* extrude face 234 of ARB4 to make ARB6 */

		for( i = 0; i < 4; i++ )  {
			j = i + 4;
			VADD2( &lsolid.s_values[j*3],
				&lsolid.s_values[i*3],
				&es_peqn[6][0]);
		}
		break;

	case 6:		/* extrude ARB4 face 123 to make ARB6 */
	case 8:		/* extrude ARB4 face 124 to make ARB6 */
	case 12:	/* extrude ARB4 face 134 to Make ARB6 */
a4toa6:
		ext4to6(pt[0], pt[1], pt[2], &lsolid);
		es_rec.s.s_cgtype = ARB6;
		es_edflag = IDLE;
		es_menu = 0;
		sedit_menu();
	break;

	case 1680:   /* protrude face 5678 */
		for( i = 0; i < 4; i++ )  {
			j = i + 4;
			VADD2( &lsolid.s_values[i*3],
				&lsolid.s_values[j*3],
				&es_peqn[6][0] );
		}
		break;

	case 60:   /* protrude face 1256 */
	case 10:   /* extrude face 125 of ARB6 */
		VADD2( &lsolid.s_values[9],
			&lsolid.s_values[0],
			&es_peqn[6][0] );
		VADD2( &lsolid.s_values[6],
			&lsolid.s_values[3],
			&es_peqn[6][0] );
		VADD2( &lsolid.s_values[21],
			&lsolid.s_values[12],
			&es_peqn[6][0] );
		VADD2( &lsolid.s_values[18],
			&lsolid.s_values[15],
			&es_peqn[6][0] );
		break;

	case 672:   /* protrude face 4378 */
	case 72:	/* extrude face 346 of ARB6 */
		VADD2( &lsolid.s_values[0],
			&lsolid.s_values[9],
			&es_peqn[6][0] );
		VADD2( &lsolid.s_values[3],
			&lsolid.s_values[6],
			&es_peqn[6][0] );
		VADD2( &lsolid.s_values[15],
			&lsolid.s_values[18],
			&es_peqn[6][0] );
		VADD2( &lsolid.s_values[12],
			&lsolid.s_values[21],
			&es_peqn[6][0] );
		break;

	case 252:   /* protrude face 2367 */
		VADD2( &lsolid.s_values[0],
			&lsolid.s_values[3],
			&es_peqn[6][0] );
		VADD2( &lsolid.s_values[9],
			&lsolid.s_values[6],
			&es_peqn[6][0] );
		VADD2( &lsolid.s_values[12],
			&lsolid.s_values[15],
			&es_peqn[6][0] );
		VADD2( &lsolid.s_values[21],
			&lsolid.s_values[18],
			&es_peqn[6][0] );
		break;

	case 160:   /* protrude face 1548 */
		VADD2( &lsolid.s_values[3],
			&lsolid.s_values[0],
			&es_peqn[6][0] );
		VADD2( &lsolid.s_values[15],
			&lsolid.s_values[12],
			&es_peqn[6][0] );
		VADD2( &lsolid.s_values[6],
			&lsolid.s_values[9],
			&es_peqn[6][0] );
		VADD2( &lsolid.s_values[18],
			&lsolid.s_values[21],
			&es_peqn[6][0] );
		break;

	case 120:
	case 180:
		(void)printf("ARB6: extrusion of face %d not allowed\n",face);
		return;

	default:
		(void)printf("bad face: %d\n", face );
		return;
	}

	/* redo the plane equations */
	for(i=0; i<6; i++) {
		if(arb_faces[es_type-4][i*4] == -1)
			break;
		pt[0] = arb_faces[es_type-4][i*4];
		pt[1] = arb_faces[es_type-4][i*4+1];
		pt[2] = arb_faces[es_type-4][i*4+2];
		if(planeqn(i, pt[0], pt[1], pt[2], &lsolid)) {
			(void)printf("No equation for face %d%d%d%d\n",
				pt[0]+1,pt[1]+1,pt[2]+1,arb_faces[es_type-4][i*4+3]);
			return;
		}
	}

	/* Convert back to point&vector notation */
	VMOVE( &es_rec.s.s_values[0], &lsolid.s_values[0] );
	for( i = 3; i <= 21; i += 3 )  {  
		VSUB2( &es_rec.s.s_values[i], &lsolid.s_values[i], &lsolid.s_values[0]);
	}

	/* draw the updated solid */
	replot_editing_solid();
	pr_solid( &es_rec.s );
	dmaflag = 1;
}

/* define an arb8 using rot fb angles to define a face */
/* Format: a name rot fb	*/
void
f_arbdef( argc, argv )
int	argc;
char	**argv;
{
	register struct directory *dp;
	union record record;
	int i, j;
	fastf_t rota, fb;
	vect_t	norm;

	if( db_lookup( dbip,  argv[1] , LOOKUP_QUIET ) != DIR_NULL )  {
		aexists( argv[1] );
		return;
	}

	/* get rotation angle */
	rota = atof( argv[2] ) * degtorad;

	/* get fallback angle */
	fb = atof( argv[3] ) * degtorad;

	if( (dp = db_diradd( dbip,  argv[1], -1, 1, DIR_SOLID )) == DIR_NULL ||
	    db_alloc( dbip, dp, 1 ) < 0 )  {
	    	ALLOC_ERR_return;
	}
	NAMEMOVE( argv[1], record.s.s_name );
	record.s.s_id = ID_SOLID;
	record.s.s_type = GENARB8;
	record.s.s_cgtype = ARB8;

	/* put vertex of new solid at center of screen */
	record.s.s_values[0] = -toViewcenter[MDX];
	record.s.s_values[1] = -toViewcenter[MDY];
	record.s.s_values[2] = -toViewcenter[MDZ];

	/* calculate normal vector (length = 2) defined by rot,fb */
	norm[0] = cos(fb) * cos(rota) * -50.8;
	norm[1] = cos(fb) * sin(rota) * -50.8;
	norm[2] = sin(fb) * -50.8;

	for( i = 3; i < 24; i++ )
		record.s.s_values[i] = 0.0;

	/* find two perpendicular vectors which are perpendicular to norm */
	j = 0;
	for( i = 0; i < 3; i++ )  {
		if( fabs(norm[i]) < fabs(norm[j]) )
			j = i;
	}
	record.s.s_values[j+3] = 1.0;
	VCROSS( &record.s.s_values[9], &record.s.s_values[3], norm );
	VCROSS( &record.s.s_values[3], &record.s.s_values[9], norm );

	/* create new rpp 20x20x2 */
	/* the 20x20 faces are in rot,fb plane */
	VUNITIZE( &record.s.s_values[3] );
	VUNITIZE( &record.s.s_values[9] );
	VSCALE(&record.s.s_values[3], &record.s.s_values[3], 508.0);
	VSCALE(&record.s.s_values[9], &record.s.s_values[9], 508.0);
	VADD2( &record.s.s_values[6],
		&record.s.s_values[3],
		&record.s.s_values[9] );
	VMOVE( &record.s.s_values[12], norm );
	for( i = 3; i < 12; i += 3 )  {
		j = i + 12;
		VADD2( &record.s.s_values[j], &record.s.s_values[i], norm );
	}

	/* update dbip->dbi_fd and draw new arb8 */
	if( db_put( dbip, dp, &record, 0, 1 ) < 0 )  WRITE_ERR_return;
	if( no_memory )  {
		(void)printf(
			"ARB8 (%s) created but no memory left to draw it\n",
			argv[1] );
		return;
	}

	/* draw the "made" solid */
	f_edit( 2, argv );	/* depends on name being in argv[1] */
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

/* Mirface command - mirror an arb face */
/* Format: mirror face axis	*/
void
f_mirface( argc, argv )
int	argc;
char	**argv;
{
	register int i, j, k;
	static int face;
	static int pt[4];
	static int prod;
	static vect_t work;
	static struct solidrec lsolid;	/* local copy of solid */

	if( not_state( ST_S_EDIT, "Mirface" ) )
		return;

	if( es_gentype != GENARB8 )  {
		(void)printf("Mirface: solid type must be ARB\n");
		return;
	}

	if(es_type != ARB8 && es_type != ARB6) {
		(void)printf("ARB%d: mirroring of faces not allowed\n",es_type);
		return;
	}
	face = atoi( argv[1] );
	if( face > 9999 || (face < 1000 && es_type != ARB6) ) {
		(void)printf("ERROR: %d bad face\n",face);
		return;
	}
	/* check which axis */
	k = -1;
	if( strcmp( argv[2], "x" ) == 0 )
		k = 0;
	if( strcmp( argv[2], "y" ) == 0 )
		k = 1;
	if( strcmp( argv[2], "z" ) == 0 )
		k = 2;
	if( k < 0 ) {
		(void)printf("axis must be x, y or z\n");
		return;
	}

	work[0] = work[1] = work[2] = 1.0;
	work[k] = -1.0;

	/* convert to point notation in temporary buffer */
	VMOVE( &lsolid.s_values[0], &es_rec.s.s_values[0] );
	for( i = 3; i <= 21; i += 3 )  {  
		VADD2(&lsolid.s_values[i], &es_rec.s.s_values[i], &lsolid.s_values[0]);
	}

	if(es_type == ARB6 && face < 1000) { 	/* 3 point face */
		pt[0] = face / 100;
		i = face - (pt[0]*100);
		pt[1] = i / 10;
		pt[2] = i - (pt[1]*10);
		pt[3] = 1;
	}
	else {
		pt[0] = face / 1000;
		i = face - (pt[0]*1000);
		pt[1] = i / 100;
		i = i - (pt[1]*100);
		pt[2] = i / 10;
		pt[3] = i - (pt[2]*10);
	}

	/* user can input face in any order - will use product of
	 * face points to distinguish faces:
	 *    product       face
	 *       24         1234 for ARB8
	 *     1680         5678 for ARB8
	 *      252         2367 for ARB8
	 *      160         1548 for ARB8
	 *      672         4378 for ARB8
	 *       60         1256 for ARB8
	 *	 10	    125 for ARB6
	 *	 72	    346 for ARB6
	 */
	prod = 1;
	for( i = 0; i <= 3; i++ )  {
		prod *= pt[i];
		pt[i]--;
		if( pt[i] > 7 )  {
			(void)printf("bad face: %d\n",face);
			return;
		}
	}

	/* mirror the selected face */
	switch( prod )  {

	case 24:   /* mirror face 1234 */
		if(es_type == ARB6) {
			(void)printf("ARB6: mirroring of face %d not allowed\n",face);
			return;
		}
		for( i = 0; i < 4; i++ )  {
			j = i + 4;
			VELMUL( &lsolid.s_values[j*3],
				&lsolid.s_values[i*3],
				work);
		}
		break;

	case 1680:   /* mirror face 5678 */
		for( i = 0; i < 4; i++ )  {
			j = i + 4;
			VELMUL( &lsolid.s_values[i*3],
				&lsolid.s_values[j*3],
				work );
		}
		break;

	case 60:   /* mirror face 1256 */
	case 10:	/* mirror face 125 of ARB6 */
		VELMUL( &lsolid.s_values[9],
			&lsolid.s_values[0],
			work );
		VELMUL( &lsolid.s_values[6],
			&lsolid.s_values[3],
			work );
		VELMUL( &lsolid.s_values[21],
			&lsolid.s_values[12],
			work );
		VELMUL( &lsolid.s_values[18],
			&lsolid.s_values[15],
			work );
		break;

	case 672:   /* mirror face 4378 */
	case 72:	/* mirror face 346 of ARB6 */
		VELMUL( &lsolid.s_values[0],
			&lsolid.s_values[9],
			work );
		VELMUL( &lsolid.s_values[3],
			&lsolid.s_values[6],
			work );
		VELMUL( &lsolid.s_values[15],
			&lsolid.s_values[18],
			work );
		VELMUL( &lsolid.s_values[12],
			&lsolid.s_values[21],
			work );
		break;

	case 252:   /* mirror face 2367 */
		VELMUL( &lsolid.s_values[0],
			&lsolid.s_values[3],
			work );
		VELMUL( &lsolid.s_values[9],
			&lsolid.s_values[6],
			work );
		VELMUL( &lsolid.s_values[12],
			&lsolid.s_values[15],
			work );
		VELMUL( &lsolid.s_values[21],
			&lsolid.s_values[18],
			work );
		break;

	case 160:   /* mirror face 1548 */
		VELMUL( &lsolid.s_values[3],
			&lsolid.s_values[0],
			work );
		VELMUL( &lsolid.s_values[15],
			&lsolid.s_values[12],
			work );
		VELMUL( &lsolid.s_values[6],
			&lsolid.s_values[9],
			work );
		VELMUL( &lsolid.s_values[18],
			&lsolid.s_values[21],
			work );
		break;

	case 120:
	case 180:
		(void)printf("ARB6: mirroring of face %d not allowed\n",face);
		return;

	default:
		(void)printf("bad face: %d\n", face );
		return;
	}

	/* redo the plane equations */
	for(i=0; i<6; i++) {
		if(arb_faces[es_type-4][i*4] == -1)
			break;
		pt[0] = arb_faces[es_type-4][i*4];
		pt[1] = arb_faces[es_type-4][i*4+1];
		pt[2] = arb_faces[es_type-4][i*4+2];
		if(planeqn(i, pt[0], pt[1], pt[2], &lsolid)) {
			(void)printf("No equation for face %d%d%d%d\n",
				pt[0]+1,pt[1]+1,pt[2]+1,arb_faces[es_type-4][i*4+3]);
			return;
		}
	}

	/* Convert back to point&vector notation */
	VMOVE( &es_rec.s.s_values[0], &lsolid.s_values[0] );
	for( i = 3; i <= 21; i += 3 )  {  
		VSUB2( &es_rec.s.s_values[i], &lsolid.s_values[i], &lsolid.s_values[0]);
	}

	/* draw the updated solid */
	replot_editing_solid();
	pr_solid( &es_rec.s );
	dmaflag = 1;
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
	if(state == ST_S_EDIT)
		pr_solid( &es_rec.s );
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
	register int i;
	char	title[256];

	title[0] = '\0';
	for(i=1; i<argc; i++) {
		(void)strcat(title, argv[i]);
		(void)strcat(title, " ");
	}

	if( db_ident( dbip, title, dbip->dbi_localunit ) < 0 )
		printf("Error: unable to change database title\n");
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
	/* XXX should have an es_keypoint for this */
	MAT4X3PNT(model_pt, es_mat, es_rec.s.s_values);
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
	/* XXX should have an es_keypoint for this */
	MAT4X3PNT(temp, es_mat, es_rec.s.s_values);
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
	/* XXX should have an es_keypoint for this */
	MAT4X3PNT(model_sol_pt, es_mat, es_rec.s.s_values);
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


/* Edgedir command:  define the direction of an arb edge being moved
 *	Format:  edgedir deltax deltay deltaz
	     OR  edgedir rot fb
 */

void
f_edgedir( argc, argv )
int	argc;
char	**argv;
{
	register int i;
	vect_t slope;
	FAST fastf_t rot, fb;

	if( not_state( ST_S_EDIT, "Edgedir" ) )
		return;

	if( es_edflag != EARB ) {
		(void)printf("Not moving an ARB edge\n");
		return;
	}

	if( es_gentype != GENARB8 ) {
		(void)printf("Edgedir: solid type must be an ARB\n");
		return;
	}

	/* set up slope -
	 *	if 2 values input assume rot, fb used
	 *	else assume delta_x, delta_y, delta_z
	 */
	if( argc == 3 ) {
		rot = atof( argv[1] ) * degtorad;
		fb = atof( argv[2] ) * degtorad;
		slope[0] = cos(fb) * cos(rot);
		slope[1] = cos(fb) * sin(rot);
		slope[2] = sin(fb);
	}
	else {
		for(i=0; i<3; i++) {
			/* put edge slope in slope[] array */
			slope[i] = atof( argv[i+1] );
		}
	}

	if(MAGNITUDE(slope) == 0) {
		(void)printf("BAD slope\n");
		return;
	}

	/* get it done */
	newedge = 1;
	editarb( slope );
	sedraw++;

}


/*	EXT4TO6():	extrudes face pt1 pt2 pt3 of an ARB4 "distance"
 *			to produce ARB6 using solid record "sp"
 */
void
ext4to6(pt1, pt2, pt3, sp)
int pt1, pt2, pt3;
register struct solidrec *sp;
{
	struct solidrec tmp;
	register int i;

	VMOVE(&tmp.s_values[0], &sp->s_values[pt1*3]);
	VMOVE(&tmp.s_values[3], &sp->s_values[pt2*3]);
	VMOVE(&tmp.s_values[12], &sp->s_values[pt3*3]);
	VMOVE(&tmp.s_values[15], &sp->s_values[pt3*3]);

	/* extrude "distance" to get remaining points */
	VADD2(&tmp.s_values[6], &tmp.s_values[3], &es_peqn[6][0]);
	VADD2(&tmp.s_values[9], &tmp.s_values[0], &es_peqn[6][0]);
	VADD2(&tmp.s_values[18], &tmp.s_values[12], &es_peqn[6][0]);
	VMOVE(&tmp.s_values[21], &tmp.s_values[18]);

	/* copy to the original record */
	for(i=0; i<=21; i+=3) {
		VMOVE(&sp->s_values[i], &tmp.s_values[i]);
	}
}



