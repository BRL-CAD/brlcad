/*
 *  			P L O T . C
 *  
 *	Provide UNIX-plot output of the current view.
 *
 *  Authors -
 *  	Michael John Muuss	(This version)
 *	Douglas A. Gwyn		(3-D UNIX Plot routines)
 *  	Gary S. Moss		(Original gedplot program)
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


#include	<math.h>
#include	<stdio.h>
#include "./machine.h"	/* special copy */
#include "../h/vmath.h"
#include "../h/mater.h"
#include "ged.h"
#include "objdir.h"
#include "solid.h"
#include "dm.h"

extern FILE	*popen();	/* stdio pipe open routine */

extern int	numargs;	/* number of args */
extern char	*cmd_args[];	/* array of pointers to args */
extern char	*local_unit[];

/* UNIX-Plot routines */
static FILE *up_fp;		/* output file pointer */
int up_color(), up_cont(), up_cont3(), up_erase(), up_label();
int up_line(), up_line3(), up_linemod();
int up_move(), up_move3(), up_putsi(), up_space(), up_space3();


/*
 *  			F _ P L O T
 *
 *  plot file [opts]
 *  potential options might include:
 *	grid, 3d w/color, |filter, infinite Z
 */
void
f_plot()
{
	register struct solid *sp;
	register struct veclist *vp;
	static vect_t clipmin, clipmax;
	static vect_t last;		/* last drawn point */
	static vect_t fin;
	static vect_t start;
	int nvec;
	int Three_D;			/* 0=2-D -vs- 1=3-D */
	int Z_clip;			/* Z clipping */
	int Dashing;			/* linetype is dashed */
	char **argv;
	char buf[128];

	if( not_state( ST_VIEW, "UNIX Plot of view" ) )
		return;

	/* Process any options */
	Three_D = 1;				/* 3-D w/color, by default */
	Z_clip = 0;				/* NO Z clipping, by default*/
	argv = &cmd_args[1];
	while( argv[0] != (char *)0 && argv[0][0] == '-' )  {
		switch( argv[0][1] )  {
		case '2':
			Three_D = 0;		/* 2-D, for portability */
			break;
		case 'g':
			/* do grid */
			(void)printf("grid unimplemented\n");
			break;
		case 'z':
		case 'Z':
			/* Enable Z clipping */
			(void)printf("Clipped in Z to viewing cube\n");
			Z_clip = 1;
			break;
		default:
			(void)printf("bad PLOT option %s\n", argv[0] );
			break;
		}
		argv++;
	}
	if( argv[0] == (char *)0 )  {
		printf("no filename or filter specified\n");
		return;
	}
	if( argv[0][0] == '|' )  {
		strncpy( buf, &argv[0][1], sizeof(buf) );
		while( (++argv)[0] != (char *)0 )  {
			strncat( buf, " ", sizeof(buf) );
			strncat( buf, argv[0], sizeof(buf) );
		}
		(void)printf("piped to %s\n", buf );
		if( (up_fp = popen( buf, "w" ) ) == NULL )  {
			perror( buf );
			return;
		}
	}  else  {
		(void)printf("plot stored in %s", argv[0] );
		if( (up_fp = fopen( argv[0], "w" )) == NULL )  {
			perror( argv[0] );
			return;
		}
	}

	color_soltab();		/* apply colors to the solid table */

	/* Compute the clipping bounds of the screen in view space */
	clipmin[X] = -1.0;
	clipmax[X] =  1.0;
	clipmin[Y] = -1.0;
	clipmax[Y] =  1.0;
	if( Z_clip )  {
		clipmin[Z] = -1.0;
		clipmax[Z] =  1.0;
	} else {
		clipmin[Z] = -1.0e17;
		clipmax[Z] =  1.0e17;
	}

	/*
	 * Viewing region is from -1.0 to +1.0
	 * which we map to integer space -2048 to +2048
	 */
	if( Three_D )
		up_space3( -2048, -2048, -2048, 2048, 2048, 2048 );
	else
		up_space( -2048, -2048, 2048, 2048 );
	up_erase();
	Dashing = 0;
	up_linemod("solid");
	FOR_ALL_SOLIDS( sp )  {
		if( Dashing != sp->s_soldash )  {
			if( sp->s_soldash )
				up_linemod("dotdashed");
			else
				up_linemod("solid");
			Dashing = sp->s_soldash;
		}
		nvec = sp->s_vlen;
		for( vp = sp->s_vlist; nvec-- > 0; vp++ )  {
			if( vp->vl_pen == PEN_UP )  {
				/* Move, not draw */
				MAT4X3PNT( last, model2view, vp->vl_pnt );
				continue;
			}
			/* draw */
			MAT4X3PNT( fin, model2view, vp->vl_pnt );
			VMOVE( start, last );
			VMOVE( last, fin );
			if(
				vclip( start, fin, clipmin, clipmax ) == 0
			)  continue;

			if( Three_D )  {
				register struct mater *mp;
				mp = (struct mater *)sp->s_materp;
				if( mp != MATER_NULL )
					up_color( mp->mt_r,
						mp->mt_g,
						mp->mt_b );
				up_line3(
					(int)( start[X] * 2047 ),
					(int)( start[Y] * 2047 ),
					(int)( start[Z] * 2047 ),
					(int)( fin[X] * 2047 ),
					(int)( fin[Y] * 2047 ),
					(int)( fin[Z] * 2047 ) );
			}  else
				up_line(
					(int)( start[0] * 2047 ),
					(int)( start[1] * 2047 ),
					(int)( fin[0] * 2047 ),
					(int)( fin[1] * 2047 ) );
		}
	}
	if( cmd_args[1][0] == '|' )
		(void)pclose( up_fp );
	else
		(void)fclose( up_fp );
}

