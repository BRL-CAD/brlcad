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

#include <stdio.h>
#include "db.h"

extern void	exit();
extern int	atoi();	/* bzero()? */
extern char	*strcpy();
extern double	atof();

void	identbld(), polyhbld(), polydbld();
void	solbld(), combbld(), membbld(), arsabld(), arsbbld();
void	materbld(), bsplbld(), bsurfbld(), zap_nl();
char	*nxt_spc();

static union record	record;		/* GED database record */
#define BUFSIZE		1024		/* Record input buffer size */
static char buf[BUFSIZE];		/* Record input buffer */

int debug;

main(argc, argv)
char **argv;
{
	if( argc > 1 )
		debug = 1;

	/* Read ASCII input file, each record on a line */
	while( ( fgets( buf, BUFSIZE, stdin ) ) != (char *)0 )  {

		/* Clear the output record -- vital! */
		(void)bzero( (char *)&record, sizeof(record) );

		/* Check record type */
		if( debug )
			(void)fprintf(stderr,"rec %c\n", buf[0] );
		switch( buf[0] )  {
		case ID_SOLID:
			solbld();
			continue;

		case ID_COMB:
			combbld();
			continue;

		case ID_MEMB:
			membbld();
			continue;

		case ID_ARS_A:
			arsabld();
			continue;

		case ID_ARS_B:
			arsbbld();
			continue;

		case ID_P_HEAD:
			polyhbld();
			continue;

		case ID_P_DATA:
			polydbld();
			continue;

		case ID_IDENT:
			identbld();
			continue;

		case ID_MATERIAL:
			materbld();
			continue;

		case ID_BSOLID:
			bsplbld();
			continue;

		case ID_BSURF:
			bsurfbld();
			continue;

		default:
			(void)fprintf(stderr,"asc2g: bad record type '%c' (0%o), skipping\n", buf[0], buf[0]);
			(void)fprintf(stderr,"%s\n", buf );
			continue;
		}
	}
	exit(0);
}

void
solbld()	/* Build Solid record */
{
	register char *cp;
	register char *np;
	register int i;

	cp = buf;
	record.s.s_id = *cp++;
	cp = nxt_spc( cp );		/* skip the space */

	record.s.s_type = (char)atoi( cp );
	cp = nxt_spc( cp );

	np = record.s.s_name;
	while( *cp != ' ' )  {
		*np++ = *cp++;
	}
	cp = nxt_spc( cp );

#if 0
	record.s.s_cgtype = (short)atoi( cp );
#else
	record.s.s_cgtype = 0;	/* no longer significant */
#endif

	for( i = 0; i < 24; i++ )  {
		cp = nxt_spc( cp );
		record.s.s_values[i] = atof( cp );
	}

	if( debug )  {
		(void)fprintf(stderr,"%s ty%d [0]=%f,%f,%f [3]=%e,%e,%e\n",
			record.s.s_name, record.s.s_type,
			record.s.s_values[0],
			record.s.s_values[1],
			record.s.s_values[2],
			record.s.s_values[3],
			record.s.s_values[4],
			record.s.s_values[5] );
	}
	/* Write out the record */
	(void)fwrite( (char *)&record, sizeof record, 1, stdout );
}

void
combbld()	/* Build Combination record */
{
	register char *cp;
	register char *np;
	int temp_nflag, temp_pflag;

	record.c.c_override = 0;
	temp_nflag = temp_pflag = 0;	/* indicators for optional fields */

	cp = buf;
	record.c.c_id = *cp++;
	cp = nxt_spc( cp );		/* skip the space */

	record.c.c_flags = *cp++;
	cp = nxt_spc( cp );

	np = record.c.c_name;
	while( *cp != ' ' )  {
		*np++ = *cp++;
	}
	cp = nxt_spc( cp );

	record.c.c_regionid = (short)atoi( cp );
	cp = nxt_spc( cp );
	record.c.c_aircode = (short)atoi( cp );
	cp = nxt_spc( cp );
	record.c.c_length = (short)atoi( cp );
	cp = nxt_spc( cp );
	record.c.c_num = (short)atoi( cp );
	cp = nxt_spc( cp );
	record.c.c_material = (short)atoi( cp );
	cp = nxt_spc( cp );
	record.c.c_los = (short)atoi( cp );
	cp = nxt_spc( cp );
	record.c.c_override = (char)atoi( cp );
	cp = nxt_spc( cp );

	record.c.c_rgb[0] = (unsigned char)atoi( cp );
	cp = nxt_spc( cp );
	record.c.c_rgb[1] = (unsigned char)atoi( cp );
	cp = nxt_spc( cp );
	record.c.c_rgb[2] = (unsigned char)atoi( cp );
	cp = nxt_spc( cp );

	temp_nflag = atoi( cp );
	cp = nxt_spc( cp );
	temp_pflag = atoi( cp );

	cp = nxt_spc( cp );
	record.c.c_inherit = atoi( cp );

	if( record.c.c_flags == 'Y' )
		record.c.c_flags = 'R';
	else
		record.c.c_flags = ' ';

	if( temp_nflag )  {
		fgets( buf, BUFSIZE, stdin );
		zap_nl();
		strncpy( record.c.c_matname, buf, sizeof(record.c.c_matname)-1 );
	}
	if( temp_pflag )  {
		fgets( buf, BUFSIZE, stdin );
		zap_nl();
		strncpy( record.c.c_matparm, buf, sizeof(record.c.c_matparm)-1 );
	}

	/* Write out the record */
	(void)fwrite( (char *)&record, sizeof record, 1, stdout );
}

