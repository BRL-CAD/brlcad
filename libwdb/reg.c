/*
 *			R E G
 *
 *  Library for writing MGED databases from arbitrary procedures.
 *
 *  This module contains routines to create combinations, and regions.
 *
 *  It is expected that this library will grow as experience is gained.
 *
 *  Authors -
 *	Michael John Muuss
 *	Paul R. Stay
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1987 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "db.h"
#include "vmath.h"
#include "wdb.h"

#ifdef SYSV
#define bzero(str,n)		memset( str, '\0', n )
#define bcopy(from,to,count)	memcpy( to, from, count )
#endif

/*
 *			M K _ M C O M B
 *
 *  Make a combination with material properties info
 */
int
mk_mcomb( fp, name, len, region, matname, matparm, override, rgb )
FILE	*fp;
char	*name;
char	*matname;
char	*matparm;
char	*rgb;
{
	static union record rec;

	bzero( (char *)&rec, sizeof(rec) );
	rec.c.c_id = ID_COMB;
	if( region )
		rec.c.c_flags = 'R';
	else
		rec.c.c_flags = ' ';
	NAMEMOVE( name, rec.c.c_name );
	rec.c.c_length = len;
	if( matname ) {
		strncpy( rec.c.c_matname, matname, sizeof(rec.c.c_matname) );
		if( matparm )
			strncpy( rec.c.c_matparm, matparm,
				sizeof(rec.c.c_matparm) );
	}
	if( override )  {
		rec.c.c_override = 1;
		rec.c.c_rgb[0] = rgb[0];
		rec.c.c_rgb[1] = rgb[1];
		rec.c.c_rgb[2] = rgb[2];
	}
	fwrite( (char *)&rec, sizeof(rec), 1, fp );
	return(0);
}


/*
 *			M K _ C O M B
 *
 *  Make a simple combination header.
 * Must be followed by 'len' mk_memb() calls before any other mk_* routines
 */
int
mk_comb( fp, name, len, region )
FILE	*fp;
char	*name;
{
	static union record rec;

	bzero( (char *)&rec, sizeof(rec) );
	rec.c.c_id = ID_COMB;
	if( region )
		rec.c.c_flags = 'R';
	else
		rec.c.c_flags = ' ';
	NAMEMOVE( name, rec.c.c_name );
	rec.c.c_length = len;
	fwrite( (char *)&rec, sizeof(rec), 1, fp );
	return(0);
}

/*
 *			M K _ M E M B
 *
 *  Must be part of combination/member clump of records.
 */
int
mk_memb( fp, name, mat, op )
FILE	*fp;
char	*name;
mat_t	mat;
int	op;
{
	static union record rec;
	register int i;

	bzero( (char *)&rec, sizeof(rec) );
	rec.M.m_id = ID_MEMB;
	rec.M.m_relation = op;
	NAMEMOVE( name, rec.M.m_instname );
	for( i=0; i<16; i++ )
		rec.M.m_mat[i] = mat[i];
	fwrite( (char *)&rec, sizeof(rec), 1, fp );
	return(0);
}

/*
 *			M K _ A D D M E M B E R
 *
 *  Obtain dynamic storage for a new wmember structure, fill in the
 *  name, default the operation and matrix, and add to doubly linked
 *  list.  In typical use, a one-line call is sufficient.  To change
 *  the defaults, catch the pointer that is returned, and adjust the
 *  structure to taste.
 *
 *  The caller is responsible for initializing the header structures
 *  forward and backward links.
 */
struct wmember *
mk_addmember( name, headp )
char	*name;
register struct wmember *headp;
{
	register struct wmember *wp;

	if( (wp = (struct wmember *)malloc(sizeof(struct wmember))) == WMEMBER_NULL )  {
		fprintf(stderr,"mk_wmember:  malloc failure\n");
		return(WMEMBER_NULL);
	}
	wp->wm_magic = WMEMBER_MAGIC;
	strncpy( wp->wm_name, name, sizeof(wp->wm_name) );
	wp->wm_op = UNION;
	mat_idn(wp->wm_mat);
	/* Append to end of doubly linked list */
	wp->wm_forw = headp;
	wp->wm_back = headp->wm_back;
	headp->wm_back->wm_forw = wp;
	headp->wm_back = wp;
	return(wp);
}

/*
 *			M K _ L C O M B
 *
 *  Make a combination, much like mk_comb(), but where the
 *  members are described by a linked list of wmember structs.
 *  This routine produces the combination and member records
 *  all at once, making it easier and less risky to use than
 *  direct use of the pair of mk_comb() and mk_memb().
 *  The linked list is freed when it has been output.
 *
 *  A shorthand version is given in wdb.h as a macro.
 */
int
mk_lcomb( fp, name, region, matname, matparm, override, rgb, headp )
FILE	*fp;
char	*name;
char	*matname;
char	*matparm;
char	*rgb;
register struct wmember *headp;
{
	register struct wmember *wp;
	register int len = 0;

	/* Measure length of list */
	for( wp = headp->wm_forw; wp != headp; wp = wp->wm_forw )  {
		if( wp->wm_magic != WMEMBER_MAGIC )  {
			fprintf(stderr, "mk_wmcomb:  corrupted linked list\n");
			abort();
		}
		len++;
	}

	/* Output combination record and member records */
	mk_mcomb( fp, name, len, region, matname, matparm, override, rgb );
	for( wp = headp->wm_forw; wp != headp; wp = wp->wm_forw )
		mk_memb( fp, wp->wm_name, wp->wm_mat, wp->wm_op );

	/* Release the member structure dynamic storage */
	for( wp = headp->wm_forw; wp != headp; )  {
		register struct wmember *next;

		wp->wm_magic = -1;	/* Sanity */
		next = wp->wm_forw;
		free( (char *)wp );
		wp = next;
	}
	headp->wm_forw = headp->wm_back = headp;
	return(0);
}
