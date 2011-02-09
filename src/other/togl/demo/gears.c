/* gears.c */

/* 
 * 3-D gear wheels.  This program is in the public domain.
 *
 * Brian Paul
 *
 *
 * Modified to work under Togl as a widget for TK 1997
 *
 * Philip Quaife
 *
 */

#define USE_TOGL_STUBS

#include "togl.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#undef TCL_STORAGE_CLASS
#define TCL_STORAGE_CLASS DLLEXPORT

#ifndef M_PI
#  define M_PI 3.14159265
#endif
#define FM_PI ((float) M_PI)

#ifdef _MSC_VER
__inline float
sinf(double a)
{
    return (float) sin(a);
}
__inline float
cosf(double a)
{
    return (float) cos(a);
}
__inline float
sqrtf(double a)
{
    return (float) sqrt(a);
}

#  define sin sinf
#  define cos cosf
#  define sqrt sqrtf
#endif

struct WHIRLYGIZMO
{
    int     Gear1, Gear2, Gear3;
    double  Rotx, Roty, Rotz;
    double  Angle;
    int     Height, Width;
};

typedef struct WHIRLYGIZMO WHIRLYGIZMO;

/* 
 * Draw a gear wheel.  You'll probably want to call this function when
 * building a display list since we do a lot of trig here.
 *
 * Input:  inner_radius - radius of hole at center
 *         outer_radius - radius at center of teeth
 *         width - width of gear
 *         teeth - number of teeth
 *         tooth_depth - depth of tooth
 */
static void
gear(GLfloat inner_radius, GLfloat outer_radius, GLfloat width,
        GLint teeth, GLfloat tooth_depth)
{
    GLint   i;
    GLfloat r0, r1, r2;
    GLfloat angle, da;
    GLfloat u, v, len;

    r0 = inner_radius;
    r1 = outer_radius - tooth_depth / 2;
    r2 = outer_radius + tooth_depth / 2;

    da = 2 * FM_PI / teeth / 4;

    glShadeModel(GL_FLAT);

    glNormal3f(0, 0, 1);

    /* draw front face */
    glBegin(GL_QUAD_STRIP);
    for (i = 0; i <= teeth; i++) {
        angle = i * 2 * FM_PI / teeth;
        glVertex3f(r0 * cos(angle), r0 * sin(angle), width * 0.5f);
        glVertex3f(r1 * cos(angle), r1 * sin(angle), width * 0.5f);
        glVertex3f(r0 * cos(angle), r0 * sin(angle), width * 0.5f);
        glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da),
                width * 0.5f);
    }
    glEnd();

    /* draw front sides of teeth */
    glBegin(GL_QUADS);
    da = 2 * FM_PI / teeth / 4;
    for (i = 0; i < teeth; i++) {
        angle = i * 2 * FM_PI / teeth;

        glVertex3f(r1 * cos(angle), r1 * sin(angle), width * 0.5f);
        glVertex3f(r2 * cos(angle + da), r2 * sin(angle + da), width * 0.5f);
        glVertex3f(r2 * cos(angle + 2 * da), r2 * sin(angle + 2 * da),
                width * 0.5f);
        glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da),
                width * 0.5f);
    }
    glEnd();


    glNormal3f(0, 0, -1);

    /* draw back face */
    glBegin(GL_QUAD_STRIP);
    for (i = 0; i <= teeth; i++) {
        angle = i * 2 * FM_PI / teeth;
        glVertex3f(r1 * cos(angle), r1 * sin(angle), -width * 0.5f);
        glVertex3f(r0 * cos(angle), r0 * sin(angle), -width * 0.5f);
        glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da),
                -width * 0.5f);
        glVertex3f(r0 * cos(angle), r0 * sin(angle), -width * 0.5f);
    }
    glEnd();

    /* draw back sides of teeth */
    glBegin(GL_QUADS);
    da = 2 * FM_PI / teeth / 4;
    for (i = 0; i < teeth; i++) {
        angle = i * 2 * FM_PI / teeth;

        glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da),
                -width * 0.5f);
        glVertex3f(r2 * cos(angle + 2 * da), r2 * sin(angle + 2 * da),
                -width * 0.5f);
        glVertex3f(r2 * cos(angle + da), r2 * sin(angle + da), -width * 0.5f);
        glVertex3f(r1 * cos(angle), r1 * sin(angle), -width * 0.5f);
    }
    glEnd();


    /* draw outward faces of teeth */
    glBegin(GL_QUAD_STRIP);
    for (i = 0; i < teeth; i++) {
        angle = i * 2 * FM_PI / teeth;

        glVertex3f(r1 * cos(angle), r1 * sin(angle), width * 0.5f);
        glVertex3f(r1 * cos(angle), r1 * sin(angle), -width * 0.5f);
        u = r2 * cos(angle + da) - r1 * cos(angle);
        v = r2 * sin(angle + da) - r1 * sin(angle);
        len = sqrt(u * u + v * v);
        u /= len;
        v /= len;
        glNormal3f(v, -u, 0);
        glVertex3f(r2 * cos(angle + da), r2 * sin(angle + da), width * 0.5f);
        glVertex3f(r2 * cos(angle + da), r2 * sin(angle + da), -width * 0.5f);
        glNormal3f(cos(angle), sin(angle), 0);
        glVertex3f(r2 * cos(angle + 2 * da), r2 * sin(angle + 2 * da),
                width * 0.5f);
        glVertex3f(r2 * cos(angle + 2 * da), r2 * sin(angle + 2 * da),
                -width * 0.5f);
        u = r1 * cos(angle + 3 * da) - r2 * cos(angle + 2 * da);
        v = r1 * sin(angle + 3 * da) - r2 * sin(angle + 2 * da);
        glNormal3f(v, -u, 0);
        glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da),
                width * 0.5f);
        glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da),
                -width * 0.5f);
        glNormal3f(cos(angle), sin(angle), 0);
    }

    glVertex3f(r1 /* * cos(0) */ , /* r1 * sin(0) */ 0, width * 0.5f);
    glVertex3f(r1 /* * cos(0) */ , /* r1 * sin(0) */ 0, -width * 0.5f);

    glEnd();


    glShadeModel(GL_SMOOTH);

    /* draw inside radius cylinder */
    glBegin(GL_QUAD_STRIP);
    for (i = 0; i <= teeth; i++) {
        angle = i * 2 * FM_PI / teeth;
        glNormal3f(-cos(angle), -sin(angle), 0);
        glVertex3f(r0 * cos(angle), r0 * sin(angle), -width * 0.5f);
        glVertex3f(r0 * cos(angle), r0 * sin(angle), width * 0.5f);
    }
    glEnd();

}

