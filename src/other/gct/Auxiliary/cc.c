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

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <float.h>
#include <time.h>
#include <stdarg.h>
#include <errno.h>
#include <assert.h>

#include "cpuconfig.h"
#include "cc.h"



////



#define CC_HASH_READ8(d,o) ((uint32_t)(((uint8_t *)d)[o]))
#define CC_HASH_AREAD16(d,o) ((uint32_t)(*((uint16_t *)ADDRESS(d,o))))
#define CC_HASH_UREAD16(d,o) ((((uint32_t)(((uint8_t *)(d))[o+1]))<<8)+(uint32_t)(((uint8_t *)(d))[o]))

uint32_t ccHash32Data( void *data, int size )
{
  uint32_t hash;
  int rem;
  rem = size & 3;
  size >>= 2;
  hash = 0;
  if( !( ( (uintptr_t)data ) & 0x1 ) )
  {
    for( ; size ; size-- )
    {
      hash += CC_HASH_AREAD16( data, 0 );
      hash = ( hash << 16 ) ^ ( ( CC_HASH_AREAD16( data, 2 ) << 11 ) ^ hash );
      hash += hash >> 11;
      data = ADDRESS( data, 4 );
    }
  }
  else
  {
    for( ; size ; size-- )
    {
      hash += CC_HASH_UREAD16( data, 0 );
      hash = ( hash << 16 ) ^ ( ( CC_HASH_UREAD16( data, 2 ) << 11 ) ^ hash );
      hash += hash >> 11;
      data = ADDRESS( data, 4 );
    }
  }
  switch( rem )
  {
    case 3:
      hash += CC_HASH_UREAD16( data, 0 );
      hash ^= hash << 16;
      hash ^= CC_HASH_READ8( data, 2 ) << 18;
      hash += hash >> 11;
      break;
    case 2:
      hash += CC_HASH_UREAD16( data, 0 );
      hash ^= hash << 11;
      hash += hash >> 17;
      break;
    case 1:
      hash += CC_HASH_READ8( data, 0 );
      hash ^= hash << 10;
      hash += hash >> 1;
      break;
    case 0:
      break;
  }
  hash ^= hash << 3;
  hash += hash >> 5;
  hash ^= hash << 4;
  hash += hash >> 17;
  hash ^= hash << 25;
  hash += hash >> 6;
  return hash;
}

uint32_t ccHash32Data32( uint32_t i )
{
  uint32_t hash;
  hash = i & 0xFFFF;
  hash = ( ( hash << 16 ) ^ hash ) ^ ( ( i & 0xFFFF0000 ) >> 5 );
  hash += hash >> 11;
  hash ^= hash << 3;
  hash += hash >> 5;
  hash ^= hash << 4;
  hash += hash >> 17;
  hash ^= hash << 25;
  hash += hash >> 6;
  return hash;
}

uint32_t ccHash32Data64( uint64_t i )
{
  uint32_t hash;
  hash = i & 0xFFFF;
  hash = ( ( hash << 16 ) ^ hash ) ^ ( ( (uint32_t)( i >> 16 ) & 0xFFFF ) << 11 );
  hash += ( hash >> 11 ) + ( (uint32_t)( i >> 32 ) & 0xFFFF );
  hash = ( ( hash << 16 ) ^ hash ) ^ (uint32_t)( ( i & 0xFFFF000000000000LL ) >> 37 );
  hash += hash >> 11;
  hash ^= hash << 3;
  hash += hash >> 5;
  hash ^= hash << 4;
  hash += hash >> 17;
  hash ^= hash << 25;
  hash += hash >> 6;
  return hash;
}

uint32_t ccHash32Array32( uint32_t *data, int count )
{
  uint32_t hash;
  hash = 0;
  for( ; count ; count-- )
  {
    hash += *data & 0xFFFF;
    hash = ( ( hash << 16 ) ^ hash ) ^ ( ( *data & 0xFFFF0000 ) >> 5 );
    hash += hash >> 11;
    data = ADDRESS( data, 4 );
  }
  hash ^= hash << 3;
  hash += hash >> 5;
  hash ^= hash << 4;
  hash += hash >> 17;
  hash ^= hash << 25;
  hash += hash >> 6;
  return hash;
}

