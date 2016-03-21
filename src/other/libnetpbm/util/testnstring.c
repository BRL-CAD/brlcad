#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

#include "nstring.h"

#define true (1)
#define false (0)

int
main(int argc, char **argv) {

    char snprintfNResult[80];
    const char * asprintfNResult; 

    printf("Hello world.\n");

    snprintfN(snprintfNResult, 19, "snprintfN test truncated");

    printf("snprintfN result='%s'\n", snprintfNResult);

    asprintfN(&asprintfNResult, "asprintf'ed string %d %u %s", 
              5, 89, "substring");

    printf("asprintfN result='%s'\n", asprintfNResult);

    strfree(asprintfNResult);
    return 0;
}
