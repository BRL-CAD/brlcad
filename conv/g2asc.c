/*
 *		G 2 A S C . C
 *  
 *  This program generates an ASCII data file which contains
 *  a GED database.
 *
 *  Usage:  g2asc < file.g > file.asc
 *  
 *  Author -
 *  	Charles M Kennedy
 *  	Michael J Muuss
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
 
#include	<stdio.h>
#include "../h/db.h"

extern void	exit();
extern int	close(), creat(), open(), read(), write();
extern int	printf(), fprintf();
extern long	lseek();

void	idendump(), polyhead(), polydata();
void	soldump(), combdump(), membdump(), arsadump(), arsbdump();
void	materdump(), bspldump(), bsurfdump();

union record	record;		/* GED database record */

main()
{
	/* Read database file */
	while( read( 0, (char*)&record, sizeof record ) > 0 )  {

		/* Check record type and skip deleted records */
		if( record.u_id == ID_FREE )  {
			continue;
		}
		if( record.u_id == ID_SOLID )  {
			(void)soldump();
		}
		else if( record.u_id == ID_COMB )  {
			(void)combdump();
		}
		else if( record.u_id == ID_ARS_A )  {
			(void)arsadump();
		}
		else if( record.u_id == ID_P_HEAD )  {
			(void)polyhead();
		}
		else if( record.u_id == ID_P_DATA )  {
			(void)polydata();
		}
		else if( record.u_id == ID_IDENT )  {
			(void)idendump();
		}
		else if( record.u_id == ID_MATERIAL )  {
			materdump();
		}
		else if( record.u_id == ID_BSOLID )  {
			bspldump();
		}
		else if( record.u_id == ID_BSURF )  {
			bsurfdump();
		}
		else  {
			(void)fprintf(stderr,"G2ASC: bad record type\n");
			exit(1);
		}
	}
	return(0);
}

void
idendump()	/* Print out Ident record information */
{
	(void)printf( "%c %d %s\n",
		record.i.i_id,			/* I */
		record.i.i_units,		/* units */
		record.i.i_version		/* version */
	);
	(void)printf( "%s\n",
		record.i.i_title	/* title or description */
	);

	/* Print a warning message on stderr if versions differ */
	if( strcmp( record.i.i_version, ID_VERSION ) != 0 )  {
		(void)fprintf(stderr,"File is Version %s, Program is version %s\n",
			record.i.i_version, ID_VERSION );
	}
}

void
polyhead()	/* Print out Polyhead record information */
{
	(void)printf("%c ", record.p.p_id );		/* P */
	(void)printf("%s", record.p.p_name );		/* unique name */
	(void)printf("\n");			/* Terminate w/ a newline */
}

void
polydata()	/* Print out Polydata record information */
{
	register int i, j;

	(void)printf("%c ", record.q.q_id );		/* Q */
	(void)printf("%d ", record.q.q_count );		/* # of vertices <= 5 */
	for( i = 0; i < 5; i++ )  {			/* [5][3] vertices */
		for( j = 0; j < 3; j++ ) {
			(void)printf("%.9e ", record.q.q_verts[i][j] );
		}
	}
	for( i = 0; i < 5; i++ )  {			/* [5][3] normals */
		for( j = 0; j < 3; j++ ) {
			(void)printf("%.9e ", record.q.q_norms[i][j] );
		}
	}
	(void)printf("\n");			/* Terminate w/ a newline */
}

void
soldump()	/* Print out Solid record information */
{
	register int i;

	(void)printf("%c ", record.s.s_id );	/* S */
	(void)printf("%d ", record.s.s_type );	/* GED primitive type */
	(void)printf("%s ", record.s.s_name );	/* unique name */
	(void)printf("%d ", record.s.s_cgtype );/* COMGEOM solid type */
	for( i = 0; i < 24; i++ )
		(void)printf("%.9e ", record.s.s_values[i] ); /* parameters */
	(void)printf("\n");			/* Terminate w/ a newline */
}

