/*                       I F _ O S G . C P P
 * BRL-CAD
 *
 * Copyright (c) 1989-2014 United States Government as represented by
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
/** @addtogroup if */
/** @{ */
/** @file if_osg.cpp
 *
 * A OpenSceneGraph Frame Buffer.
 *
 */
/** @} */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>

extern "C" {
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "fb_private.h"
#include "fb/fb_platform_specific.h"
#include "fb.h"
}

#include <osg/GraphicsContext>
#include <osgViewer/Viewer>
#include <osgViewer/ViewerEventHandlers>

#include <osgGA/StateSetManipulator>

#include <osg/ImageUtils>
#include <osg/TextureRectangle>
#include <osg/Geode>
#include <osg/Geometry>
#include <osg/StateSet>
#include <osg/Timer>

#include <iostream>


/* The following is a customization of StandardManipulator for Framebuffer applications */

/* -*-c++-*- OpenSceneGraph - Copyright (C) 1998-2010 Robert Osfield
 *
 * This library is open source and may be redistributed and/or modified under
 * the terms of the OpenSceneGraph Public License (OSGPL) version 0.0 or
 * (at your option) any later version.  The full license is in LICENSE file
 * included with this distribution, and on the openscenegraph.org website.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * OpenSceneGraph Public License for more details.
 *
 * OrbitManipulator code Copyright (C) 2010 PCJohn (Jan Peciva)
 * while some pieces of code were taken from OSG.
 * Thanks to company Cadwork (www.cadwork.ch) and
 * Brno University of Technology (www.fit.vutbr.cz) for open-sourcing this work.
*/

#include <osgGA/StandardManipulator>

using namespace osg;
using namespace osgGA;

namespace osgGA {


/** FrameBufferManipulator is base class for interacting with a framebuffer .*/
class OSGGA_EXPORT FrameBufferManipulator : public StandardManipulator
{
        typedef StandardManipulator inherited;

    public:

        FrameBufferManipulator( int flags = DEFAULT_SETTINGS );
        FrameBufferManipulator( const FrameBufferManipulator& om,
                          const osg::CopyOp& copyOp = osg::CopyOp::SHALLOW_COPY );

        META_Object( osgGA, FrameBufferManipulator );

        virtual void setByMatrix( const osg::Matrixd& matrix );
        virtual void setByInverseMatrix( const osg::Matrixd& matrix );
        virtual osg::Matrixd getMatrix() const;
        virtual osg::Matrixd getInverseMatrix() const;

        virtual void setTransformation( const osg::Vec3d& eye, const osg::Quat& rotation );
        virtual void setTransformation( const osg::Vec3d& eye, const osg::Vec3d& center, const osg::Vec3d& up );
        virtual void getTransformation( osg::Vec3d& eye, osg::Quat& rotation ) const;
        virtual void getTransformation( osg::Vec3d& eye, osg::Vec3d& center, osg::Vec3d& up ) const;

        virtual osgUtil::SceneView::FusionDistanceMode getFusionDistanceMode() const;
        virtual float getFusionDistanceValue() const;

    protected:

        osg::Vec3d _center;
        osg::Quat  _rotation;
        double     _distance;

