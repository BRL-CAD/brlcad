// -*-c++-*- osgWidget - Code by: Jeremy Moles (cubicool) 2007-2008
// $Id$

#include <osgWidget/Version>
#include <osg/Version>

extern "C" {

const char* osgWidgetGetVersion()
{
    return osgGetVersion();
}

const char* osgWidgetGetLibraryName()
{
    return "OpenSceneGraph Widget Library";
}

}
