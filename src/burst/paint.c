/*                         P A I N T . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 *
 */
/** @file paint.c
	Author:		Gary S. Moss
			U. S. Army Ballistic Research Laboratory
			Aberdeen Proving Ground
			Maryland 21005-5066
*/
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#include <assert.h>
#include <stdio.h>

#include "common.h"

#include "./vecmath.h"
#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "fb.h"

#include "./extern.h"


#define DEBUG_CELLFB	0
#define DEBUG_SPALLFB	0

/* Offset frame buffer coordinates to center of cell from grid lines. */
#define CenterCell(b)	(b += roundToInt( (fastf_t) zoom / 2.0 ))

/* Compare one RGB pixel to another. */
#define	SAMERGB(a,b)	((a)[RED] == (b)[RED] &&\
			 (a)[GRN] == (b)[GRN] &&\
			 (a)[BLU] == (b)[BLU])

static unsigned char	pixbuf[MAXDEVWID][3];
static int	gridxmargin;
static int	gridymargin;

void
gridToFb( gx, gy, fxp, fyp )
int	gx, gy;
int	*fxp, *fyp;
	{
	/* map grid offsets to frame buffer coordinates */
	*fxp = (gx - gridxorg)*zoom + gridxmargin;
	*fyp = (gy - gridyorg)*zoom + gridymargin;
	return;
	}

void
paintGridFb()
{
		register int	fx, fy;
		int		fxbeg, fybeg;
		int		fxfin, fyfin;
		int		fxorg, fyorg;
		int		fgridwid;
	if( fb_clear( fbiop, pixbkgr ) == -1 )
		return;
	gridxmargin = (devwid - (gridwidth*zoom)) / 2.0;
	gridymargin = (devhgt - (gridheight*zoom)) / 2.0;
	gridToFb( gridxorg, gridyorg, &fxbeg, &fybeg );
	gridToFb( gridxfin+1, gridyfin+1, &fxfin, &fyfin );
	gridToFb( 0, 0, &fxorg, &fyorg );
	CenterCell( fxorg ); /* center of cell */
	CenterCell( fyorg );
	if( zoom == 1 )
		{
		fgridwid = gridwidth + 2;
		fxfin++;
		}
	else
		fgridwid = gridwidth * zoom + 1;

	/* draw vertical lines */
	for( fx = 1; fx < fgridwid; fx++ )
		COPYRGB( &pixbuf[fx][0], pixbkgr );
	for( fx = fxbeg; fx <= fxfin; fx += zoom )
		COPYRGB( &pixbuf[fx-fxbeg][0], &pixgrid[0] );
	for( fy = fybeg; fy <= fyfin; fy++ )
		(void) fb_write( fbiop, fxbeg, fy, (unsigned char *)pixbuf, fgridwid );
	for( fy = 0; fy < devwid; fy++ )
		(void) fb_write( fbiop, fxorg, fy, pixaxis, 1 );

	/* draw horizontal lines */
	if( zoom > 1 )
		for( fy = fybeg; fy <= fyfin; fy += zoom )
			(void) fb_write( fbiop,
					 fxbeg, fy, pixgrid, fgridwid );
		for( fx = 0; fx < devwid; fx++ )
			COPYRGB( &pixbuf[fx][0], pixaxis );
	(void) fb_write( fbiop, 0, fyorg, (unsigned char *)pixbuf, devwid );
	return;
	}

