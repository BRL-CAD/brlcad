/*                        D M _ U T I L . C
 * BRL-CAD
 *
 * Copyright (c) 1988-2021 United States Government as represented by
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
/** @file libdm/dm_util.c
 */

#include "common.h"
#include <string.h>
#include "bn.h"
#include "dm.h"

#include "./include/private.h"

#  ifdef HAVE_GL_GL_H
#    include <GL/gl.h>
#  endif

int
drawLine3D(struct dm *dmp, point_t pt1, point_t pt2, const char *log_bu, float *wireColor)
{
    if (!dmp) {
	return BRLCAD_ERROR;
    }

    if (dmp->i->dm_debugLevel) {
	bu_log("%s", log_bu);
	bu_log("drawLine3D: %f,%f,%f -> %f,%f,%f", V3ARGS(pt1), V3ARGS(pt2));
	if (wireColor) {
	    bu_log("drawLine3D: have wirecolor");
	}
    }

#ifdef HAVE_GL_GL_H
    static float black[4] = {0.0, 0.0, 0.0, 0.0};
    GLdouble pt[3];
    if (dmp->i->dm_debugLevel) {
	GLfloat pmat[16];

	glGetFloatv(GL_PROJECTION_MATRIX, pmat);
	bu_log("projection matrix:\n");
	bu_log("%g %g %g %g\n", pmat[0], pmat[4], pmat[8], pmat[12]);
	bu_log("%g %g %g %g\n", pmat[1], pmat[5], pmat[9], pmat[13]);
	bu_log("%g %g %g %g\n", pmat[2], pmat[6], pmat[10], pmat[14]);
	bu_log("%g %g %g %g\n", pmat[3], pmat[7], pmat[11], pmat[15]);
	glGetFloatv(GL_MODELVIEW_MATRIX, pmat);
	bu_log("modelview matrix:\n");
	bu_log("%g %g %g %g\n", pmat[0], pmat[4], pmat[8], pmat[12]);
	bu_log("%g %g %g %g\n", pmat[1], pmat[5], pmat[9], pmat[13]);
	bu_log("%g %g %g %g\n", pmat[2], pmat[6], pmat[10], pmat[14]);
	bu_log("%g %g %g %g\n", pmat[3], pmat[7], pmat[11], pmat[15]);
    }

    if (dmp->i->dm_light) {
	glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, wireColor);
	glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, black);
	glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, black);
	glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, black);

	if (dmp->i->dm_transparency)
	    glDisable(GL_BLEND);
    }

    glBegin(GL_LINES);
    VMOVE(pt, pt1); /* fastf_t to GLdouble */
    glVertex3dv(pt);
    VMOVE(pt, pt2); /* fastf_t to GLdouble */
    glVertex3dv(pt);
    glEnd();
#endif

    return BRLCAD_OK;
}

int
drawLines3D(struct dm *dmp, int npoints, point_t *points, int lflag, const char *log_bu, float *wireColor)
{
    if (!dmp) {
	return BRLCAD_ERROR;
    }
    if (npoints < 2 || (!lflag && npoints%2)) {
	return BRLCAD_OK;
    }

    if (dmp->i->dm_debugLevel) {
	bu_log("%s", log_bu);
	bu_log("drawLines3D %d pnts, flag %d: %f,%f,%f -> %f,%f,%f", npoints, lflag, V3ARGS(points[0]), V3ARGS(points[npoints - 1]));
	if (wireColor) {
	    bu_log("drawLine3D: have wirecolor");
	}
    }

#ifdef HAVE_GL_GL_H
    register int i;
    static float black[4] = {0.0, 0.0, 0.0, 0.0};
    if (dmp->i->dm_debugLevel) {
	GLfloat pmat[16];

	glGetFloatv(GL_PROJECTION_MATRIX, pmat);
	bu_log("projection matrix:\n");
	bu_log("%g %g %g %g\n", pmat[0], pmat[4], pmat[8], pmat[12]);
	bu_log("%g %g %g %g\n", pmat[1], pmat[5], pmat[9], pmat[13]);
	bu_log("%g %g %g %g\n", pmat[2], pmat[6], pmat[10], pmat[14]);
	bu_log("%g %g %g %g\n", pmat[3], pmat[7], pmat[11], pmat[15]);
	glGetFloatv(GL_MODELVIEW_MATRIX, pmat);
	bu_log("modelview matrix:\n");
	bu_log("%g %g %g %g\n", pmat[0], pmat[4], pmat[8], pmat[12]);
	bu_log("%g %g %g %g\n", pmat[1], pmat[5], pmat[9], pmat[13]);
	bu_log("%g %g %g %g\n", pmat[2], pmat[6], pmat[10], pmat[14]);
	bu_log("%g %g %g %g\n", pmat[3], pmat[7], pmat[11], pmat[15]);
    }


    if (dmp->i->dm_light) {
	glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, wireColor);
	glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, black);
	glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, black);
	glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, black);

	if (dmp->i->dm_transparency)
	    glDisable(GL_BLEND);
    }

    if (lflag)
	glBegin(GL_LINE_LOOP);
    else
	glBegin(GL_LINES);

    for (i = 0; i < npoints; ++i) {
	GLdouble pt[3];
	VMOVE(pt, points[i]); /* fastf_t to GLdouble */
	glVertex3dv(pt);
    }

    glEnd();
