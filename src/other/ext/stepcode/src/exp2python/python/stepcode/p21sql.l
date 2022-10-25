/*
** 2015-08-12
**
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
**
******************************************************************************
**
** This SQLite extension implements P21 functions.  The interface is
** modeled after MySQL JSON functions:
**
**     https://dev.mysql.com/doc/refman/5.7/en/json.html
**
** For the time being, all P21 params are stored as pure text.
*/
#if !defined(SQLITE_CORE) || defined(SQLITE_ENABLE_P21SQL)
#if !defined(SQLITEINT_H)
#include "sqlite3ext.h"
#endif
SQLITE_EXTENSION_INIT1
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

/* Mark a function parameter as unused, to suppress nuisance compiler
** warnings. */
#ifndef UNUSED_PARAM
# define UNUSED_PARAM(X)  (void)(X)
#endif

#ifndef LARGEST_INT64
# define LARGEST_INT64  (0xffffffff|(((sqlite3_int64)0x7fffffff)<<32))
# define SMALLEST_INT64 (((sqlite3_int64)-1) - LARGEST_INT64)
#endif

/*
** Versions of isspace(), isalnum() and isdigit() to which it is safe
** to pass signed char values.
*/
#ifdef sqlite3Isdigit
   /* Use the SQLite core versions if this routine is part of the
   ** SQLite amalgamation */
#  define safe_isdigit(x)  sqlite3Isdigit(x)
#  define safe_isalnum(x)  sqlite3Isalnum(x)
#  define safe_isxdigit(x) sqlite3Isxdigit(x)
#else
   /* Use the standard library for separate compilation */
#include <ctype.h>  /* amalgamator: keep */
#  define safe_isdigit(x)  isdigit((unsigned char)(x))
#  define safe_isalnum(x)  isalnum((unsigned char)(x))
#  define safe_isxdigit(x) isxdigit((unsigned char)(x))
#endif

/*
** Growing our own isspace() routine this way is twice as fast as
** the library isspace() function, resulting in a 7% overall performance
** increase for the parser.  (Ubuntu14.10 gcc 4.8.4 x64 with -Os).
*/
static const char p21IsSpace[] = {
  0, 0, 0, 0, 0, 0, 0, 0,     0, 1, 1, 0, 0, 1, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0,     0, 0, 0, 0, 0, 0, 0, 0,
  1, 0, 0, 0, 0, 0, 0, 0,     0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0,     0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0,     0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0,     0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0,     0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0,     0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0,     0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0,     0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0,     0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0,     0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0,     0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0,     0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0,     0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0,     0, 0, 0, 0, 0, 0, 0, 0,
};
#define safe_isspace(x) (p21IsSpace[(unsigned char)x])

#ifndef SQLITE_AMALGAMATION
  /* Unsigned integer types.  These are already defined in the sqliteInt.h,
  ** but the definitions need to be repeated for separate compilation. */
  typedef sqlite3_uint64 u64;
  typedef unsigned int u32;
  typedef unsigned short int u16;
  typedef unsigned char u8;
#endif
  
/* some C implementations don't have these? (inttypes.h / stdint.h) */
#ifndef UINT16_WIDTH
#  define UINT16_WIDTH     16
#endif
#ifndef UINT16_MAX
#  define UINT16_MAX       65535
#endif

/* Objects */
typedef struct P21String P21String;
typedef struct P21Node P21Node;
typedef struct P21Parse P21Parse;

/* An instance of this object represents a P21 parameter string
** under construction.  Really, this is a generic string accumulator
** that can be and is used to create strings other than JSON (here P21!).
*/
struct P21String {
  sqlite3_context *pCtx;   /* Function context - put error messages here */
  char *zBuf;              /* Append P21 content here */
  u64 nAlloc;              /* Bytes of storage available in zBuf[] */
  u64 nUsed;               /* Bytes of zBuf[] currently used */
  u8 bStatic;              /* True if zBuf is static space */
  u8 bErr;                 /* True if an error has been encountered */
  char zSpace[100];        /* Initial static space */
};

#define P21_EMPTY           0x1       /* optional attribute not provided : '$' */
#define P21_DERIVED         0x2       /* derived attribute not provided  : '*' */
#define P21_ENUMERATION     0x3       /* (also) includes boolean and logical values */
#define P21_INTEGER         0x4       
#define P21_REAL            0x5
#define P21_STRING          0x6
#define P21_BINARY          0x7
#define P21_EID             0x8       /* entity_instance_name */
#define P21_LIST            0x9
#define P21_RECORD          0xA       /* simple_record */

#define P21_SUBTYPE         80      /* Ascii for "P" */


/*
** Names of the various P21 types:
*/
static const char * const p21Type[] = {
  "",
  "empty", "derived", "enumeration", "integer", "real",
  "string", "binary", "eid", "list", "record"
};

/* Bit values for the P21Node.jnFlag field
*/
#define PNODE_RAW     0x01         /* Content is raw, not P21 encoded */
#define PNODE_ESCAPE  0x02         /* Content is text with \ escapes */
#define PNODE_REMOVE  0x04         /* Do not output */
#define PNODE_REPLACE 0x08         /* Replace with P21Node.u.iReplace */
#define PNODE_PATCH   0x10         /* Patch with P21Node.u.pPatch */
#define PNODE_APPEND  0x20         /* More ARRAY/OBJECT entries at u.iAppend */
#define PNODE_LABEL   0x40         /* Is a label of an object */


/* A single node of parsed P21 params
*/
struct P21Node {
  u8 eType;              /* One of the P21_ type values */
  u8 jnFlags;            /* P21Node flags */
  u16 n_kw;              /* store the KEYWORD length */
  u32 n;                 /* Bytes of content, or number of sub-nodes */
  union {
    const char *zJContent; /* Content for INT, REAL, and STRING */
    u32 iAppend;           /* More terms for ARRAY and OBJECT */
    u32 iKey;              /* Key for ARRAY objects in p21_tree() */
    u32 iReplace;          /* Replacement content for PNODE_REPLACE */
    P21Node *pPatch;      /* Node chain of patch for PNODE_PATCH */
  } u;
};

/* A completely parsed P21 string
*/
struct P21Parse {
  u32 nNode;         /* Number of slots of aNode[] used */
  u32 nAlloc;        /* Number of slots of aNode[] allocated */
  P21Node *aNode;   /* Array of nodes containing the parse */
  const char *zP21; /* Original P21 string */
  u32 *aUp;          /* Index of parent of each node */
  u8 oom;            /* Set to true if out of memory */
  u8 nErr;           /* Number of errors seen */
  u16 iDepth;        /* Nesting depth */
  int nP21;         /* Length of the zP21 string in bytes */
  u32 iHold;         /* Replace cache line with the lowest iHold value */
};

/*
** Maximum nesting depth of P21 for this implementation.
*/
#define P21_MAX_DEPTH  20

/**************************************************************************
** Utility routines for dealing with P21String objects
**************************************************************************/

/* Set the P21String object to an empty string
*/
static void p21Zero(P21String *p){
  p->zBuf = p->zSpace;
  p->nAlloc = sizeof(p->zSpace);
  p->nUsed = 0;
  p->bStatic = 1;
}

/* Initialize the P21String object
*/
static void p21Init(P21String *p, sqlite3_context *pCtx){
  p->pCtx = pCtx;
  p->bErr = 0;
  p21Zero(p);
}


/* Free all allocated memory and reset the P21String object back to its
** initial state.
*/
static void p21Reset(P21String *p){
  if( !p->bStatic ) sqlite3_free(p->zBuf);
  p21Zero(p);
}


/* Report an out-of-memory (OOM) condition 
*/
static void p21Oom(P21String *p){
  p->bErr = 1;
  sqlite3_result_error_nomem(p->pCtx);
  p21Reset(p);
}

/* Enlarge p->zBuf so that it can hold at least N more bytes.
** Return zero on success.  Return non-zero on an OOM error
*/
static int p21Grow(P21String *p, u32 N){
  u64 nTotal = N<p->nAlloc ? p->nAlloc*2 : p->nAlloc+N+10;
  char *zNew;
  if( p->bStatic ){
    if( p->bErr ) return 1;
    zNew = sqlite3_malloc64(nTotal);
    if( zNew==0 ){
      p21Oom(p);
      return SQLITE_NOMEM;
    }
    memcpy(zNew, p->zBuf, (size_t)p->nUsed);
    p->zBuf = zNew;
    p->bStatic = 0;
  }else{
    zNew = sqlite3_realloc64(p->zBuf, nTotal);
    if( zNew==0 ){
      p21Oom(p);
      return SQLITE_NOMEM;
    }
    p->zBuf = zNew;
  }
  p->nAlloc = nTotal;
  return SQLITE_OK;
}

/* Append N bytes from zIn onto the end of the P21String string.
*/
static void p21AppendRaw(P21String *p, const char *zIn, u32 N){
  if( (N+p->nUsed >= p->nAlloc) && p21Grow(p,N)!=0 ) return;
  memcpy(p->zBuf+p->nUsed, zIn, N);
  p->nUsed += N;
}

/* Append formatted text (not to exceed N bytes) to the P21String.
*/
static void p21Printf(int N, P21String *p, const char *zFormat, ...){
  va_list ap;
  if( (p->nUsed + N >= p->nAlloc) && p21Grow(p, N) ) return;
  va_start(ap, zFormat);
  sqlite3_vsnprintf(N, p->zBuf+p->nUsed, zFormat, ap);
  va_end(ap);
  p->nUsed += (int)strlen(p->zBuf+p->nUsed);
}

/* Append a single character
*/
static void p21AppendChar(P21String *p, char c){
  if( p->nUsed>=p->nAlloc && p21Grow(p,1)!=0 ) return;
  p->zBuf[p->nUsed++] = c;
}

/* Append a comma separator to the output buffer, if the previous
** character is not '[' or '{'.
*/
static void p21AppendSeparator(P21String *p){
  char c;
  if( p->nUsed==0 ) return;
  c = p->zBuf[p->nUsed-1];
  if( c!='(' ) p21AppendChar(p, ',');
}

/* Append the N-byte string in zIn to the end of the P21String string
** under construction.  Enclose the string in '...' and escape
** any double-quotes or backslash characters contained within the
** string.
*/
static void p21AppendString(P21String *p, const char *zIn, u32 N){
  u32 i;
  if( (N+p->nUsed+2 >= p->nAlloc) && p21Grow(p,N+2)!=0 ) return;
  p->zBuf[p->nUsed++] = '\'';
  for(i=0; i<N; i++){
    unsigned char c = ((unsigned const char*)zIn)[i];
    /* TODO: string escapes are different e.g. '' */
    if( c=='\'' || c=='\\' ){
      p21_simple_escape:
      if( (p->nUsed+N+3-i > p->nAlloc) && p21Grow(p,N+3-i)!=0 ) return;
      p->zBuf[p->nUsed++] = '\\';
    }else if( c<=0x1f ){
      static const char aSpecial[] = {
         0, 0, 0, 0, 0, 0, 0, 0, 'b', 't', 'n', 0, 'f', 'r', 0, 0,
         0, 0, 0, 0, 0, 0, 0, 0,   0,   0,   0, 0,   0,   0, 0, 0
      };
      assert( sizeof(aSpecial)==32 );
      assert( aSpecial['\b']=='b' );
      assert( aSpecial['\f']=='f' );
      assert( aSpecial['\n']=='n' );
      assert( aSpecial['\r']=='r' );
      assert( aSpecial['\t']=='t' );
      if( aSpecial[c] ){
        c = aSpecial[c];
        goto p21_simple_escape;
      }
      if( (p->nUsed+N+7+i > p->nAlloc) && p21Grow(p,N+7-i)!=0 ) return;
      p->zBuf[p->nUsed++] = '\\';
      p->zBuf[p->nUsed++] = 'u';
      p->zBuf[p->nUsed++] = '0';
      p->zBuf[p->nUsed++] = '0';
      p->zBuf[p->nUsed++] = '0' + (c>>4);
      c = "0123456789abcdef"[c&0xf];
    }
    p->zBuf[p->nUsed++] = c;
  }
  p->zBuf[p->nUsed++] = '\'';
  assert( p->nUsed<p->nAlloc );
}

