#define SC_CORE_EXPORT
#define SC_DAI_EXPORT

#include <algorithm>
#include <set>
#include <string>
#include <assert.h>

#include "Registry.h"
#include "sc_strtoull.h"
#include "sdaiApplication_instance.h"
#include "read_func.h"
#include "SdaiSchemaInit.h"
#include "STEPcomplex.h"

#include "sectionReader.h"
#include "lazyFileReader.h"
#include "lazyInstMgr.h"
#include "lazyTypes.h"

#include "current_function.hpp"

sectionReader::sectionReader(lazyFileReader *parent, std::ifstream &file, std::streampos start, sectionID sid):
    _lazyFile(parent), _file(file), _sectionStart(start), _sectionID(sid)
{
    _fileID = _lazyFile->ID();
    _error = new ErrorDescriptor();
}

sectionReader::~sectionReader()
{
     delete _error;
}

std::streampos sectionReader::findNormalString(const std::string &str, bool semicolon)
{
    std::streampos found = -1, startPos = _file.tellg(), nextTry = startPos;
    int i = 0, l = str.length();
    char c;

    //i is reset every time a character doesn't match; if i == l, this means that we've found the entire string
    while(i < l || semicolon) {
        skipWS();
        c = _file.get();
        if((i == l) && (semicolon)) {
            if(c == ';') {
                break;
            } else {
                i = 0;
                _file.seekg(nextTry);
                continue;
            }
        }
        if(c == '\'') {
            //push past string
            _file.seekg(_file.tellg() - std::streampos(1));
            GetLiteralStr(_file, _lazyFile->getInstMgr()->getErrorDesc());
        }
        if((c == '/') && (_file.peek() == '*')) {
            //push past comment
            findNormalString("*/");
        }
        if(str[i] == c) {
            i++;
            if(i == 1) {
                nextTry = _file.tellg();
            }
        } else {
            if(!_file.good()) {
                break;
            }
            if(i >= 1) {
                _file.seekg(nextTry);
            }
            i = 0;
        }
    }
    if(i == l) {
        found = _file.tellg();
    }
    if(_file.is_open() && _file.good()) {
        return found;
    } else {
        return -1;
    }
}


//NOTE different behavior than const char * GetKeyword( istream & in, const char * delims, ErrorDescriptor & err ) in read_func.cc
// returns pointer to the contents of a static std::string
const char *sectionReader::getDelimitedKeyword(const char *delimiters)
{
    static std::string str;
    int c;
    str.clear();
    str.reserve(100);
    skipWS();
    while(c = _file.get(), _file.good()) {
        if(c == '-' || c == '_' || isupper(c) || isdigit(c) ||
                (c == '!' && str.length() == 0)) {
            str.append(1, c);
        } else if((c == '/') && (_file.peek() == '*') && (str.length() == 0)) {
            //push past comment
            findNormalString("*/");
            skipWS();
            continue;
        } else {
            _file.putback(c);
            break;
        }
    }
    c = _file.peek();
    if(!strchr(delimiters, c)) {
        std::cerr << SC_CURRENT_FUNCTION << ": missing delimiter. Found " << c << ", expected one of " << delimiters << " at end of keyword " << str << ". File offset: " << _file.tellg() << std::endl;
        abort();
    }
    return str.c_str();
}

/// search forward in the file for the end of the instance. Start position should
/// be the opening parenthesis; otherwise, it is likely to fail.
///NOTE *must* check return value!
std::streampos sectionReader::seekInstanceEnd(instanceRefs **refs)
{
    char c;
    int parenDepth = 0;
    while(c = _file.get(), _file.good()) {
        switch(c) {
            case '(':
                parenDepth++;
                break;
            case '/':
                if(_file.peek() == '*') {
                    findNormalString("*/");
                } else {
                    return -1;
                }
                break;
            case '\'':
                _file.seekg(_file.tellg() - std::streampos(1));
                GetLiteralStr(_file, _lazyFile->getInstMgr()->getErrorDesc());
                break;
            case '=':
                return -1;
            case '#':
                skipWS();
                if(isdigit(_file.peek())) {
                    if(refs != 0) {
                        if(! * refs) {
                            *refs = new std::vector< instanceID >;
                        }
                        instanceID n;
                        _file >> n;
                        (* refs)->push_back(n);
                    }
                } else {
                    return -1;
                }
                break;
            case ')':
                if(--parenDepth == 0) {
                    skipWS();
                    if(_file.get() == ';') {
                        return _file.tellg();
                    } else {
                        _file.seekg(_file.tellg() - std::streampos(1));
                    }
                }
            default:
                break;
        }
    }
    return -1;
    //NOTE - old way: return findNormalString( ")", true );
    // old memory consumption: 673728kb; User CPU time: 35480ms; System CPU time: 17710ms (with 266MB catia-ferrari-sharknose.stp)
    // new memory: 673340kb; User CPU time: 29890ms; System CPU time: 11650ms
}

void sectionReader::locateAllInstances()
{
    namedLazyInstance inst;
    while(inst = nextInstance(), (_file.good()) && (inst.loc.begin > 0)) {
        _lazyFile->getInstMgr()->addLazyInstance(inst);
    }
}

