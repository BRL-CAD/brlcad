/*                        D M - G L . C
 * BRL-CAD
 *
 * Copyright (c) 1988-2024 United States Government as represented by
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
/** @file libdm/dm-gl.c
 *
 * OpenGL Display Manager logic common to various OpenGL based
 * libdm backend implementations.
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
#include "bv/defines.h"
#include "dm.h"
#include "./dm-gl.h"
#include "./include/private.h"


#define ENABLE_POINT_SMOOTH 1

#define VIEWFACTOR      (1.0/(*dmp->i->dm_vp))
#define VIEWSIZE        (2.0*(*dmp->i->dm_vp))

/* these are from /usr/include/gl.h could be device dependent */
#define XMAXSCREEN	1279
#define YMAXSCREEN	1023
#define YSTEREO		491	/* subfield height, in scanlines */
#define YOFFSET_LEFT	532	/* YSTEREO + YBLANK ? */

/* Display Manager package interface */
#define IRBOUND 4095.9	/* Max magnification in Rot matrix */
#define PLOTBOUND 1000.0	/* Max magnification in Rot matrix */

void gl_printmat(struct bu_vls *tmp_vls, fastf_t *mat) {
    bu_vls_printf(tmp_vls, "%g %g %g %g\n", mat[0], mat[4], mat[8], mat[12]);
    bu_vls_printf(tmp_vls, "%g %g %g %g\n", mat[1], mat[5], mat[9], mat[13]);
    bu_vls_printf(tmp_vls, "%g %g %g %g\n", mat[2], mat[6], mat[10], mat[14]);
    bu_vls_printf(tmp_vls, "%g %g %g %g\n", mat[3], mat[7], mat[11], mat[15]);
}

void gl_printglmat(struct bu_vls *tmp_vls, GLfloat *m) {
    bu_vls_printf(tmp_vls, "   %g %g %g %g\n", m[0], m[4], m[8], m[12]);
    bu_vls_printf(tmp_vls, "   %g %g %g %g\n", m[1], m[5], m[9], m[13]);
    bu_vls_printf(tmp_vls, "   %g %g %g %g\n", m[2], m[6], m[10], m[14]);
    bu_vls_printf(tmp_vls, "   %g %g %g %g\n", m[3], m[7], m[11], m[15]);
}

static void
gl_debug_log(struct dm *dmp, struct bu_vls *msg)
{
    if (bu_vls_strlen(&dmp->i->dm_log)) {
	FILE* fp = fopen(bu_vls_cstr(&dmp->i->dm_log), "a");
	if (fp) {
	    fprintf(fp, "%s", bu_vls_cstr(msg));
	    fclose(fp);
	    return;
	}
    }
    bu_log("%s", bu_vls_cstr(msg));
}

void
gl_debug_print(struct dm *dmp, const char *title, int lvl)
{
    if (!lvl)
	return;

    struct bu_vls msg = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&msg, "%s\n", title);

    if (lvl > 3) {
	GLfloat mvmat[16], pmat[16];
	glGetFloatv(GL_MODELVIEW_MATRIX, mvmat);
	bu_vls_printf(&msg, "   MODELVIEW:\n");
	gl_printglmat(&msg, (GLfloat *)mvmat);

	glGetFloatv(GL_PROJECTION_MATRIX, pmat);
	bu_vls_printf(&msg, "   PROJECTION:\n");
	gl_printglmat(&msg, (GLfloat *)pmat);
    }

    if (lvl > 2) {
	int glerror = 0;
	while ((glerror = glGetError())!=0) {
	    bu_vls_printf(&msg, "   Error: %x\n", glerror);
	}
    }

    gl_debug_log(dmp, &msg);
    bu_vls_free(&msg);
}

void glvars_init(struct dm *dmp)
{
    struct gl_vars *mvars = (struct gl_vars *)dmp->i->m_vars;
    mvars->i.default_viewscale = 1000.0;
    mvars->i.xlim_view = 1.0;
    mvars->i.ylim_view = 1.0;
    mvars->i.amb_three[0] = 0.3;
    mvars->i.amb_three[1] = 0.3;
    mvars->i.amb_three[2] = 0.3;
    mvars->i.amb_three[3] = 1.0;
    mvars->i.light0_position[0] = 0.0;
    mvars->i.light0_position[1] = 0.0;
    mvars->i.light0_position[2] = 1.0;
    mvars->i.light0_position[3] = 0.0;
    /* Initialize to white */
    mvars->i.light0_diffuse[0] = 1.0;
    mvars->i.light0_diffuse[1] = 1.0;
    mvars->i.light0_diffuse[2] = 1.0;
    mvars->i.light0_diffuse[3] = 1.0;

    /* Unless the app tells us different, assume OpenGL based
     * displays are capable of transparency */
    mvars->transparency_on = 1;
}

void gl_fogHint(struct dm *dmp, int fastfog)
{
    gl_debug_print(dmp, "gl_fogHint", dmp->i->dm_debugLevel);

    struct gl_vars *mvars = (struct gl_vars *)dmp->i->m_vars;
    mvars->fastfog = fastfog;
    glHint(GL_FOG_HINT, fastfog ? GL_FASTEST : GL_NICEST);
}

static void
gl_gradient_bg(struct gl_vars *mvars)
{
    glDisable(GL_LIGHTING);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-1.0, 1.0, -1.0, 1.0, 1.0, -1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glDisable(GL_DEPTH_TEST); // Color buffer only rendering
    glBegin(GL_QUADS);
    glColor3f(mvars->i.r1, mvars->i.g1, mvars->i.b1);
    glVertex2f(-1.0f, -1.0f);
    glVertex2f( 1.0f, -1.0f);
    glColor3f(mvars->i.r2, mvars->i.g2, mvars->i.b2);
    glVertex2f( 1.0f, 1.0f);
    glVertex2f(-1.0f, 1.0f);
    glEnd();
    glEnable(GL_LIGHTING);
    glClear(GL_DEPTH_BUFFER_BIT);

    if (mvars->zbuffer_on) {
	glDepthFunc(GL_LEQUAL);
	glEnable(GL_DEPTH_TEST);
    } else {
	glDisable(GL_DEPTH_TEST);
    }
}

int gl_setBGColor(struct dm *dmp,
	unsigned char r1, unsigned char g1, unsigned char b1,
	unsigned char r2, unsigned char g2, unsigned char b2
	)
{
    gl_debug_print(dmp, "gl_setBGColor", dmp->i->dm_debugLevel);

    struct gl_vars *mvars = (struct gl_vars *)dmp->i->m_vars;

    dmp->i->dm_bg1[0] = r1;
    dmp->i->dm_bg1[1] = g1;
    dmp->i->dm_bg1[2] = b1;
    dmp->i->dm_bg2[0] = r2;
    dmp->i->dm_bg2[1] = g2;
    dmp->i->dm_bg2[2] = b2;

    mvars->i.r1 = r1 / 255.0;
    mvars->i.g1 = g1 / 255.0;
    mvars->i.b1 = b1 / 255.0;
    mvars->i.r2 = r2 / 255.0;
    mvars->i.g2 = g2 / 255.0;
    mvars->i.b2 = b2 / 255.0;

    if (mvars->doublebuffer) {
	if (dmp->i->dm_SwapBuffers) {
	    (*dmp->i->dm_SwapBuffers)(dmp);
	}
	if (r1 == r2 && g1 == g2 && b1 == b2) {
	    glClearColor(mvars->i.r1, mvars->i.g1, mvars->i.b1, 0.0);
	    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	} else {
	    // Have two colors - do gradient
	    gl_gradient_bg(mvars);
	}
    }

    return BRLCAD_OK;
}