uint32_t ccHash32Array64( uint64_t *data, int count )
{
  uint32_t hash;
  uint64_t v;
  hash = 0;
  for( ; count ; count-- )
  {
    v = *data;
    hash += v & 0xFFFF;
    hash = ( ( hash << 16 ) ^ hash ) ^ ( ( (uint32_t)( v >> 16 ) & 0xFFFF ) << 11 );
    hash += ( hash >> 11 ) + ( (uint32_t)( v >> 32 ) & 0xFFFF );
    hash = ( ( hash << 16 ) ^ hash ) ^ (uint32_t)( ( v & 0xFFFF000000000000LL ) >> 37 );
    hash += hash >> 11;
    data = ADDRESS( data, 8 );
  }
  hash ^= hash << 3;
  hash += hash >> 5;
  hash ^= hash << 4;
  hash += hash >> 17;
  hash ^= hash << 25;
  hash += hash >> 6;
  return hash;
}


////


int ccMemCmp( void *s0, void *s1, int size )
{
  int i;
  uint8_t *t0, *t1;
  t0 = s0;
  t1 = s1;
  for( i = 0 ; i < size ; i++ )
  {
    if( t0[i] != t1[i] )
      return 0;
  }
  return 1;
}

int ccMemCmp32( uint32_t *s0, uint32_t *s1, int count )
{
  int i;
  for( i = 0 ; i < count ; i++ )
  {
    if( s0[i] != s1[i] )
      return 0;
  }
  return 1;
}

int ccMemCmp64( uint64_t *s0, uint64_t *s1, int count )
{
  int i;
  for( i = 0 ; i < count ; i++ )
  {
    if( s0[i] != s1[i] )
      return 0;
  }
  return 1;
}

int ccMemCmpSize( void *s0, void *s1, int size )
{
  int i;
  uint8_t *t0, *t1;
  t0 = s0;
  t1 = s1;
  for( i = 0 ; i < size ; i++ )
  {
    if( t0[i] != t1[i] )
      break;
  }
  return i;
}


////


uint8_t ccLog2Int8( uint8_t v )
{
  uint8_t r = 0;
  if( v & 0xC )
  {
    v >>= 2;
    r |= 2;
  }
  if( v & 0x2 )
  {
    v >>= 1;
    r |= 1;
  }
  return r;
}

uint16_t ccLog2Int16( uint16_t v )
{
  uint16_t r = 0;
  if( v & 0xFF00 )
  {
    v >>= 8;
    r |= 8;
  }
  if( v & 0xF0 )
  {
    v >>= 4;
    r |= 4;
  }
  if( v & 0xC )
  {
    v >>= 2;
    r |= 2;
  }
  if( v & 0x2 )
  {
    v >>= 1;
    r |= 1;
  }
  return r;
}

uint32_t ccLog2Int32( uint32_t v )
{
  uint32_t r = 0;
  if( v & 0xFFFF0000 )
  {
    v >>= 16;
    r |= 16;
  }
  if( v & 0xFF00 )
  {
    v >>= 8;
    r |= 8;
  }
  if( v & 0xF0 )
  {
    v >>= 4;
    r |= 4;
  }
  if( v & 0xC )
  {
    v >>= 2;
    r |= 2;
  }
  if( v & 0x2 )
  {
    v >>= 1;
    r |= 1;
  }
  return r;
}

uint64_t ccLog2Int64( uint64_t v )
{
  uint64_t r = 0;
  if( v & 0xFFFFFFFF00000000LL )
  {
    v >>= 32;
    r |= 32;
  }
  if( v & 0xFFFF0000 )
  {
    v >>= 16;
    r |= 16;
  }
  if( v & 0xFF00 )
  {
    v >>= 8;
    r |= 8;
  }
  if( v & 0xF0 )
  {
    v >>= 4;
    r |= 4;
  }
  if( v & 0xC )
  {
    v >>= 2;
    r |= 2;
  }
  if( v & 0x2 )
  {
    v >>= 1;
    r |= 1;
  }
  return r;
}


////






////


#define CC_CHAR_IS_CONTROL(c) ((c)<' ')
#define CC_CHAR_IS_DELIMITER(c) ((c)<=' ')


void ccStrLowCopy( char *dst, char *src, int length )
{
  int i;
  for( i = 0 ; i < length ; i++ )
  {
    dst[i] = src[i];
    if( ( src[i] >= 'A' ) && ( src[i] <= 'Z' ) )
      dst[i] = src[i] + 'a' - 'A';
  }
  dst[i] = 0;
  return;
}