#endif

    return BRLCAD_OK;
}

int
drawLine2D(struct dm *dmp, fastf_t X1, fastf_t Y1, fastf_t X2, fastf_t Y2, const char *log_bu)
{
    if (!dmp) {
	return BRLCAD_ERROR;
    }

    if (dmp->i->dm_debugLevel) {
	bu_log("%s", log_bu);
	bu_log("drawLine2D: %f,%f -> %f,%f", X1, Y1, X2, Y2);
    }

#ifdef HAVE_GL_GL_H
    if (dmp->i->dm_debugLevel) {
	GLfloat pmat[16];

	glGetFloatv(GL_PROJECTION_MATRIX, pmat);
	bu_log("projection matrix:\n");
	bu_log("%g %g %g %g\n", pmat[0], pmat[4], pmat[8], pmat[12]);
	bu_log("%g %g %g %g\n", pmat[1], pmat[5], pmat[9], pmat[13]);
	bu_log("%g %g %g %g\n", pmat[2], pmat[6], pmat[10], pmat[14]);
	bu_log("%g %g %g %g\n", pmat[3], pmat[7], pmat[11], pmat[15]);
	glGetFloatv(GL_MODELVIEW_MATRIX, pmat);
	bu_log("modelview matrix:\n");
	bu_log("%g %g %g %g\n", pmat[0], pmat[4], pmat[8], pmat[12]);
	bu_log("%g %g %g %g\n", pmat[1], pmat[5], pmat[9], pmat[13]);
	bu_log("%g %g %g %g\n", pmat[2], pmat[6], pmat[10], pmat[14]);
	bu_log("%g %g %g %g\n", pmat[3], pmat[7], pmat[11], pmat[15]);
    }

    glBegin(GL_LINES);
    glVertex2f(X1, Y1);
    glVertex2f(X2, Y2);
    glEnd();
#endif

    return BRLCAD_OK;
}

int
draw_Line3D(struct dm *dmp, point_t pt1, point_t pt2)
{
    if (!dmp)
	return BRLCAD_ERROR;

    if (bn_pnt3_pnt3_equal(pt1, pt2, NULL)) {
	/* nothing to do for a singular point */
	return BRLCAD_OK;
    }

    return BRLCAD_OK;
}


void
flip_display_image_vertically(unsigned char *image, size_t width, size_t height)
{
    size_t i, j;
    size_t row_bytes = width * 3 * sizeof(unsigned char);
    size_t img_bytes = row_bytes * height;
    unsigned char *inv_img = (unsigned char *)bu_malloc(img_bytes,
	    "inverted image");

    for (i = 0, j = height - 1; i < height; ++i, --j) {
	memcpy(inv_img + i * row_bytes, image + j * row_bytes, row_bytes);
    }
    memcpy(image, inv_img, img_bytes);
    bu_free(inv_img, "inverted image");
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
