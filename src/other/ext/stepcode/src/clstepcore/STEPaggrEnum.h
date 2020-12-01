#ifndef STEPAGGRENUM_H
#define STEPAGGRENUM_H

#include "STEPaggregate.h"
#include "sc_export.h"
/** \file StepaggrEnum.h
 * classes EnumAggregate, LOGICALS, BOOLEANS, EnumNode
 */

/**
 * \class EnumAggregate
 * This is a minimal representions for a collection of SDAI_Enum
 */
class SC_CORE_EXPORT EnumAggregate  :  public STEPaggregate {
public:
    virtual SingleLinkNode * NewNode();
    virtual STEPaggregate & ShallowCopy( const STEPaggregate & );

    EnumAggregate();
    virtual ~EnumAggregate();
};
typedef        EnumAggregate  *   EnumAggregateH;
typedef        EnumAggregate  *   EnumAggregate_ptr;
typedef  const EnumAggregate  *   EnumAggregate_ptr_c;
typedef        EnumAggregate_ptr  EnumAggregate_var;

class SC_CORE_EXPORT LOGICALS  : public EnumAggregate {
public:
    virtual SingleLinkNode * NewNode();

    LOGICALS();
    virtual ~LOGICALS();
};
typedef        LOGICALS  *   LogicalsH;
typedef        LOGICALS  *   LOGICALS_ptr;
typedef  const LOGICALS  *   LOGICALS_ptr_c;
typedef        LOGICALS_ptr  LOGICALS_var;
SC_CORE_EXPORT LOGICALS * create_LOGICALS();


class SC_CORE_EXPORT BOOLEANS  : public EnumAggregate {
public:
    virtual SingleLinkNode * NewNode();

    BOOLEANS();
    virtual ~BOOLEANS();
};

typedef        BOOLEANS  *   BOOLEANS_ptr;
typedef  const BOOLEANS  *   BOOLEANS_ptr_c;
typedef        BOOLEANS_ptr  BOOLEANS_var;

SC_CORE_EXPORT BOOLEANS * create_BOOLEANS();


/**
 * * \class EnumNode
 ** This is a minimal representions for node in lists of SDAI_Enum
 */
class SC_CORE_EXPORT EnumNode  : public STEPnode {
public:
    SDAI_Enum  * node;
    //  INPUT
    virtual Severity StrToVal( const char * s, ErrorDescriptor * err );
    virtual Severity StrToVal( istream & in, ErrorDescriptor * err );

    virtual Severity STEPread( const char * s, ErrorDescriptor * err );
    virtual Severity STEPread( istream & in, ErrorDescriptor * err );

    //  OUTPUT
    virtual const char * asStr( std::string & s );
    virtual const char * STEPwrite( std::string & s, const char * = 0 );
    virtual void    STEPwrite( ostream & out = cout );

    //  CONSTRUCTORS
    EnumNode( SDAI_Enum  * e );
    EnumNode();
    ~EnumNode();

    virtual SingleLinkNode   *  NewNode();
};


#endif //STEPAGGRENUM_H
