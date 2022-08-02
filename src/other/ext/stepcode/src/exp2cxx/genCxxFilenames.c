#include "genCxxFilenames.h"
#include "class_strings.h"

#if defined(_MSC_VER) && _MSC_VER < 1900
#  include "sc_stdio.h"
#  define snprintf c99_snprintf
#endif

/** \file genCxxFilenames.c
 * functions shared by exp2cxx and the schema scanner.
 * The latter creates, at configuration time, a list
 * of file names so CMake knows how to compile the
 * generated libs.
 * exp2cxx is supposed to write to files with the same
 * names, but it doesn't have access to the list the
 * scanner created.
 */

/* these buffers are shared amongst (and potentially overwritten by) all functions in this file */
char impl[BUFSIZ+1] = {0};
char header[BUFSIZ+1] = {0};

/* struct containing pointers to above buffers. pointers are 'const char *' */
filenames_t fnames = { impl, header };


filenames_t getEntityFilenames(Entity e)
{
    const char *name = ENTITYget_classname(e);
    snprintf(header, BUFSIZ, "entity/%s.h", name);
    snprintf(impl, BUFSIZ, "entity/%s.cc", name);
    return fnames;
}

filenames_t getTypeFilenames(Type t)
{
    const char *name = TYPEget_ctype(t);
    snprintf(header, BUFSIZ, "type/%s.h", name);
    snprintf(impl, BUFSIZ, "type/%s.cc", name);
    return fnames;
}
