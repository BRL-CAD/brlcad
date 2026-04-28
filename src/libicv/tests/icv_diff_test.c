/*               I C V _ D I F F _ T E S T . C
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
/** @file icv_diff_test.c
 *
 * Tests for:
 *   - icv_diff  (per-channel counting, matching pixdiff semantics)
 *   - icv_diffimg (per-channel colour highlighting)
 *   - PNG metadata embedding/reading (icv_render_info round-trip)
 *   - icv_diff_render_info
 *   - icv_diff_nirt_shots (ray reconstruction from a single changed pixel)
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <sys/stat.h>

#include "bu/app.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "bu/vls.h"
#include "bu/file.h"
#include "icv.h"

/* Simple pass/fail accounting */
static int tests_run    = 0;
static int tests_passed = 0;

#define CHECK(cond, msg) do { \
    tests_run++; \
    if (cond) { \
	tests_passed++; \
    } else { \
	bu_log("FAIL [%s:%d]: %s\n", __FILE__, __LINE__, msg); \
    } \
} while (0)


/* ------------------------------------------------------------------ */
/* Helper: create a small solid-colour RGB image                       */
/* ------------------------------------------------------------------ */
static icv_image_t *
make_solid(size_t w, size_t h, double r, double g, double b)
{
    icv_image_t *img = icv_create(w, h, ICV_COLOR_SPACE_RGB);
    if (!img) return NULL;
    for (size_t i = 0; i < w * h; i++) {
	img->data[i*3+0] = r;
	img->data[i*3+1] = g;
	img->data[i*3+2] = b;
    }
    return img;
}


/* ================================================================== */
/* Test 1 – icv_diff counting (per-channel, matching pixdiff)          */
/* ================================================================== */
static void
test_icv_diff_counts(void)
{
    bu_log("\n=== Test 1: icv_diff channel counting ===\n");

    /* Two 2x1 images, both initially black */
    icv_image_t *img1 = make_solid(2, 1, 0.0, 0.0, 0.0);
    icv_image_t *img2 = make_solid(2, 1, 0.0, 0.0, 0.0);

    /* Make img2 pixel[0] red channel = 1/255 (off by 1) */
    img2->data[0] = 1.0/255.0;

    /* Make img2 pixel[1] green channel = 10/255 (off by many) */
    img2->data[1*3+1] = 10.0/255.0;

    int matching = 0, off_by_1 = 0, off_by_many = 0;
    int ret = icv_diff(&matching, &off_by_1, &off_by_many, img1, img2);

    /* 2 pixels × 3 channels = 6 total channels
     * pixel 0: r differs by 1 → off_by_1 += 1;  g,b match → matching += 2
     * pixel 1: g differs by 10 → off_by_many += 1;  r,b match → matching += 2
     * Total: matching=4, off_by_1=1, off_by_many=1
     */
    CHECK(ret == 1, "icv_diff returns 1 when images differ");
    CHECK(matching  == 4, "icv_diff matching channels = 4");
    CHECK(off_by_1  == 1, "icv_diff off_by_1 channels = 1");
    CHECK(off_by_many == 1, "icv_diff off_by_many channels = 1");

    bu_log("  matching=%d off_by_1=%d off_by_many=%d (expect 4,1,1)\n",
	   matching, off_by_1, off_by_many);

    icv_destroy(img1);
    icv_destroy(img2);

    /* Identical images */
    img1 = make_solid(3, 3, 0.5, 0.25, 0.125);
    img2 = make_solid(3, 3, 0.5, 0.25, 0.125);
    matching = off_by_1 = off_by_many = 0;
    ret = icv_diff(&matching, &off_by_1, &off_by_many, img1, img2);
    CHECK(ret == 0, "icv_diff returns 0 for identical images");
    CHECK(matching == 27, "icv_diff matching = 27 for identical 3x3 RGB images");
    CHECK(off_by_1 == 0, "icv_diff off_by_1 = 0 for identical images");
    CHECK(off_by_many == 0, "icv_diff off_by_many = 0 for identical images");
    icv_destroy(img1);
    icv_destroy(img2);
}


