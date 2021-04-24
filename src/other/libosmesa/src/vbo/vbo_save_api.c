/**************************************************************************

Copyright 2002 Tungsten Graphics Inc., Cedar Park, Texas.

All Rights Reserved.

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
on the rights to use, copy, modify, merge, publish, distribute, sub
license, and/or sell copies of the Software, and to permit persons to whom
the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice (including the next
paragraph) shall be included in all copies or substantial portions of the
Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
TUNGSTEN GRAPHICS AND/OR THEIR SUPPLIERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
USE OR OTHER DEALINGS IN THE SOFTWARE.

**************************************************************************/

/*
 * Authors:
 *   Keith Whitwell <keith@tungstengraphics.com>
 */



/* Display list compiler attempts to store lists of vertices with the
 * same vertex layout.  Additionally it attempts to minimize the need
 * for execute-time fixup of these vertex lists, allowing them to be
 * cached on hardware.
 *
 * There are still some circumstances where this can be thwarted, for
 * example by building a list that consists of one very long primitive
 * (eg Begin(Triangles), 1000 vertices, End), and calling that list
 * from inside a different begin/end object (Begin(Lines), CallList,
 * End).  
 *
 * In that case the code will have to replay the list as individual
 * commands through the Exec dispatch table, or fix up the copied
 * vertices at execute-time.
 *
 * The other case where fixup is required is when a vertex attribute
 * is introduced in the middle of a primitive.  Eg:
 *  Begin(Lines)
 *  TexCoord1f()           Vertex2f()
 *  TexCoord1f() Color3f() Vertex2f()
 *  End()
 *
 *  If the current value of Color isn't known at compile-time, this
 *  primitive will require fixup.
 *
 *
 * The list compiler currently doesn't attempt to compile lists
 * containing EvalCoord or EvalPoint commands.  On encountering one of
 * these, compilation falls back to opcodes.  
 *
 * This could be improved to fallback only when a mix of EvalCoord and
 * Vertex commands are issued within a single primitive.
 */


#include "glheader.h"
#include "context.h"
#include "dlist.h"
#include "enums.h"
#include "macros.h"
#include "api_validate.h"
#include "api_arrayelt.h"
#include "vtxfmt.h"
#include "dispatch.h"

#include "vbo_context.h"


#ifdef ERROR
#undef ERROR
#endif


/*
 * NOTE: Old 'parity' issue is gone, but copying can still be
 * wrong-footed on replay.
 */
static GLuint _save_copy_vertices( GLcontext *ctx, 
				   const struct vbo_save_vertex_list *node,
				   const GLfloat *src_buffer)
{
   struct vbo_save_context *save = &vbo_context( ctx )->save;
   const struct _mesa_prim *prim = &node->prim[node->prim_count-1];
   GLuint nr = prim->count;
   GLuint sz = save->vertex_size;
   const GLfloat *src = src_buffer + prim->start * sz;
   GLfloat *dst = save->copied.buffer;
   GLuint ovf, i;

   if (prim->end)
      return 0;
	 
   switch( prim->mode )
   {
   case GL_POINTS:
      return 0;
   case GL_LINES:
      ovf = nr&1;
      for (i = 0 ; i < ovf ; i++)
	 _mesa_memcpy( dst+i*sz, src+(nr-ovf+i)*sz, sz*sizeof(GLfloat) );
      return i;
   case GL_TRIANGLES:
      ovf = nr%3;
      for (i = 0 ; i < ovf ; i++)
	 _mesa_memcpy( dst+i*sz, src+(nr-ovf+i)*sz, sz*sizeof(GLfloat) );
      return i;
   case GL_QUADS:
      ovf = nr&3;
      for (i = 0 ; i < ovf ; i++)
	 _mesa_memcpy( dst+i*sz, src+(nr-ovf+i)*sz, sz*sizeof(GLfloat) );
      return i;
   case GL_LINE_STRIP:
      if (nr == 0) 
	 return 0;
      else {
	 _mesa_memcpy( dst, src+(nr-1)*sz, sz*sizeof(GLfloat) );
	 return 1;
      }
   case GL_LINE_LOOP:
   case GL_TRIANGLE_FAN:
   case GL_POLYGON:
      if (nr == 0) 
	 return 0;
      else if (nr == 1) {
	 _mesa_memcpy( dst, src+0, sz*sizeof(GLfloat) );
	 return 1;
      } else {
	 _mesa_memcpy( dst, src+0, sz*sizeof(GLfloat) );
	 _mesa_memcpy( dst+sz, src+(nr-1)*sz, sz*sizeof(GLfloat) );
	 return 2;
      }
   case GL_TRIANGLE_STRIP:
   case GL_QUAD_STRIP:
      switch (nr) {
      case 0: ovf = 0; break;
      case 1: ovf = 1; break;
      default: ovf = 2 + (nr&1); break;
      }
      for (i = 0 ; i < ovf ; i++)
	 _mesa_memcpy( dst+i*sz, src+(nr-ovf+i)*sz, sz*sizeof(GLfloat) );
      return i;
   default:
      assert(0);
      return 0;
   }
}


static struct vbo_save_vertex_store *alloc_vertex_store( GLcontext *ctx )
{
   struct vbo_save_vertex_store *vertex_store = CALLOC_STRUCT(vbo_save_vertex_store);

