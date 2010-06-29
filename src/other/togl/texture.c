/* $Id$ */

/* 
 * Togl - a Tk OpenGL widget
 * Copyright (C) 1996-1997  Brian Paul and Ben Bederson
 * Copyright (C) 2006-2007  Greg Couch
 * See the LICENSE file for copyright details.
 */


/* 
 * An example Togl program demonstrating texture mapping
 */

#define USE_TOGL_STUBS

#include "togl.h"
#include <stdlib.h>
#include <string.h>
#if defined(TOGL_AGL)
#  include <OpenGL/glu.h>
#else
#  include <GL/glu.h>
#endif
#include "image.h"

#undef TCL_STORAGE_CLASS
#define TCL_STORAGE_CLASS DLLEXPORT

#define CHECKER 0
#define FACE 1
#define TREE 2


static GLenum minfilter = GL_NEAREST_MIPMAP_LINEAR;
static GLenum magfilter = GL_LINEAR;
static GLenum swrap = GL_REPEAT;
static GLenum twrap = GL_REPEAT;
static GLenum envmode = GL_MODULATE;
static GLubyte polycolor[4] = { 255, 255, 255, 255 };
static int teximage = CHECKER;
static double coord_scale = 1;
static double xrot = 0;
static double yrot = 0;
static double texscale = 1;

static GLint width, height;

static GLboolean blend = GL_FALSE;


/* 
 * Load a texture image.  n is one of CHECKER, FACE or TREE.
 */
static void
texture_image(int n)
{
    if (n == CHECKER) {
#define WIDTH 64
#define HEIGHT 64
        GLubyte teximage[WIDTH * HEIGHT][4];
        int     i, j;

        for (i = 0; i < HEIGHT; i++) {
            for (j = 0; j < WIDTH; j++) {
                GLubyte value;

                value = ((i / 4 + j / 4) % 2) ? 0xff : 0x00;
                teximage[i * WIDTH + j][0] = value;
                teximage[i * WIDTH + j][1] = value;
                teximage[i * WIDTH + j][2] = value;
                teximage[i * WIDTH + j][3] = value;
            }
        }

        glEnable(GL_TEXTURE_2D);
        gluBuild2DMipmaps(GL_TEXTURE_2D, 4, WIDTH, HEIGHT,
                GL_RGBA, GL_UNSIGNED_BYTE, teximage);
        blend = GL_FALSE;

#undef WIDTH
#undef HEIGHT
    } else if (n == FACE) {
        TK_RGBImageRec *img = tkRGBImageLoad("ben.rgb");

        if (img) {
            glEnable(GL_TEXTURE_2D);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            gluBuild2DMipmaps(GL_TEXTURE_2D, img->sizeZ, img->sizeX, img->sizeY,
                    img->sizeZ == 3 ? GL_RGB : GL_RGBA,
                    GL_UNSIGNED_BYTE, img->data);

            blend = GL_TRUE;
        }
    } else if (n == TREE) {
        TK_RGBImageRec *img = tkRGBImageLoad("tree2.rgba");

        if (img) {
            glEnable(GL_TEXTURE_2D);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            gluBuild2DMipmaps(GL_TEXTURE_2D, img->sizeZ, img->sizeX, img->sizeY,
                    img->sizeZ == 3 ? GL_RGB : GL_RGBA,
                    GL_UNSIGNED_BYTE, img->data);

            blend = GL_TRUE;
        }
    } else {
        abort();
    }
}


/* 
 * Togl widget create callback.  This is called by Tcl/Tk when the widget has
 * been realized.  Here's where one may do some one-time context setup or
 * initializations.
 */
static int
create_cb(ClientData clientData, Tcl_Interp *interp, int objc,
        Tcl_Obj *const *objv)
{
    glEnable(GL_DEPTH_TEST);    /* Enable depth buffering */

    texture_image(CHECKER);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magfilter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minfilter);

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

    glViewport(0, 0, width, height);

    return TCL_OK;
}


