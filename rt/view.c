/*
 *			V I E W
 *
 * Ray Tracing program, lighting model
 *
 * Author -
 *	Michael John Muuss
 *
 *	U. S. Army Ballistic Research Laboratory
 *	March 27, 1984
 *
 * Notes -
 *	The normals on all surfaces point OUT of the solid.
 *	The incomming light rays point IN.  Thus the sign change.
 *
 * $Revision$
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include "vmath.h"
#include "raytrace.h"
#include "debug.h"

/* /vld/include/ray.h -- ray segment data format (D A Gwyn) */
/* binary ray segment data record; see ray(4V) */
struct vldray  {
	float	ox;			/* origin coordinates */
	float	oy;
	float	oz;
	float	rx;			/* ray vector */
	float	ry;
	float	rz;
	float	na;			/* origin surface normal */
	float	ne;
	/* the following are in 2 pieces for binary file portability: */
	short	ob_lo;			/* object code low 16 bits */
	short	ob_hi;			/* object code high 16 bits */
	short	rt_lo;			/* ray tag low 16 bits */
	short	rt_hi;			/* ray tag high 16 bits */
};
static struct vldray vldray;

extern int ikfd;		/* defined in iklib.o */
extern int ikhires;		/* defined in iklib.o */

extern int outfd;		/* defined in rt.c */
extern FILE *outfp;		/* defined in rt.c */
extern int lightmodel;		/* lighting model # to use */
extern int view_only;

#define MAX_LINE	1024	/* Max pixels/line */
static long scanline[MAX_LINE];	/* 1 scanline pixel buffer */
static long *pixelp;		/* pointer to first empty pixel */
static int scanbytes;		/* # of bytes of scanline to be written */

/* Stuff for pretty-picture output format */
static struct soltab *last_solidp;	/* pointer to last solid hit */
static int last_item;			/* item number of last region hit */
static int last_ihigh;			/* last inten_high */
static int ntomiss;			/* number of pixels to miss */
static int col;				/* column; for PP 75 char/line crap */

#define BACKGROUND	0x00404040		/* Grey */
vect_t l0color = {  28,  28, 255 };		/* R, G, B */
vect_t l1color = { 255,  28,  28 };
vect_t l2color = { 255, 255, 255 };		/* White */
vect_t ambient_color = { 255, 255, 255 };	/* Ambient white light */
extern vect_t l0vec;
extern vect_t l1vec;
extern vect_t l2vec;
extern double AmbientIntensity;

/* Null function */
nullf() { ; }

l3init(ap, title1, title2 )
register struct application *ap;
{
	extern double azimuth, elevation;
	extern int npts;

	fprintf(outfp, "%s: %s (RT)\n", title1, title2 );
	fprintf(outfp, "%10d%10d", (int)azimuth, (int)elevation );
	fprintf(outfp, "%10d%10d\n", npts, npts );
}

/* Support for Gwyn's ray files -- write a hit */
l4hit( ap, PartHeadp )
register struct application *ap;
struct partition *PartHeadp;
{
	register struct partition *pp = PartHeadp->pt_forw;
	register int i;	/* XXX */

	for( ; pp != PartHeadp; pp = pp->pt_forw )  {
		/* TODO: Not all may have both in & out points! */
		VMOVE( &(vldray.ox), pp->pt_inhit->hit_point );
		VSUB2( &(vldray.rx), pp->pt_outhit->hit_point,
			pp->pt_inhit->hit_point );
		vldray.na = vldray.ne = 0.0;	/* need angle/azim */
		i = pp->pt_regionp->reg_regionid;
		vldray.ob_lo = i & 0xFFFF;
		vldray.ob_hi = (i>>16) & 0xFFFF;
		vldray.rt_lo = ap->a_x;
		vldray.rt_hi = ap->a_y;
		fwrite( &vldray, sizeof(struct vldray), 1, outfp );
	}
}

