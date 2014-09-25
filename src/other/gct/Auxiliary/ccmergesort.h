/* *****************************************************************************
 *
 * Copyright (c) 2014 Alexis Naveros. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without modification, are
 * permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 * *****************************************************************************
 */

/*

Templates C style!

#include this whole file with the following definitions set:

#define MSORT_MAIN MyInlinedSortFunction
#define MSORT_CMP MyComparisonFunction
#define MSORT_TYPE int

*/


#ifndef CC_MSORT_INLINE

typedef struct
{
  void *src;
  void *dst;
  int count;
  int mergeflag;
  int depthbit;
} ccMergeSortStack;

 #define CC_MSORT_INLINE
 #define CC_MSORT_STACK_DEPTH (512)

#endif


#ifndef MSORT_COPY
 #define MSORT_COPY(d,s) (*(d)=*(s))
 #define CC_MSORT_COPY
#endif


static MSORT_TYPE *MSORT_MAIN( MSORT_TYPE *src, MSORT_TYPE *tmp, int count )
{
  int swapflag, depthbit, maxdepthbit;
  ssize_t leftcount, rightcount;
  MSORT_TYPE *dst, *sl, *sr, *dstend, *dstbase, *swap, temp;
  ccMergeSortStack stack[CC_MSORT_STACK_DEPTH];
  ccMergeSortStack *sp;

  dst = tmp;
  sp = stack;
  swapflag = 0;
  depthbit = 0;

  if( count <= 1 )
    return src;

  leftcount = count;
  for( maxdepthbit = 1 ; ; maxdepthbit ^= 1 )
  {
    leftcount = leftcount - ( leftcount >> 1 );
    if( leftcount <= 4 )
      break;
  }

  for( ; ; )
  {
    if( count <= 4 )
    {
      if( !( depthbit ^ maxdepthbit ) )
      {
        if( ( count == 4 ) && MSORT_CMP( &src[2], &src[3] ) )
        {
          MSORT_COPY( &temp, &src[2] );
          MSORT_COPY( &src[2], &src[3] );
          MSORT_COPY( &src[3], &temp );
        }
        if( MSORT_CMP( &src[0], &src[1] ) )
        {
          MSORT_COPY( &temp, &src[0] );
          MSORT_COPY( &src[0], &src[1] );
          MSORT_COPY( &src[1], &temp );
        }
        swapflag = 0;
      }
      else
      {
        if( count == 4 )
        {
          if( MSORT_CMP( &src[2], &src[3] ) )
          {
            MSORT_COPY( &dst[2], &src[3] );
            MSORT_COPY( &dst[3], &src[2] );
          }
          else
          {
            MSORT_COPY( &dst[2], &src[2] );
            MSORT_COPY( &dst[3], &src[3] );
          }
        }
        else if( count == 3 )
          MSORT_COPY( &dst[2], &src[2] );
        if( MSORT_CMP( &src[0], &src[1] ) )
        {
          MSORT_COPY( &dst[0], &src[1] );
          MSORT_COPY( &dst[1], &src[0] );
        }
        else
        {
          MSORT_COPY( &dst[0], &src[0] );
          MSORT_COPY( &dst[1], &src[1] );
        }
        swap = src;
        src = dst;
        dst = swap;
        swapflag = 1;
      }
    }
    else
    {
      rightcount = count >> 1;
      leftcount = count - rightcount;
      sp->src = src;
      sp->dst = dst;
      sp->count = count;
      sp->mergeflag = 1;
      sp->depthbit = depthbit;
      depthbit ^= 1;
      sp++;
      sp->src = src + leftcount;
      sp->dst = dst + leftcount;
      sp->count = rightcount;
      sp->mergeflag = 0;
      sp->depthbit = depthbit;
      sp++;
      count = leftcount;
      continue;
    }

    for( ; ; )
    {
      rightcount = count >> 1;
      leftcount = count - rightcount;
      sl = src;
      sr = src + leftcount;
      dstbase = dst;
      for( ; ; )
      {
        if( MSORT_CMP( sl, sr ) )
        {
          MSORT_COPY( dst++, sr++ );
          if( --rightcount )
            continue;
          for( dstend = &dst[leftcount] ; dst < dstend ; )
            MSORT_COPY( dst++, sl++ );
          break;
        }
        else
        {
          MSORT_COPY( dst++, sl++ );
          if( --leftcount )
            continue;
          for( dstend = &dst[rightcount] ; dst < dstend ; )
            MSORT_COPY( dst++, sr++ );
          break;
        }
      }
      if( sp == stack )
        return dstbase;
      sp--;
      src = sp->src;
      dst = sp->dst;
      count = sp->count;
      depthbit = sp->depthbit;
      if( !( sp->mergeflag ) )
        break;
      swapflag ^= 1;
      if( swapflag )
      {
        src = sp->dst;
        dst = sp->src;
      }
    }
  }

  return 0;
}


#ifndef CC_MSORT_COPY
 #undef MSORT_COPY
 #undef CC_MSORT_COPY
#endif
