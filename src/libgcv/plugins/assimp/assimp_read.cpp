
/*
---------------------------------------------------------------------------
Open Asset Import Library (assimp)
---------------------------------------------------------------------------
Copyright (c) 2006-2022, assimp team
All rights reserved.
Redistribution and use of this software in source and binary forms,
with or without modification, are permitted provided that the following
conditions are met:
* Redistributions of source code must retain the above
copyright notice, this list of conditions and the
following disclaimer.
* Redistributions in binary form must reproduce the above
copyright notice, this list of conditions and the
following disclaimer in the documentation and/or other
materials provided with the distribution.
* Neither the name of the assimp team, nor the names of its
contributors may be used to endorse or promote products
derived from this software without specific prior
written permission of the assimp team.
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
---------------------------------------------------------------------------
*/

#include "common.h"

#include <string>

#include "gcv/api.h"
#include "wdb.h"

// assimp headers


struct assimp_read_options
{
};

struct conversion_state
{
	const struct gcv_opts* gcv_options;
	struct assimp_read_options* assimp_read_options;

	std::string input_file;	                        /* name of the input file */
	FILE* fd_in;		                        /* input file */
	struct rt_wdb* fd_out;	                        /* Resulting BRL-CAD file */
};
#define CONVERSION_STATE_NULL { NULL, NULL, "", NULL, NULL }

HIDDEN int
assimp_read(struct gcv_context *context, const struct gcv_opts *UNUSED(gcv_options), const void *UNUSED(options_data), const char *source_path)
{
    struct conversion_state state = CONVERSION_STATE_NULL;
    state.input_file = source_path;
    state.fd_out = context->dbip->dbi_wdbp;

    bu_log("Assimp reader\n");
    
    return 0;
}


HIDDEN int
assimp_can_read(const char* data)
{
    /* FIXME - currently 'can_read' is unused by gcv */
    if (!data)
        return 0;

    return 1;
}

extern "C"
{
    static const struct gcv_filter gcv_conv_assimp_read = {
        "Assimp Reader", GCV_FILTER_READ, BU_MIME_MODEL_ASSIMP, assimp_can_read,
        NULL, NULL, assimp_read
    };

    static const struct gcv_filter * const filters[] = { &gcv_conv_assimp_read, NULL, NULL };

    const struct gcv_plugin gcv_plugin_info_s = { filters };

    COMPILER_DLLEXPORT const struct gcv_plugin *
    gcv_plugin_info() { return &gcv_plugin_info_s; }

}