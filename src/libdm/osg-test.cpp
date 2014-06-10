/*                    O S G - T E S T . C P P
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
/** @file osg-test.cpp
 *
 *  BRL-CAD test application for OpenSceneGraph integration, based off of
 *  osghud example from OpenSceneGraph, which has the following license:
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 *  THE SOFTWARE.
 */

#include "common.h"
#include "bu/log.h"

#if 0
#include <osgUtil/Optimizer>
#include <osgDB/ReadFile>

#include <osgViewer/Viewer>
#include <osgViewer/CompositeViewer>

#include <osgGA/TrackballManipulator>

#include <osg/Material>
#include <osg/Geode>
#include <osg/BlendFunc>
#include <osg/Depth>
#include <osg/PolygonOffset>
#include <osg/MatrixTransform>
#include <osg/Camera>
#include <osg/RenderInfo>

#include <osgDB/WriteFile>

#include <osgText/Text>


osg::Camera* createHUD()
{
    // create a camera to set up the projection and model view matrices, and the subgraph to draw in the HUD
    osg::Camera* camera = new osg::Camera;

    // set the projection matrix
    camera->setProjectionMatrix(osg::Matrix::ortho2D(0,1280,0,1024));

    // set the view matrix
    camera->setReferenceFrame(osg::Transform::ABSOLUTE_RF);
    camera->setViewMatrix(osg::Matrix::identity());

    // only clear the depth buffer
    camera->setClearMask(GL_DEPTH_BUFFER_BIT);

    // draw subgraph after main camera view.
    camera->setRenderOrder(osg::Camera::POST_RENDER);

    // we don't want the camera to grab event focus from the viewers main camera(s).
    camera->setAllowEventFocus(false);



    // add to this camera a subgraph to render
    {

	osg::Geode* geode = new osg::Geode();

	std::string timesFont("fonts/arial.ttf");

	// turn lighting off for the text and disable depth test to ensure it's always ontop.
	osg::StateSet* stateset = geode->getOrCreateStateSet();
	stateset->setMode(GL_LIGHTING,osg::StateAttribute::OFF);

	osg::Vec3 position(150.0f,800.0f,0.0f);
	osg::Vec3 delta(0.0f,-120.0f,0.0f);

	{
	    osgText::Text* text = new  osgText::Text;
	    geode->addDrawable( text );

	    text->setFont(timesFont);
	    text->setPosition(position);
	    text->setText("Head Up Displays are simple :-)");

	    position += delta;
	}


	{
	    osgText::Text* text = new  osgText::Text;
	    geode->addDrawable( text );

	    text->setFont(timesFont);
	    text->setPosition(position);
	    text->setText("All you need to do is create your text in a subgraph.");

	    position += delta;
	}


	{
	    osgText::Text* text = new  osgText::Text;
	    geode->addDrawable( text );

	    text->setFont(timesFont);
	    text->setPosition(position);
	    text->setText("Then place an osg::Camera above the subgraph\n"
		    "to create an orthographic projection.\n");

	    position += delta;
	}

	{
	    osgText::Text* text = new  osgText::Text;
	    geode->addDrawable( text );

	    text->setFont(timesFont);
	    text->setPosition(position);
	    text->setText("Set the Camera's ReferenceFrame to ABSOLUTE_RF to ensure\n"
		    "it remains independent from any external model view matrices.");

	    position += delta;
	}

	{
	    osgText::Text* text = new  osgText::Text;
	    geode->addDrawable( text );

	    text->setFont(timesFont);
	    text->setPosition(position);
	    text->setText("And set the Camera's clear mask to just clear the depth buffer.");

	    position += delta;
	}

	{
	    osgText::Text* text = new  osgText::Text;
	    geode->addDrawable( text );

	    text->setFont(timesFont);
	    text->setPosition(position);
	    text->setText("And finally set the Camera's RenderOrder to POST_RENDER\n"
		    "to make sure it's drawn last.");

	    position += delta;
	}


	{
	    osg::BoundingBox bb;
	    for(unsigned int i=0;i<geode->getNumDrawables();++i)
	    {
		bb.expandBy(geode->getDrawable(i)->getBound());
	    }

	    osg::Geometry* geom = new osg::Geometry;

	    osg::Vec3Array* vertices = new osg::Vec3Array;
	    float depth = bb.zMin()-0.1;
	    vertices->push_back(osg::Vec3(bb.xMin(),bb.yMax(),depth));
	    vertices->push_back(osg::Vec3(bb.xMin(),bb.yMin(),depth));
	    vertices->push_back(osg::Vec3(bb.xMax(),bb.yMin(),depth));
	    vertices->push_back(osg::Vec3(bb.xMax(),bb.yMax(),depth));
	    geom->setVertexArray(vertices);

	    osg::Vec3Array* normals = new osg::Vec3Array;
	    normals->push_back(osg::Vec3(0.0f,0.0f,1.0f));
	    geom->setNormalArray(normals, osg::Array::BIND_OVERALL);

	    osg::Vec4Array* colors = new osg::Vec4Array;
	    colors->push_back(osg::Vec4(1.0f,1.0,0.8f,0.2f));
	    geom->setColorArray(colors, osg::Array::BIND_OVERALL);

	    geom->addPrimitiveSet(new osg::DrawArrays(GL_QUADS,0,4));

	    osg::StateSet* stateset = geom->getOrCreateStateSet();
	    stateset->setMode(GL_BLEND,osg::StateAttribute::ON);
	    //stateset->setAttribute(new osg::PolygonOffset(1.0f,1.0f),osg::StateAttribute::ON);
	    stateset->setRenderingHint(osg::StateSet::TRANSPARENT_BIN);

	    geode->addDrawable(geom);
	}

	camera->addChild(geode);
    }

    return camera;
}

