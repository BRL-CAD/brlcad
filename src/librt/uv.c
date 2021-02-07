/*                            U V . C
 * BRL-CAD
 *
 * Copyright (c) 2019-2021 United States Government as represented by
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
/** @file uv.c
 *
 * Implementation of UV texture mapping support.
 *
 */

#include "common.h"

#include "rt/uv.h"

#include "rt/debug.h"
#include "rt/defines.h"
#include "rt/global.h"
#include "rt/db_internal.h"
#include "rt/db_io.h"
#include "rt/nongeom.h"


static void
texture_init(struct rt_texture *texture)
{
    if (!texture)
	return;

    bu_vls_init(&texture->tx_name);

    /* defaults */
    texture->tx_w = -1;
    texture->tx_n = -1;
    texture->tx_trans_valid = 0;
    texture->tx_transp[X] = 0;
    texture->tx_transp[Y] = 0;
    texture->tx_transp[Z] = 0;
    texture->tx_scale[X] = 1.0;
    texture->tx_scale[Y] = 1.0;
    texture->tx_mirror = 0;
    texture->tx_datasrc = TXT_SRC_AUTO;

    /* internal state */
    texture->tx_binunifp = NULL;
    texture->tx_mp = NULL;
}


int
rt_texture_load(struct rt_texture *texture, const char *name, struct db_i *dbip)
{
    struct directory *dirEntry;
    size_t size = 1234; /* !!! fixme */

    RT_CK_DBI(dbip);

    if (!name || !texture)
	return 0;

    texture_init(texture);

    bu_log("Loading texture %s [%s]...", texture->tx_datasrc==TXT_SRC_AUTO?"from auto-determined datasource":texture->tx_datasrc==TXT_SRC_OBJECT?"from a database object":texture->tx_datasrc==TXT_SRC_FILE?"from a file":"from an unknown source (ERROR)", bu_vls_addr(&texture->tx_name));

    /* if the source is auto or object, we try to load the object */
    if ((texture->tx_datasrc==TXT_SRC_AUTO) || (texture->tx_datasrc==TXT_SRC_OBJECT)) {

        /* see if the object exists */
        if ((dirEntry=db_lookup(dbip, bu_vls_addr(&texture->tx_name), LOOKUP_QUIET)) == RT_DIR_NULL) {

            /* unable to find the texture object */
            if (texture->tx_datasrc!=TXT_SRC_AUTO) {
                return -1;
            }
        } else {
            struct rt_db_internal *dbip2;

            BU_ALLOC(dbip2, struct rt_db_internal);

            RT_DB_INTERNAL_INIT(dbip2);
            RT_CK_DB_INTERNAL(dbip2);
            RT_CK_DIR(dirEntry);

            /* the object was in the directory, so go get it */
            if (rt_db_get_internal(dbip2, dirEntry, dbip, NULL, NULL) <= 0) {
                /* unable to load/create the texture database record object */
                return -1;
            }

            RT_CK_DB_INTERNAL(dbip2);
            RT_CK_BINUNIF(dbip2->idb_ptr);

            /* keep the binary object pointer */
            texture->tx_binunifp = (struct rt_binunif_internal *)dbip2->idb_ptr; /* make it so */

            /* release the database instance pointer struct we created */
            RT_DB_INTERNAL_INIT(dbip2);
            bu_free(dbip2, "txt_load_datasource");

            /* check size of object */
            if (texture->tx_binunifp->count < size) {
                bu_log("\nWARNING: %s needs %zu bytes, binary object only has %zu\n", bu_vls_addr(&texture->tx_name), size, texture->tx_binunifp->count);
            } else if (texture->tx_binunifp->count > size) {
                bu_log("\nWARNING: Binary object is larger than specified texture size\n"
                       "\tBinary Object: %zu pixels\n\tSpecified Texture Size: %zu pixels\n"
                       "...continuing to load using image subsection...",
                       texture->tx_binunifp->count, size);
            }
        }
    }

    /* if we are auto and we couldn't find a database object match, or
     * if source is explicitly a file then we load the file.
     */
    if (((texture->tx_datasrc==TXT_SRC_AUTO) && (texture->tx_binunifp==NULL)) || (texture->tx_datasrc==TXT_SRC_FILE)) {

        texture->tx_mp = bu_open_mapped_file_with_path(dbip->dbi_filepath,        bu_vls_addr(&texture->tx_name), NULL);

        if (texture->tx_mp==NULL)
            return -1; /* FAIL */

	if (texture->tx_mp->buflen < size) {
            bu_log("\nWARNING: %s needs %zu bytes, file only has %zu\n", bu_vls_addr(&texture->tx_name), size, texture->tx_mp->buflen);
        } else if (texture->tx_mp->buflen > size) {
            bu_log("\nWARNING: Texture file size is larger than specified texture size\n"
                   "\tInput File: %zu pixels\n\tSpecified Texture Size: %zu pixels\n"
                   "...continuing to load using image subsection...",
                   texture->tx_mp->buflen, size);
        }
    }

    bu_log("done.\n");

    return 0;
}