int gl_reshape(struct dm *dmp, int width, int height)
{
    GLint mm;

    struct gl_vars *mvars = (struct gl_vars *)dmp->i->m_vars;

    dmp->i->dm_height = height;
    dmp->i->dm_width = width;
    dmp->i->dm_aspect = (fastf_t)dmp->i->dm_width / (fastf_t)dmp->i->dm_height;

    gl_debug_print(dmp, "gl_reshape", dmp->i->dm_debugLevel);
    if (dmp->i->dm_debugLevel > 1) {
	struct bu_vls msg = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&msg, "width = %d, height = %d\n", dmp->i->dm_width, dmp->i->dm_height);
	gl_debug_log(dmp, &msg);
	bu_vls_free(&msg);
    }

    glViewport(0, 0, dmp->i->dm_width, dmp->i->dm_height);

    if (dmp->i->dm_bg1[0] == dmp->i->dm_bg2[0] && dmp->i->dm_bg1[1] == dmp->i->dm_bg2[1] && dmp->i->dm_bg1[2] == dmp->i->dm_bg2[2]) {
	glClearColor(mvars->i.r1, mvars->i.g1, mvars->i.b1, 0.0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    } else {
	// Have two colors - do gradient
	gl_gradient_bg(mvars);
    }
    glGetIntegerv(GL_MATRIX_MODE, &mm);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-mvars->i.xlim_view, mvars->i.xlim_view, -mvars->i.ylim_view, mvars->i.ylim_view, dmp->i->dm_clipmin[2], dmp->i->dm_clipmax[2]);
    glMatrixMode(mm);

    return 0;
}

int gl_setLight(struct dm *dmp, int lighting_on)
{
    gl_debug_print(dmp, "gl_setLight", dmp->i->dm_debugLevel);

    struct gl_vars *mvars = (struct gl_vars *)dmp->i->m_vars;

    mvars->lighting_on = lighting_on;

    if (!mvars->lighting_on) {
	/* Turn it off */
	glDisable(GL_LIGHTING);
    } else {
	/* Turn it on */

	if (1 < mvars->lighting_on)
	    glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE);
	else
	    glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_FALSE);

	glLightModelfv(GL_LIGHT_MODEL_AMBIENT, mvars->i.amb_three);
	glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER, GL_FALSE);

	glLightfv(GL_LIGHT0, GL_DIFFUSE, mvars->i.light0_diffuse);
	glLightfv(GL_LIGHT0, GL_SPECULAR, mvars->i.light0_diffuse);

	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
    }

    return BRLCAD_OK;
}

int gl_getLight(struct dm *dmp)
{
    gl_debug_print(dmp, "gl_setLight", dmp->i->dm_debugLevel);

    struct gl_vars *mvars = (struct gl_vars *)dmp->i->m_vars;

    return mvars->lighting_on;
}

/*
 * There are global variables which are parameters to this routine.
 */
int gl_drawBegin(struct dm *dmp)
{
    gl_debug_print(dmp, "gl_drawBegin", dmp->i->dm_debugLevel);

    struct gl_vars *mvars = (struct gl_vars *)dmp->i->m_vars;

    if (dm_make_current(dmp) != BRLCAD_OK) {
	return BRLCAD_ERROR;
    }

    /* clear back buffer */
    if (!dmp->i->dm_clearBufferAfter && mvars->doublebuffer) {
	if (dmp->i->dm_bg1[0] == dmp->i->dm_bg2[0] && dmp->i->dm_bg1[1] == dmp->i->dm_bg2[1] && dmp->i->dm_bg1[2] == dmp->i->dm_bg2[2]) {
	    glClearColor(mvars->i.r1, mvars->i.g1, mvars->i.b1, 0.0);
	    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	} else {
	    // Have two colors - do gradient
	    gl_gradient_bg(mvars);
	}
    }

    // In case we were left in a faceplace matrix state, clear it.
    //
    // TODO - not all display environments will tolerate being left in the HUD
    // state - Qt, for example, will not properly display solid wireframes in
    // that case.  Leaving this here for now, but we really need to take care
    // of this immediately after we're done drawing faceplate...
    dm_hud_end(dmp);

    if (dmp->i->dm_debugLevel > 3)
	gl_debug_print(dmp, "gl_drawBegin after:", dmp->i->dm_debugLevel);

    return BRLCAD_OK;
}


