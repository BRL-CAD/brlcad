#ifndef STEPAGGRREAL_H
#define STEPAGGRREAL_H

#include "STEPaggregate.h"
#include <sc_export.h>

class SC_CORE_EXPORT RealAggregate  : public STEPaggregate  {

public:
    virtual SingleLinkNode * NewNode();
    virtual STEPaggregate & ShallowCopy( const STEPaggregate & );

    RealAggregate();
    virtual ~RealAggregate();
};
typedef        RealAggregate  *   RealAggregateH;
typedef        RealAggregate  *   RealAggregate_ptr;
typedef  const RealAggregate  *   RealAggregate_ptr_c;
typedef        RealAggregate_ptr  RealAggregate_var;

class SC_CORE_EXPORT RealNode  : public STEPnode {
public:
    SDAI_Real  value; // double
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
    RealNode( SDAI_Real v );
    RealNode();
    ~RealNode();

    virtual SingleLinkNode   *  NewNode();
};



#endif //STEPAGGRREAL_H
