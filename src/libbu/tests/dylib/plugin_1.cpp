#include "common.h"

#include <string>
#include <string.h>

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
	// We'll copy what we can, but we don't have enough room for
	// everything.
	ret = 1;
    }

    // Copy the result in to the provided buffer and null terminate
    strncpy((*result), sout.c_str(), rlen - 1);
    size_t npos = ((size_t)rlen < sout.length()) ? rlen - 1 : sout.length();
    (*result)[npos] = '\0';

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