int
rt_texture_lookup(fastf_t *data, const struct rt_texture *tp, const struct uvcoord *uvp)
{
    fastf_t r, g, b;
    fastf_t xmin, xmax, ymin, ymax;
    int dx, dy;
    long tmp;
    struct uvcoord uvc;
    char color_warn = 0;

    uvc = *uvp; /* struct copy */

    /* take care of scaling U, V coordinates to get the desired amount
     * of replication of the texture
     */
    uvc.uv_u *= tp->tx_scale[X];
    tmp = uvc.uv_u;
    uvc.uv_u -= tmp;
    if (tp->tx_mirror && (tmp & 1))
	uvc.uv_u = 1.0 - uvc.uv_u;

    uvc.uv_v *= tp->tx_scale[Y];
    tmp = uvc.uv_v;
    uvc.uv_v -= tmp;
    if (tp->tx_mirror && (tmp & 1))
	uvc.uv_v = 1.0 - uvc.uv_v;

    uvc.uv_du /= tp->tx_scale[X];
    uvc.uv_dv /= tp->tx_scale[Y];

    /*
     * If no texture file present, or if
     * texture isn't and can't be read, give debug colors
     */

    if ((bu_vls_strlen(&tp->tx_name) <= 0) || (!tp->tx_mp && !tp->tx_binunifp)) {
	bu_log("WARNING: texture [%s] could not be read\n", bu_vls_addr(&tp->tx_name));
    }

    /* u is left->right index, v is line number bottom->top */
    /* Don't filter more than 1/8 of the texture for 1 pixel! */
    if (uvc.uv_du > 0.125)
	uvc.uv_du = 0.125;
    if (uvc.uv_dv > 0.125)
	uvc.uv_dv = 0.125;
    if (uvc.uv_du < 0 || uvc.uv_dv < 0) {
	uvc.uv_du = uvc.uv_dv = 0;
    }

    xmin = uvc.uv_u - uvc.uv_du;
    xmax = uvc.uv_u + uvc.uv_du;
    ymin = uvc.uv_v - uvc.uv_dv;
    ymax = uvc.uv_v + uvc.uv_dv;
    if (xmin < 0)
	xmin = 0;
    if (ymin < 0)
	ymin = 0;
    if (xmax > 1)
	xmax = 1;
    if (ymax > 1)
	ymax = 1;

    dx = (int)(xmax * (tp->tx_w-1)) - (int)(xmin * (tp->tx_w-1));
    dy = (int)(ymax * (tp->tx_n-1)) - (int)(ymin * (tp->tx_n-1));

    if (dx == 0 && dy == 0) {
	/* No averaging necessary */

	register unsigned char *cp=NULL;
	unsigned int offset;

	offset = (int)(ymin * (tp->tx_n-1)) * tp->tx_w * 3 + (int)(xmin * (tp->tx_w-1)) * 3;
	if (tp->tx_mp) {
	    if (offset >= tp->tx_mp->buflen) {
		offset %= tp->tx_mp->buflen;
		color_warn = 1;
	    }
	    cp = ((unsigned char *)(tp->tx_mp->buf)) + offset;
	} else if (tp->tx_binunifp) {
	    if (offset >= tp->tx_binunifp->count) {
		offset %= tp->tx_binunifp->count;
		color_warn = 1;
	    }
	    cp = ((unsigned char *)(tp->tx_binunifp->u.uint8)) + offset;
	} else {
	    bu_bomb("sh_text.c -- No texture data found\n");
	}
	r = *cp++;
	g = *cp++;
	b = *cp;
    } else {
	/* Calculate weighted average of cells in footprint */

	fastf_t tot_area = 0.0;
	fastf_t cell_area;
	int start_line, stop_line, line;
	int start_col, stop_col, col;
	fastf_t xstart, xstop, ystart, ystop;
	unsigned int max_offset;

	xstart = xmin * (tp->tx_w-1);
	xstop = xmax * (tp->tx_w-1);
	ystart = ymin * (tp->tx_n-1);
	ystop = ymax * (tp->tx_n-1);

	start_line = ystart;
	stop_line = ystop;
	start_col = xstart;
	stop_col = xstop;

	r = g = b = 0.0;

	if (RT_G_DEBUG) {
	    bu_log("\thit in texture space = (%g %g)\n", uvc.uv_u * (tp->tx_w-1), uvc.uv_v * (tp->tx_n-1));
	    bu_log("\t averaging from  (%g %g) to (%g %g)\n", xstart, ystart, xstop, ystop);
	    bu_log("\tcontributions to average:\n");
	}

	max_offset = stop_line * tp->tx_w * 3 + (int)(xstart) * 3 + (dx + 1) * 3;
	if (tp->tx_mp) {
	    if (max_offset > tp->tx_mp->buflen) {
		color_warn = 1;
	    }
	} else if (tp->tx_binunifp) {
	    if (max_offset > tp->tx_binunifp->count) {
		color_warn = 1;
	    }
	}

	for (line = start_line; line <= stop_line; line++) {
	    register unsigned char *cp=NULL;
	    fastf_t line_factor;
	    fastf_t line_upper, line_lower;
	    unsigned int offset;

	    line_upper = line + 1.0;
	    if (line_upper > ystop)
		line_upper = ystop;
	    line_lower = line;
	    if (line_lower < ystart)
		line_lower = ystart;
	    line_factor = line_upper - line_lower;

	    offset = line * tp->tx_w * 3 + (int)(xstart) * 3;
	    if (tp->tx_mp) {
		if (offset >= tp->tx_mp->buflen) {
		    offset %= tp->tx_mp->buflen;
		}
		cp = ((unsigned char *)(tp->tx_mp->buf)) + offset;
	    } else if (tp->tx_binunifp) {
		if (offset >= tp->tx_binunifp->count) {
		    offset %= tp->tx_binunifp->count;
                }
		cp = ((unsigned char *)(tp->tx_binunifp->u.uint8)) + offset;
	    } else {
		/* not reachable */
		bu_bomb("sh_text.c -- Unable to read datasource\n");
	    }

	    for (col = start_col; col <= stop_col; col++) {
		fastf_t col_upper, col_lower;

		col_upper = col + 1.0;
		if (col_upper > xstop) col_upper = xstop;
		col_lower = col;
		if (col_lower < xstart) col_lower = xstart;

		cell_area = line_factor * (col_upper - col_lower);
		tot_area += cell_area;

		if (RT_G_DEBUG)
		    bu_log("\t %d %d %d weight=%g (from col=%d line=%d)\n", *cp, *(cp+1), *(cp+2), cell_area, col, line);

		r += (*cp++) * cell_area;
		g += (*cp++) * cell_area;
		b += (*cp++) * cell_area;
	    }

	}
	r /= tot_area;
	g /= tot_area;
	b /= tot_area;
    }

    if (RT_G_DEBUG)
	bu_log(" average: %g %g %g\n", r, g, b);

    if (color_warn) {
	bu_log("color warn set\n");
    }

    /* FIXME: assumes caller provided enough memory */
    data[0] = r;
    data[1] = g;
    data[2] = b;

    return 0;
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
