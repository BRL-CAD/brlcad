#ifndef STEPCOMPLEX_H
#define STEPCOMPLEX_H

#include <sc_export.h>
#include <errordesc.h>
#include <sdai.h>
#include <baseType.h>
#include <ExpDict.h>
#include <Registry.h>

#include <list>

/* attr's for SC's are created with a pointer to their data.
 * STEPcomplex_attr_data_list is used to store the pointers for
 * deletion. this list is composed of attrData_t's, which track
 * types for ease of deletion
 */
typedef struct {
    PrimitiveType type;
    union {
        SDAI_Integer *i;
        SDAI_String *str;
        SDAI_Binary *bin;
        SDAI_Real *r;
        SDAI_BOOLEAN *b;
        SDAI_LOGICAL *l;
        SDAI_Application_instance **ai;
        SDAI_Enum *e;
        SDAI_Select *s;
        STEPaggregate *a;
    };
} attrData_t;
typedef std::list< attrData_t >               STEPcomplex_attr_data_list;
typedef STEPcomplex_attr_data_list::iterator  STEPcomplex_attr_data_iter;

/** FIXME are inverse attr's initialized for STEPcomplex? */


class SC_CORE_EXPORT STEPcomplex : public SDAI_Application_instance
{
    public: //TODO should this _really_ be public?!
        STEPcomplex *sc;
        STEPcomplex *head;
        Registry *_registry;
        int visited; ///< used when reading (or as you wish?)
#ifdef _MSC_VER
#pragma warning( push )
#pragma warning( disable: 4251 )
#endif
        STEPcomplex_attr_data_list _attr_data_list; ///< attrs are created with a pointer to data; this stores them for deletion
#ifdef _MSC_VER
#pragma warning( pop )
#endif
    public:
        STEPcomplex(Registry *registry, int fileid);
        STEPcomplex(Registry *registry, const std::string **names, int fileid,
                    const char *schnm = 0);
        STEPcomplex(Registry *registry, const char **names, int fileid,
                    const char *schnm = 0);
        virtual ~STEPcomplex();

        int EntityExists(const char *name, const char *currSch = 0);
        STEPcomplex *EntityPart(const char *name, const char *currSch = 0);


        virtual const EntityDescriptor *IsA(const EntityDescriptor *) const;

        virtual Severity ValidLevel(ErrorDescriptor *error, InstMgrBase *im,
                                    int clearError = 1);
// READ
        virtual Severity STEPread(int id, int addFileId,
                                  class InstMgrBase *instance_set,
                                  istream &in = cin, const char *currSch = NULL,
                                  bool useTechCor = true, bool strict = true);

        virtual void STEPread_error(char c, int index, istream &in, const char *schnm);

// WRITE
        virtual void STEPwrite(ostream &out = cout, const char *currSch = NULL,
                               int writeComment = 1);
        virtual const char *STEPwrite(std::string &buf, const char *currSch = NULL);

        SDAI_Application_instance *Replicate();

        virtual void WriteExtMapEntities(ostream &out = cout,
                                         const char *currSch = NULL);
        virtual const char *WriteExtMapEntities(std::string &buf,
                                                const char *currSch = NULL);
        virtual void AppendEntity(STEPcomplex *stepc);

    protected:
        virtual void CopyAs(SDAI_Application_instance *se);
        void BuildAttrs(const char *s);
        void AddEntityPart(const char *name);
        void AssignDerives();
        void Initialize(const char **names, const char *schnm);
};

#endif
