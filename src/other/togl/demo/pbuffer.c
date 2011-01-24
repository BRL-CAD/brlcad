/* $Id$ */

/* 
 * Togl - a Tk OpenGL widget
 * Copyright (C) 1996-1997  Brian Paul and Ben Bederson
 * Copyright (C) 2006-2007  Greg Couch
 * See the LICENSE file for copyright details.
 */

#undef PBUFFER_DEBUG

#define USE_TOGL_STUBS

#include "togl.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#if defined(TOGL_AGL)
#  include <OpenGL/glu.h>
#  include <AGL/agl.h>
#elif defined(TOGL_NSOPENGL)
#  include <OpenGL/glu.h>
#  include <OpenGL/OpenGL.h>
#else
#  include <GL/glu.h>
#endif
#include <GL/glext.h>           /* OpenGL 1.4 GL_GENERATE_MIPMAP */

#undef TCL_STORAGE_CLASS
#define TCL_STORAGE_CLASS DLLEXPORT

static double xAngle = 0, yAngle = 0, zAngle = 0;
static GLdouble CornerX, CornerY, CornerZ;      /* where to print strings */
static GLuint texture;
static Togl *output;

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
    double  version;

    if (objc != 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "pathName");
        return TCL_ERROR;
    }

    if (Togl_GetToglFromObj(interp, objv[1], &togl) != TCL_OK) {
        return TCL_ERROR;
    }

    version = atof((const char *) glGetString(GL_VERSION));
    if (version < 1.4) {
        Tcl_SetResult(interp, "need OpenGL 1.4 or later", TCL_STATIC);
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


/* 
 * Togl widget reshape callback.  This is called by Tcl/Tk when the widget
 * has been resized.  Typically, we call glViewport and perhaps setup the
 * projection matrix.
 */
static int
reshape2_cb(ClientData clientData, Tcl_Interp *interp, int objc,
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
    glOrtho(-aspect, aspect, -1, 1, -1, 1);

    /* Change back to model view transform for rendering */
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    return TCL_OK;
}


static void
draw_object()
{
    static GLuint cubeList = 0;

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

    if (Togl_GetToglFromObj(interp, objv[1], &togl) != TCL_OK)
        return TCL_ERROR;

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    draw_object();

#ifdef PBUFFER_DEBUG
    {
        Tk_PhotoHandle photo;

        /* first tcl: image create photo test */
        photo = Tk_FindPhoto(interp, "test2");
        if (photo == NULL) {
            fprintf(stderr, "missing tk photo object test2\n");
        } else {
            Togl_TakePhoto(togl, photo);
            Tcl_Eval(interp, "test2 write test2.ppm -format ppm");
        }
    }
#endif
    Togl_SwapBuffers(togl);
    return TCL_OK;
}


/* 
 * Togl widget display callback.  This is called by Tcl/Tk when the widget's
 * contents have to be redrawn.  Typically, we clear the color and depth
 * buffers, render our objects, then swap the front/back color buffers.
 */
static int
display2_cb(ClientData clientData, Tcl_Interp *interp, int objc,
        Tcl_Obj *const *objv)
{
    Togl   *togl;

    if (objc != 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "pathName");
        return TCL_ERROR;
    }

    if (Togl_GetToglFromObj(interp, objv[1], &togl) != TCL_OK)
        return TCL_ERROR;

    glClear(GL_COLOR_BUFFER_BIT);

    if (texture) {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, texture);
        glBegin(GL_QUADS);
        glTexCoord2i(0, 0);
        glVertex2i(-1, -1);
        glTexCoord2i(1, 0);
        glVertex2i(1, -1);
        glTexCoord2i(1, 1);
        glVertex2i(1, 1);
        glTexCoord2i(0, 1);
        glVertex2i(-1, 1);
        glEnd();
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    Togl_SwapBuffers(togl);
    return TCL_OK;
}


/* 
 * Togl widget display callback.  This is called by Tcl/Tk when the widget's
 * contents have to be redrawn.  Typically, we clear the color and depth
 * buffers, render our objects, then swap the front/back color buffers.
 */
