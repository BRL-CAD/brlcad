#include "globalRule.h"

Global_rule::Global_rule()
: _entities( 0 ), _where_rules( 0 ), _parent_schema( 0 ) {
}

Global_rule::Global_rule( const char * n, Schema_ptr parent_sch, const std::string & rt )
: _name( n ), _entities( 0 ), _where_rules( 0 ), _parent_schema( parent_sch ),
_rule_text( rt ) {
}

/// not fully implemented
Global_rule::Global_rule( Global_rule & gr ): Dictionary_instance() {
    _name = gr._name;
    _parent_schema = gr._parent_schema;
}

Global_rule::~Global_rule() {
    delete _entities;
    delete _where_rules;
}

void Global_rule::entities_( const Entity__set_var & e ) {
    if( _entities ) {
        if( _entities->EntryCount() > 0 ) {
            std::cerr << "In " << __FILE__ << ", Global_rule::entities_(): overwriting non-empty entity set!" << std::endl;
        }
        delete _entities;
    }
    _entities = e;
}

void Global_rule::where_rules_( const Where_rule__list_var & wrl ) {
    if( _where_rules ) {
        if( _where_rules->Count() > 0 ) {
            std::cerr << "In " << __FILE__ << ", Global_rule::where_rules_(): overwriting non-empty rule set!" << std::endl;
        }
        delete _where_rules;
    }
    _where_rules = wrl;
}

///////////////////////////////////////////////////////////////////////////////

Global_rule__set::Global_rule__set( int defaultSize ) {
    _bufsize = defaultSize;
    _buf = new Global_rule_ptr[_bufsize];
    _count = 0;
}

Global_rule__set::~Global_rule__set() {
    Clear();
    delete[] _buf;
}

void Global_rule__set::Check( int index ) {
    Global_rule_ptr * newbuf;

    if( index >= _bufsize ) {
        _bufsize = ( index + 1 ) * 2;
        newbuf = new Global_rule_ptr[_bufsize];
        memmove( newbuf, _buf, _count * sizeof( Global_rule_ptr ) );
        delete[] _buf;
        _buf = newbuf;
    }
}

void Global_rule__set::Insert( Global_rule_ptr v, int index ) {
    Global_rule_ptr * spot;
    index = ( index < 0 ) ? _count : index;

    if( index < _count ) {
        Check( _count + 1 );
        spot = &_buf[index];
        memmove( spot + 1, spot, ( _count - index )*sizeof( Global_rule_ptr ) );

    } else {
        Check( index );
        spot = &_buf[index];
    }
    *spot = v;
    ++_count;
}

void Global_rule__set::Append( Global_rule_ptr v ) {
    int index = _count;
    Global_rule_ptr * spot;

    if( index < _count ) {
        Check( _count + 1 );
        spot = &_buf[index];
        memmove( spot + 1, spot, ( _count - index )*sizeof( Global_rule_ptr ) );

    } else {
        Check( index );
        spot = &_buf[index];
    }
    *spot = v;
    ++_count;
}

void Global_rule__set::Remove( int index ) {
    if( 0 <= index && index < _count ) {
        --_count;
        Global_rule_ptr * spot = &_buf[index];
        memmove( spot, spot + 1, ( _count - index )*sizeof( Global_rule_ptr ) );
    }
}

int Global_rule__set::Index( Global_rule_ptr v ) {
    for( int i = 0; i < _count; ++i ) {
        if( _buf[i] == v ) {
            return i;
        }
    }
    return -1;
}

Global_rule_ptr & Global_rule__set::operator[]( int index ) {
    Check( index );
    _count = ( ( _count > index + 1 ) ? _count : ( index + 1 ) );
    return _buf[index];
}

int Global_rule__set::Count() {
    return _count;
}

void Global_rule__set::Clear() {
    for( int i = 0; i < _count; i ++ ) {
        delete _buf[i];
    }
    _count = 0;
}
