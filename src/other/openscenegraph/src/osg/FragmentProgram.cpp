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
#include <osg/Notify>
#include <osg/GLExtensions>
#include <osg/FragmentProgram>
#include <osg/State>
#include <osg/Timer>

#include <list>

#include <OpenThreads/ScopedLock>
#include <OpenThreads/Mutex>

using namespace osg;

// static cache of deleted fragment programs which can only
// by completely deleted once the appropriate OpenGL context
// is set.
typedef std::list<GLuint> FragmentProgramObjectList;
typedef osg::buffered_object<FragmentProgramObjectList> DeletedFragmentProgramObjectCache;

static OpenThreads::Mutex                s_mutex_deletedFragmentProgramObjectCache;
static DeletedFragmentProgramObjectCache s_deletedFragmentProgramObjectCache;

void FragmentProgram::deleteFragmentProgramObject(unsigned int contextID,GLuint handle)
{
    if (handle!=0)
    {
        OpenThreads::ScopedLock<OpenThreads::Mutex> lock(s_mutex_deletedFragmentProgramObjectCache);

        // insert the handle into the cache for the appropriate context.
        s_deletedFragmentProgramObjectCache[contextID].push_back(handle);
    }
}


void FragmentProgram::flushDeletedFragmentProgramObjects(unsigned int contextID,double /*currentTime*/, double& availableTime)
{
    // if no time available don't try to flush objects.
    if (availableTime<=0.0) return;

    const osg::Timer& timer = *osg::Timer::instance();
    osg::Timer_t start_tick = timer.tick();
    double elapsedTime = 0.0;

    {
        OpenThreads::ScopedLock<OpenThreads::Mutex> lock(s_mutex_deletedFragmentProgramObjectCache);

        const Extensions* extensions = getExtensions(contextID,true);

        FragmentProgramObjectList& vpol = s_deletedFragmentProgramObjectCache[contextID];

        for(FragmentProgramObjectList::iterator titr=vpol.begin();
            titr!=vpol.end() && elapsedTime<availableTime;
            )
        {
            extensions->glDeletePrograms( 1L, &(*titr ) );
            titr = vpol.erase(titr);
            elapsedTime = timer.delta_s(start_tick,timer.tick());
        }
    }

    availableTime -= elapsedTime;
}

void FragmentProgram::discardDeletedFragmentProgramObjects(unsigned int contextID)
{
    OpenThreads::ScopedLock<OpenThreads::Mutex> lock(s_mutex_deletedFragmentProgramObjectCache);
    FragmentProgramObjectList& vpol = s_deletedFragmentProgramObjectCache[contextID];
    vpol.clear();
}


FragmentProgram::FragmentProgram()
{
}


FragmentProgram::FragmentProgram(const FragmentProgram& vp,const CopyOp& copyop):
    osg::StateAttribute(vp,copyop)
{
    _fragmentProgram = vp._fragmentProgram;

    for( LocalParamList::const_iterator itr = vp._programLocalParameters.begin();
        itr != vp._programLocalParameters.end(); ++itr )
    {
        _programLocalParameters[itr->first] = itr->second;
    }

    for( MatrixList::const_iterator mitr = vp._matrixList.begin();
        mitr != vp._matrixList.end(); ++mitr )
    {
        _matrixList[mitr->first] = mitr->second;
    }
}


// virtual
FragmentProgram::~FragmentProgram()
{
    dirtyFragmentProgramObject();
}

void FragmentProgram::dirtyFragmentProgramObject()
{
    for(unsigned int i=0;i<_fragmentProgramIDList.size();++i)
    {
        if (_fragmentProgramIDList[i] != 0)
        {
            FragmentProgram::deleteFragmentProgramObject(i,_fragmentProgramIDList[i]);
            _fragmentProgramIDList[i] = 0;
        }
    }
}

void FragmentProgram::apply(State& state) const
{
#ifdef OSG_GL_FIXED_FUNCTION_AVAILABLE

    const unsigned int contextID = state.getContextID();
    const Extensions* extensions = getExtensions(contextID,true);

    if (!extensions->isFragmentProgramSupported())
        return;


    GLuint& fragmentProgramId=getFragmentProgramID(state.getContextID());

    // Fragment Program
    if (fragmentProgramId != 0)
    {
        extensions->glBindProgram( GL_FRAGMENT_PROGRAM_ARB, fragmentProgramId );
    }
    else if (!_fragmentProgram.empty())
    {
        glGetError(); // Reset Error flags.
        extensions->glGenPrograms( 1, &fragmentProgramId );
        extensions->glBindProgram( GL_FRAGMENT_PROGRAM_ARB, fragmentProgramId );
        extensions->glProgramString( GL_FRAGMENT_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB,
                                     _fragmentProgram.length(), _fragmentProgram.c_str());

        // Check for errors
        GLint errorposition = 0;
        glGetIntegerv(GL_PROGRAM_ERROR_POSITION_ARB, &errorposition);
        if (errorposition != -1)
        {
            OSG_FATAL << "FragmentProgram: " << glGetString(GL_PROGRAM_ERROR_STRING_ARB) << std::endl;

            std::string::size_type start = _fragmentProgram.rfind('\n', errorposition);
            std::string::size_type stop  = _fragmentProgram.find('\n', errorposition);
            if (start!=std::string::npos && stop!=std::string::npos)
            {
                OSG_FATAL << "             : " << _fragmentProgram.substr(start+1, stop-start-2) << std::endl;
                std::string pointAtproblem(errorposition-(start+1), ' ');
                OSG_FATAL << "             : " << pointAtproblem << '^' << std::endl;
            }
            return;
        }
    }

    // Update local program parameters
    {
        for(LocalParamList::const_iterator itr=_programLocalParameters.begin();
            itr!=_programLocalParameters.end();
            ++itr)
        {
            extensions->glProgramLocalParameter4fv(GL_FRAGMENT_PROGRAM_ARB, (*itr).first, (*itr).second.ptr());
        }
    }

    // Update matrix
    if (!_matrixList.empty())
    {
        for(MatrixList::const_iterator itr = _matrixList.begin();
            itr!=_matrixList.end();
            ++itr)
        {
            glMatrixMode((*itr).first);
            glLoadMatrix((*itr).second.ptr());
        }
        glMatrixMode(GL_MODELVIEW); // restore matrix mode
    }
#else
    OSG_NOTICE<<"Warning: FragmentProgram::apply(State&) - not supported."<<std::endl;
#endif
}