int ccStrCmpEqual( char *s0, char *s1 )
{
  int i;
  for( i = 0 ; ; i++ )
  {
    if( s0[i] != s1[i] )
      return 0;
    if( !( s0[i] ) )
      break;
  }
  return 1;
}


char *ccStrCmpWord( char *str, char *word )
{
  int i;
  for( i = 0 ; ; i++ )
  {
    if( !( word[i] ) )
      return str + i;
    if( str[i] != word[i] )
      break;
  }
  return 0;
}


char *ccStrCmpSeq( char *str, char *seq, int seqlength )
{
  int i;
  for( i = 0 ; ; i++ )
  {
    if( i >= seqlength )
      return str + i;
    if( str[i] != seq[i] )
      break;
  }
  return 0;
}


char *ccStrMatchSeq( char *str, char *seq, int seqlength )
{
  int i;
  for( i = 0 ; ; i++ )
  {
    if( i >= seqlength )
    {
      if( str[i] )
        break;
      return str + i;
    }
    if( str[i] != seq[i] )
      break;
  }
  return 0;
}


char *ccStrSeqCmpSeq( char *s1, char *s2, int s1length, int s2length )
{
  int i;
  if( s1length != s2length )
    return 0;
  for( i = 0 ; i < s1length ; i++ )
  {
    if( s1[i] != s2[i] )
      break;
  }
  return s1 + i;
}


int ccStrWordCmpWord( char *s1, char *s2 )
{
  int i;
  for( i = 0 ; ; i++ )
  {
    if( s1[i] != s2[i] )
      break;
    if( CC_CHAR_IS_DELIMITER( s1[i] ) )
    {
      if( CC_CHAR_IS_DELIMITER( s2[i] ) )
        return 1;
      else
        return 0;
    }
  }
  if( ( CC_CHAR_IS_DELIMITER( s1[i] ) ) && ( CC_CHAR_IS_DELIMITER( s2[i] ) ) )
    return 1;
  return 0;
}


char *ccStrLowCmpSeq( char *str, char *seq, int seqlength )
{
  int i;
  char c1, c2;
  for( i = 0 ; ; i++ )
  {
    if( i >= seqlength )
      return str + i;
    c1 = str[i];
    if( ( c1 >= 'A' ) && ( c1 <= 'Z' ) )
      c1 += 'a' - 'A';
    c2 = seq[i];
    if( ( c2 >= 'A' ) && ( c2 <= 'Z' ) )
      c2 += 'a' - 'A';
    if( c1 != c2 )
      break;
  }
  return 0;
}


char *ccStrFind( char *str, char *seq, int seqlength )
{
  int i;
  for( ; *str ; str++ )
  {
    for( i = 0 ; ; i++ )
    {
      if( i >= seqlength )
        return str;
      if( str[i] != seq[i] )
        break;
    }
  }
  return 0;
}


char *ccStrFindWord( char *str, char *word, int wordlength )
{
  int i, wordflag;
  wordflag = 0;
  for( ; *str ; str++ )
  {
    if( CC_CHAR_IS_DELIMITER( *str ) )
    {
      wordflag = 0;
      continue;
    }
    else if( wordflag )
      continue;
    for( i = 0 ; ; i++ )
    {
      if( i >= wordlength )
      {
        if( CC_CHAR_IS_DELIMITER( str[i] ) )
          return str;
        return 0;
      }
      if( str[i] != word[i] )
      {
        wordflag = 1;
        str += i;
        break;
      }
    }
  }
  return 0;
}


int ccStrWordLength( char *str )
{
  int i;
  for( i = 0 ; ; i++ )
  {
    if( CC_CHAR_IS_DELIMITER( str[i] ) )
      break;
  }
  return i;
}


int ccSeqFindChar( char *seq, int seqlen, char c )
{
  int i;
  for( i = 0 ; i < seqlen ; i++ )
  {
    if( seq[i] == c )
      return i;
  }
  return -1;
}

char *ccSeqFindStr( char *seq, int seqlen, char *str )
{
  int i;
  for( ; seqlen ; seqlen--, seq++ )
  {
    for( i = 0 ; ; i++ )
    {
      if( !str[i] )
        return seq;
      if( seq[i] != str[i] )
        break;
    }
  }
  return 0;
}


