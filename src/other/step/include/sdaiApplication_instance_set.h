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
 * UArray - dynamic array object.
 */
/*
 * UArray - dynamic array object.
 */

#ifndef SDAI_APPLICATION_INSTANCE_SET_h
#define SDAI_APPLICATION_INSTANCE_SET_h

//#include <STEPentity.h>

class SCLP23_NAME(Application_instance__set);
typedef SCLP23_NAME(Application_instance__set) *
				SCLP23_NAME(Application_instance__set_ptr);
typedef SCLP23_NAME(Application_instance__set_ptr) 
				SCLP23_NAME(Application_instance__set_var);

class SCLP23_NAME(Application_instance__set) 
{
public:
    SCLP23_NAME(Application_instance__set)(int = 16);
    ~SCLP23_NAME(Application_instance__set)();

    SCLP23_NAME(Application_instance_ptr)& operator[](int index);
    void Insert(SCLP23_NAME(Application_instance_ptr), int index);
    void Append(SCLP23_NAME(Application_instance_ptr));
    void Remove(int index);
    void Remove(SCLP23_NAME(Application_instance_ptr));
    int Index(SCLP23_NAME(Application_instance_ptr));

    int Count();
    void Clear();
private:
    void Check(int index);
private:
    SCLP23_NAME(Application_instance_ptr)* _buf;
    int _bufsize;
    int _count;
};

#endif
