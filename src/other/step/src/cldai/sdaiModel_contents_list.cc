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
#include <sdai.h>

// to help ObjectCenter
#ifndef HAVE_MEMMOVE
extern "C"
{
void * memmove(void *__s1, const void *__s2, size_t __n);
}
#endif

/*****************************************************************************/

SCLP23(Model_contents__list)::SCLP23_NAME(Model_contents__list) (int defaultSize)
{
    _bufsize = defaultSize;
    _buf = new SCLP23(Model_contents_ptr)[_bufsize];
    _count = 0;
}

SCLP23(Model_contents__list)::~SCLP23_NAME(Model_contents__list) () 
{
    delete _buf;
}

void SCLP23(Model_contents__list)::Check (int index) {

    SCLP23(Model_contents_ptr)* newbuf;

    if (index >= _bufsize) {
        _bufsize = (index+1) * 2;
        newbuf = new SCLP23(Model_contents_ptr)[_bufsize];
        memmove(newbuf, _buf, _count*sizeof(SCLP23(Model_contents_ptr)));
        delete _buf;
        _buf = newbuf;
    }
}

void 
SCLP23(Model_contents__list)::Insert (SCLP23(Model_contents_ptr) v, int index) {

    SCLP23(Model_contents_ptr)* spot;
    index = (index < 0) ? _count : index;

    if (index < _count) {
        Check(_count+1);
        spot = &_buf[index];
        memmove(spot+1, spot, (_count - index)*sizeof(SCLP23(Model_contents_ptr)));

    } else {
        Check(index);
        spot = &_buf[index];
    }
    *spot = v;
    ++_count;
}

void SCLP23(Model_contents__list)::Append (SCLP23(Model_contents_ptr) v) {

    int index = _count;
    SCLP23(Model_contents_ptr)* spot;

    if (index < _count) {
        Check(_count+1);
        spot = &_buf[index];
        memmove(spot+1, spot, (_count - index)*sizeof(SCLP23(Model_contents_ptr)));

    } else {
        Check(index);
        spot = &_buf[index];
    }
    *spot = v;
    ++_count;
}

void SCLP23(Model_contents__list)::Remove (int index) {

    if (0 <= index && index < _count) {
        --_count;
        SCLP23(Model_contents_ptr)* spot = &_buf[index];
        memmove(spot, spot+1, (_count - index)*sizeof(SCLP23(Model_contents_ptr)));
    }
}

int SCLP23(Model_contents__list)::Index (SCLP23(Model_contents_ptr) v) {

    for (int i = 0; i < _count; ++i) {
        if (_buf[i] == v) {
            return i;
        }
    }
    return -1;
}

SCLP23(Model_contents_ptr) 
SCLP23(Model_contents__list)::retrieve(int index)
{
    return operator[](index);
}

SCLP23(Model_contents_ptr)& 
SCLP23(Model_contents__list)::operator[] (int index) 
{

    Check(index);
//    _count = max(_count, index+1);
    _count = ( (_count > index+1) ? _count : (index+1) );
    return _buf[index];
}

int 
SCLP23(Model_contents__list)::Count()
{
    return _count; 
}

int 
SCLP23(Model_contents__list)::is_empty()
{
    return _count; 
}

void 
SCLP23(Model_contents__list)::Clear()
{
    _count = 0; 
}