char *ccStrParam( char *str, int *paramlen, int *skiplen )
{
  int i;
  if( str[0] == '"' )
  {
    for( i = 1 ; ; i++ )
    {
      if( str[i] == '"' )
        break;
      if( CC_CHAR_IS_CONTROL( str[i] ) )
        return 0;
    }
    if( !( CC_CHAR_IS_DELIMITER( str[i+1] ) ) )
      return 0;
    *paramlen = i-1;
    *skiplen = i+1;
    return str+1;
  }
  if( CC_CHAR_IS_DELIMITER( str[0] ) )
    return 0;
  for( i = 1 ; ; i++ )
  {
    if( CC_CHAR_IS_DELIMITER( str[i] ) )
      break;
  }
  *paramlen = i;
  *skiplen = i;
  return str;
}


int ccParseParameters( char *str, char **argv )
{
  int argc, paramlen, skiplen;
  char *param;

  argc = 0;
  for( ; ; )
  {
    if( !( str = ccStrNextWordSameLine( str ) ) )
      break;
    param = ccStrParam( str, &paramlen, &skiplen );
    if( argv )
      argv[argc] = param;
    if( !( param ) )
      break;
    str += skiplen;
    argc++;
  }

  return argc;
}


char *ccStrNextWord( char *str )
{
  for( ; ; str++ )
  {
    if( *str == 0 )
      return 0;
    if( !( CC_CHAR_IS_DELIMITER( *str ) ) )
      break;
  }
  return str;
}



char *ccStrSkipWord( char *str )
{
  for( ; ; str++ )
  {
    if( !( *str ) )
      return 0;
    if( ( *str == ' ' ) || ( *str == '\t' ) || ( *str == '\n' ) || ( *str == '\r' ) )
      break;
  }
  return str;
}


char *ccStrNextWordSameLine( char *str )
{
  for( ; ; str++ )
  {
    if( *str == 0 )
      return 0;
    if( *str == '\n' )
      return 0;
    if( !( CC_CHAR_IS_DELIMITER( *str ) ) )
      break;
  }
  return str;
}


char *ccStrNextParam( char *str )
{
  for( ; ; str++ )
  {
    if( ( *str == 0 ) || ( *str != '\n' ) )
      return 0;
    if( !( CC_CHAR_IS_DELIMITER( *str ) ) )
      break;
  }
  return str;
}


char *ccStrNextLine( char *str )
{
  for( ; ; str++ )
  {
    if( *str == 0 )
      return str;
    if( *str == '\n' )
      break;
  }
  return str + 1;
}


char *ccStrPassLine( char *str )
{
  for( ; ; str++ )
  {
    if( ( *str == 0 ) || ( *str == '\n' ) )
      break;
    if( !( CC_CHAR_IS_DELIMITER( *str ) ) )
      return 0;
  }
  return str;
}


int ccStrParseInt( char *str, int64_t *retint )
{
  int negflag;
  char c;
  int64_t workint;

  negflag = 0;
  if( *str == '-' )
    negflag = 1;
  str += negflag;

  workint = 0;
  for( ; ; str++ )
  {
    c = *str;
    if( CC_CHAR_IS_DELIMITER( c ) )
      break;
    if( ( c < '0' ) || ( c > '9' ) )
      return 0;
    workint = ( workint * 10 ) + ( c - '0' );
  }

  if( negflag )
    workint = -workint;
  *retint = workint;
  return 1;
}


int ccSeqParseInt( char *seq, int seqlength, int64_t *retint )
{
  int i, negflag;
  char c;
  int64_t workint;

  if( !( seqlength ) )
    return 0;
  negflag = 0;
  i = 0;
  if( *seq == '-' )
  {
    negflag = 1;
    i = 1;
  }

  workint = 0;
  for( ; i < seqlength ; i++ )
  {
    c = seq[i];
    if( CC_CHAR_IS_DELIMITER( c ) )
      break;
    if( ( c < '0' ) || ( c > '9' ) )
      return 0;
    workint = ( workint * 10 ) + ( c - '0' );
  }

  if( negflag )
    workint = -workint;
  *retint = workint;
  return 1;
}


