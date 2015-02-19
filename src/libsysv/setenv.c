/*                        S E T E N V . C
 * BRL-CAD
 *
 * Copyright (c) 2014 United States Government as represented by
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

#include "common.h"

#ifndef HAVE_SETENV

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "bio.h"
#include "sysv.h"


int
setenv(const char *name, const char *value, int overwrite)
{
    int errcode = 0;

    if (!overwrite) {
	size_t envsize = 0;

#ifdef HAVE_GETENV_S
	errcode = getenv_s(&envsize, NULL, 0, name);
#else
	if (getenv(name) == NULL)
	    errcode = EINVAL;
#endif
	if (errcode || envsize)
	    return errcode;
    }
#ifdef HAVE__PUTENV_S
    return _putenv_s(name, value);
#else
    {
	size_t maxlen = strlen(name)+strlen(value)+2;
	char *keyval = (char *)malloc(maxlen);
	if (!keyval)
	    return ENOMEM;
	snprintf(keyval, maxlen, "%s=%s", name, value);
	return putenv(keyval);
    }
#endif
}

#endif /* HAVE_SETENV */


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
