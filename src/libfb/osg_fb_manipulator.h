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

#include "common.h"

#include <osgGA/StandardManipulator>

using namespace osg;
using namespace osgGA;

namespace osgGA {


/** FrameBufferManipulator is base class for interacting with a framebuffer .*/
class FB_EXPORT FrameBufferManipulator : public StandardManipulator
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

/**************************** End FrameBufferManipulator *************************************/


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
		    //std::cout << "F6!";
                    return true;
                }
                else if (ea.getKey()==osgGA::GUIEventAdapter::KEY_F7)
                {
                    // F7 -- Toggle display of OQ test bounding volumes
		    //std::cout << "F7!";
                    return true;
                }
                else if (ea.getKey()==osgGA::GUIEventAdapter::KEY_F8)
                {
                    // F8 -- Gather stats and display
		    //std::cout << "F8!";
                    return true;
                }
                else if (ea.getKey()==osgGA::GUIEventAdapter::KEY_F9)
                {
                    // F9 -- Remove all OcclusionQueryNodes
		    //std::cout << "F9!";
                    return true;
                }
                else if (ea.getKey()=='o')
                {
		    //std::cout << "o!";
                    return true;
                }
                return false;
            }
            case(osgGA::GUIEventAdapter::PUSH):
            {
		//std::cout << "PUSH! ";
	        if (osgGA::GUIEventAdapter::LEFT_MOUSE_BUTTON & ea.getButton()) {
		    //std::cout << "Left Button\n";
		}
		if (osgGA::GUIEventAdapter::RIGHT_MOUSE_BUTTON & ea.getButton()) {
		    ea.setHandled(true);
		    osgViewer::GraphicsWindow *gw = dynamic_cast<osgViewer::GraphicsWindow*>(OSGL(fbp)->glc);
		    gw->close();
		    break;
		}
		if (osgGA::GUIEventAdapter::MIDDLE_MOUSE_BUTTON & ea.getButton()) {
		    int x = ea.getX();
		    int y = ea.getY();

		    if (x < 0 || y < 0) {
			fb_log("No RGB (outside image viewport)\n");
			break;
		    }
		    int r = fbp->if_mem[3*(y*fbp->if_width + x)];
		    int g = fbp->if_mem[3*(y*fbp->if_width + x)+1];
		    int b = fbp->if_mem[3*(y*fbp->if_width + x)+2];
		    if (r < 0) r = 255 + r + 1;
		    if (g < 0) g = 255 + g + 1;
		    if (b < 0) b = 255 + b + 1;

		    fb_log("At image (%d, %d), real RGB=(%d %d %d)\n", x, y, r, g, b);
		}
		return false;
	    }
            case(osgGA::GUIEventAdapter::RELEASE):
            {
		/*std::cout << "RELEASE! ";
	        if (osgGA::GUIEventAdapter::LEFT_MOUSE_BUTTON & ea.getButton()) std::cout << "Left Button\n";
	        if (osgGA::GUIEventAdapter::MIDDLE_MOUSE_BUTTON & ea.getButton()) std::cout << "Middle Button\n";
	        if (osgGA::GUIEventAdapter::RIGHT_MOUSE_BUTTON & ea.getButton()) std::cout << "Right Button\n";*/
		return false;
	    }
            case(osgGA::GUIEventAdapter::DOUBLECLICK):
            {
		//std::cout << "DOUBLECLICK!\n";
		return false;
	    }
            case(osgGA::GUIEventAdapter::DRAG):
            {
		//std::cout << "DRAG!\n";
		return false;
	    }
            case(osgGA::GUIEventAdapter::MOVE):
            {
		//std::cout << "MOVE!\n";
		return false;
	    }
            case(osgGA::GUIEventAdapter::SCROLL):
            {
		/*std::cout << "SCROLL ";
	        if (osgGA::GUIEventAdapter::SCROLL_UP == ea.getScrollingMotion()) std::cout << "UP!\n";
	        if (osgGA::GUIEventAdapter::SCROLL_DOWN == ea.getScrollingMotion()) std::cout << "DOWN!\n";
	        if (osgGA::GUIEventAdapter::SCROLL_LEFT == ea.getScrollingMotion()) std::cout << "LEFT!\n";
	        if (osgGA::GUIEventAdapter::SCROLL_RIGHT == ea.getScrollingMotion()) std::cout << "RIGHT!\n";*/
		return false;
	    }


            default:
                return false;
                break;
        }
        return false;
    }

    osg::Node& _node;

    fb *fbp;

    bool _enable, _debug;
};
/*********************************** End KeyHandler ******************************************/

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
