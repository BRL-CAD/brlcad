#ifndef AGGRTYPEDESCRIPTOR_H
#define AGGRTYPEDESCRIPTOR_H

#include "typeDescriptor.h"
#include "create_Aggr.h"

#include "assert.h"

enum AggrBoundTypeEnum {
    bound_unset = 0,
    bound_constant,
    bound_runtime,
    bound_funcall
};

/**
 * \class AggrTypeDescriptor
 * I think we decided on a simplistic representation of aggr. types for now?
 * i.e. just have one AggrTypeDesc for Array of [list of] [set of] someType
 * the inherited variable _referentType will point to the TypeDesc for someType
 * So I don't believe this class was necessary.  If we were to retain
 * info for each of the [aggr of]'s in the example above then there would be
 * one of these for each [aggr of] above and they would be strung
 * together by the _aggrDomainType variables.  If you can make this
 * work then go for it.
 */
class SC_CORE_EXPORT AggrTypeDescriptor  :    public TypeDescriptor
{

    protected:

        SDAI_Integer  _bound1, _bound2;
        SDAI_LOGICAL _uniqueElements;
        TypeDescriptor *_aggrDomainType;
        AggregateCreator CreateNewAggr;

        AggrBoundTypeEnum _bound1_type, _bound2_type;
        boundCallbackFn _bound1_callback, _bound2_callback;
#ifdef _MSC_VER
#pragma warning( push )
#pragma warning( disable: 4251 )
#endif
        std::string _bound1_str, _bound2_str;
#ifdef _MSC_VER
#pragma warning( pop )
#endif

    public:

        void AssignAggrCreator(AggregateCreator f = 0);

        STEPaggregate *CreateAggregate();

        AggrTypeDescriptor();
        AggrTypeDescriptor(SDAI_Integer b1, SDAI_Integer b2,
                           Logical uniqElem,
                           TypeDescriptor *aggrDomType);
        AggrTypeDescriptor(const char *nm, PrimitiveType ft,
                           Schema *origSchema, const char *d,
                           AggregateCreator f = 0)
            : TypeDescriptor(nm, ft, origSchema, d), _bound1(0), _bound2(0), _uniqueElements(0), _aggrDomainType(NULL), CreateNewAggr(f), _bound1_type(bound_unset), _bound2_type(bound_unset), _bound1_callback(NULL), _bound2_callback(NULL) { }
        virtual ~AggrTypeDescriptor();


        /// find bound type
        AggrBoundTypeEnum Bound1Type() const
        {
            return _bound1_type;
        }
        /// get a constant bound
        SDAI_Integer Bound1() const
        {
            assert(_bound1_type == bound_constant);
            return _bound1;
        }
        /// get a runtime bound using an object's 'this' pointer
        SDAI_Integer Bound1Runtime(SDAI_Application_instance *this_ptr) const
        {
            assert(this_ptr && (_bound1_type == bound_runtime));
            return _bound1_callback(this_ptr) ;
        }
        /// get a bound's EXPRESS function call string
        std::string Bound1Funcall() const
        {
            return _bound1_str;
        }
        /// set bound to a constant
        void SetBound1(SDAI_Integer  b1)
        {
            _bound1 = b1;
            _bound1_type = bound_constant;
        }
        ///set bound's callback fn. only for bounds dependent on an attribute
        void SetBound1FromMemberAccessor(boundCallbackFn callback)
        {
            _bound1_callback = callback;
            _bound1_type = bound_runtime;
        }
        ///set bound from express function call. currently, this only stores the function call as a string.
        void SetBound1FromExpressFuncall(std::string s)
        {
            _bound1_str = s;
            _bound1_type = bound_funcall;
        }

