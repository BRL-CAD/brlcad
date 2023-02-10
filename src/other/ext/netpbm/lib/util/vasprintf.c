#define _GNU_SOURCE
   /* Because of conditional compilation, this is GNU source only if the C
      library is GNU.
   */
#include <stdlib.h>
#include <string.h>

#include "pm_config.h"
#include "pm_c_util.h"

#include "nstring.h"



void
pm_vasprintf(const char ** const resultP,
             const char *  const format,
             va_list             varargs) {

    char * result;

#if HAVE_VASPRINTF
    int rc;

    rc = vasprintf(&result, format, varargs);

    if (rc < 0)
        *resultP = pm_strsol;
    else
        *resultP = result;
#else
    /* We have a big compromise here.  To do this right, without a
       huge amount of work, we need to go through the variable
       arguments twice: once to determine how much memory to allocate,
       and once to format the string.  On some machines, you can
       simply make two copies of the va_list variable in normal C
       fashion, but on others you need va_copy, which is a relatively
       recent invention.  In particular, the simple va_list copy
       failed on an AMD64 Gcc Linux system in March 2006.  

       So instead, we just allocate 4K and truncate or waste as
       necessary.

       Note that we don't recognize the floating point specifiers (%f, %e, %g)
       - we render them as 'f', 'e', and 'g'.  It would be too much work to
       make this code handle those, just for the few systems on which it runs.
       Instead, we have pm_vasprintf_knows_float(), and any caller that cares
       enough can avoid using these specifiers where they don't work.
    */
    size_t const allocSize = 4096;
    result = malloc(allocSize);
    
    if (result == NULL)
        *resultP = pm_strsol;
    else {
        size_t realLen;

        pm_vsnprintf(result, allocSize, format, varargs, &realLen);
        
        if (realLen >= allocSize)
            strcpy(result + allocSize - 15, "<<<TRUNCATED");

        *resultP = result;
    }
#endif
}



bool
pm_vasprintf_knows_float(void) {
#if HAVE_VASPRINTF
    return true;
#else
    return false;
#endif
}
