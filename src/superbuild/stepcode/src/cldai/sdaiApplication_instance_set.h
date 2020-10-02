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

#include <sc_export.h>

class SDAI_Application_instance;
class SDAI_Application_instance__set;
typedef SDAI_Application_instance__set * SDAI_Application_instance__set_ptr;
typedef SDAI_Application_instance__set_ptr SDAI_Application_instance__set_var;

class SC_DAI_EXPORT SDAI_Application_instance__set {
    public:
        SDAI_Application_instance__set( int = 16 );
        ~SDAI_Application_instance__set();

        SDAI_Application_instance *& operator[]( int index );
        void Insert( SDAI_Application_instance *, int index );
        void Append( SDAI_Application_instance * );
        void Remove( int index );
        void Remove( SDAI_Application_instance * );
        int Index( SDAI_Application_instance * );

        int Count();
        void Clear();
    private:
        void Check( int index );
    private:
        SDAI_Application_instance ** _buf;
        int _bufsize;
        int _count;
};

#endif