/*
** Append a function parameter value to the P21 string under 
** construction.
*/
static void p21AppendValue(
  P21String *p,                 /* Append to this P21 string */
  sqlite3_value *pValue          /* Value to append */
){
  switch( sqlite3_value_type(pValue) ){
    case SQLITE_NULL: {
      p21AppendRaw(p, "$", 1);
      break;
    }
    case SQLITE_INTEGER:
    case SQLITE_FLOAT: {
      const char *z = (const char*)sqlite3_value_text(pValue);
      /* TODO: confirm format is valid */
      u32 n = (u32)sqlite3_value_bytes(pValue);
      p21AppendRaw(p, z, n);
      break;
    }
    case SQLITE_TEXT: {
      const char *z = (const char*)sqlite3_value_text(pValue);
      u32 n = (u32)sqlite3_value_bytes(pValue);
      if( sqlite3_value_subtype(pValue)==P21_SUBTYPE ){
        p21AppendRaw(p, z, n);
      }else{
        p21AppendString(p, z, n);
      }
      break;
    }
    default: {
      if( p->bErr==0 ){
        sqlite3_result_error(p->pCtx, "P21 cannot hold BLOB values", -1);
        p->bErr = 2;
        p21Reset(p);
      }
      break;
    }
  }
}


/* Make the P21 in p the result of the SQL function.
*/
static void p21Result(P21String *p){
  if( p->bErr==0 ){
    sqlite3_result_text64(p->pCtx, p->zBuf, p->nUsed, 
                          p->bStatic ? SQLITE_TRANSIENT : sqlite3_free,
                          SQLITE_UTF8);
    p21Zero(p);
  }
  assert( p->bStatic );
}

/**************************************************************************
** Utility routines for dealing with P21Node and P21Parse objects
**************************************************************************/

/*
** Return the number of consecutive P21Node slots need to represent
** the parsed P21 at pNode.  The minimum answer is 1.  For ARRAY and
** OBJECT types, the number might be larger.
**
** Appended elements are not counted.  The value returned is the number
** by which the P21Node counter should increment in order to go to the
** next peer value.
*/
static u32 p21NodeSize(P21Node *pNode){
  return pNode->eType < P21_LIST ? 1 : pNode->n + 1;
}

/*
** Reclaim all memory allocated by a P21Parse object.  But do not
** delete the P21Parse object itself.
*/
static void p21ParseReset(P21Parse *pParse){
  sqlite3_free(pParse->aNode);
  pParse->aNode = 0;
  pParse->nNode = 0;
  pParse->nAlloc = 0;
  sqlite3_free(pParse->aUp);
  pParse->aUp = 0;
}

/*
** Free a P21Parse object that was obtained from sqlite3_malloc().
*/
static void p21ParseFree(P21Parse *pParse){
  p21ParseReset(pParse);
  sqlite3_free(pParse);
}

/*
** Convert the P21Node pNode into a pure P21 string and
** append to pOut.  Subsubstructure is also included.  Return
** the number of P21Node objects that are encoded.
*/
static void p21RenderNode(
  P21Node *pNode,               /* The node to render */
  P21String *pOut,              /* Write P21 here */
  sqlite3_value **aReplace       /* Replacement values */
){
  if( pNode->jnFlags & (PNODE_REPLACE|PNODE_PATCH) ){
    if( pNode->jnFlags & PNODE_REPLACE ){
      p21AppendValue(pOut, aReplace[pNode->u.iReplace]);
      return;
    }
    pNode = pNode->u.pPatch;
  }
  switch( pNode->eType ){
    default: {
      assert( pNode->eType==P21_EMPTY );
      p21AppendChar(pOut, '$');
      break;
    }
    case P21_ENUMERATION: {
      p21AppendRaw(pOut, pNode->u.zJContent, pNode->n);
      break;
    }
    case P21_DERIVED:  {
      p21AppendChar(pOut, '*');
      break;
    }
    case P21_BINARY: {
      p21AppendRaw(pOut, pNode->u.zJContent, pNode->n);
      break;
    }
    case P21_EID: {
      p21AppendRaw(pOut, pNode->u.zJContent, pNode->n);
      break;
    }
    case P21_STRING: {
      if( pNode->jnFlags & PNODE_RAW ){
        p21AppendString(pOut, pNode->u.zJContent, pNode->n);
        break;
      }
      /* Fall through into the next case */
    }
    case P21_REAL:
    case P21_INTEGER: {
      p21AppendRaw(pOut, pNode->u.zJContent, pNode->n);
      break;
    }
    case P21_LIST: {
      u32 j = 1;
      p21AppendChar(pOut, '(');
      for(;;){
        while( j<=pNode->n ){
          if( (pNode[j].jnFlags & PNODE_REMOVE)==0 ){
            p21AppendSeparator(pOut);
            p21RenderNode(&pNode[j], pOut, aReplace);
          }
          j += p21NodeSize(&pNode[j]);
        }
        if( (pNode->jnFlags & PNODE_APPEND)==0 ) break;
        pNode = &pNode[pNode->u.iAppend];
        j = 1;
      }
      p21AppendChar(pOut, ')');
      break;
    }
    case P21_RECORD: {
      u32 j = 1;
      p21AppendRaw(pOut, pNode->u.zJContent, pNode->n_kw);
      p21AppendChar(pOut, '(');
      for(;;){
        while( j<= pNode->n ){
          if( (pNode[j].jnFlags & PNODE_REMOVE)==0 ){
            p21AppendSeparator(pOut);
            p21RenderNode(&pNode[j], pOut, aReplace);
          }
          j += p21NodeSize(&pNode[j]);
        }
        if( (pNode->jnFlags & PNODE_APPEND)==0 ) break;
        pNode = &pNode[pNode->u.iAppend];
        j = 1;
      }
      p21AppendChar(pOut, ')');
      break;
    }
  }
}

/*
** Return a P21Node and all its descendents as a P21 string.
*/
static void p21ReturnP21(
  P21Node *pNode,            /* Node to return */
  sqlite3_context *pCtx,      /* Return value for this function */
  sqlite3_value **aReplace    /* Array of replacement values */
){
  P21String s;
  p21Init(&s, pCtx);
  p21RenderNode(pNode, &s, aReplace);
  p21Result(&s);
  sqlite3_result_subtype(pCtx, P21_SUBTYPE);
}

/*
** Translate a single byte of Hex into an integer.
** This routine only works if h really is a valid hexadecimal
** character:  0..9a..fA..F
*/
static u8 p21HexToInt(int h){
  assert( (h>='0' && h<='9') ||  (h>='a' && h<='f') ||  (h>='A' && h<='F') );
#ifdef SQLITE_EBCDIC
  h += 9*(1&~(h>>4));
#else
  h += 9*(1&(h>>6));
#endif
  return (u8)(h & 0xf);
}

/*
** Convert a 4-byte hex string into an integer
*/
static u32 p21HexToInt4(const char *z){
  u32 v;
  assert( safe_isxdigit(z[0]) );
  assert( safe_isxdigit(z[1]) );
  assert( safe_isxdigit(z[2]) );
  assert( safe_isxdigit(z[3]) );
  v = (p21HexToInt(z[0])<<12)
    + (p21HexToInt(z[1])<<8)
    + (p21HexToInt(z[2])<<4)
    + p21HexToInt(z[3]);
  return v;
}
/*
** Make the P21Node the return value of the function.
*/
static void p21Return(
  P21Node *pNode,            /* Node to return */
  sqlite3_context *pCtx,      /* Return value for this function */
  sqlite3_value **aReplace    /* Array of replacement values */
){
  switch( pNode->eType ){
    default: {
      assert( pNode->eType==P21_EMPTY );
      sqlite3_result_null(pCtx);
      break;
    }
    case P21_DERIVED: {
        assert(0);
    }
    case P21_ENUMERATION: {
        assert(0);
    }
    case P21_BINARY: {
        assert(0);
    }
    case P21_EID: {
        sqlite3_result_text(pCtx, pNode->u.zJContent, pNode->n, SQLITE_TRANSIENT);
        break;
    }
    case P21_INTEGER: {
      sqlite3_int64 i = 0;
      const char *z = pNode->u.zJContent;
      if( z[0]=='-' ){ z++; }
      while( z[0]>='0' && z[0]<='9' ){
        unsigned v = *(z++) - '0';
        if( i>=LARGEST_INT64/10 ){
          if( i>LARGEST_INT64/10 ) goto int_as_real;
          if( z[0]>='0' && z[0]<='9' ) goto int_as_real;
          if( v==9 ) goto int_as_real;
          if( v==8 ){
            if( pNode->u.zJContent[0]=='-' ){
              sqlite3_result_int64(pCtx, SMALLEST_INT64);
              goto int_done;
            }else{
              goto int_as_real;
            }
          }
        }
        i = i*10 + v;
      }
      if( pNode->u.zJContent[0]=='-' ){ i = -i; }
      sqlite3_result_int64(pCtx, i);
      int_done:
      break;
      int_as_real: /* fall through to real */;
    }
    case P21_REAL: {
      double r;
#ifdef SQLITE_AMALGAMATION
      const char *z = pNode->u.zJContent;
      sqlite3AtoF(z, &r, sqlite3Strlen30(z), SQLITE_UTF8);
#else
      r = strtod(pNode->u.zJContent, 0);
#endif
      sqlite3_result_double(pCtx, r);
      break;
    }
    case P21_STRING: {
#if 0 /* Never happens because PNODE_RAW is only set by p21_set(),
      ** p21_insert() and p21_replace() and those routines do not
      ** call p21Return() */
      if( pNode->jnFlags & PNODE_RAW ){
        sqlite3_result_text(pCtx, pNode->u.zJContent, pNode->n,
                            SQLITE_TRANSIENT);
      }else 
#endif
      assert( (pNode->jnFlags & PNODE_RAW)==0 );
      if( (pNode->jnFlags & PNODE_ESCAPE)==0 ){
        /* P21 formatted without any backslash-escapes */
        sqlite3_result_text(pCtx, pNode->u.zJContent+1, pNode->n-2,
                            SQLITE_TRANSIENT);
      }else{
        /* Translate P21 formatted string into raw text */
        u32 i;
        u32 n = pNode->n;
        const char *z = pNode->u.zJContent;
        char *zOut;
        u32 j;
        /* TODO: */
        assert(0);
        zOut = sqlite3_malloc( n+1 );
        if( zOut==0 ){
          sqlite3_result_error_nomem(pCtx);
          break;
        }
        for(i=1, j=0; i<n-1; i++){
          char c = z[i];
          if( c!='\\' ){
            zOut[j++] = c;
          }else{
            c = z[++i];
            if( c=='u' ){
              u32 v = p21HexToInt4(z+i+1);
              i += 4;
              if( v==0 ) break;
              if( v<=0x7f ){
                zOut[j++] = (char)v;
              }else if( v<=0x7ff ){
                zOut[j++] = (char)(0xc0 | (v>>6));
                zOut[j++] = 0x80 | (v&0x3f);
              }else{
                u32 vlo;
                if( (v&0xfc00)==0xd800
                  && i<n-6
                  && z[i+1]=='\\'
                  && z[i+2]=='u'
                  && ((vlo = p21HexToInt4(z+i+3))&0xfc00)==0xdc00
                ){
                  /* We have a surrogate pair */
                  v = ((v&0x3ff)<<10) + (vlo&0x3ff) + 0x10000;
                  i += 6;
                  zOut[j++] = 0xf0 | (v>>18);
                  zOut[j++] = 0x80 | ((v>>12)&0x3f);
                  zOut[j++] = 0x80 | ((v>>6)&0x3f);
                  zOut[j++] = 0x80 | (v&0x3f);
                }else{
                  zOut[j++] = 0xe0 | (v>>12);
                  zOut[j++] = 0x80 | ((v>>6)&0x3f);
                  zOut[j++] = 0x80 | (v&0x3f);
                }
              }
            }else{
              if( c=='b' ){
                c = '\b';
              }else if( c=='f' ){
                c = '\f';
              }else if( c=='n' ){
                c = '\n';
              }else if( c=='r' ){
                c = '\r';
              }else if( c=='t' ){
                c = '\t';
              }
              zOut[j++] = c;
            }
          }
        }
        zOut[j] = 0;
        sqlite3_result_text(pCtx, zOut, j, sqlite3_free);
      }
      break;
    }
    case P21_LIST:
    case P21_RECORD: {
      p21ReturnP21(pNode, pCtx, aReplace);
      break;
    }
  }
}

