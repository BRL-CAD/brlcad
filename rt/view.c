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
#include "ray.h"
#include "debug.h"

#define XOFFSET	20
#define YOFFSET 80

vect_t l0color = { 128, 128, 255 };

viewit( segp, rayp, xscreen, yscreen )
struct seg *segp;
struct ray *rayp;
int xscreen, yscreen;
{
	long inten;
	float diffuse1, cosI1;
	float diffuse0, cosI0;
	static vect_t work;
	static int r,g,b;

	/* Really ought to find "nearest" intersection, but gap() should
	 *  do that for us.  Just use top of list. */
	if( !(segp->seg_flag & SEG_IN) )  {
		printf("viewit:  No entry point\n");
		return;
	}

	/*
	 * Diffuse reflectance from each light source
	 */
	diffuse1 = 0;
#ifdef later
	if( (cosI1 = -VDOT(segp->seg_in.hit_normal, l1vec)) >= 0.0 )
		diffuse1 = cosI1 * 0.5;	/* % from this src */
#endif

	diffuse0 = 0;		/* From EYE */
	if( (cosI0 = -VDOT(segp->seg_in.hit_normal, rayp->r_dir)) >= 0.0 )  {
		diffuse0 = cosI0 * 1.0;		/* % from this src */
	}

	if(debug&DEBUG_HITS)  {
		if( segp->seg_flag & SEG_IN )
			hit_print( " In", &segp->seg_in );
#ifdef never
		if( segp->seg_flag & SEG_OUT )
			hit_print( "Out", &segp->seg_out );
#endif never
		printf("cosI0=%f, diffuse0=%f   ", cosI0, diffuse0 );
	}

#ifdef notyet
	/* Specular reflectance from first light source */
	/* reflection = (2 * cos(i) * NormalVec) - IncidentVec */
	/* cos(s) = -VDOT(reflection, r_dir) = cosI0 */
	f = 2 * cosI1;
	VSCALE( work, segp->seg_in.hit_normal, f );
	VSUB2( reflection, work, l1vec );
	if( not_shadowed && cosI0 > cosAcceptAngle )
		/* Do specular return */;
#endif notyet

	VSCALE( work, l0color, diffuse0 );
	if(debug&DEBUG_HITS)  VPRINT("RGB", work);

	r = work[0];
	g = work[1];
	b = work[2];
	if( r > 255 ) r = 255;
	if( g > 255 ) g = 255;
	if( b > 255 ) b = 255;
	if( r<0 || g<0 || b<0 )  {
		VPRINT("@@ Negative RGB @@", work);
		inten = 0x0080FF80;
	} else {
		inten = (b << 16) |		/* B */
			(g <<  8) |		/* G */
			(r);			/* R */
	}

	ikwpixel( xscreen+XOFFSET, yscreen+YOFFSET, inten);
}

hit_print( str, hitp )
char *str;
register struct hit *hitp;
{
	printf("** %s HIT, dist=%f\n", str, hitp->hit_dist );
	VPRINT("** Point ", hitp->hit_point );
	VPRINT("** Normal", hitp->hit_normal );
}

wbackground( x, y )
int x, y;
{
	ikwpixel( x+XOFFSET, y+YOFFSET, 0x00808080 );		/* Grey */
}
