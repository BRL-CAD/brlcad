#ifndef REALTYPEDESCRIPTOR_H
#define REALTYPEDESCRIPTOR_H

#include "typeDescriptor.h"

class SC_CORE_EXPORT RealTypeDescriptor  :    public TypeDescriptor  {

protected:
    SDAI_Integer _precisionSpec;  //  OPTIONAL
public:

    RealTypeDescriptor( ) {
        _precisionSpec = 0;
    }
    virtual ~RealTypeDescriptor() { }

    SDAI_Integer PrecisionSpec() {
        return _precisionSpec;
    }
    void PrecisionSpec( SDAI_Integer ps ) {
        _precisionSpec = ps;
    }
};

#endif //REALTYPEDESCRIPTOR_H
