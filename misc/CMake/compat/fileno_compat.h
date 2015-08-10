#ifndef BRLCAD_FILENO_COMPAT_H
#define BRLCAD_FILENO_COMPAT_H

#include "common.h"

/* see file 'src/compat/README.compat' for more details */

#include <stdio.h>

__BEGIN_DECLS

int fileno(FILE *stream);

__END_DECLS

#endif /* BRLCAD_FILENO_COMPAT_H */