   /* obj->Name needs to be non-zero, but won't ever be examined more
    * closely than that.  In particular these buffers won't be entered
    * into the hash and can never be confused with ones visible to the
    * user.  Perhaps there could be a special number for internal
    * buffers:
    */
   vertex_store->bufferobj = ctx->Driver.NewBufferObject(ctx, 1, GL_ARRAY_BUFFER_ARB);

   ctx->Driver.BufferData( ctx, 
			   GL_ARRAY_BUFFER_ARB, 
			   VBO_SAVE_BUFFER_SIZE * sizeof(GLfloat),
			   NULL,
			   GL_STATIC_DRAW_ARB,
			   vertex_store->bufferobj);

   vertex_store->buffer = NULL;
   vertex_store->used = 0;
   vertex_store->refcount = 1;

   return vertex_store;
}

static void free_vertex_store( GLcontext *ctx, struct vbo_save_vertex_store *vertex_store )
{
   assert(!vertex_store->buffer);

   if (vertex_store->bufferobj)
      ctx->Driver.DeleteBuffer( ctx, vertex_store->bufferobj );

   FREE( vertex_store );
}

static GLfloat *map_vertex_store( GLcontext *ctx, struct vbo_save_vertex_store *vertex_store )
{
   assert(vertex_store->bufferobj);
   assert(!vertex_store->buffer);
   vertex_store->buffer = (GLfloat *)ctx->Driver.MapBuffer(ctx, 
							   GL_ARRAY_BUFFER_ARB,	/* not used */
							   GL_WRITE_ONLY, /* not used */
							   vertex_store->bufferobj); 

   assert(vertex_store->buffer);
   return vertex_store->buffer + vertex_store->used;
}

static void unmap_vertex_store( GLcontext *ctx, struct vbo_save_vertex_store *vertex_store )
{
   ctx->Driver.UnmapBuffer( ctx, GL_ARRAY_BUFFER_ARB, vertex_store->bufferobj );
   vertex_store->buffer = NULL;
}


static struct vbo_save_primitive_store *alloc_prim_store( GLcontext *ctx )
{
   struct vbo_save_primitive_store *store = CALLOC_STRUCT(vbo_save_primitive_store);
   (void) ctx;
   store->used = 0;
   store->refcount = 1;
   return store;
}

static void _save_reset_counters( GLcontext *ctx )
{
   struct vbo_save_context *save = &vbo_context(ctx)->save;

   save->prim = save->prim_store->buffer + save->prim_store->used;
   save->buffer = (save->vertex_store->buffer + 
		   save->vertex_store->used);

   assert(save->buffer == save->vbptr);

   if (save->vertex_size)
      save->max_vert = ((VBO_SAVE_BUFFER_SIZE - save->vertex_store->used) / 
			 save->vertex_size);
   else
      save->max_vert = 0;

   save->vert_count = 0;
   save->prim_count = 0;
   save->prim_max = VBO_SAVE_PRIM_SIZE - save->prim_store->used;
   save->dangling_attr_ref = 0;
}


/* Insert the active immediate struct onto the display list currently
 * being built.
 */
static void _save_compile_vertex_list( GLcontext *ctx )
{
   struct vbo_save_context *save = &vbo_context(ctx)->save;
   struct vbo_save_vertex_list *node;

   /* Allocate space for this structure in the display list currently
    * being compiled.
    */
   node = (struct vbo_save_vertex_list *)
      _mesa_alloc_instruction(ctx, save->opcode_vertex_list, sizeof(*node));

   if (!node)
      return;

   /* Duplicate our template, increment refcounts to the storage structs:
    */
   _mesa_memcpy(node->attrsz, save->attrsz, sizeof(node->attrsz)); 
   node->vertex_size = save->vertex_size;
   node->buffer_offset = (save->buffer - save->vertex_store->buffer) * sizeof(GLfloat); 
   node->count = save->vert_count;
   node->wrap_count = save->copied.nr;
   node->dangling_attr_ref = save->dangling_attr_ref;
   node->prim = save->prim;
   node->prim_count = save->prim_count;
   node->vertex_store = save->vertex_store;
   node->prim_store = save->prim_store;

   node->vertex_store->refcount++;
   node->prim_store->refcount++;

   assert(node->attrsz[VBO_ATTRIB_POS] != 0 ||
	  node->count == 0);

   if (save->dangling_attr_ref)
      ctx->ListState.CurrentList->flags |= MESA_DLIST_DANGLING_REFS;

   save->vertex_store->used += save->vertex_size * node->count;
   save->prim_store->used += node->prim_count;


   /* Copy duplicated vertices 
    */
   save->copied.nr = _save_copy_vertices( ctx, node, save->buffer );


   /* Deal with GL_COMPILE_AND_EXECUTE:
    */
   if (ctx->ExecuteFlag) {
      struct _glapi_table *dispatch = GET_DISPATCH();

      _glapi_set_dispatch(ctx->Exec);

      vbo_loopback_vertex_list( ctx,
				(const GLfloat *)((const char *)save->vertex_store->buffer + 
						  node->buffer_offset),
				node->attrsz,
				node->prim,
				node->prim_count,
				node->wrap_count,
				node->vertex_size);

      _glapi_set_dispatch(dispatch);
   }


   /* Decide whether the storage structs are full, or can be used for
    * the next vertex lists as well.
    */
   if (save->vertex_store->used > 
       VBO_SAVE_BUFFER_SIZE - 16 * (save->vertex_size + 4)) {

      /* Unmap old store:
       */
      unmap_vertex_store( ctx, save->vertex_store );

      /* Release old reference:
       */
      save->vertex_store->refcount--; 
      assert(save->vertex_store->refcount != 0);
      save->vertex_store = NULL;

      /* Allocate and map new store:
       */
      save->vertex_store = alloc_vertex_store( ctx );
      save->vbptr = map_vertex_store( ctx, save->vertex_store );
   } 

   if (save->prim_store->used > VBO_SAVE_PRIM_SIZE - 6) {
      save->prim_store->refcount--; 
      assert(save->prim_store->refcount != 0);
      save->prim_store = alloc_prim_store( ctx );
   } 

   /* Reset our structures for the next run of vertices:
    */
   _save_reset_counters( ctx );
}


