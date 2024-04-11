#include "derivedAttribute.h"

Derived_attribute::Derived_attribute( const char * name, const TypeDescriptor * domainType,
                                      Logical optional, Logical unique, AttrType_Enum at, const EntityDescriptor & owner )
    : AttrDescriptor( name, domainType, optional, unique, at, owner ) {
    _initializer = ( const char * )0;
}

Derived_attribute::~Derived_attribute() {
}

const char * Derived_attribute::AttrExprDefStr( std::string & s ) const {
    std::string buf;

    s.clear();
    if( Name() && strchr( Name(), '.' ) ) {
        s = "SELF\\";
    }
    s.append( Name() );
    s.append( " : " );
    if( DomainType() ) {
        DomainType()->AttrTypeName( buf );
        s.append( buf );
    }
    if( _initializer ) { // this is supposed to exist for a derived attribute.
        s.append( " \n\t\t:= " );
        s.append( _initializer );
    }
    return const_cast<char *>( s.c_str() );
}