/* Forward reference */
static int p21ParseAddNode(P21Parse*,u32,u32,const char*);

/*
** A macro to hint to the compiler that a function should not be
** inlined.
*/
#if defined(__GNUC__)
#  define P21_NOINLINE  __attribute__((noinline))
#elif defined(_MSC_VER) && _MSC_VER>=1310
#  define P21_NOINLINE  __declspec(noinline)
#else
#  define P21_NOINLINE
#endif


static P21_NOINLINE int p21ParseAddNodeExpand(
  P21Parse *pParse,        /* Append the node to this object */
  u32 eType,                /* Node type */
  u32 n,                    /* Content size or sub-node count */
  const char *zContent      /* Content */
){
  u32 nNew;
  P21Node *pNew;
  assert( pParse->nNode>=pParse->nAlloc );
  if( pParse->oom ) return -1;
  nNew = pParse->nAlloc*2 + 10;
  pNew = sqlite3_realloc64(pParse->aNode, sizeof(P21Node)*nNew);
  if( pNew==0 ){
    pParse->oom = 1;
    return -1;
  }
  pParse->nAlloc = nNew;
  pParse->aNode = pNew;
  assert( pParse->nNode<pParse->nAlloc );
  return p21ParseAddNode(pParse, eType, n, zContent);
}

/*
** Create a new P21Node instance based on the arguments and append that
** instance to the P21Parse.  Return the index in pParse->aNode[] of the
** new node, or -1 if a memory allocation fails.
*/
static int p21ParseAddNode(
  P21Parse *pParse,        /* Append the node to this object */
  u32 eType,                /* Node type */
  u32 n,                    /* Content size or sub-node count */
  const char *zContent      /* Content */
){
  P21Node *p;
  if( pParse->nNode>=pParse->nAlloc ){
    return p21ParseAddNodeExpand(pParse, eType, n, zContent);
  }
  p = &pParse->aNode[pParse->nNode];
  p->eType = (u8)eType;
  p->jnFlags = 0;
  p->n = n;
  p->u.zJContent = zContent;
  return pParse->nNode++;
}

/*
** Return true if z[] begins with 4 (or more) hexadecimal digits
*/
static int p21Is4Hex(const char *z){
  int i;
  for(i=0; i<4; i++) if( !safe_isxdigit(z[i]) ) return 0;
  return 1;
}

/*
** Parse P21 value which begins at pParse->zP21[i].  Return the
** index of the first character past the end of the value parsed.
**
** Return negative for a syntax error.
*/
static int p21ParseValue(P21Parse *pParse, u32 i) {
    static int cxtStack[P21_MAX_DEPTH];
    const unsigned char *sp, *cur, *mrk, *tok, *end;
    /*!stags:re2c format = 'const unsigned char *@@;'; */
    int *piThis, x;
    u32 n;
    P21Node *pNode;
    
    sp = cur = tok = &pParse->zP21[i];
    piThis = cxtStack + pParse->iDepth;
    
/*!re2c
    re2c:yyfill:enable = 0;
    re2c:flags:tags = 1;
    re2c:define:YYCTYPE = 'unsigned char';
    re2c:define:YYCURSOR = 'cur';
    re2c:define:YYMARKER = 'mrk';

    ascii_encoding = [][!"*$%&.#+,\-()?/:;<=>@{}|^`~0-9a-zA-Z_ ] | "''" | "\\" ;
    page_encoding  = "\\" [A-I] "\\" | "\\S\\" [][!"'*$%&.#+,\-()?/:;<=>@{}|^`~0-9a-zA-Z_\\ ] ;
    hex_encoding   = "\\X2\\" ([0-9A-F]{4})+ "\\X0\\" | "\\X4\\" ([0-9A-F]{8})+ "\\X0\\" ;
    byte_encoding  = "\\X\\" [0-9A-F]{2} ;

    WS = [ \t\r\n] ;
    KEYWORD = "!"? [A-Za-z_] [0-9A-Za-z_]* ;
    REAL = [+-]* [0-9] [0-9]* "." [0-9]* ("E" [+-]* [0-9] [0-9]*)? ;
    INTEGER = [+-]* [0-9] [0-9]* ;
    STRING = "'" (ascii_encoding | page_encoding | hex_encoding | byte_encoding )* "'" ;
    BINARY = '"' [0-3] [0-9A-F]* '"' ;
    ENUMERATION = "." [A-Z_] [A-Z0-9_]* "." ;
    EID = "#" [0-9]+ ;
 */

start:
    
    tok = cur;    
/*!re2c
    WS+ {
        goto start;
    }
    "(" {
        /* (complex_entity_instance) parameter_list */
        *piThis = p21ParseAddNode(pParse, P21_LIST, 0, 0);
        if (*piThis < 0) return -1;
        
        if( ++pParse->iDepth > P21_MAX_DEPTH ) return -1;
        piThis = cxtStack + pParse->iDepth;
        goto keywords;
    }
    "" {
        /* (simple_entity_instance) parameter_list */
        *piThis = p21ParseAddNode(pParse, P21_RECORD, 0, 0);
        if (*piThis < 0) return -1;
        
        if( ++pParse->iDepth > P21_MAX_DEPTH ) return -1;
        piThis = cxtStack + pParse->iDepth;
        goto params1;
    }
 */

keywords:
    tok = cur;    

/*!re2c
    WS+ {
        goto keywords;
    }
    KEYWORD @end WS* "(" {
        *piThis = p21ParseAddNode(pParse, P21_RECORD, 0, tok);
        if (*piThis < 0) return -1;
        pParse->aNode[*piThis].n_kw = (u16)(end - tok);
        
        if( ++pParse->iDepth > P21_MAX_DEPTH ) return -1;
        piThis = cxtStack + pParse->iDepth;
        goto params2;
    }
    ")" {
        piThis = cxtStack + --pParse->iDepth;
        pNode = pParse->aNode + *piThis;
        assert(pNode->eType == P21_LIST);
        
        pNode->n = pParse->nNode - (u32)*piThis - 1;
        goto eol;
    }
    "" {
        /* fix-up and revert to P21_RECORD */
        pNode = pParse->aNode + *(piThis - 1);
        pNode->eType = P21_RECORD;
        assert(pParse->iDepth == 1);
        goto params1;
    }
 */

params1:
    tok = cur;    

/*!re2c
    WS+ {
        goto params1;
    }
    KEYWORD @end WS* "(" {
        *piThis = p21ParseAddNode(pParse, P21_RECORD, 0, tok);
        if (*piThis < 0) return -1;
        pParse->aNode[*piThis].n_kw = (u16)(end - tok);
        
        if( ++pParse->iDepth > P21_MAX_DEPTH ) return -1;
        piThis = cxtStack + pParse->iDepth;
        goto params1;
    }
    "(" {
        *piThis = p21ParseAddNode(pParse, P21_LIST, 0, 0);
        if (*piThis < 0) return -1;
        
        if( ++pParse->iDepth > P21_MAX_DEPTH ) return -1;
        piThis = cxtStack + pParse->iDepth;
        goto params1;
    }
    "," {
        goto params1;
    }
    REAL {
        p21ParseAddNode(pParse, P21_REAL, cur - tok, tok);
        goto params1;
    }
    INTEGER {
        p21ParseAddNode(pParse, P21_INTEGER, cur - tok, tok);
        goto params1;
    }
    STRING {
        p21ParseAddNode(pParse, P21_STRING, cur - tok, tok);
        goto params1;
    }
    BINARY {
        p21ParseAddNode(pParse, P21_BINARY, cur - tok, tok);
        goto params1;
    }
    ENUMERATION {
        p21ParseAddNode(pParse, P21_ENUMERATION, cur - tok, tok);
        goto params1;
    }
    EID {
        p21ParseAddNode(pParse, P21_EID, cur - tok, tok);
        goto params1;
    }
    "$" {
        p21ParseAddNode(pParse, P21_EMPTY, cur - tok, tok);
        goto params1;
    }
    "*" {
        p21ParseAddNode(pParse, P21_DERIVED, cur - tok, tok);
        goto params1;
    }
    ")" {
        piThis = cxtStack + --pParse->iDepth;
        pNode = pParse->aNode + *piThis;
        pNode->n = pParse->nNode - (u32)*piThis - 1;
        goto params1;
    }
    "" {
        if (pParse->iDepth) --pParse->iDepth;
        piThis = cxtStack + pParse->iDepth;
        pNode = pParse->aNode + *piThis;
        assert(pNode->eType == P21_RECORD);
        pNode->n = pParse->nNode - (u32)*piThis - 1;
        goto eol;
    }
 */

params2:
    tok = cur;    

/*!re2c
    WS+ {
        goto params2;
    }
    KEYWORD @end WS* "(" {
        *piThis = p21ParseAddNode(pParse, P21_RECORD, 0, tok);
        if (*piThis < 0) return -1;
        pParse->aNode[*piThis].n_kw = (u16)(end - tok);
        
        if( ++pParse->iDepth > P21_MAX_DEPTH ) return -1;
        piThis = cxtStack + pParse->iDepth;
        goto params2;
    }
    "(" {
        *piThis = p21ParseAddNode(pParse, P21_LIST, 0, 0);
        if (*piThis < 0) return -1;
        
        if( ++pParse->iDepth > P21_MAX_DEPTH ) return -1;
        piThis = cxtStack + pParse->iDepth;
        goto params2;
    }
    "," {
        goto params2;
    }
    REAL {
        p21ParseAddNode(pParse, P21_REAL, cur - tok, tok);
        goto params2;
    }
    INTEGER {
        p21ParseAddNode(pParse, P21_INTEGER, cur - tok, tok);
        goto params2;
    }
    STRING {
        p21ParseAddNode(pParse, P21_STRING, cur - tok, tok);
        goto params2;
    }
    BINARY {
        p21ParseAddNode(pParse, P21_BINARY, cur - tok, tok);
        goto params2;
    }
    ENUMERATION {
        p21ParseAddNode(pParse, P21_ENUMERATION, cur - tok, tok);
        goto params2;
    }
    EID {
        p21ParseAddNode(pParse, P21_EID, cur - tok, tok);
        goto params2;
    }
    "$" {
        p21ParseAddNode(pParse, P21_EMPTY, cur - tok, tok);
        goto params2;
    }
    "*" {
        p21ParseAddNode(pParse, P21_DERIVED, cur - tok, tok);
        goto params2;
    }
    ")" {
        piThis = cxtStack + --pParse->iDepth;
        pNode = pParse->aNode + *piThis;
        pNode->n = pParse->nNode - (u32)*piThis - 1;
        if (pParse->iDepth > 1) {
            goto params2;
        } else {
            goto keywords;
        }
    }
    /* continue to next block for error handling */
 */

