#ifndef SDAIBINARY_H
#define SDAIBINARY_H 1

#include <string>

/*
* NIST STEP Core Class Library
* clstepcore/sdaiBinary.h
* April 1997
* KC Morris

* Development of this software was funded by the United States Government,
* and is not subject to copyright.
*/

class SC_DAI_EXPORT SDAI_Binary {
    private:
        std::string content;
    public:

        //constructor(s) & destructor
        SDAI_Binary( const char * str = 0, int max = 0 );
        SDAI_Binary( const std::string & s );
        ~SDAI_Binary( void );

        //  operators
        SDAI_Binary & operator= ( const char * s );

        void clear( void );
        bool empty( void ) const;
        const char * c_str( void ) const;
        // format for STEP
        const char * asStr() const  {
            return c_str();
        }
        void STEPwrite( ostream & out = cout )  const;
        const char * STEPwrite( std::string & s ) const;

        Severity StrToVal( const char * s, ErrorDescriptor * err );
        Severity STEPread( istream & in, ErrorDescriptor * err );
        Severity STEPread( const char * s, ErrorDescriptor * err );

        Severity BinaryValidLevel( const char * value, ErrorDescriptor * err,
                                   int optional, char * tokenList,
                                   int needDelims = 0, int clearError = 1 );
        Severity BinaryValidLevel( istream & in, ErrorDescriptor * err,
                                   int optional, char * tokenList,
                                   int needDelims = 0, int clearError = 1 );

    protected:
        Severity ReadBinary( istream & in, ErrorDescriptor * err, int AssignVal = 1,
                             int needDelims = 1 );
};

#endif