/* ================================================================== */
/* Test 2 – icv_diffimg per-channel colour highlighting                */
/* ================================================================== */
static void
test_icv_diffimg_colors(void)
{
    bu_log("\n=== Test 2: icv_diffimg per-channel colour ===\n");

    /* 3×1 image pair: pixel[0] differs only in red by >1,
     *                  pixel[1] differs only in green by 1,
     *                  pixel[2] matches.
     */
    icv_image_t *img1 = icv_create(3, 1, ICV_COLOR_SPACE_RGB);
    icv_image_t *img2 = icv_create(3, 1, ICV_COLOR_SPACE_RGB);

    /* pixel 0: img1 red=128/255, img2 red=50/255  → diff=78, only red */
    img1->data[0*3+0] = 128.0/255.0;
    img1->data[0*3+1] = 0.0;
    img1->data[0*3+2] = 0.0;
    img2->data[0*3+0] = 50.0/255.0;
    img2->data[0*3+1] = 0.0;
    img2->data[0*3+2] = 0.0;

    /* pixel 1: img1 green=100/255, img2 green=101/255 → diff=1, only green */
    img1->data[1*3+0] = 0.0;
    img1->data[1*3+1] = 100.0/255.0;
    img1->data[1*3+2] = 0.0;
    img2->data[1*3+0] = 0.0;
    img2->data[1*3+1] = 101.0/255.0;
    img2->data[1*3+2] = 0.0;

    /* pixel 2: identical (50, 50, 50) */
    img1->data[2*3+0] = img2->data[2*3+0] = 50.0/255.0;
    img1->data[2*3+1] = img2->data[2*3+1] = 50.0/255.0;
    img1->data[2*3+2] = img2->data[2*3+2] = 50.0/255.0;

    icv_image_t *diff = icv_diffimg(img1, img2);
    CHECK(diff != NULL, "icv_diffimg returns non-NULL");
    if (!diff) { icv_destroy(img1); icv_destroy(img2); return; }

    unsigned char *out = icv_data2uchar(diff);
    CHECK(out != NULL, "icv_diffimg output convertible to uchar");

    if (out) {
	/* pixel 0: red differs by many → R=0xFF, G=0, B=0 */
	CHECK(out[0*3+0] == 0xFF, "diffimg pixel0 R = 0xFF (red channel differs by many)");
	CHECK(out[0*3+1] == 0x00, "diffimg pixel0 G = 0x00");
	CHECK(out[0*3+2] == 0x00, "diffimg pixel0 B = 0x00");

	/* pixel 1: green differs by 1 → R=0, G=0xC0, B=0 */
	CHECK(out[1*3+0] == 0x00, "diffimg pixel1 R = 0x00");
	CHECK(out[1*3+1] == 0xC0, "diffimg pixel1 G = 0xC0 (green channel off by 1)");
	CHECK(out[1*3+2] == 0x00, "diffimg pixel1 B = 0x00");

	/* pixel 2: matching → half-intensity greyscale, all channels equal */
	CHECK(out[2*3+0] == out[2*3+1] && out[2*3+1] == out[2*3+2],
	      "diffimg pixel2 all channels equal (greyscale for matching pixel)");

	bu_log("  pixel0 diff RGB: (%u,%u,%u) expect (0xFF,0,0)\n",
	       out[0], out[1], out[2]);
	bu_log("  pixel1 diff RGB: (%u,%u,%u) expect (0,0xC0,0)\n",
	       out[3], out[4], out[5]);
	bu_log("  pixel2 diff RGB: (%u,%u,%u) expect equal greyscale\n",
	       out[6], out[7], out[8]);

	bu_free(out, "diffimg uchar output");
    }

    icv_destroy(img1);
    icv_destroy(img2);
    icv_destroy(diff);
}


