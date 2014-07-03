/*
 * Copyright (c) 1990, 1991 Stanford University
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Stanford not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  Stanford makes no representations about
 * the suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * STANFORD DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS.
 * IN NO EVENT SHALL STANFORD BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * UArray implementation.
 */

//#include <sdaiApplication_instance_set.h>
#include <sdai.h>

#include <memory.h>
#include <math.h>
#include "sc_memmgr.h"

// to help ObjectCenter
#ifndef HAVE_MEMMOVE
extern "C"
{
    void * memmove( void * __s1, const void * __s2, size_t __n );
}
#endif

/*****************************************************************************/

SDAI_Application_instance__set::SDAI_Application_instance__set( int defaultSize ) {
    _bufsize = defaultSize;
    _buf = new SDAI_Application_instance_ptr[_bufsize];
    _count = 0;
}

SDAI_Application_instance__set::~SDAI_Application_instance__set() {
    delete _buf;
}

void SDAI_Application_instance__set::Check( int index ) {
    SDAI_Application_instance_ptr * newbuf;

    if( index >= _bufsize ) {
        _bufsize = ( index + 1 ) * 2;
        newbuf = new SDAI_Application_instance_ptr[_bufsize];
        memmove( newbuf, _buf, _count * sizeof( SDAI_Application_instance_ptr ) );
        delete _buf;
        _buf = newbuf;
    }
}

void SDAI_Application_instance__set::Insert( SDAI_Application_instance_ptr v, int index ) {
    SDAI_Application_instance_ptr * spot;
    index = ( index < 0 ) ? _count : index;

    if( index < _count ) {
        Check( _count + 1 );
        spot = &_buf[index];
        memmove( spot + 1, spot, ( _count - index )*sizeof( SDAI_Application_instance_ptr ) );

    } else {
        Check( index );
        spot = &_buf[index];
    }
    *spot = v;
    ++_count;
}

void SDAI_Application_instance__set::Append( SDAI_Application_instance_ptr v ) {
    int index = _count;
    SDAI_Application_instance_ptr * spot;

    if( index < _count ) {
        Check( _count + 1 );
        spot = &_buf[index];
        memmove( spot + 1, spot, ( _count - index )*sizeof( SDAI_Application_instance_ptr ) );

    } else {
        Check( index );
        spot = &_buf[index];
    }
    *spot = v;
    ++_count;
}

void SDAI_Application_instance__set::Remove( int index ) {
    if( 0 <= index && index < _count ) {
        --_count;
        SDAI_Application_instance_ptr * spot = &_buf[index];
        memmove( spot, spot + 1, ( _count - index )*sizeof( SDAI_Application_instance_ptr ) );
    }
}

void SDAI_Application_instance__set::Remove( SDAI_Application_instance_ptr a ) {
    int index = Index( a );
    if( !( index < 0 ) ) {
        Remove( index );
    }
}

int SDAI_Application_instance__set::Index( SDAI_Application_instance_ptr v ) {
    for( int i = 0; i < _count; ++i ) {
        if( _buf[i] == v ) {
            return i;
        }
    }
    return -1;
}

SDAI_Application_instance_ptr & SDAI_Application_instance__set::operator[]( int index ) {
    Check( index );
//    _count = max(_count, index+1);
    _count = ( ( _count > index + 1 ) ? _count : ( index + 1 ) );
    return _buf[index];
}

int
SDAI_Application_instance__set::Count() {
    return _count;
}

void
SDAI_Application_instance__set::Clear() {
    _count = 0;
}
