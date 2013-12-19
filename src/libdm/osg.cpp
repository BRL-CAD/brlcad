/*                    O S G . C P P
 * BRL-CAD
 *
 * Copyright (c) 2011 United States Government as represented by
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
/** @file libdm/osg.cpp
 *
 * An interface to OSG.
 *
 */
/** @} */

#include "common.h"

#include "ged.h"
#include "tk.h"
#include "solid.h"
#include "dm.h"
#include "dm_xvars.h"
#include "dm-ogl.h"
#include <assert.h>

#if 1
#include <osgDB/ReadFile>
#include <osgDB/WriteFile>
#include <osgUtil/Optimizer>
#include <osg/CoordinateSystemNode>

#include <osg/Switch>
#include <osgText/Text>

#include <osgViewer/Viewer>
#include <osgViewer/ViewerEventHandlers>

#include <osgGA/StandardManipulator>
#include <osgGA/TrackballManipulator>
#include <osgGA/FlightManipulator>
#include <osgGA/DriveManipulator>
#include <osgGA/KeySwitchMatrixManipulator>
#include <osgGA/StateSetManipulator>
#include <osgGA/AnimationPathManipulator>
#include <osgGA/TerrainManipulator>
#include <osgGA/SphericalManipulator>

#include <iostream>

#include <osg/TexGen>
#include <osg/Texture2D>
#include <osgViewer/api/X11/GraphicsWindowX11>
#include <osg/MatrixTransform>
#include <osg/ShapeDrawable>

#else
#include <osg/Geode>
#include <osg/TexGen>
#include <osg/Texture2D>

#include <osgViewer/Viewer>
#include <osgViewer/api/X11/GraphicsWindowX11>
#include <osgDB/ReadFile>
#include <osg/MatrixTransform>
#include <osg/ShapeDrawable>
#endif


__BEGIN_DECLS
void dm_osgInit(struct dm *dmp);
void dm_osgReshape(struct dm *dmp);
void dm_osgPaint(struct dm *dmp);
void dm_osgLoadMatrix(struct dm *dmp, matp_t mp);
__END_DECLS

struct osg_stuff {
    osg::ref_ptr<osgViewer::Viewer>	viewer;
    double left, right, bottom, top, near, far;
    int prev_pflag;
};


/* Rim, body, lid, and bottom data must be reflected in x and
   y; handle and spout data across the y axis only.  */

static int patchdata[][16] =
{
    /* rim */
    {102, 103, 104, 105, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15},
    /* body */
    {12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27},
    {24, 25, 26, 27, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40},
    /* lid */
    {96, 96, 96, 96, 97, 98, 99, 100, 101, 101, 101, 101, 0, 1, 2, 3,},
    {0, 1, 2, 3, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117},
    /* bottom */
    {118, 118, 118, 118, 124, 122, 119, 121, 123, 126, 125, 120, 40, 39, 38, 37},
    /* handle */
    {41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56},
    {53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 28, 65, 66, 67},
    /* spout */
    {68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83},
    {80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95}
};
/* *INDENT-OFF* */

