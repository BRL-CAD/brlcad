/*                         V F O N T . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2006 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

/** \addtogroup libbu */
/*@{*/
/** @file vfont.c
 *	Provide a machine-independent interface to files containing
 *	Berkeley VFONT format fonts, stored with VAX byte ordering
 *	and word alignment.
 *
 *  Author -
 *	Michael John Muuss
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *
 */
/*@}*/

#ifndef lint
static const char libbu_vfont_RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#include <stdio.h>
#include "machine.h"
#include "vfont-if.h"
#include "bu.h"

#define FONTDIR2	"/usr/lib/vfont"
#define DEFAULT_FONT	"nonie.r.12"
#define FONTNAMESZ	128

/*
 * Forward Definitions
 */
static int vax_gshort(unsigned char *);

/*
 *			V F O N T _ G E T
 *
 *  Fetch the named font, and return a struct vfont pointer.
 *
 *  First the filename provided is used, then the BRL-CAD font
 *  directory is searched (for places where "system" directories
 *  are considered sacred), and then finally the ordinary
 *  font directory is searched.
 *
 *  The font files are treated as pure byte streams, and are expected
 *  to be in VAX order.
 *
 *  VFONT_NULL is returned on error.  On ordinary errors, the function
 *  is silent.  On extraordinary errors, a remark is placed on stderr.
 */
struct vfont *
vfont_get(char *font)
{
	register struct vfont	*vfp = VFONT_NULL;
	register FILE		*fp = NULL;
	register int	i;
	char		fname[FONTNAMESZ];
	unsigned char	header[2*5];		/* 5 16-bit vax shorts */
	unsigned char	dispatch[10*256];	/* 256 10-byte structs */
	int		magic;
	int		size;

	if( font == NULL )
		font = DEFAULT_FONT;

	/* Open the file and read in the header information. */
	if( (fp = fopen( font, "r" )) == NULL )  {
		sprintf( fname, "%s/%s", (char *)bu_brlcad_data("vfont", 0), font );
		if( (fp = fopen( fname, "r" )) == NULL )  {
			sprintf( fname, "%s/%s", FONTDIR2, font );
			if( (fp = fopen( fname, "r" )) == NULL )  {
				return(VFONT_NULL);
			}
		}
	}
	if( fread( (char *)header, sizeof(header), 1, fp ) != 1 ||
	    fread( (char *)dispatch, sizeof(dispatch), 1, fp ) != 1 )  {
		fprintf(stderr, "vfont_get(%s):  header read error\n", fname );
	    	fclose(fp);
	    	return(VFONT_NULL);
	}
	magic = vax_gshort( &header[0*2] ) & 0xFFFF;
	size = vax_gshort( &header[1*2] ) & 0xFFFF;	/* unsigned short */

	if( magic != 0436 )  {
		fprintf(stderr, "vfont_get(%s):  bad magic number 0%o\n",
			fname, magic );
		fclose(fp);
		return(VFONT_NULL);
	}

	if( (vfp = (struct vfont *)bu_malloc(sizeof(struct vfont), "vfont")) == VFONT_NULL )  {
		fprintf(stderr,"vfont_get(%s):  malloc failure 1\n", fname );
		fclose(fp);
		return(VFONT_NULL);
	}

	/* Read in the bit maps */
	if( (vfp->vf_bits = (char *)bu_malloc(size, "vfont bits")) == (char *)0 )  {
		fprintf(stderr,"vfont_get(%s):  malloc failure 2 (%d)\n",
			fname, size);
		fclose(fp);
		bu_free( (char *)vfp, "vfont");
		return(VFONT_NULL);
	}
	if( fread( vfp->vf_bits, size, 1, fp ) != 1 )  {
		fprintf(stderr,"vfont_get(%s):  bitmap read error\n", fname );
		fclose(fp);
		bu_free( vfp->vf_bits, "vfont bits" );
		bu_free( (char *)vfp, "vfont" );
		return(VFONT_NULL);
	}

	/*
	 *  Convert VAX data in header[] and dispatch[] arrays to
	 *  native machine form.
	 */
	vfp->vf_maxx = vax_gshort( &header[2*2] );
	vfp->vf_maxy = vax_gshort( &header[3*2] );
	vfp->vf_xtend = vax_gshort( &header[4*2] );

	for( i=0; i<255; i++ )  {
		register struct vfont_dispatch	*vdp = &(vfp->vf_dispatch[i]);
		register unsigned char		*cp = &dispatch[i*10];

		vdp->vd_addr = vax_gshort( &cp[0] );
		vdp->vd_nbytes = vax_gshort( &cp[2] );
		vdp->vd_up = SXT( cp[4] );
		vdp->vd_down = SXT( cp[5] );
		vdp->vd_left = SXT( cp[6] );
		vdp->vd_right = SXT( cp[7] );
		vdp->vd_width = vax_gshort( &cp[8] );
	}
	fclose(fp);
	return(vfp);
}

/*
 *			V A X _ G S H O R T
 *
 *  Obtain a 16-bit signed integer from two adjacent characters,
 *  stored in VAX order, regardless of word alignment.
 */
static int
vax_gshort(unsigned char *msgp)
{
	register unsigned char *p = (unsigned char *) msgp;
	register int	i;

	if( (i = (p[1] << 8) | p[0]) & 0x8000 )
		return(i | ~0xFFFF);	/* Sign extend */
	return(i);
}

/*
 *			V F O N T _ F R E E
 *
 *  Return the storage associated with a struct vfont
 */
void
vfont_free(register struct vfont *vfp)
{
	bu_free( vfp->vf_bits, "vfont bits" );
	bu_free( (char *)vfp, "vfont" );
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
