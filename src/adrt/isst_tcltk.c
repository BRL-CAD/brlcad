/*                    I S S T _ T C L T K . C
 * BRL-CAD
 *
 * Copyright (c) 2005-2014 United States Government as represented by
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
/** @file isst_tcltk.c
 *
 *  Tcl/Tk wrapper for adrt/tie functionality, used by Tcl/Tk version
 *  of ISST.
 *
 */

#include "common.h"

#include <stdio.h>

#include "bio.h"

#include <GL/gl.h>

#include "tcl.h"
#include "tk.h"


#include "bu/parallel.h"
#include "bu/time.h"
#include "dm.h"

#include "tie.h"
#include "adrt.h"
#include "adrt_struct.h"
#include "camera.h"
#include "isst.h"
#include "raytrace.h"

#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

static struct dm *dmp;
static struct isst_s *isst;

/* ISST functions */
#ifdef _WIN32
__declspec(dllexport) int Isst_Init(Tcl_Interp *interp);
__declspec(dllexport) extern int Issttcltk_Init(Tcl_Interp *interp) { return Isst_Init(interp); }
#define DM_TYPE_ISST DM_TYPE_WGL
#else
#define DM_TYPE_ISST DM_TYPE_OGL
#endif

void resize_isst(struct isst_s *);

/* new window size or exposure */
static int
reshape(ClientData UNUSED(clientData), Tcl_Interp *interp, int objc,
	Tcl_Obj *const *objv)
{
    int width, height;
    if (objc != 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "width height");
	return TCL_ERROR;
    }

    Tcl_GetIntFromObj(interp, objv[1], &width);
    Tcl_GetIntFromObj(interp, objv[2], &height);

    isst->w = width;
    isst->h = height;

    resize_isst(isst);

    return TCL_OK;
}

