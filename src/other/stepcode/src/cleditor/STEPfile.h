#ifndef _STEPFILE_H
#define _STEPFILE_H

/*
* NIST STEP Core Class Library
* cleditor/STEPfile.h
* April 1997
* Peter Carr
* K. C. Morris
* David Sauder

* Development of this software was funded by the United States Government,
* and is not subject to copyright.
*/

#include <sc_export.h>
#include <string>
#include <instmgr.h>
#include <Registry.h>
#include <fstream>
#include <dirobj.h>
#include <errordesc.h>
#include <time.h>

#include <read_func.h>

//error reporting level
#define READ_COMPLETE    10
#define READ_INCOMPLETE  20

enum  FileTypeCode {
    VERSION_OLD     = -1,
    VERSION_UNKNOWN =  0,
    VERSION_CURRENT =  1,
    WORKING_SESSION =  2
};

class SC_EDITOR_EXPORT STEPfile {
    protected:
        //data members

        InstMgr & _instances;
        Registry & _reg;

        InstMgr & instances()  {
            return _instances;
        }
        Registry & reg() {
            return _reg;
        }
        int _fileIdIncr;   ///< Increment value to be added to FileId Numbers on input

//header information
        InstMgr * _headerInstances;
        Registry * _headerRegistry;

        int _headerId;     ///< STEPfile_id given to SDAI_Application_instance from header section

//file information
        DirObj * _currentDir;
        std::string _fileName;

        //the following are used to compute read/write progress
        std::ifstream::pos_type _iFileSize; ///< input file size
        std::ifstream::pos_type _iFileCurrentPosition; ///< input file position (from ifstream::tellg())
        bool _iFileStage1Done; ///< set immediately before ReadData1() returns
        int _oFileInstsWritten; ///< number of instances that have been written

//error information
        ErrorDescriptor _error;

        // new errors
        int _entsNotCreated; ///< num entities not created in first pass
        int _entsInvalid;    ///< num entities that had invalid attr values
        int _entsIncomplete; /**< num entities that had missing attr values
                                  (includes entities that had invalid values
                                  for required attrs)*/
        int _entsWarning;    /**< num entities that may have had problems
                                  with attrs - reported as an attr user msg */

        // old errors
        int _errorCount;
        int _warningCount;

        int _maxErrorCount;

        bool _strict;       ///< If false, "missing and required" attributes are replaced with a generic value when file is read
        bool _verbose;      ///< Defaults to false; if true, info is always printed to stdout.

    protected:

//file type information
        FileTypeCode _fileType;
        char ENTITY_NAME_DELIM;
        std::string FILE_DELIM;
        std::string END_FILE_DELIM;

//public member functions
    public:

//public access to member variables
//header information
        InstMgr * HeaderInstances() {
            return _headerInstances;
        }
        const Registry * HeaderRegistry() {
            return _headerRegistry;
        }
// to create header instances
        SDAI_Application_instance * HeaderDefaultFileName();
        SDAI_Application_instance * HeaderDefaultFileDescription();
        SDAI_Application_instance * HeaderDefaultFileSchema();

//file information
        std::string FileName() const {
            return _fileName;
        }
        std::string SetFileName( const std::string name = "" );
        std::string TruncFileName( const std::string name ) const;
        float GetReadProgress() const;
        float GetWriteProgress() const;

//error information
        ErrorDescriptor & Error() { /* const */
            return _error;
        }
        int ErrorCount() const  {
            return _errorCount;
        }
        int WarningCount() const {
            return _warningCount;
        }
        Severity AppendEntityErrorMsg( ErrorDescriptor * e );

//version information
        FileTypeCode FileType() const   {
            return _fileType;
        }
        void FileType( FileTypeCode ft ) {
            _fileType = ft;
        }
        int SetFileType( FileTypeCode ft = VERSION_CURRENT );

//Reading and Writing
        Severity ReadExchangeFile( const std::string filename = "", bool useTechCor = 1 );
        Severity AppendExchangeFile( const std::string filename = "", bool useTechCor = 1 );