        double _minimumDistance;
        static int _minimumDistanceFlagIndex;
};


int FrameBufferManipulator::_minimumDistanceFlagIndex = allocateRelativeFlag();


/// Constructor.
FrameBufferManipulator::FrameBufferManipulator( int flags )
   : inherited( flags ), _distance( 1. )
{
}


/// Constructor.
FrameBufferManipulator::FrameBufferManipulator( const FrameBufferManipulator& om, const CopyOp& copyOp )
   : osg::Object(om, copyOp), inherited( om, copyOp ), _center( om._center ),
     _rotation( om._rotation ), _distance( om._distance ), _minimumDistance( om._minimumDistance )
{
}


/** Set the position of the manipulator using a 4x4 matrix.*/
void FrameBufferManipulator::setByMatrix( const osg::Matrixd& matrix )
{
    _center = osg::Vec3d( 0., 0., -_distance ) * matrix;
    _rotation = matrix.getRotate();

    // fix current rotation
    if( getVerticalAxisFixed() )
        fixVerticalAxis( _center, _rotation, true );
}


/** Set the position of the manipulator using a 4x4 matrix.*/
void FrameBufferManipulator::setByInverseMatrix( const osg::Matrixd& matrix )
{
    setByMatrix( osg::Matrixd::inverse( matrix ) );
}


/** Get the position of the manipulator as 4x4 matrix.*/
osg::Matrixd FrameBufferManipulator::getMatrix() const
{
    return osg::Matrixd::translate( 0., 0., _distance ) *
           osg::Matrixd::rotate( _rotation ) *
           osg::Matrixd::translate( _center );
}


/** Get the position of the manipulator as a inverse matrix of the manipulator,
    typically used as a model view matrix.*/
osg::Matrixd FrameBufferManipulator::getInverseMatrix() const
{
    return osg::Matrixd::translate( -_center ) *
           osg::Matrixd::rotate( _rotation.inverse() ) *
           osg::Matrixd::translate( 0.0, 0.0, -_distance );
}


// doc in parent
void FrameBufferManipulator::setTransformation( const osg::Vec3d& eye, const osg::Quat& rotation )
{
    _center = eye + rotation * osg::Vec3d( 0., 0., -_distance );
    _rotation = rotation;

    // fix current rotation
    if( getVerticalAxisFixed() )
        fixVerticalAxis( _center, _rotation, true );
}


// doc in parent
void FrameBufferManipulator::getTransformation( osg::Vec3d& eye, osg::Quat& rotation ) const
{
    eye = _center - _rotation * osg::Vec3d( 0., 0., -_distance );
    rotation = _rotation;
}


// doc in parent
void FrameBufferManipulator::setTransformation( const osg::Vec3d& eye, const osg::Vec3d& center, const osg::Vec3d& up )
{
    Vec3d lv( center - eye );

    Vec3d f( lv );
    f.normalize();
    Vec3d s( f^up );
    s.normalize();
    Vec3d u( s^f );
    u.normalize();

    osg::Matrixd rotation_matrix( s[0], u[0], -f[0], 0.0f,
                            s[1], u[1], -f[1], 0.0f,
                            s[2], u[2], -f[2], 0.0f,
                            0.0f, 0.0f,  0.0f, 1.0f );

    _center = center;
    _distance = lv.length();
    _rotation = rotation_matrix.getRotate().inverse();

    // fix current rotation
    if( getVerticalAxisFixed() )
        fixVerticalAxis( _center, _rotation, true );
}


// doc in parent
void FrameBufferManipulator::getTransformation( osg::Vec3d& eye, osg::Vec3d& center, osg::Vec3d& up ) const
{
    center = _center;
    eye = _center + _rotation * osg::Vec3d( 0., 0., _distance );
    up = _rotation * osg::Vec3d( 0., 1., 0. );
}

/** Get the FusionDistanceMode. Used by SceneView for setting up stereo convergence.*/
osgUtil::SceneView::FusionDistanceMode FrameBufferManipulator::getFusionDistanceMode() const
{
    return osgUtil::SceneView::USE_FUSION_DISTANCE_VALUE;
}

/** Get the FusionDistanceValue. Used by SceneView for setting up stereo convergence.*/
float FrameBufferManipulator::getFusionDistanceValue() const
{
    return _distance;
}

} // end osgGA namespace

/*********************************************************************************************/
/**************************** End FrameBufferManipulator *************************************/
/*********************************************************************************************/

// KeyHandler --
// Allow user to do interesting things with an
// OcclusionQueryNode-enabled scene graph at run time.
class KeyHandler : public osgGA::GUIEventHandler
{
public:
    KeyHandler( osg::Node& node )
      : _node( node ),
        _enable( true ),
        _debug( false )
    {}

    bool handle( const osgGA::GUIEventAdapter& ea, osgGA::GUIActionAdapter& )
    {
        switch( ea.getEventType() )
        {
            case(osgGA::GUIEventAdapter::KEYUP):
            {
                if (ea.getKey()==osgGA::GUIEventAdapter::KEY_F6)
                {
                    // F6 -- Toggle osgOQ testing.
		    std::cout << "F6!";
                    return true;
                }
                else if (ea.getKey()==osgGA::GUIEventAdapter::KEY_F7)
                {
                    // F7 -- Toggle display of OQ test bounding volumes
		    std::cout << "F7!";
                    return true;
                }
                else if (ea.getKey()==osgGA::GUIEventAdapter::KEY_F8)
                {
                    // F8 -- Gether stats and display
		    std::cout << "F8!";
                    return true;
                }
                else if (ea.getKey()==osgGA::GUIEventAdapter::KEY_F9)
                {
                    // F9 -- Remove all OcclusionQueryNodes
		    std::cout << "F9!";
                    return true;
                }
                else if (ea.getKey()=='o')
                {
		    std::cout << "o!";
                    return true;
                }
                return false;
            }
            case(osgGA::GUIEventAdapter::PUSH):
            {
		std::cout << "PUSH!\n";
		return false;
	    }
            case(osgGA::GUIEventAdapter::RELEASE):
            {
		std::cout << "RELEASE!\n";
		return false;
	    }
            case(osgGA::GUIEventAdapter::DOUBLECLICK):
            {
		std::cout << "DOUBLECLICK!\n";
		return false;
	    }
            case(osgGA::GUIEventAdapter::DRAG):
            {
		std::cout << "DRAG!\n";
		return false;
	    }
            case(osgGA::GUIEventAdapter::MOVE):
            {
		std::cout << "MOVE!\n";
		return false;
	    }

            default:
                return false;
                break;
        }
        return false;
    }

