/*                        D I F F . C P P
 * BRL-CAD
 *
 * Copyright (c) 2024-2026 United States Government as represented by
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
/** @file libicv/diff.cpp
 *
 * Higher-level image diffing utilities:
 *
 *  icv_diff_render_info  – compare embedded render metadata between two images
 *  icv_diff_nirt_shots   – generate nirt shotline commands for differing pixels;
 *                          writes a separate script for each image that has
 *                          render_info, so the caller can use whichever is needed
 *
 * Render metadata is stored in PNG files as a single tEXt chunk with the key
 * "BRL-CAD-scene" containing a JSON document (see png.cpp for the schema).
 * The icv_render_info struct is the in-memory representation; the functions
 * here operate on that struct and are format-agnostic.
 */

#include "common.h"

#include <cstring>
#include <cmath>
#include <cstdio>

#include "icv.h"
#include "bio.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "bu/vls.h"
#include "bn/mat.h"
#include "vmath.h"


/*
 * icv_diff_render_info
 *
 * Compare the icv_render_info embedded in img1 and img2.
 */
extern "C" int
icv_diff_render_info(const icv_image_t *img1, const icv_image_t *img2, struct bu_vls *out_msgs)
{
    const struct icv_render_info *r1 = img1 ? img1->render_info : NULL;
    const struct icv_render_info *r2 = img2 ? img2->render_info : NULL;

    if (!r1 && !r2)
	return -1;   /* neither image has metadata */

    int differs = 0;

    /* db filename */
    {
	const char *f1 = r1 ? r1->db_filename : NULL;
	const char *f2 = r2 ? r2->db_filename : NULL;
	if (!f1 && !f2) {
	    /* both absent – fine */
	} else if (!f1) {
	    if (out_msgs) bu_vls_printf(out_msgs, "db_filename: (none) vs '%s'\n", f2);
	    differs = 1;
	} else if (!f2) {
	    if (out_msgs) bu_vls_printf(out_msgs, "db_filename: '%s' vs (none)\n", f1);
	    differs = 1;
	} else if (!BU_STR_EQUAL(f1, f2)) {
	    if (out_msgs) bu_vls_printf(out_msgs, "db_filename: '%s' vs '%s'\n", f1, f2);
	    differs = 1;
	} else {
	    if (out_msgs) bu_vls_printf(out_msgs, "db_filename: '%s' (same)\n", f1);
	}
    }

    /* objects */
    {
	const char *o1 = r1 ? r1->objects : NULL;
	const char *o2 = r2 ? r2->objects : NULL;
	if (!o1 && !o2) {
	    /* both absent – fine */
	} else if (!o1) {
	    if (out_msgs) bu_vls_printf(out_msgs, "objects: (none) vs '%s'\n", o2);
	    differs = 1;
	} else if (!o2) {
	    if (out_msgs) bu_vls_printf(out_msgs, "objects: '%s' vs (none)\n", o1);
	    differs = 1;
	} else if (!BU_STR_EQUAL(o1, o2)) {
	    if (out_msgs) bu_vls_printf(out_msgs, "objects: '%s' vs '%s'\n", o1, o2);
	    differs = 1;
	} else {
	    if (out_msgs) bu_vls_printf(out_msgs, "objects: '%s' (same)\n", o1);
	}
    }

    /* Camera parameters – only check when both images carry camera data */
    if (r1 && r2) {
	const double tol = 1e-15;   /* strict: these should round-trip exactly */
	int cam_diff = 0;

	/* viewrotscale */
	for (int i = 0; i < 16; i++) {
	    if (fabs(r1->viewrotscale[i] - r2->viewrotscale[i]) > tol) {
		cam_diff = 1;
		break;
	    }
	}

	/* eye_model */
	for (int i = 0; i < 3; i++) {
	    if (fabs(r1->eye_model[i] - r2->eye_model[i]) > tol)
		cam_diff = 1;
	}

	if (fabs(r1->viewsize   - r2->viewsize)   > tol) cam_diff = 1;
	if (fabs(r1->aspect     - r2->aspect)     > tol) cam_diff = 1;
	if (fabs(r1->perspective - r2->perspective) > tol) cam_diff = 1;

	if (cam_diff) {
	    differs = 1;
	    if (out_msgs) {
		bu_vls_printf(out_msgs, "camera: DIFFERS\n");
		bu_vls_printf(out_msgs,
			      "  img1 eye_model: (%.17g, %.17g, %.17g)\n",
			      r1->eye_model[0], r1->eye_model[1], r1->eye_model[2]);
		bu_vls_printf(out_msgs,
			      "  img2 eye_model: (%.17g, %.17g, %.17g)\n",
			      r2->eye_model[0], r2->eye_model[1], r2->eye_model[2]);
		bu_vls_printf(out_msgs,
			      "  img1 viewsize=%.17g  img2 viewsize=%.17g\n",
			      r1->viewsize, r2->viewsize);
		bu_vls_printf(out_msgs,
			      "  img1 perspective=%.17g  img2 perspective=%.17g\n",
			      r1->perspective, r2->perspective);
	    }
	} else {
	    if (out_msgs) bu_vls_printf(out_msgs, "camera: identical\n");
	}
    } else if (r1 && !r2) {
	if (out_msgs) bu_vls_printf(out_msgs, "camera: img1 has camera data, img2 does not\n");
	differs = 1;
    } else if (!r1 && r2) {
	if (out_msgs) bu_vls_printf(out_msgs, "camera: img2 has camera data, img1 does not\n");
	differs = 1;
    }

    return differs;
}


