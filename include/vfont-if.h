/*                      V F O N T - I F . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2006 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
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
/** @addtogroup vfont */
/*@{*/
/** @file vfont-if.h
 *
 *  This header file describes the in-memory format used by
 *  the BRL-CAD Package routines for manipulating fonts stored
 *  in the Berkeley VFONT format.
 *  Note that the VFONT files are in the format found on a VAX --
 *  no conversion has been applied.
 *  Merely TARing or RCPing the VAX /usr/lib/vfont directory onto
 *  any machine suffices to install the fonts.
 *  The VAX format of the fonts is invisible to software actually
 *  using the fonts, except to be aware that bit zero in a byte of
 *  font data is on the right hand side (lsb).
 *
 *  The VAX declaration of the file is:
 *
 *	struct header {
 *		short		magic;
 *		unsigned short	size;
 *		short		maxx;
 *		short		maxy;
 *		short		xtend;
 *	};
 *	struct dispatch {
 *		unsigned short	addr;
 *		short		nbytes;
 *		char		up, down, left, right;
 *		short		width;
 *	};
 *	char	bits[header.size];
 *
 *  The char fields up, down, left, and right in the VAX-version
 *  of struct dispatch are signed.  Use the SXT macro to extend the sign.
 *
 *  The actual bits array has the upper left corner of the bitmap in
 *  the first byte.  Bits are scanned out of the bytes in a
 *  left-to-right, top-to-bottom order (most decidedly non-VAX style).
 *  Never seems to be any consistency in data formats.
 *
 *  @author
 *	Michael John Muuss
 *
 *  @par Source
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *
 *  $Header$
 */

#define	SXT(c)		((c)|((c&0x80)?(~0xFF):0))

struct vfont_dispatch  {
	unsigned short	vd_addr;
	short		vd_nbytes;
	short		vd_up;
	short		vd_down;
	short		vd_left;
	short		vd_right;
	short		vd_width;
};
struct vfont {
	short	vf_maxx;
	short	vf_maxy;
	short	vf_xtend;
	struct vfont_dispatch	vf_dispatch[256];
	char	*vf_bits;
};
#define	VFONT_NULL	((struct vfont *)NULL)

extern struct vfont	*vfont_get();
extern void		vfont_free();
/*@}*/
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

