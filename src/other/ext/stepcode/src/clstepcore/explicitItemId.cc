#include "explicitItemId.h"

Explicit_item_id::Explicit_item_id() {
    _local_definition = 0;
}

Explicit_item_id::Explicit_item_id( const Explicit_item_id & eii )
: Interfaced_item( eii ) {
    _local_definition = eii._local_definition;
    _original_id = eii._original_id;
    _new_id = eii._new_id;
}

Explicit_item_id::~Explicit_item_id() {
    _local_definition = 0;
}

Explicit_item_id__set::Explicit_item_id__set( int defaultSize ) {
    _bufsize = defaultSize;
    _buf = new Explicit_item_id_ptr[_bufsize];
    _count = 0;
}

Explicit_item_id__set::~Explicit_item_id__set() {
    delete[] _buf;
}

void Explicit_item_id__set::Check( int index ) {
    Explicit_item_id_ptr * newbuf;

    if( index >= _bufsize ) {
        _bufsize = ( index + 1 ) * 2;
        newbuf = new Explicit_item_id_ptr[_bufsize];
        memmove( newbuf, _buf, _count * sizeof( Explicit_item_id_ptr ) );
        delete[] _buf;
        _buf = newbuf;
    }
}

void Explicit_item_id__set::Insert( Explicit_item_id_ptr v, int index ) {
    Explicit_item_id_ptr * spot;
    index = ( index < 0 ) ? _count : index;

    if( index < _count ) {
        Check( _count + 1 );
        spot = &_buf[index];
        memmove( spot + 1, spot, ( _count - index )*sizeof( Explicit_item_id_ptr ) );

    } else {
        Check( index );
        spot = &_buf[index];
    }
    *spot = v;
    ++_count;
}

void Explicit_item_id__set::Append( Explicit_item_id_ptr v ) {
    int index = _count;
    Explicit_item_id_ptr * spot;

    if( index < _count ) {
        Check( _count + 1 );
        spot = &_buf[index];
        memmove( spot + 1, spot, ( _count - index )*sizeof( Explicit_item_id_ptr ) );

    } else {
        Check( index );
        spot = &_buf[index];
    }
    *spot = v;
    ++_count;
}

void Explicit_item_id__set::Remove( int index ) {
    if( 0 <= index && index < _count ) {
        --_count;
        Explicit_item_id_ptr * spot = &_buf[index];
        memmove( spot, spot + 1, ( _count - index )*sizeof( Explicit_item_id_ptr ) );
    }
}

int Explicit_item_id__set::Index( Explicit_item_id_ptr v ) {
    for( int i = 0; i < _count; ++i ) {
        if( _buf[i] == v ) {
            return i;
        }
    }
    return -1;
}

Explicit_item_id_ptr & Explicit_item_id__set::operator[]( int index ) {
    Check( index );
    _count = ( ( _count > index + 1 ) ? _count : ( index + 1 ) );
    return _buf[index];
}

int Explicit_item_id__set::Count() {
    return _count;
}

void Explicit_item_id__set::Clear() {
    _count = 0;
}