instanceID sectionReader::readInstanceNumber()
{
    char c;
    size_t digits = 0;
    instanceID id = 0;

    //find instance number ("# nnnn ="), where ' ' is any whitespace found by isspace()
    skipWS();
    c = _file.get();
    if((c == '/') && (_file.peek() == '*')) {
        findNormalString("*/");
    } else {
        _file.seekg(_file.tellg() - std::streampos(1));
    }
    skipWS();
    c = _file.get();
    if(c != '#') {
        return 0;
    }
    skipWS();

    // The largest instance ID yet supported is the maximum value of unsigned long long int
    assert(std::numeric_limits<instanceID>::max() <= std::numeric_limits<unsigned long long int>::max());

    size_t instanceIDLength = std::numeric_limits<instanceID>::digits10 + 1;
    char *buffer = new char[ instanceIDLength + 1 ];  // +1 for the terminating character

    std::stringstream errorMsg;

    do {
        c = _file.get();
        if(isdigit(c)) {
            buffer[ digits ] = c; //copy the character into the buffer
            digits++;

        } else {
            _file.seekg(_file.tellg() - std::streampos(1));
            break;
        }

        if(digits > instanceIDLength) {
            errorMsg << "A very large instance ID of string length greater then " << instanceIDLength << " found. Skipping data section " << _sectionID << ".";

            _error->GreaterSeverity(SEVERITY_INPUT_ERROR);
            _error->UserMsg("A very large instance ID encountered");
            _error->DetailMsg(errorMsg.str());

            delete buffer;
            return 0;
        }

    } while(_file.good());
    buffer[ digits ] = '\0'; //Append the terminating character
    skipWS();

    if(_file.good() && (digits > 0) && (_file.get() == '=')) {
        id = strtoull(buffer, NULL, 10);
        if(id == std::numeric_limits<instanceID>::max()) {
            //Handling those cases where although the number of digits is equal, but the id value is greater then equal to the maximum allowed value.
            errorMsg << "A very large instance ID caused an overflow. Skipping data section " << _sectionID << ".";

            _error->GreaterSeverity(SEVERITY_INPUT_ERROR);
            _error->UserMsg("A very large instance ID encountered");
            _error->DetailMsg(errorMsg.str());
        }

        assert(id > 0);
    }
    delete [] buffer;
    return id;
}

/** load an instance and return a pointer to it.
 * side effect: recursively loads any instances the specified instance depends upon
 */
SDAI_Application_instance *sectionReader::getRealInstance(const Registry *reg, long int begin, instanceID instance,
        const std::string &typeName, const std::string &schName, bool header)
{
    char c;
    const char *tName = 0, * sName = 0;  //these are necessary since typeName and schName are const
    std::string comment;
    Severity sev = SEVERITY_NULL;
    SDAI_Application_instance *inst = 0;

    tName = typeName.c_str();
    if(schName.size() > 0) {
        sName = schName.c_str();
    } else if(!header) {
        SdaiFile_schema *fs = dynamic_cast< SdaiFile_schema * >(_lazyFile->getHeaderInstances()->find(3));
        if(fs) {
            StringNode *sn = (StringNode *) fs->schema_identifiers_()->GetHead();
            if(sn) {
                sName = sn->value.c_str();
                if(sn->NextNode()) {
                    std::cerr << "Warning - multiple schema names found. Only searching with first one." << std::endl;
                }
            }
        } else {
            std::cerr << "Warning - no schema names found; the file is probably invalid. Looking for typeName in any loaded schema." << std::endl;
        }
    }

    _file.seekg(begin);
    skipWS();
    ReadTokenSeparator(_file, &comment);
    if(!header) {
        findNormalString("=");
    }
    skipWS();
    ReadTokenSeparator(_file, &comment);
    c = _file.peek();
    switch(c) {
        case '&':
            std::cerr << "Can't handle scope instances. Skipping #" << instance << ", offset " << _file.tellg() << std::endl;
            // sev = CreateScopeInstances( in, &scopelist );
            break;
        case '(':
            inst = CreateSubSuperInstance(reg, instance, sev);
            break;
        case '!':
            std::cerr << "Can't handle user-defined instances. Skipping #" << instance << ", offset " << _file.tellg() << std::endl;
            break;
        default:
            if((!header) && (typeName.size() == 0)) {
                tName = getDelimitedKeyword(";( /\\");
            }
            inst = reg->ObjCreate(tName, sName);
            break;
    }
    if(!isNilSTEPentity(inst)) {
        if(!comment.empty()) {
            inst->AddP21Comment(comment);
        }
        assert(inst->eDesc);
        _file.seekg(begin);
        findNormalString("(");
        _file.seekg(_file.tellg() - std::streampos(1));
        sev = inst->STEPread(instance, 0, _lazyFile->getInstMgr()->getAdapter(), _file, sName, true, false);
        //TODO do something with 'sev'
        inst->InitIAttrs();
    }
    return inst;
}

STEPcomplex *sectionReader::CreateSubSuperInstance(const Registry *reg, instanceID fileid, Severity &)
{
    std::string buf;
    ErrorDescriptor err;
    std::vector<std::string *> typeNames;
    _file.get(); //move past the first '('
    skipWS();
    while(_file.good() && (_file.peek() != ')')) {
        typeNames.push_back(new std::string(getDelimitedKeyword(";( /\\\n")));
        if(typeNames.back()->empty()) {
            delete typeNames.back();
            typeNames.pop_back();
        } else {
            SkipSimpleRecord(_file, buf, &err);   //exactly what does this do? if it doesn't count parenthesis, it probably should
            buf.clear();
        }
        skipWS();
        if(_file.peek() != ')') {
            // do something
        }
    }
    // STEPComplex needs an array of strings or of char*. construct the latter using c_str() on all strings in the vector
    //FIXME: STEPComplex ctor should accept std::vector<std::string *> ?
    const int s = typeNames.size();
    const char **names = new const char *[ s + 1 ];
    names[ s ] = 0;
    for(int i = 0; i < s; i++) {
        names[ i ] = typeNames[i]->c_str();
    }
    //TODO still need the schema name
    STEPcomplex *sc = new STEPcomplex((const_cast<Registry *>(reg)), names, (int) fileid /*, schnm*/);
    delete[] names;
    //TODO also delete contents of typeNames!
    return sc;
}

