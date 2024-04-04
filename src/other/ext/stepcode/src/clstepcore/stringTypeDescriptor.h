#ifndef STRINGTYPEDESCRIPTOR_H
#define STRINGTYPEDESCRIPTOR_H

#include "typeDescriptor.h"

class SC_CORE_EXPORT StringTypeDescriptor  :    public TypeDescriptor  {

protected:
    SDAI_Integer   _width;  //  OPTIONAL
    SDAI_LOGICAL  _fixedSize;
public:

    StringTypeDescriptor( ) : _fixedSize( "UNKNOWN_TYPE" ) {
        _width = 0;
    }
    virtual ~StringTypeDescriptor() { }


    SDAI_Integer Width() {
        return _width;
    }
    void Width( SDAI_Integer w ) {
        _width = w;
    }

    SDAI_LOGICAL & FixedSize() {
        return _fixedSize;
    }
    void FixedSize( SDAI_LOGICAL fs ) {
        _fixedSize.put( fs.asInt() );
    }
    void FixedSize( Logical fs ) {
        _fixedSize.put( fs );
    }
};

#endif //STRINGTYPEDESCRIPTOR_H
