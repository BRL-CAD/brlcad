#ifndef BRLCAD_GETHOSTNAME_COMPAT_H
#define BRLCAD_GETHOSTNAME_COMPAT_H

#include "common.h"

/* see file 'src/compat/README.compat' for more details */

__BEGIN_DECLS

int gethostname(char *name, size_t len);

__END_DECLS

#endif /* BRLCAD_GETHOSTNAME_COMPAT_H */
