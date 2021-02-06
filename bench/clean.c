/*                         C L E A N . C
 * BRL-CAD
 *
 * Copyright (c) 2011-2021 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following
 * disclaimer in the documentation and/or other materials provided
 * with the distribution.
 *
 * 3. The name of the author may not be used to endorse or promote
 * products derived from this software without specific prior written
 * permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/** @file clean.c
 *
 * utility functions to clean output files
 *
 */

#include "common.h"

#include "bu/file.h"
#include "bu/str.h"
#include "bu/vls.h"


/* TODO: !!! needed */
int bu_file_move(const char *fn1, const char *fn2) {if (!fn1 || !fn2) return -1; return 0;}
extern int bu_file_copy(const char *fn1, const char *fn2);


/**
 * conditionally removes or saves any .pix or .log files output.
 */
void
clean_obstacles(const char *base_filename, int id)
{
    struct bu_vls vp = BU_VLS_INIT_ZERO;
    struct bu_vls vpid = BU_VLS_INIT_ZERO;

    if (BU_STR_EMPTY(base_filename))
	return;

    /* look for an image file */
    bu_vls_printf(&vp, "%s.pix", base_filename);
    if (bu_file_exists(bu_vls_cstr(&vp), NULL)) {
	bu_vls_printf(&vpid, "%s-%d.pix", base_filename, id);
	if (bu_file_exists(bu_vls_cstr(&vpid), NULL)) {
	    bu_file_delete(bu_vls_cstr(&vp));
	}
	bu_file_move(bu_vls_cstr(&vp), bu_vls_cstr(&vpid));
    }

    /* look for a log file */
    bu_vls_printf(&vp, "%s.log", base_filename);
    if (bu_file_exists(bu_vls_cstr(&vp), NULL)) {
	bu_vls_printf(&vpid, "%s-%d.log", base_filename, id);
	if (bu_file_exists(bu_vls_cstr(&vpid), NULL)) {
	    bu_file_delete(bu_vls_cstr(&vp));
	}
	bu_file_move(bu_vls_cstr(&vp), bu_vls_cstr(&vpid));
    }
}


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
