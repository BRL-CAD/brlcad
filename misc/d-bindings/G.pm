package G;

use Readonly;

# globals

# no Windows for now
Readonly our $WIN => 0;

# file type suffixes
Readonly our $Hsuf => '.h';
Readonly our $Dsuf => '.d';

#==========================================================================
# global option vars
our $force        = 0;
our $verbose      = 0;
our $debug        = 0;
our $clean        = 0;
our $devel        = 0;
our $quitundef    = 0; # >0 - stop at first undef chunk
our $maxchunks    = 0; # >0 - stop after N successful chunks
our $inspect_tree = 0; # >0 - try to interpret parse tree
our $addfunc      = 0; # >0 - add a function to the top of the header for devel
our $pretty_print = 0; # >0 - pretty print objects

Readonly our $NEW  => -1;
Readonly our $SAME =>  0;
Readonly our $DIFF =>  1;

# 64-bit mapping from C types to D types
#   from: http://wiki.dlang.org/Converting_C_.h_Files_to_D_Modules#Types
our %d64map
  = (
     'long double'        => 'real',

     'unsigned long long' => 'ulong',
     'long long'          => 'long',
     'unsigned long'	  => 'ulong', # Linux [int (Windows)]
     'long'               => 'long', # Linux [int (Windows)]

     # these explicit variants are missing:
     'unsigned long long int' => 'ulong',
     'long long int'          => 'long',
     'unsigned long int'      => 'ulong', # Linux [int (Windows)]
     'long int'               => 'long', # Linux [int (Windows)]


     'unsigned'           => 'uint',
     'unsigned int'       => 'int',
     'unsigned short'     => 'ushort',

     'signed char'        => 'byte',
     'unsigned char'      => 'ubyte',
     'wchar_t'            => 'wchar', # or dchar
     #'bool'               => 'bool', # byte, int
     #'size_t'             => 'size_t',
     #'ptrdiff_t'          => 'ptrdiff_t',

     # other
     'NULL'               => 'null',

    );

# the preferred order for checking
our @d64map_keys
  = (
     'unsigned long long',
     'long long',
     'unsigned long',
     'long',

     # these explicit variants are missing:
     'unsigned long long int',
     'long long int',
     'unsigned long int', # Linux [int (Windows)]
     'long int',          # Linux [int (Windows)]

     'unsigned int',
     'unsigned short',
     'unsigned char',

     'unsigned',

     'long double',
     'signed char',

      'wchar_t',

     # other
     'NULL',
    );

# 32-bit mapping from C types to D types
#   from: http://wiki.dlang.org/Converting_C_.h_Files_to_D_Modules#Types
our %d32map
  = (
     'long double'        => 'real',

     'unsigned long long' => 'ulong',
     'long long'          => 'long',
     'unsigned long'      => 'uint',
     'long'               => 'int',

     # these explicit variants are missing:
     'unsigned long long int' => 'ulong',
     'long long int'          => 'long',
     'unsigned long int'      => 'uint',
     'long int'               => 'int',

     # this impplicit variant is missing
     'unsigned'           => 'uint',

     'unsigned int'       => 'uint',
     #'int'                => 'int',
     'unsigned short'     => 'ushort',
     'signed char'        => 'byte',
     'unsigned char'      => 'ubyte',
     'wchar_t'            => 'wchar', # or dchar
     #'bool'               => 'bool',  # byte, int
     #'size_t'             => 'size_t',
     #'ptrdiff_t'          => 'ptrdiff_t',

     # other
     'NULL'               => 'null',
    );

# the preferred order for checking
our @d32map_keys
  = (
     'unsigned long long' => 'ulong',
     'long long'          => 'long',
     'unsigned long'      => 'uint',
     'long'               => 'int',

     # these explicit variants are missing:
     'unsigned long long int' => 'ulong',
     'long long int'          => 'long',
     'unsigned long int'      => 'uint',
     'long int'               => 'int',

     # this impplicit variant is missing
     'unsigned'           => 'uint',

     'unsigned int'       => 'uint',
     'unsigned short'     => 'ushort',
     'signed char'        => 'byte',
     'unsigned char'      => 'ubyte',

     'wchar_t'            => 'wchar', # or dchar

     # other
     'NULL',
    );

# input type for functions in ParsePPCHeader
our @ityp
  = (
     'fnam',
     'fp',
     'str',
     'aref',
    );
our %ityp;
@ityp{@ityp} = ();

# output type for functions in ParsePPCHeader
our @otyp
  = (
     'fnam',
     'fp',
     'str',
     'aref',
    );
our %otyp;
@otyp{@otyp} = ();

# mandatory true return for a Perl module
1;

