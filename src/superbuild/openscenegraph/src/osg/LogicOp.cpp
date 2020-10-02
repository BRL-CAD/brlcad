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
#include <osg/LogicOp>
#include <osg/Notify>

using namespace osg;

LogicOp::LogicOp():
    _opcode(COPY)
{
}

LogicOp::LogicOp(Opcode opcode):
    _opcode(opcode)
{
}

LogicOp::~LogicOp()
{
}

void LogicOp::apply(State&) const
{
#ifdef OSG_GL_FIXED_FUNCTION_AVAILABLE
    glLogicOp(static_cast<GLenum>(_opcode));
#else
    OSG_NOTICE<<"Warning: LogicOp::apply(State&) - not supported."<<std::endl;
#endif
}