void
f_area()
{
	register struct solid *sp;
	register struct veclist *vp;
	static vect_t last;
	static vect_t fin;
	char buf[128];
	FILE *fp;
	register int nvec;

	if( not_state( ST_VIEW, "Presented Area Calculation" ) )
		return;

	FOR_ALL_SOLIDS( sp )  {
		if( !sp->s_Eflag && sp->s_soldash != 0 )  {
			printf("everything in view must be 'E'ed\n");
			return;
		}
	}

	/* Create pipes to boundp | parea */
	if( numargs == 2 )  {
		sprintf( buf, "boundp -t %s | parea", cmd_args[1] );
		(void)printf("Tolerance is %s\n", cmd_args[1] );
	}  else  {
		double tol = maxview * 0.001;
		sprintf( buf, "boundp -t %e | parea", tol );
		(void)printf("Auto-tolerance of 0.1%% is %e\n", tol );
	}

	if( (fp = popen( buf, "w" )) == NULL )  {
		perror( buf );
		return;
	}

	/*
	 * Write out rotated but unclipped, untranslated,
	 * and unscaled vectors
	 */
	FOR_ALL_SOLIDS( sp )  {
		nvec = sp->s_vlen;
		for( vp = sp->s_vlist; nvec-- > 0; vp++ )  {
			if( vp->vl_pen == PEN_UP )  {
				/* Move, not draw */
				MAT4X3VEC( last, Viewrot, vp->vl_pnt );
				continue;
			}
			/* draw.  */
			MAT4X3VEC( fin, Viewrot, vp->vl_pnt );

			fprintf(fp, "%.9e %.9e %.9e %.9e\n",
				last[X] * base2local,
				last[Y] * base2local,
				fin[X] * base2local,
				fin[Y] * base2local );

			VMOVE( last, fin );
		}
	}
	(void)printf("Presented area from this viewpoint, square %s:\n",
		local_unit[localunit] );
	pclose( fp );
}

/*
 *  Special version of PLOT library to allow us to write to a
 *  file descriptor other than stdout, and to provide the BRL-extensions
 *  for 3-D UNIX Plot.
 */
/*
	up_color -- deposit color selection in UNIX plot output file
		(BRL addition)

	last edit:	04-Jan-1984	D A Gwyn
*/
up_color( r, g, b )
	int	r, g, b;		/* color components, 0..255 */
	{
	putc( 'C', up_fp );
	putc( r, up_fp );
	putc( g, up_fp );
	putc( b, up_fp );
	}

up_cont(xi,yi){
	putc('n',up_fp);
	up_putsi(xi);
	up_putsi(yi);
}

up_cont3(xi,yi,zi){
	putc('N',up_fp);
	up_putsi(xi);
	up_putsi(yi);
	up_putsi(zi);
}

up_erase(){
	putc('e',up_fp);
}

up_label(s)
char *s;
{
	int i;
	putc('t',up_fp);
	for(i=0;s[i];)putc(s[i++],up_fp);
	putc('\n',up_fp);
}

up_line(xval0,yval0,xval1,yval1){
	putc('l',up_fp);
	up_putsi(xval0);
	up_putsi(yval0);
	up_putsi(xval1);
	up_putsi(yval1);
}

up_line3(xval0,yval0,zval0,xval1,yval1,zval1){
	putc('L',up_fp);
	up_putsi(xval0);
	up_putsi(yval0);
	up_putsi(zval0);
	up_putsi(xval1);
	up_putsi(yval1);
	up_putsi(zval1);
}

up_linemod(s)
char *s;
{
	int i;
	putc('f',up_fp);
	for(i=0;s[i];)putc(s[i++],up_fp);
	putc('\n',up_fp);
}

up_move(xvali,yvali){
	putc('m',up_fp);
	up_putsi(xvali);
	up_putsi(yvali);
}

up_move3(xvali,yvali,zvali){
	putc('M',up_fp);
	up_putsi(xvali);
	up_putsi(yvali);
	up_putsi(zvali);
}

up_putsi(a){
	putc(a&0377,up_fp);		/* DAG -- bug fix */
	putc((a>>8)&0377,up_fp);	/* DAG */
}

up_space(xval0,yval0,xval1,yval1){
	putc('s',up_fp);
	up_putsi(xval0);
	up_putsi(yval0);
	up_putsi(xval1);
	up_putsi(yval1);
}

up_space3(xval0,yval0,zval0,xval1,yval1,zval1){
	putc('S',up_fp);
	up_putsi(xval0);
	up_putsi(yval0);
	up_putsi(zval0);
	up_putsi(xval1);
	up_putsi(yval1);
	up_putsi(zval1);
}