void FragmentProgram::resizeGLObjectBuffers(unsigned int maxSize)
{
    _fragmentProgramIDList.resize(maxSize);
}

void FragmentProgram::releaseGLObjects(State* state) const
{
    if (!state) const_cast<FragmentProgram*>(this)->dirtyFragmentProgramObject();
    else
    {
        unsigned int contextID = state->getContextID();
        if (_fragmentProgramIDList[contextID] != 0)
        {
            FragmentProgram::deleteFragmentProgramObject(contextID,_fragmentProgramIDList[contextID]);
            _fragmentProgramIDList[contextID] = 0;
        }
    }
}


typedef buffered_value< ref_ptr<FragmentProgram::Extensions> > BufferedExtensions;
static BufferedExtensions s_extensions;

FragmentProgram::Extensions* FragmentProgram::getExtensions(unsigned int contextID,bool createIfNotInitalized)
{
    if (!s_extensions[contextID] && createIfNotInitalized) s_extensions[contextID] = new Extensions(contextID);
    return s_extensions[contextID].get();
}

void FragmentProgram::setExtensions(unsigned int contextID,Extensions* extensions)
{
    s_extensions[contextID] = extensions;
}

FragmentProgram::Extensions::Extensions(unsigned int contextID)
{
    setupGLExtensions(contextID);
}

FragmentProgram::Extensions::Extensions(const Extensions& rhs):
    Referenced()
{
    _isFragmentProgramSupported = rhs._isFragmentProgramSupported;
    _glBindProgram = rhs._glBindProgram;
    _glGenPrograms = rhs._glGenPrograms;
    _glDeletePrograms = rhs._glDeletePrograms;
    _glProgramString = rhs._glProgramString;
    _glProgramLocalParameter4fv = rhs._glProgramLocalParameter4fv;
}


void FragmentProgram::Extensions::lowestCommonDenominator(const Extensions& rhs)
{
    if (!rhs._isFragmentProgramSupported) _isFragmentProgramSupported = false;

    if (!rhs._glBindProgram) _glBindProgram = 0;
    if (!rhs._glGenPrograms) _glGenPrograms = 0;
    if (!rhs._glDeletePrograms) _glDeletePrograms = 0;
    if (!rhs._glProgramString) _glProgramString = 0;
    if (!rhs._glProgramLocalParameter4fv) _glProgramLocalParameter4fv = 0;

}

void FragmentProgram::Extensions::setupGLExtensions(unsigned int contextID)
{
    _isFragmentProgramSupported = isGLExtensionSupported(contextID,"GL_ARB_fragment_program");

    setGLExtensionFuncPtr(_glBindProgram, "glBindProgramARB");
    setGLExtensionFuncPtr(_glGenPrograms, "glGenProgramsARB");
    setGLExtensionFuncPtr(_glDeletePrograms, "glDeleteProgramsARB");
    setGLExtensionFuncPtr(_glProgramString, "glProgramStringARB");
    setGLExtensionFuncPtr(_glProgramLocalParameter4fv, "glProgramLocalParameter4fvARB");
}

void FragmentProgram::Extensions::glBindProgram(GLenum target, GLuint id) const
{
    if (_glBindProgram)
    {
        _glBindProgram(target,id);
    }
    else
    {
        OSG_WARN<<"Error: glBindProgram not supported by OpenGL driver"<<std::endl;
    }
}

void FragmentProgram::Extensions::glGenPrograms(GLsizei n, GLuint *programs) const
{
    if (_glGenPrograms)
    {
        _glGenPrograms(n,programs);
    }
    else
    {
        OSG_WARN<<"Error: glGenPrograms not supported by OpenGL driver"<<std::endl;
    }
}

void FragmentProgram::Extensions::glDeletePrograms(GLsizei n, GLuint *programs) const
{
    if (_glDeletePrograms)
    {
        _glDeletePrograms(n,programs);
    }
    else
    {
        OSG_WARN<<"Error: glDeletePrograms not supported by OpenGL driver"<<std::endl;
    }
}

void FragmentProgram::Extensions::glProgramString(GLenum target, GLenum format, GLsizei len, const void *string) const
{
    if (_glProgramString)
    {
        _glProgramString(target,format, len, string);
    }
    else
    {
        OSG_WARN<<"Error: glProgramString not supported by OpenGL driver"<<std::endl;
    }
}

void FragmentProgram::Extensions::glProgramLocalParameter4fv(GLenum target, GLuint index, const GLfloat *params) const
{
    if (_glProgramLocalParameter4fv)
    {
        _glProgramLocalParameter4fv(target, index, params);
    }
    else
    {
        OSG_WARN<<"Error: glProgramLocalParameter4fv not supported by OpenGL driver"<<std::endl;
    }
}
