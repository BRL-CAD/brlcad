/*
 * Copyright (c) 2005-2011 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
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

#include "tcl.h"
#include "tk.h"

#define USE_TOGL_STUBS

#include "togl.h"

#include "bu.h"

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

/* ISST functions */
TIE_EXPORT extern int (Issttcltk_Init)(Tcl_Interp *interp);

void resize_isst(struct isst_s *);

/* new window size or exposure */
static int
reshape(ClientData UNUSED(clientData), Tcl_Interp *interp, int objc,
        Tcl_Obj *const *objv)
{
    Togl   *togl;
    struct isst_s *isst;

    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "pathName");
	return TCL_ERROR;
    }

    if (Togl_GetToglFromObj(interp, objv[1], &togl) != TCL_OK) {
	return TCL_ERROR;
    }

    isst = Togl_GetClientData(togl);

    isst->w = Togl_Width(togl);
    isst->h = Togl_Height(togl);

    resize_isst(isst);

    return TCL_OK;
}

void
resize_isst(struct isst_s *isst)
{
    switch(isst->gs) {
	case 0:
	    isst->camera.w = isst->tile.size_x = isst->w;
	    isst->camera.h = isst->tile.size_y = isst->h;
	    break;
	default:
	    isst->camera.w = isst->tile.size_x = isst->gs;
	    isst->camera.h = isst->tile.size_y = isst->camera.w * isst->h / isst->w;
	    break;
    }
    isst->tile.format = RENDER_CAMERA_BIT_DEPTH_24;
    TIENET_BUFFER_SIZE(isst->buffer_image, (size_t)(3 * isst->camera.w * isst->camera.h));
    glClearColor (0.0, 0, 0.0, 1);
    glBindTexture (GL_TEXTURE_2D, isst->texid);
    glPixelStorei (GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    isst->texdata = realloc(isst->texdata, isst->camera.w * isst->camera.h * 3);
    glTexImage2D (GL_TEXTURE_2D, 0, GL_RGB, isst->camera.w, isst->camera.h, 0, GL_RGB, GL_UNSIGNED_BYTE, isst->texdata);
    glDisable(GL_LIGHTING);

    glViewport(0,0,isst->w, isst->h);
    glMatrixMode (GL_PROJECTION);
    glLoadIdentity ();
    glOrtho(0, isst->w, isst->h, 0, -1, 1);
    glMatrixMode (GL_MODELVIEW);

    glClear(GL_COLOR_BUFFER_BIT);
    isst->dirty = 1;
}


static int
isst_load_g(ClientData UNUSED(clientData), Tcl_Interp *interp, int objc,
	    Tcl_Obj *const *objv)
{
    struct isst_s *isst;
    char **argv;
    int argc;
    double az, el;
    struct bu_vls tclstr;
    vect_t vec;
    Togl   *togl;

    bu_vls_init(&tclstr);    

    if (objc < 4) {
        Tcl_WrongNumArgs(interp, 1, objv, "load_g pathname object");
        return TCL_ERROR;
    }

    if (Togl_GetToglFromObj(interp, objv[1], &togl) != TCL_OK) {
        return TCL_ERROR;
    }

    isst = (struct isst_s *) Togl_GetClientData(togl);

    argv = (char **)malloc(sizeof(char *) * (strlen(Tcl_GetString(objv[3]) + 1)));	/* allocate way too much. */
    argc = bu_argv_from_string(argv, strlen(Tcl_GetString(objv[3])), Tcl_GetString(objv[3]));
    
    load_g(isst->tie, Tcl_GetString(objv[2]), argc, (const char **)argv, &(isst->meshes));
    free(argv);

    VSETALL(isst->camera.pos, isst->tie->radius);
    VMOVE(isst->camera.focus, isst->tie->mid);
    VMOVE(isst->camera_pos_init, isst->camera.pos);
    VMOVE(isst->camera_focus_init, isst->camera.focus);

    /* Set the inital az and el values in Tcl/Tk */
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
    isst->w = Togl_Width(togl);
    isst->h = Togl_Height(togl);

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
    struct bu_vls tclstr;
    bu_vls_init(&tclstr);

    if (objc < 3) {
        Tcl_WrongNumArgs(interp, 1, objv, "file varname");
        return TCL_ERROR;
    }

    if ((dbip = db_open(Tcl_GetString(objv[1]), "r")) == DBI_NULL) {
        bu_log("Unable to open geometry file (%s)\n", Tcl_GetString(objv[1]));
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
    struct isst_s *isst;
    Togl   *togl;
    double dt = 1.0;
    int glclrbts = GL_DEPTH_BUFFER_BIT;

    if (objc != 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "pathName");
        return TCL_ERROR;
    }

    if (Togl_GetToglFromObj(interp, objv[1], &togl) != TCL_OK) {
        return TCL_ERROR;
    }
    
    isst = (struct isst_s *) Togl_GetClientData(togl);

    isst->t2 = bu_gettime();

    dt = isst->t2 - isst->t1;

    if (dt > 1e6*0.08 && isst->dirty) {
	isst->buffer_image.ind = 0;

	render_camera_prep(&isst->camera);
	render_camera_render(&isst->camera, isst->tie, &isst->tile, &isst->buffer_image);

	isst->t1 = bu_gettime();

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

	Togl_SwapBuffers(togl);
    }
    return TCL_OK;
}

static int
set_resolution(ClientData UNUSED(clientData), Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    struct isst_s *isst;
    Togl   *togl;
    int resolution;

    if (objc != 3) {
        Tcl_WrongNumArgs(interp, 1, objv, "pathName resolution");
        return TCL_ERROR;
    }

    if (Togl_GetToglFromObj(interp, objv[1], &togl) != TCL_OK) {
        return TCL_ERROR;
    }

    if (Tcl_GetIntFromObj(interp, objv[2], &resolution) != TCL_OK) {
        return TCL_ERROR;
    }

    isst = (struct isst_s *) Togl_GetClientData(togl);

    if (resolution < 1) resolution = 1;
    if (resolution > 20) {
	resolution = 20;
	isst->gs = 0;
    } else {
	isst->gs = (int)floor(isst->w * .05 * resolution);
    }
    resize_isst(isst);

    return TCL_OK;
}

static int
idle(ClientData UNUSED(clientData), Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    Togl   *togl;

    if (objc != 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "pathName");
        return TCL_ERROR;
    }

    if (Togl_GetToglFromObj(interp, objv[1], &togl) != TCL_OK) {
        return TCL_ERROR;
    }

    (void)Togl_GetClientData(togl);
    Togl_PostRedisplay(togl);

    return TCL_OK;
}


