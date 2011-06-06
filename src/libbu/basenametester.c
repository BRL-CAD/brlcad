#include "common.h"

#include <stdlib.h>
#include <stdio.h>

#include "bu.h"

#include <string.h>
#include <libgen.h>


/* Test against basename UNIX tool */
void
automatic_test(const char *input)
{

    char *ans, buf_input[1000];
    char *res;

    if (input)
	bu_strlcpy(buf_input, input, strlen(input)+1);

    /* build UNIX 'basename' command */
    if (!input)
	ans = basename(NULL);
    else
	ans = basename(buf_input);

    if (!input)
	res = bu_basename(NULL);
    else
	res = bu_basename(buf_input);

    if (BU_STR_EQUAL(res, ans))
        printf("%24s -> %24s [PASSED]\n", input, res);
    else 
        printf("%24s -> %24s (should be: %s) [FAIL]\n", input, res, ans);

    bu_free(res, NULL);
}


int
main(int ac, char *av[])
{
    char input[1000] = {0};

    /* pre-define tests */
    printf("Performing pre-defined tests:\n");
    automatic_test("/usr/dir/file");
    automatic_test("/usr/dir/");
    automatic_test("/usr/");
    automatic_test("/usr");
    automatic_test("usr");
    automatic_test("/usr/some long/file");
    automatic_test("/usr/some file");
    automatic_test("/a test file");
    automatic_test("another file");
    automatic_test("/");
    automatic_test("/////");
    automatic_test(".");
    automatic_test("..");
    automatic_test("...");
    automatic_test("   ");
    automatic_test("");
    automatic_test(NULL);

    /* user tests */
    if (ac > 1) {
	printf("Enter a string:\n");
	fgets(input, 1000, stdin);
	if (strlen(input) > 0)
	    input[strlen(input)-1] = '\0';
	automatic_test(input);
    }

    printf("%s: testing complete\n", av[0]);
    return 0;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