        /// find bound type
        AggrBoundTypeEnum Bound2Type() const
        {
            return _bound2_type;
        }
        /// get a constant bound
        SDAI_Integer Bound2() const
        {
            assert(_bound2_type == bound_constant);
            return _bound2;
        }
        /// get a runtime bound using an object's 'this' pointer
        SDAI_Integer Bound2Runtime(SDAI_Application_instance *this_ptr) const
        {
            assert(this_ptr && (_bound2_type == bound_runtime));
            return _bound2_callback(this_ptr) ;
        }
        /// get a bound's EXPRESS function call string
        std::string Bound2Funcall() const
        {
            return _bound2_str;
        }
        /// set bound to a constant
        void SetBound2(SDAI_Integer  b2)
        {
            _bound2 = b2;
            _bound2_type = bound_constant;
        }
        ///set bound's callback fn
        void SetBound2FromMemberAccessor(boundCallbackFn callback)
        {
            _bound2_callback = callback;
            _bound2_type = bound_runtime;
        }
        ///set bound from express function call. currently, this only stores the function call as a string.
        void SetBound2FromExpressFuncall(std::string s)
        {
            _bound2_str = s;
            _bound2_type = bound_funcall;
        }

        SDAI_LOGICAL &UniqueElements()
        {
            return _uniqueElements;
        }
        void UniqueElements(SDAI_LOGICAL &ue)
        {
            _uniqueElements.put(ue.asInt());
        }
        void UniqueElements(Logical ue)
        {
            _uniqueElements.put(ue);
        }
        void UniqueElements(const char *ue)
        {
            _uniqueElements.put(ue);
        }

        class TypeDescriptor *AggrDomainType()
        {
                return _aggrDomainType;
        }
        void AggrDomainType(TypeDescriptor *adt)
        {
            _aggrDomainType = adt;
        }
};

class SC_CORE_EXPORT ArrayTypeDescriptor  :    public AggrTypeDescriptor
{

    protected:
        SDAI_LOGICAL  _optionalElements;
    public:

        ArrayTypeDescriptor() : _optionalElements("UNKNOWN_TYPE") { }
        ArrayTypeDescriptor(Logical optElem) : _optionalElements(optElem)
        { }
        ArrayTypeDescriptor(const char *nm, PrimitiveType ft,
                            Schema *origSchema, const char *d,
                            AggregateCreator f = 0)
            : AggrTypeDescriptor(nm, ft, origSchema, d, f),
              _optionalElements("UNKNOWN_TYPE")
        { }

        virtual ~ArrayTypeDescriptor() {}


        SDAI_LOGICAL &OptionalElements()
        {
            return _optionalElements;
        }
        void OptionalElements(SDAI_LOGICAL &oe)
        {
            _optionalElements.put(oe.asInt());
        }
        void OptionalElements(Logical oe)
        {
            _optionalElements.put(oe);
        }
        void OptionalElements(const char *oe)
        {
            _optionalElements.put(oe);
        }
};

class SC_CORE_EXPORT ListTypeDescriptor  :    public AggrTypeDescriptor
{

    protected:
    public:
        ListTypeDescriptor() { }
        ListTypeDescriptor(const char *nm, PrimitiveType ft,
                           Schema *origSchema, const char *d,
                           AggregateCreator f = 0)
            : AggrTypeDescriptor(nm, ft, origSchema, d, f) { }
        virtual ~ListTypeDescriptor() { }

};

class SC_CORE_EXPORT SetTypeDescriptor  :    public AggrTypeDescriptor
{

    protected:
    public:

        SetTypeDescriptor() { }
        SetTypeDescriptor(const char *nm, PrimitiveType ft,
                          Schema *origSchema, const char *d,
                          AggregateCreator f = 0)
            : AggrTypeDescriptor(nm, ft, origSchema, d, f) { }
        virtual ~SetTypeDescriptor() { }

};

class SC_CORE_EXPORT BagTypeDescriptor  :    public AggrTypeDescriptor
{

    protected:
    public:

        BagTypeDescriptor() { }
        BagTypeDescriptor(const char *nm, PrimitiveType ft,
                          Schema *origSchema, const char *d,
                          AggregateCreator f = 0)
            : AggrTypeDescriptor(nm, ft, origSchema, d, f) { }
        virtual ~BagTypeDescriptor() { }

};

#endif //AGGRTYPEDESCRIPTOR_H