    osg::Node& _node;

    bool _enable, _debug;
};
/*********************************************************************************************/
/*********************************** End KeyHandler ******************************************/
/*********************************************************************************************/


struct osginfo {
    osgViewer::Viewer *viewer;
    osg::Image *image;
    osg::TextureRectangle *texture;
    osg::Geometry *pictureQuad;
    osg::Timer *timer;
    int last_update_time;
};

#define OSG(ptr) ((struct osginfo *)((ptr)->u6.p))


HIDDEN int
osg_open(fb *ifp, const char *UNUSED(file), int width, int height)
{
    FB_CK_FB(ifp);

    /* Get some memory for the osg specific stuff */
    if ((ifp->u6.p = (char *)calloc(1, sizeof(struct osginfo))) == NULL) {
	fb_log("fb_osg_open:  osginfo malloc failed\n");
	return -1;
    }

    if (width > 0)
	ifp->if_width = width;
    if (height > 0)
	ifp->if_height = height;

    OSG(ifp)->timer = new osg::Timer;
    OSG(ifp)->last_update_time = 0;

    OSG(ifp)->viewer = new osgViewer::Viewer();
    OSG(ifp)->viewer->setUpViewInWindow(0, 0, width, height);

    OSG(ifp)->image = new osg::Image;
    OSG(ifp)->image->allocateImage(width, height, 1, GL_RGB, GL_UNSIGNED_BYTE);
    OSG(ifp)->image->setPixelBufferObject(new osg::PixelBufferObject(OSG(ifp)->image));
    OSG(ifp)->pictureQuad = osg::createTexturedQuadGeometry(osg::Vec3(0.0f,0.0f,0.0f),
	    osg::Vec3(ifp->if_width,0.0f,0.0f), osg::Vec3(0.0f,0.0f, ifp->if_height), 0.0f, 0.0, OSG(ifp)->image->s(), OSG(ifp)->image->t());
    OSG(ifp)->texture = new osg::TextureRectangle(OSG(ifp)->image);
    OSG(ifp)->texture->setFilter(osg::Texture::MIN_FILTER,osg::Texture::LINEAR);
    OSG(ifp)->texture->setFilter(osg::Texture::MAG_FILTER,osg::Texture::LINEAR);
    OSG(ifp)->texture->setWrap(osg::Texture::WRAP_R,osg::Texture::REPEAT);
    OSG(ifp)->pictureQuad->getOrCreateStateSet()->setTextureAttributeAndModes(0, OSG(ifp)->texture, osg::StateAttribute::ON);


    osg::Geode *geode = new osg::Geode;
    osg::StateSet* stateset = geode->getOrCreateStateSet();
    stateset->setMode(GL_LIGHTING,osg::StateAttribute::OFF);
    geode->addDrawable(OSG(ifp)->pictureQuad);

    OSG(ifp)->viewer->getCamera()->setViewMatrix(osg::Matrix::identity());
    osg::Vec3 topleft(0.0f, 0.0f, 0.0f);
    osg::Vec3 bottomright(width, height, 0.0f);
    OSG(ifp)->viewer->getCamera()->setProjectionMatrixAsOrtho2D(-width/2,width/2,-height/2, height/2);
    OSG(ifp)->viewer->getCamera()->setClearColor(osg::Vec4(0.0f,0.0f,0.0f,1.0f));

    OSG(ifp)->viewer->setSceneData(geode);

    OSG(ifp)->viewer->setCameraManipulator( new osgGA::FrameBufferManipulator() );
    OSG(ifp)->viewer->addEventHandler(new osgGA::StateSetManipulator(OSG(ifp)->viewer->getCamera()->getOrCreateStateSet()));
    OSG(ifp)->viewer->addEventHandler(new KeyHandler(*geode));

    OSG(ifp)->viewer->realize();

    OSG(ifp)->timer->setStartTick();

    return 0;
}

