/* $Id$ */

/* 
 * Togl - a Tk OpenGL widget
 * Copyright (C) 1996-1997  Brian Paul and Ben Bederson
 * Copyright (C) 2006-2007  Greg Couch
 * See the LICENSE file for copyright details.
 */


/* 
 * An example Togl program using an overlay.
 */

#define USE_TOGL_STUBS

#include "togl.h"
#include <stdlib.h>
#include <string.h>

#undef TCL_STORAGE_CLASS
#define TCL_STORAGE_CLASS DLLEXPORT


/* Overlay color indexes: */
static unsigned long Red, Green;


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

    /* allocate overlay color indexes */
    Red = Togl_AllocColorOverlay(togl, 1, 0, 0);
    Green = Togl_AllocColorOverlay(togl, 0, 1, 0);

    /* in this demo we always show the overlay */
    if (Togl_ExistsOverlay(togl)) {
        Togl_ShowOverlay(togl);
        printf("Red and green lines are in the overlay\n");
    } else {
        printf("Sorry, this display doesn't support overlays\n");
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

    /* Set up viewing for normal plane's context */
    glViewport(0, 0, width, height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-aspect, aspect, -1, 1, -1, 1);
    glMatrixMode(GL_MODELVIEW);

    /* Set up viewing for overlay plane's context */
    if (Togl_ExistsOverlay(togl)) {
        Togl_UseLayer(togl, TOGL_OVERLAY);
        glViewport(0, 0, width, height);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(-1, 1, -1, 1, -1, 1);
        glMatrixMode(GL_MODELVIEW);
        Togl_UseLayer(togl, TOGL_NORMAL);
    }
    return TCL_OK;
}


/* 
 * Togl widget overlay display callback.  This is called by Tcl/Tk when the
 * overlay has to be redrawn.
 */
static int
overlay_display_cb(ClientData clientData, Tcl_Interp *interp, int objc,
        Tcl_Obj *const *objv)
{
    glClear(GL_COLOR_BUFFER_BIT);

    glIndexi(Red);
    glBegin(GL_LINES);
    glVertex2f(-1, -1);
    glVertex2f(1, 1);
    glVertex2f(-1, 1);
    glVertex2f(1, -1);
    glEnd();

    glIndexi(Green);
    glBegin(GL_LINE_LOOP);
    glVertex2f(-0.5f, -0.5f);
    glVertex2f(0.5f, -0.5f);
    glVertex2f(0.5f, 0.5f);
    glVertex2f(-0.5f, 0.5f);
    glEnd();
    glFlush();
    return TCL_OK;
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
    glClear(GL_COLOR_BUFFER_BIT);

    glLoadIdentity();

    glBegin(GL_TRIANGLES);

    glColor3f(1, 0, 1);
    glVertex2f(-0.5f, -0.3f);
    glVertex2f(0.5f, -0.3f);
    glVertex2f(0, 0.6f);

    glColor3f(1, 1, 0);
    glVertex2f(-0.5f + 0.2f, -0.3f - 0.2f);
    glVertex2f(0.5f + 0.2f, -0.3f - 0.2f);
    glVertex2f(0 + 0.2f, 0.6f - 0.2f);

    glColor3f(0, 1, 1);
    glVertex2f(-0.5f + 0.4f, -0.3f - 0.4f);
    glVertex2f(0.5f + 0.4f, -0.3f - 0.4f);
    glVertex2f(0 + 0.4f, 0.6f - 0.4f);

    glEnd();

    glFlush();
    return TCL_OK;
}


/* 
 * Called by Tcl to let me initialize the modules (Togl) I will need.
 */
EXTERN int
Overlay_Init(Tcl_Interp *interp)
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

    Tcl_CreateObjCommand(interp, "overlay_display_cb", overlay_display_cb, NULL,
            NULL);

    /* 
     * Make a new Togl widget command so the Tcl code can set a C variable.
     */
    /* NONE */

    /* 
     * Call Tcl_CreateCommand for application-specific commands, if
     * they weren't already created by the init procedures called above.
     */
    return TCL_OK;
}