/* TODO -- If no new vertices have been stored, don't bother saving
 * it.
 */
static void _save_wrap_buffers( GLcontext *ctx )
{
   struct vbo_save_context *save = &vbo_context(ctx)->save;
   GLint i = save->prim_count - 1;
   GLenum mode;
   GLboolean weak;

   assert(i < (GLint) save->prim_max);
   assert(i >= 0);

   /* Close off in-progress primitive.
    */
   save->prim[i].count = (save->vert_count - 
			  save->prim[i].start);
   mode = save->prim[i].mode;
   weak = save->prim[i].weak;
   
   /* store the copied vertices, and allocate a new list.
    */
   _save_compile_vertex_list( ctx );

   /* Restart interrupted primitive
    */
   save->prim[0].mode = mode;
   save->prim[0].weak = weak;
   save->prim[0].begin = 0;
   save->prim[0].end = 0;
   save->prim[0].pad = 0;
   save->prim[0].start = 0;
   save->prim[0].count = 0;
   save->prim_count = 1;
}



/* Called only when buffers are wrapped as the result of filling the
 * vertex_store struct.  
 */
static void _save_wrap_filled_vertex( GLcontext *ctx )
{
   struct vbo_save_context *save = &vbo_context(ctx)->save;
   GLfloat *data = save->copied.buffer;
   GLuint i;

   /* Emit a glEnd to close off the last vertex list.
    */
   _save_wrap_buffers( ctx );
   
    /* Copy stored stored vertices to start of new list.
    */
   assert(save->max_vert - save->vert_count > save->copied.nr);

   for (i = 0 ; i < save->copied.nr ; i++) {
      _mesa_memcpy( save->vbptr, data, save->vertex_size * sizeof(GLfloat));
      data += save->vertex_size;
      save->vbptr += save->vertex_size;
      save->vert_count++;
   }
}


static void _save_copy_to_current( GLcontext *ctx )
{
   struct vbo_save_context *save = &vbo_context(ctx)->save; 
   GLuint i;

   for (i = VBO_ATTRIB_POS+1 ; i < VBO_ATTRIB_MAX ; i++) {
      if (save->attrsz[i]) {
	 save->currentsz[i][0] = save->attrsz[i];
	 COPY_CLEAN_4V(save->current[i], 
		       save->attrsz[i], 
		       save->attrptr[i]);
      }
   }
}


static void _save_copy_from_current( GLcontext *ctx )
{
   struct vbo_save_context *save = &vbo_context(ctx)->save; 
   GLint i;

   for (i = VBO_ATTRIB_POS+1 ; i < VBO_ATTRIB_MAX ; i++) {
      switch (save->attrsz[i]) {
      case 4: save->attrptr[i][3] = save->current[i][3];
      case 3: save->attrptr[i][2] = save->current[i][2];
      case 2: save->attrptr[i][1] = save->current[i][1];
      case 1: save->attrptr[i][0] = save->current[i][0];
      case 0: break;
      }
   }
}




/* Flush existing data, set new attrib size, replay copied vertices.
 */ 
