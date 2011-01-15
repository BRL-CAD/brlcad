/*                       G E T F O N T . C
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
/** @file getfont.c
 *
 * Load a new font by reading in the header and directory.
 *
 */

#include "common.h"

#include <stdio.h>

#include "fb.h"

#include "./vfont.h"


struct vfont
get_font(const char* fontname, void (*vfont_log)(const char *fmt, ...))
{
    struct vfont font;
    struct header lochdr;
    static char	fname[FONTNAMESZ];

    if (fontname == NULL)
	fontname = FONTNAME;

    if (fontname[0] != '/') {
	/* absolute path */
	const char *vfont = bu_brlcad_data("vfont", 1);
	if (vfont)
	    snprintf(fname, FONTNAMESZ, "%s/%s", vfont, fontname);
	else
	    bu_strlcpy(fname, fontname, sizeof(fname));
    } else
	bu_strlcpy(fname, fontname, sizeof(fname));
    
    /* Open the file and read in the header information. */
    font.ffdes = fopen(fname, "rb");
    if (font.ffdes == NULL) {
	if (vfont_log)
	    vfont_log("Error opening font file '%s'\n", fname);
	    
	font.ffdes = NULL;
	return font;
    }

    if (fread((char *)&lochdr, (int)sizeof(struct header), 1, font.ffdes) != 1) {
	if (vfont_log)
	    vfont_log("get_Font() read failed!\n");
	font.ffdes = NULL;
	return font;
    }

    SWAB(lochdr.magic);
    SWAB(lochdr.size);
    SWAB(lochdr.maxx);
    SWAB(lochdr.maxy);
    SWAB(lochdr.xtend);
    
    if (lochdr.magic != 0436) {
	if (vfont_log)
	    vfont_log("Not a font file \"%s\": magic=0%o\n", fname, (int)lochdr.magic);
	font.ffdes = NULL;
	return font;
    }
    font.hdr = lochdr;
    
    /* Read in the directory for the font. */
    if (fread((char *) font.dir, (int)sizeof(struct dispatch), 256, font.ffdes) != 256) {
	if (vfont_log)
	    vfont_log("get_Font() read failed!\n");
	font.ffdes = NULL;
	return font;
    }

    /* Addresses of characters in the file are relative to point in
     * the file after the directory, so grab the current position.
     */
    font.offset = ftell(font.ffdes);

    return font;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