HIDDEN int
osg_close(fb *ifp)
{
    FB_CK_FB(ifp);

    return (*OSG(ifp)->viewer).ViewerBase::run();
}


HIDDEN int
osg_clear(fb *ifp, unsigned char *UNUSED(pp))
{
    FB_CK_FB(ifp);

    return 0;
}


HIDDEN ssize_t
osg_read(fb *ifp, int UNUSED(x), int UNUSED(y), unsigned char *UNUSED(pixelp), size_t count)
{
    FB_CK_FB(ifp);

    return count;
}


HIDDEN ssize_t
osg_write(fb *ifp, int xstart, int ystart, const unsigned char *pixelp, size_t count)
{
    register int x;
    register int y;
    size_t scan_count;  /* # pix on this scanline */
    size_t pix_count;   /* # pixels to send */
    ssize_t ret;

    //fb_log("write got called!");

    FB_CK_FB(ifp);

    /* fast exit cases */
    pix_count = count;
    if (pix_count == 0)
	return 0;       /* OK, no pixels transferred */

    x = xstart;
    y = ystart;

    if (x < 0 || x >= ifp->if_width ||
	    y < 0 || y >= ifp->if_height)
	return -1;

    ret = 0;

    while (pix_count) {
	void *scanline;

	if (y >= ifp->if_height)
	    break;

	if (pix_count >= (size_t)(ifp->if_width-x))
	    scan_count = (size_t)(ifp->if_width-x);
	else
	    scan_count = pix_count;

	scanline = (void *)(OSG(ifp)->image->data(0,y,0));

	memcpy(scanline, pixelp, scan_count*3);

	ret += scan_count;
	pix_count -= scan_count;
	x = 0;
	if (++y >= ifp->if_height)
	    break;
    }

    OSG(ifp)->image->dirty();
    if (OSG(ifp)->timer->time_m() - OSG(ifp)->last_update_time > 10) {
	OSG(ifp)->viewer->frame();
	OSG(ifp)->last_update_time = OSG(ifp)->timer->time_m();
    }
    return ret;
}


HIDDEN int
osg_rmap(fb *ifp, ColorMap *UNUSED(cmp))
{
    FB_CK_FB(ifp);

    return 0;
}


HIDDEN int
osg_wmap(fb *ifp, const ColorMap *UNUSED(cmp))
{
    FB_CK_FB(ifp);

    fb_log("wmap got called!");

    return 0;
}


HIDDEN int
osg_view(fb *ifp, int UNUSED(xcenter), int UNUSED(ycenter), int UNUSED(xzoom), int UNUSED(yzoom))
{
    FB_CK_FB(ifp);
    fb_log("view was called!\n");

    /*fb_sim_view(ifp, xcenter, ycenter, xzoom, yzoom);*/
    return 0;
}


HIDDEN int
osg_getview(fb *ifp, int *UNUSED(xcenter), int *UNUSED(ycenter), int *UNUSED(xzoom), int *UNUSED(yzoom))
{
    FB_CK_FB(ifp);

    /*fb_sim_getview(ifp, xcenter, ycenter, xzoom, yzoom);*/
    return 0;
}


HIDDEN int
osg_setcursor(fb *ifp, const unsigned char *UNUSED(bits), int UNUSED(xbits), int UNUSED(ybits), int UNUSED(xorig), int UNUSED(yorig))
{
    FB_CK_FB(ifp);

    return 0;
}


HIDDEN int
osg_cursor(fb *ifp, int UNUSED(mode), int UNUSED(x), int UNUSED(y))
{
    FB_CK_FB(ifp);

    /*fb_sim_cursor(ifp, mode, x, y);*/
    return 0;
}


HIDDEN int
osg_getcursor(fb *ifp, int *UNUSED(mode), int *UNUSED(x), int *UNUSED(y))
{
    FB_CK_FB(ifp);

    /*fb_sim_getcursor(ifp, mode, x, y);*/
    return 0;
}


HIDDEN int
osg_readrect(fb *ifp, int UNUSED(xmin), int UNUSED(ymin), int width, int height, unsigned char *UNUSED(pp))
{
    FB_CK_FB(ifp);

    return width*height;
}


HIDDEN int
osg_writerect(fb *ifp, int UNUSED(xmin), int UNUSED(ymin), int width, int height, const unsigned char *UNUSED(pp))
{
    FB_CK_FB(ifp);

    fb_log("writerect got called!");

    return width*height;
}