int ccStrParseFloat( char *str, float *retfloat )
{
  char c;
  int negflag;
  double workfloat;
  double decfactor;

  negflag = 0;
  if( *str == '-' )
    negflag = 1;
  str += negflag;

  workfloat = 0.0;
  for( ; ; str++ )
  {
    c = *str;
    if( CC_CHAR_IS_DELIMITER( c ) )
      goto done;
    if( c == '.' )
      break;
    if( ( c < '0' ) || ( c > '9' ) )
      return 0;
    workfloat = ( workfloat * 10.0 ) + (float)( c - '0' );
  }

  str++;
  decfactor = 0.1;
  for( ; ; str++ )
  {
    c = *str;
    if( CC_CHAR_IS_DELIMITER( c ) )
      break;
    if( ( c < '0' ) || ( c > '9' ) )
      return 0;
    workfloat += (float)( c - '0' ) * decfactor;
    decfactor *= 0.1;
  }

  done:

  if( negflag )
    workfloat = -workfloat;
  *retfloat = (float)workfloat;
  return 1;
}


////


#define CC_SORT_SWAP(a,b) ({temp=table[b];table[b]=table[a];table[a]=temp;})

#define CC_SORT_STACK_DEPTH (512)

#define CC_SORT_MIN_QSORT_COUNT (5)

typedef struct
{
  void *table;
  int count;
} ccQuickSortStack;

static void ccQuickSortPart( void **table, int count, int (*sortfunc)( void *t0, void *t1 ) )
{
  void *temp;
  switch( count )
  {
    case 4:
      if( sortfunc( table[0], table[1] ) )
    	CC_SORT_SWAP( 1, 0 );
      if( sortfunc( table[2], table[3] ) )
    	CC_SORT_SWAP( 3, 2 );
      if( sortfunc( table[0], table[2] ) )
      {
    	temp = table[2];
    	table[2] = table[1];
    	table[1] = table[0];
    	table[0] = temp;
    	if( sortfunc( table[2], table[3] ) )
    	{
    	  CC_SORT_SWAP( 3, 2 );
    	  if( sortfunc( table[1], table[2] ) )
    	    CC_SORT_SWAP( 2, 1 );
    	}
      }
      else
      {
    	if( sortfunc( table[1], table[2] ) )
    	{
    	  CC_SORT_SWAP( 2, 1 );
    	  if( sortfunc( table[2], table[3] ) )
    	    CC_SORT_SWAP( 3, 2 );
    	}
      }
      break;
    case 3:
      if( sortfunc( table[0], table[1] ) )
      {
    	if( sortfunc( table[1], table[2] ) )
    	{
    	  /* [1]>[0], [2]>[1] = [2]>[1]>[0] */
    	  CC_SORT_SWAP( 2, 0 );
    	}
    	else
    	{
          if( sortfunc( table[0], table[2] ) )
    	  {
    	    /* [1]>[0], [2]<[1], [2]>[0] = [1]>[2]>[0] */
    	    temp = table[0];
    	    table[0] = table[1];
    	    table[1] = table[2];
    	    table[2] = temp;
    	  }
    	  else
    	  {
    	    /* [1]>[0], [2]<[1], [2]<[0] = [1]>[0]>[2] */
    	    CC_SORT_SWAP( 1, 0 );
    	  }
    	}
      }
      else
      {
    	if( sortfunc( table[1], table[2] ) )
    	{
    	  if( sortfunc( table[0], table[2] ) )
    	  {
    	    /* [1]<[0], [2]>[1], [2]>[0] = [2]>[0]>[1] */
    	    temp = table[2];
    	    table[2] = table[1];
    	    table[1] = table[0];
    	    table[0] = temp;
    	  }
    	  else
    	  {
    	    /* [1]<[0], [2]>[1], [2]<[0] = [0]>[2]>[1] */
    	    CC_SORT_SWAP( 1, 2 );
    	  }
    	}
    	else
    	{
    	  /* [1]<[0], [2]<[1] = [0]>[1]>[2] */
    	}
      }
      break;
    case 2:
      if( sortfunc( table[0], table[1] ) )
        CC_SORT_SWAP( 1, 0 );
      break;
    case 1:
    case 0:
    default:
      break;
  }
  return;
}


