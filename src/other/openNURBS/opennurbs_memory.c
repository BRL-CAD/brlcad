/* $Header$ */
/* $NoKeywords: $ */
/*
//
// Copyright (c) 1993-2006 Robert McNeel & Associates. All rights reserved.
// Rhinoceros is a registered trademark of Robert McNeel & Assoicates.
//
// THIS SOFTWARE IS PROVIDED "AS IS" WITHOUT EXPRESS OR IMPLIED WARRANTY.
// ALL IMPLIED WARRANTIES OF FITNESS FOR ANY PARTICULAR PURPOSE AND OF
// MERCHANTABILITY ARE HEREBY DISCLAIMED.
//				
// For complete openNURBS copyright information see <http://www.opennurbs.org>.
//
////////////////////////////////////////////////////////////////
*/

#include "opennurbs_system.h"
#include "opennurbs_defines.h"
#include "opennurbs_memory.h"
#include "opennurbs_error.h"

void* onmalloc( size_t sz )
{
  return (sz > 0) ? malloc(sz) : 0;
}

void* oncalloc( size_t num, size_t sz )
{
  return (num > 0 && sz > 0) ? calloc( num, sz ) : 0;
}


void onfree( void* memblock )
{
  if ( memblock )
    free( memblock );
}


void* onrealloc( void* memblock, size_t sz )
{
#if defined(_MSC_VER)
#if _MSC_VER == 1200
/*
//  (_MSC_VER is defined as 1200 for Microsoft Visual C++ 6.0)
//
//   NOTE WELL: Microsoft's VC 6.0 realloc() contains a bug that can cause
//              crashes and should be avoided.  See MSDN Knowledge Base
//              article ID Q225099 for more information.
*/
#define ON_REALLOC_BROKEN
#endif
#endif

#if defined(ON_REALLOC_BROKEN)
  void* p;
  size_t memblocksz;
#endif

  if ( !memblock )
  {
    return onmalloc(sz);
  }
  
  if ( sz <= 0 )
  {
    onfree(memblock);
    return 0;
  }

#if defined(ON_REALLOC_BROKEN)

  /* use malloc() and memcpy() instead of buggy realloc() */
  memblocksz = _msize(memblock);
  if ( sz <= memblocksz ) 
  {
    /* shrink */
    if ( memblocksz <= 28 || 8*sz >= 7*memblocksz ) 
    {
      /* don't bother reallocating */
      p = memblock;
    }
    else 
    {
      /* allocate smaller block */
      p = malloc(sz);
      if ( p ) 
      {
        memcpy( p, memblock, sz );
        free(memblock);
      }
    }
  }
  else if ( sz > memblocksz ) 
  {
    /* grow */
    p = malloc(sz);
    if ( p ) 
    {
      memcpy( p, memblock, memblocksz );
      free(memblock);
    }
  }
  return p;

#else

  return realloc(memblock,sz);

#endif
}


size_t onmsize( const void* memblock )
{
#if defined(ON_OS_WINDOWS)
  return _msize((void*)memblock);
#else
  // OS doesn't support _msize().
  return 0;
#endif
}

