#ifndef PM_C_UTIL_INCLUDED
#define PM_C_UTIL_INCLUDED

#undef MAX
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#undef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#undef ABS
#define ABS(a) ((a) >= 0 ? (a) : -(a))
#undef SGN
#define SGN(a)		(((a)<0) ? -1 : 1)
#undef ODD
#define ODD(n) ((n) & 1)
#undef ROUND
#define ROUND(X) (((X) >= 0) ? (int)((X)+0.5) : (int)((X)-0.5))
#undef ROUNDU
#define ROUNDU(X) ((unsigned int)((X)+0.5))
#undef SQR
#define SQR(a) ((a)*(a))

/* NOTE: do not use "bool" as a type in an external interface.  It could
   have different definitions on either side of the interface.  Even if both
   sides include this interface header file, the conditional compilation
   here means one side may use the typedef below and the other side may
   use some other definition.  For an external interface, be safe and just
   use "int".
*/

/* We used to assume that if TRUE was defined, then bool was too.
   However, we had a report on 2001.09.21 of a Tru64 system that had
   TRUE but not bool and on 2002.03.21 of an AIX 4.3 system that was
   likewise.  So now we define bool all the time, unless the macro
   HAVE_BOOL is defined.  If someone is using the Netpbm libraries and
   also another library that defines bool, he can either make the
   other library define/respect HAVE_BOOL or just define HAVE_BOOL in
   the file that includes pm_config.h or with a compiler option.  Note
   that C++ always has bool.  

   A preferred way of getting booleans is <stdbool.h>.  But it's not
   available on all platforms, and it's easy to reproduce what it does
   here.
*/
#ifndef TRUE
  #define TRUE 1
  #endif
#ifndef FALSE
  #define FALSE 0
  #endif
/* C++ has a bool type and false and true constants built in. */
#ifndef __cplusplus
  #ifndef HAVE_BOOL
    #define HAVE_BOOL 1
    typedef int bool;
    #endif
  #ifndef true
    enum boolvalue {false=0, true=1};
    #endif
  #endif

#define ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))

#endif
