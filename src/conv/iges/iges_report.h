/* BRL-CAD
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef CONV_IGES_REPORT_H
#define CONV_IGES_REPORT_H

#include "common.h"
#include <string>

namespace brlcad {
namespace iges {
/** Encode the contents of a JSON string, without surrounding quotes. */
std::string json_escape(const std::string &value);
}
}
#endif

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
