/*                         P N G . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2025 United States Government as represented by
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
/** @file libged/png.c
 *
 * The png command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "png.h"


#include "bu/getopt.h"
#include "vmath.h"
#include "bn.h"
#include "bg/clip.h"


#include "../ged_private.h"


static unsigned int img_size = 512;
static unsigned int img_half_size = 256;

static unsigned char bg_red = 255;
static unsigned char bg_green = 255;
static unsigned char bg_blue = 255;

struct coord {
    short x;
    short y;
};

struct stroke {
    struct coord pixel; /* starting scan, nib */
    short xsign;        /* 0 or +1 */
    short ysign;        /* -1, 0, or +1 */
    int ymajor;         /* true if Y is major dir. */
    short major;        /* major dir delta (nonneg) */
    short minor;        /* minor dir delta (nonneg) */
    short e;            /* DDA error accumulator */
    short de;           /* increment for `e' */
};

/*
 * Method:
 * Modified Bresenham algorithm.  Guaranteed to mark a dot for
 * a zero-length stroke.
 */
static void
raster(unsigned char **image, struct stroke *vp, const unsigned char *color, size_t size)
{
    size_t dy;          /* raster within active band */

    /*
     * Write the color of this vector on all pixels.
     */
    for (dy = vp->pixel.y; dy <= size;) {

        /* set the appropriate pixel in the buffer to color */
        image[size-dy][vp->pixel.x*3] = color[0];
        image[size-dy][vp->pixel.x*3+1] = color[1];
        image[size-dy][vp->pixel.x*3+2] = color[2];

        if (vp->major-- == 0)
            return;             /* Done */

        if (vp->e < 0) {
            /* advance major & minor */
            dy += vp->ysign;
            vp->pixel.x += vp->xsign;
            vp->e += vp->de;
        } else {
            /* advance major only */
            if (vp->ymajor)     /* Y is major dir */
                ++dy;
            else                /* X is major dir */
                vp->pixel.x += vp->xsign;
            vp->e -= vp->minor;
        }
    }
}

static void
draw_stroke(unsigned char **image, struct coord *coord1, struct coord *coord2, const unsigned char *color, size_t size)
{
    struct stroke cur_stroke;
    struct stroke *vp = &cur_stroke;

    /* arrange for pt1 to have the smaller Y-coordinate: */
    if (coord1->y > coord2->y) {
        struct coord *temp;     /* temporary for swap */

        temp = coord1;          /* swap pointers */
        coord1 = coord2;
        coord2 = temp;
    }

    /* set up DDA parameters for stroke */
    vp->pixel = *coord1;                /* initial pixel */
    vp->major = coord2->y - vp->pixel.y;        /* always nonnegative */
    vp->ysign = vp->major ? 1 : 0;
    vp->minor = coord2->x - vp->pixel.x;
    vp->xsign = vp->minor ? (vp->minor > 0 ? 1 : -1) : 0;
    if (vp->xsign < 0)
        vp->minor = -vp->minor;

    /* if Y is not really major, correct the assignments */
    vp->ymajor = vp->minor <= vp->major;
    if (!vp->ymajor) {
        short temp;     /* temporary for swap */

        temp = vp->minor;
        vp->minor = vp->major;
        vp->major = temp;
    }

    vp->e = vp->major / 2 - vp->minor;  /* initial DDA error */
    vp->de = vp->major - vp->minor;

    raster(image, vp, color, size);
}