static float cpdata[][3] =
{
    {0.2, 0, 2.7}, {0.2, -0.112, 2.7}, {0.112, -0.2, 2.7}, {0,-0.2, 2.7},
    {1.3375, 0, 2.53125}, {1.3375, -0.749, 2.53125}, {0.749, -1.3375, 2.53125},
    {0, -1.3375, 2.53125}, {1.4375,0, 2.53125}, {1.4375, -0.805, 2.53125},
    {0.805, -1.4375, 2.53125}, {0, -1.4375, 2.53125}, {1.5, 0, 2.4},
    {1.5, -0.84, 2.4}, {0.84, -1.5, 2.4}, {0, -1.5, 2.4}, {1.75, 0, 1.875},
    {1.75, -0.98, 1.875}, {0.98, -1.75, 1.875}, {0, -1.75, 1.875}, {2, 0, 1.35},
    {2, -1.12, 1.35}, {1.12, -2, 1.35}, {0, -2, 1.35}, {2, 0, 0.9},
    {2, -1.12, 0.9}, {1.12, -2, 0.9}, {0, -2, 0.9}, {-2, 0, 0.9}, {2, 0, 0.45},
    {2, -1.12, 0.45}, {1.12, -2, 0.45}, {0, -2, 0.45}, {1.5, 0, 0.225},
    {1.5, -0.84, 0.225}, {0.84, -1.5, 0.225}, {0, -1.5, 0.225}, {1.5, 0, 0.15},
    {1.5, -0.84, 0.15}, {0.84, -1.5, 0.15}, {0, -1.5, 0.15}, {-1.6, 0, 2.025},
    {-1.6, -0.3, 2.025}, {-1.5,-0.3, 2.25}, {-1.5, 0, 2.25}, {-2.3, 0, 2.025},
    {-2.3, -0.3, 2.025}, {-2.5, -0.3, 2.25}, {-2.5, 0, 2.25}, {-2.7, 0, 2.025},
    {-2.7, -0.3, 2.025}, {-3, -0.3, 2.25}, {-3, 0, 2.25}, {-2.7, 0, 1.8},
    {-2.7, -0.3, 1.8}, {-3, -0.3, 1.8}, {-3, 0, 1.8}, {-2.7, 0, 1.575},
    {-2.7, -0.3, 1.575}, {-3, -0.3, 1.35}, {-3, 0, 1.35}, {-2.5, 0, 1.125},
    {-2.5, -0.3, 1.125}, {-2.65, -0.3, 0.9375}, {-2.65, 0, 0.9375},
    {-2, -0.3, 0.9}, {-1.9, -0.3, 0.6}, {-1.9, 0, 0.6}, {1.7, 0, 1.425},
    {1.7, -0.66, 1.425}, {1.7, -0.66, 0.6}, {1.7, 0, 0.6}, {2.6, 0, 1.425},
    {2.6, -0.66, 1.425}, {3.1, -0.66, 0.825}, {3.1, 0, 0.825}, {2.3, 0, 2.1},
    {2.3, -0.25, 2.1}, {2.4, -0.25, 2.025}, {2.4, 0, 2.025}, {2.7, 0, 2.4},
    {2.7, -0.25, 2.4}, {3.3, -0.25, 2.4}, {3.3, 0, 2.4}, {2.8, 0, 2.475},
    {2.8, -0.25, 2.475}, {3.525, -0.25, 2.49375}, {3.525, 0, 2.49375},
    {2.9, 0, 2.475}, {2.9, -0.15, 2.475}, {3.45, -0.15, 2.5125},
    {3.45, 0, 2.5125}, {2.8, 0, 2.4}, {2.8, -0.15, 2.4}, {3.2, -0.15, 2.4},
    {3.2, 0, 2.4}, {0, 0, 3.15}, {0.8, 0, 3.15}, {0.8, -0.45, 3.15},
    {0.45, -0.8, 3.15}, {0, -0.8, 3.15}, {0, 0, 2.85}, {1.4, 0, 2.4},
    {1.4, -0.784, 2.4}, {0.784, -1.4, 2.4}, {0, -1.4, 2.4}, {0.4, 0, 2.55},
    {0.4, -0.224, 2.55}, {0.224, -0.4, 2.55}, {0, -0.4, 2.55}, {1.3, 0, 2.55},
    {1.3, -0.728, 2.55}, {0.728, -1.3, 2.55}, {0, -1.3, 2.55}, {1.3, 0, 2.4},
    {1.3, -0.728, 2.4}, {0.728, -1.3, 2.4}, {0, -1.3, 2.4}, {0, 0, 0},
    {1.425, -0.798, 0}, {1.5, 0, 0.075}, {1.425, 0, 0}, {0.798, -1.425, 0},
    {0, -1.5, 0.075}, {0, -1.425, 0}, {1.5, -0.84, 0.075}, {0.84, -1.5, 0.075}
};

static float tex[2][2][2] =
{
    { {0, 0},
	{1, 0}},
    { {0, 1},
	{1, 1}}
};

/* *INDENT-ON* */

    static void