static int
pbuffer_cb(ClientData clientData, Tcl_Interp *interp, int objc,
        Tcl_Obj *const *objv)
{
    Togl   *togl;
    int     width;
    int     height;
    GLenum  error;

    if (objc != 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "pathName");
        return TCL_ERROR;
    }

    if (Togl_GetToglFromObj(interp, objv[1], &togl) != TCL_OK)
        return TCL_ERROR;

    width = Togl_Width(togl);
    height = Togl_Height(togl);

    if (texture == 0) {
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
#if !defined(TOGL_AGL) && !defined(TOGL_NSOPENGL)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                GL_LINEAR_MIPMAP_LINEAR);
#else
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
#endif
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB,
                GL_BYTE, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
        glBindTexture(GL_TEXTURE_2D, 0);
        error = glGetError();
        if (error != GL_NO_ERROR) {
            fprintf(stderr, "texture init: %s\n", gluErrorString(error));
        }
#if 0 && defined(TOGL_AGL)
        AGLContext ctx = aglGetCurrentContext();
        AGLPbuffer pbuf;
        GLint   face, level, screen;
        GLenum  err;

        aglGetPBuffer(ctx, &pbuf, &face, &level, &screen);
        err = aglGetError();
        if (err != AGL_NO_ERROR)
            fprintf(stderr, "getPBuffer: %s\n", aglErrorString(err));
        aglTexImagePBuffer(ctx, pbuf, GL_FRONT);
        err = aglGetError();
        if (err != AGL_NO_ERROR)
            fprintf(stderr, "teximagepbuffer: %s\n", aglErrorString(err));
#endif
    }

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    draw_object();

#if 1 || !defined(TOGL_AGL)
    glBindTexture(GL_TEXTURE_2D, texture);
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, width, height);
    glBindTexture(GL_TEXTURE_2D, 0);
    error = glGetError();
    if (error != GL_NO_ERROR) {
        fprintf(stderr, "after tex copy: %s\n", gluErrorString(error));
    }
#endif
#ifdef PBUFFER_DEBUG
    {
        Tk_PhotoHandle photo;

        /* first tcl: image create photo test */
        photo = Tk_FindPhoto(interp, "test");
        Togl_TakePhoto(togl, photo);
        Tcl_Eval(interp, "test write test.ppm -format ppm");
    }
#endif
    Togl_SwapBuffers(togl);
    if (output)
        Togl_PostRedisplay(output);

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
    if (xAngle < 0.0)
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
        Tcl_WrongNumArgs(interp, 1, objv, "angle");
        return TCL_ERROR;
    }

    if (Tcl_GetDoubleFromObj(interp, objv[1], &yAngle) != TCL_OK) {
        return TCL_ERROR;
    }

    yAngle = fmod(yAngle, 360.0);
    if (yAngle < 0.0)
        yAngle += 360.0;

    /* Let result equal value */
    Tcl_SetObjResult(interp, objv[1]);
    return TCL_OK;
}

static int
setOutput_cb(ClientData clientData, Tcl_Interp *interp, int objc,
        Tcl_Obj *const *objv)
{

    if (objc != 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "pathName");
        return TCL_ERROR;
    }

    if (Togl_GetToglFromObj(interp, objv[1], &output) != TCL_OK)
        return TCL_ERROR;

    return TCL_OK;
}

/* 
 * Called by Tcl to let me initialize the modules (Togl) I will need.
 */
EXTERN int
Pbuffer_Init(Tcl_Interp *interp)
{
    /* 
     * Initialize Tcl and the Togl widget module.
     */
    if (Tcl_InitStubs(interp, "8.1", 0) == NULL
#ifdef PBUFFER_DEBUG
            || Tk_InitStubs(interp, "8.1", 0) == NULL
#endif
            || Togl_InitStubs(interp, "2.0", 0) == NULL) {
        return TCL_ERROR;
    }

    /* 
     * Specify the C callback functions for widget creation, display,
     * and reshape.
     */
    Tcl_CreateObjCommand(interp, "create_cb", create_cb, NULL, NULL);
    Tcl_CreateObjCommand(interp, "display_cb", display_cb, NULL, NULL);
    Tcl_CreateObjCommand(interp, "display2_cb", display2_cb, NULL, NULL);
    Tcl_CreateObjCommand(interp, "pbuffer_cb", pbuffer_cb, NULL, NULL);
    Tcl_CreateObjCommand(interp, "reshape_cb", reshape_cb, NULL, NULL);
    Tcl_CreateObjCommand(interp, "reshape2_cb", reshape2_cb, NULL, NULL);

    /* 
     * Make a new Togl widget command so the Tcl code can set a C variable.
     */

    Tcl_CreateObjCommand(interp, "setXrot", setXrot_cb, NULL, NULL);
    Tcl_CreateObjCommand(interp, "setYrot", setYrot_cb, NULL, NULL);
    Tcl_CreateObjCommand(interp, "setOutput", setOutput_cb, NULL, NULL);

    /* 
     * Call Tcl_CreateCommand for application-specific commands, if
     * they weren't already created by the init procedures called above.
     */

    return TCL_OK;
}
