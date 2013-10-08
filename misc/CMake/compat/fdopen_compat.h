#ifndef BRLCAD_FDOPEN_COMPAT_H
#define BRLCAD_FDOPEN_COMPAT_H

/* see file 'src/compat/README.compat' for more details */

#include <stdio.h>

FILE *fdopen(int fd, const char *mode);

#endif /* BRLCAD_FDOPEN_COMPAT_H */
