/*                    P L U G I N _ 1 . C P P
 * BRL-CAD
 *
 * Published in 2020 by the United States Government.
 * This work is in the public domain.
 *
 */
/** @file plugin_1.cpp
 *
 * Sample plugin
 *
 */

#include "common.h"

#include <string>
#include <string.h>

#include "bu/str.h"
#include "./dylib.h"

extern "C" int
calc(char **result, int rlen, int input)
{
    if (rlen <= 0 || !result) {
	return -1;
    }

    int ret = 0;
    int output = 2 * input;
    std::string sout = std::to_string(output);

    if (sout.length() > (size_t)(rlen - 1)) {
	// copy what we can, but don't have room for everything.
	ret = 1;
    }

    // Copy result into the provided buffer with guaranteed nul-termination
    bu_strlcpy((*result), sout.c_str(), rlen);

    return ret;
}

static const struct dylib_contents pcontents = {"Plugin 1", 1.0, &calc};

const struct dylib_plugin pinfo = { &pcontents };

extern "C" {
    BU_DYLIB_EXPORT const struct dylib_plugin *dylib_plugin_info()
    {
	return &pinfo;
    }
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