/*
 * write_nirt_shots_for_ri
 *
 * Internal helper: given a specific render_info and a list of differing
 * pixels, reconstruct world-space rays and write a nirt script to fp.
 *
 * diffs is a flat array of (col, row, r1, g1, b1, r2, g2, b2) tuples stored
 * as 8 ints each.  ndiff is the number of such tuples.
 */
static void
write_nirt_shots_for_ri(const struct icv_render_info *ri,
			const unsigned int *diffs, int ndiff,
			size_t width, size_t height,
			FILE *fp)
{
    const double aspect = (ri->aspect > 0.0) ? ri->aspect : ((double)width / (double)height);
    const double viewsize = ri->viewsize;

    /* Reconstruct view2model from Viewrotscale + eye translation */
    mat_t Viewrotscale;
    MAT_COPY(Viewrotscale, ri->viewrotscale);
    Viewrotscale[15] = 0.5 * viewsize;   /* Viewscale element */

    mat_t toEye;
    MAT_IDN(toEye);
    MAT_DELTAS_VEC_NEG(toEye, ri->eye_model);

    mat_t model2view, view2model;
    bn_mat_mul(model2view, Viewrotscale, toEye);
    bn_mat_inv(view2model, model2view);

    const double cell_width  = viewsize / (double)width;
    const double cell_height = (aspect > 0.0)
	? viewsize / ((double)height * aspect)
	: viewsize / (double)height;

    /* dx_model, dy_model – rotate only, then scale */
    vect_t dx_model, dy_model;
    {
	vect_t temp;
	VSET(temp, 1, 0, 0);
	MAT3X3VEC(dx_model, view2model, temp);
	VSCALE(dx_model, dx_model, cell_width);

	VSET(temp, 0, 1, 0);
	MAT3X3VEC(dy_model, view2model, temp);
	VSCALE(dy_model, dy_model, cell_height);
    }

    /* Precompute the perspective zoomout factor (0 for orthographic) */
    const double zoomout = (ri->perspective > 0.0)
	? 1.0 / tan(DEG2RAD * ri->perspective / 2.0)
	: 0.0;

    /* viewbase_model – lower-left corner of the view plane */
    point_t viewbase_model;
    {
	vect_t temp;
	if (ri->perspective > 0.0) {
	    VSET(temp, -1.0, -1.0 / aspect, -zoomout);
	} else {
	    VSET(temp, -1.0, -1.0 / aspect, 0.0);
	}
	MAT4X3PNT(viewbase_model, view2model, temp);
    }

    /* Ray direction (orthographic – same for all pixels) */
    vect_t ray_dir;
    {
	vect_t temp;
	VSET(temp, 0.0, 0.0, -1.0);
	MAT4X3VEC(ray_dir, view2model, temp);
	VUNITIZE(ray_dir);
    }

    /* Header comment */
    fprintf(fp, "# nirt shotlines for differing pixels\n");
    if (ri->db_filename)
	fprintf(fp, "# database: %s\n", ri->db_filename);
    if (ri->objects)
	fprintf(fp, "# objects:  %s\n", ri->objects);
    fprintf(fp, "#\n");
    fprintf(fp, "# Usage: nirt -f <this_file> %s %s\n",
	    ri->db_filename ? ri->db_filename : "model.g",
	    ri->objects     ? ri->objects     : "[objects...]");
    fprintf(fp, "#\n");

    /* Set the ray direction once (same for all pixels in orthographic) */
    fprintf(fp, "dir %.17g %.17g %.17g\n",
	    ray_dir[X], ray_dir[Y], ray_dir[Z]);
    fprintf(fp, "\n");

    for (int i = 0; i < ndiff; i++) {
	const unsigned int *t = diffs + i * 8;
	size_t col = (size_t)t[0];
	size_t row = (size_t)t[1];
	int r1 = (int)t[2], g1 = (int)t[3], b1 = (int)t[4];
	int r2 = (int)t[5], g2 = (int)t[6], b2 = (int)t[7];

	point_t ray_pt;

	if (ri->perspective > 0.0) {
	    /* Perspective: diverging rays from a single eye point */
	    vect_t temp;
	    VSET(temp, -1.0 + 2.0 * (col + 0.5) / (double)width,
		       (-1.0 / aspect) + 2.0 * (row + 0.5) / ((double)height * aspect),
		       -zoomout);
	    vect_t world_dir;
	    MAT4X3VEC(world_dir, view2model, temp);
	    VUNITIZE(world_dir);
	    VMOVE(ray_pt, ri->eye_model);
	    fprintf(fp, "# pixel col=%zu row=%zu  rgb1=(%d,%d,%d) rgb2=(%d,%d,%d)\n",
		    col, row, r1, g1, b1, r2, g2, b2);
	    fprintf(fp, "xyz %.17g %.17g %.17g\n",
		    ray_pt[X], ray_pt[Y], ray_pt[Z]);
	    fprintf(fp, "dir %.17g %.17g %.17g\n",
		    world_dir[X], world_dir[Y], world_dir[Z]);
	    fprintf(fp, "s\n\n");
	} else {
	    /* Orthographic */
	    VJOIN2(ray_pt, viewbase_model,
		   (double)col, dx_model,
		   (double)row, dy_model);
	    fprintf(fp, "# pixel col=%zu row=%zu  rgb1=(%d,%d,%d) rgb2=(%d,%d,%d)\n",
		    col, row, r1, g1, b1, r2, g2, b2);
	    fprintf(fp, "xyz %.17g %.17g %.17g\n",
		    ray_pt[X], ray_pt[Y], ray_pt[Z]);
	    fprintf(fp, "s\n\n");
	}
    }
}


