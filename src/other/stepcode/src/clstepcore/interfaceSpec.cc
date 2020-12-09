#include "interfaceSpec.h"

Interface_spec__set::Interface_spec__set( int defaultSize ) {
    _bufsize = defaultSize;
    _buf = new Interface_spec_ptr[_bufsize];
    _count = 0;
}

Interface_spec__set::~Interface_spec__set() {
    delete[] _buf;
}

void Interface_spec__set::Check( int index ) {
    Interface_spec_ptr * newbuf;

    if( index >= _bufsize ) {
        _bufsize = ( index + 1 ) * 2;
        newbuf = new Interface_spec_ptr[_bufsize];
        memmove( newbuf, _buf, _count * sizeof( Interface_spec_ptr ) );
        delete[] _buf;
        _buf = newbuf;
    }
}

void Interface_spec__set::Insert( Interface_spec_ptr v, int index ) {
    Interface_spec_ptr * spot;
    index = ( index < 0 ) ? _count : index;

    if( index < _count ) {
        Check( _count + 1 );
        spot = &_buf[index];
        memmove( spot + 1, spot, ( _count - index )*sizeof( Interface_spec_ptr ) );

    } else {
        Check( index );
        spot = &_buf[index];
    }
    *spot = v;
    ++_count;
}

void Interface_spec__set::Append( Interface_spec_ptr v ) {
    int index = _count;
    Interface_spec_ptr * spot;

    if( index < _count ) {
        Check( _count + 1 );
        spot = &_buf[index];
        memmove( spot + 1, spot, ( _count - index )*sizeof( Interface_spec_ptr ) );

    } else {
        Check( index );
        spot = &_buf[index];
    }
    *spot = v;
    ++_count;
}

void Interface_spec__set::Remove( int index ) {
    if( 0 <= index && index < _count ) {
        --_count;
        Interface_spec_ptr * spot = &_buf[index];
        memmove( spot, spot + 1, ( _count - index )*sizeof( Interface_spec_ptr ) );
    }
}

int Interface_spec__set::Index( Interface_spec_ptr v ) {
    for( int i = 0; i < _count; ++i ) {
        if( _buf[i] == v ) {
            return i;
        }
    }
    return -1;
}

Interface_spec_ptr & Interface_spec__set::operator[]( int index ) {
    Check( index );
    _count = ( ( _count > index + 1 ) ? _count : ( index + 1 ) );
    return _buf[index];
}

int Interface_spec__set::Count() {
    return _count;
}

void Interface_spec__set::Clear() {
    _count = 0;
}

///////////////////////////////////////////////////////////////////////////////

Interface_spec::Interface_spec()
: _explicit_items( new Explicit_item_id__set ),
_implicit_items( 0 ), _all_objects( 0 ) {
}

/// not tested
Interface_spec::Interface_spec( Interface_spec & is ): Dictionary_instance() {
    _explicit_items = new Explicit_item_id__set;
    int count = is._explicit_items->Count();
    int i;
    for( i = 0; i < count; i++ ) {
        ( *_explicit_items )[i] =
        ( *( is._explicit_items ) )[i];
    }
    _current_schema_id = is._current_schema_id;
    _foreign_schema_id = is._foreign_schema_id;
    _all_objects = is._all_objects;
    _implicit_items = 0;
}

Interface_spec::Interface_spec( const char * cur_sch_id,
                                const char * foreign_sch_id, int all_objects )
: _current_schema_id( cur_sch_id ), _explicit_items( new Explicit_item_id__set ),
_implicit_items( 0 ), _foreign_schema_id( foreign_sch_id ),
_all_objects( all_objects ) {
}

Interface_spec::~Interface_spec() {
    delete _explicit_items;
    delete _implicit_items;
}