eol:
    tok = cur;    

/*!re2c
    [\x00] {
        return cur - sp;
    }
    * {
        return -1;
    }
 */

}


/*
** Parse a complete P21 string.  Return 0 on success or non-zero if there
** are any errors.  If an error occurs, free all memory associated with
** pParse.
**
** pParse is uninitialized when this routine is called.
*/
static int p21Parse(
  P21Parse *pParse,           /* Initialize and fill this P21Parse object */
  sqlite3_context *pCtx,       /* Report errors here */
  const char *zP21            /* Input P21 text to be parsed */
){
  int i;
  memset(pParse, 0, sizeof(*pParse));
  if( zP21==0 ) return 1;
  pParse->zP21 = zP21;
  i = p21ParseValue(pParse, 0);
  if( pParse->oom ) i = -1;
  if( i>0 ){
    assert( pParse->iDepth==0 );
  }
  if( i<=0 ){
    if( pCtx!=0 ){
      if( pParse->oom ){
        sqlite3_result_error_nomem(pCtx);
      }else{
        sqlite3_result_error(pCtx, "malformed P21", -1);
      }
    }
    p21ParseReset(pParse);
    return 1;
  }
  return 0;
}

/* Mark node i of pParse as being a child of iParent.  Call recursively
** to fill in all the descendants of node i.
*/
static void p21ParseFillInParentage(P21Parse *pParse, u32 i, u32 iParent){
  P21Node *pNode = &pParse->aNode[i];
  u32 j;
  pParse->aUp[i] = iParent;
  switch( pNode->eType ){
    case P21_RECORD:
    case P21_LIST: {
      for(j=1; j<=pNode->n; j += p21NodeSize(pNode+j)){
        p21ParseFillInParentage(pParse, i+j, i);
      }
      break;
    }
    default: {
      break;
    }
  }
}

/*
** Compute the parentage of all nodes in a completed parse.
*/
static int p21ParseFindParents(P21Parse *pParse){
  u32 *aUp;
  assert( pParse->aUp==0 );
  aUp = pParse->aUp = sqlite3_malloc64( sizeof(u32)*pParse->nNode );
  if( aUp==0 ){
    pParse->oom = 1;
    return SQLITE_NOMEM;
  }
  p21ParseFillInParentage(pParse, 0, 0);
  return SQLITE_OK;
}

/*
** Magic number used for the P21 parse cache in sqlite3_get_auxdata()
*/
#define P21_CACHE_ID  (-429938)  /* First cache entry */
#define P21_CACHE_SZ  4          /* Max number of cache entries */

/*
** Obtain a complete parse of the P21 found in the first argument
** of the argv array.  Use the sqlite3_get_auxdata() cache for this
** parse if it is available.  If the cache is not available or if it
** is no longer valid, parse the P21 again and return the new parse,
** and also register the new parse so that it will be available for
** future sqlite3_get_auxdata() calls.
*/
static P21Parse *p21ParseCached(
  sqlite3_context *pCtx,
  sqlite3_value **argv,
  sqlite3_context *pErrCtx
){
  const char *zP21 = (const char*)sqlite3_value_text(argv[0]);
  int nP21 = sqlite3_value_bytes(argv[0]);
  P21Parse *p;
  P21Parse *pMatch = 0;
  int iKey;
  int iMinKey = 0;
  u32 iMinHold = 0xffffffff;
  u32 iMaxHold = 0;
  if( zP21==0 ) return 0;
  for(iKey=0; iKey<P21_CACHE_SZ; iKey++){
    p = (P21Parse*)sqlite3_get_auxdata(pCtx, P21_CACHE_ID+iKey);
    if( p==0 ){
      iMinKey = iKey;
      break;
    }
    if( pMatch==0
     && p->nP21==nP21
     && memcmp(p->zP21,zP21,nP21)==0
    ){
      p->nErr = 0;
      pMatch = p;
    }else if( p->iHold<iMinHold ){
      iMinHold = p->iHold;
      iMinKey = iKey;
    }
    if( p->iHold>iMaxHold ){
      iMaxHold = p->iHold;
    }
  }
  if( pMatch ){
    pMatch->nErr = 0;
    pMatch->iHold = iMaxHold+1;
    return pMatch;
  }
  p = sqlite3_malloc64( sizeof(*p) + nP21 + 1 );
  if( p==0 ){
    sqlite3_result_error_nomem(pCtx);
    return 0;
  }
  memset(p, 0, sizeof(*p));
  p->zP21 = (char*)&p[1];
  memcpy((char*)p->zP21, zP21, nP21+1);
  if( p21Parse(p, pErrCtx, p->zP21) ){
    sqlite3_free(p);
    return 0;
  }
  p->nP21 = nP21;
  p->iHold = iMaxHold+1;
  sqlite3_set_auxdata(pCtx, P21_CACHE_ID+iMinKey, p,
                      (void(*)(void*))p21ParseFree);
  return (P21Parse*)sqlite3_get_auxdata(pCtx, P21_CACHE_ID+iMinKey);
}

/*
** Compare the OBJECT label at pNode against zKey,nKey.  Return true on
** a match.
*/
static int p21LabelCompare(P21Node *pNode, const char *zKey, u32 nKey){
  if( pNode->jnFlags & PNODE_RAW ){
    if( pNode->n!=nKey ) return 0;
    return strncmp(pNode->u.zJContent, zKey, nKey)==0;
  }else{
    if( pNode->n!=nKey+2 ) return 0;
    return strncmp(pNode->u.zJContent+1, zKey, nKey)==0;
  }
}

/* forward declaration */
static P21Node *p21LookupAppend(P21Parse*,const char*,int*,const char**);

/*
** Search along zPath to find the node specified.  Return a pointer
** to that node, or NULL if zPath is malformed or if there is no such
** node.
**
** If pApnd!=0, then try to append new nodes to complete zPath if it is
** possible to do so and if no existing node corresponds to zPath.  If
** new nodes are appended *pApnd is set to 1.
*/
static P21Node *p21LookupStep(
  P21Parse *pParse,      /* The P21 to search */
  u32 iRoot,              /* Begin the search at this node */
  const char *zPath,      /* The path to search */
  int *pApnd,             /* Append nodes to complete path if not NULL */
  const char **pzErr      /* Make *pzErr point to any syntax error in zPath */
){
  u32 i, j, nKey;
  const char *zKey;
  P21Node *pRoot = &pParse->aNode[iRoot];
  if( zPath[0]==0 ) return pRoot;
  if( pRoot->jnFlags & PNODE_REPLACE ) return 0;
  if( zPath[0]=='.' ){
    if( pRoot->eType!=P21_RECORD ) return 0;
    zPath++;
    if( zPath[0]=='"' ){
      zKey = zPath + 1;
      for(i=1; zPath[i] && zPath[i]!='"'; i++){}
      nKey = i-1;
      if( zPath[i] ){
        i++;
      }else{
        *pzErr = zPath;
        return 0;
      }
    }else{
      zKey = zPath;
      for(i=0; zPath[i] && zPath[i]!='.' && zPath[i]!='['; i++){}
      nKey = i;
    }
    if( nKey==0 ){
      *pzErr = zPath;
      return 0;
    }
    j = 1;
    for(;;){
      while( j<=pRoot->n ){
        if( p21LabelCompare(pRoot+j, zKey, nKey) ){
          return p21LookupStep(pParse, iRoot+j+1, &zPath[i], pApnd, pzErr);
        }
        j++;
        j += p21NodeSize(&pRoot[j]);
      }
      if( (pRoot->jnFlags & PNODE_APPEND)==0 ) break;
      iRoot += pRoot->u.iAppend;
      pRoot = &pParse->aNode[iRoot];
      j = 1;
    }
    if( pApnd ){
      u32 iStart, iLabel;
      P21Node *pNode;
      iStart = p21ParseAddNode(pParse, P21_RECORD, 2, 0);
      iLabel = p21ParseAddNode(pParse, P21_STRING, nKey, zKey);
      zPath += i;
      pNode = p21LookupAppend(pParse, zPath, pApnd, pzErr);
      if( pParse->oom ) return 0;
      if( pNode ){
        pRoot = &pParse->aNode[iRoot];
        pRoot->u.iAppend = iStart - iRoot;
        pRoot->jnFlags |= PNODE_APPEND;
        pParse->aNode[iLabel].jnFlags |= PNODE_RAW;
      }
      return pNode;
    }
  }else if( zPath[0]=='[' ){
    i = 0;
    j = 1;
    while( safe_isdigit(zPath[j]) ){
      i = i*10 + zPath[j] - '0';
      j++;
    }
    if( j<2 || zPath[j]!=']' ){
      if( zPath[1]=='#' ){
        P21Node *pBase = pRoot;
        int iBase = iRoot;
        if( pRoot->eType!=P21_LIST && pRoot->eType!=P21_RECORD) return 0;
        for(;;){
          while( j<=pBase->n ){
            if( (pBase[j].jnFlags & PNODE_REMOVE)==0 ) i++;
            j += p21NodeSize(&pBase[j]);
          }
          if( (pBase->jnFlags & PNODE_APPEND)==0 ) break;
          iBase += pBase->u.iAppend;
          pBase = &pParse->aNode[iBase];
          j = 1;
        }
        j = 2;
        if( zPath[2]=='-' && safe_isdigit(zPath[3]) ){
          unsigned int x = 0;
          j = 3;
          do{
            x = x*10 + zPath[j] - '0';
            j++;
          }while( safe_isdigit(zPath[j]) );
          if( x>i ) return 0;
          i -= x;
        }
        if( zPath[j]!=']' ){
          *pzErr = zPath;
          return 0;
        }
      }else{
        *pzErr = zPath;
        return 0;
      }
    }
    if( pRoot->eType!=P21_LIST && pRoot->eType!=P21_RECORD ) return 0;
    zPath += j + 1;
    j = 1;
    for(;;){
      while( j<=pRoot->n && (i>0 || (pRoot[j].jnFlags & PNODE_REMOVE)!=0) ){
        if( (pRoot[j].jnFlags & PNODE_REMOVE)==0 ) i--;
        j += p21NodeSize(&pRoot[j]);
      }
      if( (pRoot->jnFlags & PNODE_APPEND)==0 ) break;
      iRoot += pRoot->u.iAppend;
      pRoot = &pParse->aNode[iRoot];
      j = 1;
    }
    if( j<=pRoot->n ){
      return p21LookupStep(pParse, iRoot+j, zPath, pApnd, pzErr);
    }
    if( i==0 && pApnd ){
      u32 iStart;
      P21Node *pNode;
      iStart = p21ParseAddNode(pParse, P21_LIST, 1, 0);
      pNode = p21LookupAppend(pParse, zPath, pApnd, pzErr);
      if( pParse->oom ) return 0;
      if( pNode ){
        pRoot = &pParse->aNode[iRoot];
        pRoot->u.iAppend = iStart - iRoot;
        pRoot->jnFlags |= PNODE_APPEND;
      }
      return pNode;
    }
  }else{
    *pzErr = zPath;
  }
  return 0;
}

