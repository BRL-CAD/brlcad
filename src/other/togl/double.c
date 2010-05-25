/* $Id$ */

/* 
 * Togl - a Tk OpenGL widget
 * Copyright (C) 1996-1997  Brian Paul and Ben Bederson
 * Copyright (C) 2006-2007  Greg Couch
 * See the LICENSE file for copyright details.
 */

#define USE_TOGL_STUBS

#include "togl.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#undef TCL_STORAGE_CLASS
#define TCL_STORAGE_CLASS DLLEXPORT

static Tcl_Obj *toglFont;
static double xAngle = 0, yAngle = 0, zAngle = 0;
static GLdouble CornerX, CornerY, CornerZ;      /* where to print strings */


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

    if (!toglFont)
        toglFont = Togl_LoadBitmapFont(togl, "Helvetica");
    if (!toglFont) {
        static int shown;

        if (!shown) {
            fprintf(stderr, "Couldn't load font!\n");
            shown = 1;
        }
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
    double  aspect;
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
    aspect = (double) width / (double) height;

    glViewport(0, 0, width, height);

    /* Set up projection transform */
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustum(-aspect, aspect, -1, 1, 1, 10);

    CornerX = -aspect;
    CornerY = -1;
    CornerZ = -1.1;

    /* Change back to model view transform for rendering */
    glMatrixMode(GL_MODELVIEW);

    return TCL_OK;
}



static void
print_string(Togl *togl, const char *s)
{
    if (toglFont)
        Togl_WriteChars(togl, toglFont, s, 0);
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
    static GLuint cubeList = 0;
    const char *ident;
    Togl   *togl;

    if (objc != 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "pathName");
        return TCL_ERROR;
    }

    if (Togl_GetToglFromObj(interp, objv[1], &togl) != TCL_OK)
        return TCL_ERROR;

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glLoadIdentity();           /* Reset modelview matrix to the identity
                                 * matrix */
    glTranslatef(0, 0, -3);     /* Move the camera back three units */
    glRotated(xAngle, 1, 0, 0); /* Rotate by X, Y, and Z angles */
    glRotated(yAngle, 0, 1, 0);
    glRotated(zAngle, 0, 0, 1);

    glEnable(GL_DEPTH_TEST);

    if (!cubeList) {
        cubeList = glGenLists(1);
        glNewList(cubeList, GL_COMPILE);

        /* Front face */
        glBegin(GL_QUADS);
        glColor3f(0, 0.7f, 0.1f);       /* Green */
        glVertex3f(-1, 1, 1);
        glVertex3f(1, 1, 1);
        glVertex3f(1, -1, 1);
        glVertex3f(-1, -1, 1);
        /* Back face */
        glColor3f(0.9f, 1, 0);  /* Yellow */
        glVertex3f(-1, 1, -1);
        glVertex3f(1, 1, -1);
        glVertex3f(1, -1, -1);
        glVertex3f(-1, -1, -1);
        /* Top side face */
        glColor3f(0.2f, 0.2f, 1);       /* Blue */
        glVertex3f(-1, 1, 1);
        glVertex3f(1, 1, 1);
        glVertex3f(1, 1, -1);
        glVertex3f(-1, 1, -1);
        /* Bottom side face */
        glColor3f(0.7f, 0, 0.1f);       /* Red */
        glVertex3f(-1, -1, 1);
        glVertex3f(1, -1, 1);
        glVertex3f(1, -1, -1);
        glVertex3f(-1, -1, -1);
        glEnd();

        glEndList();

    }
    glCallList(cubeList);

    glDisable(GL_DEPTH_TEST);
    glLoadIdentity();
    glColor3f(1, 1, 1);
    glRasterPos3d(CornerX, CornerY, CornerZ);
    ident = Togl_Ident(togl);
    if (ident)
        print_string(togl, ident);
    Togl_SwapBuffers(togl);
    return TCL_OK;
}




static int
setXrot_cb(ClientData clientData, Tcl_Interp *interp, int objc,
        Tcl_Obj *const *objv)
{
    if (objc != 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "angle");
        return TCL_ERROR;
    }

    if (Tcl_GetDoubleFromObj(interp, objv[1], &xAngle) != TCL_OK) {
        return TCL_ERROR;
    }

    /* printf( "before %f ", xAngle ); */

    xAngle = fmod(xAngle, 360.0);
    if (xAngle < 0)
        xAngle += 360.0;

    /* printf( "after %f \n", xAngle ); */

    /* Let result string equal value */
    Tcl_SetObjResult(interp, objv[1]);
    return TCL_OK;
}


static int
setYrot_cb(ClientData clientData, Tcl_Interp *interp, int objc,
        Tcl_Obj *const *objv)
{
    if (objc != 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "pathName angle");
        return TCL_ERROR;
    }

    if (Tcl_GetDoubleFromObj(interp, objv[1], &yAngle) != TCL_OK) {
        return TCL_ERROR;
    }

    yAngle = fmod(yAngle, 360.0);
    if (yAngle < 0)
        yAngle += 360.0;

    /* Let result string equal value */
    Tcl_SetObjResult(interp, objv[1]);
    return TCL_OK;
}

/* 
 * Called by Tcl to let me initialize the modules (Togl) I will need.
 */
EXTERN int
Double_Init(Tcl_Interp *interp)
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
    Tcl_CreateObjCommand(interp, "double::create_cb", create_cb, NULL, NULL);
    Tcl_CreateObjCommand(interp, "double::display_cb", display_cb, NULL, NULL);
    Tcl_CreateObjCommand(interp, "double::reshape_cb", reshape_cb, NULL, NULL);

    /* 
     * Make a new Togl widget command so the Tcl code can set a C variable.
     */

    Tcl_CreateObjCommand(interp, "double::setXrot", setXrot_cb, NULL, NULL);
    Tcl_CreateObjCommand(interp, "double::setYrot", setYrot_cb, NULL, NULL);

    /* 
     * Call Tcl_CreateCommand for application-specific commands, if
     * they weren't already created by the init procedures called above.
     */

    return TCL_OK;
}
