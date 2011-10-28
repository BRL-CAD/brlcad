
#include <memory.h>
#include <math.h>

#include <sdai.h>

// to help ObjectCenter
#ifndef HAVE_MEMMOVE
extern "C"
{
void * memmove(void *__s1, const void *__s2, size_t __n);
}
#endif


SCLP23(PID)::SCLP23_NAME(PID)()
{
}

SCLP23(PID)::~SCLP23_NAME(PID)()
{
}

SCLP23(PID_DA)::SCLP23_NAME(PID_DA)()
{
}

SCLP23(PID_DA)::~SCLP23_NAME(PID_DA)()
{
}

SCLP23(PID_SDAI)::SCLP23_NAME(PID_SDAI)()
{
}

SCLP23(PID_SDAI)::~SCLP23_NAME(PID_SDAI)()
{
}

SCLP23(DAObject)::SCLP23_NAME(DAObject)()
{
}

SCLP23(DAObject)::~SCLP23_NAME(DAObject)()
{
}


#ifdef PART26
//SCLP26(Application_instance_ptr) 
IDL_Application_instance_ptr 
SCLP23(DAObject)::create_TIE() 
{
    cout << "ERROR DAObject::create_TIE() called." << endl;
    return 0;
}
#endif

SCLP23(DAObject_SDAI)::SCLP23_NAME(DAObject_SDAI)()
{
}

/*
SCLP23(DAObject_SDAI)::SCLP23_NAME(DAObject_SDAI)(const DAObject_SDAI&)
{
}
*/

SCLP23(DAObject_SDAI)::~SCLP23_NAME(DAObject_SDAI)()
{
}


#ifdef PART26
//SCLP26(Application_instance_ptr) 
IDL_Application_instance_ptr 
SCLP23(DAObject_SDAI)::create_TIE() 
{ 
    cout << "ERROR DAObject_SDAI::create_TIE() called." << endl;  
    return 0;
}
#endif

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

/*****************************************************************************/

SCLP23(DAObject__set)::SCLP23_NAME(DAObject__set) (int defaultSize)
{
    _bufsize = defaultSize;
    _buf = new SCLP23(DAObject_ptr)[_bufsize];
    _count = 0;
}

SCLP23(DAObject__set)::~SCLP23_NAME(DAObject__set) () 
{
    delete _buf;
}

void SCLP23(DAObject__set)::Check (int index) {

    SCLP23(DAObject_ptr)* newbuf;

    if (index >= _bufsize) {
        _bufsize = (index+1) * 2;
        newbuf = new SCLP23(DAObject_ptr)[_bufsize];
        memmove(newbuf, _buf, _count*sizeof(SCLP23(DAObject_ptr)));
        delete _buf;
        _buf = newbuf;
    }
}

void 
SCLP23(DAObject__set)::Insert (SCLP23(DAObject_ptr) v, int index) {

    SCLP23(DAObject_ptr)* spot;
    index = (index < 0) ? _count : index;

    if (index < _count) {
        Check(_count+1);
        spot = &_buf[index];
        memmove(spot+1, spot, (_count - index)*sizeof(SCLP23(DAObject_ptr)));

    } else {
        Check(index);
        spot = &_buf[index];
    }
    *spot = v;
    ++_count;
}

void SCLP23(DAObject__set)::Append (SCLP23(DAObject_ptr) v) {

    int index = _count;
    SCLP23(DAObject_ptr)* spot;

    if (index < _count) {
        Check(_count+1);
        spot = &_buf[index];
        memmove(spot+1, spot, (_count - index)*sizeof(SCLP23(DAObject_ptr)));

    } else {
        Check(index);
        spot = &_buf[index];
    }
    *spot = v;
    ++_count;
}

void SCLP23(DAObject__set)::Remove (int index) {

    if (0 <= index && index < _count) {
        --_count;
        SCLP23(DAObject_ptr)* spot = &_buf[index];
        memmove(spot, spot+1, (_count - index)*sizeof(SCLP23(DAObject_ptr)));
    }
}

int SCLP23(DAObject__set)::Index (SCLP23(DAObject_ptr) v) {

    for (int i = 0; i < _count; ++i) {
        if (_buf[i] == v) {
            return i;
        }
    }
    return -1;
}

SCLP23(DAObject_ptr) 
SCLP23(DAObject__set)::retrieve(int index)
{
    return operator[](index);
}

SCLP23(DAObject_ptr)& SCLP23(DAObject__set)::operator[] (int index) {

    Check(index);
//    _count = max(_count, index+1);
    _count = ( (_count > index+1) ? _count : (index+1) );
    return _buf[index];
}

int 
SCLP23(DAObject__set)::Count()
{
    return _count; 
}

int 
SCLP23(DAObject__set)::is_empty()
{
    return _count; 
}

void 
SCLP23(DAObject__set)::Clear()
{
    _count = 0; 
}