/*
** Append content to pParse that will complete zPath.  Return a pointer
** to the inserted node, or return NULL if the append fails.
*/
static P21Node *p21LookupAppend(
  P21Parse *pParse,     /* Append content to the P21 parse */
  const char *zPath,     /* Description of content to append */
  int *pApnd,            /* Set this flag to 1 */
  const char **pzErr     /* Make this point to any syntax error */
){
  *pApnd = 1;
  if( zPath[0]==0 ){
    p21ParseAddNode(pParse, P21_EMPTY, 0, 0);
    return pParse->oom ? 0 : &pParse->aNode[pParse->nNode-1];
  }
  if( zPath[0]=='.' ){
    p21ParseAddNode(pParse, P21_RECORD, 0, 0);
  }else if( strncmp(zPath,"[0]",3)==0 ){
    p21ParseAddNode(pParse, P21_LIST, 0, 0);
  }else{
    return 0;
  }
  if( pParse->oom ) return 0;
  return p21LookupStep(pParse, pParse->nNode-1, zPath, pApnd, pzErr);
}

/*
** Return the text of a syntax error message on a P21 path.  Space is
** obtained from sqlite3_malloc().
*/
static char *p21PathSyntaxError(const char *zErr){
  return sqlite3_mprintf("P21 path error near '%q'", zErr);
}

/*
** Do a node lookup using zPath.  Return a pointer to the node on success.
** Return NULL if not found or if there is an error.
**
** On an error, write an error message into pCtx and increment the
** pParse->nErr counter.
**
** If pApnd!=NULL then try to append missing nodes and set *pApnd = 1 if
** nodes are appended.
*/
static P21Node *p21Lookup(
  P21Parse *pParse,      /* The P21 to search */
  const char *zPath,      /* The path to search */
  int *pApnd,             /* Append nodes to complete path if not NULL */
  sqlite3_context *pCtx   /* Report errors here, if not NULL */
){
  const char *zErr = 0;
  P21Node *pNode = 0;
  char *zMsg;

  if( zPath==0 ) return 0;
  if( zPath[0]!='$' ){
    zErr = zPath;
    goto lookup_err;
  }
  zPath++;
  pNode = p21LookupStep(pParse, 0, zPath, pApnd, &zErr);
  if( zErr==0 ) return pNode;

lookup_err:
  pParse->nErr++;
  assert( zErr!=0 && pCtx!=0 );
  zMsg = p21PathSyntaxError(zErr);
  if( zMsg ){
    sqlite3_result_error(pCtx, zMsg, -1);
    sqlite3_free(zMsg);
  }else{
    sqlite3_result_error_nomem(pCtx);
  }
  return 0;
}


/*
** Report the wrong number of arguments for p21_insert(), p21_replace()
** or p21_set().
*/
static void p21WrongNumArgs(
  sqlite3_context *pCtx,
  const char *zFuncName
){
  char *zMsg = sqlite3_mprintf("p21_%s() needs an odd number of arguments",
                               zFuncName);
  sqlite3_result_error(pCtx, zMsg, -1);
  sqlite3_free(zMsg);     
}

/*
** Mark all NULL entries in the Object passed in as PNODE_REMOVE.
*/
static void p21RemoveAllNulls(P21Node *pNode){
  int i, n;
  assert( pNode->eType==P21_RECORD );
  n = pNode->n;
  for(i=2; i<=n; i += p21NodeSize(&pNode[i])+1){
    switch( pNode[i].eType ){
      case P21_EMPTY:
        pNode[i].jnFlags |= PNODE_REMOVE;
        break;
      case P21_RECORD:
        p21RemoveAllNulls(&pNode[i]);
        break;
    }
  }
}


/****************************************************************************
** SQL functions used for testing and debugging
****************************************************************************/

#ifdef SQLITE_DEBUG
/*
** The p21_parse(P21) function returns a string which describes
** a parse of the P21 provided.  Or it returns NULL if P21 is not
** well-formed.
*/
static void p21ParseFunc(
  sqlite3_context *ctx,
  int argc,
  sqlite3_value **argv
){
  P21String s;       /* Output string - not real P21 */
  P21Parse x;        /* The parse */
  u32 i;

  assert( argc==1 );
  if( p21Parse(&x, ctx, (const char*)sqlite3_value_text(argv[0])) ) return;
  p21ParseFindParents(&x);
  p21Init(&s, ctx);
  for(i=0; i<x.nNode; i++){
    const char *zType;
    zType = p21Type[x.aNode[i].eType];
    p21Printf(100, &s,"node %3u: %7s n=%-4d up=%-4d",
               i, zType, x.aNode[i].n, x.aUp[i]);
    if( x.aNode[i].u.zJContent!=0 && x.aNode[i].eType != P21_RECORD ){
      p21AppendRaw(&s, " ", 1);
      p21AppendRaw(&s, x.aNode[i].u.zJContent, x.aNode[i].n);
    } else if ( x.aNode[i].u.zJContent!=0 && x.aNode[i].eType == P21_RECORD ) {
      p21AppendRaw(&s, " ", 1);
      p21AppendRaw(&s, x.aNode[i].u.zJContent, x.aNode[i].n_kw);        
    }
    p21AppendRaw(&s, "\n", 1);
  }
  p21ParseReset(&x);
  p21Result(&s);
}

/*
** The p21_test1(P21) function return true (1) if the input is P21
** text generated by another p21 function.  It returns (0) if the input
** is not known to be P21.
*/
static void p21Test1Func(
  sqlite3_context *ctx,
  int argc,
  sqlite3_value **argv
){
  UNUSED_PARAM(argc);
  sqlite3_result_int(ctx, sqlite3_value_subtype(argv[0])==P21_SUBTYPE);
}
#endif /* SQLITE_DEBUG */

/****************************************************************************
** Scalar SQL function implementations
****************************************************************************/

/*
** Implementation of the p21_QUOTE(VALUE) function.  Return a P21 value
** corresponding to the SQL value input.  Mostly this means putting 
** double-quotes around strings and returning the unquoted string "null"
** when given a NULL input.
*/
static void p21QuoteFunc(
  sqlite3_context *ctx,
  int argc,
  sqlite3_value **argv
){
  P21String jx;
  UNUSED_PARAM(argc);

  p21Init(&jx, ctx);
  p21AppendValue(&jx, argv[0]);
  p21Result(&jx);
  sqlite3_result_subtype(ctx, P21_SUBTYPE);
}

/*
** Implementation of the p21_array(VALUE,...) function.  Return a P21
** array that contains all values given in arguments.  Or if any argument
** is a BLOB, throw an error.
*/
static void p21ArrayFunc(
  sqlite3_context *ctx,
  int argc,
  sqlite3_value **argv
){
  int i;
  P21String jx;

  p21Init(&jx, ctx);
  p21AppendChar(&jx, '[');
  for(i=0; i<argc; i++){
    p21AppendSeparator(&jx);
    p21AppendValue(&jx, argv[i]);
  }
  p21AppendChar(&jx, ']');
  p21Result(&jx);
  sqlite3_result_subtype(ctx, P21_SUBTYPE);
}


/*
** p21_array_length(P21)
** p21_array_length(P21, PATH)
**
** Return the number of elements in the top-level P21 array.  
** Return 0 if the input is not a well-formed P21 array.
*/
static void p21ArrayLengthFunc(
  sqlite3_context *ctx,
  int argc,
  sqlite3_value **argv
){
  P21Parse *p;          /* The parse */
  sqlite3_int64 n = 0;
  u32 i;
  P21Node *pNode;

  p = p21ParseCached(ctx, argv, ctx);
  if( p==0 ) return;
  assert( p->nNode );
  if( argc==2 ){
    const char *zPath = (const char*)sqlite3_value_text(argv[1]);
    pNode = p21Lookup(p, zPath, 0, ctx);
  }else{
    pNode = p->aNode;
  }
  if( pNode==0 ){
    return;
  }
  if( pNode->eType==P21_LIST ){
    assert( (pNode->jnFlags & PNODE_APPEND)==0 );
    for(i=1; i<=pNode->n; n++){
      i += p21NodeSize(&pNode[i]);
    }
  }
  sqlite3_result_int64(ctx, n);
}

/*
** p21_extract(P21, PATH, ...)
**
** Return the element described by PATH.  Return NULL if there is no
** PATH element.  If there are multiple PATHs, then return a P21 array
** with the result from each path.  Throw an error if the P21 or any PATH
** is malformed.
*/
static void p21ExtractFunc(
  sqlite3_context *ctx,
  int argc,
  sqlite3_value **argv
){
  P21Parse *p;          /* The parse */
  P21Node *pNode;
  const char *zPath;
  P21String jx;
  int i;

  if( argc<2 ) return;
  p = p21ParseCached(ctx, argv, ctx);
  if( p==0 ) return;
  p21Init(&jx, ctx);
  p21AppendChar(&jx, '[');
  for(i=1; i<argc; i++){
    zPath = (const char*)sqlite3_value_text(argv[i]);
    pNode = p21Lookup(p, zPath, 0, ctx);
    if( p->nErr ) break;
    if( argc>2 ){
      p21AppendSeparator(&jx);
      if( pNode ){
        p21RenderNode(pNode, &jx, 0);
      }else{
        p21AppendRaw(&jx, "null", 4);
      }
    }else if( pNode ){
      p21Return(pNode, ctx, 0);
    }
  }
  if( argc>2 && i==argc ){
    p21AppendChar(&jx, ']');
    p21Result(&jx);
    sqlite3_result_subtype(ctx, P21_SUBTYPE);
  }
  p21Reset(&jx);
}

#if 0
/* TODO: a MergeRecord function could be useful
 */
