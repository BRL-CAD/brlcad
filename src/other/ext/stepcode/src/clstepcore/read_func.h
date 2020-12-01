#ifndef READ_FUNC_H
#define READ_FUNC_H

#include <sc_export.h>
#include <sdai.h>

/// This was 512. According to 10303-21:2002 section 5.6: comment length is unlimited. FIXME need to check the code for potential problems before eliminating this limit completely.
#define MAX_COMMENT_LENGTH 8192

// print Error information for debugging purposes
extern SC_CORE_EXPORT void PrintErrorState( ErrorDescriptor & err );

// print istream error information for debugging purposes
extern SC_CORE_EXPORT void IStreamState( istream & in );

extern SC_CORE_EXPORT int ReadInteger( SDAI_Integer & val, istream & in, ErrorDescriptor * err,
                                       const char * tokenList );

extern SC_CORE_EXPORT int ReadInteger( SDAI_Integer & val, const char * s, ErrorDescriptor * err,
                                       const char * tokenList );

extern SC_CORE_EXPORT Severity IntValidLevel( const char * attrValue, ErrorDescriptor * err,
        int clearError, int optional, const char * tokenList );

extern SC_CORE_EXPORT std::string WriteReal( SDAI_Real val );

extern SC_CORE_EXPORT void WriteReal( SDAI_Real val, ostream & out );

extern SC_CORE_EXPORT int ReadReal( SDAI_Real & val, istream & in, ErrorDescriptor * err,
                                    const char * tokenList );

extern SC_CORE_EXPORT int ReadReal( SDAI_Real & val, const char * s, ErrorDescriptor * err,
                                    const char * tokenList );

extern SC_CORE_EXPORT Severity RealValidLevel( const char * attrValue, ErrorDescriptor * err,
        int clearError, int optional, const char * tokenList );

extern SC_CORE_EXPORT int ReadNumber( SDAI_Real & val, istream & in, ErrorDescriptor * err,
                                      const char * tokenList );

extern SC_CORE_EXPORT int ReadNumber( SDAI_Real & val, const char * s, ErrorDescriptor * err,
                                      const char * tokenList );

extern SC_CORE_EXPORT Severity NumberValidLevel( const char * attrValue, ErrorDescriptor * err,
        int clearError, int optional, const char * tokenList );


////////////////////

extern SC_CORE_EXPORT int   QuoteInString( istream & in );

extern SC_CORE_EXPORT void PushPastString( istream & in, std::string & s, ErrorDescriptor * err );

extern SC_CORE_EXPORT void PushPastImbedAggr( istream & in, std::string & s, ErrorDescriptor * err );

extern SC_CORE_EXPORT void PushPastAggr1Dim( istream & in, std::string & s, ErrorDescriptor * err );

////////////////////

extern SC_CORE_EXPORT Severity FindStartOfInstance( istream & in, std::string & inst );

///  used for instances that aren\'t valid - reads to next \';\'
extern SC_CORE_EXPORT Severity SkipInstance( istream & in, std::string & inst );

extern SC_CORE_EXPORT const char * SkipSimpleRecord( istream & in, std::string & buf, ErrorDescriptor * err );

/// this includes entity names
extern SC_CORE_EXPORT const char * ReadStdKeyword( istream & in, std::string & buf, int skipInitWS = 1 );

extern SC_CORE_EXPORT const char * GetKeyword( istream & in, const char * delims, ErrorDescriptor & err );

extern SC_CORE_EXPORT int FoundEndSecKywd( istream& in );

extern SC_CORE_EXPORT const char * ReadComment( std::string & ss, const char * s );

extern SC_CORE_EXPORT const char * ReadComment( istream & in, std::string & s );

extern SC_CORE_EXPORT Severity ReadPcd( istream & in ); //print control directive

extern SC_CORE_EXPORT void ReadTokenSeparator( istream & in, std::string * comments = 0 );

#endif
