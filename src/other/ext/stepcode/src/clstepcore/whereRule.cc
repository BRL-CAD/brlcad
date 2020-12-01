#include "whereRule.h"

Where_rule::Where_rule() {
    _type_or_rule = 0;
}

Where_rule::Where_rule( const Where_rule & wr ): Dictionary_instance() {
    _label = wr._label;
    _type_or_rule = wr._type_or_rule;
}

Where_rule::~Where_rule() {
}

///////////////////////////////////////////////////////////////////////////////

Where_rule__list::Where_rule__list( int defaultSize ) {
    _bufsize = defaultSize;
    _buf = new Where_rule_ptr[_bufsize];
    _count = 0;
}

Where_rule__list::~Where_rule__list() {
    Clear();

    delete[] _buf;
}

void Where_rule__list::Check( int index ) {
    Where_rule_ptr * newbuf;

    if( index >= _bufsize ) {
        _bufsize = ( index + 1 ) * 2;
        newbuf = new Where_rule_ptr[_bufsize];
        memmove( newbuf, _buf, _count * sizeof( Where_rule_ptr ) );
        delete[] _buf;
        _buf = newbuf;
    }
}

void Where_rule__list::Insert( Where_rule_ptr v, int index ) {
    Where_rule_ptr * spot;
    index = ( index < 0 ) ? _count : index;

    if( index < _count ) {
        Check( _count + 1 );
        spot = &_buf[index];
        memmove( spot + 1, spot, ( _count - index )*sizeof( Where_rule_ptr ) );

    } else {
        Check( index );
        spot = &_buf[index];
    }
    *spot = v;
    ++_count;
}

void Where_rule__list::Append( Where_rule_ptr v ) {
    int index = _count;
    Where_rule_ptr * spot;

    if( index < _count ) {
        Check( _count + 1 );
        spot = &_buf[index];
        memmove( spot + 1, spot, ( _count - index )*sizeof( Where_rule_ptr ) );

    } else {
        Check( index );
        spot = &_buf[index];
    }
    *spot = v;
    ++_count;
}

void Where_rule__list::Remove( int index ) {
    if( 0 <= index && index < _count ) {
        --_count;
        Where_rule_ptr * spot = &_buf[index];
        memmove( spot, spot + 1, ( _count - index )*sizeof( Where_rule_ptr ) );
    }
}

int Where_rule__list::Index( Where_rule_ptr v ) {
    for( int i = 0; i < _count; ++i ) {
        if( _buf[i] == v ) {
            return i;
        }
    }
    return -1;
}

Where_rule_ptr & Where_rule__list::operator[]( int index ) {
    Check( index );
    _count = ( ( _count > index + 1 ) ? _count : ( index + 1 ) );
    return _buf[index];
}

int Where_rule__list::Count() {
    return _count;
}

void Where_rule__list::Clear() {
    for( int i = 0; i < _count ; i ++ ) {
        delete _buf[i];
    }
    _count = 0;
}
