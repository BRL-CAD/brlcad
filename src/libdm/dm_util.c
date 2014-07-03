/*                        D M _ U T I L . C
 * BRL-CAD
 *
 * Copyright (c) 1988-2014 United States Government as represented by
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

#include "bu.h"
#include "bn.h"
#include "dm.h"

#include "./dm_util.h"

#if defined(DM_OGL) || defined(DM_WGL) || defined(DM_RTGL)
int
drawLine3D(struct dm *dmp, point_t pt1, point_t pt2, const char *log_bu, float *wireColor)
{
    static float black[4] = {0.0, 0.0, 0.0, 0.0};
    GLdouble pt[3];

    if (dmp->dm_debugLevel)
	bu_log(log_bu);

    if (dmp->dm_debugLevel) {
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

    if (dmp->dm_light) {
	glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, wireColor);
	glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, black);
	glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, black);
	glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, black);

	if (dmp->dm_transparency)
	    glDisable(GL_BLEND);
    }

    glBegin(GL_LINES);
    VMOVE(pt, pt1); /* fastf_t to GLdouble */
    glVertex3dv(pt);
    VMOVE(pt, pt2); /* fastf_t to GLdouble */
    glVertex3dv(pt);
    glEnd();

    return TCL_OK;
}

int
drawLines3D(struct dm *dmp, int npoints, point_t *points, int sflag, const char *log_bu, float *wireColor)
{
    register int i;
    static float black[4] = {0.0, 0.0, 0.0, 0.0};

    if (dmp->dm_debugLevel)
	bu_log(log_bu);

    if (dmp->dm_debugLevel) {
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

    if (npoints < 2 || (!sflag && npoints%2))
	return TCL_OK;

    if (dmp->dm_light) {
	glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, wireColor);
	glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, black);
	glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, black);
	glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, black);

	if (dmp->dm_transparency)
	    glDisable(GL_BLEND);
    }

    if (sflag)
	glBegin(GL_LINE_STRIP);
    else
	glBegin(GL_LINES);

    for (i = 0; i < npoints; ++i) {
	GLdouble pt[3];
	VMOVE(pt, points[i]); /* fastf_t to GLdouble */
	glVertex3dv(pt);
    }

    glEnd();

    return TCL_OK;
}

int
drawLine2D(struct dm *dmp, fastf_t X1, fastf_t Y1, fastf_t X2, fastf_t Y2, const char *log_bu)
{
    if (dmp->dm_debugLevel)
	bu_log(log_bu);

    if (dmp->dm_debugLevel) {
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

    return TCL_OK;
}
#endif

int
draw_Line3D(struct dm *dmp, point_t pt1, point_t pt2)
{
    if (!dmp)
	return TCL_ERROR;

    if (bn_pt3_pt3_equal(pt1, pt2, NULL)) {
	/* nothing to do for a singular point */
	return TCL_OK;
    }

    return TCL_OK;
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