int gl_drawEnd(struct dm *dmp)
{
    gl_debug_print(dmp, "gl_drawEnd", dmp->i->dm_debugLevel);

    struct gl_vars *mvars = (struct gl_vars *)dmp->i->m_vars;

    if (mvars->lighting_on) {
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glLightfv(GL_LIGHT0, GL_POSITION, mvars->i.light0_position);
    }

    if (mvars->doublebuffer) {
	if (dmp->i->dm_SwapBuffers) {
	    (*dmp->i->dm_SwapBuffers)(dmp);
	}

	if (dmp->i->dm_clearBufferAfter) {
	    /* give Graphics pipe time to work */
	    if (dmp->i->dm_bg1[0] == dmp->i->dm_bg2[0] && dmp->i->dm_bg1[1] == dmp->i->dm_bg2[1] && dmp->i->dm_bg1[2] == dmp->i->dm_bg2[2]) {
		glClearColor(mvars->i.r1, mvars->i.g1, mvars->i.b1, 0.0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	    } else {
		// Have two colors - do gradient
		gl_gradient_bg(mvars);
	    }
	}
    }

    if (dmp->i->dm_debugLevel > 3)
	gl_debug_print(dmp, "gl_drawEnd after:", dmp->i->dm_debugLevel);

    return BRLCAD_OK;
}


/*
 * Load a new transformation matrix.  This will be followed by
 * many calls to gl_draw().
 */
int gl_loadMatrix(struct dm *dmp, fastf_t *mat, int which_eye)
{
    fastf_t *mptr;
    GLfloat gtmat[16];

    gl_debug_print(dmp, "gl_loadMatrix", dmp->i->dm_debugLevel);
    if (dmp->i->dm_debugLevel == 3) {
	struct bu_vls msg = BU_VLS_INIT_ZERO;
	bu_vls_printf(&msg, "gl_loadMatrix input matrix = \n");
	gl_printmat(&msg, mat);
	gl_debug_log(dmp, &msg);
	bu_vls_free(&msg);
    }

    switch (which_eye) {
	case 0:
	    /* Non-stereo */
	    break;
	case 1:
	    /* R eye */
	    glViewport(0,  0, (XMAXSCREEN)+1, (YSTEREO)+1);
	    glScissor(0,  0, (XMAXSCREEN)+1, (YSTEREO)+1);
	    if (dmp->i->dm_drawString2D) {
		(*dmp->i->dm_drawString2D)(dmp, "R", 0.986, 0.0, 0, 1);
	    }
	    break;
	case 2:
	    /* L eye */
	    glViewport(0,  0+YOFFSET_LEFT, (XMAXSCREEN)+1,
		       (YSTEREO+YOFFSET_LEFT)-(YOFFSET_LEFT)+1);
	    glScissor(0,  0+YOFFSET_LEFT, (XMAXSCREEN)+1,
		      (YSTEREO+YOFFSET_LEFT)-(YOFFSET_LEFT)+1);
	    break;
    }

    mptr = mat;

    gtmat[0] = *(mptr++);
    gtmat[4] = *(mptr++);
    gtmat[8] = *(mptr++);
    gtmat[12] = *(mptr++);

    gtmat[1] = *(mptr++) * dmp->i->dm_aspect;
    gtmat[5] = *(mptr++) * dmp->i->dm_aspect;
    gtmat[9] = *(mptr++) * dmp->i->dm_aspect;
    gtmat[13] = *(mptr++) * dmp->i->dm_aspect;

    gtmat[2] = *(mptr++);
    gtmat[6] = *(mptr++);
    gtmat[10] = *(mptr++);
    gtmat[14] = *(mptr++);

    gtmat[3] = *(mptr++);
    gtmat[7] = *(mptr++);
    gtmat[11] = *(mptr++);
    gtmat[15] = *(mptr++);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glLoadMatrixf(gtmat);

    if (dmp->i->dm_debugLevel > 3)
	gl_debug_print(dmp, "gl_loadMatrix after:", dmp->i->dm_debugLevel);

    return BRLCAD_OK;
}


/*
 * Load a new projection matrix.
 *
 */
int gl_loadPMatrix(struct dm *dmp, const fastf_t *mat)
{
    const fastf_t *mptr;
    GLfloat gtmat[16];

    gl_debug_print(dmp, "gl_loadPMatrix", dmp->i->dm_debugLevel);

    struct gl_vars *mvars = (struct gl_vars *)dmp->i->m_vars;

    glMatrixMode(GL_PROJECTION);

    if (mat == (fastf_t *)NULL) {
	glLoadIdentity();
	glOrtho(-mvars->i.xlim_view, mvars->i.xlim_view, -mvars->i.ylim_view, mvars->i.ylim_view, dmp->i->dm_clipmin[2], dmp->i->dm_clipmax[2]);

	return BRLCAD_OK;
    }

    mptr = mat;

    gtmat[0] = *(mptr++);
    gtmat[4] = *(mptr++);
    gtmat[8] = *(mptr++);
    gtmat[12] = *(mptr++);

    gtmat[1] = *(mptr++);
    gtmat[5] = *(mptr++);
    gtmat[9] = *(mptr++);
    gtmat[13] = *(mptr++);

    gtmat[2] = *(mptr++);
    gtmat[6] = *(mptr++);
    gtmat[10] = -*(mptr++);
    gtmat[14] = -*(mptr++);

    gtmat[3] = *(mptr++);
    gtmat[7] = *(mptr++);
    gtmat[11] = *(mptr++);
    gtmat[15] = *(mptr++);

    glLoadIdentity();
    glLoadMatrixf(gtmat);

    if (dmp->i->dm_debugLevel > 3)
	gl_debug_print(dmp, "gl_loadPMatrix after", dmp->i->dm_debugLevel);

    return BRLCAD_OK;
}

void gl_popPMatrix(struct dm *UNUSED(dmp))
{
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
}

int gl_drawVListHiddenLine(struct dm *dmp, register struct bv_vlist *vp)
{
    struct gl_vars *mvars = (struct gl_vars *)dmp->i->m_vars;

    register struct bv_vlist *tvp;
    register int first;

    gl_debug_print(dmp, "gl_drawVListHiddenLine", dmp->i->dm_debugLevel);

    /* First, draw polygons using background color. */

    if (mvars->lighting_on) {
	glDisable(GL_LIGHTING);
    }

    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glPolygonOffset(1.0, 1.0);

    /* Set color to background color for drawing polygons. */
    glColor3f(mvars->i.r1, mvars->i.g1, mvars->i.b1);

    /* Viewing region is from -1.0 to +1.0 */
    first = 1;
    for (BU_LIST_FOR(tvp, bv_vlist, &vp->l)) {
	register int i;
	register int nused = tvp->nused;
	register int *cmd = tvp->cmd;
	point_t *pt = tvp->pt;
	for (i = 0; i < nused; i++, cmd++, pt++) {
	    GLdouble dpt[3];
	    VMOVE(dpt, *pt); /* fastf_t-to-double */

	    switch (*cmd) {
		case BV_VLIST_LINE_MOVE:
		case BV_VLIST_LINE_DRAW:
		    break;
		case BV_VLIST_POLY_START:
		    /* Start poly marker & normal */
		    if (first == 0)
			glEnd();
		    first = 0;

		    glBegin(GL_POLYGON);
		    /* Set surface normal (vl_pnt points outward) */
		    glNormal3dv(dpt);
		    break;
		case BV_VLIST_POLY_MOVE:
		case BV_VLIST_POLY_DRAW:
		case BV_VLIST_TRI_MOVE:
		case BV_VLIST_TRI_DRAW:
		    glVertex3dv(dpt);
		    break;
		case BV_VLIST_POLY_END:
		    /* Draw, End Polygon */
		    glEnd();
		    first = 1;
		    break;
		case BV_VLIST_POLY_VERTNORM:
		case BV_VLIST_TRI_VERTNORM:
		    /* Set per-vertex normal.  Given before vert. */
		    glNormal3dv(dpt);
		    break;
		case BV_VLIST_TRI_START:
		    if (first)
			glBegin(GL_TRIANGLES);

		    first = 0;

		    /* Set surface normal (vl_pnt points outward) */
		    glNormal3dv(dpt);

		    break;
		case BV_VLIST_TRI_END:
		    break;
	    }
	}
    }

    if (first == 0)
	glEnd();

    /* Last, draw wireframe/edges. */

    /* Set color to wireColor for drawing wireframe/edges */
    glColor3f(mvars->i.wireColor[0], mvars->i.wireColor[1], mvars->i.wireColor[2]);

    /* Viewing region is from -1.0 to +1.0 */
    first = 1;
    for (BU_LIST_FOR(tvp, bv_vlist, &vp->l)) {
	register int i;
	register int nused = tvp->nused;
	register int *cmd = tvp->cmd;
	point_t *pt = tvp->pt;

	for (i = 0; i < nused; i++, cmd++, pt++) {
	    GLdouble dpt[3];
	    VMOVE(dpt, *pt); /* fastf_t-to-double */

	    switch (*cmd) {
		case BV_VLIST_LINE_MOVE:
		    /* Move, start line */
		    if (first == 0)
			glEnd();
		    first = 0;

		    glBegin(GL_LINE_STRIP);
		    glVertex3dv(dpt);
		    break;
		case BV_VLIST_POLY_START:
		case BV_VLIST_TRI_START:
		    /* Start poly marker & normal */
		    if (first == 0)
			glEnd();
		    first = 0;

		    glBegin(GL_LINE_STRIP);
		    break;
		case BV_VLIST_LINE_DRAW:
		case BV_VLIST_POLY_MOVE:
		case BV_VLIST_POLY_DRAW:
		case BV_VLIST_TRI_MOVE:
		case BV_VLIST_TRI_DRAW:
		    glVertex3dv(dpt);
		    break;
		case BV_VLIST_POLY_END:
		case BV_VLIST_TRI_END:
		    /* Draw, End Polygon */
		    glVertex3dv(dpt);
		    glEnd();
		    first = 1;
		    break;
		case BV_VLIST_POLY_VERTNORM:
		case BV_VLIST_TRI_VERTNORM:
		    /* Set per-vertex normal.  Given before vert. */
		    glNormal3dv(dpt);
		    break;
	    }
	}
    }

    if (first == 0)
	glEnd();

    if (mvars->lighting_on) {
	glEnable(GL_LIGHTING);
    }

    if (!mvars->zbuffer_on)
	glDisable(GL_DEPTH_TEST);

    if (!dmp->i->dm_depthMask)
	glDepthMask(GL_FALSE);

    glDisable(GL_POLYGON_OFFSET_FILL);

    if (dmp->i->dm_debugLevel > 3)
	gl_debug_print(dmp, "gl_drawVListHiddenLine", dmp->i->dm_debugLevel);

    return BRLCAD_OK;
}


int gl_drawVList(struct dm *dmp, struct bv_vlist *vp)
{
    struct gl_vars *mvars = (struct gl_vars *)dmp->i->m_vars;
    struct bv_vlist *tvp;
    register int first;
    register int mflag = 1;
    GLfloat pointSize = 0.0;
    static float black[4] = {0.0, 0.0, 0.0, 0.0};
    GLfloat originalPointSize, originalLineWidth;
    GLdouble m[16];
    GLdouble mt[16];
    GLdouble tlate[3];

    glGetFloatv(GL_POINT_SIZE, &originalPointSize);
    glGetFloatv(GL_LINE_WIDTH, &originalLineWidth);

    pointSize = originalPointSize;

    gl_debug_print(dmp, "gl_drawVListd", dmp->i->dm_debugLevel);

    /* Viewing region is from -1.0 to +1.0 */
    first = 1;
    for (BU_LIST_FOR(tvp, bv_vlist, &vp->l)) {
	int i;
	int nused = tvp->nused;
	int *cmd = tvp->cmd;
	point_t *pt = tvp->pt;
	for (i = 0; i < nused; i++, cmd++, pt++) {
	    GLdouble dpt[3];
	    VMOVE(dpt, *pt);

	    if (dmp->i->dm_debugLevel > 4) {
		struct bu_vls msg = BU_VLS_INIT_ZERO;
		bu_vls_sprintf(&msg, "\t%d (%g %g %g)\n", *cmd, V3ARGS(dpt));
		gl_debug_log(dmp, &msg);
		bu_vls_free(&msg);
	    }

	    switch (*cmd) {
		case BV_VLIST_LINE_MOVE:
		    /* Move, start line */
		    if (first == 0)
			glEnd();
		    first = 0;

		    if (mvars->lighting_on && mflag) {
			mflag = 0;
			glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, mvars->i.wireColor);
			glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, black);
			glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, black);
			glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, black);

			if (mvars->transparency_on)
			    glDisable(GL_BLEND);
		    }

		    glBegin(GL_LINE_STRIP);
		    glVertex3dv(dpt);
		    break;
		case BV_VLIST_MODEL_MAT:
		    if (first == 0) {
			glEnd();
			first = 1;
		    }

		    glMatrixMode(GL_MODELVIEW);
		    glPopMatrix();
		    break;
		case BV_VLIST_DISPLAY_MAT:
		    glMatrixMode(GL_MODELVIEW);
		    glGetDoublev(GL_MODELVIEW_MATRIX, m);

		    MAT_TRANSPOSE(mt, m);
		    MAT4X3PNT(tlate, mt, dpt);

		    glPushMatrix();
		    glLoadIdentity();
		    glTranslated(tlate[0], tlate[1], tlate[2]);
		    /* 96 dpi = 3.78 pixel/mm hardcoded */
		    glScaled(2. * 3.78 / dmp->i->dm_width,
		             2. * 3.78 / dmp->i->dm_height,
		             1.);
		    break;
		case BV_VLIST_POLY_START:
		case BV_VLIST_TRI_START:
		    /* Start poly marker & normal */

		    if (mvars->lighting_on && mflag) {
			mflag = 0;
			glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, black);
			glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, mvars->i.ambientColor);
			glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, mvars->i.specularColor);
			glMaterialfv(GL_FRONT, GL_DIFFUSE, mvars->i.diffuseColor);

			switch (mvars->lighting_on) {
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

			if (mvars->transparency_on) {
			    glEnable(GL_BLEND);
			    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			}
		    }

		    if (*cmd == BV_VLIST_POLY_START) {
			if (first == 0)
			    glEnd();

			glBegin(GL_POLYGON);
		    } else if (first)
			glBegin(GL_TRIANGLES);

		    /* Set surface normal (vl_pnt points outward) */
		    glNormal3dv(dpt);

		    first = 0;

		    break;
		case BV_VLIST_LINE_DRAW:
		case BV_VLIST_POLY_MOVE:
		case BV_VLIST_POLY_DRAW:
		case BV_VLIST_TRI_MOVE:
		case BV_VLIST_TRI_DRAW:
		    glVertex3dv(dpt);
		    break;
		case BV_VLIST_POLY_END:
		    /* Draw, End Polygon */
		    glEnd();
		    first = 1;
		    break;
		case BV_VLIST_TRI_END:
		    break;
		case BV_VLIST_POLY_VERTNORM:
		case BV_VLIST_TRI_VERTNORM:
		    /* Set per-vertex normal.  Given before vert. */
		    glNormal3dv(dpt);
		    break;
		case BV_VLIST_POINT_DRAW:
		    if (first == 0)
			glEnd();
		    first = 0;
