#ifndef STEPAGGRSTRING_H
#define STEPAGGRSTRING_H

#include "STEPaggregate.h"
#include <sc_export.h>

/** \file STEPaggrString.h
 * classes StringAggregate, StringNode
 */

/**
 * * \class StringAggregate
 ** This class supports LIST OF STRING type
 */
class SC_CORE_EXPORT StringAggregate  :  public STEPaggregate {
public:
    virtual SingleLinkNode * NewNode();
    virtual STEPaggregate & ShallowCopy( const STEPaggregate & );

    StringAggregate();
    virtual ~StringAggregate();
};
typedef        StringAggregate *   StringAggregateH;
typedef        StringAggregate *   StringAggregate_ptr;
typedef  const StringAggregate *   StringAggregate_ptr_c;
typedef        StringAggregate_ptr StringAggregate_var;

/**
 * * \class StringNode
 ** This class is for the Nodes of StringAggregates
 */
class SC_CORE_EXPORT StringNode  : public STEPnode {
public:
    SDAI_String  value;
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
    StringNode( StringNode & sn );
    StringNode( const char * sStr );
    StringNode();
    ~StringNode();

    virtual SingleLinkNode   *  NewNode();
};

#endif //STEPAGGRSTRING_H
