#include <stdlib.h>
#include "../pp.h"
#include "exppp.h"

int main() {
    char * testarr[] = {
        "ASSEMBLY_STRUCTURE_MIM_LF.PRODUCT_DEFINITION_RELATIONSHIP.RELATED_PRODUCT_DEFINITION",
        "ASSEMBLY_STRUCTURE_MIM_LF.PRODUCT_DEFINITION"
    };
    int i, c;

    /* globals */
    exppp_fp = stdout;
    exppp_linelength = 40;
/*
    indent2 = 30;
    curpos = 20;
*/
    for( i = 0; i < exppp_linelength + 10; i += 15 ) {
        for( c = 2; c < exppp_linelength + 10; c += 13 ) {
            curpos = c;
            indent2 = i;
            raw( "indent2: %d, curpos: %d\n%*s||", i, c, c, "" );
            breakLongStr( testarr[0] );
            curpos = c;
            indent2 = i;
            raw( "\n%*s||", c, "" );
            breakLongStr( testarr[1] );
            raw( "\n" );
        }
    }
    curpos = 77;
    exppp_linelength = 130;
    indent2 = 17;
    raw( "\n%*s||", 77, "" );
    breakLongStr( testarr[0] );
    raw( "\n" );

    return EXIT_SUCCESS;
}
