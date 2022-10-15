#ifndef GENCXXFILENAMES_H
#define GENCXXFILENAMES_H

/** \file genCxxFilenames.c
 * functions shared by exp2cxx and the schema
 * scanner which determines, at configuration time,
 * the names of the files exp2cxx will generate -
 * as well as a relative path, if any
 */

#include "express/entity.h"
#include "express/type.h"

typedef struct {
    /* will we ever need more file names? */
    const char * impl;
    const char * header;
} filenames_t;

/** write entity filenames to a pair of shared static buffers.
 * names will be overwritten by next call to a function using those buffers!
 *
 * \sa getTypeFilenames()
 */
filenames_t getEntityFilenames( Entity e );

/** write type filenames to a pair of shared static buffers.
 * names will be overwritten by next call to a function using those buffers!
 *
 * \sa getEntityFilenames()
 */
filenames_t getTypeFilenames( Type t );

#endif /* GENCXXFILENAMES_H */