static void _save_upgrade_vertex( GLcontext *ctx, 
				 GLuint attr,
				 GLuint newsz )
{
   struct vbo_save_context *save = &vbo_context(ctx)->save;
   GLuint oldsz;
   GLuint i;
   GLfloat *tmp;

   /* Store the current run of vertices, and emit a GL_END.  Emit a
    * BEGIN in the new buffer.
    */
   if (save->vert_count) 
      _save_wrap_buffers( ctx );
   else
      assert( save->copied.nr == 0 );

   /* Do a COPY_TO_CURRENT to ensure back-copying works for the case
    * when the attribute already exists in the vertex and is having
    * its size increased.  
    */
   _save_copy_to_current( ctx );

   /* Fix up sizes:
    */
   oldsz = save->attrsz[attr];
   save->attrsz[attr] = newsz;

   save->vertex_size += newsz - oldsz;
   save->max_vert = ((VBO_SAVE_BUFFER_SIZE - save->vertex_store->used) / 
		      save->vertex_size);
   save->vert_count = 0;

   /* Recalculate all the attrptr[] values:
    */
   for (i = 0, tmp = save->vertex ; i < VBO_ATTRIB_MAX ; i++) {
      if (save->attrsz[i]) {
	 save->attrptr[i] = tmp;
	 tmp += save->attrsz[i];
      }
      else 
	 save->attrptr[i] = NULL; /* will not be dereferenced. */
   }

   /* Copy from current to repopulate the vertex with correct values.
    */
   _save_copy_from_current( ctx );

   /* Replay stored vertices to translate them to new format here.
    *
    * If there are copied vertices and the new (upgraded) attribute
    * has not been defined before, this list is somewhat degenerate,
    * and will need fixup at runtime.
    */
   if (save->copied.nr)
   {
      GLfloat *data = save->copied.buffer;
      GLfloat *dest = save->buffer;
      GLuint j;

      /* Need to note this and fix up at runtime (or loopback):
       */
      if (attr != VBO_ATTRIB_POS && save->currentsz[attr][0] == 0) {
	 assert(oldsz == 0);
	 save->dangling_attr_ref = GL_TRUE;
      }

      for (i = 0 ; i < save->copied.nr ; i++) {
	 for (j = 0 ; j < VBO_ATTRIB_MAX ; j++) {
	    if (save->attrsz[j]) {
	       if (j == attr) {
		  if (oldsz) {
		     COPY_CLEAN_4V( dest, oldsz, data );
		     data += oldsz;
		     dest += newsz;
		  }
		  else {
		     COPY_SZ_4V( dest, newsz, save->current[attr] );
		     dest += newsz;
		  }
	       }
	       else {
		  GLint sz = save->attrsz[j];
		  COPY_SZ_4V( dest, sz, data );
		  data += sz;
		  dest += sz;
	       }
	    }
	 }
      }

      save->vbptr = dest;
      save->vert_count += save->copied.nr;
   }
}

static void save_fixup_vertex( GLcontext *ctx, GLuint attr, GLuint sz )
{
   struct vbo_save_context *save = &vbo_context(ctx)->save; 

   if (sz > save->attrsz[attr]) {
      /* New size is larger.  Need to flush existing vertices and get
       * an enlarged vertex format.
       */
      _save_upgrade_vertex( ctx, attr, sz );
   }
   else if (sz < save->active_sz[attr]) {
      static GLfloat id[4] = { 0, 0, 0, 1 };
      GLuint i;

      /* New size is equal or smaller - just need to fill in some
       * zeros.
       */
      for (i = sz ; i <= save->attrsz[attr] ; i++)
	 save->attrptr[attr][i-1] = id[i-1];
   }

   save->active_sz[attr] = sz;
}

static void _save_reset_vertex( GLcontext *ctx )
{
   struct vbo_save_context *save = &vbo_context(ctx)->save;
   GLuint i;

   for (i = 0 ; i < VBO_ATTRIB_MAX ; i++) {
      save->attrsz[i] = 0;
      save->active_sz[i] = 0;
   }
      
   save->vertex_size = 0;
}



#define ERROR()   _mesa_compile_error( ctx, GL_INVALID_ENUM, __FUNCTION__ );


/* Only one size for each attribute may be active at once.  Eg. if
 * Color3f is installed/active, then Color4f may not be, even if the
 * vertex actually contains 4 color coordinates.  This is because the
 * 3f version won't otherwise set color[3] to 1.0 -- this is the job
 * of the chooser function when switching between Color4f and Color3f.
 */
#define ATTR( A, N, V0, V1, V2, V3 )				\
do {								\
   struct vbo_save_context *save = &vbo_context(ctx)->save;	\
								\
   if (save->active_sz[A] != N)				\
      save_fixup_vertex(ctx, A, N);				\
								\
   {								\
      GLfloat *dest = save->attrptr[A];			\
      if (N>0) dest[0] = V0;					\
      if (N>1) dest[1] = V1;					\
      if (N>2) dest[2] = V2;					\
      if (N>3) dest[3] = V3;					\
   }								\
								\
   if ((A) == 0) {						\
      GLuint i;							\
								\
      for (i = 0; i < save->vertex_size; i++)			\
	 save->vbptr[i] = save->vertex[i];			\
								\
      save->vbptr += save->vertex_size;				\
								\
      if (++save->vert_count >= save->max_vert)			\
	 _save_wrap_filled_vertex( ctx );			\
   }								\
} while (0)

#define TAG(x) _save_##x

#include "vbo_attrib_tmp.h"




/* Cope with EvalCoord/CallList called within a begin/end object:
 *     -- Flush current buffer
 *     -- Fallback to opcodes for the rest of the begin/end object.
 */
#define DO_FALLBACK(ctx) 							\
do {									\
   struct vbo_save_context *save = &vbo_context(ctx)->save;					\
									\
   if (save->vert_count || save->prim_count) 						\
      _save_compile_vertex_list( ctx );					\
									\
   _save_copy_to_current( ctx );					\
   _save_reset_vertex( ctx );						\
   _save_reset_counters( ctx );  \
   _mesa_install_save_vtxfmt( ctx, &ctx->ListState.ListVtxfmt );	\
   ctx->Driver.SaveNeedFlush = 0;					\
} while (0)

