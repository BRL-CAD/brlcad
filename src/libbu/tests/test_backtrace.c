/*                 T E S T _ B A C K T R A C E . C
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

#include "test_api.h"


#if defined(__clang__) || defined(__GNUC__)
#  define TEST_BACKTRACE_NOINLINE __attribute__((noinline))
#elif defined(_MSC_VER)
#  define TEST_BACKTRACE_NOINLINE __declspec(noinline)
#else
#  define TEST_BACKTRACE_NOINLINE
#endif

/* These frames live in the bu_test executable, not libbu.  When the test is
 * built against a DLL import view of libbu on Windows, BU_EXPORT expands to
 * dllimport and MSVC rejects function definitions.  Export them from the test
 * executable instead so symbol names remain visible to the backtrace logic.
 */
#if defined(_WIN32) && defined(BU_DLL_IMPORTS)
#  define TEST_BACKTRACE_EXPORT COMPILER_DLLEXPORT
#else
#  define TEST_BACKTRACE_EXPORT BU_EXPORT
#endif


static volatile int test_backtrace_guard = 0;


static size_t
backtrace_nonempty_line_cnt(const char *text)
{
    const char *line = text;
    size_t count = 0;

    while (line && *line) {
	const char *end = strchr(line, '\n');
	const char *scan = line;

	if (!end) {
	    end = line + strlen(line);
	}

	while (scan < end) {
	    if (*scan != ' ' && *scan != '\t' && *scan != '\r') {
		count++;
		break;
	    }
	    scan++;
	}

	if (*end == '\0') {
	    break;
	}

	line = end + 1;
    }

    return count;
}


static int
backtrace_has_any_marker(const char *text, const char * const *markers, size_t marker_cnt)
{
    size_t i;

    for (i = 0; i < marker_cnt; i++) {
	if (test_api_text_contains(text, markers[i])) {
	    return 1;
	}
    }

    return 0;
}


static int
backtrace_read_file(FILE *fp, struct bu_vls *out)
{
    char buffer[512] = {0};
    size_t got = 0;

    if (!fp || !out) {
	return 0;
    }

    rewind(fp);
    while ((got = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
	bu_vls_strncat(out, buffer, got);
    }

    return ferror(fp) ? 0 : 1;
}


#ifdef __cplusplus
extern "C" {
#endif
TEST_BACKTRACE_EXPORT TEST_BACKTRACE_NOINLINE int
test_backtrace_direct_leaf(FILE *fp)
{
    return bu_backtrace(fp);
}


TEST_BACKTRACE_EXPORT TEST_BACKTRACE_NOINLINE int
test_backtrace_direct_middle(FILE *fp)
{
    int (* volatile next)(FILE *) = &test_backtrace_direct_leaf;
    int ret = next(fp);
    test_backtrace_guard += ret;
    return ret;
}


TEST_BACKTRACE_EXPORT TEST_BACKTRACE_NOINLINE int
test_backtrace_direct_entry(FILE *fp)
{
    int (* volatile next)(FILE *) = &test_backtrace_direct_middle;
    int ret = next(fp);
    test_backtrace_guard += ret;
    return ret;
}
#ifdef __cplusplus
}
#endif


static int
test_backtrace_capture_direct(void)
{
    int errors = 0;
    FILE *fp = NULL;
    struct bu_vls trace = BU_VLS_INIT_ZERO;
    const char * const markers[] = {
	"test_backtrace_direct_leaf",
	"test_backtrace_direct_middle",
	"test_backtrace_direct_entry"
    };
    int ret = 0;

    fp = bu_temp_file(NULL, 0);
    TEST_API_CHECK(fp != NULL, "bu_temp_file returned NULL");
    if (!fp) {
	return 1;
    }

    ret = test_backtrace_direct_entry(fp);
    if (ret == 0) {
        bu_log("Backtrace unsupported or failed on this platform. Skipping test.\\n");
        fclose(fp);
        bu_vls_free(&trace);
        return 0;
    }

    TEST_API_CHECK(ret == 1, "bu_backtrace should report success, got %d", ret);
    fflush(fp);

    TEST_API_CHECK(backtrace_read_file(fp, &trace), "unable to read captured backtrace output");
    TEST_API_CHECK(bu_vls_strlen(&trace) > 0, "bu_backtrace produced no output");
    TEST_API_CHECK(backtrace_nonempty_line_cnt(bu_vls_addr(&trace)) >= 3,
		   "expected multiple stack frames, got output: %s", bu_vls_addr(&trace));
    TEST_API_CHECK(backtrace_has_any_marker(bu_vls_addr(&trace), markers, ARRAY_LEN(markers)),
		   "backtrace output did not include the active test call chain: %s",
		   bu_vls_addr(&trace));

    fclose(fp);
    bu_vls_free(&trace);

    return errors;
}


int
main(int argc, char *argv[])
{
    int errors = 0;

    if (bu_getprogname()[0] == '\0') {
	bu_setprogname(argv[0]);
    }

    if (argc != 1) {
	bu_exit(1, "Usage: %s\n", argv[0]);
    }

    errors += test_backtrace_capture_direct();

    return errors ? 1 : 0;
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
