/*                       T E S T L I B . C
 * BRL-CAD
 *
 * Copyright (c) 2009-2011 United States Government as represented by
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
/** @file testlibs.c
 *
 * Start of a simple library tester.  Needs a lot of work to operate
 * cleanly within the regression testing harness and to robustly load
 * and call functions.
 *
 */
#if 0
Example use:
/* FIXME: regex needs to be rewritten to find function names */
grep '^BU_EXPORT' bu.h | sed 's/^[^,]*[[:space:]*]\([a-zA-Z_][a-zA-Z0-9_]*\),.*/\1/g' | xargs ./a.out
#endif

#include <stdio.h>
#include <dlfcn.h>

int
main(int ac, char *av[])
{
    void *handle;
    int i;

    typedef void (*func_t)();
    func_t func;

    const char *error;

    printf("Opening libbu\n");
    handle = dlopen("/usr/brlcad/lib/libbu.dylib", RTLD_LAZY);
    if (!handle) {
	printf("couldn't open libbu\n");
    }

    for (i = 1; i < ac; i++) { 
	printf("Loading %s\n", av[i]);
	func = (func_t)dlsym(handle, av[i]);
	error = dlerror();
	if (error) {
	    printf("couldn't find %s\n", av[i]);
	    dlclose(handle);
	    return 1;
	}

	/* try running it? */
	//    func(0, 0, 0, 0, 0, 0, 0, 0);
    }

    return 0;
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