void ccQuickSort( void **table, int count, int (*sortfunc)( void *t0, void *t1 ), uint32_t randmask )
{
  ssize_t i, pivotindex, leftcount, rightcount, highindex, pivotstore;
  void *temp;
  void *pivot;
  ccQuickSortStack stack[CC_SORT_STACK_DEPTH];
  ccQuickSortStack *sp;

  if( count < CC_SORT_MIN_QSORT_COUNT )
  {
    ccQuickSortPart( table, count, sortfunc );
    return;
  }

  sp = stack;
  for( ; ; )
  {
    /* Select pivot */
    randmask += count;
    pivotindex = 1 + ( randmask % ( count-2 ) );

    if( sortfunc( table[0], table[pivotindex] ) )
      CC_SORT_SWAP( pivotindex, 0 );
    if( sortfunc( table[pivotindex], table[count-1] ) )
    {
      CC_SORT_SWAP( count-1, pivotindex );
      if( sortfunc( table[0], table[pivotindex] ) )
        CC_SORT_SWAP( pivotindex, 0 );
    }

    /* Quick sort on both sides of the pivot */
    pivot = table[pivotindex];
    highindex = count - 2;
    pivotstore = highindex;
    CC_SORT_SWAP( pivotstore, pivotindex );
    pivotindex = 1;
    for( i = highindex ; --i ; )
    {
      if( sortfunc( pivot, table[pivotindex] ) )
        pivotindex++;
      else
      {
        highindex--;
        CC_SORT_SWAP( highindex, pivotindex );
      }
    }
    CC_SORT_SWAP( pivotindex, pivotstore );

    /* Count of entries on both sides of the pivot */
    leftcount = pivotindex;
    pivotindex++;
    rightcount = count - pivotindex;

    /* Fast sort small chunks, iterate */
    if( leftcount < CC_SORT_MIN_QSORT_COUNT )
    {
      ccQuickSortPart( table, leftcount, sortfunc );
      table += pivotindex;
      count = rightcount;
      if( rightcount < CC_SORT_MIN_QSORT_COUNT )
      {
        ccQuickSortPart( table, count, sortfunc );
        if( sp == stack )
          break;
        sp--;
        table = sp->table;
        count = sp->count;
      }
    }
    else if( rightcount < CC_SORT_MIN_QSORT_COUNT )
    {
      ccQuickSortPart( &table[pivotindex], rightcount, sortfunc );
      count = leftcount;
    }
    else if( leftcount < rightcount )
    {
      sp->table = &table[pivotindex];
      sp->count = rightcount;
      sp++;
      count = leftcount;
    }
    else
    {
      sp->table = table;
      sp->count = leftcount;
      sp++;
      table += pivotindex;
      count = rightcount;
    }
  }

  return;
}

static void ccQuickSortContextPart( void **table, int count, int (*sortfunc)( void *context, void *t0, void *t1 ), void *context )
{
  void *temp;
  switch( count )
  {
    case 4:
      if( sortfunc( context, table[0], table[1] ) )
    	CC_SORT_SWAP( 1, 0 );
      if( sortfunc( context, table[2], table[3] ) )
    	CC_SORT_SWAP( 3, 2 );
      if( sortfunc( context, table[0], table[2] ) )
      {
    	temp = table[2];
    	table[2] = table[1];
    	table[1] = table[0];
    	table[0] = temp;
    	if( sortfunc( context, table[2], table[3] ) )
    	{
    	  CC_SORT_SWAP( 3, 2 );
    	  if( sortfunc( context, table[1], table[2] ) )
    	    CC_SORT_SWAP( 2, 1 );
    	}
      }
      else
      {
    	if( sortfunc( context, table[1], table[2] ) )
    	{
    	  CC_SORT_SWAP( 2, 1 );
    	  if( sortfunc( context, table[2], table[3] ) )
    	    CC_SORT_SWAP( 3, 2 );
    	}
      }
      break;
    case 3:
      if( sortfunc( context, table[0], table[1] ) )
      {
    	if( sortfunc( context, table[1], table[2] ) )
    	{
    	  /* [1]>[0], [2]>[1] = [2]>[1]>[0] */
    	  CC_SORT_SWAP( 2, 0 );
    	}
    	else
    	{
          if( sortfunc( context, table[0], table[2] ) )
    	  {
    	    /* [1]>[0], [2]<[1], [2]>[0] = [1]>[2]>[0] */
    	    temp = table[0];
    	    table[0] = table[1];
    	    table[1] = table[2];
    	    table[2] = temp;
    	  }
    	  else
    	  {
    	    /* [1]>[0], [2]<[1], [2]<[0] = [1]>[0]>[2] */
    	    CC_SORT_SWAP( 1, 0 );
    	  }
    	}
      }
      else
      {
    	if( sortfunc( context, table[1], table[2] ) )
    	{
    	  if( sortfunc( context, table[0], table[2] ) )
    	  {
    	    /* [1]<[0], [2]>[1], [2]>[0] = [2]>[0]>[1] */
    	    temp = table[2];
    	    table[2] = table[1];
    	    table[1] = table[0];
    	    table[0] = temp;
    	  }
    	  else
    	  {
    	    /* [1]<[0], [2]>[1], [2]<[0] = [0]>[2]>[1] */
    	    CC_SORT_SWAP( 1, 2 );
    	  }
    	}
    	else
    	{
    	  /* [1]<[0], [2]<[1] = [0]>[1]>[2] */
    	}
      }
      break;
    case 2:
      if( sortfunc( context, table[0], table[1] ) )
        CC_SORT_SWAP( 1, 0 );
      break;
    case 1:
    case 0:
    default:
      break;
  }
  return;
}