static void
draw_png_solid(fastf_t perspective, unsigned char **image, struct bv_scene_obj *sp, matp_t psmat, size_t size, size_t half_size)
{
    static vect_t last;
    point_t clipmin = {-1.0, -1.0, -MAX_FASTF};
    point_t clipmax = {1.0, 1.0, MAX_FASTF};
    struct bv_vlist *tvp;
    point_t *pt_prev=NULL;
    fastf_t dist_prev=1.0;
    fastf_t dist;
    struct bv_vlist *vp = (struct bv_vlist *)&sp->s_vlist;
    fastf_t delta;
    struct coord coord1;
    struct coord coord2;

    /* delta is used in clipping to insure clipped endpoint is slightly
     * in front of eye plane (perspective mode only).
     * This value is a SWAG that seems to work OK.
     */
    delta = psmat[15]*0.0001;
    if (delta < 0.0)
        delta = -delta;
    if (delta < SQRT_SMALL_FASTF)
        delta = SQRT_SMALL_FASTF;

    for (BU_LIST_FOR(tvp, bv_vlist, &vp->l)) {
        size_t i;
        size_t nused = tvp->nused;
        int *cmd = tvp->cmd;
        point_t *pt = tvp->pt;
        for (i = 0; i < nused; i++, cmd++, pt++) {
            static vect_t start, fin;
            switch (*cmd) {
                case BV_VLIST_POLY_START:
                case BV_VLIST_POLY_VERTNORM:
                case BV_VLIST_TRI_START:
                case BV_VLIST_TRI_VERTNORM:
                    continue;
                case BV_VLIST_POLY_MOVE:
                case BV_VLIST_LINE_MOVE:
                case BV_VLIST_TRI_MOVE:
                    /* Move, not draw */
                    if (perspective > 0) {
                        /* cannot apply perspective transformation to
                         * points behind eye plane!!!!
                         */
                        dist = VDOT(*pt, &psmat[12]) + psmat[15];
                        if (dist <= 0.0) {
                            pt_prev = pt;
                            dist_prev = dist;
                            continue;
                        } else {
                            MAT4X3PNT(last, psmat, *pt);
                            dist_prev = dist;
                            pt_prev = pt;
                        }
                    } else
                        MAT4X3PNT(last, psmat, *pt);
                    continue;
                case BV_VLIST_POLY_DRAW:
                case BV_VLIST_POLY_END:
                case BV_VLIST_LINE_DRAW:
                case BV_VLIST_TRI_DRAW:
                case BV_VLIST_TRI_END:
                    /* draw */
                    if (perspective > 0) {
                        /* cannot apply perspective transformation to
                         * points behind eye plane!!!!
                         */
                        dist = VDOT(*pt, &psmat[12]) + psmat[15];
                        if (dist <= 0.0) {
                            if (dist_prev <= 0.0) {
                                /* nothing to plot */
                                dist_prev = dist;
                                pt_prev = pt;
                                continue;
                            } else {
                                if (pt_prev) {
				    fastf_t alpha;
				    vect_t diff;
				    point_t tmp_pt;

				    /* clip this end */
				    VSUB2(diff, *pt, *pt_prev);
				    alpha = (dist_prev - delta) / (dist_prev - dist);
				    VJOIN1(tmp_pt, *pt_prev, alpha, diff);
				    MAT4X3PNT(fin, psmat, tmp_pt);
                                }
                            }
                        } else {
                            if (dist_prev <= 0.0) {
                                if (pt_prev) {
				    fastf_t alpha;
				    vect_t diff;
				    point_t tmp_pt;

				    /* clip other end */
				    VSUB2(diff, *pt, *pt_prev);
				    alpha = (-dist_prev + delta) / (dist - dist_prev);
				    VJOIN1(tmp_pt, *pt_prev, alpha, diff);
				    MAT4X3PNT(last, psmat, tmp_pt);
				    MAT4X3PNT(fin, psmat, *pt);
                                }
                            } else {
                                MAT4X3PNT(fin, psmat, *pt);
                            }
                        }
                    } else
                        MAT4X3PNT(fin, psmat, *pt);
                    VMOVE(start, last);
                    VMOVE(last, fin);
                    break;
            }

            if (bg_ray_vclip(start, fin, clipmin, clipmax) == 0)
                continue;

            coord1.x = start[0] * half_size + half_size;
            coord1.y = start[1] * half_size + half_size;
            coord2.x = fin[0] * half_size + half_size;
            coord2.y = fin[1] * half_size + half_size;
            draw_stroke(image, &coord1, &coord2, sp->s_color, size);
        }
    }
}



static void
dl_png(struct bu_list *hdlp, mat_t model2view, fastf_t perspective, vect_t eye_pos, size_t size, size_t half_size, unsigned char **image)
{
    struct display_list *gdlp;
    struct display_list *next_gdlp;
    mat_t newmat;
    matp_t mat;
    mat_t perspective_mat;
    struct bv_scene_obj *sp;

    mat = model2view;

    if (0 < perspective) {
        point_t l, h;

        VSET(l, -1.0, -1.0, -1.0);
        VSET(h, 1.0, 1.0, 200.0);

        if (ZERO(eye_pos[Z] - 1.0)) {
            /* This way works, with reasonable Z-clipping */
            persp_mat(perspective_mat, perspective,
		      (fastf_t)1.0f, (fastf_t)0.01f, (fastf_t)1.0e10f, (fastf_t)1.0f);
        } else {
            /* This way does not have reasonable Z-clipping,
             * but includes shear, for GDurf's testing.
             */
            deering_persp_mat(perspective_mat, l, h, eye_pos);
        }

        bn_mat_mul(newmat, perspective_mat, mat);
        mat = newmat;
    }

    gdlp = BU_LIST_NEXT(display_list, hdlp);
    while (BU_LIST_NOT_HEAD(gdlp, hdlp)) {
        next_gdlp = BU_LIST_PNEXT(display_list, gdlp);

        for (BU_LIST_FOR(sp, bv_scene_obj, &gdlp->dl_head_scene_obj)) {
            draw_png_solid(perspective, image, sp, mat, size, half_size);
        }

        gdlp = next_gdlp;
    }
}



