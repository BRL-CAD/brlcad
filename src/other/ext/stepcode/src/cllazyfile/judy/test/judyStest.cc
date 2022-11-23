#include <iostream>
#include <stdint.h>
#include <stdlib.h>

#include "judySArray.h"

int main() {
    bool pass = true;
    std::cout.setf( std::ios::boolalpha );

    judySArray< uint64_t > js( 255 );
    js.insert( "blah", 1234 );
    js.insert( "bah",   124 );
    js.insert( "blh",   123 );
    js.insert( "bla",   134 );
    js.insert( "bh",    234 );


    judySArray< uint64_t >::pair kv = js.atOrAfter( "ab" );
    //TODO if()...
    std::cout << "k " << kv.key << " v " << kv.value << std::endl;

    long v = js.find( "sdafsd" );
    if( v != 0 || js.success() ) {
        std::cout << "find: false positive - v: " << v << " success: " << js.success() << std::endl;
        pass = false;
    }
    v = js.find( "bah" );
    if( v != 124 || !js.success() ) {
        std::cout << "find: false negative - v: " << v << " success: " << js.success() << std::endl;
        pass = false;
    }


    //TODO test all of judySArray
    if( pass ) {
        std::cout << "All tests passed." << std::endl;
        exit( EXIT_SUCCESS );
    } else {
        std::cout << "At least one test failed." << std::endl;
        exit( EXIT_FAILURE );
    }
}