#ifndef SELECTTYPEDESCRIPTOR_H
#define SELECTTYPEDESCRIPTOR_H

#include "typeDescriptor.h"

typedef SDAI_Select * ( * SelectCreator )();

class SC_CORE_EXPORT SelectTypeDescriptor  :    public TypeDescriptor  {

protected:
    TypeDescriptorList _elements;    //  of  TYPE_DESCRIPTOR
    int _unique_elements;

public:

    SelectCreator CreateNewSelect;

    void AssignSelectCreator( SelectCreator f = 0 ) {
        CreateNewSelect = f;
    }

    SDAI_Select * CreateSelect();

    SelectTypeDescriptor( int b, const char * nm, PrimitiveType ft,
                          Schema * origSchema,
                          const char * d, SelectCreator f = 0 )
    : TypeDescriptor( nm, ft, origSchema, d ),
    _unique_elements( b ), CreateNewSelect( f )
    { }
    virtual ~SelectTypeDescriptor() { }

    TypeDescriptorList & Elements() {
        return _elements;
    }
    const TypeDescriptorList & GetElements() const {
        return _elements;
    }
    int UniqueElements() const {
        return _unique_elements;
    }
    virtual const TypeDescriptor * IsA( const TypeDescriptor * ) const;
    virtual const TypeDescriptor * IsA( const char * n ) const {
        return TypeDescriptor::IsA( n );
    }
    virtual const TypeDescriptor * CanBe( const TypeDescriptor * ) const;
    virtual const TypeDescriptor * CanBe( const char * n ) const;
    virtual const TypeDescriptor * CanBeSet( const char *, const char * )
    const;
};

#endif //SELECTTYPEDESCRIPTOR_H
