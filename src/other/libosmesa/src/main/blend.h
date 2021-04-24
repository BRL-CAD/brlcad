/**
 * \file blend.h
 * Blending functions operations.
 */

/*
 * Mesa 3-D graphics library
 * Version:  6.5.2
 *
 * Copyright (C) 1999-2006  Brian Paul   All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * BRIAN PAUL BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
 * AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */



#ifndef BLEND_H
#define BLEND_H


#include "mtypes.h"


extern void GLAPIENTRY
_mesa_BlendFunc( GLenum sfactor, GLenum dfactor );


extern void GLAPIENTRY
_mesa_BlendFuncSeparateEXT( GLenum sfactorRGB, GLenum dfactorRGB,
                            GLenum sfactorA, GLenum dfactorA );


extern void GLAPIENTRY
_mesa_BlendEquation( GLenum mode );


extern void GLAPIENTRY
_mesa_BlendEquationSeparateEXT( GLenum modeRGB, GLenum modeA );


extern void GLAPIENTRY
_mesa_BlendColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);


extern void GLAPIENTRY
_mesa_AlphaFunc( GLenum func, GLclampf ref );


extern void GLAPIENTRY
_mesa_LogicOp( GLenum opcode );


extern void GLAPIENTRY
_mesa_IndexMask( GLuint mask );

extern void GLAPIENTRY
_mesa_ColorMask( GLboolean red, GLboolean green,
                 GLboolean blue, GLboolean alpha );


extern void GLAPIENTRY
_mesa_ClampColorARB(GLenum target, GLenum clamp);


extern void  
_mesa_init_color( GLcontext * ctx );

#endif
