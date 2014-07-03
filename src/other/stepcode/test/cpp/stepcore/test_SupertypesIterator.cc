//test SubtypesIterator, SupertypesIterator

//what about an iterator for inverse attrs?

//subtypesiterator shouldn't need tested separately from supertypesiterator since there is very little difference

#include "SubSuperIterators.h"
#include "ExpDict.h"
#include <iomanip>

int main( int /*argc*/, char ** /*argv*/ ) {
    Schema * a = 0;
    Logical b( LFalse );
    char buf[20][2]  = { { '\0' } };
    int i, d;
    EntityDescriptor * descriptors[20], ed( "ed", a, b, b );
    bool failed = false;

    //create 20 more ed's
    for( i = 0; i < 20; i++ ) {
        buf[i][0] = 'a' + i; //ed names are 1st 20 lowercase chars
        descriptors[i] = new EntityDescriptor( buf[i], a, b, b );
    }
    //link the ed's together
    ed.AddSupertype( descriptors[0] );
    for( i = 0; i < 10; i++ ) {
        descriptors[i]->AddSupertype( descriptors[i + 1] );
    }
    for( i = 11; i < 20; i++ ) {
        descriptors[5]->AddSupertype( descriptors[i] );
    }

    //print the ed's
    i = 0;
    d = 0;
    std::cout << "head, name " << ed.Name() << std::endl;
    supertypesIterator iter( &ed );
    for( ; !iter.empty(); iter++ ) {
        if( !iter.hasNext() ) { //hasNext should be false once and once only
            i++;
        }
        if( iter.depth() > d ) {
            d = iter.depth();
        }
        std::cout << "position " << std::setw( 3 ) << iter.pos() << ", name " << std::setw( iter.depth() ) << iter->Name() << std::endl;
    }
    if( iter.pos() == 20 ) {
        std::cout << "success" << std::endl;
    } else {
        std::cout << "expected 20, got " << iter.pos() << std::endl;
        failed = true;
    }
    if( i != 1 ) {
        std::cout << "problem with hasNext(): expected 1, got " << i << std::endl;
        failed = true;
    }
    if( d != 11 ) {
        std::cout << "problem with depth(): expected 11, got " << d << std::endl;
        failed = true;
    }

    supertypesIterator it2( &ed ), it3, uninitializedIter;
    it3 = it2;

    //test operator==, operator!=
    if( ( it2 == iter ) || ( it2 == uninitializedIter ) ) {
        std::cout << "problem with operator== at " << __LINE__ << std::endl;
        failed = true;
    } else {
        std::cout << "operator== passed 1st" << std::endl;
    }

    if( !( it2 == it3 ) ) {
        std::cout << "problem with operator== at " << __LINE__ << std::endl;
        failed = true;
    } else {
        std::cout << "operator== passed 2nd" << std::endl;
    }

    if( !( it2 != iter ) ) {
        std::cout << "problem with operator!=" << std::endl;
        failed = true;
    } else {
        std::cout << "operator!= passed" << std::endl;
    }

    it3 = it2;
    ++it3;

    // operator>
    if( ( it3 > it2 ) != LTrue ) {
        std::cout << "problem with operator>, expected LTrue" << std::endl;
        failed = true;
    } else {
        std::cout << "operator>  passed LTrue" << std::endl;
    }
    if( ( uninitializedIter > it2 ) != LUnknown ) {
        std::cout << "problem with operator>, expected LUnknown" << std::endl;
        failed = true;
    } else {
        std::cout << "operator>  passed LUnknown" << std::endl;
    }
    if( ( it2 > it2 ) != LFalse ) {
        std::cout << "problem with operator>, expected LFalse" << std::endl;
        failed = true;
    } else {
        std::cout << "operator>  passed LFalse" << std::endl;
    }

    // operator<
    if( ( it2 < it3 ) != LTrue ) {
        std::cout << "problem with operator<, expected LTrue" << std::endl;
        failed = true;
    } else {
        std::cout << "operator<  passed LTrue" << std::endl;
    }
    if( ( it2 < uninitializedIter ) != LUnknown ) {
        std::cout << "problem with operator<, expected LUnknown" << std::endl;
        failed = true;
    } else {
        std::cout << "operator<  passed LUnknown" << std::endl;
    }
    if( ( it2 < it2 ) != LFalse ) {
        std::cout << "problem with operator<, expected LFalse" << std::endl;
        failed = true;
    } else {
        std::cout << "operator<  passed LFalse" << std::endl;
    }

    // operator<=
    if( ( it2 <= it2 ) != LTrue ) {
        std::cout << "problem with operator<=, expected LTrue" << std::endl;
        failed = true;
    } else {
        std::cout << "operator<= passed LTrue" << std::endl;
    }
    if( ( it2 <= uninitializedIter ) != LUnknown ) {
        std::cout << "problem with operator<=, expected LUnknown" << std::endl;
        failed = true;
    } else {
        std::cout << "operator<= passed LUnknown" << std::endl;
    }
    if( ( it3 <= it2 ) != LFalse ) {
        std::cout << "problem with operator<=, expected LFalse" << std::endl;
        failed = true;
    } else {
        std::cout << "operator<= passed LFalse" << std::endl;
    }

    // operator>=
    if( ( it2 >= it2 ) != LTrue ) {
        std::cout << "problem with operator>=, expected LTrue" << std::endl;
        failed = true;
    } else {
        std::cout << "operator>= passed LTrue" << std::endl;
    }
    if( ( it2 >= uninitializedIter ) != LUnknown ) {
        std::cout << "problem with operator>=, expected LUnknown" << std::endl;
        failed = true;
    } else {
        std::cout << "operator>= passed LUnknown" << std::endl;
    }
    if( ( it2 >= it3 ) != LFalse ) {
        std::cout << "problem with operator>=, expected LFalse" << std::endl;
        failed = true;
    } else {
        std::cout << "operator>= passed LFalse" << std::endl;
    }
    /// still need operator* >=

    it3.reset();
    const EntityDescriptor * e = *it3;
    const char * n = "a";
    if( strcmp( e->Name(), n ) ) {
        std::cout << "problem with operator*" << std::endl;
        failed = true;
    } else {
        std::cout << "operator*  passed " << std::endl;
    }

    if( failed ) {
        exit( EXIT_FAILURE );
    } else {
        exit( EXIT_SUCCESS );
    }
}
/* output:
 *
 * head, name ed
 * position   0, name a
 * position   1, name  b
 * position   2, name   c
 * position   3, name    d
 * position   4, name     e
 * position   5, name      f
 * position   6, name       g
 * position   7, name       l
 * position   8, name       m
 * position   9, name       n
 * position  10, name       o
 * position  11, name       p
 * position  12, name       q
 * position  13, name       r
 * position  14, name       s
 * position  15, name       t
 * position  16, name        h
 * position  17, name         i
 * position  18, name          j
 * position  19, name           k
 * success
 * operator== passed 1st
 * operator== passed 2nd
 * operator!= passed
 * operator>  passed LTrue
 * operator>  passed LUnknown
 * operator>  passed LFalse
 * operator<  passed LTrue
 * operator<  passed LUnknown
 * operator<  passed LFalse
 * operator<= passed LTrue
 * operator<= passed LUnknown
 * operator<= passed LFalse
 * operator>= passed LTrue
 * operator>= passed LUnknown
 * operator>= passed LFalse
 * operator*  passed
 *
 */