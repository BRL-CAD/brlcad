#ifndef IMPLICITITEMID_H
#define IMPLICITITEMID_H

#include "interfacedItem.h"

class SC_CORE_EXPORT Implicit_item_id : public Interfaced_item {
protected:
    Implicit_item_id();
    Implicit_item_id( Implicit_item_id & );
    virtual ~Implicit_item_id();
public:
    const TypeDescriptor * _local_definition;

    const TypeDescriptor * local_definition_() const {
        return _local_definition;
    }

    //  private:
    void local_definition_( const TypeDescriptor * td ) {
        _local_definition = td;
    }
};

typedef Implicit_item_id * Implicit_item_id_ptr;


class SC_CORE_EXPORT Implicit_item_id__set {
public:
    Implicit_item_id__set( int = 16 );
    ~Implicit_item_id__set();

    Implicit_item_id_ptr & operator[]( int index );
    void Insert( Implicit_item_id_ptr, int index );
    void Append( Implicit_item_id_ptr );
    void Remove( int index );
    int Index( Implicit_item_id_ptr );

    int Count();
    void Clear();
private:
    void Check( int index );
private:
    Implicit_item_id_ptr * _buf;
    int _bufsize;
    int _count;
};

typedef Implicit_item_id__set * Implicit_item_id__set_ptr;
typedef Implicit_item_id__set_ptr Implicit_item_id__set_var;

#endif //IMPLICITITEMID_H
