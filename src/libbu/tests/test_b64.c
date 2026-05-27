/*                      T E S T _ B 6 4 . C
 * BRL-CAD
 *
 * Copyright (c) 2011-2026 United States Government as represented by
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

#include <stdio.h>
#include <string.h>

#include "bu.h"


/* Test for bu_b64_encode/bu_b64_decode for reversibility:
 *
 *   1. encode the input string
 *   2. decode the encoded string
 *   3. decoded string should be the same as input
 *
 */
static void
test_b64_encode(const signed char *str)
{
    int passed = 1;
    int decoded_size = 0;
    signed char *encoded = NULL;
    signed char *decoded = NULL;

    encoded = bu_b64_encode(str);
    decoded_size = bu_b64_decode(&decoded, encoded);

    if (BU_STR_EQUAL((const char *)str, (const char *)decoded) && decoded_size == (int)strlen((const char *)str)) {
	printf("%s -> %s -> %s [PASS]\n", str, encoded, decoded);
    } else {
	printf("%s -> %s -> %s [FAIL]\n", str, encoded, decoded);
	passed = 0;
    }
    bu_free(encoded, "free encoded");
    bu_free(decoded, "free decoded");

    if (!passed) {
	bu_exit(1, "b64 test failed");
    }
}


int
main(int ac, char *av[])
{
    if (ac != 1)
	bu_exit(1, "Usage: %s\n", av[0]);

    // Normally this file is part of bu_test, so only set this if it
    // looks like the program name is still unset.
    if (bu_getprogname()[0] == '\0')
	bu_setprogname(av[0]);

    test_b64_encode((const signed char *)"hello world!");
    test_b64_encode((const signed char *)"!@#&#$%@&#$^@(*&^%(#$@&^#*$nasty_string!<>?");

    return 0;
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