static void
check_error(char *where)
{
    GLenum  error;

    while (1) {
        error = glGetError();
        if (error == GL_NO_ERROR) {
            break;
        }
        printf("OpenGL error near %s: %s\n", where, gluErrorString(error));
    }
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
    float   aspect = (float) width / (float) height;
    Togl   *togl;

    if (objc != 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "pathName");
        return TCL_ERROR;
    }

    if (Togl_GetToglFromObj(interp, objv[1], &togl) != TCL_OK) {
        return TCL_ERROR;
    }

    check_error("begin display\n");

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    /* Draw background image */
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glDisable(GL_TEXTURE_2D);
    glDisable(GL_DEPTH_TEST);
    glBegin(GL_POLYGON);
    glColor3f(0, 0, 0.3f);
    glVertex2f(-1, -1);
    glColor3f(0, 0, 0.3f);
    glVertex2f(1, -1);
    glColor3f(0, 0, 0.9f);
    glVertex2f(1, 1);
    glColor3f(0, 0, 0.9f);
    glVertex2f(-1, 1);
    glEnd();

    /* draw textured object */
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustum(-aspect, aspect, -1, 1, 2, 10);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(0, 0, -5);
    glScaled(texscale, texscale, texscale);
    glRotated(yrot, 0, 1, 0);
    glRotated(xrot, 1, 0, 0);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_2D);
    glColor4ubv(polycolor);

    if (blend) {
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_BLEND);
    }

    glBegin(GL_POLYGON);
    glTexCoord2f(0, 0);
    glVertex2f(-1, -1);
    glTexCoord2d(coord_scale, 0);
    glVertex2f(1, -1);
    glTexCoord2d(coord_scale, coord_scale);
    glVertex2f(1, 1);
    glTexCoord2d(0, coord_scale);
    glVertex2f(-1, 1);
    glEnd();

    glDisable(GL_BLEND);

    Togl_SwapBuffers(togl);

    return TCL_OK;
}


/* 
 * Called when a magnification filter radio button is pressed.
 */
static int
magfilter_cmd(ClientData clientData, Tcl_Interp *interp, int objc,
        Tcl_Obj *const *objv)
{
    static const char *names[] = { "GL_NEAREST", "GL_LINEAR", NULL };
    static const GLenum magfilters[] = { GL_NEAREST, GL_LINEAR };
    int     result, index;
    Togl   *togl;

    if (objc != 3) {
        Tcl_WrongNumArgs(interp, 1, objv, "pathName magnification-filter-type");
        return TCL_ERROR;
    }

    if (Togl_GetToglFromObj(interp, objv[1], &togl) != TCL_OK) {
        return TCL_ERROR;
    }

    result = Tcl_GetIndexFromObj(interp, objv[2], names,
            "magnification filter type", 0, &index);
    if (result == TCL_OK) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                magfilters[index]);
        Togl_PostRedisplay(togl);
    }
    return result;
}


/* 
 * Called when a minification filter radio button is pressed.
 */
static int
minfilter_cmd(ClientData clientData, Tcl_Interp *interp, int objc,
        Tcl_Obj *const *objv)
{
    static const char *names[] = {
        "GL_NEAREST", "GL_LINEAR",
        "GL_NEAREST_MIPMAP_NEAREST", "GL_LINEAR_MIPMAP_NEAREST",
        "GL_NEAREST_MIPMAP_LINEAR", "GL_LINEAR_MIPMAP_LINEAR", NULL
    };
    static const GLenum minfilters[] = {
        GL_NEAREST, GL_LINEAR,
        GL_NEAREST_MIPMAP_NEAREST, GL_LINEAR_MIPMAP_NEAREST,
        GL_NEAREST_MIPMAP_LINEAR, GL_LINEAR_MIPMAP_LINEAR
    };
    int     result, index;
    Togl   *togl;

    if (objc != 3) {
        Tcl_WrongNumArgs(interp, 1, objv, "pathName minification-filter-type");
        return TCL_ERROR;
    }

    if (Togl_GetToglFromObj(interp, objv[1], &togl) != TCL_OK) {
        return TCL_ERROR;
    }

    result = Tcl_GetIndexFromObj(interp, objv[2], names,
            "minification filter type", 0, &index);
    if (result == TCL_OK) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                minfilters[index]);
        Togl_PostRedisplay(togl);
    }
    return result;
}


static int
xrot_cmd(ClientData clientData, Tcl_Interp *interp, int objc,
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

    if (Tcl_GetDoubleFromObj(interp, objv[2], &xrot) != TCL_OK) {
        return TCL_ERROR;
    }

    Togl_PostRedisplay(togl);

    /* Let result string equal value */
    Tcl_SetObjResult(interp, objv[2]);
    return TCL_OK;
}