/* ================================================================== */
/* Test 3 – PNG render_info metadata round-trip                        */
/* ================================================================== */
static void
test_png_metadata_roundtrip(const char *tmpdir)
{
    bu_log("\n=== Test 3: PNG render_info metadata round-trip ===\n");

    /* Build a test image with render metadata */
    icv_image_t *img = make_solid(4, 4, 0.5, 0.3, 0.1);
    CHECK(img != NULL, "created test image for PNG round-trip");
    if (!img) return;

    struct icv_render_info *ri = icv_render_info_create();
    ri->db_filename = bu_strdup("/some/path/test.g");
    ri->objects     = bu_strdup("sphere.r cube.r");

    /* Set a simple orthographic camera */
    MAT_IDN(ri->viewrotscale);
    VSET(ri->eye_model, 123.456789012345678, -987.654321098765432, 0.5);
    ri->viewsize    = 1234.5678901234567;
    ri->aspect      = 1.0;
    ri->perspective = 0.0;

    icv_image_set_render_info(img, ri);

    /* Write to a temp PNG file */
    struct bu_vls fname = BU_VLS_INIT_ZERO;
    bu_vls_printf(&fname, "%s/icv_diff_meta_test.png", tmpdir);

    int wret = icv_write(img, bu_vls_cstr(&fname), BU_MIME_IMAGE_PNG);
    CHECK(wret == BRLCAD_OK, "wrote PNG with render metadata");
    icv_destroy(img);

    /* Read it back */
    icv_image_t *img2 = icv_read(bu_vls_cstr(&fname), BU_MIME_IMAGE_PNG, 0, 0);
    CHECK(img2 != NULL, "read PNG back successfully");

    if (img2) {
	const struct icv_render_info *ri2 = icv_image_get_render_info(img2);
	CHECK(ri2 != NULL, "render_info present after PNG read-back");

	if (ri2) {
	    CHECK(ri2->db_filename && BU_STR_EQUAL(ri2->db_filename, "/some/path/test.g"),
		  "db_filename round-trips correctly");
	    CHECK(ri2->objects && BU_STR_EQUAL(ri2->objects, "sphere.r cube.r"),
		  "objects round-trips correctly");

	    /* Check eye_model with strict equality (values encoded at max precision) */
	    CHECK(fabs(ri2->eye_model[0] - 123.456789012345678) < 1e-10,
		  "eye_model[0] round-trips with high precision");
	    CHECK(fabs(ri2->eye_model[1] - (-987.654321098765432)) < 1e-10,
		  "eye_model[1] round-trips with high precision");
	    CHECK(fabs(ri2->viewsize - 1234.5678901234567) < 1e-8,
		  "viewsize round-trips with high precision");

	    bu_log("  db_filename: '%s'\n", ri2->db_filename ? ri2->db_filename : "(null)");
	    bu_log("  objects:     '%s'\n", ri2->objects     ? ri2->objects     : "(null)");
	    bu_log("  eye_model:   (%.17g, %.17g, %.17g)\n",
		   ri2->eye_model[0], ri2->eye_model[1], ri2->eye_model[2]);
	    bu_log("  viewsize:    %.17g\n", ri2->viewsize);
	}
	icv_destroy(img2);
    }

    /* Clean up temp file */
    bu_file_delete(bu_vls_cstr(&fname));
    bu_vls_free(&fname);
}


