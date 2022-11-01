#ifndef DERIVEDATTRIBUTE_H
#define DERIVEDATTRIBUTE_H

#include "attrDescriptor.h"

#include "sc_export.h"

class SC_CORE_EXPORT Derived_attribute  :    public AttrDescriptor  {
    public:
        const char * _initializer;

        Derived_attribute(
            const char * name,                // i.e. char *
            const TypeDescriptor * domainType,
            Logical optional,        // i.e. F U or T
            Logical unique,        // i.e. F U or T
            AttrType_Enum at,// AttrType_Explicit, AttrType_Inverse,
            // AttrType_Deriving,AttrType_Redefining
            const EntityDescriptor & owner
        );
        virtual ~Derived_attribute();
        const char * AttrExprDefStr( std::string & s ) const;

        const char * initializer_() {
            return _initializer;
        }
        void initializer_( const char * i ) {
            _initializer = i;
        }
};

#endif //DERIVEDATTRIBUTE_H
