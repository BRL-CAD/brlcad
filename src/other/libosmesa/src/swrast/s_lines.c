/*
 * Mesa 3-D graphics library
 * Version:  6.5.3
 *
 * Copyright (C) 1999-2007  Brian Paul   All Rights Reserved.
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


#include "glheader.h"
#include "context.h"
#include "colormac.h"
#include "macros.h"
#include "s_aaline.h"
#include "s_context.h"
#include "s_depth.h"
#include "s_feedback.h"
#include "s_lines.h"
#include "s_span.h"


/*
 * Init the mask[] array to implement a line stipple.
 */
static void
compute_stipple_mask( GLcontext *ctx, GLuint len, GLubyte mask[] )
{
   SWcontext *swrast = SWRAST_CONTEXT(ctx);
   GLuint i;

   for (i = 0; i < len; i++) {
      GLuint bit = (swrast->StippleCounter / ctx->Line.StippleFactor) & 0xf;
      if ((1 << bit) & ctx->Line.StipplePattern) {
         mask[i] = GL_TRUE;
      }
      else {
         mask[i] = GL_FALSE;
      }
      swrast->StippleCounter++;
   }
}


/*
 * To draw a wide line we can simply redraw the span N times, side by side.
 */
static void
draw_wide_line( GLcontext *ctx, SWspan *span, GLboolean xMajor )
{
   GLint width, start;

   ASSERT(span->end < MAX_WIDTH);

   width = (GLint) CLAMP( ctx->Line._Width, MIN_LINE_WIDTH, MAX_LINE_WIDTH );

   if (width & 1)
      start = width / 2;
   else
      start = width / 2 - 1;

   if (xMajor) {
      GLint *y = span->array->y;
      GLuint i;
      GLint w;
      for (w = 0; w < width; w++) {
         if (w == 0) {
            for (i = 0; i < span->end; i++)
               y[i] -= start;
         }
         else {
            for (i = 0; i < span->end; i++)
               y[i]++;
         }
         if (ctx->Visual.rgbMode)
            _swrast_write_rgba_span(ctx, span);
         else
            _swrast_write_index_span(ctx, span);
      }
   }
   else {
      GLint *x = span->array->x;
      GLuint i;
      GLint w;
      for (w = 0; w < width; w++) {
         if (w == 0) {
            for (i = 0; i < span->end; i++)
               x[i] -= start;
         }
         else {
            for (i = 0; i < span->end; i++)
               x[i]++;
         }
         if (ctx->Visual.rgbMode)
            _swrast_write_rgba_span(ctx, span);
         else
            _swrast_write_index_span(ctx, span);
      }
   }
}



/**********************************************************************/
/*****                    Rasterization                           *****/
/**********************************************************************/

/* Simple color index line (no stipple, width=1, no Z, no fog, no tex)*/
#define NAME simple_ci_line
#define INTERP_INDEX
#define RENDER_SPAN(span) _swrast_write_index_span(ctx, &span)
#include "s_linetemp.h"

/* Simple RGBA index line (no stipple, width=1, no Z, no fog, no tex)*/
#define NAME simple_rgba_line
#define INTERP_RGBA
#define RENDER_SPAN(span) _swrast_write_rgba_span(ctx, &span);
#include "s_linetemp.h"


/* Z, fog, wide, stipple color index line */
#define NAME general_ci_line
#define INTERP_INDEX
#define INTERP_Z
#define INTERP_FOG
#define RENDER_SPAN(span)					\
   if (ctx->Line.StippleFlag) {					\
      span.arrayMask |= SPAN_MASK;				\
      compute_stipple_mask(ctx, span.end, span.array->mask);    \
   }								\
   if (ctx->Line._Width > 1.0) {					\
      draw_wide_line(ctx, &span, (GLboolean)(dx > dy));		\
   }								\
   else {							\
      _swrast_write_index_span(ctx, &span);			\
   }
#include "s_linetemp.h"


/* Z, fog, wide, stipple RGBA line */
#define NAME general_rgba_line
#define INTERP_RGBA
#define INTERP_Z
#define INTERP_FOG
#define RENDER_SPAN(span)					\
   if (ctx->Line.StippleFlag) {					\
      span.arrayMask |= SPAN_MASK;				\
      compute_stipple_mask(ctx, span.end, span.array->mask);	\
   }								\
   if (ctx->Line._Width > 1.0) {					\
      draw_wide_line(ctx, &span, (GLboolean)(dx > dy));		\
   }								\
   else {							\
      _swrast_write_rgba_span(ctx, &span);			\
   }
