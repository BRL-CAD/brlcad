#include "uniquenessRule.h"

#include <string.h>

Uniqueness_rule::Uniqueness_rule()
: _parent_entity( 0 ) {
}

Uniqueness_rule::Uniqueness_rule( const Uniqueness_rule & ur ): Dictionary_instance() {
    _label = ur._label;
    _parent_entity = ur._parent_entity;
}

Uniqueness_rule::~Uniqueness_rule() {
    // don't delete _parent_entity
}

Uniqueness_rule__set::Uniqueness_rule__set( int defaultSize ) {
    _bufsize = defaultSize;
    _buf = new Uniqueness_rule_ptr[_bufsize];
    _count = 0;
}

Uniqueness_rule__set::~Uniqueness_rule__set() {
    Clear();

    delete[] _buf;
}

void Uniqueness_rule__set::Check( int index ) {
    Uniqueness_rule_ptr * newbuf;

    if( index >= _bufsize ) {
        _bufsize = ( index + 1 ) * 2;
        newbuf = new Uniqueness_rule_ptr[_bufsize];
        memmove( newbuf, _buf, _count * sizeof( Uniqueness_rule_ptr ) );
        delete[] _buf;
        _buf = newbuf;
    }
}

void Uniqueness_rule__set::Insert( Uniqueness_rule_ptr v, int index ) {
    Uniqueness_rule_ptr * spot;
    index = ( index < 0 ) ? _count : index;

    if( index < _count ) {
        Check( _count + 1 );
        spot = &_buf[index];
        memmove( spot + 1, spot, ( _count - index )*sizeof( Uniqueness_rule_ptr ) );

    } else {
        Check( index );
        spot = &_buf[index];
    }
    *spot = v;
    ++_count;
}

void Uniqueness_rule__set::Append( Uniqueness_rule_ptr v ) {
    int index = _count;
    Uniqueness_rule_ptr * spot;

    if( index < _count ) {
        Check( _count + 1 );
        spot = &_buf[index];
        memmove( spot + 1, spot, ( _count - index )*sizeof( Uniqueness_rule_ptr ) );

    } else {
        Check( index );
        spot = &_buf[index];
    }
    *spot = v;
    ++_count;
}

void Uniqueness_rule__set::Remove( int index ) {
    if( 0 <= index && index < _count ) {
        --_count;
        Uniqueness_rule_ptr * spot = &_buf[index];
        memmove( spot, spot + 1, ( _count - index )*sizeof( Uniqueness_rule_ptr ) );
    }
}

int Uniqueness_rule__set::Index( Uniqueness_rule_ptr v ) {
    for( int i = 0; i < _count; ++i ) {
        if( _buf[i] == v ) {
            return i;
        }
    }
    return -1;
}

Uniqueness_rule_ptr & Uniqueness_rule__set::operator[]( int index ) {
    Check( index );
    _count = ( ( _count > index + 1 ) ? _count : ( index + 1 ) );
    return _buf[index];
}

int Uniqueness_rule__set::Count() {
    return _count;
}

void Uniqueness_rule__set::Clear() {
    for( int i = 0; i < _count; i ++ ) {
        delete _buf[i];
    }
    _count = 0;
}
