#include "attrDescriptor.h"

AttrDescriptor::AttrDescriptor( const char * name, const TypeDescriptor * domainType,
                                Logical optional, Logical unique, AttrType_Enum at,
                                const EntityDescriptor & owner )
    : _name( name ), _domainType( domainType ), _optional( optional ),
      _unique( unique ), _attrType( at ), _owner( ( EntityDescriptor & )owner ) {
}

AttrDescriptor::~AttrDescriptor() {
}

Logical AttrDescriptor::Explicit() const {
    if( _attrType == AttrType_Explicit ) {
        return LTrue;
    }
    return LFalse;
}

Logical AttrDescriptor::Inverse() const {
    if( _attrType == AttrType_Inverse ) {
        return LTrue;
    }
    return LFalse;
}

Logical AttrDescriptor::Redefining() const {
    if( _attrType == AttrType_Redefining ) {
        return LTrue;
    }
    return LFalse;
}

Logical AttrDescriptor::Deriving() const {
    if( _attrType == AttrType_Deriving ) {
        return LTrue;
    }
    return LFalse;
}

const char * AttrDescriptor::AttrExprDefStr( std::string & s ) const {
    std::string buf;

    s = Name();
    s.append( " : " );
    if( _optional.asInt() == LTrue ) {
        s.append( "OPTIONAL " );
    }
    if( DomainType() ) {
        DomainType()->AttrTypeName( buf );
        s.append( buf );
    }
    return const_cast<char *>( s.c_str() );
}

PrimitiveType AttrDescriptor::BaseType() const {
    if( _domainType ) {
        return _domainType->BaseType();
    }
    return UNKNOWN_TYPE;
}

int AttrDescriptor::IsAggrType() const {
    return ReferentType()->IsAggrType();
}

PrimitiveType AttrDescriptor::AggrElemType() const {
    if( IsAggrType() ) {
        return ReferentType()->AggrElemType();
    }
    return UNKNOWN_TYPE;
}

const TypeDescriptor * AttrDescriptor::AggrElemTypeDescriptor() const {
    if( IsAggrType() ) {
        return ReferentType()->AggrElemTypeDescriptor();
    }
    return 0;
}

const TypeDescriptor * AttrDescriptor::NonRefTypeDescriptor() const {
    if( _domainType ) {
        return _domainType->NonRefTypeDescriptor();
    }
    return 0;
}

PrimitiveType
AttrDescriptor::NonRefType() const {
    if( _domainType ) {
        return _domainType->NonRefType();
    }
    return UNKNOWN_TYPE;
}

PrimitiveType
AttrDescriptor::Type() const {
    if( _domainType ) {
        return _domainType->Type();
    }
    return UNKNOWN_TYPE;
}

/**
 * right side of attr def
 * NOTE this returns a \'const char * \' instead of an std::string
 */
const std::string AttrDescriptor::TypeName() const {
    std::string buf;

    if( _domainType ) {
        _domainType->AttrTypeName( buf );
    }
    return buf;
}

/// an expanded right side of attr def
const char *
AttrDescriptor::ExpandedTypeName( std::string & s ) const {
    s.clear();
    if( Derived() == LTrue ) {
        s = "DERIVE  ";
    }
    if( _domainType ) {
        std::string tmp;
        return const_cast<char *>( ( s.append( _domainType->TypeString( tmp ) ).c_str() ) );
    } else {
        return 0;
    }
}

const char * AttrDescriptor::GenerateExpress( std::string & buf ) const {
    std::string sstr;
    buf = AttrExprDefStr( sstr );
    buf.append( ";\n" );
    return const_cast<char *>( buf.c_str() );
}

