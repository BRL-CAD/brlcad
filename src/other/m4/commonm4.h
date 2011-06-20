/* Portability wrappers needed in multiple files */

#if defined(_WIN32) && !defined(__CYGWIN__)
#  define NOMINMAX
#  include <windows.h>
#  include <io.h>

#   undef rad1 /* Win32 radio button 1 */
#   undef rad2 /* Win32 radio button 2 */
#   undef small /* defined as part of the Microsoft Interface Definition Language (MIDL) */
#   undef IN
#   undef OUT

#else
#  include <unistd.h>
#endif


/* make sure most of the C99 stdint types are provided including the
 *  * optional uintptr_t type.
 *   */
#if !defined(INT8_MAX) || !defined(INT16_MAX) || !defined(INT32_MAX) || !defined(INT64_MAX)
#  if (defined _MSC_VER && (_MSC_VER <= 1500))  
     /* Older Versions of Visual C++ seem to need pstdint.h 
      *       * but still pass the tests below, so force it based on
      *             * version (ugh.) */
#    include "pstdint.h"
#  elif defined(__STDC__) || defined(__STRICT_ANSI__) || defined(__SIZE_TYPE__) || defined(HAVE_STDINT_H)
#    define __STDC_LIMIT_MACROS 1
#    define __STDC_CONSTANT_MACROS 1
#    include <stdint.h>
#  else
#    include "pstdint.h"
#  endif
#endif

