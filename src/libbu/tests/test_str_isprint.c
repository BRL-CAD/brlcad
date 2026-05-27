/*              T E S T _ S T R _ I S P R I N T . C
 * BRL-CAD
 *
 * Copyright (c) 2007-2026 United States Government as represented by
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

#include <string.h>
#include <ctype.h>

#include "bu.h"


static int
test_str_isprint(const char *inp , int texp)
{
    int res;
    res = bu_str_isprint(inp);
    if (res == texp) {

	if (res)
	    printf("Testing with string : %10s is printable->PASSED!\n", inp);
	else
	    printf("Given string not printable->PASSED!\n");

	return 1;
    }
    printf("Failed\n");
    return 0;
}


int
main(int argc, char *argv[])
{
    int test_num = 0;

    // Normally this file is part of bu_test, so only set this if it
    // looks like the program name is still unset.
    if (bu_getprogname()[0] == '\0')
	bu_setprogname(argv[0]);

    if (argc < 2) {
	fprintf(stderr, "Usage: %s {test_num}\n", argv[0]);
	return 1;
    }

    sscanf(argv[1], "%d", &test_num);

    switch (test_num) {
	case 1:
	    return !test_str_isprint("abc", 1);
	case 2:
	    /* \n is end of line -not printable */
	    return !test_str_isprint("abc123\n", 0);
	case 3:
	    return !test_str_isprint("abc123\\n1!", 1);
	case 4:
	    /* \t is horizontal tab - not printable */
	    return !test_str_isprint("123\txyz", 0);
	case 5:
	    return !test_str_isprint("#$ ab12", 1);
	case 6:
	    /* \n is end of line -not printable */
	    return !test_str_isprint("#$%\n 748", 0);
	case 7:
	    /* \r is carriage return - not printable */
	    return !test_str_isprint("#$^\ry", 0);
    }

    return 1;
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8 cino=N-s
 */
