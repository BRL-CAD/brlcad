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

/*
 *  Character stroke table, taken from libtig/symbol.c
 *
 *	Terminal Independant Graphics Display Package
 *		Mike Muuss  July 31, 1978
 *
 *	This routine is used to plot a string of ASCII symbols
 *  on the plot being generated, using a built-in set of fonts
 *  drawn as vector lists.
 * 
 *	Internally, the basic font resides in a 10x10 unit square.
 *  Externally, each character can be thought to occupy one square
 *  plotting unit;  the 'scale'
 *  parameter allows this to be changed as desired, although scale
 *  factors less than 10.0 are unlikely to be legible.
 *
 */

/*
 *	Motion encoding macros
 *
 * All characters reference absolute points within a 10 x 10 square
 */
#define	brt(x,y)	(11*x+y)
#define drk(x,y)	-(11*x+y)
#define	LAST		-128		/* 0200 Marks end of stroke list */
#define	NEGY		-127		/* 0201 Denotes negative y stroke */
#define bneg(x,y)	NEGY, brt(x,y)
#define dneg(x,y)	NEGY, drk(x,y)

#if defined(CRAY1) || defined(CRAY2) || defined(mips)
#define TINY	int
#else
#define TINY	char		/* must be signed */
#endif

static TINY	*tp_cindex[256];	/* index to stroke tokens */
extern TINY	tp_ctable[];	/* table of strokes */

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
extern void	vlist_3symbol();

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
			vlist_3symbol( vhead, label, pt, mat, sz );
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
			if( s->s_path[s->s_last] != dp )  continue;
			label_vlist_verts( vbp, &s->s_vlist, mat, scale );
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
			vlist_3symbol( vhead, carg, last_pos, mat, Viewscale * 0.01 );
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

/*
 *  Once-only setup routine
 */
static void
tp_setup()
{
	register TINY	*p;	/* pointer to stroke table */
	register int i;

	p = tp_ctable;		/* pointer to stroke list */

	/* Store start addrs of each stroke list */
	for( i=040-5; i<128; i++)  {
		tp_cindex[i+128] = tp_cindex[i] = p;
		while( (*p++) != LAST );
	}
	for( i=6; i<040; i++ )  {
		tp_cindex[i+128] = tp_cindex[i] = tp_cindex['?'];
	}
	for( i=1; i<6; i++ )  {
		tp_cindex[i+128] = tp_cindex[i] = tp_cindex[040-1+i];
	}
}

/*
 *			V L I S T _ 3 S Y M B O L
 *
 *  'scale' is the width, in mm, of one character.
 */
void
vlist_3symbol( vhead, string, origin, rot, scale )
register struct rt_list	*vhead;
char	*string;		/* string of chars to be plotted */
point_t	origin;			/* lower left corner of 1st char */
mat_t	rot;			/* Transform matrix (WARNING: may xlate) */
double	scale;			/* scale factor to change 1x1 char sz */
{
	register unsigned char *cp;
	double	offset;			/* offset of char from given x,y */
	int	ysign;			/* sign of y motion, either +1 or -1 */
	vect_t	temp;
	vect_t	loc;
	mat_t	xlate_to_origin;
	mat_t	mtemp;
	mat_t	mat;

	if( string == NULL || *string == '\0' )
		return;			/* done before begun! */

	/*
	 *  The point "origin" will be the center of the axis rotation.
	 *  The text is located in a local coordinate system with the
	 *  lower left corner of the first character at (0,0,0), with
	 *  the text proceeding onward towards +X.
	 *  We need to rotate the text around it's local (0,0,0),
	 *  and then translate to the user's designated "origin".
	 *  If the user provided translation or
	 *  scaling in his matrix, it will *also* be applied.
	 */
	mat_idn( xlate_to_origin );
	MAT_DELTAS( xlate_to_origin,	origin[X], origin[Y], origin[Z] );
	mat_mul( mat, xlate_to_origin, rot );

	/* Check to see if initialization is needed */
	if( tp_cindex[040] == 0 )  tp_setup();

	/* Draw each character in the input string */
	offset = 0;
	for( cp = (unsigned char *)string ; *cp; cp++, offset += scale )  {
		register TINY	*p;	/* pointer to stroke table */
		register int	stroke;

		VSET( temp, offset, 0, 0 );
		MAT4X3PNT( loc, mat, temp );
		RT_ADD_VLIST( vhead, loc, RT_VLIST_LINE_MOVE );		/* move */

		for( p = tp_cindex[*cp]; (stroke= *p) != LAST; p++ )  {
			int	draw;

			if( stroke==NEGY )  {
				ysign = (-1);
				stroke = *++p;
			} else
				ysign = 1;

			/* Detect & process pen control */
			if( stroke < 0 )  {
				stroke = -stroke;
				draw = 0;
			} else
				draw = 1;

			/* stroke co-ordinates in string coord system */
			VSET( temp, (stroke/11) * 0.1 * scale + offset,
				   (ysign * (stroke%11)) * 0.1 * scale, 0 );
			MAT4X3PNT( loc, mat, temp );
			if( draw )  {
				RT_ADD_VLIST( vhead, loc, RT_VLIST_LINE_DRAW );
			} else {
				RT_ADD_VLIST( vhead, loc, RT_VLIST_LINE_MOVE );
			}
		}
	}
}


