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
extern int	close(), creat(), open(), read(), write();
extern int	printf(), fprintf(), sscanf();		/* bzero()? */
extern long	lseek();
extern char	*strcpy();

void	identbld(), polyhbld(), polydbld();
void	solbld(), combbld(), membbld(), arsabld(), arsbbld();
void	materbld();

static union record	record;		/* GED database record */
int	count;				/* Number of records */
char	*bp;				/* Pointer for input buffer */
#define BUFSIZE		1000		/* Record input buffer size */
static char buf[BUFSIZE];		/* Record input buffer */

main()
{
	count = 0;

	/* Read ASCII input file, each record on a line */
	while( ( fgets( buf, BUFSIZE, stdin ) ) != (char *)0 )  {
		count++;
		bp = &buf[0];

		/* Clear the output record */
		(void)bzero( (char *)&record, sizeof(record) );

		/* Check record type */
		if( buf[0] == ID_SOLID )  {
			/* Build the record */
			solbld();
			/* Write out the record */
			(void)write( 1, (char *)&record, sizeof record );
		}
		else if( buf[0] == ID_COMB )  {
			/* Build the record */
			combbld();
			/* Write out the record */
			(void)write( 1, (char *)&record, sizeof record );
		}
		else if( buf[0] == ID_MEMB )  {
			/* Build the record */
			membbld();
			/* Write out the record */
			(void)write( 1, (char *)&record, sizeof record );
		}
		else if( buf[0] == ID_ARS_A )  {
			/* Build the record */
			arsabld();
			/* Write out the record */
			(void)write( 1, (char *)&record, sizeof record );
		}
		else if( buf[0] == ID_ARS_B )  {
			/* Build the record */
			arsbbld();
			/* Write out the record */
			(void)write( 1, (char *)&record, sizeof record );
		}
		else if( buf[0] == ID_P_HEAD )  {
			/* Build the record */
			polyhbld();
			/* Write out the record */
			(void)write( 1, (char *)&record, sizeof record );
		}
		else if( buf[0] == ID_P_DATA )  {
			/* Build the record */
			polydbld();
			/* Write out the record */
			(void)write( 1, (char *)&record, sizeof record );
		}
		else if( buf[0] == ID_IDENT )  {
			/* Build the record */
			identbld();
			/* Write out the record */
			(void)write( 1, (char *)&record, sizeof record );
		}
		else if( buf[0] == ID_MATERIAL )  {
			materbld();
			(void)write( 1, (char *)&record, sizeof record );
		}
		else  {
			(void)fprintf(stderr,"ASC2VG: bad record type\n");
			exit(1);
		}
	}
	return(0);
}

void
solbld()	/* Build Solid record */
{
	int temp1, temp2;

		/*		   0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 */
	(void)sscanf( buf, "%c %d %s %d %e %e %e %e %e %e %e %e %e %e %e %e %e %e %e %e %e %e %e %e %e %e %e %e",
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
	record.s.s_type = (char)temp1;
	record.s.s_cgtype = (short)temp2;
}

void
combbld()	/* Build Combination record */
{
	int temp1, temp2, temp3, temp4, temp5, temp6;

	(void)sscanf( buf, "%c %c %s %d %d %d %d %d %d",
		&record.c.c_id,
		&record.c.c_flags,
		&record.c.c_name[0],
		&temp1,
		&temp2,
		&temp3,
		&temp4,
		&temp5,
		&temp6
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
}

void
membbld()	/* Build Member record */
{
	int temp1;

		/*		      0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 */
	(void)sscanf( buf, "%c %c %s %e %e %e %e %e %e %e %e %e %e %e %e %e %e %e %e %d", 
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
}

void
arsabld()	/* Build ARS A record */
{
	int temp1, temp2, temp3, temp4, temp5;

	(void)sscanf( buf, "%c %d %s %d %d %d %d %e %e %e %e %e %e",
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
}

void
arsbbld()	/* Build ARS B record */
{
	int temp1, temp2, temp3;

		/*		   0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 */
	(void)sscanf( buf, "%c %d %d %d %e %e %e %e %e %e %e %e %e %e %e %e %e %e %e %e %e %e %e %e %e %e %e %e",
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
	bp = &buf[0];

	while( *bp != '\0' )  {
		if( *bp == '\n' )
			*bp = '\0';
		bp++;
	}
	(void)strcpy( &record.i.i_title[0], buf );
}

void
polyhbld()	/* Build Polyhead record */
{
	(void)sscanf( buf, "%c %s",
		&record.p.p_id,
		&record.p.p_name[0]
	);
}

void
polydbld()	/* Build Polydata record */
{
	int temp1;

#ifdef later
		/*		   0  1  2  3  4  5  6  7  8  9 10 11 12 13 14  0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 */
	(void)sscanf( buf, "%c %d %e %e %e %e %e %e %e %e %e %e %e %e %e %e %e %e %e %e %e %e %e %e %e %e %e %e %e %e %e %e", 
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
}

void
materbld()
{
	int flags, low, hi, r, g, b;

	(void)sscanf( buf, "%c %d %d %d %d %d %d %s",
		&record.md.md_id,
		&flags, &low, &hi,
		&r, &g, &b,
		record.md.md_material );
	record.md.md_flags = flags;
	record.md.md_low = low;
	record.md.md_hi = hi;
	record.md.md_r = r;
	record.md.md_g = g;
	record.md.md_b = b;
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