void
resize_isst(struct isst_s *isstp)
{
    switch (isstp->gs) {
	case 0:
	    isstp->camera.w = isstp->tile.size_x = isstp->w;
	    isstp->camera.h = isstp->tile.size_y = isstp->h;
	    break;
	default:
	    isstp->camera.w = isstp->tile.size_x = isstp->gs;
	    isstp->camera.h = isstp->tile.size_y = isstp->camera.w * isstp->h / isstp->w;
	    break;
    }
    isstp->tile.format = RENDER_CAMERA_BIT_DEPTH_24;
    TIENET_BUFFER_SIZE(isstp->buffer_image, (uint32_t)(3 * isstp->camera.w * isstp->camera.h));
    glClearColor (0.0, 0, 0.0, 1);
    glBindTexture (GL_TEXTURE_2D, isstp->texid);
    glPixelStorei (GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    isstp->texdata = realloc(isstp->texdata, isstp->camera.w * isstp->camera.h * 3);
    glTexImage2D (GL_TEXTURE_2D, 0, GL_RGB, isstp->camera.w, isstp->camera.h, 0, GL_RGB, GL_UNSIGNED_BYTE, isstp->texdata);
    glDisable(GL_LIGHTING);

    glViewport(0,0,isstp->w, isstp->h);
    glMatrixMode (GL_PROJECTION);
    glLoadIdentity ();
    glOrtho(0, isstp->w, isstp->h, 0, -1, 1);
    glMatrixMode (GL_MODELVIEW);

    glClear(GL_COLOR_BUFFER_BIT);
    isstp->dirty = 1;
}


static int
isst_load_g(ClientData UNUSED(clientData), Tcl_Interp *interp, int objc,
	    Tcl_Obj *const *objv)
{
    char **argv;
    int argc;
    double az, el;
    struct bu_vls tclstr = BU_VLS_INIT_ZERO;
    vect_t vec;

    if (objc < 4) {
	Tcl_WrongNumArgs(interp, 1, objv, "load_g pathname object");
	return TCL_ERROR;
    }

    argv = (char **)malloc(sizeof(char *) * (strlen(Tcl_GetString(objv[3])) + 1));	/* allocate way too much. */
    argc = bu_argv_from_string(argv, strlen(Tcl_GetString(objv[3])), Tcl_GetString(objv[3]));

    load_g(isst->tie, Tcl_GetString(objv[2]), argc, (const char **)argv, &(isst->meshes));
    free(argv);

    VSETALL(isst->camera.pos, isst->tie->radius);
    VMOVE(isst->camera.focus, isst->tie->mid);
    VMOVE(isst->camera_pos_init, isst->camera.pos);
    VMOVE(isst->camera_focus_init, isst->camera.focus);

    /* Set the initial az and el values in Tcl/Tk */
    VSUB2(vec, isst->camera.pos, isst->camera.focus);
    VUNITIZE(vec);
    AZEL_FROM_V3DIR(az, el, vec);
    az = az * -DEG2RAD;
    el = el * -DEG2RAD;
    bu_vls_sprintf(&tclstr, "%f", az);
    Tcl_SetVar(interp, "az", bu_vls_addr(&tclstr), 0);
    bu_vls_sprintf(&tclstr, "%f", el);
    Tcl_SetVar(interp, "el", bu_vls_addr(&tclstr), 0);
    bu_vls_free(&tclstr);

    render_phong_init(&isst->camera.render, NULL);

    isst->ogl = 1;
    isst->w = 800;
    isst->h = 600;

    resize_isst(isst);

    isst->t1 = bu_gettime();
    isst->t2 = bu_gettime();

    return TCL_OK;
}

static int
list_geometry(ClientData UNUSED(clientData), Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    static struct db_i *dbip;
    struct directory *dp;
    int i;
    struct bu_vls tclstr = BU_VLS_INIT_ZERO;

    if (objc < 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "file varname");
	return TCL_ERROR;
    }

    if ((dbip = db_open(Tcl_GetString(objv[1]), DB_OPEN_READONLY)) == DBI_NULL) {
	bu_log("Unable to open geometry database file (%s)\n", Tcl_GetString(objv[1]));
	return TCL_ERROR;
    }
    db_dirbuild(dbip);
    for (i = 0; i < RT_DBNHASH; i++) {
	for (dp = dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {
	    if (dp->d_flags & RT_DIR_HIDDEN) continue;
	    bu_vls_sprintf(&tclstr, "set %s [concat $%s [list %s]]", Tcl_GetString(objv[2]), Tcl_GetString(objv[2]), dp->d_namep);
	    Tcl_Eval(interp, bu_vls_addr(&tclstr));
	}
    }
    db_close(dbip);
    bu_vls_free(&tclstr);
    return TCL_OK;
}


static int
paint_window(ClientData UNUSED(clientData), Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    double dt = 1.0;
    int glclrbts = GL_DEPTH_BUFFER_BIT;

    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "pathName");
	return TCL_ERROR;
    }

    isst->t2 = bu_gettime();

    dt = isst->t2 - isst->t1;

    if (dt > 1e6*0.08 && isst->dirty) {
	isst->buffer_image.ind = 0;

	render_camera_prep(&isst->camera);
	render_camera_render(&isst->camera, isst->tie, &isst->tile, &isst->buffer_image);

	isst->t1 = bu_gettime();

	DM_MAKE_CURRENT(dmp);

	glClear(glclrbts);
	glLoadIdentity();
	glColor3f(1,1,1);
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, isst->texid);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, isst->camera.w, isst->camera.h, GL_RGB, GL_UNSIGNED_BYTE, isst->buffer_image.data + sizeof(camera_tile_t));
	glBegin(GL_TRIANGLE_STRIP);

	glTexCoord2d(0, 0); glVertex3f(0, 0, 0);
	glTexCoord2d(0, 1); glVertex3f(0, isst->h, 0);
	glTexCoord2d(1, 0); glVertex3f(isst->w, 0, 0);
	glTexCoord2d(1, 1); glVertex3f(isst->w, isst->h, 0);

	glEnd();

	isst->dirty = 0;

	DM_DRAW_END(dmp);
    }
    return TCL_OK;
}

static int
set_resolution(ClientData UNUSED(clientData), Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    int resolution = 0;

    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "resolution");
	return TCL_ERROR;
    }

    if (Tcl_GetIntFromObj(interp, objv[1], &resolution) != TCL_OK) {
	return TCL_ERROR;
    }

    if (resolution < 1) resolution = 1;
    if (resolution > 20) {
	resolution = 20;
	isst->gs = 0;
    } else {
	isst->gs = lrint(floor(isst->w * .05 * resolution));
    }
    resize_isst(isst);

    return TCL_OK;
}


