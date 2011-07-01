/*                         V F O N T . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
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
 */
/** @file vfont.h
 *
 * Header file for putting fonts up.
 *
 */

#ifndef VFONT_FONT_H
#define VFONT_FONT_H

#include "common.h"

#include "bu.h"

#define SWAB(shrt)	if (bu_byteorder() == BU_BIG_ENDIAN) (shrt=(((shrt)>>8) & 0xff) | (((shrt)<<8) & 0xff00))
#define SWABV(shrt)	(bu_byteorder() == BU_BIG_ENDIAN) ? ((((shrt)>>8) & 0xff) | (((shrt)<<8) & 0xff00)) : (shrt)

#define FONTBUFSZ 200
#define FONTNAME	"times.r.6"		/* Default font name.	*/
#define FONTNAMESZ	128

#define SIGNBIT		(1<<7)
#define SIGNMASK	~SIGNBIT
#define TWOSCOMP(chr)	((~(chr)&0xff)+1)

#if defined(mips)
#  define CHARS_UNSIGNED_ONLY
#endif
#ifdef CHARS_UNSIGNED_ONLY
#  define SignedChar(chr)	(((chr)&SIGNBIT) ? -TWOSCOMP(chr) : (chr))
#else
#  define SignedChar(chr)	chr
#endif

/* The structures header and dispatch define the format of a font file. */
struct header {
    short		magic;
    unsigned short	size;
    short		maxx;
    short		maxy;
    short		xtend;
};

struct dispatch {
    unsigned short	addr;
    short		nbytes;
    char up, down, left, right;
    short		width;
};

struct vfont {
    FILE		*ffdes;		/* File pointer for current font.	*/
    long		offset;		/* Current offset to character data.	*/
    struct header	hdr;		/* Header for font file.		*/
    struct dispatch	dir[256];	/* Directory for character font.	*/
    int			width;		/* Current character width.		*/
    int			height;		/* Current character height.		*/
};


/**
 * get_font validates and loads the specified fontname, logging any
 * error messages via the provided log callback.
 *
 * the ffdes field of the returned struct vfont indicates whether the
 * load was successful or not.
 */
struct vfont
get_font(const char* fontname, void (*vfont_log)(const char *fmt, ...));


#endif /* VFONT_FONT_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