/*
 * icv_diff_nirt_shots
 *
 * For each pixel that differs between img1 and img2, reconstruct
 * world-space rays and write nirt commands.  A separate script is written
 * for each image that has render_info (and whose corresponding output FILE*
 * is non-NULL).  Both scripts contain shotlines for the same set of differing
 * pixels; they differ only in the camera parameters used for reconstruction
 * and the scene header comment.  This lets the caller interrogate either
 * scene independently, even when the db filenames or object names differ.
 *
 * Ray reconstruction mirrors BRL-CAD rt/grid.c grid_setup() for the
 * orthographic case (rt_perspective == 0):
 *
 *   1. Rebuild model2view from Viewrotscale + toEye(eye_model)
 *   2. Invert to get view2model
 *   3. dx_model = MAT3X3VEC(view2model, (1,0,0)) * cell_width
 *   4. dy_model = MAT3X3VEC(view2model, (0,1,0)) * cell_height
 *   5. viewbase_model = MAT4X3PNT(view2model, (-1, -1/aspect, 0))
 *   6. For pixel (col, row): ray_pt = viewbase + col*dx + row*dy
 *   7. ray_dir = MAT4X3VEC(view2model, (0, 0, -1)), normalised
 *
 * For perspective, a diverging ray is computed similarly.
 */