void ccQuickSortContext( void **table, int count, int (*sortfunc)( void *context, void *t0, void *t1 ), void *context, uint32_t randmask )
{
  ssize_t i, pivotindex, leftcount, rightcount, highindex, pivotstore;
  void *temp;
  void *pivot;
  ccQuickSortStack stack[CC_SORT_STACK_DEPTH];
  ccQuickSortStack *sp;

  if( count < CC_SORT_MIN_QSORT_COUNT )
  {
    ccQuickSortContextPart( table, count, sortfunc, context );
    return;
  }

  sp = stack;
  for( ; ; )
  {
    /* Select pivot */
    randmask += count;
    pivotindex = 1 + ( randmask % ( count-2 ) );

    if( sortfunc( context, table[0], table[pivotindex] ) )
      CC_SORT_SWAP( pivotindex, 0 );
    if( sortfunc( context, table[pivotindex], table[count-1] ) )
    {
      CC_SORT_SWAP( count-1, pivotindex );
      if( sortfunc( context, table[0], table[pivotindex] ) )
        CC_SORT_SWAP( pivotindex, 0 );
    }

    /* Quick sort on both sides of the pivot */
    pivot = table[pivotindex];
    highindex = count - 2;
    pivotstore = highindex;
    CC_SORT_SWAP( pivotstore, pivotindex );
    pivotindex = 1;
    for( i = highindex ; --i ; )
    {
      if( sortfunc( context, pivot, table[pivotindex] ) )
        pivotindex++;
      else
      {
        highindex--;
        CC_SORT_SWAP( highindex, pivotindex );
      }
    }
    CC_SORT_SWAP( pivotindex, pivotstore );

    /* Count of entries on both sides of the pivot */
    leftcount = pivotindex;
    pivotindex++;
    rightcount = count - pivotindex;

    /* Fast sort small chunks, iterate */
    if( leftcount < CC_SORT_MIN_QSORT_COUNT )
    {
      ccQuickSortContextPart( table, leftcount, sortfunc, context );
      table += pivotindex;
      count = rightcount;
      if( rightcount < CC_SORT_MIN_QSORT_COUNT )
      {
        ccQuickSortContextPart( table, count, sortfunc, context );
        if( sp == stack )
          break;
        sp--;
        table = sp->table;
        count = sp->count;
      }
    }
    else if( rightcount < CC_SORT_MIN_QSORT_COUNT )
    {
      ccQuickSortContextPart( &table[pivotindex], rightcount, sortfunc, context );
      count = leftcount;
    }
    else if( leftcount < rightcount )
    {
      sp->table = &table[pivotindex];
      sp->count = rightcount;
      sp++;
      count = leftcount;
    }
    else
    {
      sp->table = table;
      sp->count = leftcount;
      sp++;
      table += pivotindex;
      count = rightcount;
    }
  }

  return;
}



////


typedef struct
{
  void **src;
  void **dst;
  int count;
  int mergeflag;
  int depthbit;
} ccMergeSortStack;