void
combdump()	/* Print out Combination record information */
{
	register int i;
	register int length;	/* Keep track of number of members */

	(void)printf("%c ", record.c.c_id );		/* C */
	if( record.c.c_flags == 'R' )			/* set region flag */
		(void)printf("Y ");			/* Y if `R' */
	else
		(void)printf("N ");			/* N if ` ' */
	(void)printf("%s ", record.c.c_name );		/* unique name */
	(void)printf("%d ", record.c.c_regionid );	/* region ID code */
	(void)printf("%d ", record.c.c_aircode );	/* air space code */
	(void)printf("%d ", record.c.c_length );	/* # of members */
	(void)printf("%d ", record.c.c_num );		/* COMGEOM region # */
	(void)printf("%d ", record.c.c_material );	/* material code */
	(void)printf("%d ", record.c.c_los );		/* equiv. LOS est. */
	(void)printf("%d %d %d %d ",
		record.c.c_override,
		record.c.c_rgb[0],
		record.c.c_rgb[1],
		record.c.c_rgb[2] );
	if( record.c.c_matname[0] )  {
		printf("1 ");	/* flag: line 1 follows */
		if( record.c.c_matparm[0] )
			printf("2 ");	/* flag: line 2 follows */
	}
	(void)printf("\n");			/* Terminate w/ a newline */

	if( record.c.c_matname[0] )  {
		(void)printf("%s\n", record.c.c_matname );
		if( record.c.c_matparm[0] )
			(void)printf("%s\n", record.c.c_matparm );
	}

	length = (int)record.c.c_length;	/* Get # of member records */

	for( i = 0; i < length; i++ )  {
		(void)membdump();
	}
}

void
membdump()	/* Print out Member record information */
{
	register int i;

	/* Read in a member record for processing */
	(void)read( 0, (char*)&record, sizeof record );

	(void)printf("%c ", record.M.m_id );		/* M */
	(void)printf("%c ", record.M.m_relation );	/* Boolean oper. */
	(void)printf("%s ", record.M.m_instname );	/* referred-to obj. */
	for( i = 0; i < 16; i++ )			/* homogeneous transform matrix */
		(void)printf("%.9e ", record.M.m_mat[i] );
	(void)printf("%d ", record.M.m_num );		/* COMGEOM solid # */
	(void)printf("\n");				/* Terminate w/ nl */
}

void
arsadump()	/* Print out ARS record information */
{
	register int i;
	register int length;	/* Keep track of number of ARS B records */

	(void)printf("%c ", record.a.a_id );	/* A */
	(void)printf("%d ", record.a.a_type );	/* primitive type */
	(void)printf("%s ", record.a.a_name );	/* unique name */
	(void)printf("%d ", record.a.a_m );	/* # of curves */
	(void)printf("%d ", record.a.a_n );	/* # of points per curve */
	(void)printf("%d ", record.a.a_curlen );/* # of granules per curve */
	(void)printf("%d ", record.a.a_totlen );/* # of granules for ARS */
	(void)printf("%.9e ", record.a.a_xmax );	/* max x coordinate */
	(void)printf("%.9e ", record.a.a_xmin );	/* min x coordinate */
	(void)printf("%.9e ", record.a.a_ymax );	/* max y coordinate */
	(void)printf("%.9e ", record.a.a_ymin );	/* min y coordinate */
	(void)printf("%.9e ", record.a.a_zmax );	/* max z coordinate */
	(void)printf("%.9e ", record.a.a_zmin );	/* min z coordinate */
	(void)printf("\n");			/* Terminate w/ a newline */
			
	length = (int)record.a.a_totlen;	/* Get # of ARS B records */

	for( i = 0; i < length; i++ )  {
		(void)arsbdump();
	}
}

void
arsbdump()	/* Print out ARS B record information */
{
	register int i;
	
	/* Read in a member record for processing */
	(void)read( 0, (char*)&record, sizeof record );

	(void)printf("%c ", record.b.b_id );		/* B */
	(void)printf("%d ", record.b.b_type );		/* primitive type */
	(void)printf("%d ", record.b.b_n );		/* current curve # */
	(void)printf("%d ", record.b.b_ngranule );	/* current granule */
	for( i = 0; i < 24; i++ )  {			/* [8*3] vectors */
		(void)printf("%.9e ", record.b.b_values[i] );
	}
	(void)printf("\n");			/* Terminate w/ a newline */
}