static void GLAPIENTRY _save_EvalCoord1f( GLfloat u )
{
   GET_CURRENT_CONTEXT(ctx);
   DO_FALLBACK(ctx);
   ctx->Save->EvalCoord1f( u );
}

static void GLAPIENTRY _save_EvalCoord1fv( const GLfloat *v )
{
   GET_CURRENT_CONTEXT(ctx);
   DO_FALLBACK(ctx);
   ctx->Save->EvalCoord1fv( v );
}

static void GLAPIENTRY _save_EvalCoord2f( GLfloat u, GLfloat v )
{
   GET_CURRENT_CONTEXT(ctx);
   DO_FALLBACK(ctx);
   ctx->Save->EvalCoord2f( u, v );
}

static void GLAPIENTRY _save_EvalCoord2fv( const GLfloat *v )
{
   GET_CURRENT_CONTEXT(ctx);
   DO_FALLBACK(ctx);
   ctx->Save->EvalCoord2fv( v );
}

static void GLAPIENTRY _save_EvalPoint1( GLint i )
{
   GET_CURRENT_CONTEXT(ctx);
   DO_FALLBACK(ctx);
   ctx->Save->EvalPoint1( i );
}

static void GLAPIENTRY _save_EvalPoint2( GLint i, GLint j )
{
   GET_CURRENT_CONTEXT(ctx);
   DO_FALLBACK(ctx);
   ctx->Save->EvalPoint2( i, j );
}

static void GLAPIENTRY _save_CallList( GLuint l )
{
   GET_CURRENT_CONTEXT(ctx);
   DO_FALLBACK(ctx);
   ctx->Save->CallList( l );
}

static void GLAPIENTRY _save_CallLists( GLsizei n, GLenum type, const GLvoid *v )
{
   GET_CURRENT_CONTEXT(ctx);
   DO_FALLBACK(ctx);
   ctx->Save->CallLists( n, type, v );
}




/* This begin is hooked into ...  Updating of
 * ctx->Driver.CurrentSavePrimitive is already taken care of.
 */
GLboolean vbo_save_NotifyBegin( GLcontext *ctx, GLenum mode )
{
   struct vbo_save_context *save = &vbo_context(ctx)->save; 

   GLuint i = save->prim_count++;

   assert(i < save->prim_max);
   save->prim[i].mode = mode & ~VBO_SAVE_PRIM_WEAK;
   save->prim[i].begin = 1;
   save->prim[i].end = 0;
   save->prim[i].weak = (mode & VBO_SAVE_PRIM_WEAK) ? 1 : 0;
   save->prim[i].pad = 0;
   save->prim[i].start = save->vert_count;
   save->prim[i].count = 0;   

   _mesa_install_save_vtxfmt( ctx, &save->vtxfmt );      
   ctx->Driver.SaveNeedFlush = 1;
   return GL_TRUE;
}



static void GLAPIENTRY _save_End( void )
{
   GET_CURRENT_CONTEXT( ctx ); 
   struct vbo_save_context *save = &vbo_context(ctx)->save; 
   GLint i = save->prim_count - 1;

   ctx->Driver.CurrentSavePrimitive = PRIM_OUTSIDE_BEGIN_END;
   save->prim[i].end = 1;
   save->prim[i].count = (save->vert_count - 
			  save->prim[i].start);

   if (i == (GLint) save->prim_max - 1) {
      _save_compile_vertex_list( ctx );
      assert(save->copied.nr == 0);
   }

   /* Swap out this vertex format while outside begin/end.  Any color,
    * etc. received between here and the next begin will be compiled
    * as opcodes.
    */   
   _mesa_install_save_vtxfmt( ctx, &ctx->ListState.ListVtxfmt );
}


/* These are all errors as this vtxfmt is only installed inside
 * begin/end pairs.
 */
static void GLAPIENTRY _save_DrawElements(GLenum mode, GLsizei count, GLenum type,
			       const GLvoid *indices)
{
   GET_CURRENT_CONTEXT(ctx);
   (void) mode; (void) count; (void) type; (void) indices;
   _mesa_compile_error( ctx, GL_INVALID_OPERATION, "glDrawElements" );
}


static void GLAPIENTRY _save_DrawRangeElements(GLenum mode,
				    GLuint start, GLuint end,
				    GLsizei count, GLenum type,
				    const GLvoid *indices)
{
   GET_CURRENT_CONTEXT(ctx);
   (void) mode; (void) start; (void) end; (void) count; (void) type; (void) indices;
   _mesa_compile_error( ctx, GL_INVALID_OPERATION, "glDrawRangeElements" );
}

static void GLAPIENTRY _save_DrawArrays(GLenum mode, GLint start, GLsizei count)
{
   GET_CURRENT_CONTEXT(ctx);
   (void) mode; (void) start; (void) count;
   _mesa_compile_error( ctx, GL_INVALID_OPERATION, "glDrawArrays" );
}

static void GLAPIENTRY _save_Rectf( GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2 )
{
   GET_CURRENT_CONTEXT(ctx);
   (void) x1; (void) y1; (void) x2; (void) y2;
   _mesa_compile_error( ctx, GL_INVALID_OPERATION, "glRectf" );
}