#if ENABLE_POINT_SMOOTH
		    glEnable(GL_POINT_SMOOTH);
#endif
		    if (pointSize > 0.0) {
			glPointSize(pointSize);
		    }
		    glBegin(GL_POINTS);
		    glVertex3dv(dpt);
		    break;
		case BV_VLIST_LINE_WIDTH:
		    {
		    GLfloat lineWidth = (GLfloat)(*pt)[0];
		    if (lineWidth > 0.0) {
			glLineWidth(lineWidth);
		    }
		    break;
		}
		case BV_VLIST_POINT_SIZE:
		    {
		    pointSize = (GLfloat)(*pt)[0];
		    if (pointSize > 0.0) {
			glPointSize(pointSize);
		    }
		    break;
		}
	    }
	}
    }

    if (first == 0)
	glEnd();

    if (mvars->lighting_on && mvars->transparency_on)
	glDisable(GL_BLEND);

    glPointSize(originalPointSize);
    glLineWidth(originalLineWidth);

    if (dmp->i->dm_debugLevel > 3)
	gl_debug_print(dmp, "gl_drawVList", dmp->i->dm_debugLevel);

    return BRLCAD_OK;
}

int gl_draw_data_axes(struct dm *dmp,
                  fastf_t sf,
                  struct bv_data_axes_state *bndasp)
{
    struct gl_vars *mvars = (struct gl_vars *)dmp->i->m_vars;
    int npoints = bndasp->num_points * 6;
    if (npoints < 1)
        return 0;

    gl_debug_print(dmp, "gl_draw_data_axes", dmp->i->dm_debugLevel);

    /* set color */
    dm_set_fg(dmp, bndasp->color[0], bndasp->color[1], bndasp->color[2], 1, 1.0);

    if (bndasp->draw > 1) {
        if (mvars->lighting_on)
            glDisable(GL_LIGHTING);

        glPointSize(bndasp->size);
        dm_draw_points_3d(dmp, bndasp->num_points, bndasp->points);
        glPointSize(1);

        if (mvars->lighting_on)
            glEnable(GL_LIGHTING);

	return 0;
    }

