#ifndef BRLCAD_SNPRINTF_COMPAT_H
#define BRLCAD_SNPRINTF_COMPAT_H

#include "common.h"

/* see file 'src/compat/README.compat' for more details */

__BEGIN_DECLS

int snprintf(char *str, size_t size, const char *format, ...);

__END_DECLS

#endif /* BRLCAD_SNPRINTF_COMPAT_H */
