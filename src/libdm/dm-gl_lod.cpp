/*                    D M - G L _ L O D . C P P
 * BRL-CAD
 *
 * Copyright (c) 1988-2022 United States Government as represented by
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
/** @file libdm/dm-gl_lod.cpp
 *
 * OpenGL logic for rendering LoD structures.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "vmath.h"
#include "bu.h"
#include "bn.h"
extern "C" {
#include "bv/defines.h"
#include "bg/lod.h"
#include "dm.h"
#include "./dm-gl.h"
#include "./include/private.h"
}

extern "C"
int gl_draw_tri(struct dm *dmp, struct bv_mesh_lod_info *info)
{
    int fset_cnt = info->fset_cnt;
    int *fset = info->fset;
    int fcnt = info->fcnt;
    const int *faces = info->faces;
    const point_t *points = info->points;
    const point_t *points_orig = info->points_orig;
    const int *face_normals = info->face_normals;
    const vect_t *normals = info->normals;
    int mode = info->s->s_os.s_dmode;

    struct gl_vars *mvars = (struct gl_vars *)dmp->i->m_vars;
    GLdouble dpt[3];
    static float black[4] = {0.0, 0.0, 0.0, 0.0};
    GLfloat originalLineWidth;

    if (mode < 0 || mode > 1)
	return BRLCAD_ERROR;

    glGetFloatv(GL_LINE_WIDTH, &originalLineWidth);

    gl_debug_print(dmp, "gl_draw_tri", dmp->i->dm_debugLevel);

    // Wireframe
    if (mode == 0) {

	if (dmp->i->dm_light) {
	    glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, mvars->i.wireColor);
	    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, black);
	    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, black);
	    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, black);
	    if (dmp->i->dm_transparency)
		glDisable(GL_BLEND);
	}

	if (!fset) {
	    // Draw all the triangles in faces array
	    for (int i = 0; i < fcnt; i++) {
		glBegin(GL_LINE_STRIP);
		VMOVE(dpt, points[faces[3*i+0]]);
		glVertex3dv(dpt);
		VMOVE(dpt, points[faces[3*i+1]]);
		glVertex3dv(dpt);
		VMOVE(dpt, points[faces[3*i+2]]);
		glVertex3dv(dpt);
		glEnd();
	    }
	} else {
	    // Draw the specified triangles
	    for (int i = 0; i < fset_cnt; i++) {
		glBegin(GL_LINE_STRIP);
		VMOVE(dpt, points[faces[3*fset[i]+0]]);
		glVertex3dv(dpt);
		VMOVE(dpt, points[faces[3*fset[i]+1]]);
		glVertex3dv(dpt);
		VMOVE(dpt, points[faces[3*fset[i]+2]]);
		glVertex3dv(dpt);
		glEnd();
	    }
	}

	if (dmp->i->dm_light && dmp->i->dm_transparency)
	    glDisable(GL_BLEND);

	glLineWidth(originalLineWidth);

	return BRLCAD_OK;
    }

    // Shaded
    if (mode == 1) {
	if (dmp->i->dm_light) {

	    glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, black);
	    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, mvars->i.ambientColor);
	    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, mvars->i.specularColor);
	    glMaterialfv(GL_FRONT, GL_DIFFUSE, mvars->i.diffuseColor);

	    switch (dmp->i->dm_light) {
		case 1:
		    break;
		case 2:
		    glMaterialfv(GL_BACK, GL_DIFFUSE, mvars->i.diffuseColor);
		    break;
		case 3:
		    glMaterialfv(GL_BACK, GL_DIFFUSE, mvars->i.backDiffuseColorDark);
		    break;
		default:
		    glMaterialfv(GL_BACK, GL_DIFFUSE, mvars->i.backDiffuseColorLight);
		    break;
	    }

	    if (dmp->i->dm_transparency)
		glEnable(GL_BLEND);
	}

	glBegin(GL_TRIANGLES);

	if (!fset) {
	    // Draw all the triangles in faces array
	    for (int i = 0; i < fcnt; i++) {

		// Set surface normal
		vect_t ab, ac, norm;
		VSUB2(ab, points_orig[faces[3*i+0]], points_orig[faces[3*i+1]]);
		VSUB2(ac, points_orig[faces[3*i+0]], points_orig[faces[3*i+2]]);
		VCROSS(norm, ab, ac);
		VUNITIZE(norm);
		glNormal3dv(norm);

		if (face_normals && normals) {
		    bu_log("todo - per-vertex normals\n");
		}
		VMOVE(dpt, points[faces[3*i+0]]);
		glVertex3dv(dpt);
		if (face_normals && normals) {
		    bu_log("todo - per-vertex normals\n");
		}
		VMOVE(dpt, points[faces[3*i+1]]);
		glVertex3dv(dpt);
		if (face_normals && normals) {
		    bu_log("todo - per-vertex normals\n");
		}
		VMOVE(dpt, points[faces[3*i+2]]);
		glVertex3dv(dpt);

	    }
	} else {
	    // Draw the specified triangles
	    for (int i = 0; i < fset_cnt; i++) {
		// Set surface normal
		vect_t ab, ac, norm;
		VSUB2(ab, points[faces[3*fset[i]+0]], points[faces[3*fset[i]+1]]);
		VSUB2(ac, points[faces[3*fset[i]+0]], points[faces[3*fset[i]+2]]);
		VCROSS(norm, ab, ac);
		VUNITIZE(norm);
		glNormal3dv(norm);

		if (face_normals && normals) {
		    bu_log("todo - per-vertex normals\n");
		}
		VMOVE(dpt, points[faces[3*fset[i]+0]]);
		glVertex3dv(dpt);
		if (face_normals && normals) {
		    bu_log("todo - per-vertex normals\n");
		}
		VMOVE(dpt, points[faces[3*fset[i]+1]]);
		glVertex3dv(dpt);
		if (face_normals && normals) {
		    bu_log("todo - per-vertex normals\n");
		}
		VMOVE(dpt, points[faces[3*fset[i]+2]]);
		glVertex3dv(dpt);
	    }
	}

	glEnd();

	if (dmp->i->dm_light && dmp->i->dm_transparency)
	    glDisable(GL_BLEND);

	glLineWidth(originalLineWidth);

	return BRLCAD_OK;
    }

    return BRLCAD_ERROR;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