teapot(GLint grid, GLenum type)
{
    float p[4][4][3], q[4][4][3], r[4][4][3], s[4][4][3];
    long i, j, k, l;

    glPushAttrib(GL_ENABLE_BIT | GL_EVAL_BIT);
    glEnable(GL_AUTO_NORMAL);
    glEnable(GL_NORMALIZE);
    glEnable(GL_MAP2_VERTEX_3);
    glEnable(GL_MAP2_TEXTURE_COORD_2);
    for (i = 0; i < 10; i++) {
	for (j = 0; j < 4; j++) {
	    for (k = 0; k < 4; k++) {
		for (l = 0; l < 3; l++) {
		    p[j][k][l] = cpdata[patchdata[i][j * 4 + k]][l];
		    q[j][k][l] = cpdata[patchdata[i][j * 4 + (3 - k)]][l];
		    if (l == 1)
			q[j][k][l] *= -1.0;
		    if (i < 6) {
			r[j][k][l] =
			    cpdata[patchdata[i][j * 4 + (3 - k)]][l];
			if (l == 0)
			    r[j][k][l] *= -1.0;
			s[j][k][l] = cpdata[patchdata[i][j * 4 + k]][l];
			if (l == 0)
			    s[j][k][l] *= -1.0;
			if (l == 1)
			    s[j][k][l] *= -1.0;
		    }
		}
	    }
	}
	glMap2f(GL_MAP2_TEXTURE_COORD_2, 0, 1, 2, 2, 0, 1, 4, 2,
		&tex[0][0][0]);
	glMap2f(GL_MAP2_VERTEX_3, 0, 1, 3, 4, 0, 1, 12, 4,
		&p[0][0][0]);
	glMapGrid2f(grid, 0.0, 1.0, grid, 0.0, 1.0);
	glEvalMesh2(type, 0, grid, 0, grid);
	glMap2f(GL_MAP2_VERTEX_3, 0, 1, 3, 4, 0, 1, 12, 4,
		&q[0][0][0]);
	glEvalMesh2(type, 0, grid, 0, grid);
	if (i < 6) {
	    glMap2f(GL_MAP2_VERTEX_3, 0, 1, 3, 4, 0, 1, 12, 4,
		    &r[0][0][0]);
	    glEvalMesh2(type, 0, grid, 0, grid);
	    glMap2f(GL_MAP2_VERTEX_3, 0, 1, 3, 4, 0, 1, 12, 4,
		    &s[0][0][0]);
	    glEvalMesh2(type, 0, grid, 0, grid);
	}
    }
    glPopAttrib();
}


// Now the OSG wrapper for the above OpenGL code, the most complicated bit is computing
// the bounding box for the above example, normally you'll find this the easy bit.
class Teapot : public osg::Drawable
{
    public:
	Teapot() {}

	/** Copy constructor using CopyOp to manage deep vs shallow copy.*/
	Teapot(const Teapot& teapot,const osg::CopyOp& copyop=osg::CopyOp::SHALLOW_COPY):
	    osg::Drawable(teapot,copyop) {}

	META_Object(myTeapotApp,Teapot)


	    // the draw immediate mode method is where the OSG wraps up the drawing of
	    // of OpenGL primitives.
	    virtual void drawImplementation(osg::RenderInfo&) const
	    {
		// teapot(..) doesn't use vertex arrays at all so we don't need to toggle their state
		// if we did we'd need to do something like the following call
		// state.disableAllVertexArrays(), see src/osg/Geometry.cpp for the low down.

		// just call the OpenGL code.
		teapot(14,GL_FILL);
	    }


	// we need to set up the bounding box of the data too, so that the scene graph knows where this
	// object is, for both positioning the camera at start up, and most importantly for culling.
	virtual osg::BoundingBox computeBound() const
	{
	    osg::BoundingBox bbox;

	    // following is some truly horrible code required to calculate the
	    // bounding box of the teapot.  Have used the original code above to do
	    // help compute it.
	    float p[4][4][3], q[4][4][3], r[4][4][3], s[4][4][3];
	    long i, j, k, l;

	    for (i = 0; i < 10; i++) {
		for (j = 0; j < 4; j++) {
		    for (k = 0; k < 4; k++) {

			for (l = 0; l < 3; l++) {
			    p[j][k][l] = cpdata[patchdata[i][j * 4 + k]][l];
			    q[j][k][l] = cpdata[patchdata[i][j * 4 + (3 - k)]][l];
			    if (l == 1)
				q[j][k][l] *= -1.0;

			    if (i < 6) {
				r[j][k][l] =
				    cpdata[patchdata[i][j * 4 + (3 - k)]][l];
				if (l == 0)
				    r[j][k][l] *= -1.0;
				s[j][k][l] = cpdata[patchdata[i][j * 4 + k]][l];
				if (l == 0)
				    s[j][k][l] *= -1.0;
				if (l == 1)
				    s[j][k][l] *= -1.0;
			    }
			}

			bbox.expandBy(osg::Vec3(p[j][k][0],p[j][k][1],p[j][k][2]));
			bbox.expandBy(osg::Vec3(q[j][k][0],q[j][k][1],q[j][k][2]));

			if (i < 6)
			{
			    bbox.expandBy(osg::Vec3(r[j][k][0],r[j][k][1],r[j][k][2]));
			    bbox.expandBy(osg::Vec3(s[j][k][0],s[j][k][1],s[j][k][2]));
			}


		    }
		}
	    }

	    return bbox;
	}

