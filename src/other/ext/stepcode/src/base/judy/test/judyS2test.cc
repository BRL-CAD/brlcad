#include <iostream>
#include <stdint.h>
#include <stdlib.h>

#include "judyS2Array.h"

typedef judyS2Array< uint64_t > js2a;

bool testFind( js2a & j, const char * key, unsigned int count ) {
    js2a::cvector * v = j.find( key );
    std::cout << "find: key " << key << " ..." << std::endl;
    if( count > 0 ) {
        if( !v || !j.success() || ( v->size() != count ) ) {
            std::cout << "    false negative - v: " << v << " success: " << j.success();
            if( v ) {
                std::cout << "    expected count: " << count << " actual: " << v->size();
            }
            std::cout << std::endl;
            return false;
        } else {
            // note - this doesn't verify that the right keys are returned, just the right number!
            js2a::vector::const_iterator it = v->begin();
            std::cout << "    correct number of values -";
            for( ; it != v->end(); it++ ) {
                std::cout << " " << *it;
            }
            std::cout << std::endl;
        }
    } else {
        if( v || j.success() ) {
            std::cout << "    false positive - v: " << v << " success: " << j.success() << std::endl;
            return false;
        } else {
            std::cout << "    not found, as expected." << std::endl;
        }
    }
    return true;
}

int main() {
    bool pass = true;
    std::cout.setf( std::ios::boolalpha );

    js2a js( 255 );
    js.insert( "blah", 1234 );
    js.insert( "bah",   124 );
    js.insert( "blh",   123 );
    js.insert( "blh",  4123 );
    js.insert( "bla",   134 );
    js.insert( "bh",    234 );

    js2a::cpair kv = js.atOrAfter( "ab" );
    std::cout << "atOrAfter test ..." << std::endl;
    if( kv.value != 0 && js.success() ) {
        std::cout << "    key " << kv.key << "    value " << kv.value->at( 0 ) << std::endl;
    } else {
        std::cout << "    failed" << std::endl;
        pass = false;
    }

    pass &= testFind( js, "sdafsd",  0 );
    pass &= testFind( js, "bah",  1 );
    pass &= testFind( js, "blh",  2 );

    js.clear();

    //TODO test all of judyS2Array
    if( pass ) {
        std::cout << "All tests passed." << std::endl;
        exit( EXIT_SUCCESS );
    } else {
        std::cout << "At least one test failed." << std::endl;
        exit( EXIT_FAILURE );
    }
}