void
materdump()	/* Print out material description record information */
{
	(void)printf( "%c %d %d %d %d %d %d\n",
		record.md.md_id,			/* m */
		record.md.md_flags,			/* UNUSED */
		record.md.md_low,	/* low end of region IDs affected */
		record.md.md_hi,	/* high end of region IDs affected */
		record.md.md_r,
		record.md.md_g,		/* color of regions: 0..255 */
		record.md.md_b );
}

void
bspldump()	/* Print out B-spline solid description record information */
{
	(void)printf( "%c %s %d %.9e\n",
		record.B.B_id,		/* b */
		record.B.B_name,	/* unique name */
		record.B.B_nsurf,	/* # of surfaces in this solid */
		record.B.B_resolution );	/* resolution of flatness */
}

void
bsurfdump()	/* Print d-spline surface description record information */
{
	register int i;
	register float *vp;
	int nbytes, count;
	float *fp;

	(void)printf( "%c %d %d %d %d %d %d %d %d %d\n",
		record.d.d_id,		/* D */
		record.d.d_order[0],	/* order of u and v directions */
		record.d.d_order[1],	/* order of u and v directions */
		record.d.d_kv_size[0],	/* knot vector size (u and v) */
		record.d.d_kv_size[1],	/* knot vector size (u and v) */
		record.d.d_ctl_size[0],	/* control mesh size (u and v) */
		record.d.d_ctl_size[1],	/* control mesh size (u and v) */
		record.d.d_geom_type,	/* geom type 3 or 4 */
		record.d.d_nknots,	/* # granules of knots */
		record.d.d_nctls );	/* # granules of ctls */
	/* 
	 * The b_surf_head record is followed by
	 * d_nknots granules of knot vectors (first u, then v),
	 * and then by d_nctls granules of control mesh information.
	 * Note that neither of these have an ID field!
	 *
	 * B-spline surface record, followed by
	 *	d_kv_size[0] floats,
	 *	d_kv_size[1] floats,
	 *	padded to d_nknots granules, followed by
	 *	ctl_size[0]*ctl_size[1]*geom_type floats,
	 *	padded to d_nctls granules.
	 *
	 * IMPORTANT NOTE: granule == sizeof(union record)
	 */

	/* Malloc and clear memory for the KNOT DATA and read it */
	nbytes = record.d.d_nknots * sizeof(union record);
	if( (vp = (float *)malloc(nbytes))  == (float *)0 )  {
		(void)fprintf(stderr, "G2ASC: spline knot malloc error\n");
		exit(1);
	}
	fp = vp;
	(void)bzero( (char *)fp, nbytes );
	count =	read( 0, (char*)fp, nbytes );
	if( count != nbytes )  {
		(void)fprintf(stderr, "G2ASC: spline knot read failure\n");
		exit(1);
	}
	/* Print the knot vector information */
	count = record.d.d_kv_size[0] + record.d.d_kv_size[1];
	for( i = 0; i < count; i++ )  {
		(void)printf("%.9e\n", *vp++);
	}
	/* Free the knot data memory */
	(void)free( (char *)fp );

	/* Malloc and clear memory for the CONTROL MESH data and read it */
	nbytes = record.d.d_nctls * sizeof(union record);
	if( (vp = (float *)malloc(nbytes))  == (float *)0 )  {
		(void)fprintf(stderr, "G2ASC: control mesh malloc error\n");
		exit(1);
	}
	fp = vp;
	(void)bzero( (char *)fp, nbytes );
	count =	read( 0, (char*)fp, nbytes );
	if( count != nbytes )  {
		(void)fprintf(stderr, "G2ASC: control mesh read failure\n");
		exit(1);
	}
	/* Print the control mesh information */
	count = record.d.d_ctl_size[0] * record.d.d_ctl_size[1] *
		record.d.d_geom_type;
	for( i = 0; i < count; i++ )  {
		(void)printf("%.9e\n", *vp++);
	}
	/* Free the control mesh memory */
	(void)free( (char *)fp );
}
