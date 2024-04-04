#ifndef STEPAGGREGATE_H
#define STEPAGGREGATE_H

/*
* NIST STEP Core Class Library
* clstepcore/STEPaggregate.h
* April 1997
* K. C. Morris
* David Sauder

* Development of this software was funded by the United States Government,
* and is not subject to copyright.
*/

class InstMgrBase;
class STEPaggregate;
class TypeDescriptor;

#include <sc_export.h>
#include <errordesc.h>
#include <SingleLinkList.h>
#include <baseType.h>
#include <sdai.h>
#include <STEPundefined.h>
#include <string>

#define     AGGR_NULL   &NilSTEPaggregate
extern STEPaggregate NilSTEPaggregate;

class SingleLinkNode;


typedef STEPaggregate * STEPaggregateH;
typedef STEPaggregate * STEPaggregate_ptr;
typedef STEPaggregate_ptr STEPaggregate_var;

class SC_CORE_EXPORT STEPaggregate :  public SingleLinkList {
    protected:
        bool _null;

    protected:

        virtual Severity ReadValue( istream & in, ErrorDescriptor * err,
                                    const TypeDescriptor * elem_type,
                                    InstMgrBase * insts, int addFileId = 0,
                                    int assignVal = 1, int ExchangeFileFormat = 1,
                                    const char * currSch = 0 );
    public:

        bool is_null() {
            return _null;
        }

        virtual Severity AggrValidLevel( const char * value, ErrorDescriptor * err,
                                         const TypeDescriptor * elem_type, InstMgrBase * insts,
                                         int optional, char * tokenList, int addFileId = 0,
                                         int clearError = 0 );

        virtual Severity AggrValidLevel( istream & in, ErrorDescriptor * err,
                                         const TypeDescriptor * elem_type, InstMgrBase * insts,
                                         int optional, char * tokenList, int addFileId = 0,
                                         int clearError = 0 );

// INPUT
        virtual Severity StrToVal( const char * s, ErrorDescriptor * err = 0,
                                   const TypeDescriptor * elem_type = 0,
                                   InstMgrBase * insts = 0, int addFileId = 0 );
        virtual Severity STEPread( istream & in, ErrorDescriptor * err,
                                   const TypeDescriptor * elem_type = 0,
                                   InstMgrBase * insts = 0, int addFileId = 0,
                                   const char * currSch = 0 );
// OUTPUT
        virtual const char * asStr( std::string & s ) const;
        virtual void STEPwrite( ostream & out = cout, const char * = 0 ) const;

        virtual SingleLinkNode * NewNode();
        void AddNode( SingleLinkNode * );
        void Empty();

        STEPaggregate();
        virtual ~STEPaggregate();

// COPY - defined in subtypes
        virtual STEPaggregate & ShallowCopy( const STEPaggregate & );
};

class SC_CORE_EXPORT STEPnode :  public SingleLinkNode  {
protected:
    int _null;

public:
    int is_null() {
        return _null;
    }
    void set_null() {
        _null = 1;
    }

    //  INPUT
    virtual Severity StrToVal( const char * s, ErrorDescriptor * err );
    virtual Severity StrToVal( istream & in, ErrorDescriptor * err );

    virtual Severity STEPread( const char * s, ErrorDescriptor * err );
    virtual Severity STEPread( istream & in, ErrorDescriptor * err );

    //  OUTPUT
    virtual const char * asStr( std::string & s );
    virtual const char * STEPwrite( std::string & s, const char * = 0 );
    virtual void STEPwrite( ostream & out = cout );
};
typedef  STEPnode  * STEPnodeH;

#include "STEPaggrGeneric.h"
#include "STEPaggrEntity.h"
#include "STEPaggrSelect.h"
#include "STEPaggrString.h"
#include "STEPaggrBinary.h"
#include "STEPaggrEnum.h"
#include "STEPaggrReal.h"
#include "STEPaggrInt.h"

/******************************************************************
 **   FIXME The following classes are currently stubs
 **
**/
/*
class Array  :  public STEPaggregate  {
  public:
    int lowerBound;
    int upperBound;
};

class Bag  :  public STEPaggregate  {
  public:
    int min_ele;
    int max_ele;
};

class List :  public STEPaggregate  {
    int min_ele;
    int max_ele;
    List *prev;
};

class Set :
public STEPaggregate  {
    int cnt;
};
*/
#endif
