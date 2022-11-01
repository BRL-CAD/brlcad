/** \file test_operators_SDAI_Select.cc
 * tests for SDAI_Select operators
 * currently only implements a test for operator=
 * TODO implement others
 */

#include <iostream>

#include "ExpDict.h"
#include "baseType.h"
#include "sdaiSelect.h"

using namespace std;

class TestSdaiSelect: public SDAI_Select {
    public:
        TestSdaiSelect( SelectTypeDescriptor * s, TypeDescriptor * t, BASE_TYPE b, SDAI_String v, ErrorDescriptor e ):
                SDAI_Select( s, t ) {
            base_type = b;
            val = v;
            _error = e;
        }
        TestSdaiSelect(): SDAI_Select() {}
        TestSdaiSelect & operator=( const TestSdaiSelect & other ) {
            SDAI_Select::operator=( other );
            return *this;
        }
        SDAI_Select& operator=( const SDAI_Select& other ) {
            SDAI_Select::operator=( other );
            return *this;
        }

        /// \return true for match
        bool compare( SelectTypeDescriptor * s, TypeDescriptor * t, BASE_TYPE b, SDAI_String * v, ErrorDescriptor * e ) {
            bool pass = _type == s;
            pass &= underlying_type == t;
            pass &= base_type == b;
            pass &= val == v->c_str();
            pass &= ( _error.severity() == e->severity() ) && ( _error.debug_level() == e->debug_level() );
            return pass;
        }

        // dummy implementations of pure virtual funcs
        const TypeDescriptor* AssignEntity( SDAI_Application_instance * /*i*/ ) { return (TypeDescriptor *)0; }
        SDAI_Select* NewSelect(){ return (SDAI_Select *)0; }
        BASE_TYPE ValueType() const { return sdaiBOOLEAN; }
        void STEPwrite_content( std::ostream& /*o*/, const char* /*a*/ ) const {}
        Severity StrToVal_content( const char* /*a*/, InstMgrBase* /*m*/ ) { return SEVERITY_NULL; }
        Severity STEPread_content(std::istream& i, InstMgrBase* m, const char* c, int n, const char* d) {
            (void)i; (void)m; (void)c; (void)n; (void)d; return SEVERITY_NULL;
        }
};

/// \return true for success
bool testOperatorEq() {
    Schema * sch = 0;
    SelectTypeDescriptor s( 5, "a", sdaiBOOLEAN, sch, "b" );
    TypeDescriptor t;
    BASE_TYPE b = sdaiAGGR;
    SDAI_String v( "test string" );
    ErrorDescriptor e( SEVERITY_MAX, 99 );
    TestSdaiSelect s1( &s, &t, b, v, e );
    TestSdaiSelect s2;

    s2 = s1;
    if( s2.compare( &s, &t, b, &v, &e ) ) {
        return true;
    } else {
        cerr << __FILE__ << ":" << __LINE__ << " - error: test for SDAI_Select::operator= failed" << endl;
        return false;
    }
}

int main( int /*argc*/, char ** /*argv*/ ) {
    bool pass = true;
    pass &= testOperatorEq();
    //TODO test other operators
    cerr << "FIXME this test is incomplete!" << endl;
    if( pass ) {
        exit( EXIT_SUCCESS );
    } else {
        exit( EXIT_FAILURE );
    }
}