static P21Node *p21MergePatch(
  P21Parse *pParse,   /* The P21 parser that contains the TARGET */
  u32 iTarget,         /* Node of the TARGET in pParse */
  P21Node *pPatch     /* The PATCH */
){
  u32 i, j;
  u32 iRoot;
  P21Node *pTarget;
  if( pPatch->eType!=P21_RECORD ){
    return pPatch;
  }
  assert( iTarget<pParse->nNode );
  pTarget = &pParse->aNode[iTarget];
  assert( (pPatch->jnFlags & PNODE_APPEND)==0 );
  if( pTarget->eType!=P21_RECORD ){
    p21RemoveAllNulls(pPatch);
    return pPatch;
  }
  iRoot = iTarget;
  for(i=1; i<pPatch->n; i += p21NodeSize(&pPatch[i+1])+1){
    u32 nKey;
    const char *zKey;
    assert( pPatch[i].eType==P21_STRING );
    assert( pPatch[i].jnFlags & PNODE_LABEL );
    nKey = pPatch[i].n;
    zKey = pPatch[i].u.zJContent;
    assert( (pPatch[i].jnFlags & PNODE_RAW)==0 );
    for(j=1; j<pTarget->n; j += p21NodeSize(&pTarget[j+1])+1 ){
      assert( pTarget[j].eType==P21_STRING );
      assert( pTarget[j].jnFlags & PNODE_LABEL );
      assert( (pPatch[i].jnFlags & PNODE_RAW)==0 );
      if( pTarget[j].n==nKey && strncmp(pTarget[j].u.zJContent,zKey,nKey)==0 ){
        if( pTarget[j+1].jnFlags & (PNODE_REMOVE|PNODE_PATCH) ) break;
        if( pPatch[i+1].eType==P21_EMPTY ){
          pTarget[j+1].jnFlags |= PNODE_REMOVE;
        }else{
          P21Node *pNew = p21MergePatch(pParse, iTarget+j+1, &pPatch[i+1]);
          if( pNew==0 ) return 0;
          pTarget = &pParse->aNode[iTarget];
          if( pNew!=&pTarget[j+1] ){
            pTarget[j+1].u.pPatch = pNew;
            pTarget[j+1].jnFlags |= PNODE_PATCH;
          }
        }
        break;
      }
    }
    if( j>=pTarget->n && pPatch[i+1].eType!=P21_EMPTY ){
      int iStart, iPatch;
      iStart = p21ParseAddNode(pParse, P21_RECORD, 2, 0);
      p21ParseAddNode(pParse, P21_STRING, nKey, zKey);
      iPatch = p21ParseAddNode(pParse, P21_TRUE, 0, 0);
      if( pParse->oom ) return 0;
      p21RemoveAllNulls(pPatch);
      pTarget = &pParse->aNode[iTarget];
      pParse->aNode[iRoot].jnFlags |= PNODE_APPEND;
      pParse->aNode[iRoot].u.iAppend = iStart - iRoot;
      iRoot = iStart;
      pParse->aNode[iPatch].jnFlags |= PNODE_PATCH;
      pParse->aNode[iPatch].u.pPatch = &pPatch[i+1];
    }
  }
  return pTarget;
}

 
/*
** Implementation of the json_mergepatch(JSON1,JSON2) function.  Return a P21
** object that is the result of running the RFC 7396 MergePatch() algorithm
** on the two arguments.
*/
static void p21PatchFunc(
  sqlite3_context *ctx,
  int argc,
  sqlite3_value **argv
){
  P21Parse x;     /* The P21 that is being patched */
  P21Parse y;     /* The patch */
  P21Node *pResult;   /* The result of the merge */

  UNUSED_PARAM(argc);
  if( p21Parse(&x, ctx, (const char*)sqlite3_value_text(argv[0])) ) return;
  if( p21Parse(&y, ctx, (const char*)sqlite3_value_text(argv[1])) ){
    p21ParseReset(&x);
    return;
  }
  pResult = p21MergePatch(&x, 0, y.aNode);
  assert( pResult!=0 || x.oom );
  if( pResult ){
    p21ReturnP21(pResult, ctx, 0);
  }else{
    sqlite3_result_error_nomem(ctx);
  }
  p21ParseReset(&x);
  p21ParseReset(&y);
}
#endif

/*
** Implementation of the p21_object(NAME,VALUE,...) function.  Return a P21
** object that contains all name/value given in arguments.  Or if any name
** is not a string or if any value is a BLOB, throw an error.
*/
static void p21ObjectFunc(
  sqlite3_context *ctx,
  int argc,
  sqlite3_value **argv
){
  int i;
  P21String jx;
  const char *z;
  u32 n;

  if( argc&1 ){
    sqlite3_result_error(ctx, "p21_object() requires an even number "
                                  "of arguments", -1);
    return;
  }
  p21Init(&jx, ctx);
  p21AppendChar(&jx, '{');
  for(i=0; i<argc; i+=2){
    if( sqlite3_value_type(argv[i])!=SQLITE_TEXT ){
      sqlite3_result_error(ctx, "p21_object() labels must be TEXT", -1);
      p21Reset(&jx);
      return;
    }
    p21AppendSeparator(&jx);
    z = (const char*)sqlite3_value_text(argv[i]);
    n = (u32)sqlite3_value_bytes(argv[i]);
    p21AppendString(&jx, z, n);
    p21AppendChar(&jx, ':');
    p21AppendValue(&jx, argv[i+1]);
  }
  p21AppendChar(&jx, '}');
  p21Result(&jx);
  sqlite3_result_subtype(ctx, P21_SUBTYPE);
}


/*
** p21_remove(P21, PATH, ...)
**
** Remove the named elements from P21 and return the result.  malformed
** P21 or PATH arguments result in an error.
*/
static void p21RemoveFunc(
  sqlite3_context *ctx,
  int argc,
  sqlite3_value **argv
){
  P21Parse x;          /* The parse */
  P21Node *pNode;
  const char *zPath;
  u32 i;

  if( argc<1 ) return;
  if( p21Parse(&x, ctx, (const char*)sqlite3_value_text(argv[0])) ) return;
  assert( x.nNode );
  for(i=1; i<(u32)argc; i++){
    zPath = (const char*)sqlite3_value_text(argv[i]);
    if( zPath==0 ) goto remove_done;
    pNode = p21Lookup(&x, zPath, 0, ctx);
    if( x.nErr ) goto remove_done;
    if( pNode ) pNode->jnFlags |= PNODE_REMOVE;
  }
  if( (x.aNode[0].jnFlags & PNODE_REMOVE)==0 ){
    p21ReturnP21(x.aNode, ctx, 0);
  }
remove_done:
  p21ParseReset(&x);
}

/*
** p21_replace(P21, PATH, VALUE, ...)
**
** Replace the value at PATH with VALUE.  If PATH does not already exist,
** this routine is a no-op.  If P21 or PATH is malformed, throw an error.
*/
static void p21ReplaceFunc(
  sqlite3_context *ctx,
  int argc,
  sqlite3_value **argv
){
  P21Parse x;          /* The parse */
  P21Node *pNode;
  const char *zPath;
  u32 i;

  if( argc<1 ) return;
  if( (argc&1)==0 ) {
    p21WrongNumArgs(ctx, "replace");
    return;
  }
  if( p21Parse(&x, ctx, (const char*)sqlite3_value_text(argv[0])) ) return;
  assert( x.nNode );
  for(i=1; i<(u32)argc; i+=2){
    zPath = (const char*)sqlite3_value_text(argv[i]);
    pNode = p21Lookup(&x, zPath, 0, ctx);
    if( x.nErr ) goto replace_err;
    if( pNode ){
      pNode->jnFlags |= (u8)PNODE_REPLACE;
      pNode->u.iReplace = i + 1;
    }
  }
  if( x.aNode[0].jnFlags & PNODE_REPLACE ){
    sqlite3_result_value(ctx, argv[x.aNode[0].u.iReplace]);
  }else{
    p21ReturnP21(x.aNode, ctx, argv);
  }
replace_err:
  p21ParseReset(&x);
}

/*
** p21_set(P21, PATH, VALUE, ...)
**
** Set the value at PATH to VALUE.  Create the PATH if it does not already
** exist.  Overwrite existing values that do exist.
** If P21 or PATH is malformed, throw an error.
**
** p21_insert(P21, PATH, VALUE, ...)
**
** Create PATH and initialize it to VALUE.  If PATH already exists, this
** routine is a no-op.  If P21 or PATH is malformed, throw an error.
*/
static void p21SetFunc(
  sqlite3_context *ctx,
  int argc,
  sqlite3_value **argv
){
  P21Parse x;          /* The parse */
  P21Node *pNode;
  const char *zPath;
  u32 i;
  int bApnd;
  int bIsSet = *(int*)sqlite3_user_data(ctx);

  if( argc<1 ) return;
  if( (argc&1)==0 ) {
    p21WrongNumArgs(ctx, bIsSet ? "set" : "insert");
    return;
  }
  if( p21Parse(&x, ctx, (const char*)sqlite3_value_text(argv[0])) ) return;
  assert( x.nNode );
  for(i=1; i<(u32)argc; i+=2){
    zPath = (const char*)sqlite3_value_text(argv[i]);
    bApnd = 0;
    pNode = p21Lookup(&x, zPath, &bApnd, ctx);
    if( x.oom ){
      sqlite3_result_error_nomem(ctx);
      goto p21SetDone;
    }else if( x.nErr ){
      goto p21SetDone;
    }else if( pNode && (bApnd || bIsSet) ){
      pNode->jnFlags |= (u8)PNODE_REPLACE;
      pNode->u.iReplace = i + 1;
    }
  }
  if( x.aNode[0].jnFlags & PNODE_REPLACE ){
    sqlite3_result_value(ctx, argv[x.aNode[0].u.iReplace]);
  }else{
    p21ReturnP21(x.aNode, ctx, argv);
  }
p21SetDone:
  p21ParseReset(&x);
}

/*
** p21_type(P21)
** p21_type(P21, PATH)
**
** Return the top-level "type" of a P21 string.  Throw an error if
** either the P21 or PATH inputs are not well-formed.
*/
static void p21TypeFunc(
  sqlite3_context *ctx,
  int argc,
  sqlite3_value **argv
){
  P21Parse *p;          /* The parse */
  const char *zPath;
  P21Node *pNode;

  p = p21ParseCached(ctx, argv, ctx);
  if( p==0 ) return;
  if( argc==2 ){
    zPath = (const char*)sqlite3_value_text(argv[1]);
    pNode = p21Lookup(p, zPath, 0, ctx);
  }else{
    pNode = p->aNode;
  }
  if( pNode ){
    sqlite3_result_text(ctx, p21Type[pNode->eType], -1, SQLITE_STATIC);
  }
}

/*
** p21_valid(P21)
**
** Return 1 if P21 is a well-formed P21 string according to RFC-7159.
** Return 0 otherwise.
*/
static void p21ValidFunc(
  sqlite3_context *ctx,
  int argc,
  sqlite3_value **argv
){
  P21Parse *p;          /* The parse */
  UNUSED_PARAM(argc);
  p = p21ParseCached(ctx, argv, 0);
  sqlite3_result_int(ctx, p!=0);
}


/****************************************************************************
** Aggregate SQL function implementations
****************************************************************************/
/*
** p21_group_array(VALUE)
**
** Return a P21 array composed of all values in the aggregate.
*/
static void p21ArrayStep(
  sqlite3_context *ctx,
  int argc,
  sqlite3_value **argv
){
  P21String *pStr;
  UNUSED_PARAM(argc);
  pStr = (P21String*)sqlite3_aggregate_context(ctx, sizeof(*pStr));
  if( pStr ){
    if( pStr->zBuf==0 ){
      p21Init(pStr, ctx);
      p21AppendChar(pStr, '[');
    }else if( pStr->nUsed>1 ){
      p21AppendChar(pStr, ',');
      pStr->pCtx = ctx;
    }
    p21AppendValue(pStr, argv[0]);
  }
}
static void p21ArrayCompute(sqlite3_context *ctx, int isFinal){
  P21String *pStr;
  pStr = (P21String*)sqlite3_aggregate_context(ctx, 0);
  if( pStr ){
    pStr->pCtx = ctx;
    p21AppendChar(pStr, ']');
    if( pStr->bErr ){
      if( pStr->bErr==1 ) sqlite3_result_error_nomem(ctx);
      assert( pStr->bStatic );
    }else if( isFinal ){
      sqlite3_result_text(ctx, pStr->zBuf, (int)pStr->nUsed,
                          pStr->bStatic ? SQLITE_TRANSIENT : sqlite3_free);
      pStr->bStatic = 1;
    }else{
      sqlite3_result_text(ctx, pStr->zBuf, (int)pStr->nUsed, SQLITE_TRANSIENT);
      pStr->nUsed--;
    }
  }else{
    sqlite3_result_text(ctx, "[]", 2, SQLITE_STATIC);
  }
  sqlite3_result_subtype(ctx, P21_SUBTYPE);
}
static void p21ArrayValue(sqlite3_context *ctx){
  p21ArrayCompute(ctx, 0);
}
static void p21ArrayFinal(sqlite3_context *ctx){
  p21ArrayCompute(ctx, 1);
}

