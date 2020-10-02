/* -*-c++-*- OpenSceneGraph - Copyright (C) 1998-2006 Robert Osfield
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
*/
#include <string.h>
#include <string>

#include <osg/GL>
#include <osg/LightModel>
#include <osg/Notify>
#include <osg/State>

using namespace osg;


LightModel::LightModel():
          StateAttribute(),
          _ambient(0.2f,0.2f,0.2f,1.0f),
          _colorControl(LightModel::SINGLE_COLOR),
          _localViewer(false),
          _twoSided(false)
{
}


LightModel::~LightModel()
{
}

#ifdef OSG_GL_FIXED_FUNCTION_AVAILABLE

// need to define if gl.h version < 1.2.
#ifndef GL_LIGHT_MODEL_COLOR_CONTROL
#define GL_LIGHT_MODEL_COLOR_CONTROL 0x81F8
#endif

#ifndef GL_SINGLE_COLOR
#define GL_SINGLE_COLOR 0x81F9
#endif

#ifndef GL_SEPARATE_SPECULAR_COLOR
#define GL_SEPARATE_SPECULAR_COLOR 0x81FA
#endif

void LightModel::apply(State& state) const
{

    #ifdef OSG_GLES1_AVAILABLE
    #define glLightModeli glLightModelx
    #endif

    glLightModelfv(GL_LIGHT_MODEL_AMBIENT,_ambient.ptr());

    if (state.get<GLExtensions>()->glVersion>=1.2)
    {
        if (_colorControl==SEPARATE_SPECULAR_COLOR)
        {
            glLightModeli(GL_LIGHT_MODEL_COLOR_CONTROL,GL_SEPARATE_SPECULAR_COLOR);
        }
        else
        {
            glLightModeli(GL_LIGHT_MODEL_COLOR_CONTROL,GL_SINGLE_COLOR);
        }
    }

    #ifndef OSG_GLES1_AVAILABLE
    glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER,_localViewer);
    #endif

    glLightModeli(GL_LIGHT_MODEL_TWO_SIDE,_twoSided);
}

#else

void LightModel::apply(State&) const
{
    OSG_NOTICE<<"Warning: LightModel::apply(State&) - not supported."<<std::endl;
}

#endif

