/*
 *			O V E R L A Y . C
 *
 * Functions -
 *	f_overlay		Read a UNIX-Plot file as an overlay
 *	invent_solid		Turn list of vectors into phony solid
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

extern void	perror();
extern int	atoi(), execl(), fork(), nice(), wait();
extern long	time();

extern int	numargs;	/* number of args */
extern char	*cmd_args[];	/* array of pointers to args */

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
f_overlay()
{
	char	*name;
	FILE	*fp;
	struct vlhead	vhead;
	int	ret;

	if( numargs <= 2 )
		name = "_PLOT_OVERLAY_";
	else
		name = cmd_args[2];

	if( (fp = fopen(cmd_args[1], "r")) == NULL )  {
		perror(cmd_args[1]);
		return;
	}

	vhead.vh_first = vhead.vh_last = VL_NULL;
	ret = uplot_vlist( &vhead, fp );
	fclose(fp);
	if( ret < 0 )  return;

	invent_solid( name, &vhead );

	dmaflag++;
}

/*
 *			I N V E N T _ S O L I D
 *
 *  Invent a solid by adding a fake entry in the database table,
 *  adding an entry to the solid table, and populating it with
 *  the given vector list.
 *
 *  This parallels much of the code in dodraw.c
 */
int
invent_solid( name, vhead )
char	*name;
struct vlhead	*vhead;
{
	register struct directory *dp;
	register struct solid *sp;
	register struct vlist *vp;
	vect_t		max, min;

#define PHONY_ADDR	(-1L)
	if( (dp = db_lookup( dbip,  name, LOOKUP_QUIET )) != DIR_NULL )  {
		if( dp->d_addr != PHONY_ADDR )  {
			printf("invent_solid(%s) would clobber existing database entry, ignored\n");
			return(-1);
		}
		/* Name exists from some other overlay,
		 * zap any associated solids
		 */
		eraseobj(dp);
	} else {
		/* Need to enter phony name in directory structure */
		dp = db_diradd( dbip,  name, PHONY_ADDR, 0, DIR_SOLID );
	}

	/* Obtain a fresh solid structure, and fill it in */
	GET_SOLID(sp);

	VSETALL( max, -INFINITY );
	VSETALL( min,  INFINITY );
	sp->s_vlist = vhead->vh_first;
	sp->s_vlen = 0;
	for( vp = vhead->vh_first; vp != VL_NULL; vp = vp->vl_forw )  {
		VMINMAX( min, max, vp->vl_pnt );
		sp->s_vlen++;
	}
	VSET( sp->s_center,
		(max[X] + min[X])*0.5,
		(max[Y] + min[Y])*0.5,
		(max[Z] + min[Z])*0.5 );

	sp->s_size = max[X] - min[X];
	MAX( sp->s_size, max[Y] - min[Y] );
	MAX( sp->s_size, max[Z] - min[Z] );

	/* set path information -- this is a top level node */
	sp->s_last = 0;
	sp->s_path[0] = dp;

	sp->s_iflag = DOWN;
	sp->s_soldash = 0;
	sp->s_Eflag = 0;		/* This is a solid */
	sp->s_color[0] = sp->s_basecolor[0] = 255;
	sp->s_color[1] = sp->s_basecolor[1] = 255;
	sp->s_color[2] = sp->s_basecolor[2] = 0;
	sp->s_regionid = 0;
	sp->s_addr = 0;
	sp->s_bytes = 0;

	/* Cvt to displaylist, determine displaylist memory requirement. */
	if( !no_memory && (sp->s_bytes = dmp->dmr_cvtvecs( sp )) != 0 )  {
		/* Allocate displaylist storage for object */
		sp->s_addr = memalloc( &(dmp->dmr_map), sp->s_bytes );
		if( sp->s_addr == 0 )  {
			no_memory = 1;
			(void)printf("invent_solid: out of Displaylist\n");
			sp->s_bytes = 0;	/* not drawn */
		} else {
			sp->s_bytes = dmp->dmr_load(sp->s_addr, sp->s_bytes );
		}
	}

	/* Solid successfully drawn, add to linked list of solid structs */
	APPEND_SOLID( sp, HeadSolid.s_back );
	dmp->dmr_viewchange( DM_CHGV_ADD, sp );
	dmp->dmr_colorchange();
	return(0);		/* OK */
}

/*
 *			U P L O T _ V L I S T
 *
 *  Read a BRL-style 3-D UNIX-plot file into a vector list.
 *  For now, discard color information, only extract vectors.
 *  This might be more naturally located in mged/plot.c
 */
int
uplot_vlist( vhead, fp )
register struct vlhead	*vhead;
register FILE		*fp;
{
	register int	c;
	struct	uplot *up;
	fastf_t	arg[6];
	char	inbuf[8];
	vect_t	a,b;
	int	cc;
	int	i;

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
						while( (cc = getc(fp)) != '\n'
							&& cc != EOF )
								;
						break;
					case TCHAR:
						(void) getc(fp);
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
			ADD_VL( vhead, arg, 0 );
			break;
		case 'M':
		case 'O':
			/* 3-D move */
			ADD_VL( vhead, arg, 0 );
			break;
		case 'n':
		case 'q':
			/* 2-D draw */
			arg[Z] = 0;
			ADD_VL( vhead, arg, 1 );
			break;
		case 'N':
		case 'Q':
			/* 3-D draw */
			ADD_VL( vhead, arg, 1 );
			break;
		case 'l':
		case 'v':
			/* 2-D line */
			VSET( a, arg[0], arg[1], 0.0 );
			VSET( b, arg[2], arg[3], 0.0 );
			ADD_VL( vhead, a, 0 );
			ADD_VL( vhead, b, 1 );
			break;
		case 'L':
		case 'V':
			/* 3-D line */
			VSET( a, arg[0], arg[1], arg[2] );
			VSET( b, arg[3], arg[4], arg[5] );
			ADD_VL( vhead, a, 0 );
			ADD_VL( vhead, b, 1 );
			break;
		case 'p':
		case 'x':
			/* 2-D point */
			arg[Z] = 0;
			ADD_VL( vhead, arg, 0 );
			ADD_VL( vhead, arg, 1 );
			break;
		case 'P':
		case 'X':
			/* 3-D point */
			ADD_VL( vhead, arg, 0 );
			ADD_VL( vhead, arg, 1 );
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
