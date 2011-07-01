/*                      V F O N T - I F . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @addtogroup vfont */
/** @ingroup io */
/** @{ */
/** @file vfont-if.h
 *
 * This header file describes the in-memory format used by the BRL-CAD
 * Package routines for manipulating fonts stored in the Berkeley
 * VFONT format.
 *
 * Note that the VFONT files are in the format found on a VAX -- no
 * conversion has been applied.
 *
 * Merely TARing or RCPing the VAX /usr/lib/vfont directory onto any
 * machine suffices to install the fonts.
 *
 * The VAX format of the fonts is invisible to software actually using
 * the fonts, except to be aware that bit zero in a byte of font data
 * is on the right hand side (lsb).
 *
 * The VAX declaration of the file is:
 *
 *	struct header {
 *		short magic;
 *		unsigned short size;
 *		short maxx;
 *		short maxy;
 *		short xtend;
 *	};
 *	struct dispatch {
 *		unsigned short addr;
 *		short nbytes;
 *		char up, down, left, right;
 *		short width;
 *	};
 *	char bits[header.size];
 *
 * The char fields up, down, left, and right in the VAX-version
 * of struct dispatch are signed.  Use the SXT macro to extend the sign.
 *
 * The actual bits array has the upper left corner of the bitmap in
 * the first byte.  Bits are scanned out of the bytes in a
 * left-to-right, top-to-bottom order (most decidedly non-VAX style).
 * Never seems to be any consistency in data formats.
 *
 */

#include "bu.h"

#define SXT(c) ((c)|((c&0x80)?(~0xFF):0))

struct vfont_dispatch  {
    unsigned short vd_addr;
    short vd_nbytes;
    short vd_up;
    short vd_down;
    short vd_left;
    short vd_right;
    short vd_width;
};
struct vfont {
    short vf_maxx;
    short vf_maxy;
    short vf_xtend;
    struct vfont_dispatch vf_dispatch[256];
    char *vf_bits;
};
#define VFONT_NULL ((struct vfont *)NULL)

/** @file vfont.c
 *
 * @brief Berkely Vector Fonts
 *
 * Provide a machine-independent interface to files containing
 * Berkeley VFONT format fonts, stored with VAX byte ordering and word
 * alignment.
 *
 */

/**
 * V F O N T _ G E T
 *
 * Fetch the named font, and return a struct vfont pointer.
 *
 * First the filename provided is used, then the BRL-CAD font
 * directory is searched (for places where "system" directories are
 * considered sacred), and then finally the ordinary font directory is
 * searched.
 *
 * The font files are treated as pure byte streams, and are expected
 * to be in VAX order.
 *
 * VFONT_NULL is returned on error.  On ordinary errors, the function
 * is silent.  On extraordinary errors, a remark is placed on stderr.
 */
BU_EXPORT extern struct vfont *vfont_get(char *font);

/**
 * V F O N T _ F R E E
 *
 * Return the storage associated with a struct vfont
 */
BU_EXPORT extern void vfont_free(struct vfont *font);

/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
