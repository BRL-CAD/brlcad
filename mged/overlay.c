/*
 *			O V E R L A Y . C
 *
 * Functions -
 *	f_overlay		Read a UNIX-Plot file as an overlay
 *	uplot_vlist		Read UNIX-Plot, create list of vectors
 *	getshort		Read VAX-order 16-bit number
 *
 *  Author -
 *	Michael John Muuss
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1988 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <math.h>
#include <signal.h>
#include <stdio.h>
#include "machine.h"
#include "vmath.h"
#include "db.h"
#include "mater.h"

#include "./sedit.h"
#include "raytrace.h"
#include "./ged.h"
#include "externs.h"
#include "./solid.h"
#include "./dm.h"

#define	TBAD	0	/* no such command */
#define TNONE	1	/* no arguments */
#define TSHORT	2	/* Vax 16-bit short */
#define	TIEEE	3	/* IEEE 64-bit floating */
#define	TCHAR	4	/* unsigned chars */
#define	TSTRING	5	/* linefeed terminated string */

struct uplot {
	int	targ;	/* type of args */
	int	narg;	/* number or args */
	char	*desc;	/* description */
};
struct uplot uplot_error = { 0, 0, 0 };
struct uplot uplot_letters[] = {
/*A*/	{ 0, 0, 0 },
/*B*/	{ 0, 0, 0 },
/*C*/	{ TCHAR, 3, "color" },
/*D*/	{ 0, 0, 0 },
/*E*/	{ 0, 0, 0 },
/*F*/	{ TNONE, 0, "flush" },
/*G*/	{ 0, 0, 0 },
/*H*/	{ 0, 0, 0 },
/*I*/	{ 0, 0, 0 },
/*J*/	{ 0, 0, 0 },
/*K*/	{ 0, 0, 0 },
/*L*/	{ TSHORT, 6, "3line" },
/*M*/	{ TSHORT, 3, "3move" },
/*N*/	{ TSHORT, 3, "3cont" },
/*O*/	{ TIEEE, 3, "d_3move" },
/*P*/	{ TSHORT, 3, "3point" },
/*Q*/	{ TIEEE, 3, "d_3cont" },
/*R*/	{ 0, 0, 0 },
/*S*/	{ TSHORT, 6, "3space" },
/*T*/	{ 0, 0, 0 },
/*U*/	{ 0, 0, 0 },
/*V*/	{ TIEEE, 6, "d_3line" },
/*W*/	{ TIEEE, 6, "d_3space" },
/*X*/	{ TIEEE, 3, "d_3point" },
/*Y*/	{ 0, 0, 0 },
/*Z*/	{ 0, 0, 0 },
/*[*/	{ 0, 0, 0 },
/*\*/	{ 0, 0, 0 },
/*]*/	{ 0, 0, 0 },
/*^*/	{ 0, 0, 0 },
/*_*/	{ 0, 0, 0 },
/*`*/	{ 0, 0, 0 },
/*a*/	{ TSHORT, 6, "arc" },
/*b*/	{ 0, 0, 0 },
/*c*/	{ TSHORT, 3, "circle" },
/*d*/	{ 0, 0, 0 },
/*e*/	{ TNONE, 0, "erase" },
/*f*/	{ TSTRING, 1, "linmod" },
/*g*/	{ 0, 0, 0 },
/*h*/	{ 0, 0, 0 },
/*i*/	{ TIEEE, 3, "d_circle" },
/*j*/	{ 0, 0, 0 },
/*k*/	{ 0, 0, 0 },
/*l*/	{ TSHORT, 4, "line" },
/*m*/	{ TSHORT, 2, "move" },
/*n*/	{ TSHORT, 2, "cont" },
/*o*/	{ TIEEE, 2, "d_move" },
/*p*/	{ TSHORT, 2, "point" },
/*q*/	{ TIEEE, 2, "d_cont" },
/*r*/	{ TIEEE, 6, "d_arc" },
/*s*/	{ TSHORT, 4, "space" },
/*t*/	{ TSTRING, 1, "label" },
/*u*/	{ 0, 0, 0 },
/*v*/	{ TIEEE, 4, "d_line" },
/*w*/	{ TIEEE, 4, "d_space" },
/*x*/	{ TIEEE, 2, "d_point" },
/*y*/	{ 0, 0, 0 },
/*z*/	{ 0, 0, 0 }
};

