/* $Id$ */

/* 
 * Togl - a Tk OpenGL widget
 * Copyright (C) 1996-1997  Brian Paul and Ben Bederson
 * Copyright (C) 2006-2009  Greg Couch
 * See the LICENSE file for copyright details.
 */

#define USE_TOGL_STUBS

#include "togl.h"
#include <stdlib.h>
#include <string.h>

#undef TCL_STORAGE_CLASS
#define TCL_STORAGE_CLASS DLLEXPORT


static Tcl_Obj *toglFont;
static double xAngle = 0, yAngle = 0, zAngle = 0;
static GLfloat CornerX, CornerY, CornerZ;       /* where to print strings */
static double coord_scale = 1;



/* 
 * Togl widget create callback.  This is called by Tcl/Tk when the widget has
 * been realized.  Here's where one may do some one-time context setup or
 * initializations.
 */
static int
create_cb(ClientData clientData, Tcl_Interp *interp, int objc,
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

    return TCL_OK;
}


/* 
 * Togl widget reshape callback.  This is called by Tcl/Tk when the widget
 * has been resized.  Typically, we call glViewport and perhaps setup the
 * projection matrix.
 */
static int
reshape_cb(ClientData clientData, Tcl_Interp *interp, int objc,
        Tcl_Obj *const *objv)
{
    int     width;
    int     height;
    float   aspect;
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
    aspect = (float) width / (float) height;

    glViewport(0, 0, width, height);

    /* Set up projection transform */
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustum(-aspect, aspect, -1, 1, 1, 10);

    CornerX = -aspect;
    CornerY = -1;
    CornerZ = -1.1f;

    /* Change back to model view transform for rendering */
    glMatrixMode(GL_MODELVIEW);
    return TCL_OK;
}