int ccMergeSort( void **src, void **tmp, int count, int (*sortfunc)( void *t0, void *t1 ) )
{
  int swapflag, depthbit, maxdepthbit;
  ssize_t i, leftcount, rightcount;
  void **dst, **sl, **sr, *temp, **swap;
  ccMergeSortStack stack[CC_SORT_STACK_DEPTH];
  ccMergeSortStack *sp;

  dst = tmp;
  sp = stack;
  swapflag = 0;
  depthbit = 0;

  if( count <= 1 )
    return 0;

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
        if( ( count == 4 ) && sortfunc( src[2], src[3] ) )
        {
          temp = src[2];
          src[2] = src[3];
          src[3] = temp;
        }
        if( sortfunc( src[0], src[1] ) )
        {
          temp = src[0];
          src[0] = src[1];
          src[1] = temp;
        }
        swapflag = 0;
      }
      else
      {
        if( count == 4 )
        {
          if( sortfunc( src[2], src[3] ) )
          {
            dst[2] = src[3];
            dst[3] = src[2];
          }
          else
          {
            dst[2] = src[2];
            dst[3] = src[3];
          }
        }
        else if( count == 3 )
          dst[2] = src[2];
        if( sortfunc( src[0], src[1] ) )
        {
          dst[0] = src[1];
          dst[1] = src[0];
        }
        else
        {
          dst[0] = src[0];
          dst[1] = src[1];
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
      for( ; ; )
      {
        if( sortfunc( *sl, *sr ) )
        {
          *dst++ = *sr++;
          if( --rightcount )
            continue;
          for( i = 0 ; i < leftcount ; i++ )
            dst[i] = sl[i];
          break;
        }
        else
        {
          *dst++ = *sl++;
          if( --leftcount )
            continue;
          for( i = 0 ; i < rightcount ; i++ )
            dst[i] = sr[i];
          break;
        }
      }

      if( sp == stack )
        return swapflag ^ 1;
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

int ccMergeSortContext( void **src, void **tmp, int count, int (*sortfunc)( void *context, void *t0, void *t1 ), void *context )
{
  int swapflag, depthbit, maxdepthbit;
  ssize_t i, leftcount, rightcount;
  void **dst, **sl, **sr, *temp, **swap;
  ccMergeSortStack stack[CC_SORT_STACK_DEPTH];
  ccMergeSortStack *sp;

  dst = tmp;
  sp = stack;
  swapflag = 0;
  depthbit = 0;

  if( count <= 1 )
    return 0;

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
        if( ( count == 4 ) && sortfunc( context, src[2], src[3] ) )
        {
          temp = src[2];
          src[2] = src[3];
          src[3] = temp;
        }
        if( sortfunc( context, src[0], src[1] ) )
        {
          temp = src[0];
          src[0] = src[1];
          src[1] = temp;
        }
        swapflag = 0;
      }
      else
      {
        if( count == 4 )
        {
          if( sortfunc( context, src[2], src[3] ) )
          {
            dst[2] = src[3];
            dst[3] = src[2];
          }
          else
          {
            dst[2] = src[2];
            dst[3] = src[3];
          }
        }
        else if( count == 3 )
          dst[2] = src[2];
        if( sortfunc( context, src[0], src[1] ) )
        {
          dst[0] = src[1];
          dst[1] = src[0];
        }
        else
        {
          dst[0] = src[0];
          dst[1] = src[1];
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
      for( ; ; )
      {
        if( sortfunc( context, *sl, *sr ) )
        {
          *dst++ = *sr++;
          if( --rightcount )
            continue;
          for( i = 0 ; i < leftcount ; i++ )
            dst[i] = sl[i];
          break;
        }
        else
        {
          *dst++ = *sl++;
          if( --leftcount )
            continue;
          for( i = 0 ; i < rightcount ; i++ )
            dst[i] = sr[i];
          break;
        }
      }

      if( sp == stack )
        return swapflag ^ 1;
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



////



#define CC_DEBUG_LOG_SIZE (4096)

void ccDebugLog( char *filename, char *string, ... )
{
  ssize_t slen;
  char buffer[CC_DEBUG_LOG_SIZE];
  char *wbuf;
  size_t bufsize;
  va_list ap;
  FILE *file;

  if( !( file = fopen( filename, "a" ) ) )
    return;

  wbuf = buffer;
  bufsize = CC_DEBUG_LOG_SIZE;
  for( ; ; )
  {
    va_start( ap, string );
    slen = vsnprintf( wbuf, bufsize, string, ap );
    va_end( ap );
    if( slen < bufsize )
      break;
    if( wbuf != buffer )
      free( wbuf );
    bufsize = slen + 1;
    wbuf = malloc( bufsize );
  }

  fprintf( file, "%s", wbuf );

  if( wbuf != buffer )
    free( wbuf );
  fclose( file );

  return;
}

