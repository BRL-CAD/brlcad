#ifndef STEPCOMPLEX_H
#define STEPCOMPLEX_H

#include <errordesc.h>
#include <sdai.h>
#include <baseType.h>
#include <ExpDict.h>
//#include <STEPentity.h>
#include <Registry.h>

class STEPcomplex : public SCLP23(Application_instance) {
  public:
    STEPcomplex * sc;
    STEPcomplex * head;
    Registry * _registry;
    int visited; // used when reading (or as you wish?)

  public:
    STEPcomplex(Registry *registry, int fileid);
    STEPcomplex(Registry *registry, const SCLstring **names, int fileid,
		const char *schnm =0);
    STEPcomplex(Registry *registry, const char **names, int fileid,
		const char *schnm =0);
    virtual ~STEPcomplex();

    int EntityExists(const char *name, const char *currSch =0);
    STEPcomplex *EntityPart(const char *name, const char *currSch =0);

/*
    // page 241 Stroustrup
    STEPcomplex &operator[](const char *name);
    STEPcomplex &operator[](const int index);
*/

    virtual const EntityDescriptor * IsA(const EntityDescriptor *) const;
    
    virtual Severity ValidLevel(ErrorDescriptor *error, InstMgr *im, 
			int clearError = 1);
// READ
    virtual Severity STEPread(int id, int addFileId, 
			      class InstMgr * instance_set,
			      istream& in =cin, const char *currSch =NULL,
			      int useTechCor =1);

    virtual void STEPread_error(char c, int index, istream& in);

// WRITE
    virtual void STEPwrite(ostream& out =cout, const char *currSch =NULL,
			   int writeComment = 1);
    virtual const char * STEPwrite(SCLstring &buf, const char *currSch =NULL);

    SCLP23(Application_instance) *Replicate();

    virtual void WriteExtMapEntities(ostream& out =cout,
				     const char *currSch =NULL);
    virtual const char * WriteExtMapEntities(SCLstring &buf,
					     const char *currSch =NULL);
    virtual void AppendEntity(STEPcomplex *stepc);

  protected:
    virtual void CopyAs(SCLP23(Application_instance) *);
    void BuildAttrs(const char *s);
    void AddEntityPart(const char *name);
    void AssignDerives();
    void Initialize(const char **, const char *);
};

#endif