HIDDEN int
osg_poll(fb *ifp)
{
    FB_CK_FB(ifp);

    return 0;
}


HIDDEN int
osg_flush(fb *ifp)
{
    FB_CK_FB(ifp);

    fb_log("flush was called!\n");

    return 0;
}


HIDDEN int
osg_free(fb *ifp)
{
    FB_CK_FB(ifp);

    return 0;
}


HIDDEN int
osg_help(fb *ifp)
{
    FB_CK_FB(ifp);

    fb_log("Description: %s\n", osg_interface.if_type);
    fb_log("Device: %s\n", ifp->if_name);
    fb_log("Max width/height: %d %d\n",
	   osg_interface.if_max_width,
	   osg_interface.if_max_height);
    fb_log("Default width/height: %d %d\n",
	   osg_interface.if_width,
	   osg_interface.if_height);
    fb_log("Useful for Benchmarking/Debugging\n");
    return 0;
}

/* Functions for pre-exising windows */
extern "C" int
osg_close_existing(fb *ifp)
{
    FB_CK_FB(ifp);
    return 0;
}

extern "C" struct fb_platform_specific *
osg_get_fbps(uint32_t magic)
{
    struct fb_platform_specific *fb_ps = NULL;
    //struct osg_fb_info *data = NULL;
    BU_GET(fb_ps, struct fb_platform_specific);
    //BU_GET(data, struct osg_fb_info);
    fb_ps->magic = magic;
    //fb_ps->data = data;
    return fb_ps;
}


extern "C" void
osg_put_fbps(struct fb_platform_specific *fbps)
{
    BU_CKMAG(fbps, FB_OSG_MAGIC, "osg framebuffer");
    //BU_PUT(fbps->data, struct osg_fb_info);
    BU_PUT(fbps, struct fb_platform_specific);
    return;
}


extern "C" int
osg_open_existing(fb *ifp, int UNUSED(w), int UNUSED(h), struct fb_platform_specific *UNUSED(fb_ps))
{
    FB_CK_FB(ifp);
    return 0;
}

extern "C" int
osg_refresh(fb *ifp, int x, int y, int w, int h){
    FB_CK_FB(ifp);
    if (!x || !y || !w || !h) return -1;

    fb_log("refresh got called!\n");
    return 0;
}

extern "C" int 
osg_configureWindow(fb *ifp, int width, int height)
{
    FB_CK_FB(ifp);

    if (!width || !height) return -1;
    return 0;
}

/* This is the ONLY thing that we normally "export" */
fb osg_interface =  {
    0,
    osg_open,		/* device_open */
    osg_open_existing,  /* device_open_existing */
    osg_close_existing,  /* device_close_existing */
    osg_get_fbps,
    osg_put_fbps,
    osg_close,		/* device_close */
    osg_clear,		/* device_clear */
    osg_read,		/* buffer_read */
    osg_write,		/* buffer_write */
    osg_rmap,		/* colormap_read */
    osg_wmap,		/* colormap_write */
    osg_view,		/* set view */
    osg_getview,	/* get view */
    osg_setcursor,	/* define cursor */
    osg_cursor,		/* set cursor */
    osg_getcursor,	/* get cursor */
    osg_readrect,	/* rectangle read */
    osg_writerect,	/* rectangle write */
    osg_readrect,	/* bw rectangle read */
    osg_writerect,	/* bw rectangle write */
    osg_configureWindow,
    osg_refresh,
    osg_poll,		/* handle events */
    osg_flush,		/* flush output */
    osg_free,		/* free resources */
    osg_help,		/* help message */
    bu_strdup("OpenSceneGraph"),	/* device description */
    32*1024,		/* max width */
    32*1024,		/* max height */
    bu_strdup("/dev/osg"),		/* short device name */
    512,		/* default/current width */
    512,		/* default/current height */
    -1,			/* select fd */
    -1,			/* file descriptor */
    1, 1,		/* zoom */
    256, 256,		/* window center */
    0, 0, 0,		/* cursor */
    PIXEL_NULL,		/* page_base */
    PIXEL_NULL,		/* page_curp */
    PIXEL_NULL,		/* page_endp */
    -1,			/* page_no */
    0,			/* page_dirty */
    0L,			/* page_curpos */
    0L,			/* page_pixels */
    0,			/* debug */
    {0}, /* u1 */
    {0}, /* u2 */
    {0}, /* u3 */
    {0}, /* u4 */
    {0}, /* u5 */
    {0}  /* u6 */
};


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