static int
draw_png(struct ged *gedp, FILE *fp)
{
    long i;
    png_structp png_p;
    png_infop info_p;
    double out_gamma = 1.0;

    /* TODO: explain why this is size+1 */
    size_t num_bytes_per_row = (img_size+1) * 3;
    size_t num_bytes = num_bytes_per_row * (img_size+1);
    unsigned char **image = (unsigned char **)bu_malloc(sizeof(unsigned char *) * (img_size+1), "draw_png, image");
    unsigned char *bytes = (unsigned char *)bu_malloc(num_bytes, "draw_png, bytes");

    /* Initialize bytes using the background color */
    if (bg_red == bg_green && bg_red == bg_blue)
	memset((void *)bytes, bg_red, num_bytes);
    else {
	for (i = 0; (size_t)i < num_bytes; i += 3) {
	    bytes[i] = bg_red;
	    bytes[i+1] = bg_green;
	    bytes[i+2] = bg_blue;
	}
    }

    png_p = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_p) {
	bu_vls_printf(gedp->ged_result_str, "Could not create PNG write structure\n");
	return BRLCAD_ERROR;
    }

    info_p = png_create_info_struct(png_p);
    if (!info_p) {
	bu_vls_printf(gedp->ged_result_str, "Could not create PNG info structure\n");
	bu_free((void *)image, "draw_png, image");
	bu_free((void *)bytes, "draw_png, bytes");

	return BRLCAD_ERROR;
    }

    png_init_io(png_p, fp);
    png_set_filter(png_p, 0, PNG_FILTER_NONE);
    png_set_compression_level(png_p, 9);
    png_set_IHDR(png_p, info_p,
		 img_size, img_size, 8,
		 PNG_COLOR_TYPE_RGB,
		 PNG_INTERLACE_NONE,
		 PNG_COMPRESSION_TYPE_DEFAULT,
		 PNG_FILTER_TYPE_DEFAULT);
    png_set_gAMA(png_p, info_p, out_gamma);
    png_write_info(png_p, info_p);

    /* Arrange the rows of data/pixels */
    for (i = img_size; i >= 0; --i) {
	image[i] = (unsigned char *)(bytes + ((img_size-i) * num_bytes_per_row));
    }

    dl_png(gedp->i->ged_gdp->gd_headDisplay, gedp->ged_gvp->gv_model2view, gedp->ged_gvp->gv_perspective, gedp->ged_gvp->gv_eye_pos, (size_t)img_size, (size_t)img_half_size, image);

    /* Write out pixels */
    png_write_image(png_p, image);
    png_write_end(png_p, NULL);

    bu_free((void *)image, "draw_png, image");
    bu_free((void *)bytes, "draw_png, bytes");

    return BRLCAD_OK;
}


int
ged_png_core(struct ged *gedp, int argc, const char *argv[])
{
    FILE *fp;
    int k;
    int ret;
    int r, g, b;
    static const char *usage = "[-c r/g/b] [-s size] file";

    GED_CHECK_VIEW(gedp, BRLCAD_ERROR);
    GED_CHECK_DRAWABLE(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    /* Process options */
    bu_optind = 1;
    while ((k = bu_getopt(argc, (char * const *)argv, "c:s:")) != -1) {
	switch (k) {
	    case 'c':
		/* parse out a delimited rgb color value */
		if (sscanf(bu_optarg, "%d%*c%d%*c%d", &r, &g, &b) != 3) {
		    bu_vls_printf(gedp->ged_result_str, "%s: bad color - %s", argv[0], bu_optarg);
		    return BRLCAD_ERROR;
		}

		/* Clamp color values */
		if (r < 0)
		    r = 0;
		else if (r > 255)
		    r = 255;

		if (g < 0)
		    g = 0;
		else if (g > 255)
		    g = 255;

		if (b < 0)
		    b = 0;
		else if (b > 255)
		    b = 255;

		bg_red = (unsigned char)r;
		bg_green = (unsigned char)g;
		bg_blue = (unsigned char)b;

		break;
	    case 's':
		if (sscanf(bu_optarg, "%u", &img_size) != 1) {
		    bu_vls_printf(gedp->ged_result_str, "%s: bad size - %s", argv[0], bu_optarg);
		    return BRLCAD_ERROR;
		}

		if (img_size < 50) {
		    bu_vls_printf(gedp->ged_result_str, "%s: bad size - %s, must be greater than or equal to 50\n", argv[0], bu_optarg);
		    return BRLCAD_ERROR;
		}

		img_half_size = img_size * 0.5;

		break;
	    default:
		bu_vls_printf(gedp->ged_result_str, "%s: Unrecognized option - %s", argv[0], argv[bu_optind-1]);
		return BRLCAD_ERROR;
	}
    }

    if ((argc - bu_optind) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    fp = fopen(argv[bu_optind], "wb");
    if (fp == NULL) {
	bu_vls_printf(gedp->ged_result_str, "%s: Error opening file - %s\n", argv[0], argv[bu_optind]);
	return BRLCAD_ERROR;
    }

    ret = draw_png(gedp, fp);
    fclose(fp);

    return ret;
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl png_cmd_impl = {"png", ged_png_core, GED_CMD_DEFAULT};
const struct ged_cmd png_cmd = { &png_cmd_impl };

struct ged_cmd_impl pngwf_cmd_impl = {"pngwf", ged_png_core, GED_CMD_DEFAULT};
const struct ged_cmd pngwf_cmd = { &pngwf_cmd_impl };

const struct ged_cmd *png_cmds[] = { &png_cmd, &pngwf_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  png_cmds, 2 };

COMPILER_DLLEXPORT const struct ged_plugin *ged_plugin_info(void)
{
    return &pinfo;
}
#endif /* GED_PLUGIN */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
