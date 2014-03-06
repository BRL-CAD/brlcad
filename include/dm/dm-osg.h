/*                        D M -  O S G . H
 * BRL-CAD
 *
 * Copyright (c) 2014 United States Government as represented by
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
/** @addtogroup libdm */
/** @{ */
/** @file dm-osg.h
 *
 */

#ifndef DM_OSG_H
#define DM_OSG_H

#ifdef DM_OSG

#include "common.h"

#include "bu/vls.h"

#ifdef __cplusplus
#include <iostream>

#include <osgUtil/Optimizer>

#include <osgDB/ReadFile>
#include <osgDB/WriteFile>
#include <osgDB/Registry>

#include <osg/CoordinateSystemNode>

#include <osg/Material>

#include <osg/Switch>

#include <osgText/Font>
#include <osgText/Text>

#include <osgGA/StandardManipulator>
#include <osgGA/TrackballManipulator>
#include <osgGA/FlightManipulator>
#include <osgGA/DriveManipulator>
#include <osgGA/KeySwitchMatrixManipulator>
#include <osgGA/StateSetManipulator>
#include <osgGA/AnimationPathManipulator>
#include <osgGA/TerrainManipulator>
#include <osgGA/SphericalManipulator>

#include <osgViewer/Viewer>
#include <osgViewer/CompositeViewer>

#include <osg/Timer>

#include <osg/TexGen>
#include <osg/Texture2D>
#include <osgViewer/api/X11/GraphicsWindowX11>
#if defined(DM_WIN32)
#  include <osgViewer/api/Win32/GraphicsWindowWin32>
#endif
#include <osg/MatrixTransform>
#include <osg/ShapeDrawable>

#endif /* __cplusplus */

#define CMAP_BASE 40

/* Map +/-2048 GED space into -1.0..+1.0 :: x/2048*/
#define GED2IRIS(x)	(((float)(x))*0.00048828125)

#define Osg_MV_O(_m) offsetof(struct modifiable_osg_vars, _m)

struct modifiable_osg_vars {
    int cueing_on;
    int zclipping_on;
    int zbuffer_on;
    int lighting_on;
    int transparency_on;
    int fastfog;
    double fogdensity;
    int zbuf;
    int rgb;
    int doublebuffer;
    int depth;
    int debug;
    struct bu_vls log;
    double bound;
    int boundFlag;
};

struct osg_vars {
#ifdef __cplusplus
    GLdouble faceplate_mat[16];
#endif
    int face_flag;
    int *perspective_mode;
    int fontOffset;
    int ovec;		/* Old color map entry number */
#ifdef __cplusplus
    GLclampf r, g, b;
#endif
    struct modifiable_osg_vars mvars;
#if defined(DM_WIN32)
    HGLRC glxc; /* Need to figure out what OSG needs on Win32 */
#endif
#ifdef __cplusplus
    osg::ref_ptr<osg::GraphicsContext> graphicsContext;
    osg::Timer *timer;
#endif
};

__BEGIN_DECLS

extern void osg_fogHint();

__END_DECLS

#endif /* DM_OSG */
#endif /* DM_OSG_H */

/** @} */
/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
