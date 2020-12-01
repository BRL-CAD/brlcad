#include <stdio.h>
#include <string.h>

#include "driver.h"

#include "express/memory.h"
#include "express/factory.h"

extern struct test_def tests[];

int main(int argc, char *argv[]) {
    int status;
     
    /* enable libexpress allocator */
    MEMORYinitialize();
    FACTORYinitialize();
    
    argc--;
    status = 0;
    if (argc) {
        int test_counter = argc;
        
        /* selected tests */
        for (int i=1; i <= argc; i++) {
            for (unsigned int j=0; tests[j].name != NULL; j++) {
                const char *test_name = tests[j].name;
                int (*test_ptr) (void) = tests[j].testfunc;
                
                if (!strcmp(argv[i], test_name)) {
                    test_counter--;
                    setup();
                    status |= test_ptr();
                }
            }
        }
        
        if (test_counter)
            fprintf(stderr, "WARNING: some tests not found...\n");
    } else {
        /* all tests */
        for (unsigned int j=0; tests[j].name != NULL; j++) {
            int (*test_ptr) (void) = tests[j].testfunc;
            setup();
            status |= test_ptr();
        }
    }

    return status;
}