void
membbld()	/* Build Member record */
{
	register char *cp;
	register char *np;
	register int i;

	cp = buf;
	record.M.m_id = *cp++;
	cp = nxt_spc( cp );		/* skip the space */

	record.M.m_relation = *cp++;
	cp = nxt_spc( cp );

	np = record.M.m_instname;
	while( *cp != ' ' )  {
		*np++ = *cp++;
	}
	cp = nxt_spc( cp );

	for( i = 0; i < 16; i++ )  {
		record.M.m_mat[i] = atof( cp );
		cp = nxt_spc( cp );
	}

	record.M.m_num = (short)atoi( cp );

	/* Write out the record */
	(void)fwrite( (char *)&record, sizeof record, 1, stdout );
}

void
arsabld()	/* Build ARS A record */
{
	register char *cp;
	register char *np;

	cp = buf;
	record.a.a_id = *cp++;
	cp = nxt_spc( cp );		/* skip the space */

	record.a.a_type = (char)atoi( cp );
	cp = nxt_spc( cp );

	np = record.a.a_name;
	while( *cp != ' ' )  {
		*np++ = *cp++;
	}
	cp = nxt_spc( cp );

	record.a.a_m = (short)atoi( cp );
	cp = nxt_spc( cp );
	record.a.a_n = (short)atoi( cp );
	cp = nxt_spc( cp );
	record.a.a_curlen = (short)atoi( cp );
	cp = nxt_spc( cp );
	record.a.a_totlen = (short)atoi( cp );
	cp = nxt_spc( cp );

	record.a.a_xmax = atof( cp );
	cp = nxt_spc( cp );
	record.a.a_xmin = atof( cp );
	cp = nxt_spc( cp );
	record.a.a_ymax = atof( cp );
	cp = nxt_spc( cp );
	record.a.a_ymin = atof( cp );
	cp = nxt_spc( cp );
	record.a.a_zmax = atof( cp );
	cp = nxt_spc( cp );
	record.a.a_zmin = atof( cp );

#ifdef never
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
#endif

	/* Write out the record */
	(void)fwrite( (char *)&record, sizeof record, 1, stdout );
}

void
arsbbld()	/* Build ARS B record */
{
	register char *cp;
	register int i;

	cp = buf;
	record.b.b_id = *cp++;
	cp = nxt_spc( cp );		/* skip the space */

	record.b.b_type = (char)atoi( cp );
	cp = nxt_spc( cp );
	record.b.b_n = (short)atoi( cp );
	cp = nxt_spc( cp );
	record.b.b_ngranule = (short)atoi( cp );

	for( i = 0; i < 24; i++ )  {
		cp = nxt_spc( cp );
		record.b.b_values[i] = atof( cp );
	}

#ifdef never
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
#endif

	/* Write out the record */
	(void)fwrite( (char *)&record, sizeof record, 1, stdout );
}

void
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
	register char *cp;
	register char *np;

	cp = buf;
	record.i.i_id = *cp++;
	cp = nxt_spc( cp );		/* skip the space */

	record.i.i_units = (char)atoi( cp );
	cp = nxt_spc( cp );

	np = record.i.i_version;
	while( *cp != '\n' && *cp != '\0' )  {
		*np++ = *cp++;
	}

	(void)fgets( buf, BUFSIZE, stdin);
	zap_nl();
	(void)strcpy( &record.i.i_title[0], buf );

	/* Write out the record */
	(void)fwrite( (char *)&record, sizeof record, 1, stdout );
}

void
polyhbld()	/* Build Polyhead record */
{
	register char *cp;
	register char *np;

	cp = buf;
	record.p.p_id = *cp++;
	cp = nxt_spc( cp );		/* skip the space */

	np = record.p.p_name;
	while( *cp != '\n' && *cp != '\0' )  {
		*np++ = *cp++;
	}

	/* Write out the record */
	(void)fwrite( (char *)&record, sizeof record, 1, stdout );
}