static void GLAPIENTRY _save_EvalMesh1( GLenum mode, GLint i1, GLint i2 )
{
   GET_CURRENT_CONTEXT(ctx);
   (void) mode; (void) i1; (void) i2;
   _mesa_compile_error( ctx, GL_INVALID_OPERATION, "glEvalMesh1" );
}

static void GLAPIENTRY _save_EvalMesh2( GLenum mode, GLint i1, GLint i2,
				  GLint j1, GLint j2 )
{
   GET_CURRENT_CONTEXT(ctx);
   (void) mode; (void) i1; (void) i2; (void) j1; (void) j2;
   _mesa_compile_error( ctx, GL_INVALID_OPERATION, "glEvalMesh2" );
}

static void GLAPIENTRY _save_Begin( GLenum mode )
{
   GET_CURRENT_CONTEXT( ctx );
   (void) mode;
   _mesa_compile_error( ctx, GL_INVALID_OPERATION, "Recursive glBegin" );
}


/* Unlike the functions above, these are to be hooked into the vtxfmt
 * maintained in ctx->ListState, active when the list is known or
 * suspected to be outside any begin/end primitive.
 */
static void GLAPIENTRY _save_OBE_Rectf( GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2 )
{
   GET_CURRENT_CONTEXT(ctx);
   vbo_save_NotifyBegin( ctx, GL_QUADS | VBO_SAVE_PRIM_WEAK );
   CALL_Vertex2f(GET_DISPATCH(), ( x1, y1 ));
   CALL_Vertex2f(GET_DISPATCH(), ( x2, y1 ));
   CALL_Vertex2f(GET_DISPATCH(), ( x2, y2 ));
   CALL_Vertex2f(GET_DISPATCH(), ( x1, y2 ));
   CALL_End(GET_DISPATCH(), ());
}


static void GLAPIENTRY _save_OBE_DrawArrays(GLenum mode, GLint start, GLsizei count)
{
   GET_CURRENT_CONTEXT(ctx);
   GLint i;

   if (!_mesa_validate_DrawArrays( ctx, mode, start, count ))
      return;

   _ae_map_vbos( ctx );

   vbo_save_NotifyBegin( ctx, mode | VBO_SAVE_PRIM_WEAK );

   for (i = 0; i < count; i++)
       CALL_ArrayElement(GET_DISPATCH(), (start + i));
   CALL_End(GET_DISPATCH(), ());

   _ae_unmap_vbos( ctx );
}

/* Could do better by copying the arrays and element list intact and
 * then emitting an indexed prim at runtime.
 */
static void GLAPIENTRY _save_OBE_DrawElements(GLenum mode, GLsizei count, GLenum type,
				   const GLvoid *indices)
{
   GET_CURRENT_CONTEXT(ctx);
   GLint i;

   if (!_mesa_validate_DrawElements( ctx, mode, count, type, indices ))
      return;

   _ae_map_vbos( ctx );

   if (ctx->Array.ElementArrayBufferObj->Name)
      indices = ADD_POINTERS(ctx->Array.ElementArrayBufferObj->Pointer, indices);

   vbo_save_NotifyBegin( ctx, mode | VBO_SAVE_PRIM_WEAK );

   switch (type) {
   case GL_UNSIGNED_BYTE:
      for (i = 0 ; i < count ; i++)
	  CALL_ArrayElement(GET_DISPATCH(), ( ((GLubyte *)indices)[i] ));
      break;
   case GL_UNSIGNED_SHORT:
      for (i = 0 ; i < count ; i++)
	  CALL_ArrayElement(GET_DISPATCH(), ( ((GLushort *)indices)[i] ));
      break;
   case GL_UNSIGNED_INT:
      for (i = 0 ; i < count ; i++)
	  CALL_ArrayElement(GET_DISPATCH(), ( ((GLuint *)indices)[i] ));
      break;
   default:
      _mesa_error( ctx, GL_INVALID_ENUM, "glDrawElements(type)" );
      break;
   }

   CALL_End(GET_DISPATCH(), ());

   _ae_unmap_vbos( ctx );
}

static void GLAPIENTRY _save_OBE_DrawRangeElements(GLenum mode,
					GLuint start, GLuint end,
					GLsizei count, GLenum type,
					const GLvoid *indices)
{
   GET_CURRENT_CONTEXT(ctx);
   if (_mesa_validate_DrawRangeElements( ctx, mode,
					 start, end,
					 count, type, indices ))
      _save_OBE_DrawElements( mode, count, type, indices );
}





