/*
 *		A S C 2 G . C
 *  
 *  This program generates a GED database from an
 *  ASCII GED data file.
 *
 *  Usage:  asc2g < file.asc > file.g
 *  
 *  Authors -
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
extern int	printf(), fprintf(), sscanf();		/* bzero()? */
extern char	*strcpy();

void	identbld(), polyhbld(), polydbld();
void	solbld(), combbld(), membbld(), arsabld(), arsbbld();
void	materbld(), bsplbld(), bsurfbld();

static union record	record;		/* GED database record */
#define BUFSIZE		1024		/* Record input buffer size */
static char buf[BUFSIZE];		/* Record input buffer */

main(argc, argv)
char **argv;
{
	/* Read ASCII input file, each record on a line */
	while( ( fgets( buf, BUFSIZE, stdin ) ) != (char *)0 )  {

		/* Clear the output record */
		(void)bzero( (char *)&record, sizeof(record) );

		/* Check record type */
		if( argc > 1 )
			fprintf(stderr,"0%o (%c)\n", buf[0], buf[0] );
		if( buf[0] == ID_SOLID )  {
			/* Build the record */
			solbld();
		}
		else if( buf[0] == ID_COMB )  {
			/* Build the record */
			combbld();
		}
		else if( buf[0] == ID_MEMB )  {
			/* Build the record */
			membbld();
		}
		else if( buf[0] == ID_ARS_A )  {
			/* Build the record */
			arsabld();
		}
		else if( buf[0] == ID_ARS_B )  {
			/* Build the record */
			arsbbld();
		}
		else if( buf[0] == ID_P_HEAD )  {
			/* Build the record */
			polyhbld();
		}
		else if( buf[0] == ID_P_DATA )  {
			/* Build the record */
			polydbld();
		}
		else if( buf[0] == ID_IDENT )  {
			/* Build the record */
			identbld();
		}
		else if( buf[0] == ID_MATERIAL )  {
			/* Build the record */
			materbld();
		}
		else if( buf[0] == ID_BSOLID )  {
			/* Build the record */
			bsplbld();
		}
		else if( buf[0] == ID_BSURF )  {
			/* Build the record */
			bsurfbld();
		}
		else  {
			(void)fprintf(stderr,"ASC2G: bad record type '%c'\n", buf[0]);
			exit(1);
		}
	}
	return(0);
}

void
solbld()	/* Build Solid record */
{
	register int i;
	auto int temp1, temp2;

	i=sscanf( buf,
		/*	      0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 */
		"%c %d %s %d %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f",
		&record.s.s_id,
		&temp1,
		&record.s.s_name[0],
		&temp2,
		&record.s.s_values[0],
		&record.s.s_values[1],
		&record.s.s_values[2],
		&record.s.s_values[3],
		&record.s.s_values[4],
		&record.s.s_values[5],
		&record.s.s_values[6],
		&record.s.s_values[7],
		&record.s.s_values[8],
		&record.s.s_values[9],
		&record.s.s_values[10],
		&record.s.s_values[11],
		&record.s.s_values[12],
		&record.s.s_values[13],
		&record.s.s_values[14],
		&record.s.s_values[15],
		&record.s.s_values[16],
		&record.s.s_values[17],
		&record.s.s_values[18],
		&record.s.s_values[19],
		&record.s.s_values[20],
		&record.s.s_values[21],
		&record.s.s_values[22],
		&record.s.s_values[23]
	);
	if( i != 24+4 )  {
		fprintf(stderr,"solbld(%s)  %d items converted, dropped\n",
			record.s.s_name, i);
		return;
	}
	record.s.s_type = (char)temp1;
	record.s.s_cgtype = (short)temp2;

	/* Write out the record */
	(void)fwrite( (char *)&record, sizeof record, 1, stdout );
}

void
combbld()	/* Build Combination record */
{
	int temp1, temp2, temp3, temp4, temp5, temp6;
	int temp_override, temp_r, temp_g, temp_b;
	int temp_nflag, temp_pflag;

	temp_override = 0;
	temp_nflag = temp_pflag = 0;	/* optional fields */

	(void)sscanf( buf, "%c %c %s %d %d %d %d %d %d %d %d %d %d %d %d",
		&record.c.c_id,
		&record.c.c_flags,
		&record.c.c_name[0],
		&temp1,
		&temp2,
		&temp3,
		&temp4,
		&temp5,
		&temp6,
		&temp_override,
		&temp_r, &temp_g, &temp_b,
		&temp_nflag,
		&temp_pflag
	);
	if( record.c.c_flags == 'Y' )
		record.c.c_flags = 'R';
	else
		record.c.c_flags = ' ';
	record.c.c_regionid = (short)temp1;
	record.c.c_aircode = (short)temp2;
	record.c.c_length = (short)temp3;
	record.c.c_num = (short)temp4;
	record.c.c_material = (short)temp5;
	record.c.c_los = (short)temp6;
	record.c.c_override = temp_override;
	record.c.c_rgb[0] = temp_r;
	record.c.c_rgb[1] = temp_g;
	record.c.c_rgb[2] = temp_b;

	if( temp_nflag )  {
		fgets( buf, BUFSIZE, stdin );
		zap_nl();
		strncpy( record.c.c_matname, buf, sizeof(record.c.c_matname) );
	}
	if( temp_pflag )  {
		fgets( buf, BUFSIZE, stdin );
		zap_nl();
		strncpy( record.c.c_matparm, buf, sizeof(record.c.c_matparm) );
	}

	/* Write out the record */
	(void)fwrite( (char *)&record, sizeof record, 1, stdout );
}

