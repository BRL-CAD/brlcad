/*                      T E S T _ V L B . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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

#include "bio.h"
#include "test_api.h"


static int
test_vlb_api(void)
{
    int errors = 0;
    struct bu_vlb invalid = BU_VLB_INIT_ZERO;
    struct bu_vlb vlb = BU_VLB_INIT_ZERO;
    struct test_api_log_capture capture;
    unsigned char prefix[] = {0x00, 0x01, 0x7f};
    unsigned char suffix[] = {'o', 'k'};
    unsigned char tiny_bytes[] = {0x00, 0xab, 0xff};
    unsigned char large[600];
    unsigned char expected[603];
    unsigned char file_buf[603];
    FILE *fp = NULL;
    size_t i;
    size_t read_cnt = 0;

    for (i = 0; i < sizeof(large); i++) {
	large[i] = (unsigned char)(i % 251);
    }
    memcpy(expected, prefix, sizeof(prefix));
    memcpy(expected + sizeof(prefix), large, sizeof(large));

    test_api_log_capture_begin(&capture);
    bu_vlb_initialize(&invalid, 0);
    test_api_log_capture_end(&capture);
    TEST_API_CHECK(test_api_text_contains(bu_vls_addr(&capture.text), "illegal initial size"),
		   "invalid initialization should log a warning");
    TEST_API_CHECK(invalid.bufCapacity >= 512,
		   "zero-size initialization should fall back to default block size");
    test_api_log_capture_free(&capture);

    bu_vlb_initialize(&vlb, 8);
    TEST_API_CHECK(BU_VLB_IS_INITIALIZED(&vlb), "vlb should report initialized after bu_vlb_initialize");
    TEST_API_CHECK(bu_vlb_buflen(&vlb) == 0, "new vlb should start empty");

    bu_vlb_write(&vlb, prefix, sizeof(prefix));
    bu_vlb_write(&vlb, large, sizeof(large));
    TEST_API_CHECK(bu_vlb_buflen(&vlb) == sizeof(expected), "unexpected vlb length after writes");
    TEST_API_CHECK(vlb.bufCapacity >= sizeof(expected), "vlb capacity should grow to fit written data");
    TEST_API_CHECK(memcmp(bu_vlb_addr(&vlb), expected, sizeof(expected)) == 0,
		   "vlb bytes should preserve write order");

    fp = tmpfile();
    TEST_API_CHECK(fp != NULL, "tmpfile failed");
    if (fp) {
	bu_vlb_print(&vlb, fp);
	rewind(fp);
	read_cnt = fread(file_buf, 1, sizeof(file_buf), fp);
	TEST_API_CHECK(read_cnt == sizeof(expected), "unexpected byte count after vlb_print");
	TEST_API_CHECK(memcmp(file_buf, expected, sizeof(expected)) == 0,
		       "vlb_print should emit the raw buffer bytes");
	fclose(fp);
    }

    bu_vlb_reset(&vlb);
    TEST_API_CHECK(bu_vlb_buflen(&vlb) == 0, "reset should empty the buffer");
    bu_vlb_write(&vlb, suffix, sizeof(suffix));
    TEST_API_CHECK(bu_vlb_buflen(&vlb) == sizeof(suffix), "unexpected length after reset and rewrite");
    TEST_API_CHECK(memcmp(bu_vlb_addr(&vlb), suffix, sizeof(suffix)) == 0,
		   "rewrite after reset should start from byte zero");

    bu_vlb_reset(&vlb);
    bu_vlb_write(&vlb, tiny_bytes, sizeof(tiny_bytes));
    test_api_log_capture_begin(&capture);
    bu_pr_vlb("vlb api", &vlb);
    test_api_log_capture_end(&capture);
    TEST_API_CHECK(test_api_text_contains(bu_vls_addr(&capture.text), "vlb api"),
		   "pretty-printer should include the title");
    TEST_API_CHECK(test_api_text_contains(bu_vls_addr(&capture.text), "00"),
		   "pretty-printer should include hexadecimal bytes");
    TEST_API_CHECK(test_api_text_contains(bu_vls_addr(&capture.text), "ab"),
		   "pretty-printer should include lower-case hexadecimal bytes");
    TEST_API_CHECK(test_api_text_contains(bu_vls_addr(&capture.text), "ff"),
		   "pretty-printer should include all bytes");
    test_api_log_capture_free(&capture);

    bu_vlb_free(&vlb);
    bu_vlb_free(&invalid);

    return errors ? BRLCAD_ERROR : BRLCAD_OK;
}


int
main(int UNUSED(argc), char *argv[])
{
    if (bu_getprogname()[0] == '\0') {
	bu_setprogname(argv[0]);
    }

    return test_vlb_api();
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
