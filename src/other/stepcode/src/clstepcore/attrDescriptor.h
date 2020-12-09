#ifndef ATTRDESCRIPTOR_H
#define ATTRDESCRIPTOR_H

#include "typeDescriptor.h"

#include "sdaiEnum.h"

#include "sc_export.h"

enum AttrType_Enum {
    AttrType_Explicit = 0,
    AttrType_Inverse,
    AttrType_Deriving,
    AttrType_Redefining
};

class EntityDescriptor;

/** AttrDescriptor
 * An instance of this class will be generated for each attribute for
 * an Entity.  They will be pointed to by the EntityTypeDescriptors.
 */
class SC_CORE_EXPORT AttrDescriptor {

    protected:
        const char  * _name;   // the attributes name
        // this defines the domain of the attribute
        const TypeDescriptor * _domainType;
        SDAI_LOGICAL _optional;
        SDAI_LOGICAL _unique;
        AttrType_Enum _attrType; // former attribute _derived
        const EntityDescriptor & _owner;  // the owning entityDescriptor
    public:

        AttrDescriptor(
            const char * name,       // i.e. char *
            const TypeDescriptor * domainType,
            Logical optional,    // i.e. F U or T
            Logical unique,  // i.e. F U or T
            AttrType_Enum at,// AttrType_Explicit, AttrType_Inverse,
            // AttrType_Deriving,AttrType_Redefining
            const EntityDescriptor & owner
        );
        virtual ~AttrDescriptor();

        const char * GenerateExpress( std::string & buf ) const;

        // the attribute Express def
        virtual const char * AttrExprDefStr( std::string & s ) const;

        // left side of attr def
        const char * Name() const      {
            return _name;
        }
        void     Name( const char  * n ) {
            _name = n;
        }

        /** BaseType() is the underlying type of this attribute.
         * NonRefType() is the first non REFERENCE_TYPE type
         * e.g. Given attributes of each of the following types
         * TYPE count = INTEGER;
         * TYPE ref_count = count;
         * TYPE count_set = SET OF ref_count;
         *  BaseType() will return INTEGER_TYPE for an attr of each type.
         *  BaseTypeDescriptor() returns the TypeDescriptor for Integer
         *  NonRefType() will return INTEGER_TYPE for the first two. For an
         *    attribute of type count_set NonRefType() would return
         *    AGGREGATE_TYPE
         *  NonRefTypeDescriptor() returns the TypeDescriptor for Integer
         *     for the first two and a TypeDescriptor for an
         *     aggregate for the last.
         *
         * \sa NonRefType()
         * \sa NonRefTypeDescriptor()
         */
        ///@{
        PrimitiveType BaseType() const;
        const TypeDescriptor * BaseTypeDescriptor() const;
        ///@}

        /**
         * the first PrimitiveType that is not REFERENCE_TYPE (the first
         * TypeDescriptor *_referentType that does not have REFERENCE_TYPE
         * for it's fundamentalType variable).  This would return the same
         * as BaseType() for fundamental types.  An aggregate type
         * would return AGGREGATE_TYPE then you could find out the type of
         * an element by calling AggrElemType().  Select types
         * would work the same?
         *
         * \sa BaseType()
         */
        ///@{
        PrimitiveType NonRefType() const;
        const TypeDescriptor * NonRefTypeDescriptor() const;
        ///@}

        int   IsAggrType() const;
        PrimitiveType   AggrElemType() const;
        const TypeDescriptor * AggrElemTypeDescriptor() const;

        /// The type of the attributes TypeDescriptor
        PrimitiveType Type() const;

        /// right side of attr def
        const std::string TypeName() const;

        /// an expanded right side of attr def
        const char * ExpandedTypeName( std::string & s ) const;

        int RefersToType() const    {
            return !( _domainType == 0 );
        }

        const TypeDescriptor * ReferentType() const {
            return _domainType;
        }
        const TypeDescriptor * DomainType() const {
            return _domainType;
        }
        void DomainType( const TypeDescriptor * td )  {
            _domainType = td;
        }
        void ReferentType( const TypeDescriptor * td ) {
            _domainType = td;
        }

        const SDAI_LOGICAL & Optional() const {
            return _optional;
        }
        void Optional( SDAI_LOGICAL & opt ) {
            _optional.put( opt.asInt() );
        }

        void Optional( Logical opt ) {
            _optional.put( opt );
        }
        void Optional( const char * opt ) {
            _optional.put( opt );
        }

        const SDAI_LOGICAL & Unique() const {
            return _unique;
        }
        void Unique( SDAI_LOGICAL uniq ) {
            _unique.put( uniq.asInt() );
        }
        void Unique( Logical uniq ) {
            _unique.put( uniq );
        }
        void Unique( const char * uniq )  {
            _unique.put( uniq );
        }

        void AttrType( enum AttrType_Enum ate ) {
            _attrType = ate;
        }
        enum AttrType_Enum AttrType() const {
            return _attrType;
        }

        Logical Explicit() const;
        Logical Inverse() const;
        Logical Redefining() const;
        Logical Deriving() const;

        //outdated functions, use AttrType func above, new support of redefined
        Logical Derived() const {
            return Deriving();
        }
        void Derived( Logical x );    // outdated DAS
        void Derived( SDAI_LOGICAL x ); // outdated DAS
        void Derived( const char * x ); // outdated DAS

        const SDAI_LOGICAL & Optionality() const {
            return _optional;
        }
        void Optionality( SDAI_LOGICAL & opt ) {
            _optional.put( opt.asInt() );
        }
        void Optionality( Logical opt ) {
            _optional.put( opt );
        }
        void Optionality( const char * opt ) {
            _optional.put( opt );
        }

        const SDAI_LOGICAL & Uniqueness() const {
            return _unique;
        }
        void Uniqueness( SDAI_LOGICAL uniq ) {
            _unique.put( uniq.asInt() );
        }
        void Uniqueness( Logical uniq ) {
            _unique.put( uniq );
        }
        void Uniqueness( const char * uniq ) {
            _unique.put( uniq );
        }

        const EntityDescriptor & Owner() const {
            return _owner;
        }
};

#endif //ATTRDESCRIPTOR_H
