#ifndef INVERSEATTRIBUTE_H
#define INVERSEATTRIBUTE_H

#include "attrDescriptor.h"

class SC_CORE_EXPORT Inverse_attribute  :    public AttrDescriptor  {

    public:
        const char * _inverted_attr_id;
        const char * _inverted_entity_id;
    protected:
        const AttrDescriptor * _inverted_attr; // not implemented (?!) (perhaps this means "not used"?)
    public:

        Inverse_attribute(
            const char * name,                // i.e. char *
            TypeDescriptor * domainType,
            Logical optional,        // i.e. F U or T*/
            Logical unique,        // i.e. F U or T
            const EntityDescriptor & owner,
            const char * inverted_attr_id = 0
        ) : AttrDescriptor( name, domainType, optional, unique,
                                AttrType_Inverse, owner ),
            _inverted_attr_id( inverted_attr_id ),
            _inverted_entity_id( 0 ), _inverted_attr( 0 )
        { }
        virtual ~Inverse_attribute() { }

        const char * AttrExprDefStr( std::string & s ) const;

        const char * inverted_attr_id_() const {
            return _inverted_attr_id;
        }

        void inverted_attr_id_( const char * iai ) {
            _inverted_attr_id = iai;
        }

        const char * inverted_entity_id_() const {
            return _inverted_entity_id;
        }

        void inverted_entity_id_( const char * iei ) {
            _inverted_entity_id = iei;
        }

        /// FIXME not implemented (?!) (perhaps this means "not set"?)
        //set _inverted_attr in an extra init step in generated code? any other way to ensure pointers are valid?
        const class AttrDescriptor * inverted_attr_() const {
                return _inverted_attr;
        }

        void inverted_attr_( const AttrDescriptor * ia ) {
            _inverted_attr = ia;
        }

        // below are obsolete (and not implemented anyway)
        //         class AttrDescriptor * InverseAttribute() {
        //                 return _inverted_attr;
        //         }
        //         void InverseOf( AttrDescriptor * invAttr ) {
        //             _inverted_attr = invAttr;
        //         }
};


#endif //INVERSEATTRIBUTE_H