/* ================================================================== */
/* Test 4 – icv_diff_render_info metadata comparison                   */
/* ================================================================== */
static void
test_diff_render_info(void)
{
    bu_log("\n=== Test 4: icv_diff_render_info ===\n");

    icv_image_t *img1 = make_solid(2, 2, 0.1, 0.2, 0.3);
    icv_image_t *img2 = make_solid(2, 2, 0.1, 0.2, 0.3);

    /* No metadata on either */
    int r = icv_diff_render_info(img1, img2, NULL);
    CHECK(r == -1, "icv_diff_render_info returns -1 with no metadata on either image");

    /* Attach identical metadata */
    struct icv_render_info *ri1 = icv_render_info_create();
    ri1->db_filename = bu_strdup("model.g");
    ri1->objects     = bu_strdup("all.r");
    VSET(ri1->eye_model, 0, 0, 1000);
    ri1->viewsize = 500.0;
    ri1->aspect = 1.0;
    MAT_IDN(ri1->viewrotscale);
    icv_image_set_render_info(img1, ri1);

    struct icv_render_info *ri2 = icv_render_info_create();
    ri2->db_filename = bu_strdup("model.g");
    ri2->objects     = bu_strdup("all.r");
    VSET(ri2->eye_model, 0, 0, 1000);
    ri2->viewsize = 500.0;
    ri2->aspect = 1.0;
    MAT_IDN(ri2->viewrotscale);
    icv_image_set_render_info(img2, ri2);

    struct bu_vls msgs = BU_VLS_INIT_ZERO;
    r = icv_diff_render_info(img1, img2, &msgs);
    CHECK(r == 0, "icv_diff_render_info returns 0 for identical metadata");
    bu_log("  identical report: %s\n", bu_vls_cstr(&msgs));
    bu_vls_trunc(&msgs, 0);

    /* Replace img2's render info with a different db_filename */
    struct icv_render_info *ri3 = icv_render_info_create();
    ri3->db_filename = bu_strdup("OTHER.g");
    ri3->objects     = bu_strdup("all.r");
    VSET(ri3->eye_model, 0, 0, 1000);
    ri3->viewsize = 500.0;
    ri3->aspect = 1.0;
    MAT_IDN(ri3->viewrotscale);
    /* icv_image_set_render_info will free ri2 and install ri3 */
    icv_image_set_render_info(img2, ri3);

    r = icv_diff_render_info(img1, img2, &msgs);
    CHECK(r == 1, "icv_diff_render_info returns 1 when db_filename differs");
    bu_log("  different db report: %s\n", bu_vls_cstr(&msgs));
    bu_vls_free(&msgs);

    icv_destroy(img1);
    icv_destroy(img2);
}