static void _save_vtxfmt_init( GLcontext *ctx )
{
   struct vbo_save_context *save = &vbo_context(ctx)->save;
   GLvertexformat *vfmt = &save->vtxfmt;

   vfmt->ArrayElement = _ae_loopback_array_elt;	        /* generic helper */
   vfmt->Begin = _save_Begin;
   vfmt->Color3f = _save_Color3f;
   vfmt->Color3fv = _save_Color3fv;
   vfmt->Color4f = _save_Color4f;
   vfmt->Color4fv = _save_Color4fv;
   vfmt->EdgeFlag = _save_EdgeFlag;
   vfmt->End = _save_End;
   vfmt->FogCoordfEXT = _save_FogCoordfEXT;
   vfmt->FogCoordfvEXT = _save_FogCoordfvEXT;
   vfmt->Indexf = _save_Indexf;
   vfmt->Indexfv = _save_Indexfv;
   vfmt->Materialfv = _save_Materialfv;
   vfmt->MultiTexCoord1fARB = _save_MultiTexCoord1f;
   vfmt->MultiTexCoord1fvARB = _save_MultiTexCoord1fv;
   vfmt->MultiTexCoord2fARB = _save_MultiTexCoord2f;
   vfmt->MultiTexCoord2fvARB = _save_MultiTexCoord2fv;
   vfmt->MultiTexCoord3fARB = _save_MultiTexCoord3f;
   vfmt->MultiTexCoord3fvARB = _save_MultiTexCoord3fv;
   vfmt->MultiTexCoord4fARB = _save_MultiTexCoord4f;
   vfmt->MultiTexCoord4fvARB = _save_MultiTexCoord4fv;
   vfmt->Normal3f = _save_Normal3f;
   vfmt->Normal3fv = _save_Normal3fv;
   vfmt->SecondaryColor3fEXT = _save_SecondaryColor3fEXT;
   vfmt->SecondaryColor3fvEXT = _save_SecondaryColor3fvEXT;
   vfmt->TexCoord1f = _save_TexCoord1f;
   vfmt->TexCoord1fv = _save_TexCoord1fv;
   vfmt->TexCoord2f = _save_TexCoord2f;
   vfmt->TexCoord2fv = _save_TexCoord2fv;
   vfmt->TexCoord3f = _save_TexCoord3f;
   vfmt->TexCoord3fv = _save_TexCoord3fv;
   vfmt->TexCoord4f = _save_TexCoord4f;
   vfmt->TexCoord4fv = _save_TexCoord4fv;
   vfmt->Vertex2f = _save_Vertex2f;
   vfmt->Vertex2fv = _save_Vertex2fv;
   vfmt->Vertex3f = _save_Vertex3f;
   vfmt->Vertex3fv = _save_Vertex3fv;
   vfmt->Vertex4f = _save_Vertex4f;
   vfmt->Vertex4fv = _save_Vertex4fv;
   vfmt->VertexAttrib1fARB = _save_VertexAttrib1fARB;
   vfmt->VertexAttrib1fvARB = _save_VertexAttrib1fvARB;
   vfmt->VertexAttrib2fARB = _save_VertexAttrib2fARB;
   vfmt->VertexAttrib2fvARB = _save_VertexAttrib2fvARB;
   vfmt->VertexAttrib3fARB = _save_VertexAttrib3fARB;
   vfmt->VertexAttrib3fvARB = _save_VertexAttrib3fvARB;
   vfmt->VertexAttrib4fARB = _save_VertexAttrib4fARB;
   vfmt->VertexAttrib4fvARB = _save_VertexAttrib4fvARB;

   vfmt->VertexAttrib1fNV = _save_VertexAttrib1fNV;
   vfmt->VertexAttrib1fvNV = _save_VertexAttrib1fvNV;
   vfmt->VertexAttrib2fNV = _save_VertexAttrib2fNV;
   vfmt->VertexAttrib2fvNV = _save_VertexAttrib2fvNV;
   vfmt->VertexAttrib3fNV = _save_VertexAttrib3fNV;
   vfmt->VertexAttrib3fvNV = _save_VertexAttrib3fvNV;
   vfmt->VertexAttrib4fNV = _save_VertexAttrib4fNV;
   vfmt->VertexAttrib4fvNV = _save_VertexAttrib4fvNV;
   
   /* This will all require us to fallback to saving the list as opcodes:
    */ 
   vfmt->CallList = _save_CallList; /* inside begin/end */
   vfmt->CallLists = _save_CallLists; /* inside begin/end */
   vfmt->EvalCoord1f = _save_EvalCoord1f;
   vfmt->EvalCoord1fv = _save_EvalCoord1fv;
   vfmt->EvalCoord2f = _save_EvalCoord2f;
   vfmt->EvalCoord2fv = _save_EvalCoord2fv;
   vfmt->EvalPoint1 = _save_EvalPoint1;
   vfmt->EvalPoint2 = _save_EvalPoint2;

   /* These are all errors as we at least know we are in some sort of
    * begin/end pair:
    */
   vfmt->EvalMesh1 = _save_EvalMesh1;	
   vfmt->EvalMesh2 = _save_EvalMesh2;
   vfmt->Begin = _save_Begin;
   vfmt->Rectf = _save_Rectf;
   vfmt->DrawArrays = _save_DrawArrays;
   vfmt->DrawElements = _save_DrawElements;
   vfmt->DrawRangeElements = _save_DrawRangeElements;

}


void vbo_save_SaveFlushVertices( GLcontext *ctx )
{
   struct vbo_save_context *save = &vbo_context(ctx)->save;

   /* Noop when we are actually active:
    */
   if (ctx->Driver.CurrentSavePrimitive == PRIM_INSIDE_UNKNOWN_PRIM ||
       ctx->Driver.CurrentSavePrimitive <= GL_POLYGON)
      return;

   if (save->vert_count ||
       save->prim_count) 
      _save_compile_vertex_list( ctx );
   
   _save_copy_to_current( ctx );
   _save_reset_vertex( ctx );
   _save_reset_counters( ctx );  
   ctx->Driver.SaveNeedFlush = 0;
}