#include "s_linetemp.h"


/* General-purpose textured line (any/all features). */
#define NAME textured_line
#define INTERP_RGBA
#define INTERP_SPEC
#define INTERP_Z
#define INTERP_FOG
#define INTERP_ATTRIBS
#define RENDER_SPAN(span)					\
   if (ctx->Line.StippleFlag) {					\
      span.arrayMask |= SPAN_MASK;				\
      compute_stipple_mask(ctx, span.end, span.array->mask);	\
   }								\
   if (ctx->Line._Width > 1.0) {					\
      draw_wide_line(ctx, &span, (GLboolean)(dx > dy));		\
   }								\
   else {							\
      _swrast_write_rgba_span(ctx, &span);			\
   }
#include "s_linetemp.h"



void
_swrast_add_spec_terms_line( GLcontext *ctx,
                             const SWvertex *v0,
                             const SWvertex *v1 )
{
   SWvertex *ncv0 = (SWvertex *)v0;
   SWvertex *ncv1 = (SWvertex *)v1;
   GLchan c[2][4];
   COPY_CHAN4( c[0], ncv0->color );
   COPY_CHAN4( c[1], ncv1->color );
   ACC_3V( ncv0->color, ncv0->specular );
   ACC_3V( ncv1->color, ncv1->specular );
   SWRAST_CONTEXT(ctx)->SpecLine( ctx, ncv0, ncv1 );
   COPY_CHAN4( ncv0->color, c[0] );
   COPY_CHAN4( ncv1->color, c[1] );
}


#ifdef DEBUG
extern void
_mesa_print_line_function(GLcontext *ctx);  /* silence compiler warning */
void
_mesa_print_line_function(GLcontext *ctx)
{
   SWcontext *swrast = SWRAST_CONTEXT(ctx);

   _mesa_printf("Line Func == ");
   if (swrast->Line == simple_ci_line)
      _mesa_printf("simple_ci_line\n");
   else if (swrast->Line == simple_rgba_line)
      _mesa_printf("simple_rgba_line\n");
   else if (swrast->Line == general_ci_line)
      _mesa_printf("general_ci_line\n");
   else if (swrast->Line == general_rgba_line)
      _mesa_printf("general_rgba_line\n");
   else if (swrast->Line == textured_line)
      _mesa_printf("textured_line\n");
   else
      _mesa_printf("Driver func %p\n", (void *(*)()) swrast->Line);
}
#endif



#ifdef DEBUG

/* record the current line function name */
static const char *lineFuncName = NULL;

#define USE(lineFunc)                   \
do {                                    \
    lineFuncName = #lineFunc;           \
    /*_mesa_printf("%s\n", lineFuncName);*/   \
    swrast->Line = lineFunc;            \
} while (0)

#else

#define USE(lineFunc)  swrast->Line = lineFunc

#endif



/*
 * Determine which line drawing function to use given the current
 * rendering context.
 *
 * Please update the summary flag _SWRAST_NEW_LINE if you add or remove
 * tests to this code.
 */
void
_swrast_choose_line( GLcontext *ctx )
{
   SWcontext *swrast = SWRAST_CONTEXT(ctx);
   const GLboolean rgbmode = ctx->Visual.rgbMode;

   if (ctx->RenderMode == GL_RENDER) {
      if (ctx->Line.SmoothFlag) {
         /* antialiased lines */
         _swrast_choose_aa_line_function(ctx);
         ASSERT(swrast->Line);
      }
      else if (ctx->Texture._EnabledCoordUnits
             || ctx->FragmentProgram._Current) {
         /* textured lines */
         USE(textured_line);
      }
      else if (ctx->Depth.Test || swrast->_FogEnabled || ctx->Line._Width != 1.0
               || ctx->Line.StippleFlag) {
         /* no texture, but Z, fog, width>1, stipple, etc. */
         if (rgbmode)
            USE(general_rgba_line);
         else
            USE(general_ci_line);
      }
      else {
         /* simplest lines */
         if (rgbmode)
            USE(simple_rgba_line);
         else
            USE(simple_ci_line);
      }
   }
   else if (ctx->RenderMode == GL_FEEDBACK) {
      USE(_swrast_feedback_line);
   }
   else {
      ASSERT(ctx->RenderMode == GL_SELECT);
      USE(_swrast_select_line);
   }

   /*_mesa_print_line_function(ctx);*/
}
