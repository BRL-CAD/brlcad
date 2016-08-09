#ifndef BRLCAD_STRNCASECMP_COMPAT_H
#define BRLCAD_STRNCASECMP_COMPAT_H

#include "common.h"

/* see file 'src/compat/README.compat' for more details */

__BEGIN_DECLS

int strncasecmp(const char *s1, const char *s2, size_t n);

__END_DECLS

#endif /* BRLCAD_STRNCASECMP_COMPAT_H */