static int
yrot_cmd(ClientData clientData, Tcl_Interp *interp, int objc,
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

    if (Tcl_GetDoubleFromObj(interp, objv[2], &yrot) != TCL_OK) {
        return TCL_ERROR;
    }

    Togl_PostRedisplay(togl);

    /* Let result string equal value */
    Tcl_SetObjResult(interp, objv[2]);
    return TCL_OK;
}


static int
texscale_cmd(ClientData clientData, Tcl_Interp *interp, int objc,
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

    if (Tcl_GetDoubleFromObj(interp, objv[2], &texscale) != TCL_OK) {
        return TCL_ERROR;
    }

    Togl_PostRedisplay(togl);

    /* Let result string equal value */
    Tcl_SetObjResult(interp, objv[2]);
    return TCL_OK;
}


/* 
 * Called when S texture coordinate wrapping is changed.
 */
static int
swrap_cmd(ClientData clientData, Tcl_Interp *interp, int objc,
        Tcl_Obj *const *objv)
{
    static const char *names[] = { "GL_CLAMP", "GL_REPEAT", NULL };
    static const GLenum swraps[] = { GL_CLAMP, GL_REPEAT };
    int     result, index;
    Togl   *togl;

    if (objc != 3) {
        Tcl_WrongNumArgs(interp, 1, objv, "pathName wrap-mode");
        return TCL_ERROR;
    }

    if (Togl_GetToglFromObj(interp, objv[1], &togl) != TCL_OK) {
        return TCL_ERROR;
    }

    result = Tcl_GetIndexFromObj(interp, objv[2], names,
            "wrap mode", 0, &index);
    if (result == TCL_OK) {
        swrap = swraps[index];
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, swrap);
        Togl_PostRedisplay(togl);
        /* Let result string equal value */
        Tcl_SetObjResult(interp, objv[2]);
    }
    return result;
}


/* 
 * Called when T texture coordinate wrapping is changed.
 */
static int
twrap_cmd(ClientData clientData, Tcl_Interp *interp, int objc,
        Tcl_Obj *const *objv)
{
    static const char *names[] = { "GL_CLAMP", "GL_REPEAT", NULL };
    static const GLenum twraps[] = { GL_CLAMP, GL_REPEAT };
    int     result, index;
    Togl   *togl;

    if (objc != 3) {
        Tcl_WrongNumArgs(interp, 1, objv, "pathName wrap-mode");
        return TCL_ERROR;
    }

    if (Togl_GetToglFromObj(interp, objv[1], &togl) != TCL_OK) {
        return TCL_ERROR;
    }

    result = Tcl_GetIndexFromObj(interp, objv[2], names,
            "wrap mode", 0, &index);
    if (result == TCL_OK) {
        twrap = twraps[index];
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, twrap);
        Togl_PostRedisplay(togl);
        /* Let result string equal value */
        Tcl_SetObjResult(interp, objv[2]);
    }
    return result;
}


/* 
 * Called when the texture environment mode is changed.
 */
static int
envmode_cmd(ClientData clientData, Tcl_Interp *interp, int objc,
        Tcl_Obj *const *objv)
{
    static const char *names[] = {
        "GL_MODULATE", "GL_DECAL", "GL_BLEND", NULL
    };
    static const GLenum envmodes[] = { GL_MODULATE, GL_DECAL, GL_BLEND };
    int     result, index;
    Togl   *togl;

    if (objc != 3) {
        Tcl_WrongNumArgs(interp, 1, objv, "pathName texture-env-mode");
        return TCL_ERROR;
    }

    if (Togl_GetToglFromObj(interp, objv[1], &togl) != TCL_OK) {
        return TCL_ERROR;
    }

    result = Tcl_GetIndexFromObj(interp, objv[2], names,
            "texture env mode", 0, &index);
    if (result == TCL_OK) {
        envmode = envmodes[index];
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, envmode);
        Togl_PostRedisplay(togl);
        /* Let result string equal value */
        Tcl_SetObjResult(interp, objv[2]);
    }
    return result;
}


/* 
 * Called when the polygon color is changed.
 */