void
membbld()	/* Build Member record */
{
	int temp1;

		/*		      0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 */
	(void)sscanf( buf, "%c %c %s %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %d", 
		&record.M.m_id,
		&record.M.m_relation,
		&record.M.m_instname[0],
		&record.M.m_mat[0],
		&record.M.m_mat[1],
		&record.M.m_mat[2],
		&record.M.m_mat[3],
		&record.M.m_mat[4],
		&record.M.m_mat[5],
		&record.M.m_mat[6],
		&record.M.m_mat[7],
		&record.M.m_mat[8],
		&record.M.m_mat[9],
		&record.M.m_mat[10],
		&record.M.m_mat[11],
		&record.M.m_mat[12],
		&record.M.m_mat[13],
		&record.M.m_mat[14],
		&record.M.m_mat[15],
		&temp1
	);
	record.M.m_num = (short)temp1;

	/* Write out the record */
	(void)fwrite( (char *)&record, sizeof record, 1, stdout );
}

void
arsabld()	/* Build ARS A record */
{
	int temp1, temp2, temp3, temp4, temp5;

	(void)sscanf( buf, "%c %d %s %d %d %d %d %f %f %f %f %f %f",
		&record.a.a_id,
		&temp1,
		&record.a.a_name[0],
		&temp2,
		&temp3,
		&temp4,
		&temp5,
		&record.a.a_xmax,
		&record.a.a_xmin,
		&record.a.a_ymax,
		&record.a.a_ymin,
		&record.a.a_zmax,
		&record.a.a_zmin
	);
	record.a.a_type = (char)temp1;
	record.a.a_m = (short)temp2;
	record.a.a_n = (short)temp3;
	record.a.a_curlen = (short)temp4;
	record.a.a_totlen = (short)temp5;

	/* Write out the record */
	(void)fwrite( (char *)&record, sizeof record, 1, stdout );
}

void
arsbbld()	/* Build ARS B record */
{
	int temp1, temp2, temp3;

		/*		   0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 */
	(void)sscanf( buf, "%c %d %d %d %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f",
		&record.b.b_id,
		&temp1,
		&temp2,
		&temp3,
		&record.b.b_values[0],
		&record.b.b_values[1],
		&record.b.b_values[2],
		&record.b.b_values[3],
		&record.b.b_values[4],
		&record.b.b_values[5],
		&record.b.b_values[6],
		&record.b.b_values[7],
		&record.b.b_values[8],
		&record.b.b_values[9],
		&record.b.b_values[10],
		&record.b.b_values[11],
		&record.b.b_values[12],
		&record.b.b_values[13],
		&record.b.b_values[14],
		&record.b.b_values[15],
		&record.b.b_values[16],
		&record.b.b_values[17],
		&record.b.b_values[18],
		&record.b.b_values[19],
		&record.b.b_values[20],
		&record.b.b_values[21],
		&record.b.b_values[22],
		&record.b.b_values[23]
	);
	record.b.b_type = (char)temp1;
	record.b.b_n = (short)temp2;
	record.b.b_ngranule = (short)temp3;

	/* Write out the record */
	(void)fwrite( (char *)&record, sizeof record, 1, stdout );
}

zap_nl()
{
	register char *bp;

	bp = &buf[0];

	while( *bp != '\0' )  {
		if( *bp == '\n' )
			*bp = '\0';
		bp++;
	}
}

void
identbld()	/* Build Ident record */
{
	int temp1;

	(void)sscanf( buf, "%c %d %s",
		&record.i.i_id,
		&temp1,
		&record.i.i_version[0]
	);
	record.i.i_units = (char)temp1;

	(void)fgets( buf, BUFSIZE, stdin);
	zap_nl();
	(void)strcpy( &record.i.i_title[0], buf );

	/* Write out the record */
	(void)fwrite( (char *)&record, sizeof record, 1, stdout );
}

void
polyhbld()	/* Build Polyhead record */
{
	(void)sscanf( buf, "%c %s",
		&record.p.p_id,
		&record.p.p_name[0]
	);

	/* Write out the record */
	(void)fwrite( (char *)&record, sizeof record, 1, stdout );
}

