#ifndef PM_C_UTIL_INCLUDED
#define PM_C_UTIL_INCLUDED

/* Note that for MAX and MIN, if either of the operands is a floating point
   Not-A-Number, the result is the second operand.  So if you're computing a
   running maximum and want to ignore the NaNs in the computation, put the
   running maximum variable second.
*/
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

/* ROUNDUP rounds up to a specified multiple.  E.g. ROUNDUP(22, 8) == 24 */

#undef ROUNDUP
#define ROUNDUP(X,M) (((X)+(M)-1)/(M)*(M))
#undef ROUNDDN
#define ROUNDDN(X,M) ((X)/(M)*(M))

#define ROUNDDIV(DIVIDEND,DIVISOR) (((DIVIDEND) + (DIVISOR)/2)/(DIVISOR))

#undef SQR
#define SQR(a) ((a)*(a))

/* NOTE: do not use "bool" as a type in an external interface.  It could
   have different definitions on either side of the interface.  Even if both
   sides include this interface header file, the conditional compilation
   here means one side may use the typedef below and the other side may
   use some other definition.  For an external interface, be safe and just
   use "int".
*/

/* We will probably never again see a system that does not have
   <stdbool.h>, but just in case, we have our own alternative here.

   Evidence shows that the compiler actually produces better code with
   <stdbool.h> than with bool simply typedefed to int.
*/

#ifdef __cplusplus
  /* C++ has a bool type and false and true constants built in. */
#else
  /* The test for __STDC__ is paranoid.  It is there just in case some
     nonstandard compiler defines __STDC_VERSION__ in an arbitrary manner.
  */
  #if ( defined(__GNUC__) && (__GNUC__ >= 3) ) || \
      ( defined(__STDC__) && (__STDC__ ==1) && \
        defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L) ) 
    #include <stdbool.h>
  #else
    /* We used to assume that if TRUE was defined, then bool was too.
       However, we had a report on 2001.09.21 of a Tru64 system that had
       TRUE but not bool and on 2002.03.21 of an AIX 4.3 system that was
       likewise.  So now we define bool all the time, unless the macro
       HAVE_BOOL is defined.  If someone is using the Netpbm libraries and
       also another library that defines bool, he can either make the
       other library define/respect HAVE_BOOL or just define HAVE_BOOL in
       the file that includes pm_config.h or with a compiler option.  Note
       that C++ always has bool.  
    */
    #ifndef HAVE_BOOL
      #define HAVE_BOOL 1
      typedef int bool;
    #endif
    #ifndef true
      enum boolvalue {false=0, true=1};
    #endif
  #endif
#endif

#ifndef TRUE
  #define TRUE true
  #endif
#ifndef FALSE
  #define FALSE false
  #endif

#define ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))

#endif
