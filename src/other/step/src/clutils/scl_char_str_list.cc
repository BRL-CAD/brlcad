
///////////////////////////////////////////////////////////////////////////////
#ifdef HAVE_CONFIG_H
# include <scl_cf.h>
#endif

#include <memory.h>
#include <math.h>

#include <string.h>

// to help ObjectCenter
#ifndef HAVE_MEMMOVE
extern "C"
{
void * memmove(void *__s1, const void *__s2, size_t __n);
}
#endif

#include <scl_char_str_list.h>


scl_char_str__list::scl_char_str__list (int defaultSize) {
    _bufsize = defaultSize;
    _buf = new scl_char_str_ptr[_bufsize];
    _count = 0;
}

scl_char_str__list::~scl_char_str__list () { delete _buf; }

void scl_char_str__list::Check (int index) {
    scl_char_str_ptr* newbuf;

    if (index >= _bufsize) {
        _bufsize = (index+1) * 2;
        newbuf = new scl_char_str_ptr[_bufsize];
        memmove(newbuf, _buf, _count*sizeof(scl_char_str_ptr));
        delete [] _buf;
        _buf = newbuf;
    }
}

void scl_char_str__list::Insert (scl_char_str_ptr v, int index) {
    scl_char_str_ptr* spot;
    index = (index < 0) ? _count : index;

    if (index < _count) {
        Check(_count+1);
        spot = &_buf[index];
        memmove(spot+1, spot, (_count - index)*sizeof(scl_char_str_ptr));

    } else {
        Check(index);
        spot = &_buf[index];
    }
    *spot = v;
    ++_count;
}

void scl_char_str__list::Append (scl_char_str_ptr v) {
    int index = _count;
    scl_char_str_ptr* spot;

    if (index < _count) {
        Check(_count+1);
        spot = &_buf[index];
        memmove(spot+1, spot, (_count - index)*sizeof(scl_char_str_ptr));

    } else {
        Check(index);
        spot = &_buf[index];
    }
    *spot = v;
    ++_count;
}

void scl_char_str__list::Remove (int index) {
    if (0 <= index && index < _count) {
        --_count;
        scl_char_str_ptr* spot = &_buf[index];
        memmove(spot, spot+1, (_count - index)*sizeof(scl_char_str_ptr));
    }
}

int scl_char_str__list::Index (scl_char_str_ptr v) {
    for (int i = 0; i < _count; ++i) {
        if (_buf[i] == v) {
            return i;
        }
    }
    return -1;
}

scl_char_str_ptr& scl_char_str__list::operator[] (int index) {
    Check(index);
//    _count = max(_count, index+1);
    _count = ( (_count > index+1) ? _count : (index+1) );
    return _buf[index];
}

int 
scl_char_str__list::Count()
{
    return _count; 
}

void 
scl_char_str__list::Clear()
{
    _count = 0; 
}