/* ================================================================== */
/* Test 5 – icv_diff_nirt_shots ray reconstruction                     */
/* ================================================================== */
static void
test_nirt_shots(const char *tmpdir)
{
    bu_log("\n=== Test 5: icv_diff_nirt_shots ray reconstruction ===\n");

    /* 4×4 orthographic image – identity view (looking down -Z from +Z) */
    const size_t IMG_W = 4, IMG_H = 4;
    icv_image_t *img1 = make_solid(IMG_W, IMG_H, 0.0, 0.0, 0.0);
    icv_image_t *img2 = make_solid(IMG_W, IMG_H, 0.0, 0.0, 0.0);

    /* Change pixel (col=2, row=1) red channel in img2 */
    const size_t test_col = 2;
    const size_t test_row = 1;
    img2->data[(test_row * IMG_W + test_col) * 3 + 0] = 1.0;  /* max red */

    /* Orthographic camera looking straight down -Z, eye at origin */
    struct icv_render_info *ri = icv_render_info_create();
    ri->db_filename = bu_strdup("test_scene.g");
    ri->objects     = bu_strdup("sph.r");

    /* Identity Viewrotscale (no rotation) so view == model axes */
    MAT_IDN(ri->viewrotscale);
    /* viewrotscale[15] will be set to 0.5*viewsize inside grid_setup;
     * we store it here as 0.5*viewsize directly (matching rt convention) */
    ri->viewrotscale[15] = 0.5 * 400.0;  /* 0.5 * viewsize */

    VSET(ri->eye_model, 0.0, 0.0, 0.0);
    ri->viewsize    = 400.0;    /* view is 400 mm wide → cell = 100 mm */
    ri->aspect      = 1.0;
    ri->perspective = 0.0;

    icv_image_set_render_info(img1, ri);

    /* Sub-test A: only img1 has metadata → only nirt_out1 is written */
    {
	struct bu_vls nirt_fname = BU_VLS_INIT_ZERO;
	bu_vls_printf(&nirt_fname, "%s/test_shots_1.nirt", tmpdir);
	FILE *fp = fopen(bu_vls_cstr(&nirt_fname), "w");
	CHECK(fp != NULL, "opened nirt output file (img1 only)");

	if (fp) {
	    int nshots = icv_diff_nirt_shots(img1, img2, fp, NULL);
	    fclose(fp);
	    CHECK(nshots == 1, "icv_diff_nirt_shots (img1-only) found exactly 1 differing pixel");
	    bu_log("  nshots = %d  (expect 1)\n", nshots);

	    /* Read the file back and spot-check the xyz line */
	    FILE *rfp = fopen(bu_vls_cstr(&nirt_fname), "r");
	    CHECK(rfp != NULL, "re-opened nirt output file for verification");
	    if (rfp) {
		char line[512];
		int found_xyz = 0;
		double lx = 0, ly = 0, lz = 0;
		while (bu_fgets(line, sizeof(line), rfp)) {
		    if (bu_strncmp(line, "xyz ", 4) == 0) {
			if (sscanf(line + 4, "%lf %lf %lf", &lx, &ly, &lz) == 3)
			    found_xyz = 1;
		    }
		}
		fclose(rfp);

		CHECK(found_xyz, "nirt file contains an xyz command");

		if (found_xyz) {
		    /*
		     * For the identity view, viewbase is at (-1, -1/aspect, 0) in
		     * view space.  With viewsize=400, view2model is the identity
		     * (after the eye translation which is zero here), so:
		     *
		     *   viewbase_model = (-1, -1, 0) world  (in normalised coords)
		     *   cell_width  = 400/4 = 100
		     *   cell_height = 400/4 = 100
		     *   dx_model = (100, 0, 0)
		     *   dy_model = (0, 100, 0)
		     *
		     * pixel(col=2, row=1):
		     *   x = -200 + 2*100 = 0  mm
		     *   y = -200 + 1*100 = -100 mm
		     *   z = 0
		     */
		    const double expected_x = -200.0 + test_col * 100.0;  /* 0 */
		    const double expected_y = -200.0 + test_row * 100.0;  /* -100 */
		    const double expected_z = 0.0;
		    const double tol = 1e-6;

		    bu_log("  xyz read back: (%.6g, %.6g, %.6g)\n", lx, ly, lz);
		    bu_log("  expected:      (%.6g, %.6g, %.6g)\n",
			   expected_x, expected_y, expected_z);

		    CHECK(fabs(lx - expected_x) < tol, "nirt shot x coordinate correct");
		    CHECK(fabs(ly - expected_y) < tol, "nirt shot y coordinate correct");
		    CHECK(fabs(lz - expected_z) < tol, "nirt shot z coordinate correct");
		}
	    }
	}

	bu_file_delete(bu_vls_cstr(&nirt_fname));
	bu_vls_free(&nirt_fname);
    }

    /* Sub-test B: both images have metadata → both outputs are written.
     * Give img2 a different db_filename/objects (different scene) to verify
     * that the function still writes both scripts regardless. */
    {
	struct icv_render_info *ri2 = icv_render_info_create();
	ri2->db_filename = bu_strdup("other_scene.g");  /* different from img1! */
	ri2->objects     = bu_strdup("cube.r");
	MAT_IDN(ri2->viewrotscale);
	ri2->viewrotscale[15] = 0.5 * 400.0;
	VSET(ri2->eye_model, 10.0, 0.0, 0.0);          /* slightly different eye */
	ri2->viewsize    = 400.0;
	ri2->aspect      = 1.0;
	ri2->perspective = 0.0;
	icv_image_set_render_info(img2, ri2);

	struct bu_vls fname1 = BU_VLS_INIT_ZERO;
	struct bu_vls fname2 = BU_VLS_INIT_ZERO;
	bu_vls_printf(&fname1, "%s/test_shots_both1.nirt", tmpdir);
	bu_vls_printf(&fname2, "%s/test_shots_both2.nirt", tmpdir);

	FILE *fp1 = fopen(bu_vls_cstr(&fname1), "w");
	FILE *fp2 = fopen(bu_vls_cstr(&fname2), "w");
	CHECK(fp1 != NULL, "opened nirt output file for img1 (both-metadata test)");
	CHECK(fp2 != NULL, "opened nirt output file for img2 (both-metadata test)");

	if (fp1 && fp2) {
	    int ns = icv_diff_nirt_shots(img1, img2, fp1, fp2);
	    fclose(fp1);
	    fclose(fp2);
	    CHECK(ns == 1, "icv_diff_nirt_shots (both metadata) found exactly 1 differing pixel");

	    /* Both output files should be non-empty */
	    struct stat sb1, sb2;
	    int r1s = stat(bu_vls_cstr(&fname1), &sb1);
	    int r2s = stat(bu_vls_cstr(&fname2), &sb2);
	    CHECK(r1s == 0 && sb1.st_size > 0, "img1 nirt script is non-empty");
	    CHECK(r2s == 0 && sb2.st_size > 0, "img2 nirt script is non-empty");
	} else {
	    if (fp1) fclose(fp1);
	    if (fp2) fclose(fp2);
	}

	bu_file_delete(bu_vls_cstr(&fname1));
	bu_file_delete(bu_vls_cstr(&fname2));
	bu_vls_free(&fname1);
	bu_vls_free(&fname2);
    }

    /* Sub-test C: img1 has no render_info but img2 does → only fp2 is written */
    {
	icv_image_t *img_a = make_solid(IMG_W, IMG_H, 0.0, 0.0, 0.0);
	icv_image_t *img_b = make_solid(IMG_W, IMG_H, 0.0, 0.0, 0.0);

	img_b->data[(test_row * IMG_W + test_col) * 3 + 0] = 1.0;

	struct icv_render_info *ri3 = icv_render_info_create();
	ri3->db_filename = bu_strdup("test_scene.g");
	ri3->objects     = bu_strdup("sph.r");
	MAT_IDN(ri3->viewrotscale);
	ri3->viewrotscale[15] = 0.5 * 400.0;
	VSET(ri3->eye_model, 0.0, 0.0, 0.0);
	ri3->viewsize    = 400.0;
	ri3->aspect      = 1.0;
	ri3->perspective = 0.0;
	icv_image_set_render_info(img_b, ri3);

	struct bu_vls fname_c = BU_VLS_INIT_ZERO;
	bu_vls_printf(&fname_c, "%s/test_shots_c.nirt", tmpdir);
	FILE *fp_c = fopen(bu_vls_cstr(&fname_c), "w");
	CHECK(fp_c != NULL, "opened nirt output for img2-only render_info");
	if (fp_c) {
	    /* Pass NULL for fp1 since img_a has no render_info */
	    int ns = icv_diff_nirt_shots(img_a, img_b, NULL, fp_c);
	    fclose(fp_c);
	    CHECK(ns == 1, "icv_diff_nirt_shots works when only img2 has render_info");
	}
	bu_file_delete(bu_vls_cstr(&fname_c));
	bu_vls_free(&fname_c);
	icv_destroy(img_a);
	icv_destroy(img_b);
    }

    icv_destroy(img1);
    icv_destroy(img2);
}


/* ================================================================== */
/* main                                                                 */
/* ================================================================== */
int
main(int argc, char *argv[])
{
    bu_setprogname(argv[0]);

    /* Determine a writable temp directory */
    const char *tmpdir = "/tmp";
    if (argc > 1)
	tmpdir = argv[1];

    test_icv_diff_counts();
    test_icv_diffimg_colors();
    test_png_metadata_roundtrip(tmpdir);
    test_diff_render_info();
    test_nirt_shots(tmpdir);

    bu_log("\n=== Results: %d/%d tests passed ===\n", tests_passed, tests_run);

    return (tests_passed == tests_run) ? 0 : 1;
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
