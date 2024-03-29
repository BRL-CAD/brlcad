/*                           P P M . C
 * BRL-CAD
 *
 * Copyright (c) 2013-2024 United States Government as represented by
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
/** @file ppm.c
 *
 * This file contains reading and writing routines for the Utah RLE format.
 *
 */

#include "common.h"
#include <string.h>
#include "rle.h"

#include "bu/log.h"
#include "bu/malloc.h"
#include "vmath.h"
#include "icv_private.h"

typedef struct {
    unsigned short cm_red[256];
    unsigned short cm_green[256];
    unsigned short cm_blue[256];
} ColorMap;

int
rle_write(icv_image_t *bif, const char *filename)
{
    if (!bif || !filename)
	return BRLCAD_ERROR;

    return BRLCAD_OK;
}

icv_image_t*
rle_read(const char *filename)
{
    FILE *fp = NULL;
    if (filename == NULL) {
	fp = stdin;
	setmode(fileno(fp), O_BINARY);
    } else if ((fp = fopen(filename, "r")) == NULL) {
	bu_log("ERROR: Cannot open file for reading\n");
	return NULL;
    }

    // Older BRL-CAD code used the default header from libutahrle.
    // Globals are usually bad news, so we'll start out with a local
    // version with the same defaults.
    int bg_color[3] = {0, 0, 0};
    rle_hdr rle_icv_hdr = { RUN_DISPATCH, 3, bg_color, 0, 2, 0, 511, 0, 511, 0, 8, NULL, NULL, 0, {7}, 0L, "libicv", "no file", 0, {{0}}};
    rle_icv_hdr.rle_file = fp;
    if (rle_get_setup(&rle_icv_hdr) < 0) {
	bu_log("rle_read: Error reading setup information\n");
	if (fp != stdin)
	    fclose(fp);
	return NULL;
    }

    /* Only interested in R, G, & B
     * TODO - icv, unlike the old rle-pix, should be able to handle
     * more here - initial revival of the code is trying to to change
     * things until we get the old functionality working, but this
     * should be revisited. */
    RLE_CLR_BIT(rle_icv_hdr, RLE_ALPHA);
    for (int i = 3; i < rle_icv_hdr.ncolors; i++)
	RLE_CLR_BIT(rle_icv_hdr, i);
    int iwidth = rle_icv_hdr.xmax + 1;
    int iheight = rle_icv_hdr.xmax + 1;

    double *data = (double *)bu_calloc(iwidth * iheight * 3, sizeof(double), "image data");

    // Set bg color
    if (rle_icv_hdr.bg_color) {
	for (int i = 0; i < (rle_icv_hdr.xmax + 1) * (rle_icv_hdr.ymax + 1); i++) {
	    data[3*i+0] = (double)rle_icv_hdr.bg_color[0]/(double)255.0;
	    data[3*i+1] = (double)rle_icv_hdr.bg_color[1]/(double)255.0;
	    data[3*i+2] = (double)rle_icv_hdr.bg_color[2]/(double)255.0;
	}
    }

    int ncolors = rle_icv_hdr.ncolors > 3 ? 3 : rle_icv_hdr.ncolors;
    int file_width = rle_icv_hdr.xmax - rle_icv_hdr.xmin + 1;
    unsigned char *rows[4];
    int mi = 0;
    for (mi=0; mi < ncolors; mi++)
	rows[mi] = (unsigned char *)bu_malloc((size_t)file_width, "row[]");
    for (; mi < 3; mi++)
	rows[mi] = rows[0];      /* handle monochrome images */

    /*
     * Import Utah color map, converting to libfb format.
     * Check for old format color maps, where high 8 bits
     * were zero, and correct them.
     * XXX need to handle < 3 channels of color map, by replication.
     */
    ColorMap cmap;
    if (rle_icv_hdr.ncmap && rle_icv_hdr.ncmap > 0) {
	int maplen = (1 << rle_icv_hdr.cmaplen);
	int all = 0;
	for (int i = 0; i < 256; i++) {
	    cmap.cm_red[i] = rle_icv_hdr.cmap[i];
	    cmap.cm_green[i] = rle_icv_hdr.cmap[i+maplen];
	    cmap.cm_blue[i] = rle_icv_hdr.cmap[i+2*maplen];
	    all |= cmap.cm_red[i] | cmap.cm_green[i] | cmap.cm_blue[i];
	}
	if ((all & 0xFF00) == 0 && (all & 0x00FF) != 0) {
	    /* This is an old (Edition 2) color map.
	     * Correct by shifting it left 8 bits.
	     */
	    for (int i = 0; i < 256; i++) {
		cmap.cm_red[i] <<= 8;
		cmap.cm_green[i] <<= 8;
		cmap.cm_blue[i] <<= 8;
	    }
	    bu_log("rle_read: correcting for old style colormap\n");
	}
    }

    unsigned char *scan_buf = (unsigned char *)bu_malloc(sizeof(unsigned char) * 3 * iwidth, "scan_buf");
    for (int i = 0; i < rle_icv_hdr.ymax; i++) {
	unsigned char *pp = (unsigned char *)scan_buf;
	rle_pixel *rp = &(rows[0][0]);
	rle_pixel *gp = &(rows[1][0]);
	rle_pixel *bp = &(rows[2][0]);

	rle_getrow(&rle_icv_hdr, rows);

	/* Grumble, convert from Utah layout */
	if (!rle_icv_hdr.ncmap) {
	    for (int j = 0; j < iwidth; j++) {
		*pp++ = *rp++;
		*pp++ = *gp++;
		*pp++ = *bp++;
	    }
	} else {
	    for (int j = 0; j < iwidth; j++) {
		*pp++ = cmap.cm_red[*rp++]>>8;
		*pp++ = cmap.cm_green[*gp++]>>8;
		*pp++ = cmap.cm_blue[*bp++]>>8;
	    }
	}

	int offset = i * 3 * iwidth;
	for (int j = 0; j < iwidth; j++) {
	    data[offset + 3*j+0] = (double)scan_buf[3*j+0]/(double)255.0;
	    data[offset + 3*j+1] = (double)scan_buf[3*j+1]/(double)255.0;
	    data[offset + 3*j+2] = (double)scan_buf[3*j+2]/(double)255.0;
	}
    }

    bu_free(scan_buf, "scan_buf");

    icv_image_t *bif = NULL;
    BU_ALLOC(bif, struct icv_image);
    ICV_IMAGE_INIT(bif);
    bif->width = (size_t)rle_icv_hdr.xmax + 1;
    bif->height = (size_t)rle_icv_hdr.ymax + 1;
    bif->data = data;
    bif->magic = ICV_IMAGE_MAGIC;
    bif->channels = 3;
    bif->color_space = ICV_COLOR_SPACE_RGB;

    if (fp != stdin)
	fclose(fp);
    return bif;
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
