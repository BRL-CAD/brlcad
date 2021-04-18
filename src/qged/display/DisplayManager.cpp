/*              D I S P L A Y M A N A G E R . C P P
 * BRL-CAD
 *
 * Copyright (c) 2020-2021 United States Government as represented by
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
/** @file DisplayManager.cpp */

#include <rt/db_internal.h>
#include <rt/functab.h>
#include <rt/db_io.h>
#include "DisplayManager.h"
#include <rt/global.h>

#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic push
#endif
#if defined(__clang__)
#  pragma clang diagnostic push
#endif
#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic ignored "-Wshadow"
#endif
#if defined(__clang__)
#  pragma clang diagnostic ignored "-Wshadow"
#endif

#define DM_SOLID_LINE 0
#define DM_DASHED_LINE 1

DisplayManager::DisplayManager(BRLCADDisplay *display) : display(display) {
    setFGColor(0,0,0, 1);
    glLineStipple(1, 0xCF33);
}

void DisplayManager::drawVList(bn_vlist *vp) {

    struct bn_vlist *tvp;
    int first = 1;
    int mflag = 1;
    GLfloat originalPointSize, originalLineWidth;
    GLdouble m[16];
    GLdouble mt[16];
    GLdouble tlate[3];
    const  float black[4] = {0.0, 0.0, 0.0, 0.0};

    glGetFloatv(GL_POINT_SIZE, &originalPointSize);
    glGetFloatv(GL_LINE_WIDTH, &originalLineWidth);

    /* Viewing region is from -1.0 to +1.0 */

    for (BU_LIST_FOR(tvp, bn_vlist, &vp->l)) {
        int i;
        int nused = tvp->nused;
        int *cmd = tvp->cmd;
        point_t *pt = tvp->pt;
        GLdouble glpt[3];

        for (i = 0; i < nused; i++, cmd++, pt++) {
            VMOVE(glpt, *pt);

            switch (*cmd) {
                case BN_VLIST_LINE_MOVE:
                    /* Move, start line */
                    if (first == 0)
                        glEnd();
                    first = 0;

                    if (dmLight && mflag) {
                        mflag = 0;
                        glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, wireColor);
                        glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, black);
                        glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, black);
                        glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, black);

                        if (dmTransparency)
                            glDisable(GL_BLEND);
                    }

                    glBegin(GL_LINE_STRIP);
                    glVertex3dv(glpt);
                    break;
                case BN_VLIST_MODEL_MAT:
                    if (first == 0) {
                        glEnd();
                        first = 1;
                    }

                    glMatrixMode(GL_MODELVIEW);
                    glPopMatrix();
                    break;
                case BN_VLIST_DISPLAY_MAT:
                    glMatrixMode(GL_MODELVIEW);
                    glGetDoublev(GL_MODELVIEW_MATRIX, m);

                    MAT_TRANSPOSE(mt, m);
                    MAT4X3PNT(tlate, mt, glpt);

                    glPushMatrix();
                    glLoadIdentity();
                    glTranslated(tlate[0], tlate[1], tlate[2]);
                    /* 96 dpi = 3.78 pixel/mm hardcoded */
                    glScaled(2. * 3.78 / display->getW(),
                             2. * 3.78 / display->getH(),
                             1.);
                    break;
                case BN_VLIST_POLY_START:
                case BN_VLIST_TRI_START:
                    /* Start poly marker & normal */

                    if (dmLight && mflag) {
                        mflag = 0;
                        glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, black);
                        glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, ambientColor);
                        glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, specularColor);
                        glMaterialfv(GL_FRONT, GL_DIFFUSE, diffuseColor);

                        switch (dmLight) {
                            case 1:
                                break;
                            case 2:
                                glMaterialfv(GL_BACK, GL_DIFFUSE, diffuseColor);
                                break;
                            case 3:
                                glMaterialfv(GL_BACK, GL_DIFFUSE, backDiffuseColorDark);
                                break;
                            default:
                                glMaterialfv(GL_BACK, GL_DIFFUSE, backDiffuseColorLight);
                                break;
                        }

                        if (dmTransparency)
                            glEnable(GL_BLEND);
                    }

                    if (*cmd == BN_VLIST_POLY_START) {
                        if (first == 0)
                            glEnd();

                        glBegin(GL_POLYGON);
                    } else if (first)
                        glBegin(GL_TRIANGLES);

                    /* Set surface normal (vl_pnt points outward) */
                    glNormal3dv(glpt);

                    first = 0;

                    break;
                case BN_VLIST_LINE_DRAW:
                case BN_VLIST_POLY_MOVE:
                case BN_VLIST_POLY_DRAW:
                case BN_VLIST_TRI_MOVE:
                case BN_VLIST_TRI_DRAW:
                    glVertex3dv(glpt);

                    break;
                case BN_VLIST_POLY_END:
                    /* Draw, End Polygon */
                    glVertex3dv(glpt);
                    glEnd();
                    first = 1;
                    break;
                case BN_VLIST_TRI_END:
                    break;
                case BN_VLIST_POLY_VERTNORM:
                case BN_VLIST_TRI_VERTNORM:
                    /* Set per-vertex normal.  Given before vert. */
                    glNormal3dv(glpt);
                    break;
                case BN_VLIST_POINT_DRAW:
                    if (first == 0)
                        glEnd();
                    first = 0;
                    glBegin(GL_POINTS);
                    glVertex3dv(glpt);
                    break;
                case BN_VLIST_LINE_WIDTH: {
                    GLfloat lineWidth = (GLfloat) (*pt)[0];
                    if (lineWidth > 0.0) {
                        glLineWidth(lineWidth);
                    }
                    break;
                }
                case BN_VLIST_POINT_SIZE: {
                    GLfloat pointSize = (GLfloat) (*pt)[0];
                    if (pointSize > 0.0) {
                        glPointSize(pointSize);
                    }
                    break;
                }
            }
        }
    }

    if (first == 0)
        glEnd();

    if (dmLight && dmTransparency)
        glDisable(GL_BLEND);

    glPointSize(originalPointSize);
    glLineWidth(originalLineWidth);
    return;
}


