#ifndef BRLCAD_POSIX_MEMALIGN_COMPAT_H
#define BRLCAD_POSIX_MEMALIGN_COMPAT_H

#include "common.h"

/* see file 'src/compat/README.compat' for more details */

__BEGIN_DECLS

int posix_memalign(void **memptr, size_t alignment, size_t size);

#include "common.h"

#endif /* BRLCAD_POSIX_MEMALIGN_COMPAT_H */
