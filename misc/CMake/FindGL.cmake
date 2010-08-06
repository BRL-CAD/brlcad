########################################################################
#
# Copyright (c) 2008, Lawrence Livermore National Security, LLC.  
# Produced at the Lawrence Livermore National Laboratory  
# Written by bremer5@llnl.gov,pascucci@sci.utah.edu.  
# LLNL-CODE-406031.  
# All rights reserved.  
#   
# This file is part of "Simple and Flexible Scene Graph Version 2.0."
# Please also read BSD_ADDITIONAL.txt.
#   
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#   
# @ Redistributions of source code must retain the above copyright
#   notice, this list of conditions and the disclaimer below.
# @ Redistributions in binary form must reproduce the above copyright
#   notice, this list of conditions and the disclaimer (as noted below) in
#   the documentation and/or other materials provided with the
#   distribution.
# @ Neither the name of the LLNS/LLNL nor the names of its contributors
#   may be used to endorse or promote products derived from this software
#   without specific prior written permission.
#   
#  
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL LAWRENCE
# LIVERMORE NATIONAL SECURITY, LLC, THE U.S. DEPARTMENT OF ENERGY OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
# LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
# NEGLIGENCE OR OTHERWISE) ARISING
#
########################################################################
#
# BSD_ADDITIONAL.txt
#
# 1. This notice is required to be provided under our contract with the
# U.S.  Department of Energy (DOE).  This work was produced at Lawrence
# Livermore National Laboratory under Contract No. DE-AC52-07NA27344
# with the DOE.
#  
# 2. Neither the United States Government nor Lawrence Livermore
# National Security, LLC nor any of their employees, makes any warranty,
# express or implied, or assumes any liability or responsibility for the
# accuracy, completeness, or usefulness of any information, apparatus,
# product, or process disclosed, or represents that its use would not
# infringe privately-owned rights.
#    
# 3.  Also, reference herein to any specific commercial products,
# process, or services by trade name, trademark, manufacturer or
# otherwise does not necessarily constitute or imply its endorsement,
# recommendation, or favoring by the United States Government or
# Lawrence Livermore National Security, LLC.  The views and opinions of
# authors expressed herein do not necessarily state or reflect those of
# the United States Government or Lawrence Livermore National Security,
# LLC, and shall not be used for advertising or product endorsement
# purposes.
# 
 
INCLUDE(${CMAKE_ROOT}/Modules/FindOpenGL.cmake)

IF (APPLE_NATIVE_GL)

   FIND_PATH(AGL_INCLUDE_DIR agl.h
      /System/Library/Frameworks/AGL.framework/Headers
      NO_DEFAULT_PATH)
      

ELSE (APPLE_NATIVE_GL)

   # If we are supposed to use the standard OpenGL we first check
   # whetherwe got the correct path (the one containing the GL/
   # prefix)

   IF (NOT EXISTS ${OPENGL_INCLUDE_DIR}/GL/gl.h)
      SET(OPENGL_INCLUDE_DIR OPENGL_INCLUDE_DIR-NOTFOUND)
      FIND_PATH(OPENGL_INCLUDE_DIR GL/gl.h
         /usr/
         /usr/X11R6/
         PATH_SUFFIXES /include 
      ) 
   ENDIF (NOT EXISTS ${OPENGL_INCLUDE_DIR}/GL/gl.h)

   # IF we are on apple but are not using the native OpenGl we must
   # search for the X version of the libraries since the first find
   # command has likely returned the Frameworks instead

   IF (APPLE)
      SET(OPENGL_glu_LIBRARY OPENGL_glu_LIBRARY-NOTFOUND) 
      FIND_LIBRARY(OPENGL_glu_LIBRARY glu
         /usr
         /usr/X11
         /usr/X11R6
         PATH_SUFFIXES /lib        
      ) 
   
      SET(OPENGL_gl_LIBRARY OPENGL_gl_LIBRARY-NOTFOUND) 
      FIND_LIBRARY(OPENGL_gl_LIBRARY gl
         /usr
         /usr/X11
         /usr/X11R6
         PATH_SUFFIXES /lib        
      )   
   
      SET(OPENGL_LIBRARIES ${OPENGL_glu_LIBRARY} ${OPENGL_gl_LIBRARY})
   ENDIF (APPLE)
ENDIF (APPLE_NATIVE_GL)
     
# Now do a last check whether we found everything we needed
   
IF (OPENGL_INCLUDE_DIR AND OPENGL_LIBRARIES)   

   IF (CMAKE_VERBOSE_MAKEFILE)
      MESSAGE("Using OPENGL_INCLUDE_DIR = " ${OPENGL_INCLUDE_DIR})
      MESSAGE("Using OPENGL_LIBRARIES = " ${OPENGL_LIBRARIES})        
   ENDIF (CMAKE_VERBOSE_MAKEFILE)
 
ELSE (OPENGL_INCLUDE_DIR AND OPENGL_LIBRARIES)   
   SET(OPENGL_FOUND "NO") 
ENDIF (OPENGL_INCLUDE_DIR AND OPENGL_LIBRARIES)   
