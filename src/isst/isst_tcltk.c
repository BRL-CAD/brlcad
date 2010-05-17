/*
 * Copyright (c) 2005-2010 United States Government as represented by
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
/** @file main.c
 *
 *  Tcl/Tk wrapper for adrt/tie functionality, used by Tcl/Tk version
 *  of ISST.
 *
 */

#include <stdio.h>

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#include "tcl.h"
#include "tk.h"

#define USE_TOGL_STUBS

#include "togl.h"

#include "tie.h"
#include "adrt.h"
#include "adrt_struct.h"
#include "camera.h"
#include "isst.h"

int     width, height;
struct isst_s	*gisst;
void resize_isst(struct isst_s *);

/* new window size or exposure */
static int
reshape(ClientData clientData, Tcl_Interp *interp, int objc,
        Tcl_Obj *const *objv)
{
    Togl   *togl;

    if (objc != 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "pathName");
        return TCL_ERROR;
    }

    if (Togl_GetToglFromObj(interp, objv[1], &togl) != TCL_OK) {
        return TCL_ERROR;
    }

    width = Togl_Width(togl);
    height = Togl_Height(togl);
    glViewport(0, 0, (GLint) width, (GLint) height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    if (width > height) {
        GLfloat w = (GLfloat) width / (GLfloat) height;

        glFrustum(-w, w, -1, 1, 5, 60);
    } else {
        GLfloat h = (GLfloat) height / (GLfloat) width;

        glFrustum(-1, 1, -h, h, 5, 60);
    }

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(0, 0, -40);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    resize_isst(gisst);

    return TCL_OK;
}

void
resize_isst(struct isst_s *isst)
{
    isst->w = width;
    isst->h = height;
    switch(isst->gs) {
            case 0:
                isst->camera.w = isst->tile.size_x = isst->w;
                isst->camera.h = isst->tile.size_y = isst->h;
                break;
            case 1:
                isst->camera.w = isst->tile.size_x = 320;
                isst->camera.h = isst->tile.size_y = isst->camera.w * isst->h / isst->w;
                break;
            case 2:
                isst->camera.w = isst->tile.size_x = 40;
                isst->camera.h = isst->tile.size_y = isst->camera.w * isst->h / isst->w;
                break;
            default:
                bu_log("Unknown level...\n");
    }
    isst->tile.format = RENDER_CAMERA_BIT_DEPTH_24;
    TIENET_BUFFER_SIZE(isst->buffer_image, 3 * isst->camera.w * isst->camera.h);
    glClearColor (0.0, 0, 0.0, 1);
    glBindTexture (GL_TEXTURE_2D, isst->texid);
    glPixelStorei (GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    isst->texdata = malloc(isst->camera.w * isst->camera.h * 3);
    glTexImage2D (GL_TEXTURE_2D, 0, GL_RGB, isst->camera.w, isst->camera.h, 0, GL_RGB, GL_UNSIGNED_BYTE, isst->texdata);
    glDisable(GL_LIGHTING);
    glViewport(0,0,isst->w, isst->h);
    glMatrixMode (GL_PROJECTION);
    glLoadIdentity ();
    glOrtho(0, isst->w, isst->h, 0, -1, 1);
    glMatrixMode (GL_MODELVIEW);
}


static int
isst_load_g(ClientData clientData, Tcl_Interp *interp, int objc,
        Tcl_Obj *const *objv)
{
    struct isst_s *isst;
    char *argstring;
    char **argv;
    int argc = 2;
    Togl   *togl;

    if (objc < 4) {
        Tcl_WrongNumArgs(interp, 1, objv, "load_g pathname object");
        return TCL_ERROR;
    }

    bu_log("objv[1]: %s\n", Tcl_GetString(objv[1]));

    if (Togl_GetToglFromObj(interp, objv[1], &togl) != TCL_OK) {
        return TCL_ERROR;
    }

    argv = (char **)bu_malloc(sizeof(char *)*(argc + 1), "isst tcltk");
    argv[0] = Tcl_GetString(objv[3]);
    argv[1] = NULL;
    isst = (struct isst_s *) Togl_GetClientData(togl);

    load_g(isst->tie, Tcl_GetString(objv[2]), argc-1, (const char **)argv, &(isst->meshes));

    bu_free((genptr_t)argv, "isst tcltk"); 

    VSETALL(isst->camera.pos.v, isst->tie->radius);
    VMOVE(isst->camera.focus.v, isst->tie->mid);
    render_phong_init(&isst->camera.render, NULL);

    isst->ogl = 1;
    isst->w = Togl_Width(togl);
    isst->h = Togl_Height(togl);

    resize_isst(isst);

    return TCL_OK;
}

static int
paint_window(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    struct isst_s *isst;
    Togl   *togl;
    int glclrbts = GL_DEPTH_BUFFER_BIT;

    if (objc != 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "pathName");
        return TCL_ERROR;
    }

    if (Togl_GetToglFromObj(interp, objv[1], &togl) != TCL_OK) {
        return TCL_ERROR;
    }
    
    isst = (struct isst *) Togl_GetClientData(togl);

    render_camera_prep(&isst->camera);
    render_camera_render(&isst->camera, isst->tie, &isst->tile, &isst->buffer_image);

    if(isst->ui)
        glclrbts |= GL_COLOR_BUFFER_BIT;
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

    Togl_SwapBuffers(togl);

    return TCL_OK;
}

