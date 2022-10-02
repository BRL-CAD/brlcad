#include <iostream>
#include <stdint.h>
#include <stdlib.h>

#include "judyL2Array.h"
typedef judyL2Array< uint64_t, uint64_t > jl2a;

bool testFind( jl2a & j, uint64_t key, unsigned int count ) {
    jl2a::cvector * v = j.find( key );
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
            jl2a::vector::const_iterator it = v->begin();
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
    jl2a jl;
    std::cout.setf( std::ios::boolalpha );
//     std::cout << "size of judyL2Array: " << sizeof( jl ) << std::endl;
    jl.insert( 5,  12 );
    jl.insert( 6,  2 );
    jl.insert( 7,  312 );
    jl.insert( 11, 412 );
    jl.insert( 7,  313 );
    jl2a::cpair kv = jl.atOrAfter( 4 );
    std::cout << "atOrAfter test ..." << std::endl;
    if( kv.value != 0 && jl.success() ) {
        std::cout << "    key " << kv.key << "    value " << kv.value->at( 0 ) << std::endl;
    } else {
        std::cout << "    failed" << std::endl;
        pass = false;
    }

    pass &= testFind( jl, 8,  0 );
    pass &= testFind( jl, 11, 1 );
    pass &= testFind( jl, 7,  2 );

    jl.clear();

    //TODO test all of judyL2Array
    if( pass ) {
        std::cout << "All tests passed." << std::endl;
        exit( EXIT_SUCCESS );
    } else {
        std::cout << "At least one test failed." << std::endl;
        exit( EXIT_FAILURE );
    }
}