void vbo_save_NewList( GLcontext *ctx, GLuint list, GLenum mode )
{
   struct vbo_save_context *save = &vbo_context(ctx)->save;

   (void) list; (void) mode;

   if (!save->prim_store)
      save->prim_store = alloc_prim_store( ctx );

   if (!save->vertex_store) 
      save->vertex_store = alloc_vertex_store( ctx );
      
   save->vbptr = map_vertex_store( ctx, save->vertex_store );
   
   _save_reset_vertex( ctx );
   _save_reset_counters( ctx );  
   ctx->Driver.SaveNeedFlush = 0;
}

void vbo_save_EndList( GLcontext *ctx )
{
   struct vbo_save_context *save = &vbo_context(ctx)->save;
   unmap_vertex_store( ctx, save->vertex_store );

   assert(save->vertex_size == 0);
}
 
void vbo_save_BeginCallList( GLcontext *ctx, struct mesa_display_list *dlist )
{
   struct vbo_save_context *save = &vbo_context(ctx)->save;
   save->replay_flags |= dlist->flags;
}

void vbo_save_EndCallList( GLcontext *ctx )
{
   struct vbo_save_context *save = &vbo_context(ctx)->save;
   
   if (ctx->ListState.CallDepth == 1) {
      /* This is correct: want to keep only the VBO_SAVE_FALLBACK
       * flag, if it is set:
       */
      save->replay_flags &= VBO_SAVE_FALLBACK;
   }
}


static void vbo_destroy_vertex_list( GLcontext *ctx, void *data )
{
   struct vbo_save_vertex_list *node = (struct vbo_save_vertex_list *)data;
   (void) ctx;

   if ( --node->vertex_store->refcount == 0 ) 
      free_vertex_store( ctx, node->vertex_store );

   if ( --node->prim_store->refcount == 0 )
      FREE( node->prim_store );
}


static void vbo_print_vertex_list( GLcontext *ctx, void *data )
{
   struct vbo_save_vertex_list *node = (struct vbo_save_vertex_list *)data;
   GLuint i;
   (void) ctx;

   _mesa_debug(NULL, "VBO-VERTEX-LIST, %u vertices %d primitives, %d vertsize\n",
               node->count,
	       node->prim_count,
	       node->vertex_size);

   for (i = 0 ; i < node->prim_count ; i++) {
      struct _mesa_prim *prim = &node->prim[i];
      _mesa_debug(NULL, "   prim %d: %s%s %d..%d %s %s\n",
		  i, 
		  _mesa_lookup_enum_by_nr(prim->mode),
		  prim->weak ? " (weak)" : "",
		  prim->start, 
		  prim->start + prim->count,
		  (prim->begin) ? "BEGIN" : "(wrap)",
		  (prim->end) ? "END" : "(wrap)");
   }
}


static void _save_current_init( GLcontext *ctx ) 
{
   struct vbo_save_context *save = &vbo_context(ctx)->save;
   GLint i;

   for (i = VBO_ATTRIB_POS; i <= VBO_ATTRIB_GENERIC15; i++) {
      const GLuint j = i - VBO_ATTRIB_POS;
      ASSERT(j < VERT_ATTRIB_MAX);
      save->currentsz[i] = &ctx->ListState.ActiveAttribSize[j];
      save->current[i] = ctx->ListState.CurrentAttrib[j];
   }

   for (i = VBO_ATTRIB_FIRST_MATERIAL; i <= VBO_ATTRIB_LAST_MATERIAL; i++) {
      const GLuint j = i - VBO_ATTRIB_FIRST_MATERIAL;
      ASSERT(j < MAT_ATTRIB_MAX);
      save->currentsz[i] = &ctx->ListState.ActiveMaterialSize[j];
      save->current[i] = ctx->ListState.CurrentMaterial[j];
   }
}

/**
 * Initialize the display list compiler
 */
void vbo_save_api_init( struct vbo_save_context *save )
{
   GLcontext *ctx = save->ctx;
   GLuint i;

   save->opcode_vertex_list =
      _mesa_alloc_opcode( ctx,
			  sizeof(struct vbo_save_vertex_list),
			  vbo_save_playback_vertex_list,
			  vbo_destroy_vertex_list,
			  vbo_print_vertex_list );

   ctx->Driver.NotifySaveBegin = vbo_save_NotifyBegin;

   _save_vtxfmt_init( ctx );
   _save_current_init( ctx );

   for (i = 0; i < VBO_ATTRIB_MAX; i++)
      save->inputs[i] = &save->arrays[i];

   /* Hook our array functions into the outside-begin-end vtxfmt in 
    * ctx->ListState.
    */
   ctx->ListState.ListVtxfmt.Rectf = _save_OBE_Rectf;
   ctx->ListState.ListVtxfmt.DrawArrays = _save_OBE_DrawArrays;
   ctx->ListState.ListVtxfmt.DrawElements = _save_OBE_DrawElements;
   ctx->ListState.ListVtxfmt.DrawRangeElements = _save_OBE_DrawRangeElements;
   _mesa_install_save_vtxfmt( ctx, &ctx->ListState.ListVtxfmt );
}