static int	getshort();

/* Usage:  overlay file.plot [name] */
void
f_overlay( argc, argv )
int	argc;
char	**argv;
{
	char		*name;
	FILE		*fp;
	int		ret;
	struct rt_vlblock	*vbp;

	if( argc <= 2 )
		name = "_PLOT_OVERLAY_";
	else
		name = argv[2];

	if( (fp = fopen(argv[1], "r")) == NULL )  {
		perror(argv[1]);
		return;
	}

	vbp = rt_vlblock_init();
	ret = uplot_vlist( vbp, fp );
	fclose(fp);
	if( ret < 0 )  {
		rt_vlblock_free(vbp);
		return;
	}

	cvt_vlblock_to_solids( vbp, name, 0 );

	rt_vlblock_free(vbp);
	dmaflag = 1;
}

void
label_vlist_verts( vbp, src, mat, sz )
struct rt_vlblock	*vbp;
struct rt_list		*src;
mat_t			mat;
double			sz;
{
	struct rt_vlist	*vp;
	struct rt_list	*vhead;
	char		label[256];
	fastf_t		scale;

	vhead = rt_vlblock_find( vbp, 255, 255, 255 );	/* white */

	for( RT_LIST_FOR( vp, rt_vlist, src ) )  {
		register int	i;
		register int	nused = vp->nused;
		register int	*cmd = vp->cmd;
		register point_t *pt = vp->pt;
		for( i = 0; i < nused; i++,cmd++,pt++ )  {
			sprintf( label, " %g, %g, %g",
				V3ARGS(*pt) );
			rt_vlist_3string( vhead, label, pt, mat, sz );
		}
	}
}

/* Usage:  labelvert solid(s) */
void
f_labelvert( argc, argv )
int	argc;
char	**argv;
{
	int	i;
	struct rt_vlblock	*vbp;
	struct directory	*dp;
	mat_t			mat;
	fastf_t			scale;

	vbp = rt_vlblock_init();
	mat_idn(mat);
	mat_inv( mat, Viewrot );
	scale = VIEWSIZE / 100;		/* divide by # chars/screen */

	for( i=1; i<argc; i++ )  {
		struct solid	*s;
		if( (dp = db_lookup( dbip, argv[i], LOOKUP_NOISY )) == DIR_NULL )
			continue;
		/* Find uses of this solid in the solid table */
		FOR_ALL_SOLIDS(s)  {
			int	j;
			for( j = s->s_last; j >= 0; j-- )  {
				if( s->s_path[j] == dp )  {
					label_vlist_verts( vbp, &s->s_vlist, mat, scale );
					break;
				}
			}
		}
	}

	cvt_vlblock_to_solids( vbp, "_LABELVERT_", 0 );

	rt_vlblock_free(vbp);
	dmaflag = 1;
}

/*
 *			U P L O T _ V L I S T
 *
 *  Read a BRL-style 3-D UNIX-plot file into a vector list.
 *  For now, discard color information, only extract vectors.
 *  This might be more naturally located in mged/plot.c
 */