    int i, j;
    fastf_t halfAxesSize;               /* half the length of an axis */
    point_t ptA, ptB;
    point_t *points;
    /* Save the line attributes */
    int saveLineWidth = dmp->i->dm_lineWidth;
    int saveLineStyle = dmp->i->dm_lineStyle;

    points = (point_t *)bu_calloc(npoints, sizeof(point_t), "data axes points");
    halfAxesSize = bndasp->size * 0.5 * sf;

    /* set linewidth */
    dm_set_line_attr(dmp, bndasp->line_width, 0);  /* solid lines */

    for (i = 0, j = -1; i < bndasp->num_points; ++i) {
	/* draw X axis with x/y offsets */
	VSET(ptA, bndasp->points[i][X] - halfAxesSize, bndasp->points[i][Y], bndasp->points[i][Z]);
	VSET(ptB, bndasp->points[i][X] + halfAxesSize, bndasp->points[i][Y], bndasp->points[i][Z]);
	++j;
	VMOVE(points[j], ptA);
	++j;
	VMOVE(points[j], ptB);

	/* draw Y axis with x/y offsets */
	VSET(ptA, bndasp->points[i][X], bndasp->points[i][Y] - halfAxesSize, bndasp->points[i][Z]);
	VSET(ptB, bndasp->points[i][X], bndasp->points[i][Y] + halfAxesSize, bndasp->points[i][Z]);
	++j;
	VMOVE(points[j], ptA);
	++j;
	VMOVE(points[j], ptB);

	/* draw Z axis with x/y offsets */
	VSET(ptA, bndasp->points[i][X], bndasp->points[i][Y], bndasp->points[i][Z] - halfAxesSize);
	VSET(ptB, bndasp->points[i][X], bndasp->points[i][Y], bndasp->points[i][Z] + halfAxesSize);
	++j;
	VMOVE(points[j], ptA);
	++j;
	VMOVE(points[j], ptB);
    }

    dm_draw_lines_3d(dmp, npoints, points, 0);
    bu_free((void *)points, "data axes points");

    /* Restore the line attributes */
    dm_set_line_attr(dmp, saveLineWidth, saveLineStyle);


    return 0;
}

int gl_draw(struct dm *dmp, struct bv_vlist *(*callback_function)(void *), void **data)
{
    struct bv_vlist *vp;
    if (!callback_function) {
	if (data) {
	    vp = (struct bv_vlist *)data;
	    gl_drawVList(dmp, vp);
	}
    } else {
	if (!data) {
	    return BRLCAD_ERROR;
	} else {
	    (void)callback_function(data);
	}
    }
    return BRLCAD_OK;
}


/*
 * Prepare for drawing a Head-Up Display (HUD).
 */
int gl_hud_begin(struct dm *dmp)
{
    struct gl_vars *mvars = (struct gl_vars *)dmp->i->m_vars;

    gl_debug_print(dmp, "gl_hud_begin", dmp->i->dm_debugLevel);

    if (!mvars->i.faceFlag) {
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadMatrixd(mvars->i.faceplate_mat);
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();
	mvars->i.faceFlag = 1;
	if (mvars->cueing_on)
	    glDisable(GL_FOG);
	if (mvars->lighting_on)
	    glDisable(GL_LIGHTING);
    }

    if (dmp->i->dm_debugLevel > 3)
	gl_debug_print(dmp, "gl_hud_begin after", dmp->i->dm_debugLevel);

    return BRLCAD_OK;
}

int gl_hud_end(struct dm *dmp)
{
    struct gl_vars *mvars = (struct gl_vars *)dmp->i->m_vars;

    GLfloat fogdepth;

    gl_debug_print(dmp, "gl_hud_end", dmp->i->dm_debugLevel);
    if (dmp->i->dm_debugLevel > 3) {
	struct bu_vls msg = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&msg, "faceFlag: %d\n", mvars->i.faceFlag);
	gl_debug_log(dmp, &msg);
	bu_vls_free(&msg);
    }

    if (mvars->i.faceFlag) {
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
	mvars->i.faceFlag = 0;
	if (mvars->cueing_on) {
	    glEnable(GL_FOG);
	    /*XXX Need to do something with Viewscale */
	    fogdepth = 2.2 * (*dmp->i->dm_vp); /* 2.2 is heuristic */
	    glFogf(GL_FOG_END, fogdepth);
	    fogdepth = (GLfloat) (0.5*mvars->fogdensity/
		    (*dmp->i->dm_vp));
	    glFogf(GL_FOG_DENSITY, fogdepth);
	    glFogi(GL_FOG_MODE, dmp->i->dm_perspective ? GL_EXP : GL_LINEAR);
	}
	if (mvars->lighting_on) {
	    glEnable(GL_LIGHTING);
	}
   }

    if (dmp->i->dm_debugLevel > 3)
	gl_debug_print(dmp, "gl_hud_end after", dmp->i->dm_debugLevel);

    return BRLCAD_OK;
}

int
drawLine3D(struct dm *dmp, point_t pt1, point_t pt2, const char *log_bu, float *wireColor)
{
    if (!dmp) {
	return BRLCAD_ERROR;
    }
    struct gl_vars *mvars = (struct gl_vars *)dmp->i->m_vars;
    if (!mvars)
	return BRLCAD_ERROR;

    if (dmp->i->dm_debugLevel) {
	struct bu_vls msg = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&msg, "%s (drawLine3D): %f,%f,%f -> %f,%f,%f", log_bu, V3ARGS(pt1), V3ARGS(pt2));
	if (wireColor) {
	    bu_vls_printf(&msg, "%s: have wirecolor", log_bu);
	}
	gl_debug_log(dmp, &msg);
	bu_vls_free(&msg);
    }
    gl_debug_print(dmp, log_bu, dmp->i->dm_debugLevel);

    static float black[4] = {0.0, 0.0, 0.0, 0.0};
    GLdouble pt[3];

    if (mvars->lighting_on) {
	glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, wireColor);
	glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, black);
	glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, black);
	glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, black);

	if (mvars->transparency_on)
	    glDisable(GL_BLEND);
    }

    glBegin(GL_LINES);
    VMOVE(pt, pt1); /* fastf_t to GLdouble */
    glVertex3dv(pt);
    VMOVE(pt, pt2); /* fastf_t to GLdouble */
    glVertex3dv(pt);
    glEnd();

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
    struct gl_vars *mvars = (struct gl_vars *)dmp->i->m_vars;
    if (!mvars)
	return BRLCAD_ERROR;

    if (dmp->i->dm_debugLevel) {
	struct bu_vls msg = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&msg, "%s (drawLines3D) %d pnts, flag %d: %f,%f,%f -> %f,%f,%f", log_bu, npoints, lflag, V3ARGS(points[0]), V3ARGS(points[npoints - 1]));
	if (wireColor) {
	    bu_vls_printf(&msg, "%s: have wirecolor", log_bu);
	}
	gl_debug_log(dmp, &msg);
	bu_vls_free(&msg);
    }
    gl_debug_print(dmp, log_bu, dmp->i->dm_debugLevel);

    register int i;
    static float black[4] = {0.0, 0.0, 0.0, 0.0};

    if (mvars->lighting_on) {
	glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, wireColor);
	glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, black);
	glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, black);
	glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, black);

	if (mvars->transparency_on)
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

    return BRLCAD_OK;
}

