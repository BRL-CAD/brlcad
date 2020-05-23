/*                         D Y L I B . H
 * BRL-CAD
 *
 * Published in 2020 by the United States Government.
 * This work is in the public domain.
 *
 */
/** @file dylib.h
 *
 * Header defining data structures and functions common to plugins, as well as
 * the libdylib API that wraps loading and unloading them.
 */

#include "common.h"
#include "bu/ptbl.h"

#ifndef BU_DYLIB_EXPORT
#  if defined(BU_DYLIB_DLL_EXPORTS) && defined(BU_DYLIB_DLL_IMPORTS)
#    error "Only BU_DYLIB_DLL_EXPORTS or BU_DYLIB_DLL_IMPORTS can be defined, not both."
#  elif defined(BU_DYLIB_DLL_EXPORTS)
#    define BU_DYLIB_EXPORT COMPILER_DLLEXPORT
#  elif defined(BU_DYLIB_DLL_IMPORTS)
#    define BU_DYLIB_EXPORT COMPILER_DLLIMPORT
#  else
#    define BU_DYLIB_EXPORT
#  endif
#endif

struct dylib_contents {
    const char *const name;
    double version;

    int (*calc)(char **result, int rmaxlen, int input);
};

struct dylib_plugin {
    const struct dylib_contents * const i;
};

// Load all plugins present in the LIBEXEC_PLUGINS/dylib directory.  Optionally return
// handles in a second table to allow the calling of dylib_close_plugins.
BU_DYLIB_EXPORT extern int dylib_load_plugins(struct bu_ptbl *plugins, struct bu_ptbl *handles);

// If we stored handles when calling dylib_load_plugins, we can close them with this call.
BU_DYLIB_EXPORT extern int dylib_close_plugins(struct bu_ptbl *handles);

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
