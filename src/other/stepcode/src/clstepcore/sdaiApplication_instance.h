#ifndef STEPENTITY_H
#define STEPENTITY_H

/*
* NIST STEP Core Class Library
* clstepcore/sdaiApplication_instance.h
* April 1997
* K. C. Morris
* David Sauder

* Development of this software was funded by the United States Government,
* and is not subject to copyright.
*/

#include <map>
#include <iostream>

#include <sc_export.h>
#include <sdaiDaObject.h>

class EntityAggregate;
class Inverse_attribute;
typedef struct {
    union {
        EntityAggregate *a;
        SDAI_Application_instance *i;
    };
} iAstruct;

/** @class
 * this used to be STEPentity
 */
class SC_CORE_EXPORT SDAI_Application_instance  : public SDAI_DAObject_SDAI
{
    private:
        int _cur;        // provides a built-in way of accessing attributes in order.

    public:
        typedef std::map< const Inverse_attribute *const, iAstruct> iAMap_t;
        const EntityDescriptor *eDesc;
    protected:
#ifdef _MSC_VER
#pragma warning( push )
#pragma warning( disable: 4251 )
#endif
        iAMap_t iAMap;
#ifdef _MSC_VER
#pragma warning( pop )
#endif
        bool _complex;

    public: //TODO make these private?
        STEPattributeList attributes;

        /* see mgrnode.cc where -1 is returned when there is no sdai
         * instance.  might be possible to treat 0 for this purpose
         * instead of negative so the ID's can become unsigned.
         */
        int               STEPfile_id;

        ErrorDescriptor   _error;
#ifdef _MSC_VER
#pragma warning( push )
#pragma warning( disable: 4251 )
#endif
        std::string       p21Comment;
#ifdef _MSC_VER
#pragma warning( pop )
#endif

        /**
        ** head entity for multiple inheritance.  If it is null then this
        ** SDAI_Application_instance is not part of a multiply inherited entity.  If it
        ** points to a SDAI_Application_instance then this SDAI_Application_instance is part of a mi entity
        ** and head points at the root SDAI_Application_instance of the primary inheritance
        ** path (the one that is the root of the leaf entity).
        */
        SDAI_Application_instance *headMiEntity;
        /// these form a chain of other entity parents for multiple inheritance
        SDAI_Application_instance *nextMiEntity;

    public:
        SDAI_Application_instance();
        SDAI_Application_instance(int fileid, int complex = 0);
        virtual ~SDAI_Application_instance();

        bool IsComplex() const
        {
            return _complex;
        }
        /// initialize inverse attribute list
        void InitIAttrs();

        void StepFileId(int fid)
        {
            STEPfile_id = fid;
        }
        int StepFileId() const
        {
            return STEPfile_id;
        }

        void AddP21Comment(const std::string &s, bool replace = true);
        void AddP21Comment(const char *s, bool replace = true);
        void PrependP21Comment(const std::string &s);
        void DeleteP21Comment()
        {
            p21Comment = "";
        }

        std::string P21Comment() const
        {
            return p21Comment;
        }

        const char *EntityName(const char *schnm = NULL) const;

        virtual const EntityDescriptor *IsA(const EntityDescriptor *) const;

        virtual Severity ValidLevel(ErrorDescriptor *error, InstMgrBase *im,
                                    int clearError = 1);
        ErrorDescriptor &Error()
        {
            return _error;
        }
        // clears entity's error and optionally all attr's errors
        void ClearError(int clearAttrs = 1);
        // clears all attr's errors
        void ClearAttrError();

        virtual SDAI_Application_instance   *Replicate();

// ACCESS attributes in order.
        int AttributeCount();
        STEPattribute *NextAttribute();
        void ResetAttributes()
        {
            _cur = 0;
        }
// ACCESS inverse attributes
        const iAstruct getInvAttr(const Inverse_attribute *const ia) const;
        const iAMap_t::value_type getInvAttr(const char *name) const;
        void setInvAttr(const Inverse_attribute *const ia, const iAstruct ias);
        const iAMap_t &getInvAttrs() const
        {
            return iAMap;
        }

// READ
        virtual Severity STEPread(int id, int addFileId,
                                  class InstMgrBase *instance_set,
                                  std::istream &in = std::cin, const char *currSch = NULL,
                                  bool useTechCor = true, bool strict = true);
        virtual void STEPread_error(char c, int i, std::istream &in, const char *schnm);

// WRITE
        virtual void STEPwrite(std::ostream &out = std::cout, const char *currSch = NULL,
                               int writeComments = 1);
        virtual const char *STEPwrite(std::string &buf, const char *currSch = NULL);

        void WriteValuePairs(std::ostream &out, const char *currSch = NULL,
                             int writeComments = 1, int mixedCase = 1);

        void         STEPwrite_reference(std::ostream &out = std::cout);
        const char *STEPwrite_reference(std::string &buf);

        void beginSTEPwrite(std::ostream &out = std::cout);    ///< writes out the SCOPE section
        void endSTEPwrite(std::ostream &out = std::cout);

// MULTIPLE INHERITANCE
        int MultipleInheritance()
        {
            return !(headMiEntity == 0);
        }

        void HeadEntity(SDAI_Application_instance *se)
        {
            headMiEntity = se;
        }
        SDAI_Application_instance   *HeadEntity()
        {
            return headMiEntity;
        }

        SDAI_Application_instance   *GetNextMiEntity()
        {
            return nextMiEntity;
        }
        SDAI_Application_instance   *GetMiEntity(char *entName);
        void AppendMultInstance(SDAI_Application_instance *se);

    protected:
        STEPattribute *GetSTEPattribute(const char *nm, const char *entity = NULL);
        STEPattribute *MakeDerived(const char *nm, const char *entity = NULL);
        STEPattribute *MakeRedefined(STEPattribute *redefiningAttr,
                                     const char *nm);

        virtual void CopyAs(SDAI_Application_instance *);
        void PrependEntityErrMsg();
    public:
        // these functions are going to go away in the future.
        int SetFileId(int fid)
        {
            return STEPfile_id = fid;
        }
        int GetFileId() const
        {
            return STEPfile_id;
        }
        int FileId(int fid)
        {
            return STEPfile_id = fid;
        }
        int FileId() const
        {
            return STEPfile_id;
        }

};

// current style of CORBA handles for Part 23 - NOTE - used for more than CORBA
typedef SDAI_Application_instance *SDAI_Application_instance_ptr;
typedef SDAI_Application_instance_ptr SDAI_Application_instance_var;
SC_CORE_EXPORT bool isNilSTEPentity(const SDAI_Application_instance *ai);

#endif //STEPENTITY_H