        Severity ReadWorkingFile( const std::string filename = "", bool useTechCor = 1 );
        Severity AppendWorkingFile( const std::string filename = "", bool useTechCor = 1 );

        Severity AppendFile( istream * in, bool useTechCor = 1 ) ;

        Severity WriteExchangeFile( ostream & out, int validate = 1,
                                    int clearError = 1, int writeComments = 1 );
        Severity WriteExchangeFile( const std::string filename = "", int validate = 1,
                                    int clearError = 1, int writeComments = 1 );
        Severity WriteValuePairsFile( ostream & out, int validate = 1,
                                      int clearError = 1,
                                      int writeComments = 1, int mixedCase = 1 );

        Severity WriteWorkingFile( ostream & out, int clearError = 1,
                                   int writeComments = 1 );
        Severity WriteWorkingFile( const std::string filename = "", int clearError = 1,
                                   int writeComments = 1 );

        stateEnum EntityWfState( char c );

        void Renumber();

//constructors
        STEPfile( Registry & r, InstMgr & i, const std::string filename = "", bool strict = true );
        virtual ~STEPfile();

    protected:
//member functions
        std::string schemaName(); /**< Returns and copies out schema name from header instances.
                                             Called by ReadExchangeFile */
        istream * OpenInputFile( const std::string filename = "" );
        void CloseInputFile( istream * in );

        Severity ReadHeader( istream & in );

        Severity HeaderVerifyInstances( InstMgr * im );
        void HeaderMergeInstances( InstMgr * im );

        int HeaderId( int increment = 1 );
        int HeaderId( const char * nm = "\0" );

        int ReadData1( istream & in ); /**< First pass, to create instances */
        int ReadData2( istream & in, bool useTechCor = true ); /**< Second pass, to read instances */

// obsolete
        int ReadWorkingData1( istream & in );
        int ReadWorkingData2( istream & in, bool useTechCor = true );

        void ReadRestOfFile( istream & in );

        /// create instance - used by ReadData1()
        SDAI_Application_instance  *  CreateInstance( istream & in, ostream & out );
        /// create complex instance - used by CreateInstance()
        SDAI_Application_instance  * CreateSubSuperInstance( istream & in, int fileid,
                ErrorDescriptor & );

        // read the instance - used by ReadData2()
        SDAI_Application_instance  * ReadInstance( istream & in, ostream & out,
                std::string & cmtStr, bool useTechCor = true );

        ///  reading scopes are still incomplete, CreateScopeInstances and ReadScopeInstances are stubs
        Severity CreateScopeInstances( istream & in, SDAI_Application_instance_ptr  ** scopelist );
        Severity ReadScopeInstances( istream & in );
//    Severity ReadSubSuperInstance(istream& in);

        int FindDataSection( istream & in );
        int FindHeaderSection( istream & in );

// writing working session files
        void WriteWorkingData( ostream & out, int writeComments = 1 );

//called by WriteExchangeFile
        ofstream * OpenOutputFile( const std::string filename = "" );
        void CloseOutputFile( ostream * out );

        void WriteHeader( ostream & out );
        void WriteHeaderInstance( SDAI_Application_instance * obj, ostream & out );
        void WriteHeaderInstanceFileName( ostream & out );
        void WriteHeaderInstanceFileDescription( ostream & out );
        void WriteHeaderInstanceFileSchema( ostream & out );

        void WriteData( ostream & out, int writeComments = 1 );
        void WriteValuePairsData( ostream & out, int writeComments = 1,
                                  int mixedCase = 1 );

        int IncrementFileId( int fileid );
        int FileIdIncr() {
            return _fileIdIncr;
        }
        void SetFileIdIncrement();
        void MakeBackupFile();

//    void ReadWhiteSpace(istream& in);
};

//inline functions

#endif  /*  _STEPFILE_H  */
