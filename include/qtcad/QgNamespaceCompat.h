/*                Q G N A M E S P A C E C O M P A T . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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
/** @file QgNamespaceCompat.h
 *
 * Transitional compatibility header.
 *
 * @deprecated Include specific qtcad headers (or qtcad/QgNamespace.h) directly.
 */

#ifndef QGNAMESPACECOMPAT_H
#define QGNAMESPACECOMPAT_H

#include "qtcad/QgNamespace.h"

/* One-release compatibility using block for callers that rely on unqualified
 * symbol resolution after including this compatibility header. */
namespace qtcad_compat {
using namespace qtcad;
}
using namespace qtcad_compat;

#endif /* QGNAMESPACECOMPAT_H */
