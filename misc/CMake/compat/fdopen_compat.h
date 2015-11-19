#ifndef BRLCAD_FDOPEN_COMPAT_H
#define BRLCAD_FDOPEN_COMPAT_H

#include "common.h"

/* see file 'src/compat/README.compat' for more details */

#include <stdio.h>

__BEGIN_DECLS

FILE *fdopen(int fd, const char *mode);

__END_DECLS

#endif /* BRLCAD_FDOPEN_COMPAT_H */