struct SnapImage : public osg::Camera::DrawCallback
{
    SnapImage(const std::string& filename):
	_filename(filename),
	_snapImage(false)
    {
	_image = new osg::Image;
    }

    virtual void operator () (osg::RenderInfo& renderInfo) const
    {

	if (!_snapImage) return;

	osg::notify(osg::NOTICE)<<"Camera callback"<<std::endl;

	osg::Camera* camera = renderInfo.getCurrentCamera();
	osg::Viewport* viewport = camera ? camera->getViewport() : 0;

	osg::notify(osg::NOTICE)<<"Camera callback "<<camera<<" "<<viewport<<std::endl;

	if (viewport && _image.valid())
	{
	    _image->readPixels(int(viewport->x()),int(viewport->y()),int(viewport->width()),int(viewport->height()),
		    GL_RGBA,
		    GL_UNSIGNED_BYTE);
	    osgDB::writeImageFile(*_image, _filename);

	    osg::notify(osg::NOTICE)<<"Taken screenshot, and written to '"<<_filename<<"'"<<std::endl;
	}

	_snapImage = false;
    }

    std::string                         _filename;
    mutable bool                        _snapImage;
    mutable osg::ref_ptr<osg::Image>    _image;
};

struct SnapeImageHandler : public osgGA::GUIEventHandler
{

    SnapeImageHandler(int key,SnapImage* si):
	_key(key),
	_snapImage(si) {}

    bool handle(const osgGA::GUIEventAdapter& ea, osgGA::GUIActionAdapter&)
    {
	if (ea.getHandled()) return false;

	switch(ea.getEventType())
	{
	    case(osgGA::GUIEventAdapter::KEYUP):
		{
		    if (ea.getKey() == _key)
		    {
			osg::notify(osg::NOTICE)<<"event handler"<<std::endl;
			_snapImage->_snapImage = true;
			return true;
		    }

		    break;
		}
	    default:
		break;
	}

	return false;
    }

    int                     _key;
    osg::ref_ptr<SnapImage> _snapImage;
};
#endif


int main( int argc, char **argv )
{
#if 0
    struct db_i *dbip = DBI_NULL;
    struct directory *dp = RT_DIR_NULL;
    struct rt_db_internal intern;
    struct bu_list plot_segments;
    const struct bn_tol tol = {BN_TOL_MAGIC, 0.0005, 0.0005 * 0.0005, 1e-6, 1 - 1e-6};
    const struct rt_tess_tol rttol = {RT_TESS_TOL_MAGIC, 0.0, 0.01, 0};
#endif
    if (argc != 3 || !argv) {
	bu_exit(1, "Error - please specify a .g file and an object\n");
    }
#if 0
    if (!bu_file_exists(argv[1], NULL)) {
	bu_exit(1, "Cannot stat file %s\n", argv[1]);
    }

    if ((dbip = db_open(argv[1], DB_OPEN_READONLY)) == DBI_NULL) {
	bu_exit(1, "Cannot open geometry database file %s\n", argv[1]);
    }
    RT_CK_DBI(dbip);
    if (db_dirbuild(dbip) < 0) {
	db_close(dbip);
	bu_exit(1, "db_dirbuild failed on geometry database file %s\n", argv[1]);
    }

    dp = db_lookup(dbip, argv[2], LOOKUP_QUIET);
    if (dp == RT_DIR_NULL) {
	bu_exit(1, "ERROR: Unable to fine object %s in %s\n", argv[2], argv[1]);
    }

    RT_DB_INTERNAL_INIT(&intern);

    if (rt_db_get_internal(intern, dp, dbip, NULL, &rt_uniresource) < 0) {
	bu_exit(1, "ERROR: Unable to get internal representation of %s\n", dp->d_namep);
    }

    /* Get the vlist associated with this particular object */
    BU_LIST_INIT(&plot_segments);
    if (rt_obj_plot(&plot_segments, &intern, &tol, &rttol) < 0) {
	bu_exit(1, "ERROR: Unable to get plot segment list from %s\n", dp->d_namep);
    }

    // construct the viewer.
    osgViewer::Viewer viewer;

    // create a HUD as slave camera attached to the master view.

    viewer.setUpViewAcrossAllScreens();

    osgViewer::Viewer::Windows windows;
    viewer.getWindows(windows);

    if (windows.empty()) return 1;

    osg::Camera* hudCamera = createHUD();

    // set up cameras to render on the first window available.
    hudCamera->setGraphicsContext(windows[0]);
    hudCamera->setViewport(0,0,windows[0]->getTraits()->width, windows[0]->getTraits()->height);

    viewer.addSlave(hudCamera, false);

    // set the scene to render
    viewer.setSceneData(scene.get());

    return viewer.run();
#endif
}
// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