/*	tables for markers	*/

TINY	tp_ctable[] = {

/*	+	*/
	drk(0, 5),
	brt(8, 5),
	drk(4, 8),
	brt(4, 2),
	LAST,

/*	x	*/
	drk(0, 2),
	brt(8, 8),
	drk(0, 8),
	brt(8, 2),
	LAST,

/*	triangle	*/
	drk(0, 2),
	brt(4, 8),
	brt(8, 2),
	brt(0, 2),
	LAST,

/*	square	*/
	drk(0, 2),
	brt(0, 8),
	brt(8, 8),
	brt(8, 2),
	brt(0, 2),
	LAST,

/*	hourglass	*/
	drk(0, 2),
	brt(8, 8),
	brt(0, 8),
	brt(8, 2),
	brt(0, 2),
	LAST,

/*	table for ascii 040, ' '	*/
	LAST,

/*	table for !	*/
	drk(3, 0),
	brt(5, 2),
	brt(5, 0),
	brt(3, 2),
	brt(3, 0),
	drk(4, 4),
	brt(3, 10),
	brt(5, 10),
	brt(4, 4),
	brt(4, 10),
	LAST,

/*	table for "	*/
	drk(1, 10),
	brt(3, 10),
	brt(2, 7),
	brt(1, 10 ),
	drk(5, 10),
	brt(7, 10),
	brt(6, 7),
	brt(5, 10),
	LAST,

/*	table for #	*/
	drk(1, 0),
	brt(3, 9),
	drk(6, 9),
	brt(4, 0),
	drk(6, 3),
	brt(0, 3),
	drk(1, 6),
	brt(7, 6),
	LAST,

/*	table for $	*/
	drk(1, 2),
	brt(1, 1),
	brt(7, 1),
	brt(7, 5),
	brt(1, 5),
	brt(1, 9),
	brt(7, 9),
	brt(7, 8),
	drk(4, 10),
	brt(4, 0),
	LAST,

/*	table for %	*/
	drk(3, 10),
	brt(3, 7),
	brt(0, 7),
	brt(0, 10),
	brt(8, 10),
	brt(0, 0),
	drk(8, 0),
	brt(5, 0),
	brt(5, 3),
	brt(8, 3),
	brt(8, 0),
	LAST,

/*	table for &	*/
	drk(7, 3),
	brt(4, 0),
	brt(1, 0),
	brt(0, 3),
	brt(5, 8),
	brt(4, 10),
	brt(3, 10),
	brt(1, 8),
	brt(8, 0),
	LAST,

/*	table for '	*/
	drk(4, 6),
	brt(5, 10),
	brt(6, 10),
	brt(4, 6),
	LAST,

/*	table for (	*/
	drk(5, 0 ),
	brt(3, 1 ),
	brt(2, 4 ),
	brt(2, 6 ),
	brt(3, 9 ),
	brt(5, 10 ),
	LAST,

/*	table for )	*/
	drk(3, 0 ),
	brt(5, 1 ),
	brt(6, 4 ),
	brt(6, 6 ),
	brt(5, 9 ),
	brt(3, 10 ),
	LAST,

/*	table for *	*/
	drk(4, 2 ),
	brt(4, 8 ),
	drk(6, 7 ),
	brt(2, 3 ),
	drk(6, 3 ),
	brt(2, 7 ),
	drk(1, 5 ),
	brt(7, 5 ),
	LAST,

/*	table for +	*/
	drk(1, 5 ),
	brt(7, 5 ),
	drk(4, 8 ),
	brt(4, 2 ),
	LAST,

/*	table for, 	*/
	drk(5, 0 ),
	brt(3, 2 ),
	brt(3, 0 ),
	brt(5, 2 ),
	brt(5, 0 ),
	bneg(2, 2 ),
	brt(4, 0 ),
	LAST,

/*	table for -	*/
	drk(1, 5 ),
	brt(7, 5 ),
	LAST,

/*	table for .	*/
	drk(5, 0 ),
	brt(3, 2 ),
	brt(3, 0 ),
	brt(5, 2 ),
	brt(5, 0 ),
	LAST,

/*	table for /	*/
	brt(8, 10 ),
	LAST,

/*	table for 0	*/
	drk(8, 10),
	brt(0, 0),
	brt(0, 10),
	brt(8, 10),
	brt(8, 0),
	brt(0, 0),
	LAST,

/*	table for 1	*/
	drk(4, 0 ),
	brt(4, 10 ),
	brt(2, 8 ),
	LAST,

/*	table for 2	*/
	drk(0, 6 ),
	brt(0, 8 ),
	brt(3, 10 ),
	brt(5, 10 ),
	brt(8, 8 ),
	brt(8, 7 ),
	brt(0, 2 ),
	brt(0, 0 ),
	brt(8, 0 ),
	LAST,

/*	table for 3	*/
	drk(0, 10 ),
	brt(8, 10 ),
	brt(8, 5 ),
	brt(0, 5 ),
	brt(8, 5 ),
	brt(8, 0 ),
	brt(0, 0 ),
	LAST,

/*	table for 4	*/
	drk(0, 10 ),
	brt(0, 5 ),
	brt(8, 5 ),
	drk(8, 10 ),
	brt(8, 0 ),
	LAST,

/*	table for 5	*/
	drk(8, 10 ),
	brt(0, 10 ),
	brt(0, 5 ),
	brt(8, 5 ),
	brt(8, 0 ),
	brt(0, 0 ),
	LAST,

/*	table for 6	*/
	drk(0, 10 ),
	brt(0, 0 ),
	brt(8, 0 ),
	brt(8, 5 ),
	brt(0, 5 ),
	LAST,

/*	table for 7	*/
	drk(0, 10 ),
	brt(8, 10 ),
	brt(6, 0 ),
	LAST,

/*	table for 8	*/
	drk(0, 5 ),
	brt(0, 0 ),
	brt(8, 0 ),
	brt(8, 5 ),
	brt(0, 5 ),
	brt(0, 10 ),
	brt(8, 10 ),
	brt(8, 5 ),
	LAST,

/*	table for 9	*/
	drk(8, 5 ),
	brt(0, 5 ),
	brt(0, 10 ),
	brt(8, 10 ),
	brt(8, 0 ),
	LAST,

/*	table for :	*/
	drk(5, 6 ),
	brt(3, 8 ),
	brt(3, 6 ),
	brt(5, 8 ),
	brt(5, 6 ),
	drk(5, 0 ),
	brt(3, 2 ),
	brt(3, 0 ),
	brt(5, 2 ),
	brt(5, 0 ),
	LAST,

/*	table for ;	*/
	drk(5, 6 ),
	brt(3, 8 ),
	brt(3, 6 ),
	brt(5, 8 ),
	brt(5, 6 ),
	drk(5, 0 ),
	brt(3, 2 ),
	brt(3, 0 ),
	brt(5, 2 ),
	brt(5, 0 ),
	bneg(2, 2 ),
	brt(4, 0 ),
	LAST,

/*	table for <	*/
	drk(8, 8 ),
	brt(0, 5 ),
	brt(8, 2 ),
	LAST,

/*	table for =	*/
	drk(0, 7 ),
	brt(8, 7 ),
	drk(0, 3 ),
	brt(8, 3 ),
	LAST,

/*	table for >	*/
	drk(0, 8 ),
	brt(8, 5 ),
	brt(0, 2 ),
	LAST,

/*	table for ?	*/
	drk(3, 0 ),
	brt(5, 2 ),
	brt(5, 0 ),
	brt(3, 2 ),
	brt(3, 0 ),
	drk(1, 7 ),
	brt(1, 9 ),
	brt(3, 10 ),
	brt(5, 10 ),
	brt(7, 9 ),
	brt(7, 7 ),
	brt(4, 5 ),
	brt(4, 3 ),
	LAST,

/*	table for @	*/
	drk(0, 8 ),
	brt(2, 10 ),
	brt(6, 10 ),
	brt(8, 8 ),
	brt(8, 2 ),
	brt(6, 0 ),
	brt(2, 0 ),
	brt(1, 1 ),
	brt(1, 4 ),
	brt(2, 5 ),
	brt(4, 5 ),
	brt(5, 4 ),
	brt(5, 0 ),
	LAST,

/*	table for A	*/
	brt(0, 8 ),
	brt(2, 10 ),
	brt(6, 10 ),
	brt(8, 8 ),
	brt(8, 0 ),
	drk(0, 5 ),
	brt(8, 5 ),
	LAST,

/*	table for B	*/
	brt(0, 10 ),
	brt(5, 10 ),
	brt(8, 9 ),
	brt(8, 6 ),
	brt(5, 5 ),
	brt(0, 5 ),
	brt(5, 5 ),
	brt(8, 4 ),
	brt(8, 1 ),
	brt(5, 0 ),
	brt(0, 0 ),
	LAST,

/*	table for C	*/
	drk(8, 2 ),
	brt(6, 0 ),
	brt(2, 0 ),
	brt(0, 2 ),
	brt(0, 8 ),
	brt(2, 10 ),
	brt(6, 10 ),
	brt(8, 8 ),
	LAST,

/*	table for D	*/
	brt(0, 10 ),
	brt(5, 10 ),
	brt(8, 8 ),
	brt(8, 2 ),
	brt(5, 0 ),
	brt(0, 0 ),
	LAST,

/*	table for E	*/
	drk(8, 0 ),
	brt(0, 0 ),
	brt(0, 10 ),
	brt(8, 10 ),
	drk(0, 5 ),
	brt(5, 5 ),
	LAST,

/*	table for F	*/
	brt(0, 10 ),
	brt(8, 10 ),
	drk(0, 5 ),
	brt(5, 5 ),
	LAST,

/*	table for G	*/
	drk(5, 5 ),
	brt(8, 5 ),
	brt(8, 2 ),
	brt(6, 0 ),
	brt(2, 0 ),
	brt(0, 2 ),
	brt(0, 8 ),
	brt(2, 10 ),
	brt(6, 10 ),
	brt(8, 8 ),
	LAST,

/*	table for H	*/
	brt(0, 10 ),
	drk(8, 10 ),
	brt(8, 0 ),
	drk(0, 6 ),
	brt(8, 6 ),
	LAST,

/*	table for I	*/
	drk(4, 0 ),
	brt(6, 0 ),
	drk(5, 0 ),
	brt(5, 10 ),
	brt(4, 10 ),
	brt(6, 10 ),
	LAST,

/*	table for J	*/
	drk(0, 2 ),
	brt(2, 0 ),
	brt(5, 0 ),
	brt(7, 2 ),
	brt(7, 10 ),
	brt(6, 10 ),
	brt(8, 10 ),
	LAST,

/*	table for K	*/
	brt(0, 10 ),
	drk(0, 5 ),
	brt(8, 10 ),
	drk(3, 7 ),
	brt(8, 0 ),
	LAST,

/*	table for L	*/
	drk(8, 0 ),
	brt(0, 0 ),
	brt(0, 10 ),
	LAST,

/*	table for M	*/
	brt(0, 10 ),
	brt(4, 5 ),
	brt(8, 10 ),
	brt(8, 10 ),
	brt(8, 0 ),
	LAST,

/*	table for N	*/
	brt(0, 10 ),
	brt(8, 0 ),
	brt(8, 10 ),
	LAST,

/*	table for O	*/
	drk(0, 2 ),
	brt(0, 8 ),
	brt(2, 10 ),
	brt(6, 10 ),
	brt(8, 8 ),
	brt(8, 2 ),
	brt(6, 0 ),
	brt(2, 0 ),
	brt(0, 2 ),
	LAST,

/*	table for P	*/
	brt(0, 10 ),
	brt(6, 10 ),
	brt(8, 9 ),
	brt(8, 6 ),
	brt(6, 5 ),
	brt(0, 5 ),
	LAST,

/*	table for Q	*/
	drk(0, 2 ),
	brt(0, 8 ),
	brt(2, 10 ),
	brt(6, 10 ),
	brt(8, 8 ),
	brt(8, 2 ),
	brt(6, 0 ),
	brt(2, 0 ),
	brt(0, 2 ),
	drk(5, 3 ),
	brt(8, 0 ),
	LAST,

/*	table for R	*/
	brt(0, 10 ),
	brt(6, 10 ),
	brt(8, 8 ),
	brt(8, 6 ),
	brt(6, 5 ),
	brt(0, 5 ),
	drk(5, 5 ),
	brt(8, 0 ),
	LAST,

/*	table for S	*/
	drk(0, 1 ),
	brt(1, 0 ),
	brt(6, 0 ),
	brt(8, 2 ),
	brt(8, 4 ),
	brt(6, 6 ),
	brt(2, 6 ),
	brt(0, 7 ),
	brt(0, 9 ),
	brt(1, 10 ),
	brt(7, 10 ),
	brt(8, 9 ),
	LAST,

/*	table for T	*/
	drk(4, 0 ),
	brt(4, 10 ),
	drk(0, 10 ),
	brt(8, 10 ),
	LAST,

/*	table for U	*/
	drk(0, 10 ),
	brt(0, 2 ),
	brt(2, 0 ),
	brt(6, 0 ),
	brt(8, 2 ),
	brt(8, 10 ),
	LAST,

/*	table for V	*/
	drk(0, 10 ),
	brt(4, 0 ),
	brt(8, 10 ),
	LAST,

/*	table for W	*/
	drk(0, 10 ),
	brt(1, 0 ),
	brt(4, 4 ),
	brt(7, 0 ),
	brt(8, 10 ),
	LAST,

/*	table for X	*/
	brt(8, 10 ),
	drk(0, 10 ),
	brt(8, 0 ),
	LAST,

/*	table for Y	*/
	drk(0, 10 ),
	brt(4, 4 ),
	brt(8, 10 ),
	drk(4, 4 ),
	brt(4, 0 ),
	LAST,

/*	table for Z	*/
	drk(0, 10 ),
	brt(8, 10 ),
	brt(0, 0 ),
	brt(8, 0 ),
	LAST,

/*	table for [	*/
	drk(6, 0 ),
	brt(4, 0 ),
	brt(4, 10 ),
	brt(6, 10 ),
	LAST,

/*	table for \	*/
	drk(0, 10 ),
	brt(8, 0 ),
	LAST,

/*	table for ]	*/
	drk(2, 0 ),
	brt(4, 0 ),
	brt(4, 10 ),
	brt(2, 10 ),
	LAST,

/*	table for ^	*/
	drk(4, 0 ),
	brt(4, 10 ),
	drk(2, 8 ),
	brt(4, 10 ),
	brt(6, 8 ),
	LAST,

/*	table for _	*/
	dneg(0, 1),
	bneg(11, 1),
	LAST,

/*	table for ascii 96: accent	*/
	drk(3, 10),
	brt(5, 6),
	brt(4, 10),
	brt(3, 10),
	LAST,

/*	table for a	*/
	drk(0, 5),
	brt(1, 6),
	brt(6, 6),
	brt(7, 5),
	brt(7, 1),
	brt(8, 0),
	drk(7, 1),
	brt(6, 0),
	brt(1, 0),
	brt(0, 1),
	brt(0, 2),
	brt(1, 3),
	brt(6, 3),
	brt(7, 2),
	LAST,

/*	table for b	*/
	brt(0, 10),
	drk(8, 3),
	brt(7, 5),
	brt(4, 6),
	brt(1, 5),
	brt(0, 3),
	brt(1, 1),
	brt(4, 0),
	brt(7, 1),
	brt(8, 3),
	LAST,

/*	table for c	*/
	drk(8, 5),
	brt(7, 6),
	brt(2, 6),
	brt(0, 4),
	brt(0, 4),
	brt(0, 2),
	brt(2, 0),
	brt(7, 0),
	brt(8, 1),
	LAST,

/*	table for d	*/
	drk(8, 0),
	brt(8, 10),
	drk(8, 3),
	brt(7, 5),
	brt(4, 6),
	brt(1, 5),
	brt(0, 3),
	brt(1, 1),
	brt(4, 0),
	brt(7, 1),
	brt(8, 3),
	LAST,

/*	table for e	*/
	drk(0, 4),
	brt(1, 3),
	brt(7, 3),
	brt(8, 4),
	brt(8, 5),
	brt(7, 6),
	brt(1, 6),
	brt(0, 5),
	brt(0, 1),
	brt(1, 0),
	brt(7, 0),
	brt(8, 1),
	LAST,

/*	table for f	*/
	drk(2, 0),
	brt(2, 9),
	brt(3, 10),
	brt(5, 10),
	brt(6, 9),
	drk(1, 5),
	brt(4, 5),
	LAST,

/*	table for g	*/
	drk(8, 6),
	drk(8, 3),
	brt(7, 5),
	brt(4, 6),
	brt(1, 5),
	brt(0, 3),
	brt(1, 1),
	brt(4, 0),
	brt(7, 1),
	brt(8, 3),
	bneg(8, 2),
	bneg(7, 3),
	bneg(1, 3),
	bneg(0, 2),
	LAST,

/*	table for h	*/
	brt(0, 10),
	drk(0, 4),
	brt(2, 6),
	brt(6, 6),
	brt(8, 4),
	brt(8, 0),
	LAST,

/*	table for i	*/
	drk(4, 0),
	brt(4, 6),
	brt(3, 6),
	drk(4, 9),
	brt(4, 8),
	drk(3, 0),
	brt(5, 0),
	LAST,

/*	table for j	*/
	drk(5, 6),
	brt(6, 6),
	bneg(6, 2),
	bneg(5, 3),
	bneg(3, 3),
	bneg(2, 2),
	LAST,

/*	table for k	*/
	brt(2, 0),
	brt(2, 10),
	brt(0, 10),
	drk(2, 4),
	brt(4, 4),
	brt(8, 6),
	drk(4, 4),
	brt(8, 0),
	LAST,

/*	table for l	*/
	drk(3, 10),
	brt(4, 10),
	brt(4, 2),
	brt(5, 0),
	LAST,

/*	table for m	*/
	brt(0, 6),
	drk(0, 5),
	brt(1, 6),
	brt(3, 6),
	brt(4, 5),
	brt(4, 0),
	drk(4, 5),
	brt(5, 6),
	brt(7, 6),
	brt(8, 5),
	brt(8, 0),
	LAST,

/*	table for n	*/
	brt(0, 6),
	drk(0, 4),
	brt(2, 6),
	brt(6, 6),
	brt(8, 4),
	brt(8, 0),
	LAST,

/*	table for o	*/
	drk(8, 3),
	brt(7, 5),
	brt(4, 6),
	brt(1, 5),
	brt(0, 3),
	brt(1, 1),
	brt(4, 0),
	brt(7, 1),
	brt(8, 3),
	LAST,

/*	table for p	*/
	drk(0, 6),
	bneg(0, 3),
	drk(8, 3),
	brt(7, 5),
	brt(4, 6),
	brt(1, 5),
	brt(0, 3),
	brt(1, 1),
	brt(4, 0),
	brt(7, 1),
	brt(8, 3),
	LAST,

/*	table for q	*/
	drk(8, 6),
	drk(8, 3),
	brt(7, 5),
	brt(4, 6),
	brt(1, 5),
	brt(0, 3),
	brt(1, 1),
	brt(4, 0),
	brt(7, 1),
	brt(8, 3),
	bneg(8, 3),
	bneg(9, 3),
	LAST,

/*	table for r	*/
	brt(1, 0),
	brt(1, 6),
	brt(0, 6),
	drk(1, 4),
	brt(3, 6),
	brt(6, 6),
	brt(8, 4),
	LAST,

/*	table for s	*/
	drk(0, 1),
	brt(1, 0),
	brt(7, 0),
	brt(8, 1),
	brt(7, 2),
	brt(1, 4),
	brt(0, 5),
	brt(1, 6),
	brt(7, 6),
	brt(8, 5),
	LAST,

/*	table for t	*/
	drk(7, 1),
	brt(6, 0),
	brt(4, 0),
	brt(3, 1),
	brt(3, 10),
	brt(2, 10),
	drk(1, 5),
	brt(5, 5),
	LAST,

/*	table for u	*/
	drk(0, 6),
	brt(1, 6),
	brt(1, 1),
	brt(2, 0),
	brt(6, 0),
	brt(7, 1),
	brt(7, 6),
	drk(7, 1),
	brt(8, 0),
	LAST,

/*	table for v	*/
	drk(0, 6),
	brt(4, 0),
	brt(8, 6),
	LAST,

/*	table for w	*/
	drk(0, 6),
	brt(0, 5),
	brt(2, 0),
	brt(4, 5),
	brt(6, 0),
	brt(8, 5),
	brt(8, 6),
	LAST,

/*	table for x	*/
	brt(8, 6),
	drk(0, 6),
	brt(8, 0),
	LAST,

/*	table for y	*/
	drk(0, 6),
	brt(0, 1),
	brt(1, 0),
	brt(7, 0),
	brt(8, 1),
	drk(8, 6),
	bneg(8, 2),
	bneg(7, 3),
	bneg(1, 3),
	bneg(0, 2),
	LAST,

/*	table for z	*/
	drk(0, 6),
	brt(8, 6),
	brt(0, 0),
	brt(8, 0),
	LAST,

/*	table for ascii 123, left brace	*/
	drk(6, 10),
	brt(5, 10),
	brt(4, 9),
	brt(4, 6),
	brt(3, 5),
	brt(4, 4),
	brt(4, 1),
	brt(5, 0),
	brt(6, 0),
	LAST,

/*	table for ascii 124, vertical bar	*/
	drk(4, 4),
	brt(4, 0),
	brt(5, 0),
	brt(5, 4),
	brt(4, 4),
	drk(4, 6),
	brt(4, 10),
	brt(5, 10),
	brt(5, 6),
	brt(4, 6),
	LAST,

/*	table for ascii 125, right brace	*/
	drk(2, 0),
	brt(3, 0),
	brt(4, 1),
	brt(4, 4),
	brt(5, 5),
	brt(4, 6),
	brt(4, 9),
	brt(3, 10),
	brt(2, 10),
	LAST,

/*	table for ascii 126, tilde	*/
	drk(0, 5),
	brt(1, 6),
	brt(3, 6),
	brt(5, 4),
	brt(7, 4),
	brt(8, 5),
	LAST,

/*	table for ascii 127, rubout	*/
	drk(0, 2),
	brt(0, 8),
	brt(8, 8),
	brt(8, 2),
	brt(0, 2),
	LAST
};
