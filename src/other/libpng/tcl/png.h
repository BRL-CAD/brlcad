
/*
 * Redirect the png definitions through the stub definitions of the
 * binding. A wrapper for a support library using png has to use this
 * header to ensure usage of the stub macros during the compilation of
 * the support library itself. In this way we avoid the need for
 * changing the original sources.
 *
 * This header has to be avoided when building the libpng wrapper itself.
 */

#include "pngtclDecls.h"
