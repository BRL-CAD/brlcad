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

#include <sc_export.h>

#ifndef SDAIENTITY_EXTENT_SET_h
#define SDAIENTITY_EXTENT_SET_h

/*
//#include <sdaiDefs.h>
//#include <EntityExtent.h>
*/

class SC_DAI_EXPORT SDAI_Entity_extent__set {
    public:

        SDAI_Entity_extent__set( int = 16 );
        ~SDAI_Entity_extent__set();

        SDAI_Entity_extent_ptr retrieve( int index );
        int is_empty();

        SDAI_Entity_extent_ptr & operator[]( int index );

        void Insert( SDAI_Entity_extent_ptr, int index );
        void Append( SDAI_Entity_extent_ptr );
        void Remove( int index );
        int Index( SDAI_Entity_extent_ptr );

        void Clear();
        int Count();

    private:
        void Check( int index );
    private:
        SDAI_Entity_extent_ptr * _buf;
        int _bufsize;
        int _count;

};

typedef SDAI_Entity_extent__set * SDAI_Entity_extent__set_ptr;
typedef SDAI_Entity_extent__set_ptr SDAI_Entity_extent__set_var;

/*
class Entity_extent__set {
public:
    Entity_extent__set(int = 16);
    ~Entity_extent__set();

    Entity_extent_ptr& operator[](int index);
    void Insert(Entity_extent_ptr, int index);
    void Append(Entity_extent_ptr);
    void Remove(int index);
    int Index(Entity_extent_ptr);

    int Count();
    void Clear();
private:
    void Check(int index);
private:
    Entity_extent_ptr* _buf;
    int _bufsize;
    int _count;
};
*/
#endif