extern "C" int
icv_diff_nirt_shots(const icv_image_t *img1, const icv_image_t *img2,
		    FILE *nirt_out1, FILE *nirt_out2)
{
    if (!img1 || !img2)
	return -1;
    if (!nirt_out1 && !nirt_out2)
	return -1;

    if (img1->width != img2->width || img1->height != img2->height) {
	bu_log("icv_diff_nirt_shots: images must be the same size\n");
	return -1;
    }

    const struct icv_render_info *ri1 = img1->render_info;
    const struct icv_render_info *ri2 = img2->render_info;

    /* We need at least one valid render_info for the requested outputs */
    if (nirt_out1 && !ri1) {
	bu_log("icv_diff_nirt_shots: img1 has no render_info – cannot write img1 nirt script\n");
	nirt_out1 = NULL;
    }
    if (nirt_out2 && !ri2) {
	bu_log("icv_diff_nirt_shots: img2 has no render_info – cannot write img2 nirt script\n");
	nirt_out2 = NULL;
    }
    if (!nirt_out1 && !nirt_out2) {
	bu_log("icv_diff_nirt_shots: neither active output has render_info – cannot compute rays\n");
	return -1;
    }
    if (ri1 && nirt_out1 && ri1->viewsize <= 0.0) {
	bu_log("icv_diff_nirt_shots: img1 render_info has invalid viewsize (%g)\n", ri1->viewsize);
	nirt_out1 = NULL;
    }
    if (ri2 && nirt_out2 && ri2->viewsize <= 0.0) {
	bu_log("icv_diff_nirt_shots: img2 render_info has invalid viewsize (%g)\n", ri2->viewsize);
	nirt_out2 = NULL;
    }
    if (!nirt_out1 && !nirt_out2)
	return -1;

    const size_t width  = img1->width;
    const size_t height = img1->height;

    /* Get uint8 pixel data for comparison */
    unsigned char *d1 = icv_data2uchar(img1);
    unsigned char *d2 = icv_data2uchar(img2);

    /* Collect all differing pixels into a flat tuple array */
    unsigned int *diffs = (unsigned int *)bu_malloc(
	width * height * 8 * sizeof(unsigned int), "nirt diffs");
    int ndiff = 0;

    for (size_t row = 0; row < height; row++) {
	for (size_t col = 0; col < width; col++) {
	    size_t idx = row * width + col;
	    unsigned int r1p = d1[idx*3+0], g1p = d1[idx*3+1], b1p = d1[idx*3+2];
	    unsigned int r2p = d2[idx*3+0], g2p = d2[idx*3+1], b2p = d2[idx*3+2];

	    if (r1p == r2p && g1p == g2p && b1p == b2p)
		continue;

	    unsigned int *t = diffs + ndiff * 8;
	    t[0] = (unsigned int)col;
	    t[1] = (unsigned int)row;
	    t[2] = r1p; t[3] = g1p; t[4] = b1p;
	    t[5] = r2p; t[6] = g2p; t[7] = b2p;
	    ndiff++;
	}
    }

    bu_free(d1, "icv_diff_nirt d1");
    bu_free(d2, "icv_diff_nirt d2");

    /* Write a script for each image that has metadata and an open output */
    if (nirt_out1)
	write_nirt_shots_for_ri(ri1, diffs, ndiff, width, height, nirt_out1);
    if (nirt_out2)
	write_nirt_shots_for_ri(ri2, diffs, ndiff, width, height, nirt_out2);

    bu_free(diffs, "nirt diffs");

    return ndiff;
}


