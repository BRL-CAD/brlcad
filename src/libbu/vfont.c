/*                         V F O N T . C
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

#include "common.h"

#include <stdio.h>
#include "vfont-if.h"
#include "bu.h"

#define FONTDIR2 "/usr/lib/vfont"
#define DEFAULT_FONT "nonie.r.12"
#define FONTNAMESZ 128


/**
 * _ V A X _ G S H O R T
 *
 * Obtain a 16-bit signed integer from two adjacent characters, stored
 * in VAX order, regardless of word alignment.
 */
HIDDEN int
_vax_gshort(unsigned char *msgp)
{
    register unsigned char *p = (unsigned char *) msgp;
    register int i;

    if ((i = (p[1] << 8) | p[0]) & 0x8000)
	return i | ~0xFFFF;	/* Sign extend */
    return i;
}


struct vfont *
vfont_get(char *font)
{
    register struct vfont *vfp = VFONT_NULL;
    register FILE *fp = NULL;
    register int i;
    char fname[FONTNAMESZ];
    unsigned char header[2*5];		/* 5 16-bit vax shorts */
    unsigned char dispatch[10*256];	/* 256 10-byte structs */
    int magic;
    int size;

    if (font == NULL)
	font = DEFAULT_FONT;

    /* Open the file and read in the header information. */
    if ((fp = fopen(font, "rb")) == NULL) {
	snprintf(fname, FONTNAMESZ, "%s/%s", (char *)bu_brlcad_data("vfont", 0), font);
	if ((fp = fopen(fname, "rb")) == NULL) {
	    snprintf(fname, FONTNAMESZ, "%s/%s", FONTDIR2, font);
	    if ((fp = fopen(fname, "rb")) == NULL) {
		return VFONT_NULL;
	    }
	}
    }
    if (fread((char *)header, sizeof(header), 1, fp) != 1 ||
	fread((char *)dispatch, sizeof(dispatch), 1, fp) != 1) {
	fprintf(stderr, "vfont_get(%s):  header read error\n", fname);
	fclose(fp);
	return VFONT_NULL;
    }
    magic = _vax_gshort(&header[0*2]) & 0xFFFF;
    size = _vax_gshort(&header[1*2]) & 0xFFFF;	/* unsigned short */

    if (UNLIKELY(magic != 0436)) {
	fprintf(stderr, "vfont_get(%s):  bad magic number 0%o\n",
		fname, magic);
	fclose(fp);
	return VFONT_NULL;
    }

    /* Read in the bit maps */
    vfp = (struct vfont *)bu_malloc(sizeof(struct vfont), "vfont");
    vfp->vf_bits = (char *)bu_malloc((size_t)size, "vfont bits");
    if (fread(vfp->vf_bits, (size_t)size, 1, fp) != 1) {
	fprintf(stderr, "vfont_get(%s):  bitmap read error\n", fname);
	fclose(fp);
	bu_free(vfp->vf_bits, "vfont bits");
	bu_free((char *)vfp, "vfont");
	return VFONT_NULL;
    }

    /*
     * Convert VAX data in header[] and dispatch[] arrays to native
     * machine form.
     */
    vfp->vf_maxx = _vax_gshort(&header[2*2]);
    vfp->vf_maxy = _vax_gshort(&header[3*2]);
    vfp->vf_xtend = _vax_gshort(&header[4*2]);

    for (i=0; i<255; i++) {
	register struct vfont_dispatch *vdp = &(vfp->vf_dispatch[i]);
	register unsigned char *cp = &dispatch[i*10];

	vdp->vd_addr = _vax_gshort(&cp[0]);
	vdp->vd_nbytes = _vax_gshort(&cp[2]);
	vdp->vd_up = SXT(cp[4]);
	vdp->vd_down = SXT(cp[5]);
	vdp->vd_left = SXT(cp[6]);
	vdp->vd_right = SXT(cp[7]);
	vdp->vd_width = _vax_gshort(&cp[8]);
    }
    fclose(fp);
    return vfp;
}


void
vfont_free(register struct vfont *vfp)
{
    bu_free(vfp->vf_bits, "vfont bits");
    bu_free((char *)vfp, "vfont");
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
