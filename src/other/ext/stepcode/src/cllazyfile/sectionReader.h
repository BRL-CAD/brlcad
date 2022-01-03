#ifndef SECTIONREADER_H
#define SECTIONREADER_H

#include <fstream>
#include <set>
#include "lazyTypes.h"
#include "sc_memmgr.h"
#include "sc_export.h"
#include "errordesc.h"
#include "STEPcomplex.h"

class SDAI_Application_instance;
class lazyFileReader;
class ErrorDescriptor;
class Registry;

class SC_LAZYFILE_EXPORT sectionReader
{
    protected:
        //protected data members
        lazyFileReader *_lazyFile;
#ifdef _MSC_VER
#pragma warning( push )
#pragma warning( disable: 4251 )
#endif
        std::ifstream &_file;

        std::streampos _sectionStart,  ///< the start of this section as reported by tellg()
            _sectionEnd;               ///< the end of this section as reported by tellg()
#ifdef _MSC_VER
#pragma warning( pop )
#endif
        unsigned long _totalInstances;

        ErrorDescriptor *_error;
        sectionID _sectionID;
        fileID _fileID;

        // protected member functions

        sectionReader(lazyFileReader *parent, std::ifstream &file, std::streampos start, sectionID sid);
	~sectionReader();

        /** Find a string, ignoring occurrences in comments or Part 21 strings (i.e. 'string with \S\' control directive' )
         * \param str string to find
         * \param semicolon if true, 'str' must be followed by a semicolon, possibly preceded by whitespace.
         * \returns the position of the end of the found string
         */
        std::streampos findNormalString(const std::string &str, bool semicolon = false);

        /** Get a keyword ending with one of delimiters.
         */
        const char *getDelimitedKeyword(const char *delimiters);

        /** Seek to the end of the current instance */
        std::streampos seekInstanceEnd(instanceRefs **refs);

        /// operator>> is very slow?!
        inline void skipWS()
        {
            while(isspace(_file.peek()) && _file.good()) {
                _file.ignore(1);
            }
        }

        STEPcomplex *CreateSubSuperInstance(const Registry *reg, instanceID fileid, Severity &sev);

    public:
        SDAI_Application_instance *getRealInstance(const Registry *reg, long int begin, instanceID instance,
                const std::string &typeName = "", const std::string &schName = "", bool header = false);

        sectionID ID() const
        {
            return _sectionID;
        }

        virtual void findSectionStart() = 0;

        void findSectionEnd()
        {
            _sectionEnd = findNormalString("ENDSEC", true);
        }

        std::streampos sectionStart() const
        {
            return _sectionStart;
        }

        std::streampos sectionEnd() const
        {
            return _sectionEnd;
        }

        void locateAllInstances(); /**< find instances in section, and add lazyInstance's to lazyInstMgr */

        virtual const namedLazyInstance nextInstance() = 0;

        /** returns the type string for an instance, read straight from the file
         * if this function changes, probably need to change nextInstance() as well
         * don't check errors - they would have been encountered during the initial file scan, and the file is still open so it can't have been modified */
        const char *getType(long int offset)
        {
            if(offset <= 0) {
                return 0;
            }
            _file.seekg(offset);
            readInstanceNumber();
            skipWS();
            return getDelimitedKeyword(";( /\\");
        }

        instanceID readInstanceNumber();

        void seekg(std::streampos pos)
        {
            _file.seekg(pos);
        }
        std::streampos tellg()
        {
            return _file.tellg();
        }
};

#endif //SECTIONREADER_H