/* 
 * static GLfloat view_rotx=20, view_roty=30, view_rotz=0; static GLint
 * gear1, gear2, gear3; static GLfloat angle = 0; */
static GLuint limit;
static GLuint count = 1;

static GLubyte polycolor[4] = { 255, 255, 255, 255 };

static int
draw(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    WHIRLYGIZMO *Wg;
    Togl   *togl;

    if (objc != 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "pathName");
        return TCL_ERROR;
    }

    if (Togl_GetToglFromObj(interp, objv[1], &togl) != TCL_OK) {
        return TCL_ERROR;
    }

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    Wg = (WHIRLYGIZMO *) Togl_GetClientData(togl);
    glDisable(GL_TEXTURE_2D);
    glPushMatrix();
    glRotatef((float) Wg->Rotx, 1, 0, 0);
    glRotatef((float) Wg->Roty, 0, 1, 0);
    glRotatef((float) Wg->Rotz, 0, 0, 1);

    glPushMatrix();
    glTranslatef(-3, -2, 0);
    glRotatef((float) Wg->Angle, 0, 0, 1);
    glEnable(GL_DEPTH_TEST);
    glCallList(Wg->Gear1);
    glEnable(GL_DEPTH_TEST);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(3.1f, -2, 0);
    glRotatef(-2 * (float) Wg->Angle - 9, 0, 0, 1);
    glCallList(Wg->Gear2);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(-3.1f, 4.2f, 0);
    glRotatef(-2 * (float) Wg->Angle - 25, 0, 0, 1);
    glCallList(Wg->Gear3);
    glPopMatrix();

    glPopMatrix();

    Togl_SwapBuffers(togl);

    return TCL_OK;
}


static int
zap(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    WHIRLYGIZMO *Wg;
    Togl   *togl;

    if (objc != 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "pathName");
        return TCL_ERROR;
    }

    if (Togl_GetToglFromObj(interp, objv[1], &togl) != TCL_OK) {
        return TCL_ERROR;
    }

    Wg = (WHIRLYGIZMO *) Togl_GetClientData(togl);
    free(Wg);

    return TCL_OK;
}