/* Support for pretty-picture files */
l3hit( ap, PartHeadp )
register struct application *ap;
struct partition *PartHeadp;
{
	register struct partition *pp = PartHeadp->pt_forw;
	register struct hit *hitp= pp->pt_inhit;
	static double cosI0;
	register int i,j;

#define pchar(c) {putc(c,outfp);if(col++==74){putc('\n',outfp);col=0;}}
	if( (cosI0 = -VDOT(hitp->hit_normal, ap->a_ray.r_dir)) <= 0.0 )  {
		ntomiss++;
		return;
	}
	if( ntomiss > 0 )  {
		pchar(' ');	/* miss target cmd */
		pknum( ntomiss );
		ntomiss = 0;
		last_solidp = SOLTAB_NULL;
	}
	if( last_item != pp->pt_regionp->reg_regionid )  {
		last_item = pp->pt_regionp->reg_regionid;
		pchar( '#' );	/* new item cmd */
		pknum( last_item );
		last_solidp = SOLTAB_NULL;
	}
	if( last_solidp != pp->pt_instp )  {
		last_solidp = pp->pt_instp;
		pchar( '!' );		/* new solid cmd */
	}
	i = cosI0 * 255.0;		/* integer angle */
	j = (i>>5) & 07;
	if( j != last_ihigh )  {
		last_ihigh = j;
		pchar( '0'+j );		/* new inten high */
	}
	j = i & 037;
	pchar( '@'+j );			/* low bits of pixel */
}

/*
 *			V I E W I T
 *
 *  a_hit() routine for simple lighting model.
 */
viewit( ap, PartHeadp )
register struct application *ap;
struct partition *PartHeadp;
{
	register struct partition *pp = PartHeadp->pt_forw;
	register struct hit *hitp= pp->pt_inhit;
	static long inten;
	static fastf_t diffuse2, cosI2;
	static fastf_t diffuse1, cosI1;
	static fastf_t diffuse0, cosI0;
	static vect_t work0, work1;
	static int r,g,b;

	/*
	 * Diffuse reflectance from each light source
	 */
	if( lightmodel == 1 )  {
		/* Light from the "eye" (ray source).  Note sign change */
		diffuse0 = 0;
		if( (cosI0 = -VDOT(hitp->hit_normal, ap->a_ray.r_dir)) >= 0.0 )
			diffuse0 = cosI0 * ( 1.0 - AmbientIntensity);
		VSCALE( work0, l0color, diffuse0 );

		/* Add in contribution from ambient light */
		VSCALE( work1, ambient_color, AmbientIntensity );
		VADD2( work0, work0, work1 );
	}  else if( lightmodel == 0 )  {
		/* Simple attempt at a 3-light model. */
		diffuse0 = 0;
		if( (cosI0 = VDOT(hitp->hit_normal, l0vec)) >= 0.0 )
			diffuse0 = cosI0 * 0.5;		/* % from this src */
		diffuse1 = 0;
		if( (cosI1 = VDOT(hitp->hit_normal, l1vec)) >= 0.0 )
			diffuse1 = cosI1 * 0.5;		/* % from this src */
		diffuse2 = 0;
		if( (cosI2 = VDOT(hitp->hit_normal, l2vec)) >= 0.0 )
			diffuse2 = cosI2 * 0.2;		/* % from this src */

#ifdef notyet
		/* Specular reflectance from first light source */
		/* reflection = (2 * cos(i) * NormalVec) - IncidentVec */
		/* cos(s) = -VDOT(reflection, r_dir) = cosI0 */
		f = 2 * cosI1;
		VSCALE( work, hitp->hit_normal, f );
		VSUB2( reflection, work, l1vec );
		if( not_shadowed && cosI0 > cosAcceptAngle )
			/* Do specular return */;
#endif notyet

		VSCALE( work0, l0color, diffuse0 );
		VSCALE( work1, l1color, diffuse1 );
		VADD2( work0, work0, work1 );
		VSCALE( work1, l2color, diffuse2 );
		VADD2( work0, work0, work1 );

		/* Add in contribution from ambient light */
		VSCALE( work1, ambient_color, AmbientIntensity );
		VADD2( work0, work0, work1 );
	} else if( lightmodel == 2 )  {
		/* Store surface normals pointing inwards */
		/* (For Spencer's moving light program */
		work0[0] = (hitp->hit_normal[0] * (-127)) + 128;
		work0[1] = (hitp->hit_normal[1] * (-127)) + 128;
		work0[2] = (hitp->hit_normal[2] * (-127)) + 128;
	}

	if(debug&DEBUG_HITS)  {
		hit_print( " In", hitp );
		printf("cosI0=%f, diffuse0=%f   ", cosI0, diffuse0 );
		VPRINT("RGB", work0);
	}

	r = work0[0];
	g = work0[1];
	b = work0[2];
	if( r > 255 ) r = 255;
	if( g > 255 ) g = 255;
	if( b > 255 ) b = 255;
	if( r<0 || g<0 || b<0 )  {
		VPRINT("@@ Negative RGB @@", work0);
		inten = 0x0080FF80;
	} else {
		inten = (b << 16) |		/* B */
			(g <<  8) |		/* G */
			(r);			/* R */
	}