int
drawLine2D(struct dm *dmp, fastf_t X1, fastf_t Y1, fastf_t X2, fastf_t Y2, const char *log_bu)
{
    if (!dmp) {
	return BRLCAD_ERROR;
    }

    if (dmp->i->dm_debugLevel) {
	struct bu_vls msg = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&msg, "%s (drawLine2D): %f,%f -> %f,%f", log_bu, X1, Y1, X2, Y2);
	gl_debug_log(dmp, &msg);
	bu_vls_free(&msg);
    }
    gl_debug_print(dmp, log_bu, dmp->i->dm_debugLevel);

    glBegin(GL_LINES);
    glVertex2f(X1, Y1);
    glVertex2f(X2, Y2);
    glEnd();

    return BRLCAD_OK;
}



int gl_drawLine2D(struct dm *dmp, fastf_t X1, fastf_t Y1, fastf_t X2, fastf_t Y2)
{
    return drawLine2D(dmp, X1, Y1, X2, Y2, "gl_drawLine2D()\n");
}


int gl_drawLine3D(struct dm *dmp, point_t pt1, point_t pt2)
{
    struct gl_vars *mvars = (struct gl_vars *)dmp->i->m_vars;
    return drawLine3D(dmp, pt1, pt2, "gl_drawLine3D()\n", mvars->i.wireColor);
}


int gl_drawLines3D(struct dm *dmp, int npoints, point_t *points, int sflag)
{
    struct gl_vars *mvars = (struct gl_vars *)dmp->i->m_vars;
    return drawLines3D(dmp, npoints, points, sflag, "gl_drawLine3D()\n", mvars->i.wireColor);
}


int gl_drawPoint2D(struct dm *dmp, fastf_t x, fastf_t y)
{
    if (dmp->i->dm_debugLevel) {
	struct bu_vls msg = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&msg, "gl_drawPoint2D: \tdmp: %p\tx - %lf\ty - %lf\n", (void *)dmp, x, y);
	gl_debug_log(dmp, &msg);
	bu_vls_free(&msg);
    }

#if ENABLE_POINT_SMOOTH
    glEnable(GL_POINT_SMOOTH);
#endif
    glBegin(GL_POINTS);
    glVertex2f(x, y);
    glEnd();

    return BRLCAD_OK;
}


int gl_drawPoint3D(struct dm *dmp, point_t point)
{
    GLdouble dpt[3];

    if (!dmp || !point)
	return BRLCAD_ERROR;

    gl_debug_print(dmp, "gl_drawPoint3D", dmp->i->dm_debugLevel);
    if (dmp->i->dm_debugLevel) {
	struct bu_vls msg = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&msg, "   pt: %lf %lf %lf\n", V3ARGS(point));
	gl_debug_log(dmp, &msg);
	bu_vls_free(&msg);
    }

    /* fastf_t to double */
    VMOVE(dpt, point);

#if ENABLE_POINT_SMOOTH
    glEnable(GL_POINT_SMOOTH);
#endif
    glBegin(GL_POINTS);
    glVertex3dv(dpt);
    glEnd();

    return BRLCAD_OK;
}


int gl_drawPoints3D(struct dm *dmp, int npoints, point_t *points)
{
    GLdouble dpt[3];
    register int i;

    if (!dmp || npoints < 0 || !points)
	return BRLCAD_ERROR;

    gl_debug_print(dmp, "gl_drawPoints3D", dmp->i->dm_debugLevel);

#if ENABLE_POINT_SMOOTH
    glEnable(GL_POINT_SMOOTH);
#endif
    glBegin(GL_POINTS);
    for (i = 0; i < npoints; ++i) {
	/* fastf_t to double */
	VMOVE(dpt, points[i]);
	glVertex3dv(dpt);
    }
    glEnd();

    return BRLCAD_OK;
}


int gl_setFGColor(struct dm *dmp, unsigned char r, unsigned char g, unsigned char b, int strict, fastf_t transparency)
{
    struct gl_vars *mvars = (struct gl_vars *)dmp->i->m_vars;
    dmp->i->dm_fg[0] = r;
    dmp->i->dm_fg[1] = g;
    dmp->i->dm_fg[2] = b;

    gl_debug_print(dmp, "gl_setFGColor", dmp->i->dm_debugLevel);

    /* wireColor gets the full rgb */
    mvars->i.wireColor[0] = r / 255.0;
    mvars->i.wireColor[1] = g / 255.0;
    mvars->i.wireColor[2] = b / 255.0;
    mvars->i.wireColor[3] = transparency;

    if (strict) {
	glColor3ub((GLubyte)r, (GLubyte)g, (GLubyte)b);
    } else {

	if (mvars->lighting_on) {
	    /* Ambient = .2, Diffuse = .6, Specular = .2 */

	    mvars->i.ambientColor[0] = mvars->i.wireColor[0] * 0.2;
	    mvars->i.ambientColor[1] = mvars->i.wireColor[1] * 0.2;
	    mvars->i.ambientColor[2] = mvars->i.wireColor[2] * 0.2;
	    mvars->i.ambientColor[3] = mvars->i.wireColor[3];

	    mvars->i.specularColor[0] = mvars->i.ambientColor[0];
	    mvars->i.specularColor[1] = mvars->i.ambientColor[1];
	    mvars->i.specularColor[2] = mvars->i.ambientColor[2];
	    mvars->i.specularColor[3] = mvars->i.ambientColor[3];

	    mvars->i.diffuseColor[0] = mvars->i.wireColor[0] * 0.6;
	    mvars->i.diffuseColor[1] = mvars->i.wireColor[1] * 0.6;
	    mvars->i.diffuseColor[2] = mvars->i.wireColor[2] * 0.6;
	    mvars->i.diffuseColor[3] = mvars->i.wireColor[3];

	    mvars->i.backDiffuseColorDark[0] = mvars->i.wireColor[0] * 0.3;
	    mvars->i.backDiffuseColorDark[1] = mvars->i.wireColor[1] * 0.3;
	    mvars->i.backDiffuseColorDark[2] = mvars->i.wireColor[2] * 0.3;
	    mvars->i.backDiffuseColorDark[3] = mvars->i.wireColor[3];

	    mvars->i.backDiffuseColorLight[0] = mvars->i.wireColor[0] * 0.9;
	    mvars->i.backDiffuseColorLight[1] = mvars->i.wireColor[1] * 0.9;
	    mvars->i.backDiffuseColorLight[2] = mvars->i.wireColor[2] * 0.9;
	    mvars->i.backDiffuseColorLight[3] = mvars->i.wireColor[3];

	    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, mvars->i.ambientColor);
	    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, mvars->i.specularColor);
	    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, mvars->i.diffuseColor);
	} else {
	    glColor3ub((GLubyte)r,  (GLubyte)g,  (GLubyte)b);
	}
    }

    return BRLCAD_OK;
}


int gl_setLineAttr(struct dm *dmp, int width, int style)
{
    dmp->i->dm_lineWidth = width;
    dmp->i->dm_lineStyle = style;

    gl_debug_print(dmp, "gl_setLineAttr", dmp->i->dm_debugLevel);

    glLineWidth((GLfloat) width);

    if (style == DM_DASHED_LINE)
	glEnable(GL_LINE_STIPPLE);
    else
	glDisable(GL_LINE_STIPPLE);

    return BRLCAD_OK;
}