void DisplayManager::setFGColor(float r, float g, float b, float transparency) {
    wireColor[0] = r;
    wireColor[1] = g;
    wireColor[2] = b;
    wireColor[3] = transparency;

    diffuseColor[0] = wireColor[0] * 0.6f;
    diffuseColor[1] = wireColor[1] * 0.6f;
    diffuseColor[2] = wireColor[2] * 0.6f;
    diffuseColor[3] = wireColor[3];

    ambientColor[0] = wireColor[0] * 0.2f;
    ambientColor[1] = wireColor[1] * 0.2f;
    ambientColor[2] = wireColor[2] * 0.2f;
    ambientColor[3] = wireColor[3];

    specularColor[0] = ambientColor[0];
    specularColor[1] = ambientColor[1];
    specularColor[2] = ambientColor[2];
    specularColor[3] = ambientColor[3];

    backDiffuseColorDark[0] = wireColor[0] * 0.3f;
    backDiffuseColorDark[1] = wireColor[1] * 0.3f;
    backDiffuseColorDark[2] = wireColor[2] * 0.3f;
    backDiffuseColorDark[3] = wireColor[3];

    backDiffuseColorLight[0] = wireColor[0] * 0.9f;
    backDiffuseColorLight[1] = wireColor[1] * 0.9f;
    backDiffuseColorLight[2] = wireColor[2] * 0.9f;
    backDiffuseColorLight[3] = wireColor[3];

    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, ambientColor);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, specularColor);
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, diffuseColor);
}

/*
 * Set the style of the line
 *
 * Valid Styles,
 * DM_SOLID_LINE=0
 * DM_DASHED_LINE=1
 */
void DisplayManager::setLineStyle(int style)
{
    if (style == DM_DASHED_LINE) {
        glEnable(GL_LINE_STIPPLE);
    }
    else{
        glDisable(GL_LINE_STIPPLE);
    }
}

void DisplayManager::setLineWidth(int width)
{
    glLineWidth((GLfloat) width);
}

/*
 * Set the width and style of the line.
 *
 * Valid Styles,
 * DM_SOLID_LINE=0
 * DM_DASHED_LINE=1
 */
void DisplayManager::setLineAttr(int width, int style)
{
    if (width>0) {
        glLineWidth((GLfloat) width);
    }

    if (style == DM_DASHED_LINE) {
        glEnable(GL_LINE_STIPPLE);
    }
    else {
        glDisable(GL_LINE_STIPPLE);
    }
}

/*
 * Displays a saved display list identified by `list`
 */
void DisplayManager::drawDList(unsigned int list) {
    glCallList((GLuint) list);
}

/*
 * Generates `range` number of display lists and returns first display list's index
 */
unsigned int DisplayManager::genDLists(size_t range)
{
    return glGenLists((GLsizei)range);
}

/*
 * Supported subsequent opengl commands are compiled into the display list `list` rather than being rendered to the screen.
 * Should have called genDLists and generated the display list before calling this.
 */
void DisplayManager::beginDList(unsigned int list)
{
    glNewList((GLuint)list, GL_COMPILE);
}

/*
 * End of the display list initiated by beginDList.
 */
void DisplayManager::endDList()
{
    glEndList();
}

/*
 * End of the display list initiated by beginDList.
 */
GLboolean DisplayManager::isDListValid(unsigned int list)
{
    return glIsList(list);
}

void DisplayManager::freeDLists(unsigned int list, int range)
{
    glDeleteLists((GLuint)list, (GLsizei)range);
}
void DisplayManager::drawBegin()
{
    glClearColor(bgColor[0],bgColor[1],bgColor[2],1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
//    glMatrixMode(GL_PROJECTION);
//    glPopMatrix();
//    glMatrixMode(GL_MODELVIEW);
//    glPopMatrix();

}

void DisplayManager::saveState(){
    glPushAttrib(GL_ALL_ATTRIB_BITS);
}
void DisplayManager::restoreState(){
    glPopAttrib();
}

void DisplayManager::loadMatrix(const GLfloat *m)
{
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glLoadMatrixf(m);
}
void DisplayManager::loadPMatrix(const GLfloat *m)
{
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glLoadMatrixf(m);
}

#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic pop
#endif
#if defined(__clang__)
#  pragma clang diagnostic pop
#endif