int
uplot_vlist( vbp, fp )
struct rt_vlblock	*vbp;
register FILE		*fp;
{
	register struct rt_list	*vhead;
	register int	c;
	mat_t	mat;
	struct	uplot *up;
	char	carg[256];
	fastf_t	arg[6];
	char	inbuf[8];
	vect_t	a,b;
	point_t	last_pos;
	int	cc;
	int	i;
	int	j;

	vhead = rt_vlblock_find( vbp, 0xFF, 0xFF, 0x00 );	/* Yellow */

	while( (c = getc(fp)) != EOF ) {
		/* look it up */
		if( c < 'A' || c > 'z' ) {
			up = &uplot_error;
		} else {
			up = &uplot_letters[ c - 'A' ];
		}

		if( up->targ == TBAD ) {
			fprintf( stderr, "Bad command '%c' (0x%02x)\n", c, c );
			return(-1);
		}

		if( up->narg > 0 )  {
			for( i = 0; i < up->narg; i++ ) {
			switch( up->targ ){
				case TSHORT:
					arg[i] = getshort(fp);
					break;
				case TIEEE:
					fread( inbuf, 8, 1, fp );
					ntohd( &arg[i], inbuf, 1 );
					break;
				case TSTRING:
					j = 0;
					while( (cc = getc(fp)) != '\n'
					    && cc != EOF )
						carg[j++] = cc;
					carg[j] = '\0';
					break;
				case TCHAR:
					carg[i] = getc(fp);
					arg[i] = 0;
					break;
				case TNONE:
				default:
					arg[i] = 0;	/* ? */
					break;
				}
			}
		}

		switch( c ) {
		case 's':
		case 'w':
		case 'S':
		case 'W':
			/* Space commands, do nothing. */
			break;
		case 'm':
		case 'o':
			/* 2-D move */
			arg[Z] = 0;
			RT_ADD_VLIST( vhead, arg, RT_VLIST_LINE_MOVE );
			break;
		case 'M':
		case 'O':
			/* 3-D move */
			RT_ADD_VLIST( vhead, arg, RT_VLIST_LINE_MOVE );
			break;
		case 'n':
		case 'q':
			/* 2-D draw */
			arg[Z] = 0;
			RT_ADD_VLIST( vhead, arg, RT_VLIST_LINE_DRAW );
			break;
		case 'N':
		case 'Q':
			/* 3-D draw */
			RT_ADD_VLIST( vhead, arg, RT_VLIST_LINE_DRAW );
			break;
		case 'l':
		case 'v':
			/* 2-D line */
			VSET( a, arg[0], arg[1], 0.0 );
			VSET( b, arg[2], arg[3], 0.0 );
			RT_ADD_VLIST( vhead, a, RT_VLIST_LINE_MOVE );
			RT_ADD_VLIST( vhead, b, RT_VLIST_LINE_DRAW );
			break;
		case 'L':
		case 'V':
			/* 3-D line */
			VSET( a, arg[0], arg[1], arg[2] );
			VSET( b, arg[3], arg[4], arg[5] );
			RT_ADD_VLIST( vhead, a, RT_VLIST_LINE_MOVE );
			RT_ADD_VLIST( vhead, b, RT_VLIST_LINE_DRAW );
			break;
		case 'p':
		case 'x':
			/* 2-D point */
			arg[Z] = 0;
			RT_ADD_VLIST( vhead, arg, RT_VLIST_LINE_MOVE );
			RT_ADD_VLIST( vhead, arg, RT_VLIST_LINE_DRAW );
			break;
		case 'P':
		case 'X':
			/* 3-D point */
			RT_ADD_VLIST( vhead, arg, RT_VLIST_LINE_MOVE );
			RT_ADD_VLIST( vhead, arg, RT_VLIST_LINE_DRAW );
			break;
		case 'C':
			/* Color */
			vhead = rt_vlblock_find( vbp,
				carg[0], carg[1], carg[2] );
			break;
		case 't':
			/* Text string */
			mat_idn(mat);
			if( RT_LIST_NON_EMPTY( vhead ) )  {
				struct rt_vlist *vlp;
				/* Use coordinates of last op */
				vlp = RT_LIST_LAST( rt_vlist, vhead );
				VMOVE( last_pos, vlp->pt[vlp->nused-1] );
			} else {
				VSETALL( last_pos, 0 );
			}
			rt_vlist_3string( vhead, carg, last_pos, mat, Viewscale * 0.01 );
			break;
		}
	}
	return(0);
}

static int
getshort(fp)
FILE	*fp;
{
	register long	v, w;

	v = getc(fp);
	v |= (getc(fp)<<8);	/* order is important! */

	/* worry about sign extension - sigh */
	if( v <= 0x7FFF )  return(v);
	w = -1;
	w &= ~0x7FFF;
	return( w | v );
}
