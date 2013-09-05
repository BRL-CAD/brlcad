#ifndef STEPCOMPLEX_H
#define STEPCOMPLEX_H

#include <sc_export.h>
#include <errordesc.h>
#include <sdai.h>
#include <baseType.h>
#include <ExpDict.h>
#include <Registry.h>

#include <list>

typedef std::list<void *>            STEPcomplex_attr_data_list;
typedef std::list<void *>::iterator  STEPcomplex_attr_data;

class SC_CORE_EXPORT STEPcomplex : public SDAI_Application_instance {
    public: //TODO should this _really_ be public?!
        STEPcomplex * sc;
        STEPcomplex * head;
        Registry * _registry;
        int visited; ///< used when reading (or as you wish?)
        STEPcomplex_attr_data_list _attr_data_list;
    public:
        STEPcomplex( Registry * registry, int fileid );
        STEPcomplex( Registry * registry, const std::string ** names, int fileid,
                     const char * schnm = 0 );
        STEPcomplex( Registry * registry, const char ** names, int fileid,
                     const char * schnm = 0 );
        virtual ~STEPcomplex();

        int EntityExists( const char * name, const char * currSch = 0 );
        STEPcomplex * EntityPart( const char * name, const char * currSch = 0 );


        virtual const EntityDescriptor * IsA( const EntityDescriptor * ) const;

        virtual Severity ValidLevel( ErrorDescriptor * error, InstMgr * im,
                                     int clearError = 1 );
// READ
        virtual Severity STEPread( int id, int addFileId,
                                   class InstMgr * instance_set,
                                   istream & in = cin, const char * currSch = NULL,
                                   bool useTechCor = true, bool strict = true );

        virtual void STEPread_error( char c, int index, istream& in, const char * schnm );

// WRITE
        virtual void STEPwrite( ostream & out = cout, const char * currSch = NULL,
                                int writeComment = 1 );
        virtual const char * STEPwrite( std::string & buf, const char * currSch = NULL );

        SDAI_Application_instance * Replicate();

        virtual void WriteExtMapEntities( ostream & out = cout,
                                          const char * currSch = NULL );
        virtual const char * WriteExtMapEntities( std::string & buf,
                const char * currSch = NULL );
        virtual void AppendEntity( STEPcomplex * stepc );

    protected:
        virtual void CopyAs( SDAI_Application_instance * se );
        void BuildAttrs( const char * s );
        void AddEntityPart( const char * name );
        void AssignDerives();
        void Initialize( const char ** names, const char * schnm );
};

#endif
