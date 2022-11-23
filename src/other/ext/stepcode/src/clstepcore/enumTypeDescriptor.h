#ifndef ENUMTYPEDESCRIPTOR_H
#define ENUMTYPEDESCRIPTOR_H

#include "typeDescriptor.h"

typedef SDAI_Enum * ( * EnumCreator )();

class SC_CORE_EXPORT EnumTypeDescriptor  :    public TypeDescriptor  {
    public:
        EnumCreator CreateNewEnum;

        const char * GenerateExpress( std::string & buf ) const;

        void AssignEnumCreator( EnumCreator f = 0 ) {
            CreateNewEnum = f;
        }

        SDAI_Enum * CreateEnum();

        EnumTypeDescriptor( ) { }
        EnumTypeDescriptor( const char * nm, PrimitiveType ft,
                            Schema * origSchema, const char * d,
                            EnumCreator f = 0 );

        virtual ~EnumTypeDescriptor() { }
};

/** \class EnumerationTypeDescriptor
 * FIXME not implemented
 *
 * this was in ExpDict.h before splitting it up
 * why have EnumTypeDescriptor + EnumerationTypeDescriptor ???
 */
#ifdef NOT_YET
class SC_CORE_EXPORT EnumerationTypeDescriptor  :    public TypeDescriptor  {

    protected:
        StringAggregate * _elements;     //  of  (null)

    public:
        EnumerationTypeDescriptor( );
        virtual ~EnumerationTypeDescriptor() { }


        StringAggregate & Elements() {
            return *_elements;
        }
        //        void Elements (StringAggregate  e);
};
#endif


#endif //ENUMTYPEDESCRIPTOR_H
