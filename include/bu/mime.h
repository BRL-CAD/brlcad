/*                         M I M E . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

#ifndef BU_MIME_H
#define BU_MIME_H

#include "bu/mime_runtime.h"
#include "bu/mime_types.h"

__BEGIN_DECLS

/**
 * DEPRECATED (7.48): Use bu_mime_detect or
 * bu_mime_detect_file and consume canonical MIME candidates.
 *
 * @brief Use the file extension to attempt to determine the media type
 * of the file within the specified context.
 *
 * returns -1 if no match was found, or an integer if a result
 * was found.  It is the responsibility of the caller to cast
 * the return int to the correct mime_CONTEXT_t type.
 */
BU_EXPORT extern int bu_file_mime(const char *ext, bu_mime_context_t context);


/**
 * DEPRECATED (7.48): Use bu_mime_list to query registered
 * extensions by canonical MIME name or alias.
 *
 * @brief Given a mime type and a context, return the file extension(s)
 * associated with that type.
 *
 * returns NULL if no match was found, or a comma separated string
 * containing the extensions if a result was found.
 * It is the responsibility of the caller to free the returned string.
 */
BU_EXPORT extern const char *bu_file_mime_ext(int t, bu_mime_context_t context);


/**
 * DEPRECATED (7.48): Use canonical MIME names or
 * bu_mime_description instead of generated enum spellings.
 *
 * @brief Given a mime type and a context, return a human readable string
 * spelling out the type (corresponding to the enum string in
 * source code.)
 *
 * returns NULL if no match was found, or a string if a result was found.
 * It is the responsibility of the caller to free the returned string.
 */
BU_EXPORT extern const char *bu_file_mime_str(int t, bu_mime_context_t context);


/**
 * DEPRECATED (7.48): Use bu_mime_canonical to resolve
 * names and aliases without converting them to integer identities.
 *
 * @brief Given a string produced by bu_file_mime_str, convert it back into
 * integer form.
 *
 * returns -1 if no match was found, or an integer if a result
 * was found.  It is the responsibility of the caller to cast
 * the return int to the correct mime_CONTEXT_t type.
 */
BU_EXPORT extern int bu_file_mime_int(const char *str);

/** DEPRECATED (7.48): This temporary migration bridge
 * is removed with the generated enum types; new APIs use MIME names.
 * Translate legacy values without relying on enum spellings. Returned
 * names have static storage; reverse lookup returns -1 if not representable. */
BU_EXPORT extern const char *bu_mime_legacy_name(int type, bu_mime_context_t context);
/** DEPRECATED (7.48): Temporary reverse bridge for callers
 * being migrated off the generated enum types. */
BU_EXPORT extern int bu_mime_legacy_type(const char *name, bu_mime_context_t context);


__END_DECLS

#endif
