#ifndef BRLCAD_REALPATH_COMPAT_H
#define BRLCAD_REALPATH_COMPAT_H

#include "common.h"

/* see file 'src/compat/README.compat' for more details */

__BEGIN_DECLS

char *realpath(const char *path, char *resolved_path);

__END_DECLS

#endif /* BRLCAD_REALPATH_COMPAT_H */
