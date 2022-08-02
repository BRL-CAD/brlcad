#ifndef STEPAGGRENTITY_H
#define STEPAGGRENTITY_H

/** \file STEPaggrEntity.h
 * classes EntityAggregate, EntityNode
 */

#include "STEPaggregate.h"
#include <sc_export.h>

class SC_CORE_EXPORT EntityAggregate  :  public  STEPaggregate
{
    public:
        virtual Severity ReadValue(istream &in, ErrorDescriptor *err,
                                   const TypeDescriptor *elem_type,
                                   InstMgrBase *insts, int addFileId = 0,
                                   int assignVal = 1, int ExchangeFileFormat = 1,
                                   const char *currSch = 0);

        virtual  SingleLinkNode *NewNode();
        virtual  STEPaggregate &ShallowCopy(const STEPaggregate &);

        EntityAggregate();
        virtual ~EntityAggregate();
};
typedef         EntityAggregate     *EntityAggregateH;
typedef         EntityAggregate     *EntityAggregate_ptr;
typedef   const EntityAggregate     *EntityAggregate_ptr_c;
typedef         EntityAggregate_ptr  EntityAggregate_var;

class SC_CORE_EXPORT EntityNode  : public STEPnode
{
    public:
        SDAI_Application_instance   *node;

        // INPUT
        virtual Severity StrToVal(const char *s, ErrorDescriptor *err,
                                  const TypeDescriptor *elem_type,
                                  InstMgrBase *insts, int addFileId = 0);
        virtual Severity StrToVal(istream &in, ErrorDescriptor *err,
                                  const TypeDescriptor *elem_type,
                                  InstMgrBase *insts, int addFileId = 0);

        virtual Severity STEPread(const char *s, ErrorDescriptor *err,
                                  const TypeDescriptor *elem_type,
                                  InstMgrBase *insts, int addFileId = 0);
        virtual Severity STEPread(istream &in, ErrorDescriptor *err,
                                  const TypeDescriptor *elem_type,
                                  InstMgrBase *insts, int addFileId = 0);
        //  OUTPUT
        virtual const char *asStr(std::string &s);
        virtual const char *STEPwrite(std::string &s, const char * = 0);
        virtual void    STEPwrite(ostream &out = cout);

        //  CONSTRUCTORS
        EntityNode(SDAI_Application_instance   *e);
        EntityNode();
        ~EntityNode();

        virtual SingleLinkNode     *NewNode();

        // Calling these functions is an error.
        Severity StrToVal(const char *s, ErrorDescriptor *err)
        {
            cerr << "Internal error:  " << __FILE__ <<  __LINE__
                 << "\n" << _POC_ "\n";
            return StrToVal(s, err, 0, 0, 0);
        }
        Severity StrToVal(istream &in, ErrorDescriptor *err)
        {
            cerr << "Internal error:  " << __FILE__ <<  __LINE__
                 << "\n" << _POC_ "\n";
            return StrToVal(in, err, 0, 0, 0);
        }

        Severity STEPread(const char *s, ErrorDescriptor *err)
        {
            cerr << "Internal error:  " << __FILE__ <<  __LINE__
                 << "\n" << _POC_ "\n";
            return STEPread(s, err, 0, 0, 0);
        }
        Severity STEPread(istream &in, ErrorDescriptor *err)
        {
            cerr << "Internal error:  " << __FILE__ <<  __LINE__
                 << "\n" << _POC_ "\n";
            return STEPread(in, err, 0, 0, 0);
        }
};

#endif //STEPAGGRENTITY_H