static int
isst_init(ClientData UNUSED(clientData), Tcl_Interp *interp, int UNUSED(objc), Tcl_Obj *const *UNUSED(objv))
{
    BU_ALLOC(isst, struct isst_s);
    isst->ui = 0;
    isst->uic = 0;

    BU_ALLOC(isst->tie, struct tie_s);
    TIENET_BUFFER_INIT(isst->buffer_image);
    render_camera_init(&isst->camera, bu_avail_cpus());

    isst->camera.type = RENDER_CAMERA_PERSPECTIVE;
    isst->camera.fov = 25;

    Tcl_LinkVar(interp, "pos_x", (char *)&isst->camera.pos[0], TCL_LINK_DOUBLE);
    Tcl_LinkVar(interp, "pos_y", (char *)&isst->camera.pos[1], TCL_LINK_DOUBLE);
    Tcl_LinkVar(interp, "pos_z", (char *)&isst->camera.pos[2], TCL_LINK_DOUBLE);
    Tcl_LinkVar(interp, "focus_x", (char *)&isst->camera.focus[0], TCL_LINK_DOUBLE);
    Tcl_LinkVar(interp, "focus_y", (char *)&isst->camera.focus[1], TCL_LINK_DOUBLE);
    Tcl_LinkVar(interp, "focus_z", (char *)&isst->camera.focus[2], TCL_LINK_DOUBLE);

    return TCL_OK;
}

static int
isst_zap(ClientData UNUSED(clientData), Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "pathName");
	return TCL_ERROR;
    }

    bu_free(isst, "isst free");
    isst = NULL;

    return TCL_OK;
}

static int
render_mode(ClientData UNUSED(clientData), Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    char *buf = NULL;
    char *mode;

    if (objc < 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "pathName mode [arguments]");
	return TCL_ERROR;
    }

    mode = Tcl_GetString(objv[2]);
    if (objc == 4)
	buf = Tcl_GetString(objv[3]);

    /* pack the 'rest' into buf - probably should use a vls for this*/
    if ( strlen(mode) == 3 && bu_strncmp("cut", mode, 3) == 0 ) {
	struct adrt_mesh_s *mesh;

	/* clear all the hit list */
	for (BU_LIST_FOR(mesh, adrt_mesh_s, &isst->meshes->l))
	    mesh->flags &= ~ADRT_MESH_HIT;
    }

    if (render_shader_init(&isst->camera.render, mode, buf) != 0)
	return TCL_ERROR;

    isst->dirty = 1;

    return TCL_OK;
}


static int
zero_view(ClientData UNUSED(clientData), Tcl_Interp *UNUSED(interp), int UNUSED(objc), Tcl_Obj *const *UNUSED(objv))
{
    vect_t vec;
    double mag_vec;

    mag_vec = DIST_PT_PT(isst->camera.pos, isst->camera.focus);

    VSUB2(vec, isst->camera_focus_init, isst->camera.pos);
    VUNITIZE(vec);
    VSCALE(vec, vec, mag_vec);
    VADD2(isst->camera.focus, isst->camera.pos, vec);

    isst->dirty = 1;
    return TCL_OK;
}


static int
move_walk(ClientData UNUSED(clientData), Tcl_Interp *interp, int UNUSED(objc), Tcl_Obj *const *objv)
{
    vect_t vec;
    int flag;

    if (Tcl_GetIntFromObj(interp, objv[2], &flag) != TCL_OK)
	return TCL_ERROR;

    if (flag >= 0) {
	VSUB2(vec, isst->camera.focus, isst->camera.pos);
	VSCALE(vec, vec, 0.1 * isst->tie->radius);
	VADD2(isst->camera.pos, isst->camera.pos, vec);
	VADD2(isst->camera.focus, isst->camera.focus, vec);
    } else {
	VSUB2(vec, isst->camera.pos, isst->camera.focus);
	VSCALE(vec, vec, 0.1 * isst->tie->radius);
	VADD2(isst->camera.pos, isst->camera.pos, vec);
	VADD2(isst->camera.focus, isst->camera.focus, vec);
    }
    isst->dirty = 1;
    return TCL_OK;
}