    protected:

	virtual ~Teapot() {}

};


osg::Geode* createTeapot()
{
    osg::Geode* geode = new osg::Geode();

    // add the teapot to the geode.
    geode->addDrawable( new Teapot );

    // add a reflection map to the teapot.
    osg::Image* image = osgDB::readImageFile("Images/reflect.rgb");
    if (image)
    {
	osg::Texture2D* texture = new osg::Texture2D;
	texture->setImage(image);

	osg::TexGen* texgen = new osg::TexGen;
	texgen->setMode(osg::TexGen::SPHERE_MAP);

	osg::StateSet* stateset = new osg::StateSet;
	stateset->setTextureAttributeAndModes(0,texture,osg::StateAttribute::ON);
	stateset->setTextureAttributeAndModes(0,texgen,osg::StateAttribute::ON);

	geode->setStateSet(stateset);    osg::Quat newrot;

    }

    return geode;
}


    void
dm_osgInit(struct dm *dmp)
{
    struct osg_stuff *osp;
    Window win;

    assert(dmp);
    assert(!dmp->dm_osgData);

    win = ((struct dm_xvars *)(dmp->dm_vars.pub_vars))->win;

    //create our graphics context directly so we can pass our own window
    osg::ref_ptr<osg::GraphicsContext::Traits> traits = new osg::GraphicsContext::Traits;

    // Init the Windata Variable that holds the handle for the Window to display OSG in.
    osg::ref_ptr<osg::Referenced> windata = new osgViewer::GraphicsWindowX11::WindowData(win);

    // Setup the traits parameters
    traits->x = 0;
    traits->y = 0;
    traits->width = dmp->dm_width;
    traits->height = dmp->dm_height;
    traits->depth = 24;
    //traits->alpha = 8;
    //traits->stencil = 8;
    traits->windowDecoration = false;
    traits->doubleBuffer = true;
    traits->sharedContext = 0;
    traits->setInheritedWindowPixelFormat = true;
    //traits->windowName = "osgViewer";

    traits->inheritedWindowData = windata;

    // Create the Graphics Context
    osg::ref_ptr<osg::GraphicsContext> graphicsContext = osg::GraphicsContext::createGraphicsContext(traits.get());

    //osgViewer::Viewer *viewer = new osgViewer::Viewer();
    //osp->viewer = viewer;
    osp = (struct osg_stuff *)bu_calloc(sizeof(struct osg_stuff), 1, "osg_stuff");
    osp->viewer = new osgViewer::Viewer();
    //osp->viewer->setThreadingModel(osgViewer::Viewer::SingleThreaded);

    if (graphicsContext) {
	//osp->viewer->setUpViewerAsEmbeddedInWindow(0, 0, dmp->dm_width, dmp->dm_height);

	osp->viewer->getCamera()->setGraphicsContext(graphicsContext);
	osp->viewer->getCamera()->setViewport(new osg::Viewport(0, 0, dmp->dm_width, dmp->dm_height));
	//osp->viewer->getCamera()->setProjectionMatrixAsOrtho(-1.0, 1.0, -1.0, 1.0, 0.0, 2.0);
	osp->viewer->getCamera()->setClearColor(osg::Vec4(1.0f, 1.0f, 1.0f, 0.0f));
	//osp->viewer->getCamera()->setClearColor(osg::Vec4(0.0f, 0.0f, 0.0f, 0.0f));
    }

    osp->viewer->setCameraManipulator( new osgGA::TrackballManipulator() );
    osp->viewer->getCamera()->setAllowEventFocus(false);
    osp->viewer->getCamera()->getProjectionMatrixAsFrustum(osp->left, osp->right, osp->bottom, osp->top, osp->near, osp->far);
    osp->prev_pflag = dmp->dm_perspective;

    // Default values are as follows:
    //
    // left - -0.325000, right - 0.325000, bottom - -0.260000, top - 0.260000, near - 1.000000, far - 10000.000000
    //

    //osp->viewer->setCameraManipulator( new osgGA::TrackballManipulator() );
    //osp->viewer->setCameraManipulator( new osgGA::OrbitManipulator() );
    //osp->viewer->setCameraManipulator( new osgGA::FirstPersonManipulator() );
    //osp->viewer->setCameraManipulator( new osgGA::TerrainManipulator() );
    //osp->viewer->setCameraManipulator( new osgGA::SphericalManipulator() );
    //osp->viewer->setCameraManipulator( new osgGA::FlightManipulator() );

    bu_log("max frame rate - %lf\n", osp->viewer->getRunMaxFrameRate());

    dmp->dm_osgData = (void *)osp;
    bu_log("dm_osgInit: leave\n");
}


    void
