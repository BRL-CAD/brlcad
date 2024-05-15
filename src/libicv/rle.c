/*                           R L E . C
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
#include "bu/str.h"
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
    if (!bif)
	return BRLCAD_ERROR;

    int bg_color[3] = {0, 0, 0};
    rle_hdr rle_icv = { RUN_DISPATCH, 3, bg_color, 0, 2, 0, 511, 0, 511, 0, 8, NULL, NULL, 0, {7}, 0L, "libicv", "no file", 0, {{0}}};

    rle_icv.ncolors = 3;
    RLE_SET_BIT(rle_icv, RLE_RED);
    RLE_SET_BIT(rle_icv, RLE_GREEN);
    RLE_SET_BIT(rle_icv, RLE_BLUE);
    rle_icv.background = 2;      /* use background */
    rle_icv.alpha = 0;           /* no alpha channel */
    rle_icv.ncmap = 0;           /* no color map */
    rle_icv.cmaplen = 0;
    rle_icv.cmap = NULL;
    rle_icv.xmin = rle_icv.ymin = 0;
    rle_icv.xmax = bif->width-1;
    rle_icv.ymax = bif->height-1;
    rle_icv.comments = NULL;
    rle_icv.rle_file = stdout;
    if (filename) {
	rle_icv.rle_file = fopen(filename, "wb");
	if (!rle_icv.rle_file) {
	    bu_log("rle_write: Could not open %s for writing\n", filename);
	    return BRLCAD_ERROR;
	}
    }

    // pix-rle had more extensive info in comments (date, time, hostname, user,
    // etc.) - for now let's just note the originating software.  User should
    // really have control over whether or not to encode extra info like that.
    struct bu_vls cmt = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&cmt, "BRL-CAD libicv export");
    rle_putcom(bu_strdup(bu_vls_cstr(&cmt)), &rle_icv);
    bu_vls_free(&cmt);

    rle_put_setup(&rle_icv);

    rle_pixel **rows;
    rle_row_alloc(&rle_icv, &rows);

    const unsigned char *bifdata = icv_data2uchar(bif);
    unsigned char *scan_buf = (unsigned char *)bu_malloc(sizeof(unsigned char) * 3 * bif->width, "scan_buf");

    for (size_t i = 0; i < bif->height; i++) {
	int offset = 3*bif->width*i;
	for (size_t j = 0; j < bif->width; j++) {
	    scan_buf[3*j+0] = bifdata[offset + 3*j+0];
	    scan_buf[3*j+1] = bifdata[offset + 3*j+1];
	    scan_buf[3*j+2] = bifdata[offset + 3*j+2];
	}

	/* Grumble, convert to Utah layout */
	{
	    unsigned char *pp = (unsigned char *)scan_buf;
	    rle_pixel *rp = rows[0];
	    rle_pixel *gp = rows[1];
	    rle_pixel *bp = rows[2];
	    for (size_t k = 0; k < bif->width; k++) {
		*rp++ = *pp++;
		*gp++ = *pp++;
		*bp++ = *pp++;
	    }
	}
	rle_putrow(rows, bif->width, &rle_icv);
    }
    rle_puteof(&rle_icv);
    fclose(rle_icv.rle_file);
    bu_free(scan_buf, "scan_buf");

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
    rle_hdr rle_icv = { RUN_DISPATCH, 3, bg_color, 0, 2, 0, 511, 0, 511, 0, 8, NULL, NULL, 0, {7}, 0L, "libicv", "no file", 0, {{0}}};
    rle_icv.rle_file = fp;
    if (rle_get_setup(&rle_icv) < 0) {
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
    RLE_CLR_BIT(rle_icv, RLE_ALPHA);
    for (int i = 3; i < rle_icv.ncolors; i++)
	RLE_CLR_BIT(rle_icv, i);
    int iwidth = rle_icv.xmax + 1;
    int iheight = rle_icv.xmax + 1;

    double *data = (double *)bu_calloc(iwidth * iheight * 3, sizeof(double), "image data");

    // Set bg color
    if (rle_icv.bg_color) {
	for (int i = 0; i < (rle_icv.xmax + 1) * (rle_icv.ymax + 1); i++) {
	    data[3*i+0] = (double)rle_icv.bg_color[0]/(double)255.0;
	    data[3*i+1] = (double)rle_icv.bg_color[1]/(double)255.0;
	    data[3*i+2] = (double)rle_icv.bg_color[2]/(double)255.0;
	}
    }

    int ncolors = rle_icv.ncolors > 3 ? 3 : rle_icv.ncolors;
    int file_width = rle_icv.xmax - rle_icv.xmin + 1;
    unsigned char *rows[4] = {NULL};
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
    if (rle_icv.ncmap && rle_icv.ncmap > 0) {
	int maplen = (1 << rle_icv.cmaplen);
	int all = 0;
	for (int i = 0; i < 256; i++) {
	    cmap.cm_red[i] = rle_icv.cmap[i];
	    cmap.cm_green[i] = rle_icv.cmap[i+maplen];
	    cmap.cm_blue[i] = rle_icv.cmap[i+2*maplen];
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
    for (int i = 0; i < rle_icv.ymax; i++) {
	unsigned char *pp = (unsigned char *)scan_buf;
	rle_pixel *rp = &(rows[0][0]);
	rle_pixel *gp = &(rows[1][0]);
	rle_pixel *bp = &(rows[2][0]);

	rle_getrow(&rle_icv, rows);

	/* Grumble, convert from Utah layout */
	if (!rle_icv.ncmap) {
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
    bif->width = (size_t)rle_icv.xmax + 1;
    bif->height = (size_t)rle_icv.ymax + 1;
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