static int
move_strafe(ClientData UNUSED(clientData), Tcl_Interp *interp, int UNUSED(objc), Tcl_Obj *const *objv)
{
    vect_t vec, dir, up;

    int flag;

    if (Tcl_GetIntFromObj(interp, objv[2], &flag) != TCL_OK)
	return TCL_ERROR;

    VSET(up, 0, 0, 1);

    if (flag >= 0) {
	VSUB2(dir, isst->camera.focus, isst->camera.pos);
	VCROSS(vec, dir, up);
	VSCALE(vec, vec, 0.1 * isst->tie->radius);
	VADD2(isst->camera.pos, isst->camera.pos, vec);
	VADD2(isst->camera.focus, isst->camera.pos, dir);
    } else {
	VSUB2(dir, isst->camera.focus, isst->camera.pos);
	VCROSS(vec, dir, up);
	VSCALE(vec, vec, -0.1 * isst->tie->radius);
	VADD2(isst->camera.pos, isst->camera.pos, vec);
	VADD2(isst->camera.focus, isst->camera.pos, dir);
    }

    isst->dirty = 1;
    return TCL_OK;
}

static int
move_float(ClientData UNUSED(clientData), Tcl_Interp *UNUSED(interp), int UNUSED(objc), Tcl_Obj *const *UNUSED(objv))
{
    isst->camera.pos[2] += 0.05;
    isst->camera.focus[2] += 0.05;
    isst->dirty = 1;
    return TCL_OK;
}


static int
aetolookat(ClientData UNUSED(clientData), Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    vect_t vecdfoc;
    double x, y;
    double az, el;
    double mag_vec;

    if (objc < 4) {
	Tcl_WrongNumArgs(interp, 1, objv, "pathName az el");
	return TCL_ERROR;
    }

    if (Tcl_GetDoubleFromObj(interp, objv[2], &x) != TCL_OK)
	return TCL_ERROR;
    if (Tcl_GetDoubleFromObj(interp, objv[3], &y) != TCL_OK)
	return TCL_ERROR;

    mag_vec = DIST_PT_PT(isst->camera.pos, isst->camera.focus);

    VSUB2(vecdfoc, isst->camera.pos, isst->camera.focus);
    VUNITIZE(vecdfoc);
    AZEL_FROM_V3DIR(az, el, vecdfoc);
    az = az * -DEG2RAD + x;
    el = el * -DEG2RAD + y;
    V3DIR_FROM_AZEL(vecdfoc, az, el);
    VUNITIZE(vecdfoc);
    VSCALE(vecdfoc, vecdfoc, mag_vec);
    VADD2(isst->camera.focus, isst->camera.pos, vecdfoc);

    isst->dirty = 1;
    return TCL_OK;
}

static int
aerotate(ClientData UNUSED(clientData), Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    vect_t vec, vecdpos, vecdfoc;
    double x, y;
    double az, el;
    double mag_pos, mag_focus;
    struct bu_vls tclstr = BU_VLS_INIT_ZERO;

    if (objc < 4) {
	Tcl_WrongNumArgs(interp, 1, objv, "pathName x y");
	return TCL_ERROR;
    }

    if (Tcl_GetDoubleFromObj(interp, objv[2], &x) != TCL_OK)
	return TCL_ERROR;
    if (Tcl_GetDoubleFromObj(interp, objv[3], &y) != TCL_OK)
	return TCL_ERROR;

    mag_pos = DIST_PT_PT(isst->camera.pos, isst->camera_focus_init);

    mag_focus = DIST_PT_PT(isst->camera.focus, isst->camera_focus_init);

    VSUB2(vecdpos, isst->camera_focus_init, isst->camera.pos);
    VUNITIZE(vecdpos);
    AZEL_FROM_V3DIR(az, el, vecdpos);
    az = az * -DEG2RAD - x;
    el = el * -DEG2RAD + y;

    /* clamp to sane values */
    while (az > M_2PI) az -= M_2PI;
    while (az < 0) az += M_2PI;
    if (el>M_PI_2) el=M_PI_2 - 0.001;
    if (el<-M_PI_2) el=-M_PI_2 + 0.001;

    V3DIR_FROM_AZEL(vecdpos, az, el);
    VSCALE(vecdpos, vecdpos, mag_pos);
    VADD2(isst->camera.pos, isst->camera_focus_init, vecdpos);
    if (mag_focus > 0) {
	VSUB2(vecdfoc, isst->camera_focus_init, isst->camera.focus);
	VUNITIZE(vecdfoc);
	AZEL_FROM_V3DIR(az, el, vecdfoc);
	az = az * -DEG2RAD - x;
	el = el * -DEG2RAD + y;

	/* clamp to sane values */
	while (az > M_2PI) az -= M_2PI;
	while (az < 0) az += M_2PI;
	if (el>M_PI_2) el=M_PI_2 - 0.001;
	if (el<-M_PI_2) el=-M_PI_2 + 0.001;

	V3DIR_FROM_AZEL(vecdfoc, az, el);
	VSCALE(vecdfoc, vecdfoc, mag_focus);
	VADD2(isst->camera.focus, isst->camera_focus_init, vecdfoc);
    }
    /* Update the tcl copies of the az/el vars */
    VSUB2(vec, isst->camera.focus, isst->camera.pos);
    VUNITIZE(vec);
    AZEL_FROM_V3DIR(az, el, vec);
    bu_vls_sprintf(&tclstr, "%f", az);
    Tcl_SetVar(interp, "az", bu_vls_addr(&tclstr), 0);
    bu_vls_sprintf(&tclstr, "%f", el);
    Tcl_SetVar(interp, "el", bu_vls_addr(&tclstr), 0);
    bu_vls_free(&tclstr);
    isst->dirty = 1;
    return TCL_OK;
}