static int
idle(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    WHIRLYGIZMO *Wg;
    Togl   *togl;

    if (objc != 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "pathName");
        return TCL_ERROR;
    }

    if (Togl_GetToglFromObj(interp, objv[1], &togl) != TCL_OK) {
        return TCL_ERROR;
    }

    Wg = (WHIRLYGIZMO *) Togl_GetClientData(togl);
    Wg->Angle += 2;
    Togl_PostRedisplay(togl);

    return TCL_OK;
}


/* change view angle, exit upon ESC */
/* 
 * static GLenum key(int k, GLenum mask) { switch (k) { case TK_UP: view_rotx
 * += 5; return GL_TRUE; case TK_DOWN: view_rotx -= 5; return GL_TRUE; case 
 * TK_LEFT: view_roty += 5; return GL_TRUE; case TK_RIGHT: view_roty -= 5;
 * return GL_TRUE; case TK_z: view_rotz += 5; return GL_TRUE; case TK_Z:
 * view_rotz -= 5; return GL_TRUE; } return GL_FALSE; } */

/* new window size or exposure */
static int
reshape(ClientData clientData, Tcl_Interp *interp, int objc,
        Tcl_Obj *const *objv)
{
    int     width, height;
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

    return TCL_OK;
}


static int
init(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    WHIRLYGIZMO *Wg;
    static GLfloat red[4] = { 0.8f, 0.1f, 0, 1 };
    static GLfloat green[4] = { 0, 0.8f, 0.2f, 1 };
    static GLfloat blue[4] = { 0.2f, 0.2f, 1, 1 };
    static GLfloat pos[4] = { 5, 5, 10, 0 };
    Togl   *togl;

    if (objc != 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "pathName");
        return TCL_ERROR;
    }

    if (Togl_GetToglFromObj(interp, objv[1], &togl) != TCL_OK) {
        return TCL_ERROR;
    }

    glLightfv(GL_LIGHT0, GL_POSITION, pos);
    glEnable(GL_CULL_FACE);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_DEPTH_TEST);
    /* make the gears */
    Wg = (WHIRLYGIZMO *) malloc(sizeof (WHIRLYGIZMO));
    if (!Wg) {
        Tcl_SetResult(Togl_Interp(togl),
                "\"Cannot allocate client data for widget\"", TCL_STATIC);
    }
    Wg->Gear1 = glGenLists(1);
    glNewList(Wg->Gear1, GL_COMPILE);
    glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, red);
    gear(1, 4, 1, 20, 0.7f);
    glEndList();

    Wg->Gear2 = glGenLists(1);
    glNewList(Wg->Gear2, GL_COMPILE);
    glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, green);
    gear(0.5f, 2, 2, 10, 0.7f);
    glEndList();

    Wg->Gear3 = glGenLists(1);
    glNewList(Wg->Gear3, GL_COMPILE);
    glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, blue);
    gear(1.3f, 2, 0.5f, 10, 0.7f);
    glEndList();

    glEnable(GL_NORMALIZE);
    Wg->Height = Togl_Height(togl);
    Wg->Width = Togl_Width(togl);
    Wg->Angle = 0;
    Wg->Rotx = 0;
    Wg->Roty = 0;
    Wg->Rotz = 0;
    Togl_SetClientData(togl, (ClientData) Wg);

    return TCL_OK;
}

static int
position(ClientData clientData, Tcl_Interp *interp, int objc,
        Tcl_Obj *const *objv)
{
    WHIRLYGIZMO *Wg;
    char    Result[100];
    Togl   *togl;

    if (objc != 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "pathName");
        return TCL_ERROR;
    }

    if (Togl_GetToglFromObj(interp, objv[1], &togl) != TCL_OK) {
        return TCL_ERROR;
    }

    Wg = (WHIRLYGIZMO *) Togl_GetClientData(togl);

    /* Let result string equal value */
    sprintf(Result, "%g %g", Wg->Roty, Wg->Rotx);

    Tcl_SetResult(interp, Result, TCL_VOLATILE);
    return TCL_OK;
}

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

EXTERN int
Gears_Init(Tcl_Interp *interp)
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
    Tcl_CreateObjCommand(interp, "init", init, NULL, NULL);
    Tcl_CreateObjCommand(interp, "zap", zap, NULL, NULL);
    Tcl_CreateObjCommand(interp, "draw", draw, NULL, NULL);
    Tcl_CreateObjCommand(interp, "reshape", reshape, NULL, NULL);
    Tcl_CreateObjCommand(interp, "idle", idle, NULL, NULL);
    Tcl_CreateObjCommand(interp, "rotate", rotate, NULL, NULL);
    Tcl_CreateObjCommand(interp, "position", position, NULL, NULL);
    return TCL_OK;
}
