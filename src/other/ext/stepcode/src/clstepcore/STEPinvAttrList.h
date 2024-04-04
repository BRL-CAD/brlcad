
#ifndef _STEPinvAttrList_h
#define _STEPinvAttrList_h 1

/// FIXME remove these classes? not sure there's a use for them now...

/** \file STEPinvAttrList.h
 * derived from STEPattributeList.h
 *
 * Similar to Inverse_attributeList, but this list also contains pointers to the setter and getter.
 *
 * \sa Inverse_attributeList
 */

class Inverse_attribute;

#include <sc_export.h>
#include <SingleLinkList.h>

class STEPinvAttrList;
class EntityAggregate;
class SDAI_Application_instance;

/**  setterI_t, getterI_t: for inverse attrs that only allow a single (Instance) reference
 * setterA_t, getterA_t: for inverse attrs that allow multiple (Aggregate) refs
 * @{
 */
typedef void ( *setterI_t )( SDAI_Application_instance *, const SDAI_Application_instance * );
typedef SDAI_Application_instance * ( *getterI_t )( const SDAI_Application_instance * );
typedef void ( *setterA_t )( SDAI_Application_instance *, const EntityAggregate * );
typedef EntityAggregate * ( *getterA_t )( const SDAI_Application_instance * );
/** @} */

/** invAttrListNode: base class + 2 derived classes, one for single instances and one for aggregates
 * @{
 */
class SC_CORE_EXPORT invAttrListNode :  public SingleLinkNode {
    friend class STEPinvAttrList;
    protected:
        invAttrListNode( Inverse_attribute * a ) : attr( a ) {};
        Inverse_attribute * attr;
    public:
        Inverse_attribute * inverseADesc() {
            return attr;
        }
        virtual bool isAggregate() = 0;
};

class SC_CORE_EXPORT invAttrListNodeI : public invAttrListNode {
    friend class STEPinvAttrList;
    protected:
        setterI_t set;
        getterI_t get;
    public:
        invAttrListNodeI( Inverse_attribute * a, setterI_t s, getterI_t g );
        virtual ~invAttrListNodeI();
        setterI_t setter() {
            return set;
        }
        getterI_t getter() {
            return get;
        }
        virtual bool isAggregate() {
            return false;
        }
};

class SC_CORE_EXPORT invAttrListNodeA : public invAttrListNode {
    friend class STEPinvAttrList;
    protected:
        setterA_t set;
        getterA_t get;
    public:
        invAttrListNodeA( Inverse_attribute * a, setterA_t s, getterA_t g );
        virtual ~invAttrListNodeA();
        setterA_t setter() {
            return set;
        }
        getterA_t getter() {
            return get;
        }
        virtual bool isAggregate() {
            return true;
        }
};
/** @} */

/// Similar to Inverse_attributeList, but this list also contains pointers to the setter and getter.
class SC_CORE_EXPORT STEPinvAttrList : public SingleLinkList {
    public:
        STEPinvAttrList();
        virtual ~STEPinvAttrList();
        invAttrListNode * operator []( int n );
        int list_length();
        void push( Inverse_attribute * a, setterA_t s, getterA_t g );
        void push( Inverse_attribute * a, setterI_t s, getterI_t g );
};


#endif