#ifndef SQLITE_OMIT_WINDOWFUNC
/*
** This method works for both p21_group_array() and p21_group_object().
** It works by removing the first element of the group by searching forward
** to the first comma (",") that is not within a string and deleting all
** text through that comma.
*/
static void p21GroupInverse(
  sqlite3_context *ctx,
  int argc,
  sqlite3_value **argv
){
  unsigned int i;
  int inStr = 0;
  int nNest = 0;
  char *z;
  char c;
  P21String *pStr;
  UNUSED_PARAM(argc);
  UNUSED_PARAM(argv);
  pStr = (P21String*)sqlite3_aggregate_context(ctx, 0);
#ifdef NEVER
  /* pStr is always non-NULL since p21ArrayStep() or p21ObjectStep() will
  ** always have been called to initalize it */
  if( NEVER(!pStr) ) return;
#endif
  z = pStr->zBuf;
  for(i=1; (c = z[i])!=',' || inStr || nNest; i++){
    if( i>=pStr->nUsed ){
      pStr->nUsed = 1;
      return;
    }
    if( c=='"' ){
      inStr = !inStr;
    }else if( c=='\\' ){
      i++;
    }else if( !inStr ){
      if( c=='{' || c=='[' ) nNest++;
      if( c=='}' || c==']' ) nNest--;
    }
  }
  pStr->nUsed -= i;      
  memmove(&z[1], &z[i+1], (size_t)pStr->nUsed-1);
}
#else
# define p21GroupInverse 0
#endif


/*
** p21_group_obj(NAME,VALUE)
**
** Return a P21 object composed of all names and values in the aggregate.
*/
static void p21ObjectStep(
  sqlite3_context *ctx,
  int argc,
  sqlite3_value **argv
){
  P21String *pStr;
  const char *z;
  u32 n;
  UNUSED_PARAM(argc);
  pStr = (P21String*)sqlite3_aggregate_context(ctx, sizeof(*pStr));
  if( pStr ){
    if( pStr->zBuf==0 ){
      p21Init(pStr, ctx);
      p21AppendChar(pStr, '{');
    }else if( pStr->nUsed>1 ){
      p21AppendChar(pStr, ',');
      pStr->pCtx = ctx;
    }
    z = (const char*)sqlite3_value_text(argv[0]);
    n = (u32)sqlite3_value_bytes(argv[0]);
    p21AppendString(pStr, z, n);
    p21AppendChar(pStr, ':');
    p21AppendValue(pStr, argv[1]);
  }
}
static void p21ObjectCompute(sqlite3_context *ctx, int isFinal){
  P21String *pStr;
  pStr = (P21String*)sqlite3_aggregate_context(ctx, 0);
  if( pStr ){
    p21AppendChar(pStr, '}');
    if( pStr->bErr ){
      if( pStr->bErr==1 ) sqlite3_result_error_nomem(ctx);
      assert( pStr->bStatic );
    }else if( isFinal ){
      sqlite3_result_text(ctx, pStr->zBuf, (int)pStr->nUsed,
                          pStr->bStatic ? SQLITE_TRANSIENT : sqlite3_free);
      pStr->bStatic = 1;
    }else{
      sqlite3_result_text(ctx, pStr->zBuf, (int)pStr->nUsed, SQLITE_TRANSIENT);
      pStr->nUsed--;
    }
  }else{
    sqlite3_result_text(ctx, "{}", 2, SQLITE_STATIC);
  }
  sqlite3_result_subtype(ctx, P21_SUBTYPE);
}
static void p21ObjectValue(sqlite3_context *ctx){
  p21ObjectCompute(ctx, 0);
}
static void p21ObjectFinal(sqlite3_context *ctx){
  p21ObjectCompute(ctx, 1);
}



#ifndef SQLITE_OMIT_VIRTUALTABLE
/****************************************************************************
** The p21_each virtual table
****************************************************************************/
typedef struct P21EachCursor P21EachCursor;
struct P21EachCursor {
  sqlite3_vtab_cursor base;  /* Base class - must be first */
  u32 iRowid;                /* The rowid */
  u32 iBegin;                /* The first node of the scan */
  u32 i;                     /* Index in sParse.aNode[] of current row */
  u32 iEnd;                  /* EOF when i equals or exceeds this value */
  u8 eType;                  /* Type of top-level element */
  char *zP21;               /* Input P21 */
  char *zRoot;               /* Path by which to filter zP21 */
  P21Parse sParse;          /* Parse of the input P21 */
};

/* Constructor for the p21_each virtual table */
static int p21EachConnect(
  sqlite3 *db,
  void *pAux,
  int argc, const char *const*argv,
  sqlite3_vtab **ppVtab,
  char **pzErr
){
  sqlite3_vtab *pNew;
  int rc;

/* Column numbers */
#define PEACH_KEY     0
#define PEACH_VALUE   1
#define PEACH_TYPE    2
#define PEACH_ATOM    3
#define PEACH_ID      4
#define PEACH_PARENT  5
#define PEACH_FULLKEY 6
#define PEACH_PATH    7
/* The xBestIndex method assumes that the P21 and ROOT columns are
** the last two columns in the table.  Should this ever changes, be
** sure to update the xBestIndex method. */
#define PEACH_P21    8
#define PEACH_ROOT    9

  UNUSED_PARAM(pzErr);
  UNUSED_PARAM(argv);
  UNUSED_PARAM(argc);
  UNUSED_PARAM(pAux);
  rc = sqlite3_declare_vtab(db, 
     "CREATE TABLE x(key,value,type,atom,id,parent,fullkey,path,"
                    "p21 HIDDEN,root HIDDEN)");
  if( rc==SQLITE_OK ){
    pNew = *ppVtab = sqlite3_malloc( sizeof(*pNew) );
    if( pNew==0 ) return SQLITE_NOMEM;
    memset(pNew, 0, sizeof(*pNew));
    sqlite3_vtab_config(db, SQLITE_VTAB_INNOCUOUS);
  }
  return rc;
}

/* destructor for p21_each virtual table */
static int p21EachDisconnect(sqlite3_vtab *pVtab){
  sqlite3_free(pVtab);
  return SQLITE_OK;
}

/* constructor for a P21EachCursor object for p21_each(). */
static int p21EachOpenEach(sqlite3_vtab *p, sqlite3_vtab_cursor **ppCursor){
  P21EachCursor *pCur;

  UNUSED_PARAM(p);
  pCur = sqlite3_malloc( sizeof(*pCur) );
  if( pCur==0 ) return SQLITE_NOMEM;
  memset(pCur, 0, sizeof(*pCur));
  *ppCursor = &pCur->base;
  return SQLITE_OK;
}

/* Reset a P21EachCursor back to its original state.  Free any memory
** held. */
static void p21EachCursorReset(P21EachCursor *p){
  sqlite3_free(p->zP21);
  sqlite3_free(p->zRoot);
  p21ParseReset(&p->sParse);
  p->iRowid = 0;
  p->i = 0;
  p->iEnd = 0;
  p->eType = 0;
  p->zP21 = 0;
  p->zRoot = 0;
}

/* Destructor for a p21EachCursor object */
static int p21EachClose(sqlite3_vtab_cursor *cur){
  P21EachCursor *p = (P21EachCursor*)cur;
  p21EachCursorReset(p);
  sqlite3_free(cur);
  return SQLITE_OK;
}

/* Return TRUE if the p21EachCursor object has been advanced off the end
** of the P21 object */
static int p21EachEof(sqlite3_vtab_cursor *cur){
  P21EachCursor *p = (P21EachCursor*)cur;
  return p->i >= p->iEnd;
}

/* Advance the cursor to the next element for p21_tree() */
static int p21EachNext(sqlite3_vtab_cursor *cur){
  P21EachCursor *p = (P21EachCursor*)cur;
  switch( p->eType ){
    case P21_RECORD:
    case P21_LIST: {
      p->i += p21NodeSize(&p->sParse.aNode[p->i]);
      p->iRowid++;
      break;
    }
    default: {
      p->i = p->iEnd;
      break;
    }
  }
  return SQLITE_OK;
}

/* Append the name of the path for element i to pStr
*/
static void p21EachComputePath(
  P21EachCursor *p,       /* The cursor */
  P21String *pStr,        /* Write the path here */
  u32 i                    /* Path to this element */
){
  P21Node *pNode, *pUp;
  u32 iUp;
  if( i==0 ){
    p21AppendChar(pStr, '$');
    return;
  }
  iUp = p->sParse.aUp[i];
  p21EachComputePath(p, pStr, iUp);
  pNode = &p->sParse.aNode[i];
  pUp = &p->sParse.aNode[iUp];
  if( pUp->eType==P21_LIST ){
    p21Printf(30, pStr, "[%d]", pUp->u.iKey);
  }else{
    assert( pUp->eType==P21_RECORD );
    if( (pNode->jnFlags & PNODE_LABEL)==0 ) pNode--;
    assert( pNode->eType==P21_STRING );
    assert( pNode->jnFlags & PNODE_LABEL );
    p21Printf(pNode->n+1, pStr, ".%.*s", pNode->n-2, pNode->u.zJContent+1);
  }
}

/* Return the value of a column */
static int p21EachColumn(
  sqlite3_vtab_cursor *cur,   /* The cursor */
  sqlite3_context *ctx,       /* First argument to sqlite3_result_...() */
  int i                       /* Which column to return */
){
  P21EachCursor *p = (P21EachCursor*)cur;
  P21Node *pThis = &p->sParse.aNode[p->i];
  switch( i ){
    case PEACH_KEY: {
      if( p->i==0 ) break;
      if( p->eType==P21_RECORD ){
        p21Return(pThis, ctx, 0);
      }else if( p->eType==P21_LIST ){
        u32 iKey;
        iKey = p->iRowid;
        sqlite3_result_int64(ctx, (sqlite3_int64)iKey);
      }
      break;
    }
    case PEACH_VALUE: {
      if( pThis->jnFlags & PNODE_LABEL ) pThis++;
      p21Return(pThis, ctx, 0);
      break;
    }
    case PEACH_TYPE: {
      if( pThis->jnFlags & PNODE_LABEL ) pThis++;
      sqlite3_result_text(ctx, p21Type[pThis->eType], -1, SQLITE_STATIC);
      break;
    }
    case PEACH_ATOM: {
      if( pThis->jnFlags & PNODE_LABEL ) pThis++;
      if( pThis->eType>=P21_LIST ) break;
      p21Return(pThis, ctx, 0);
      break;
    }
    case PEACH_ID: {
      sqlite3_result_int64(ctx, 
         (sqlite3_int64)p->i + ((pThis->jnFlags & PNODE_LABEL)!=0));
      break;
    }
    case PEACH_FULLKEY: {
      P21String x;
      p21Init(&x, ctx);
      if( p->zRoot ){
        p21AppendRaw(&x, p->zRoot, (int)strlen(p->zRoot));
      }else{
        p21AppendChar(&x, '$');
      }
      if( p->eType==P21_LIST ){
        p21Printf(30, &x, "[%d]", p->iRowid);
      }else if( p->eType==P21_RECORD ){
        p21Printf(pThis->n, &x, ".%.*s", pThis->n-2, pThis->u.zJContent+1);
      }
      p21Result(&x);
      break;
    }
    case PEACH_PATH:
    default: {
      const char *zRoot = p->zRoot;
      if( zRoot==0 ) zRoot = "$";
      sqlite3_result_text(ctx, zRoot, -1, SQLITE_STATIC);
      break;
    }
    case PEACH_P21: {
      assert( i==PEACH_P21 );
      sqlite3_result_text(ctx, p->sParse.zP21, -1, SQLITE_STATIC);
      break;
    }
  }
  return SQLITE_OK;
}

