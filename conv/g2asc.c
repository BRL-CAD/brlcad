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
void	materdump();

union record	record;		/* GED database record */
int	count;				/* Number of records */

main()
{
	count = 0;

	/* Read database file */
	while( read( 0, (char*)&record, sizeof record ) > 0 )  {
		count++;

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
	(void)printf("%c ", record.i.i_id );		/* I */
	(void)printf("%d ", record.i.i_units );		/* units */
	(void)printf("%s ", record.i.i_version );	/* version */
	(void)printf("\n");			/* Terminate w/ a newline */
	(void)printf("%s", record.i.i_title );		/* title or description */
	(void)printf("\n");			/* Terminate w/ a newline */

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
	(void)printf("\n");			/* Terminate w/ a newline */

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
materdump()
{
	(void)printf( "%c %d %d %d %d %d %d %s\n",
		record.md.md_id,
		record.md.md_flags,
		record.md.md_low,
		record.md.md_hi,
		record.md.md_r,
		record.md.md_g,
		record.md.md_b );
}
