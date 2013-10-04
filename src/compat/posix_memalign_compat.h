#ifndef BRLCAD_POSIX_MEMALIGN_COMPAT_H
#define BRLCAD_POSIX_MEMALIGN_COMPAT_H

/* see file 'src/compat/README.compat' for more details */

int posix_memalign(void **memptr, size_t alignment, size_t size);

#endif /* BRLCAD_POSIX_MEMALIGN_COMPAT_H */