void
polydbld()	/* Build Polydata record */
{
	register char *cp;
	register int i, j;

	cp = buf;
	record.q.q_id = *cp++;
	cp = nxt_spc( cp );		/* skip the space */

	record.q.q_count = (char)atoi( cp );

	for( i = 0; i < 5; i++ )  {
		for( j = 0; j < 3; j++ )  {
			cp = nxt_spc( cp );
			record.q.q_verts[i][j] = atof( cp );
		}
	}

	for( i = 0; i < 5; i++ )  {
		for( j = 0; j < 3; j++ )  {
			cp = nxt_spc( cp );
			record.q.q_norms[i][j] = atof( cp );
		}
	}

#ifdef never
	int temp1;

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
#endif

	/* Write out the record */
	(void)fwrite( (char *)&record, sizeof record, 1, stdout );
}

void
materbld()
{
	register char *cp;

	cp = buf;
	record.md.md_id = *cp++;
	cp = nxt_spc( cp );		/* skip the space */

	record.md.md_flags = (char)atoi( cp );
	cp = nxt_spc( cp );
	record.md.md_low = (short)atoi( cp );
	cp = nxt_spc( cp );
	record.md.md_hi = (short)atoi( cp );
	cp = nxt_spc( cp );
	record.md.md_r = (unsigned char)atoi( cp);
	cp = nxt_spc( cp );
	record.md.md_g = (unsigned char)atoi( cp);
	cp = nxt_spc( cp );
	record.md.md_b = (unsigned char)atoi( cp);

	/* Write out the record */
	(void)fwrite( (char *)&record, sizeof record, 1, stdout );
}

void
bsplbld()	/* Build B-spline solid record */
{
	register char *cp;
	register char *np;

	cp = buf;
	record.B.B_id = *cp++;
	cp = nxt_spc( cp );		/* skip the space */

	np = record.B.B_name;
	while( *cp != ' ' )  {
		*np++ = *cp++;
	}
	cp = nxt_spc( cp );

	record.B.B_nsurf = (short)atoi( cp );
	cp = nxt_spc( cp );
	record.B.B_resolution = atof( cp );

#ifdef never
	(void)sscanf( buf, "%c %s %d %f",
		&record.B.B_id,
		&record.B.B_name[0],
		&temp1,
		&record.B.B_resolution
	);
	record.B.B_nsurf = (short)temp1;
#endif

	/* Write out the record */
	(void)fwrite( (char *)&record, sizeof record, 1, stdout );
}

void
bsurfbld()	/* Build d-spline surface description record */
{
	register char *cp;
	register int i;
	register float *vp;
	int nbytes, count;
	float *fp;

	cp = buf;
	record.d.d_id = *cp++;
	cp = nxt_spc( cp );		/* skip the space */

	record.d.d_order[0] = (short)atoi( cp );
	cp = nxt_spc( cp );
	record.d.d_order[1] = (short)atoi( cp );
	cp = nxt_spc( cp );
	record.d.d_kv_size[0] = (short)atoi( cp );
	cp = nxt_spc( cp );
	record.d.d_kv_size[1] = (short)atoi( cp );
	cp = nxt_spc( cp );
	record.d.d_ctl_size[0] = (short)atoi( cp );
	cp = nxt_spc( cp );
	record.d.d_ctl_size[1] = (short)atoi( cp );
	cp = nxt_spc( cp );
	record.d.d_geom_type = (short)atoi( cp );
	cp = nxt_spc( cp );
	record.d.d_nknots = (short)atoi( cp );
	cp = nxt_spc( cp );
	record.d.d_nctls = (short)atoi( cp );

	record.d.d_nknots = 
		ngran( record.d.d_kv_size[0] + record.d.d_kv_size[1] );

	record.d.d_nctls = 
		ngran( record.d.d_ctl_size[0] * record.d.d_ctl_size[1] 
			* record.d.d_geom_type);

#ifdef never
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
#endif

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
		(void)fprintf(stderr, "asc2g: spline knot malloc error\n");
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
	(void)fwrite( (char *)fp, nbytes, 1, stdout );

	/* Free the knot data memory */
	(void)free( (char *)fp );

	/* Malloc and clear memory for the CONTROL MESH data and read it */
	nbytes = record.d.d_nctls * sizeof(union record);
	if( (vp = (float *)malloc(nbytes))  == (float *)0 )  {
		(void)fprintf(stderr, "asc2g: control mesh malloc error\n");
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
	(void)fwrite( (char *)fp, nbytes, 1, stdout );

	/* Free the control mesh memory */
	(void)free( (char *)fp );
}

char *
nxt_spc( cp)
register char *cp;
{
	while( *cp != ' ' && *cp != '\t' && *cp !='\0' )  {
		cp++;
	}
	if( *cp != '\0' )  {
		cp++;
	}
	return( cp );
}

ngran( nfloat )
{
	register int gran;
	/* Round up */
	gran = nfloat + ((sizeof(union record)-1) / sizeof(float) );
	gran = (gran * sizeof(float)) / sizeof(union record);
	return(gran);
}

#ifdef SYSV

bzero( str, n )
register char *str;
register int n;
{
	while( n-- > 0 )
		*str++ = '\0';
}
#endif