int gl_debug(struct dm *dmp, int lvl)
{
    dmp->i->dm_debugLevel = lvl;

    return BRLCAD_OK;
}

int gl_logfile(struct dm *dmp, const char *filename)
{
    bu_vls_sprintf(&dmp->i->dm_log, "%s", filename);

    return BRLCAD_OK;
}

int gl_setWinBounds(struct dm *dmp, fastf_t *w)
{
    struct gl_vars *mvars = (struct gl_vars *)dmp->i->m_vars;
    GLint mm;

    gl_debug_print(dmp, "gl_setWinBounds", dmp->i->dm_debugLevel);

    dmp->i->dm_clipmin[0] = w[0];
    dmp->i->dm_clipmin[1] = w[2];
    dmp->i->dm_clipmin[2] = w[4];
    dmp->i->dm_clipmax[0] = w[1];
    dmp->i->dm_clipmax[1] = w[3];
    dmp->i->dm_clipmax[2] = w[5];

    glGetIntegerv(GL_MATRIX_MODE, &mm);
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glLoadIdentity();
    glOrtho(-mvars->i.xlim_view, mvars->i.xlim_view, -mvars->i.ylim_view, mvars->i.ylim_view, dmp->i->dm_clipmin[2], dmp->i->dm_clipmax[2]);
    glPushMatrix();
    glMatrixMode(mm);

    if (dmp->i->dm_debugLevel > 3)
	gl_debug_print(dmp, "gl_setWinBounds after:", dmp->i->dm_debugLevel);

    return BRLCAD_OK;
}


int gl_setTransparency(struct dm *dmp,
		    int transparency_on)
{
    struct gl_vars *mvars = (struct gl_vars *)dmp->i->m_vars;

    gl_debug_print(dmp, "gl_setTransparency", dmp->i->dm_debugLevel);

    mvars->transparency_on = transparency_on;

    if (transparency_on) {
	/* Turn it on */
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    } else {
	/* Turn it off */
	glDisable(GL_BLEND);
    }

    return BRLCAD_OK;
}

int gl_getTransparency(struct dm *dmp)
{
    struct gl_vars *mvars = (struct gl_vars *)dmp->i->m_vars;

    gl_debug_print(dmp, "gl_getTransparency", dmp->i->dm_debugLevel);

    return mvars->transparency_on;
}

int gl_setDepthMask(struct dm *dmp, int enable)
{
    gl_debug_print(dmp, "gl_setDepthMask", dmp->i->dm_debugLevel);

    dmp->i->dm_depthMask = enable;

    if (enable)
	glDepthMask(GL_TRUE);
    else
	glDepthMask(GL_FALSE);

    return BRLCAD_OK;
}


int gl_setZBuffer(struct dm *dmp, int zbuffer_on)
{
    struct gl_vars *mvars = (struct gl_vars *)dmp->i->m_vars;

    gl_debug_print(dmp, "gl_setZBuffer", dmp->i->dm_debugLevel);

    mvars->zbuffer_on = zbuffer_on;

    if (mvars->zbuf == 0) {
	mvars->zbuffer_on = 0;
    }

    if (mvars->zbuffer_on) {
	glDepthFunc(GL_LEQUAL);
	glEnable(GL_DEPTH_TEST);
    } else {
	glDisable(GL_DEPTH_TEST);
    }

    return BRLCAD_OK;
}

int gl_getZBuffer(struct dm *dmp)
{
    struct gl_vars *mvars = (struct gl_vars *)dmp->i->m_vars;

    gl_debug_print(dmp, "gl_getZBuffer", dmp->i->dm_debugLevel);

    return mvars->zbuffer_on;
}

int gl_setZClip(struct dm *dmp, int zclip)
{
    struct gl_vars *mvars = (struct gl_vars *)dmp->i->m_vars;

    gl_debug_print(dmp, "gl_setZClip", dmp->i->dm_debugLevel);

    mvars->zclipping_on = zclip;

    return BRLCAD_OK;
}

int gl_getZClip(struct dm *dmp)
{
    struct gl_vars *mvars = (struct gl_vars *)dmp->i->m_vars;

    gl_debug_print(dmp, "gl_getZClip", dmp->i->dm_debugLevel);

    return mvars->zclipping_on;
}

int gl_setBound(struct dm *dmp, double bound)
{
    struct gl_vars *mvars = (struct gl_vars *)dmp->i->m_vars;

    gl_debug_print(dmp, "gl_setBound", dmp->i->dm_debugLevel);

    mvars->bound = bound;

    return BRLCAD_OK;
}

double gl_getBound(struct dm *dmp)
{
    struct gl_vars *mvars = (struct gl_vars *)dmp->i->m_vars;

    gl_debug_print(dmp, "gl_getBound", dmp->i->dm_debugLevel);

    return mvars->bound;
}


int gl_setBoundFlag(struct dm *dmp, int boundf)
{
    struct gl_vars *mvars = (struct gl_vars *)dmp->i->m_vars;

    gl_debug_print(dmp, "gl_setBoundFlag", dmp->i->dm_debugLevel);

    mvars->boundFlag = boundf;

    return BRLCAD_OK;
}

int gl_getBoundFlag(struct dm *dmp)
{
    struct gl_vars *mvars = (struct gl_vars *)dmp->i->m_vars;

    gl_debug_print(dmp, "gl_getBoundFlag", dmp->i->dm_debugLevel);

    return mvars->boundFlag;
}

int gl_beginDList(struct dm *dmp, unsigned int list)
{
    gl_debug_print(dmp, "gl_beginDList", dmp->i->dm_debugLevel);

    glNewList((GLuint)list, GL_COMPILE);
    return BRLCAD_OK;
}


int gl_endDList(struct dm *dmp)
{
    gl_debug_print(dmp, "gl_endDList", dmp->i->dm_debugLevel);

    glEndList();
    return BRLCAD_OK;
}


int gl_drawDList(unsigned int list)
{
    glCallList((GLuint)list);
    return BRLCAD_OK;
}


int gl_freeDLists(struct dm *dmp, unsigned int list, int range)
{
    gl_debug_print(dmp, "gl_freeDLists", dmp->i->dm_debugLevel);

    glDeleteLists((GLuint)list, (GLsizei)range);
    return BRLCAD_OK;
}


int gl_genDLists(struct dm *dmp, size_t range)
{
    gl_debug_print(dmp, "gl_genDLists", dmp->i->dm_debugLevel);

    return glGenLists((GLsizei)range);
}

int gl_draw_display_list(struct dm *dmp, struct display_list *obj)
{
    gl_debug_print(dmp, "gl_draw_obj", dmp->i->dm_debugLevel);

    struct bv_scene_obj *sp;
    for (BU_LIST_FOR(sp, bv_scene_obj, &obj->dl_head_scene_obj)) {
	if (sp->s_dlist == 0)
	    sp->s_dlist = gl_genDLists(dmp, 1);

	(void)dm_make_current(dmp);
	(void)gl_beginDList(dmp, sp->s_dlist);
	if (sp->s_iflag == UP)
	    (void)dm_set_fg(dmp, 255, 255, 255, 0, sp->s_os->transparency);
	else {
	    // TODO - do we need to respect override color here??
	    (void)dm_set_fg(dmp,
		    (unsigned char)sp->s_color[0],
		    (unsigned char)sp->s_color[1],
		    (unsigned char)sp->s_color[2], 0, sp->s_os->transparency);
	}
	(void)dm_draw_vlist(dmp, (struct bv_vlist *)&sp->s_vlist);
	(void)gl_endDList(dmp);
    }
    return 0;
}