static int
polycolor_cmd(ClientData clientData, Tcl_Interp *interp, int objc,
        Tcl_Obj *const *objv)
{
    Togl   *togl;
    int     r, g, b;

    if (objc != 5) {
        Tcl_WrongNumArgs(interp, 1, objv, "pathName r g b");
        return TCL_ERROR;
    }

    if (Togl_GetToglFromObj(interp, objv[1], &togl) != TCL_OK) {
        return TCL_ERROR;
    }

    if (Tcl_GetIntFromObj(interp, objv[2], &r) != TCL_OK
            || Tcl_GetIntFromObj(interp, objv[3], &g) != TCL_OK
            || Tcl_GetIntFromObj(interp, objv[4], &b) != TCL_OK) {
        return TCL_ERROR;
    }
    polycolor[0] = r;
    polycolor[1] = g;
    polycolor[2] = b;

    Togl_PostRedisplay(togl);

    return TCL_OK;
}


/* 
 * Called when the texture image is to be changed
 */
static int
teximage_cmd(ClientData clientData, Tcl_Interp *interp, int objc,
        Tcl_Obj *const *objv)
{
    static const char *names[] = {
        "CHECKER", "FACE", "TREE", NULL
    };
    static const GLenum teximages[] = {
        CHECKER, FACE, TREE
    };
    int     result, index;
    Togl   *togl;

    if (objc != 3) {
        Tcl_WrongNumArgs(interp, 1, objv, "pathName texture-image-name");
        return TCL_ERROR;
    }

    if (Togl_GetToglFromObj(interp, objv[1], &togl) != TCL_OK) {
        return TCL_ERROR;
    }

    result = Tcl_GetIndexFromObj(interp, objv[2], names,
            "texture image name", 0, &index);
    if (result == TCL_OK) {
        teximage = teximages[index];
        texture_image(teximage);
        Togl_PostRedisplay(togl);
        /* Let result string equal value */
        Tcl_SetObjResult(interp, objv[2]);
    }
    return result;
}


/* 
 * Called when the texture coordinate scale is changed.
 */
static int
coord_scale_cmd(ClientData clientData, Tcl_Interp *interp, int objc,
        Tcl_Obj *const *objv)
{
    double  s;
    Togl   *togl;

    if (objc != 3) {
        Tcl_WrongNumArgs(interp, 1, objv, "pathName scale");
        return TCL_ERROR;
    }

    if (Togl_GetToglFromObj(interp, objv[1], &togl) != TCL_OK) {
        return TCL_ERROR;
    }

    if (Tcl_GetDoubleFromObj(interp, objv[2], &s) != TCL_OK) {
        return TCL_ERROR;
    }
    if (s > 0 && s < 10) {
        coord_scale = s;
        Togl_PostRedisplay(togl);
    }

    /* Let result string equal value */
    Tcl_SetObjResult(interp, objv[2]);
    return TCL_OK;
}


EXTERN int
Texture_Init(Tcl_Interp *interp)
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

    /* 
     * Make a new Togl widget command so the Tcl code can set a C variable.
     */
    Tcl_CreateObjCommand(interp, "min_filter", minfilter_cmd, NULL, NULL);
    Tcl_CreateObjCommand(interp, "mag_filter", magfilter_cmd, NULL, NULL);
    Tcl_CreateObjCommand(interp, "xrot", xrot_cmd, NULL, NULL);
    Tcl_CreateObjCommand(interp, "yrot", yrot_cmd, NULL, NULL);
    Tcl_CreateObjCommand(interp, "texscale", texscale_cmd, NULL, NULL);
    Tcl_CreateObjCommand(interp, "swrap", swrap_cmd, NULL, NULL);
    Tcl_CreateObjCommand(interp, "twrap", twrap_cmd, NULL, NULL);
    Tcl_CreateObjCommand(interp, "envmode", envmode_cmd, NULL, NULL);
    Tcl_CreateObjCommand(interp, "polycolor", polycolor_cmd, NULL, NULL);
    Tcl_CreateObjCommand(interp, "teximage", teximage_cmd, NULL, NULL);
    Tcl_CreateObjCommand(interp, "coord_scale", coord_scale_cmd, NULL, NULL);

    /* 
     * Call Tcl_CreateCommand for application-specific commands, if
     * they weren't already created by the init procedures called above.
     */

    return TCL_OK;
}
