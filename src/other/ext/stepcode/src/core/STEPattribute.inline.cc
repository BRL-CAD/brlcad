
/*
* NIST STEP Core Class Library
* clstepcore/STEPattribute.inline.cc
* April 1997
* K. C. Morris
* David Sauder

* Development of this software was funded by the United States Government,
* and is not subject to copyright.
*/

#include <assert.h>
#include "core/STEPattribute.h"
#include "core/sdai.h"
#include "core/ExpDict.h"


///  This is needed so that STEPattribute's can be passed as references to inline functions
STEPattribute::STEPattribute( const STEPattribute & a )
    : _derive( 0 ), _redefAttr( 0 ), aDesc( a.aDesc ), refCount( 0 ) {}

///  INTEGER
STEPattribute::STEPattribute( const class AttrDescriptor & d, SDAI_Integer * p )
    : _derive( 0 ), _redefAttr( 0 ), aDesc( &d ), refCount( 0 )  {
    ptr.i = p;
    assert( &d ); //ensure that the AttrDescriptor is not a null pointer
}

///  BINARY
STEPattribute::STEPattribute( const class AttrDescriptor & d, SDAI_Binary * p )
    : _derive( 0 ), _redefAttr( 0 ), aDesc( &d ), refCount( 0 ) {
    ptr.b = p;
    assert( &d ); //ensure that the AttrDescriptor is not a null pointer
}

///  STRING
STEPattribute::STEPattribute( const class AttrDescriptor & d, SDAI_String * p )
    : _derive( 0 ),  _redefAttr( 0 ), aDesc( &d ), refCount( 0 ) {
    ptr.S = p;
    assert( &d ); //ensure that the AttrDescriptor is not a null pointer
}

///  REAL & NUMBER
STEPattribute::STEPattribute( const class AttrDescriptor & d, SDAI_Real * p )
    : _derive( 0 ), _redefAttr( 0 ), aDesc( &d ), refCount( 0 ) {
    ptr.r = p;
    assert( &d ); //ensure that the AttrDescriptor is not a null pointer
}

///  ENTITY
STEPattribute::STEPattribute( const class AttrDescriptor & d,
                              SDAI_Application_instance * *p )
    : _derive( 0 ),  _redefAttr( 0 ), aDesc( &d ), refCount( 0 ) {
    ptr.c = p;
    assert( &d ); //ensure that the AttrDescriptor is not a null pointer
}

///  AGGREGATE
STEPattribute::STEPattribute( const class AttrDescriptor & d, STEPaggregate * p )
    : _derive( 0 ),  _redefAttr( 0 ), aDesc( &d ), refCount( 0 ) {
    ptr.a =  p;
    assert( &d ); //ensure that the AttrDescriptor is not a null pointer
}

///  ENUMERATION  and Logical
STEPattribute::STEPattribute( const class AttrDescriptor & d, SDAI_Enum * p )
    : _derive( 0 ),  _redefAttr( 0 ), aDesc( &d ), refCount( 0 ) {
    ptr.e = p;
    assert( &d ); //ensure that the AttrDescriptor is not a null pointer
}

///  SELECT
STEPattribute::STEPattribute( const class AttrDescriptor & d,
                              class SDAI_Select * p )
    : _derive( 0 ),  _redefAttr( 0 ), aDesc( &d ), refCount( 0 ) {
    ptr.sh = p;
    assert( &d ); //ensure that the AttrDescriptor is not a null pointer
}

///  UNDEFINED
STEPattribute::STEPattribute( const class AttrDescriptor & d, SCLundefined * p )
    : _derive( 0 ), _redefAttr( 0 ), aDesc( &d ), refCount( 0 ) {
    ptr.u = p;
    assert( &d ); //ensure that the AttrDescriptor is not a null pointer
}


/// name is the same even if redefined
const char * STEPattribute::Name() const {
    return aDesc->Name();
}

const char * STEPattribute::TypeName() const {
    if( _redefAttr )  {
        return _redefAttr->TypeName();
    }
    return aDesc->TypeName();
}

BASE_TYPE STEPattribute::Type() const {
    if( _redefAttr )  {
        return _redefAttr->Type();
    }
    return aDesc->Type();
}

BASE_TYPE STEPattribute::NonRefType() const {
    if( _redefAttr )  {
        return _redefAttr->NonRefType();
    } else if( aDesc ) {
        return aDesc->NonRefType();
    }
    return UNKNOWN_TYPE;
}

BASE_TYPE STEPattribute::BaseType() const {
    if( _redefAttr )  {
        return _redefAttr->BaseType();
    }
    return aDesc->BaseType();
}

const TypeDescriptor * STEPattribute::ReferentType() const {
    if( _redefAttr )  {
        return _redefAttr->ReferentType();
    }
    return aDesc->ReferentType();
}

int STEPattribute::Nullable() const {
    if( _redefAttr )  {
        return _redefAttr->Nullable();
    }
    return ( aDesc->Optionality().asInt() == LTrue );
}