static int
open_dm(ClientData UNUSED(cdata), Tcl_Interp *interp, int UNUSED(objc), Tcl_Obj *const *UNUSED(objv))
{
    char *av[] = { "Ogl_open", "-t", "0", "-n", ".w0", "-W", "800", "-N", "600", NULL };

    dmp = DM_OPEN(interp, DM_TYPE_ISST, sizeof(av)/sizeof(void*)-1, (const char **)av);

    if (dmp == DM_NULL) {
	printf("dm failed?\n");
	return TCL_ERROR;
    }

    DM_SET_BGCOLOR(dmp, 0, 0, 0x30);

    return TCL_OK;

}

/* this function needs to be Isst_Init() for fbsd and mac, but may need to be
 * Issttcltk_Init on other platforms (I'm looking at you, windows). Needs more
 * investigation.
 */
int
Isst_Init(Tcl_Interp *interp)
{
    if (Tcl_PkgProvide(interp, "isst", "0.1") != TCL_OK) {
	return TCL_ERROR;
    }

    /*
     * Initialize Tcl
     */
    if (Tcl_InitStubs(interp, "8.1", 0) == NULL) {
	return TCL_ERROR;
    }

    /*
     * Specify the C callback functions for widget creation, display,
     * and reshape.
     */
    Tcl_CreateObjCommand(interp, "isst_init", isst_init, NULL, NULL);
    Tcl_CreateObjCommand(interp, "isst_zap", isst_zap, NULL, NULL);
    Tcl_CreateObjCommand(interp, "refresh_ogl", paint_window, NULL, NULL);
    Tcl_CreateObjCommand(interp, "reshape", reshape, NULL, NULL);
    Tcl_CreateObjCommand(interp, "load_g", isst_load_g, NULL, NULL);
    Tcl_CreateObjCommand(interp, "list_g", list_geometry, NULL, NULL);
    Tcl_CreateObjCommand(interp, "aetolookat", aetolookat, NULL, NULL);
    Tcl_CreateObjCommand(interp, "aerotate", aerotate, NULL, NULL);
    Tcl_CreateObjCommand(interp, "walk", move_walk, NULL, NULL);
    Tcl_CreateObjCommand(interp, "strafe", move_strafe, NULL, NULL);
    Tcl_CreateObjCommand(interp, "float", move_float, NULL, NULL);
    Tcl_CreateObjCommand(interp, "reset", zero_view, NULL, NULL);
    Tcl_CreateObjCommand(interp, "set_resolution", set_resolution, NULL, NULL);
    Tcl_CreateObjCommand(interp, "render_mode", render_mode, NULL, NULL);
    Tcl_CreateObjCommand(interp, "open_dm", open_dm, NULL, NULL);

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