static int
isst_init(ClientData UNUSED(clientData), Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    struct isst_s *isst;
    Togl   *togl;

    if (objc != 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "pathName");
        return TCL_ERROR;
    }

    if (Togl_GetToglFromObj(interp, objv[1], &togl) != TCL_OK) {
        return TCL_ERROR;
    }

    isst = (struct isst_s *)bu_calloc(1, sizeof(struct isst_s), "allocate isst struct");
    isst->ui = 0;
    isst->uic = 0;
    isst->tie = (struct tie_s *)bu_calloc(1,sizeof(struct tie_s), "tie");
    TIENET_BUFFER_INIT(isst->buffer_image);
    render_camera_init(&isst->camera, bu_avail_cpus());
    isst->camera.type = RENDER_CAMERA_PERSPECTIVE;
    isst->camera.fov = 25;

    Togl_SetClientData(togl, (ClientData) isst);

    return TCL_OK;
}

static int
isst_zap(ClientData UNUSED(clientData), Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    struct isst_s *isst;
    Togl   *togl;
    if (objc != 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "pathName");
        return TCL_ERROR;
    }

    if (Togl_GetToglFromObj(interp, objv[1], &togl) != TCL_OK) {
        return TCL_ERROR;
    }

    isst = (struct isst_s *) Togl_GetClientData(togl);

    bu_free(isst, "isst free");

    return TCL_OK;
}

static int
render_mode(ClientData UNUSED(clientData), Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    struct isst_s *isst;
    Togl *togl;
    char buf[BUFSIZ];

    if (objc < 3) {
        Tcl_WrongNumArgs(interp, 1, objv, "pathName mode [arguments]");
        return TCL_ERROR;
    }

    if (Togl_GetToglFromObj(interp, objv[1], &togl) != TCL_OK)
        return TCL_ERROR;

    isst = (struct isst_s *) Togl_GetClientData(togl);

    /* pack the 'rest' into buf - probably should use a vls for this*/

    isst->dirty = 1;

    if(render_shader_init(&isst->camera.render, Tcl_GetString(objv[2]), *buf?buf:NULL) != 0)
	return TCL_ERROR;
    return TCL_OK;
}