void
paintCellFb( ap, pixpaint, pixexpendable )
register struct application	*ap;
unsigned char			*pixpaint;
unsigned char			*pixexpendable;
	{	int		gx, gy;
		register int	gyfin, gxfin;
		register int	gxorg, gyorg;
		register int	x, y;
		int		cnt;
#if DEBUG_CELLFB
	brst_log( "paintCellFb: expendable {%d,%d,%d}\n",
		pixexpendable[RED],
		pixexpendable[GRN],
		pixexpendable[BLU] );
#endif
	gridToFb( ap->a_x, ap->a_y, &gx, &gy );
	gxorg = gx+1;
	gyorg = gy+1;
	gxfin = zoom == 1 ? gx+zoom+1 : gx+zoom;
	gyfin = zoom == 1 ? gy+zoom+1 : gy+zoom;
	cnt = gxfin - gxorg;
	for( y = gyorg; y < gyfin; y++ )
		{
		if( zoom != 1 && (y - gy) % zoom == 0 )
			continue;
		bu_semaphore_acquire( RT_SEM_STATS );
		(void) fb_read( fbiop, gxorg, y, (unsigned char *)pixbuf, cnt );
		bu_semaphore_release( RT_SEM_STATS );
		for( x = gxorg; x < gxfin; x++ )
			{
			if( SAMERGB( &pixbuf[x-gxorg][0], pixexpendable )
				)
				{
#if DEBUG_CELLFB
				brst_log( "Clobbering:<%d,%d>{%d,%d,%d}\n",
					x, y,
					pixbuf[x-gxorg][RED],
					pixbuf[x-gxorg][GRN],
					pixbuf[x-gxorg][BLU] );
#endif
				COPYRGB( &pixbuf[x-gxorg][0], pixpaint );
				}
#if DEBUG_CELLFB
			else
				brst_log( "Preserving:<%d,%d>{%d,%d,%d}\n",
					x, y,
					pixbuf[x-gxorg][RED],
					pixbuf[x-gxorg][GRN],
					pixbuf[x-gxorg][BLU] );
#endif
			}
		bu_semaphore_acquire( RT_SEM_STATS );
		(void) fb_write( fbiop, gxorg, y, (unsigned char *)pixbuf, cnt );
		bu_semaphore_release( RT_SEM_STATS );
#if DEBUG_CELLFB
		brst_log( "paintCellFb: fb_write(%d,%d)\n", x, y );
#endif
		}
	return;
	}

void
paintSpallFb( ap )
register struct application	*ap;
	{	unsigned char pixel[3];
		int x, y;
		int err;
		fastf_t	celldist;
#if DEBUG_SPALLFB
	brst_log( "paintSpallFb: a_x=%d a_y=%d a_cumlen=%g cellsz=%g zoom=%d\n",
		ap->a_x, ap->a_y, ap->a_cumlen, cellsz, zoom );
#endif
	pixel[RED] = ap->a_color[RED] * 255;
	pixel[GRN] = ap->a_color[GRN] * 255;
	pixel[BLU] = ap->a_color[BLU] * 255;
	gridToFb( ap->a_x, ap->a_y, &x, &y );
	CenterCell( x );	/* center of cell */
	CenterCell( y );
	celldist = ap->a_cumlen/cellsz * zoom;
	x = roundToInt( x + Dot( ap->a_ray.r_dir, gridhor ) * celldist );
	y = roundToInt( y + Dot( ap->a_ray.r_dir, gridver ) * celldist );
	bu_semaphore_acquire( RT_SEM_STATS );
	err = fb_write( fbiop, x, y, pixel, 1 );
	bu_semaphore_release( RT_SEM_STATS );
#if DEBUG_SPALLFB
	brst_log( "paintSpallFb:gridhor=<%g,%g,%g> gridver=<%g,%g,%g>\n",
		gridhor[X], gridhor[Y], gridhor[Z],
		gridver[X], gridver[Y], gridver[Z] );
	brst_log( "paintSpallFb:fb_write(x=%d,y=%d,pixel={%d,%d,%d})\n",
		x, y,
		(int) pixel[RED],
		(int) pixel[GRN],
		(int) pixel[BLU]
		);
#endif
	if( err == -1 )
		brst_log( "Write failed to pixel <%d,%d> from cell <%d,%d>.\n",
			x, y, ap->a_x, ap->a_y );
	return;
	}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