int gl_getDisplayImage(struct dm *dmp, unsigned char **image, int flip, int alpha)
{
    gl_debug_print(dmp, "gl_getDisplayImage", dmp->i->dm_debugLevel);

    unsigned char *idata;
    int width;
    int height;

    width = dmp->i->dm_width;
    height = dmp->i->dm_height;

    if (!alpha) {
	idata = (unsigned char*)bu_calloc(height * width * 3, sizeof(unsigned char), "rgb data");
	glReadBuffer(GL_FRONT);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, idata);
	*image = idata;
    } else {
	idata = (unsigned char*)bu_calloc(height * width * 4, sizeof(unsigned char), "rgba data");
	glReadBuffer(GL_FRONT);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, idata);
	*image = idata;
    }
    if (flip)
	flip_display_image_vertically(*image, width, height, alpha);

    return BRLCAD_OK; /* caller will need to bu_free(idata, "image data"); */

}

int gl_get_internal(struct dm *dmp)
{
    struct gl_vars *mvars = NULL;
    if (!dmp->i->m_vars) {
	BU_GET(dmp->i->m_vars, struct gl_vars);
	mvars = (struct gl_vars *)dmp->i->m_vars;
	mvars->this_dm = dmp;
	bu_vls_init(&(mvars->log));
    }
    return 0;
}

int gl_put_internal(struct dm *dmp)
{
    struct gl_vars *mvars = NULL;
    if (dmp->i->m_vars) {
	mvars = (struct gl_vars *)dmp->i->m_vars;
	bu_vls_free(&(mvars->log));
	BU_PUT(dmp->i->m_vars, struct gl_vars);
    }
    return 0;
}

void gl_colorchange(const struct bu_structparse *sdp,
	const char *name,
	void *base,
	const char *value,
	void *data)
{
    struct gl_vars *mvars = (struct gl_vars *)base;
    if (mvars->cueing_on) {
	glEnable(GL_FOG);
    } else {
	glDisable(GL_FOG);
    }

    dm_generic_hook(sdp, name, base, value, data);
}

void gl_zclip_hook(const struct bu_structparse *sdp,
	const char *name,
	void *base,
	const char *value,
	void *data)
{
    struct gl_vars *mvars = (struct gl_vars *)base;
    struct dm *dmp = mvars->this_dm;
    fastf_t bounds[6] = { GED_MIN, GED_MAX, GED_MIN, GED_MAX, GED_MIN, GED_MAX };

    if (mvars->zclipping_on) {
	bounds[4] = -1.0;
	bounds[5] = 1.0;
    }

    (void)dm_make_current(dmp);
    (void)dm_set_win_bounds(dmp, bounds);

    dm_generic_hook(sdp, name, base, value, data);
}

void gl_debug_hook(const struct bu_structparse *sdp,
	const char *name,
	void *base,
	const char *value,
	void *data)
{
    dm_generic_hook(sdp, name, base, value, data);
}


void gl_logfile_hook(const struct bu_structparse *sdp,
	const char *name,
	void *base,
	const char *value,
	void *data)
{
    struct gl_vars *mvars = (struct gl_vars *)base;
    struct dm *dmp = mvars->this_dm;

    dm_logfile(dmp, bu_vls_addr(&mvars->log));

    dm_generic_hook(sdp, name, base, value, data);
}

void gl_bound_hook(const struct bu_structparse *sdp,
	const char *name,
	void *base,
	const char *value,
	void *data)
{
    dm_generic_hook(sdp, name, base, value, data);
}

void gl_bound_flag_hook(const struct bu_structparse *sdp,
	const char *name,
	void *base,
	const char *value,
	void *data)
{
    dm_generic_hook(sdp, name, base, value, data);
}

void gl_zbuffer_hook(const struct bu_structparse *sdp,
	const char *name,
	void *base,
	const char *value,
	void *data)
{
    struct gl_vars *mvars = (struct gl_vars *)base;
    struct dm *dmp = mvars->this_dm;

    (void)dm_make_current(dmp);
    (void)dm_set_zbuffer(dmp, mvars->zbuffer_on);

    dm_generic_hook(sdp, name, base, value, data);
}

void gl_lighting_hook(const struct bu_structparse *sdp,
	const char *name,
	void *base,
	const char *value,
	void *data)
{
    struct gl_vars *mvars = (struct gl_vars *)base;
    struct dm *dmp = mvars->this_dm;

    (void)dm_make_current(dmp);
    (void)dm_set_light(dmp, mvars->lighting_on);

    dm_generic_hook(sdp, name, base, value, data);
}

void gl_transparency_hook(const struct bu_structparse *sdp,
	const char *name,
	void *base,
	const char *value,
	void *data)
{
    struct gl_vars *mvars = (struct gl_vars *)base;
    struct dm *dmp = mvars->this_dm;

    (void)dm_make_current(dmp);
    (void)dm_set_transparency(dmp, mvars->transparency_on);

    dm_generic_hook(sdp, name, base, value, data);
}

void gl_fog_hook(const struct bu_structparse *sdp,
	const char *name,
	void *base,
	const char *value,
	void *data)
{
    struct gl_vars *mvars = (struct gl_vars *)base;
    struct dm *dmp = mvars->this_dm;

    dm_fogHint(dmp, mvars->fastfog);

    dm_generic_hook(sdp, name, base, value, data);
}


struct bu_structparse gl_vparse[] = {
    {"%d",  1, "depthcue",         gl_MV_O(cueing_on),       gl_colorchange, NULL, NULL },
    {"%d",  1, "zclip",            gl_MV_O(zclipping_on),    gl_zclip_hook, NULL, NULL },
    {"%d",  1, "zbuffer",          gl_MV_O(zbuffer_on),      gl_zbuffer_hook, NULL, NULL },
    {"%d",  1, "lighting",         gl_MV_O(lighting_on),     gl_lighting_hook, NULL, NULL },
    {"%d",  1, "transparency",     gl_MV_O(transparency_on), gl_transparency_hook, NULL, NULL },
    {"%d",  1, "fastfog",          gl_MV_O(fastfog),         gl_fog_hook, NULL, NULL },
    {"%g",  1, "density",          gl_MV_O(fogdensity),      dm_generic_hook, NULL, NULL },
    {"%d",  1, "has_zbuf",         gl_MV_O(zbuf),            dm_generic_hook, NULL, NULL },
    {"%d",  1, "has_rgb",          gl_MV_O(rgb),             dm_generic_hook, NULL, NULL },
    {"%d",  1, "has_doublebuffer", gl_MV_O(doublebuffer),    dm_generic_hook, NULL, NULL },
    {"%d",  1, "depth",            gl_MV_O(depth),           dm_generic_hook, NULL, NULL },
    {"%V",  1, "log",              gl_MV_O(log),             gl_logfile_hook, NULL, NULL },
    {"%g",  1, "bound",            gl_MV_O(bound),           gl_bound_hook, NULL, NULL },
    {"%d",  1, "useBound",         gl_MV_O(boundFlag),       gl_bound_flag_hook, NULL, NULL },
    {"",    0, (char *)0,          0,                        BU_STRUCTPARSE_FUNC_NULL, NULL, NULL }
};

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
