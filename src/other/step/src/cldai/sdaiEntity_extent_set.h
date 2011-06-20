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

#ifndef SDAIENTITY_EXTENT_SET_h
#define SDAIENTITY_EXTENT_SET_h

/*
//#include <sdaiDefs.h>
//#include <EntityExtent.h>
*/

class SCLP23_NAME(Entity_extent__set) {
public:
#ifdef __OSTORE__
    os_Collection<SCLP23_NAME(Entity_extent_ptr)> &_rep;
    os_Cursor<SCLP23_NAME(Entity_extent_ptr)> _cursor;
    Boolean _first;
#endif

    SCLP23_NAME(Entity_extent__set)(int = 16);
    ~SCLP23_NAME(Entity_extent__set)();

    SCLP23_NAME(Entity_extent_ptr) retrieve(int index);
    int is_empty();

#ifndef __OSTORE__
    SCLP23_NAME(Entity_extent_ptr)& operator[](int index);
#endif
    void Insert(SCLP23_NAME(Entity_extent_ptr), int index);
    void Append(SCLP23_NAME(Entity_extent_ptr));
    void Remove(int index);
#ifndef __OSTORE__
    int Index(SCLP23_NAME(Entity_extent_ptr));

    void Clear();
#endif
    int Count();

#ifdef __OSTORE__
// cursor stuff
    SCLP23_NAME(Entity_extent_ptr) first();
    SCLP23_NAME(Entity_extent_ptr) next();
    SCLP23_NAME(Integer) more();
#endif

private:
    void Check(int index);
private:
    SCLP23_NAME(Entity_extent_ptr)* _buf;
    int _bufsize;
    int _count;

  public:

#ifdef __OSTORE__
    static os_typespec* get_os_typespec();
#endif
};

typedef SCLP23_NAME(Entity_extent__set)* SCLP23_NAME(Entity_extent__set_ptr);
typedef SCLP23_NAME(Entity_extent__set_ptr) SCLP23_NAME(Entity_extent__set_var);

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
