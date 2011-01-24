/* $Id$ */

/* 
 * Togl - a Tk OpenGL widget
 * Copyright (C) 1996-1997  Brian Paul and Ben Bederson
 * Copyright (C) 2006-2007  Greg Couch
 * See the LICENSE file for copyright details.
 */


/* 
 * An example Togl program using color-index mode.
 */

#define USE_TOGL_STUBS

#include "togl.h"
#include <stdlib.h>
#include <string.h>

#undef TCL_STORAGE_CLASS
#define TCL_STORAGE_CLASS DLLEXPORT


/* Our color indexes: */
static unsigned long black, red, green, blue;

/* Rotation angle */
static float Angle = 0;


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

    /* allocate color indexes */
    black = Togl_AllocColor(togl, 0, 0, 0);
    red = Togl_AllocColor(togl, 1, 0, 0);
    green = Togl_AllocColor(togl, 0, 1, 0);
    blue = Togl_AllocColor(togl, 0, 0, 1);

    /* If we were using a private read/write colormap we'd setup our color
     * table with something like this: */
    /* 
     * black = 1; Togl_SetColor( togl, black, 0, 0, 0 ); red = 2;
     * Togl_SetColor( togl, red, 1, 0, 0 ); green = 3; Togl_SetColor(
     * togl, green, 0, 1, 0 ); blue = 4; Togl_SetColor( togl, blue, 0,
     * 0, 1 ); */

    glShadeModel(GL_FLAT);
    glDisable(GL_DITHER);

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
    glOrtho(-aspect, aspect, -1, 1, -1, 1);

    /* Change back to model view transform for rendering */
    glMatrixMode(GL_MODELVIEW);

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
    Togl   *togl;

    if (objc != 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "pathName");
        return TCL_ERROR;
    }

    if (Togl_GetToglFromObj(interp, objv[1], &togl) != TCL_OK) {
        return TCL_ERROR;
    }

    glClearIndex((float) black);
    glClear(GL_COLOR_BUFFER_BIT);

    glPushMatrix();
    glTranslatef(0.3f, -0.3f, 0);
    glRotatef(Angle, 0, 0, 1);
    glIndexi(red);
    glBegin(GL_TRIANGLES);
    glVertex2f(-0.5f, -0.3f);
    glVertex2f(0.5f, -0.3f);
    glVertex2f(0, 0.6f);
    glEnd();
    glPopMatrix();

    glPushMatrix();
    glRotatef(Angle, 0, 0, 1);
    glIndexi(green);
    glBegin(GL_TRIANGLES);
    glVertex2f(-0.5f, -0.3f);
    glVertex2f(0.5f, -0.3f);
    glVertex2f(0, 0.6f);
    glEnd();
    glPopMatrix();

    glPushMatrix();
    glTranslatef(-0.3f, 0.3f, 0);
    glRotatef(Angle, 0, 0, 1);
    glIndexi(blue);
    glBegin(GL_TRIANGLES);
    glVertex2f(-0.5f, -0.3f);
    glVertex2f(0.5f, -0.3f);
    glVertex2f(0, 0.6f);
    glEnd();
    glPopMatrix();

    glFlush();
    Togl_SwapBuffers(togl);

    return TCL_OK;
}


static int
timer_cb(ClientData clientData, Tcl_Interp *interp, int objc,
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

    Angle += 5.0;
    Togl_PostRedisplay(togl);

    return TCL_OK;
}


EXTERN int
Index_Init(Tcl_Interp *interp)
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
    Tcl_CreateObjCommand(interp, "::index::create_cb", create_cb, NULL, NULL);
    Tcl_CreateObjCommand(interp, "::index::display_cb", display_cb, NULL, NULL);
    Tcl_CreateObjCommand(interp, "::index::reshape_cb", reshape_cb, NULL, NULL);
    Tcl_CreateObjCommand(interp, "::index::timer_cb", timer_cb, NULL, NULL);

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
