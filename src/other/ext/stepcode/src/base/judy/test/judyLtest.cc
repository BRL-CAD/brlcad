#include <iostream>
#include <stdint.h>
#include <stdlib.h>

#include "judyLArray.h"

int main() {
    std::cout.setf( std::ios::boolalpha );
    judyLArray< uint64_t, uint64_t > jl;
    std::cout << "size of judyLArray: " << sizeof( jl ) << std::endl;
    jl.insert( 5, 12 );
    jl.insert( 6, 2 );
    jl.insert( 7, 312 );
    jl.insert( 8, 412 );
    judyLArray< uint64_t, uint64_t >::pair kv = jl.atOrAfter( 4 );
    std::cout << "k " << kv.key << " v " << kv.value << std::endl;

    long v = jl.find( 11 );
    if( v != 0 || jl.success() ) {
        std::cout << "find: false positive - v: " << v << " success: " << jl.success() << std::endl;
        exit( EXIT_FAILURE );
    }
    v = jl.find( 7 );
    if( v != 312 || !jl.success() ) {
        std::cout << "find: false negative - v: " << v << " success: " << jl.success() << std::endl;
        exit( EXIT_FAILURE );
    }

    jl.clear();

    //TODO test all of judyLArray
    exit( EXIT_SUCCESS );
}