void
polydbld()	/* Build Polydata record */
{
	int temp1;

#ifdef later
		/*		   0  1  2  3  4  5  6  7  8  9 10 11 12 13 14  0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 */
	(void)sscanf( buf, "%c %d %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f", 
		&record.q.q_id,
		&temp1,
		&record.q.q_verts[0][0],
		&record.q.q_verts[0][1],
		&record.q.q_verts[0][2],
		&record.q.q_verts[1][0],
		&record.q.q_verts[1][1],
		&record.q.q_verts[1][2],
		&record.q.q_verts[2][0],
		&record.q.q_verts[2][1],
		&record.q.q_verts[2][2],
		&record.q.q_verts[3][0],
		&record.q.q_verts[3][1],
		&record.q.q_verts[3][2],
		&record.q.q_verts[4][0],
		&record.q.q_verts[4][1],
		&record.q.q_verts[4][2],
		&record.q.q_norms[0][0],
		&record.q.q_norms[0][1],
		&record.q.q_norms[0][2],
		&record.q.q_norms[1][0],
		&record.q.q_norms[1][1],
		&record.q.q_norms[1][2],
		&record.q.q_norms[2][0],
		&record.q.q_norms[2][1],
		&record.q.q_norms[2][2],
		&record.q.q_norms[3][0],
		&record.q.q_norms[3][1],
		&record.q.q_norms[3][2],
		&record.q.q_norms[4][0],
		&record.q.q_norms[4][1],
		&record.q.q_norms[4][2]
	);
	record.q.q_count = (char)temp1;

	/* Write out the record */
	(void)fwrite( (char *)&record, sizeof record, 1, stdout );
#endif
}

void
materbld()
{
	int flags, low, hi, r, g, b;

	(void)sscanf( buf, "%c %d %d %d %d %d %d",
		&record.md.md_id,
		&flags, &low, &hi,
		&r, &g, &b
	);
	record.md.md_flags = (char)flags;
	record.md.md_low = (short)low;
	record.md.md_hi = (short)hi;
	record.md.md_r = (unsigned char)r;
	record.md.md_g = (unsigned char)g;
	record.md.md_b = (unsigned char)b;

	/* Write out the record */
	(void)fwrite( (char *)&record, sizeof record, 1, stdout );
}

void
bsplbld()	/* Build B-spline solid record */
{
	int temp1;

	(void)sscanf( buf, "%c %s %d %f",
		&record.B.B_id,
		&record.B.B_name[0],
		&temp1,
		&record.B.B_resolution
	);
	record.B.B_nsurf = (short)temp1;

	/* Write out the record */
	(void)fwrite( (char *)&record, sizeof record, 1, stdout );
}

void
bsurfbld()	/* Build d-spline surface description record */
{
	register int i;
	register float *vp;
	int nbytes, count;
	float *fp;
	int temp1, temp2, temp3, temp4, temp5, temp6, temp7, temp8, temp9;

	(void)sscanf( buf, "%c %d %d %d %d %d %d %d %d %d",
		&record.d.d_id,
		&temp1,
		&temp2,
		&temp3,
		&temp4,
		&temp5,
		&temp6,
		&temp7,
		&temp8,
		&temp9
	);
	record.d.d_order[0] = (short)temp1;
	record.d.d_order[1] = (short)temp2;
	record.d.d_kv_size[0] = (short)temp3;
	record.d.d_kv_size[1] = (short)temp4;
	record.d.d_ctl_size[0] = (short)temp5;
	record.d.d_ctl_size[1] = (short)temp6;
	record.d.d_geom_type = (short)temp7;
	record.d.d_nknots = (short)temp8;
	record.d.d_nctls = (short)temp9;

	/* Write out the record */
	(void)fwrite( (char *)&record, sizeof record, 1, stdout );

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
		(void)fprintf(stderr, "ASC2G: spline knot malloc error\n");
		exit(1);
	}
	fp = vp;
	(void)bzero( (char *)vp, nbytes );
	/* Read the knot vector information */
	count = record.d.d_kv_size[0] + record.d.d_kv_size[1];
	for( i = 0; i < count; i++ )  {
		fgets( buf, BUFSIZE, stdin );
		(void)sscanf( buf, "%f", vp++);
	}
	/* Write out the information */
	(void)fwrite( (char *)&fp, nbytes, 1, stdout );

	/* Free the knot data memory */
	(void)free( (char *)fp );

	/* Malloc and clear memory for the CONTROL MESH data and read it */
	nbytes = record.d.d_nctls * sizeof(union record);
	if( (vp = (float *)malloc(nbytes))  == (float *)0 )  {
		(void)fprintf(stderr, "ASC2G: control mesh malloc error\n");
		exit(1);
	}
	fp = vp;
	(void)bzero( (char *)vp, nbytes );
	/* Read the control mesh information */
	count = record.d.d_ctl_size[0] * record.d.d_ctl_size[1] *
		record.d.d_geom_type;
	for( i = 0; i < count; i++ )  {
		fgets( buf, BUFSIZE, stdin );
		(void)sscanf( buf, "%f", vp++);
	}
	/* Write out the information */
	(void)fwrite( (char *)&fp, nbytes, 1, stdout );

	/* Free the control mesh memory */
	(void)free( (char *)fp );
}

#ifndef BSD42

bzero( str, n )
register char *str;
register int n;
{
	while( n-- > 0 )
		*str++ = '\0';
}
#endif