dm_osgReshape(struct dm *dmp)
{
    struct osg_stuff *osp;

    bu_log("dm_osgReshape: enter\n");
    assert(dmp);

    if (!dmp->dm_osgData) {
	bu_log("dm_osgReshape: leave early\n");
	return;
    }

    osp = (struct osg_stuff *)dmp->dm_osgData;
    //osp->viewer->getCamera()->setViewport(new osg::Viewport(0, 0, dmp->dm_width, dmp->dm_height));

    osp->viewer->getEventQueue()->windowResize(0, 0, dmp->dm_width, dmp->dm_height);
    osp->viewer->getCamera()->getGraphicsContext()->resized(0, 0, dmp->dm_width, dmp->dm_height);

    bu_log("dm_osgReshape: leave\n");
}


    void
dm_osgPaint(struct dm *dmp)
{
    struct osg_stuff *osp;

    assert(dmp);
    assert(dmp->dm_osgData);

    if (dmp->dm_perspective == 0)
	return;

    osp = (struct osg_stuff *)dmp->dm_osgData;
    osp->viewer->frame();
}


    void
dm_osgLoadMatrix(struct dm *dmp, matp_t mp)
{
    struct osg_stuff *osp;

    assert(dmp);
    assert(dmp->dm_osgData);

    if (dmp->dm_perspective == 0)
	return;

    osp = (struct osg_stuff *)dmp->dm_osgData;
    osgGA::TrackballManipulator *tbmp = (dynamic_cast<osgGA::TrackballManipulator *>(osp->viewer->getCameraManipulator()));
    quat_t quat;
    osg::Quat rot;
    osg::Vec3d old_center;
    osg::Vec3d center;
    mat_t brl_rot;
    mat_t brl_invrot;
    mat_t brl_center;

    // Set the view rotation
    quat_mat2quat(quat, mp);
    rot.set(quat[X], quat[Y], quat[Z], quat[W]);
    tbmp->setRotation(rot);

    // Set the view center by first extracting the
    // view center from mp by backing out the rotation.
    quat_quat2mat(brl_rot, quat);
    bn_mat_inv(brl_invrot, brl_rot);
    bn_mat_mul(brl_center, brl_invrot, mp);
    center.set(-brl_center[MDX], -brl_center[MDY], -brl_center[MDZ]);
    old_center = tbmp->getCenter();
    tbmp->setCenter(center);

    //    bu_log("dm_osgLoadMatrix: mp[MSA] - %lf\n", mp[MSA]);

    // Set the distance from eye to center
    //XXX the current use of dm_perspective is only temporary. At the moment, if dm_perspective is non zero
    //    a perspective matrix from brlcad is passed in. We don't want this.
    if (dmp->dm_perspective == 0) {
	tbmp->setDistance(mp[MSA]);

	if (osp->prev_pflag != dmp->dm_perspective) {
	    bu_log("Setting projection as frustum\n");
	    osp->viewer->getCamera()->setProjectionMatrixAsFrustum(osp->left, osp->right, osp->bottom, osp->top, osp->near, osp->far);
	}

	osp->prev_pflag = dmp->dm_perspective;
    } else {
	fastf_t sa;

	if (mp[MSA] < 0)
	    sa = -mp[MSA];
	else
	    sa = mp[MSA];

	osp->viewer->getCamera()->setProjectionMatrixAsOrtho(-mp[MSA], mp[MSA], -mp[MSA], mp[MSA], 0.0, 2.0);
	osp->prev_pflag = dmp->dm_perspective;
    }
}



/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
