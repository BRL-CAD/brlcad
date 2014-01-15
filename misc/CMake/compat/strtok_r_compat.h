#ifndef BRLCAD_STRTOK_R_COMPAT_H
#define BRLCAD_STRTOK_R_COMPAT_H

#include "common.h"

/* see file 'src/compat/README.compat' for more details */

__BEGIN_DECLS

/* from <string.h> */
char *strtok_r(char *str, const char *delim, char **saveptr);

__END_DECLS

#endif /* BRLCAD_STRTOK_R_COMPAT_H */