static int
zero_view(ClientData UNUSED(clientData), Tcl_Interp *interp, int UNUSED(objc), Tcl_Obj *const *objv)
{
    struct isst_s *isst;
    Togl *togl;
    vect_t vec;
    double mag_vec;

    if (Togl_GetToglFromObj(interp, objv[1], &togl) != TCL_OK)
        return TCL_ERROR;

    isst = (struct isst_s *) Togl_GetClientData(togl);

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
    struct isst_s *isst;
    Togl *togl;
    vect_t vec;
    int flag;

    if (Togl_GetToglFromObj(interp, objv[1], &togl) != TCL_OK)
        return TCL_ERROR;

    isst = (struct isst_s *) Togl_GetClientData(togl);

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
    struct isst_s *isst;
    Togl *togl;
    vect_t vec, dir, up;

    int flag;

    if (Togl_GetToglFromObj(interp, objv[1], &togl) != TCL_OK)
        return TCL_ERROR;

    isst = (struct isst_s *) Togl_GetClientData(togl);

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
move_float(ClientData UNUSED(clientData), Tcl_Interp *interp, int UNUSED(objc), Tcl_Obj *const *objv)
{
    struct isst_s *isst;
    Togl *togl;

    if (Togl_GetToglFromObj(interp, objv[1], &togl) != TCL_OK)
        return TCL_ERROR;

    isst = (struct isst_s *) Togl_GetClientData(togl);

    isst->camera.pos[2] += 0.05;
    isst->camera.focus[2] += 0.05;
    isst->dirty = 1;
    return TCL_OK;
}


static int
aetolookat(ClientData UNUSED(clientData), Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    struct isst_s *isst;
    Togl *togl;
    vect_t vecdfoc;
    double x, y;
    double az, el;
    double mag_vec;

    if (objc < 4) {
        Tcl_WrongNumArgs(interp, 1, objv, "pathName az el");
        return TCL_ERROR;
    }

    if (Togl_GetToglFromObj(interp, objv[1], &togl) != TCL_OK)
        return TCL_ERROR;

    isst = (struct isst_s *) Togl_GetClientData(togl);

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
    struct isst_s *isst;
    Togl *togl;
    vect_t vec, vecdpos, vecdfoc;
    double x, y;
    double az, el;
    double mag_pos, mag_focus;
    struct bu_vls tclstr;
    bu_vls_init(&tclstr);    


    if (objc < 4) {
        Tcl_WrongNumArgs(interp, 1, objv, "pathName x y");
        return TCL_ERROR;
    }

    if (Togl_GetToglFromObj(interp, objv[1], &togl) != TCL_OK)
        return TCL_ERROR;

    isst = (struct isst_s *) Togl_GetClientData(togl);

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
    while(az > 2*M_PI) az -= 2*M_PI;
    while(az < 0) az += 2*M_PI;
    if(el>M_PI_2) el=M_PI_2 - 0.001;
    if(el<-M_PI_2) el=-M_PI_2 + 0.001;

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
    	while(az > 2*M_PI) az -= 2*M_PI;
    	while(az < 0) az += 2*M_PI;
    	if(el>M_PI_2) el=M_PI_2 - 0.001;
    	if(el<-M_PI_2) el=-M_PI_2 + 0.001;

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

int
Isst_Init(Tcl_Interp *interp)
{
    if (Tcl_PkgProvide(interp, "isst", "0.1") != TCL_OK) {
        return TCL_ERROR;
    }

    /* 
     * Initialize Tcl and the Togl widget module.
     */
    if (Tcl_InitStubs(interp, "8.1", 0) == NULL
	|| Togl_InitStubs(interp, "2.0", 0) == NULL) {
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
    Tcl_CreateObjCommand(interp, "idle", idle, NULL, NULL);
    Tcl_CreateObjCommand(interp, "aetolookat", aetolookat, NULL, NULL);
    Tcl_CreateObjCommand(interp, "aerotate", aerotate, NULL, NULL);
    Tcl_CreateObjCommand(interp, "walk", move_walk, NULL, NULL);
    Tcl_CreateObjCommand(interp, "strafe", move_strafe, NULL, NULL);
    Tcl_CreateObjCommand(interp, "float", move_float, NULL, NULL);
    Tcl_CreateObjCommand(interp, "reset", zero_view, NULL, NULL);
    Tcl_CreateObjCommand(interp, "set_resolution", set_resolution, NULL, NULL);
    Tcl_CreateObjCommand(interp, "render_mode", render_mode, NULL, NULL);

    return TCL_OK;
}

int
Issttcltk_Init(Tcl_Interp *interp)
{
    if (Tcl_PkgProvide(interp, "isst", "0.1") != TCL_OK) {
        return TCL_ERROR;
    }

    /* 
     * Initialize Tcl and the Togl widget module.
     */
    if (Tcl_InitStubs(interp, "8.1", 0) == NULL
	|| Togl_InitStubs(interp, "2.0", 0) == NULL) {
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
    Tcl_CreateObjCommand(interp, "idle", idle, NULL, NULL);
    Tcl_CreateObjCommand(interp, "aetolookat", aetolookat, NULL, NULL);
    Tcl_CreateObjCommand(interp, "aerotate", aerotate, NULL, NULL);
    Tcl_CreateObjCommand(interp, "walk", move_walk, NULL, NULL);
    Tcl_CreateObjCommand(interp, "strafe", move_strafe, NULL, NULL);
    Tcl_CreateObjCommand(interp, "float", move_float, NULL, NULL);
    Tcl_CreateObjCommand(interp, "reset", zero_view, NULL, NULL);
    Tcl_CreateObjCommand(interp, "set_resolution", set_resolution, NULL, NULL);
    Tcl_CreateObjCommand(interp, "render_mode", render_mode, NULL, NULL);

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