	if( ikfd > 0 )
		ikwpixel( ap->a_x, ap->a_y, inten);
	if( outfd > 0 )
		*pixelp++ = inten;
}

l3miss()  {
	last_solidp = SOLTAB_NULL;
	ntomiss++;
}

/*
 *			W B A C K G R O U N D
 *
 *  a_miss() routine.
 */
wbackground( ap )
register struct application *ap;
{
	register long bg;

	if( lightmodel == 2 )
		bg = 0;
	else
		bg = BACKGROUND;
		
	if( ikfd > 0 )
		ikwpixel( ap->a_x, ap->a_y, bg );
	if( outfd > 0 )
		*pixelp++ = bg;
}

/*
 *  			H I T _ P R I N T
 */
hit_print( str, hitp )
char *str;
register struct hit *hitp;
{
	printf("** %s HIT, dist=%f\n", str, hitp->hit_dist );
	VPRINT("** Point ", hitp->hit_point );
	VPRINT("** Normal", hitp->hit_normal );
}

/*
 *  			D E V _ S E T U P
 *  
 *  Prepare the Ikonas display for operation with
 *  npts x npts of useful pixels.
 */
dev_setup(npts)
int npts;
{
	if( npts > MAX_LINE )  {
		printf("view:  %d pixels/line is too many\n", npts);
		exit(12);
	}
	if( outfd > 0 )  {
		/* Output is destined for a pixel file */
		pixelp = &scanline[0];
		if( npts > 512 )
			scanbytes = MAX_LINE * sizeof(long);
		else
			scanbytes = 512 * sizeof(long);
	}  else  {
		/* Output directly to Ikonas */
		if( npts > 512 )
			ikhires = 1;

		ikopen();
		load_map(1);		/* Standard map: linear */
		ikclear();
		if( npts <= 64 )  {
			ikzoom( 7, 7 );		/* 1 pixel gives 8 */
			ikwindow( (0)*4, 4063+29 );
		} else if( npts <= 128 )  {
			ikzoom( 3, 3 );		/* 1 pixel gives 4 */
			ikwindow( (0)*4, 4063+25 );
		} else if ( npts <= 256 )  {
			ikzoom( 1, 1 );		/* 1 pixel gives 2 */
			ikwindow( (0)*4, 4063+17 );
		}
	}
}

l3eol()
{
		pchar( '.' );		/* End of scanline */
		last_solidp = SOLTAB_NULL;
		ntomiss = 0;
}

/*
 *  			D E V _ E O L
 *  
 *  This routine is called by main when the end of a scanline is
 *  reached.
 */
dev_eol()
{
	if( outfd > 0 )  {
		write( outfd, (char *)scanline, scanbytes );
		bzero( (char *)scanline, scanbytes );
		pixelp = &scanline[0];
	}
}

/*
 *  Called when the picture is finally done.
 */
l3end()
{
	fprintf( outfp, "/\n" );	/* end of view */
	fflush( outfp );
}

/*
 *  			P K N U M
 *  
 *  Oddball 5-bits in a char ('@', 'A', ... on up) number packing.
 *  Number is written 5 bits at a time, right to left (low to high)
 *  until there are no more non-zero bits remaining.
 */
pknum( arg )
int arg;
{
	register long i = arg;

	do {
		pchar( '@'+(i & 037) );
		i >>= 5;
	} while( i > 0 );
}

/*
 *  			V I E W _ I N I T
 */
view_init( ap )
register struct application *ap;
{
	/* Initialize the application selected */
	ap->a_hit = ap->a_miss = nullf;	/* ?? */
	ap->a_init = ap->a_eol = ap->a_end = nullf;

	switch( lightmodel )  {
	case 0:
	case 1:
	case 2:
		ap->a_hit = viewit;
		ap->a_miss = wbackground;
		ap->a_eol = dev_eol;
		view_only = 1;
		break;
	case 3:
		ap->a_hit = l3hit;
		ap->a_miss = l3miss;
		ap->a_init = l3init;
		ap->a_end = l3end;
		ap->a_eol = l3eol;
		view_only = 1;
		break;
	case 4:
		ap->a_hit = l4hit;
		ap->a_miss = nullf;
		ap->a_eol = nullf;
		break;
	default:
		bomb("bad lighting model #");
	}

	if( lightmodel == 3 || lightmodel == 4 )
		if( outfd > 0 )
			outfp = fdopen( outfd, "w" );
		else
			bomb("No output file specified");
}
