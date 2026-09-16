#ifndef BRLCAD_FILENO_COMPAT_H
#define BRLCAD_FILENO_COMPAT_H

#include "common.h"

/* see file 'src/compat/README.compat' for more details */

#include <stdio.h>

/* Keep declaration minimal and consistent with include/bio.h: provide C
 * linkage when compiled as C++ but do not add any dll-export macros here.
 */
#ifdef __cplusplus
extern "C" {
#endif

int fileno(FILE *stream);

#ifdef __cplusplus
}
#endif

#endif /* BRLCAD_FILENO_COMPAT_H */
