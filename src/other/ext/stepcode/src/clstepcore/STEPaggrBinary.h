#ifndef STEPAGGRBINARY_H
#define STEPAGGRBINARY_H

#include "STEPaggregate.h"
#include <sc_export.h>

/** \file STEPaggrBinary.h
 * classes BinaryAggregate, BinaryNode
 */

/**
 * * \class BinaryAggregate
 ** This class supports LIST OF BINARY type
 */
class SC_CORE_EXPORT BinaryAggregate  :  public STEPaggregate {
public:
    virtual SingleLinkNode * NewNode();
    virtual STEPaggregate & ShallowCopy( const STEPaggregate & );

    BinaryAggregate();
    virtual ~BinaryAggregate();
};
typedef        BinaryAggregate *   BinaryAggregateH;
typedef        BinaryAggregate *   BinaryAggregate_ptr;
typedef  const BinaryAggregate *   BinaryAggregate_ptr_c;
typedef        BinaryAggregate_ptr BinaryAggregate_var;

/**
 * * \class BinaryNode
 ** This class is for the Nodes of BinaryAggregates
 */
class SC_CORE_EXPORT BinaryNode  : public STEPnode {
public:
    SDAI_Binary  value;
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
    BinaryNode( BinaryNode & bn );
    BinaryNode( const char * sStr );
    BinaryNode();
    ~BinaryNode();

    virtual SingleLinkNode   *  NewNode();
};


#endif //STEPAGGRBINARY_H