/* Return the current rowid value */
static int p21EachRowid(sqlite3_vtab_cursor *cur, sqlite_int64 *pRowid){
  P21EachCursor *p = (P21EachCursor*)cur;
  *pRowid = p->iRowid;
  return SQLITE_OK;
}

/* The query strategy is to look for an equality constraint on the p21
** column.  Without such a constraint, the table cannot operate.  idxNum is
** 1 if the constraint is found, 3 if the constraint and zRoot are found,
** and 0 otherwise.
*/
static int p21EachBestIndex(
  sqlite3_vtab *tab,
  sqlite3_index_info *pIdxInfo
){
  int i;                     /* Loop counter or computed array index */
  int aIdx[2];               /* Index of constraints for P21 and ROOT */
  int unusableMask = 0;      /* Mask of unusable P21 and ROOT constraints */
  int idxMask = 0;           /* Mask of usable == constraints P21 and ROOT */
  const struct sqlite3_index_constraint *pConstraint;

  /* This implementation assumes that P21 and ROOT are the last two
  ** columns in the table */
  assert( PEACH_ROOT == PEACH_P21+1 );
  UNUSED_PARAM(tab);
  aIdx[0] = aIdx[1] = -1;
  pConstraint = pIdxInfo->aConstraint;
  for(i=0; i<pIdxInfo->nConstraint; i++, pConstraint++){
    int iCol;
    int iMask;
    if( pConstraint->iColumn < PEACH_P21 ) continue;
    iCol = pConstraint->iColumn - PEACH_P21;
    assert( iCol==0 || iCol==1 );
    iMask = 1 << iCol;
    if( pConstraint->usable==0 ){
      unusableMask |= iMask;
    }else if( pConstraint->op==SQLITE_INDEX_CONSTRAINT_EQ ){
      aIdx[iCol] = i;
      idxMask |= iMask;
    }
  }
  if( (unusableMask & ~idxMask)!=0 ){
    /* If there are any unusable constraints on P21 or ROOT, then reject
    ** this entire plan */
    return SQLITE_CONSTRAINT;
  }
  if( aIdx[0]<0 ){
    /* No P21 input.  Leave estimatedCost at the huge value that it was
    ** initialized to to discourage the query planner from selecting this
    ** plan. */
    pIdxInfo->idxNum = 0;
  }else{
    pIdxInfo->estimatedCost = 1.0;
    i = aIdx[0];
    pIdxInfo->aConstraintUsage[i].argvIndex = 1;
    pIdxInfo->aConstraintUsage[i].omit = 1;
    if( aIdx[1]<0 ){
      pIdxInfo->idxNum = 1;  /* Only P21 supplied.  Plan 1 */
    }else{
      i = aIdx[1];
      pIdxInfo->aConstraintUsage[i].argvIndex = 2;
      pIdxInfo->aConstraintUsage[i].omit = 1;
      pIdxInfo->idxNum = 3;  /* Both P21 and ROOT are supplied.  Plan 3 */
    }
  }
  return SQLITE_OK;
}

/* Start a search on a new P21 string */
static int p21EachFilter(
  sqlite3_vtab_cursor *cur,
  int idxNum, const char *idxStr,
  int argc, sqlite3_value **argv
){
  P21EachCursor *p = (P21EachCursor*)cur;
  const char *z;
  const char *zRoot = 0;
  sqlite3_int64 n;

  UNUSED_PARAM(idxStr);
  UNUSED_PARAM(argc);
  p21EachCursorReset(p);
  if( idxNum==0 ) return SQLITE_OK;
  z = (const char*)sqlite3_value_text(argv[0]);
  if( z==0 ) return SQLITE_OK;
  n = sqlite3_value_bytes(argv[0]);
  p->zP21 = sqlite3_malloc64( n+1 );
  if( p->zP21==0 ) return SQLITE_NOMEM;
  memcpy(p->zP21, z, (size_t)n+1);
  if( p21Parse(&p->sParse, 0, p->zP21) ){
    int rc = SQLITE_NOMEM;
    if( p->sParse.oom==0 ){
      sqlite3_free(cur->pVtab->zErrMsg);
      cur->pVtab->zErrMsg = sqlite3_mprintf("malformed P21");
      if( cur->pVtab->zErrMsg ) rc = SQLITE_ERROR;
    }
    p21EachCursorReset(p);
    return rc;
  }else{
    P21Node *pNode = 0;
    if( idxNum==3 ){
      const char *zErr = 0;
      zRoot = (const char*)sqlite3_value_text(argv[1]);
      if( zRoot==0 ) return SQLITE_OK;
      n = sqlite3_value_bytes(argv[1]);
      p->zRoot = sqlite3_malloc64( n+1 );
      if( p->zRoot==0 ) return SQLITE_NOMEM;
      memcpy(p->zRoot, zRoot, (size_t)n+1);
      if( zRoot[0]!='$' ){
        zErr = zRoot;
      }else{
        pNode = p21LookupStep(&p->sParse, 0, p->zRoot+1, 0, &zErr);
      }
      if( zErr ){
        sqlite3_free(cur->pVtab->zErrMsg);
        cur->pVtab->zErrMsg = p21PathSyntaxError(zErr);
        p21EachCursorReset(p);
        return cur->pVtab->zErrMsg ? SQLITE_ERROR : SQLITE_NOMEM;
      }else if( pNode==0 ){
        return SQLITE_OK;
      }
    }else{
      pNode = p->sParse.aNode;
    }
    p->iBegin = p->i = (int)(pNode - p->sParse.aNode);
    p->eType = pNode->eType;
    if( p->eType>=P21_LIST ){
      pNode->u.iKey = 0;
      p->iEnd = p->i + pNode->n + 1;
      p->i++;
    }else{
      p->iEnd = p->i+1;
    }
  }
  return SQLITE_OK;
}

/* The methods of the p21_each virtual table */
static sqlite3_module p21EachModule = {
  0,                         /* iVersion */
  0,                         /* xCreate */
  p21EachConnect,           /* xConnect */
  p21EachBestIndex,         /* xBestIndex */
  p21EachDisconnect,        /* xDisconnect */
  0,                         /* xDestroy */
  p21EachOpenEach,          /* xOpen - open a cursor */
  p21EachClose,             /* xClose - close a cursor */
  p21EachFilter,            /* xFilter - configure scan constraints */
  p21EachNext,              /* xNext - advance a cursor */
  p21EachEof,               /* xEof - check for end of scan */
  p21EachColumn,            /* xColumn - read data */
  p21EachRowid,             /* xRowid - read data */
  0,                         /* xUpdate */
  0,                         /* xBegin */
  0,                         /* xSync */
  0,                         /* xCommit */
  0,                         /* xRollback */
  0,                         /* xFindMethod */
  0,                         /* xRename */
  0,                         /* xSavepoint */
  0,                         /* xRelease */
  0,                         /* xRollbackTo */
  0                          /* xShadowName */
};

#endif /* SQLITE_OMIT_VIRTUALTABLE */

/****************************************************************************
** The following routines are the only publically visible identifiers in this
** file.  Call the following routines in order to register the various SQL
** functions and the virtual table implemented by this file.
****************************************************************************/

int sqlite3P21sqlInit(sqlite3 *db){
  int rc = SQLITE_OK;
  unsigned int i;
  static const struct {
     const char *zName;
     int nArg;
     int flag;
     void (*xFunc)(sqlite3_context*,int,sqlite3_value**);
  } aFunc[] = {
    { "p21",                 1, 0,   p21RemoveFunc        },
    { "p21_array",          -1, 0,   p21ArrayFunc         },
    { "p21_array_length",    1, 0,   p21ArrayLengthFunc   },
    { "p21_array_length",    2, 0,   p21ArrayLengthFunc   },
    { "p21_extract",        -1, 0,   p21ExtractFunc       },
    { "p21_insert",         -1, 0,   p21SetFunc           },
    { "p21_object",         -1, 0,   p21ObjectFunc        },
#if 0
    { "p21_patch",           2, 0,   p21PatchFunc         },
#endif
    { "p21_quote",           1, 0,   p21QuoteFunc         },
    { "p21_remove",         -1, 0,   p21RemoveFunc        },
    { "p21_replace",        -1, 0,   p21ReplaceFunc       },
    { "p21_set",            -1, 1,   p21SetFunc           },
    { "p21_type",            1, 0,   p21TypeFunc          },
    { "p21_type",            2, 0,   p21TypeFunc          },
    { "p21_valid",           1, 0,   p21ValidFunc         },

#if SQLITE_DEBUG
    /* DEBUG and TESTING functions */
    { "p21_parse",           1, 0,   p21ParseFunc         },
    { "p21_test1",           1, 0,   p21Test1Func         },
#endif
  };
  static const struct {
     const char *zName;
     int nArg;
     void (*xStep)(sqlite3_context*,int,sqlite3_value**);
     void (*xFinal)(sqlite3_context*);
     void (*xValue)(sqlite3_context*);
  } aAgg[] = {
    { "p21_group_array",     1,
      p21ArrayStep,   p21ArrayFinal,  p21ArrayValue  },
    { "p21_group_object",    2,
      p21ObjectStep,  p21ObjectFinal, p21ObjectValue },
  };
#ifndef SQLITE_OMIT_VIRTUALTABLE
  static const struct {
     const char *zName;
     sqlite3_module *pModule;
  } aMod[] = {
    { "p21_each",            &p21EachModule               },
  };
#endif
  static const int enc = 
       SQLITE_UTF8 |
       SQLITE_DETERMINISTIC |
       SQLITE_INNOCUOUS;
  for(i=0; i<sizeof(aFunc)/sizeof(aFunc[0]) && rc==SQLITE_OK; i++){
    rc = sqlite3_create_function(db, aFunc[i].zName, aFunc[i].nArg, enc,
                                 (void*)&aFunc[i].flag,
                                 aFunc[i].xFunc, 0, 0);
  }
#ifndef SQLITE_OMIT_WINDOWFUNC
  for(i=0; i<sizeof(aAgg)/sizeof(aAgg[0]) && rc==SQLITE_OK; i++){
    rc = sqlite3_create_window_function(db, aAgg[i].zName, aAgg[i].nArg,
                                 SQLITE_SUBTYPE | enc, 0,
                                 aAgg[i].xStep, aAgg[i].xFinal,
                                 aAgg[i].xValue, p21GroupInverse, 0);
  }
#endif
#ifndef SQLITE_OMIT_VIRTUALTABLE
  for(i=0; i<sizeof(aMod)/sizeof(aMod[0]) && rc==SQLITE_OK; i++){
    rc = sqlite3_create_module(db, aMod[i].zName, aMod[i].pModule, 0);
  }
#endif
  return rc;
}


#ifndef SQLITE_CORE
#ifdef _WIN32
__declspec(dllexport)
#endif
int sqlite3_p21sql_init(
  sqlite3 *db, 
  char **pzErrMsg, 
  const sqlite3_api_routines *pApi
){
  SQLITE_EXTENSION_INIT2(pApi);
  (void)pzErrMsg;  /* Unused parameter */
  return sqlite3P21sqlInit(db);
}
#endif
#endif /* !defined(SQLITE_CORE) || defined(SQLITE_ENABLE_P21SQL) */