static int
idle(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
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

    isst = (struct isst *) Togl_GetClientData(togl);
    Togl_PostRedisplay(togl);

    return TCL_OK;
}


static int
isst_init(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    struct isst_s *isst;
    Togl   *togl;
    static GLfloat pos[4] = { 5, 5, 10, 0 };

    if (objc != 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "pathName");
        return TCL_ERROR;
    }

    if (Togl_GetToglFromObj(interp, objv[1], &togl) != TCL_OK) {
        return TCL_ERROR;
    }

    gisst = isst = (struct isst_s *)malloc(sizeof(struct isst_s));
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
isst_zap(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
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

    isst = (struct isst *) Togl_GetClientData(togl);

    bu_free(isst, "isst free");
}

static int
position(ClientData clientData, Tcl_Interp *interp, int objc,
        Tcl_Obj *const *objv)
{
    struct isst_s *isst;
    char    Result[100];
    Togl   *togl;

    if (objc != 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "pathName");
        return TCL_ERROR;
    }

    if (Togl_GetToglFromObj(interp, objv[1], &togl) != TCL_OK) {
        return TCL_ERROR;
    }

    isst = (struct isst *) Togl_GetClientData(togl);

    /* Let result string equal value */
    sprintf(Result, "TODO?");

    Tcl_SetResult(interp, Result, TCL_VOLATILE);
    return TCL_OK;
}

/*
static int
rotate(ClientData clientData, Tcl_Interp *interp, int objc,
        Tcl_Obj *const *objv)
{
    WHIRLYGIZMO *Wg;
    Togl   *togl;

    if (objc != 4) {
        Tcl_WrongNumArgs(interp, 1, objv, "pathName yrot xrot");
        return TCL_ERROR;
    }

    if (Togl_GetToglFromObj(interp, objv[1], &togl) != TCL_OK) {
        return TCL_ERROR;
    }

    Wg = (WHIRLYGIZMO *) Togl_GetClientData(togl);

    if (Tcl_GetDoubleFromObj(interp, objv[2], &Wg->Roty) != TCL_OK) {
        return TCL_ERROR;
    }
    if (Tcl_GetDoubleFromObj(interp, objv[3], &Wg->Rotx) != TCL_OK) {
        return TCL_ERROR;
    }
    Togl_PostRedisplay(togl);

    return TCL_OK;
}
*/

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
    Tcl_CreateObjCommand(interp, "idle", idle, NULL, NULL);
    /*Tcl_CreateObjCommand(interp, "rotate", rotate, NULL, NULL);*/
    Tcl_CreateObjCommand(interp, "position", position, NULL, NULL);

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
