#include "implicitItemId.h"

Implicit_item_id::Implicit_item_id() {
    _local_definition = 0;
}

Implicit_item_id::Implicit_item_id( Implicit_item_id & iii )
: Interfaced_item( iii ) {
    _local_definition = iii._local_definition;
}

Implicit_item_id::~Implicit_item_id() {
    _local_definition = 0;
}

Implicit_item_id__set::Implicit_item_id__set( int defaultSize ) {
    _bufsize = defaultSize;
    _buf = new Implicit_item_id_ptr[_bufsize];
    _count = 0;
}

Implicit_item_id__set::~Implicit_item_id__set() {
    delete[] _buf;
}

void Implicit_item_id__set::Check( int index ) {
    Implicit_item_id_ptr * newbuf;

    if( index >= _bufsize ) {
        _bufsize = ( index + 1 ) * 2;
        newbuf = new Implicit_item_id_ptr[_bufsize];
        memmove( newbuf, _buf, _count * sizeof( Implicit_item_id_ptr ) );
        delete[]_buf;
        _buf = newbuf;
    }
}

void Implicit_item_id__set::Insert( Implicit_item_id_ptr v, int index ) {
    Implicit_item_id_ptr * spot;
    index = ( index < 0 ) ? _count : index;

    if( index < _count ) {
        Check( _count + 1 );
        spot = &_buf[index];
        memmove( spot + 1, spot, ( _count - index )*sizeof( Implicit_item_id_ptr ) );

    } else {
        Check( index );
        spot = &_buf[index];
    }
    *spot = v;
    ++_count;
}

void Implicit_item_id__set::Append( Implicit_item_id_ptr v ) {
    int index = _count;
    Implicit_item_id_ptr * spot;

    if( index < _count ) {
        Check( _count + 1 );
        spot = &_buf[index];
        memmove( spot + 1, spot, ( _count - index )*sizeof( Implicit_item_id_ptr ) );

    } else {
        Check( index );
        spot = &_buf[index];
    }
    *spot = v;
    ++_count;
}

void Implicit_item_id__set::Remove( int index ) {
    if( 0 <= index && index < _count ) {
        --_count;
        Implicit_item_id_ptr * spot = &_buf[index];
        memmove( spot, spot + 1, ( _count - index )*sizeof( Implicit_item_id_ptr ) );
    }
}

int Implicit_item_id__set::Index( Implicit_item_id_ptr v ) {
    for( int i = 0; i < _count; ++i ) {
        if( _buf[i] == v ) {
            return i;
        }
    }
    return -1;
}

Implicit_item_id_ptr & Implicit_item_id__set::operator[]( int index ) {
    Check( index );
    _count = ( ( _count > index + 1 ) ? _count : ( index + 1 ) );
    return _buf[index];
}

int Implicit_item_id__set::Count() {
    return _count;
}

void Implicit_item_id__set::Clear() {
    _count = 0;
}
