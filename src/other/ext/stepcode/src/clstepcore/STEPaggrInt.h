#ifndef STEPAGGRINT_H
#define STEPAGGRINT_H

#include "STEPaggregate.h"
#include <sc_export.h>

class SC_CORE_EXPORT IntAggregate  : public STEPaggregate  {

public:
    virtual SingleLinkNode * NewNode();
    virtual STEPaggregate & ShallowCopy( const STEPaggregate & );

    IntAggregate();
    virtual ~IntAggregate();
};
typedef        IntAggregate  *   IntAggregateH;
typedef        IntAggregate  *   IntAggregate_ptr;
typedef  const IntAggregate  *   IntAggregate_ptr_c;
typedef        IntAggregate_ptr  IntAggregate_var;


class SC_CORE_EXPORT IntNode  : public STEPnode {
public:
    SDAI_Integer  value; // long int
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
    IntNode( SDAI_Integer v );
    IntNode();
    ~IntNode();

    virtual SingleLinkNode   *  NewNode();
};


#endif //STEPAGGRINT_H
