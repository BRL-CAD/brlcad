/*                          F O N T . C
 * BRL-CAD
 *
 * Copyright (C) 2004-2005 United States Government as represented by
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

/** \addtogroup libbn */
/*@{*/
/** @file font.c
 *
 *  Author -
 *	Michael John Muuss
 *	John Anderson
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 */
/*@}*/

#ifndef lint
static const char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include "common.h"



#include <stdio.h>
#include <math.h>
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include "machine.h"
#include "vmath.h"
#include "bu.h"
#include "bn.h"
#include "vectfont.h"

/* References tp_xxx symbols from libbn/vectfont.c */

/*
 *			B N _ V L I S T _ 3 S T R I N G
 *
 *  'scale' is the width, in mm, of one character.
 */
void
bn_vlist_3string(struct bu_list *vhead, struct bu_list *free_hd, const char *string, const vect_t origin, const mat_t rot, double scale)
              	       
              	         	/* source of free vlists */
          	        	/* string of chars to be plotted */
             	       		/* lower left corner of 1st char */
           	    		/* Transform matrix (WARNING: may xlate) */
      		      		/* scale factor to change 1x1 char sz */
{
	register unsigned char *cp;
	double	offset;			/* offset of char from given x,y */
	int	ysign;			/* sign of y motion, either +1 or -1 */
	vect_t	temp;
	vect_t	loc;
	mat_t	xlate_to_origin;
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
	MAT_IDN( xlate_to_origin );
	MAT_DELTAS( xlate_to_origin,	origin[X], origin[Y], origin[Z] );
	bn_mat_mul( mat, xlate_to_origin, rot );

	/* Check to see if initialization is needed */
	if( tp_cindex[040] == 0 )  tp_setup();

	/* Draw each character in the input string */
	offset = 0;
	for( cp = (unsigned char *)string ; *cp; cp++, offset += scale )  {
		register TINY	*p;	/* pointer to stroke table */
		register int	stroke;

		VSET( temp, offset, 0, 0 );
		MAT4X3PNT( loc, mat, temp );
		BN_ADD_VLIST(free_hd, vhead, loc, BN_VLIST_LINE_MOVE );

		for( p = tp_cindex[*cp]; ((stroke= *p)) != LAST; p++ )  {
			int	draw;

			if( (stroke)==NEGY )  {
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
				BN_ADD_VLIST( free_hd, vhead, loc, BN_VLIST_LINE_DRAW );
			} else {
				BN_ADD_VLIST( free_hd, vhead, loc, BN_VLIST_LINE_MOVE );
			}
		}
	}
}


/*
 *			B N _ V L I S T _ 2 S T R I N G
 *
 *  A simpler interface, for those cases where the text lies
 *  in the X-Y plane.
 */
void
bn_vlist_2string(struct bu_list *vhead, struct bu_list *free_hd, const char *string, double x, double y, double scale, double theta)
              	       
              	         	/* source of free vlists */
          	        	/* string of chars to be plotted */
      	  			/* x,y of lower left corner of 1st char */
      	  
      	      			/* scale factor to change 1x1 char sz */
      	      			/* degrees ccw from X-axis */
{
	mat_t	mat;
	vect_t	p;

	bn_mat_angles( mat, 0.0, 0.0, theta );
	VSET( p, x, y, 0 );
	bn_vlist_3string( vhead, free_hd, string, p, mat, scale );
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