static void
draw_eye(Togl *togl)
{
    static GLuint cubeList = 0;

    Togl_Clear(togl, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    Togl_Frustum(togl, -1, 1, -1, 1, 1, 10);
    glMatrixMode(GL_MODELVIEW);

    if (!cubeList) {
        cubeList = glGenLists(1);
        glNewList(cubeList, GL_COMPILE);

	/* Front face */
	glBegin(GL_QUADS);
	glColor3f(0.4f, 0.8f, 0.4f);        /* Green-ish */
	glVertex3f(-1, 1, 1);
	glVertex3f(1, 1, 1);
	glVertex3f(1, -1, 1);
	glVertex3f(-1, -1, 1);
	/* Back face */
	glColor3f(0.8f, 0.8f, 0.4f);        /* Yellow-ish */
	glVertex3f(-1, 1, -1);
	glVertex3f(1, 1, -1);
	glVertex3f(1, -1, -1);
	glVertex3f(-1, -1, -1);
	/* Top side face */
	glColor3f(0.4f, 0.4f, 0.8f);        /* Blue-ish */
	glVertex3f(-1, 1, 1);
	glVertex3f(1, 1, 1);
	glVertex3f(1, 1, -1);
	glVertex3f(-1, 1, -1);
	/* Bottom side face */
	glColor3f(0.8f, 0.4f, 0.4f);        /* Red-ish */
	glVertex3f(-1, -1, 1);
	glVertex3f(1, -1, 1);
	glVertex3f(1, -1, -1);
	glVertex3f(-1, -1, -1);
	glEnd();

        glEndList();
    }
    glCallList(cubeList);
}

/* 
 * Togl widget display callback.  This is called by Tcl/Tk when the widget's
 * contents have to be redrawn.  Typically, we clear the color and depth
 * buffers, render our objects, then swap the front/back color buffers.
 */
static int
display_cb(ClientData clientData, Tcl_Interp *interp, int objc,
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

    /* setup modelview matrix */
    glLoadIdentity();           /* Reset modelview matrix to the identity
                                 * matrix */
    glTranslatef(0, 0, -3.0);   /* Move the camera back three units */
    glScaled(coord_scale, coord_scale, coord_scale);    /* Zoom in and out */
    glRotated(xAngle, 1, 0, 0); /* Rotate by X, Y, and Z angles */
    glRotated(yAngle, 0, 1, 0);
    glRotated(zAngle, 0, 0, 1);

    glEnable(GL_DEPTH_TEST);

    if (Togl_NumEyes(togl) == 1) {
        /* single eye */
        Togl_DrawBuffer(togl, GL_BACK);
        draw_eye(togl);
    } else {
        /* stereo left eye */
        Togl_DrawBuffer(togl, GL_BACK_LEFT);
        draw_eye(togl);

        /* stereo right eye */
        Togl_DrawBuffer(togl, GL_BACK_RIGHT);
        draw_eye(togl);
    }

    glDisable(GL_DEPTH_TEST);
    glLoadIdentity();
    glColor3f(1, 1, 1);
    glRasterPos3f(CornerX, CornerY, CornerZ);
    Togl_SwapBuffers(togl);
    return TCL_OK;
}


static int
setXrot_cb(ClientData clientData, Tcl_Interp *interp, int objc,
        Tcl_Obj *const *objv)
{
    Togl   *togl;

    if (objc != 3) {
        Tcl_WrongNumArgs(interp, 1, objv, "pathName angle");
        return TCL_ERROR;
    }

    if (Togl_GetToglFromObj(interp, objv[1], &togl) != TCL_OK) {
        return TCL_ERROR;
    }

    if (Tcl_GetDoubleFromObj(interp, objv[2], &xAngle) != TCL_OK) {
        return TCL_ERROR;
    }

    /* printf( "before %f ", xAngle ); */

    if (xAngle < 0) {
        xAngle += 360;
    } else if (xAngle > 360) {
        xAngle -= 360;
    }

    /* printf( "after %f \n", xAngle ); */

    Togl_PostRedisplay(togl);

    /* Let result string equal value */
    Tcl_SetObjResult(interp, objv[2]);
    return TCL_OK;
}


static int
setYrot_cb(ClientData clientData, Tcl_Interp *interp, int objc,
        Tcl_Obj *const *objv)
{
    Togl   *togl;

    if (objc != 3) {
        Tcl_WrongNumArgs(interp, 1, objv, "pathName angle");
        return TCL_ERROR;
    }

    if (Togl_GetToglFromObj(interp, objv[1], &togl) != TCL_OK) {
        return TCL_ERROR;
    }

    if (Tcl_GetDoubleFromObj(interp, objv[2], &yAngle) != TCL_OK) {
        return TCL_ERROR;
    }

    if (yAngle < 0) {
        yAngle += 360;
    } else if (yAngle > 360) {
        yAngle -= 360;
    }

    Togl_PostRedisplay(togl);

    /* Let result string equal value */
    Tcl_SetObjResult(interp, objv[2]);
    return TCL_OK;
}


int
getXrot_cb(ClientData clientData, Tcl_Interp *interp,
        int argc, CONST84 char *argv[])
{
    Tcl_SetObjResult(interp, Tcl_NewDoubleObj(xAngle));
    return TCL_OK;
}


int
getYrot_cb(ClientData clientData, Tcl_Interp *interp,
        int argc, CONST84 char *argv[])
{
    Tcl_SetObjResult(interp, Tcl_NewDoubleObj(yAngle));
    return TCL_OK;
}


static int
coord_scale_cb(ClientData clientData, Tcl_Interp *interp, int objc,
        Tcl_Obj *const *objv)
{
    Togl   *togl;

    if (objc != 3) {
        Tcl_WrongNumArgs(interp, 1, objv, "pathName value");
        return TCL_ERROR;
    }

    if (Togl_GetToglFromObj(interp, objv[1], &togl) != TCL_OK) {
        return TCL_ERROR;
    }

    if (Tcl_GetDoubleFromObj(interp, objv[2], &coord_scale) != TCL_OK) {
        return TCL_ERROR;
    }

    Togl_PostRedisplay(togl);

    /* Let result string equal value */
    Tcl_SetObjResult(interp, objv[2]);
    return TCL_OK;
}


EXTERN int
Stereo_Init(Tcl_Interp *interp)
{
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
    Tcl_CreateObjCommand(interp, "create_cb", create_cb, NULL, NULL);
    Tcl_CreateObjCommand(interp, "display_cb", display_cb, NULL, NULL);
    Tcl_CreateObjCommand(interp, "reshape_cb", reshape_cb, NULL, NULL);

    Tcl_CreateObjCommand(interp, "setXrot", setXrot_cb, NULL, NULL);
    Tcl_CreateObjCommand(interp, "setYrot", setYrot_cb, NULL, NULL);
    Tcl_CreateObjCommand(interp, "coord_scale", coord_scale_cb, NULL, NULL);

    /* 
     * Call Tcl_CreateCommand for application-specific commands, if
     * they weren't already created by the init procedures called above.
     */

    Tcl_CreateCommand(interp, "getXrot", getXrot_cb, NULL, NULL);
    Tcl_CreateCommand(interp, "getYrot", getYrot_cb, NULL, NULL);

    return TCL_OK;
}