extern "C" int
icv_diff(
	int *matching, int *off_by_1, int *off_by_many,
	icv_image_t *img1, icv_image_t *img2
	)
{
    if (!img1 || !img2)
	return -1;

    int ret = 0;

    /* Compare channel by channel, matching pixdiff counting semantics:
     * matching  = number of channel bytes with identical values
     * off_by_1  = number of channel bytes differing by exactly 1
     * off_by_many = number of channel bytes differing by more than 1
     */
    unsigned char *d1 = icv_data2uchar(img1);
    unsigned char *d2 = icv_data2uchar(img2);
    size_t s1 = img1->width * img1->height;
    size_t s2 = img2->width * img2->height;
    size_t smin = (s1 < s2) ? s1 : s2;
    size_t smax = (s1 > s2) ? s1 : s2;

    for (size_t i = 0; i < smin; i++) {
	int ch;
	for (ch = 0; ch < 3; ch++) {
	    int c1 = d1[i*3+ch];
	    int c2 = d2[i*3+ch];
	    int diff = c1 - c2;
	    if (diff < 0) diff = -diff;
	    if (diff == 0) {
		if (matching) (*matching)++;
	    } else if (diff == 1) {
		ret = 1;
		if (off_by_1) (*off_by_1)++;
	    } else {
		ret = 1;
		if (off_by_many) (*off_by_many)++;
	    }
	}
    }

    /* Count any extra channels in the larger image as off_by_many */
    if (smin != smax) {
	ret = 1;
	if (off_by_many) {
	    (*off_by_many) += (int)((smax - smin) * 3);
	}
    }

    bu_free(d1, "image 1 rgb");
    bu_free(d2, "image 2 rgb");

    return ret;
}

extern "C" icv_image_t *
icv_diffimg(icv_image_t *img1, icv_image_t *img2)
{
    long p;

    if (!img1 || !img2 || !img1->width || !img2->width)
	return NULL;

    if ((img1->width != img2->width) || (img1->height != img2->height) || (img1->channels != img2->channels)) {
	bu_log("icv_diffimg : Image Parameters not Equal");
	return NULL;
    }

    /* For each pixel:
     *   - If all channels match: output half-intensity greyscale (NTSC average)
     *   - If any channel differs: for each channel independently,
     *       |diff| > 1 → output 0xFF for that channel
     *       |diff| == 1 → output 0xC0 for that channel
     *       diff == 0   → output 0x00 for that channel
     *
     * This matches pixdiff semantics: a pixel different in only one channel
     * appears as a pure R, G, or B highlight rather than white/grey.
     */
    unsigned char *d1 = icv_data2uchar(img1);
    unsigned char *d2 = icv_data2uchar(img2);
    size_t s = img1->width * img1->height;
    size_t nbytes = s * 3;
    unsigned char *od = (unsigned char *)bu_malloc(nbytes, "diffimg output");
    memset(od, 0, nbytes);

    for (size_t i = 0; i < s; i++) {
	int r1 = d1[i*3+0], g1 = d1[i*3+1], b1 = d1[i*3+2];
	int r2 = d2[i*3+0], g2 = d2[i*3+1], b2 = d2[i*3+2];

	if (r1 == r2 && g1 == g2 && b1 == b2) {
	    /* Common case: equal - half-intensity NTSC greyscale */
	    p = ((22937 * r1 + 36044 * g1 + 6553 * b1) >> 17);
	    if (p < 0) p = 0;
	    p /= 2;
	    od[3*i+0] = (unsigned char)p;
	    od[3*i+1] = (unsigned char)p;
	    od[3*i+2] = (unsigned char)p;
	} else {
	    /* Per-channel difference highlighting */
	    int ch;
	    const int c1[3] = { r1, g1, b1 };
	    const int c2[3] = { r2, g2, b2 };
	    for (ch = 0; ch < 3; ch++) {
		int diff = c1[ch] - c2[ch];
		if (diff < 0) diff = -diff;
		if (diff == 0)
		    od[3*i+ch] = 0x00;
		else if (diff == 1)
		    od[3*i+ch] = 0xC0;
		else
		    od[3*i+ch] = 0xFF;
	    }
	}
    }

    icv_image_t *out_img;
    BU_ALLOC(out_img, struct icv_image);
    ICV_IMAGE_INIT(out_img);
    out_img->width = img1->width;
    out_img->height = img1->height;
    out_img->channels = 3;
    out_img->color_space = ICV_COLOR_SPACE_RGB;
    out_img->data = icv_uchar2double(od, nbytes);

    bu_free(d1, "image 1 rgb");
    bu_free(d2, "image 2 rgb");
    bu_free(od, "diffimg output");

    return out_img;
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C++
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
