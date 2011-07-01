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
#include <memory.h>
#include <math.h>

#include <sdai.h>

/*
#include <EntityExtent.h>
#include <EntityExtentSet.h>
*/

// to help ObjectCenter
#ifndef HAVE_MEMMOVE
extern "C"
{
void * memmove(void *__s1, const void *__s2, size_t __n);
}
#endif

/*****************************************************************************/

SCLP23(Entity_extent__set)::SCLP23_NAME(Entity_extent__set) (int defaultSize)
#ifdef __OSTORE__
// : _rep(os_Collection<Entity_extent_ptr>::create(os_database::of(this)) ), 
 : _rep(os_List<SCLP23(Entity_extent_ptr)>::create(os_database::of(this)) ), 
   _cursor(_rep),
   _first(BTrue)
{
//    _rep = os_Set<Entity_extent_ptr>::create(os_database::of(this));
}
#else
{
    _bufsize = defaultSize;
    _buf = new SCLP23(Entity_extent_ptr)[_bufsize];
    _count = 0;
}
#endif

SCLP23(Entity_extent__set)::~SCLP23_NAME(Entity_extent__set) () 
{
#ifndef __OSTORE__
    delete _buf;
#endif
}

void SCLP23(Entity_extent__set)::Check (int index) {

#ifdef __OSTORE__
#else
    SCLP23(Entity_extent_ptr)* newbuf;

    if (index >= _bufsize) {
        _bufsize = (index+1) * 2;
        newbuf = new SCLP23(Entity_extent_ptr)[_bufsize];
        memmove(newbuf, _buf, _count*sizeof(SCLP23(Entity_extent_ptr)));
        delete _buf;
        _buf = newbuf;
    }
#endif
}

void 
SCLP23(Entity_extent__set)::Insert (SCLP23(Entity_extent_ptr) v, int index) {

#ifdef __OSTORE__
    _rep.insert_before( (SCLP23(Entity_extent_ptr)) v, index);
#else
    SCLP23(Entity_extent_ptr)* spot;
    index = (index < 0) ? _count : index;

    if (index < _count) {
        Check(_count+1);
        spot = &_buf[index];
        memmove(spot+1, spot, (_count - index)*sizeof(SCLP23(Entity_extent_ptr)));

    } else {
        Check(index);
        spot = &_buf[index];
    }
    *spot = v;
    ++_count;
#endif
}

void SCLP23(Entity_extent__set)::Append (SCLP23(Entity_extent_ptr) v) {

#ifdef __OSTORE__
    _rep.insert_last( (SCLP23(Entity_extent_ptr)) v);
#else
    int index = _count;
    SCLP23(Entity_extent_ptr)* spot;

    if (index < _count) {
        Check(_count+1);
        spot = &_buf[index];
        memmove(spot+1, spot, (_count - index)*sizeof(SCLP23(Entity_extent_ptr)));

    } else {
        Check(index);
        spot = &_buf[index];
    }
    *spot = v;
    ++_count;
#endif
}

void SCLP23(Entity_extent__set)::Remove (int index) {

#ifdef __OSTORE__
    _rep.remove_at( index );
#else
    if (0 <= index && index < _count) {
        --_count;
        SCLP23(Entity_extent_ptr)* spot = &_buf[index];
        memmove(spot, spot+1, (_count - index)*sizeof(SCLP23(Entity_extent_ptr)));
    }
#endif
}

#ifndef __OSTORE__
int SCLP23(Entity_extent__set)::Index (SCLP23(Entity_extent_ptr) v) {

    for (int i = 0; i < _count; ++i) {
        if (_buf[i] == v) {
            return i;
        }
    }
    return -1;
}
#endif

SCLP23(Entity_extent_ptr) 
SCLP23(Entity_extent__set)::retrieve(int index)
{
#ifdef __OSTORE__
    return _rep.retrieve(index);
#else
    return operator[](index);
#endif
}

#ifndef __OSTORE__
SCLP23(Entity_extent_ptr)& SCLP23(Entity_extent__set)::operator[] (int index) {
    Check(index);
//    _count = max(_count, index+1);
    _count = ( (_count > index+1) ? _count : (index+1) );
    return _buf[index];
}
#endif

int 
SCLP23(Entity_extent__set)::Count()
{
#ifdef __OSTORE__
    return _rep.cardinality();
#else
    return _count; 
#endif
}

int 
SCLP23(Entity_extent__set)::is_empty()
{
#ifdef __OSTORE__
    return _rep.empty();
#else
    return _count; 
#endif
}

#ifndef __OSTORE__
void 
SCLP23(Entity_extent__set)::Clear()
{
    _count = 0; 
}
#endif

#ifdef __OSTORE__

SCLP23(Entity_extent_ptr) 
SCLP23(Entity_extent__set)::first()
{
    return _cursor.first();
}

SCLP23(Entity_extent_ptr) 
SCLP23(Entity_extent__set)::next()
{
    return _cursor.next();
}

SCLP23(Integer) 
SCLP23(Entity_extent__set)::more()
{
    return _cursor.more();
}

#endif

/*****************************************************************************/


#if 0
Entity_extent__set::Entity_extent__set (int defaultSize) {
    _bufsize = defaultSize;
    _buf = new Entity_extent_ptr[_bufsize];
    _count = 0;
}

Entity_extent__set::~Entity_extent__set () { delete _buf; }

void Entity_extent__set::Check (int index) {
    Entity_extent_ptr* newbuf;

    if (index >= _bufsize) {
        _bufsize = (index+1) * 2;
        newbuf = new Entity_extent_ptr[_bufsize];
        memmove(newbuf, _buf, _count*sizeof(Entity_extent_ptr));
        delete _buf;
        _buf = newbuf;
    }
}

void Entity_extent__set::Insert (Entity_extent_ptr v, int index) {
    Entity_extent_ptr* spot;
    index = (index < 0) ? _count : index;

    if (index < _count) {
        Check(_count+1);
        spot = &_buf[index];
        memmove(spot+1, spot, (_count - index)*sizeof(Entity_extent_ptr));

    } else {
        Check(index);
        spot = &_buf[index];
    }
    *spot = v;
    ++_count;
}

void Entity_extent__set::Append (Entity_extent_ptr v) {
    int index = _count;
    Entity_extent_ptr* spot;

    if (index < _count) {
        Check(_count+1);
        spot = &_buf[index];
        memmove(spot+1, spot, (_count - index)*sizeof(Entity_extent_ptr));

    } else {
        Check(index);
        spot = &_buf[index];
    }
    *spot = v;
    ++_count;
}

void Entity_extent__set::Remove (int index) {
    if (0 <= index && index < _count) {
        --_count;
        Entity_extent_ptr* spot = &_buf[index];
        memmove(spot, spot+1, (_count - index)*sizeof(Entity_extent_ptr));
    }
}

int Entity_extent__set::Index (Entity_extent_ptr v) {
    for (int i = 0; i < _count; ++i) {
        if (_buf[i] == v) {
            return i;
        }
    }
    return -1;
}

Entity_extent_ptr& Entity_extent__set::operator[] (int index) {
    Check(index);
//    _count = max(_count, index+1);
    _count = ( (_count > index+1) ? _count : (index+1) );
    return _buf[index];
}

int 
Entity_extent__set::Count()
{
    return _count; 
}

void 
Entity_extent__set::Clear()
{
    _count = 0; 
}

#endif
