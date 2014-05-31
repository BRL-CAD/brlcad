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
our $chunkparse   = 0; # >0 - parse input in chunks
our $quitundef    = 0; # >0 - stop at first undef chunk
our $maxchunks    = 0; # >0 - stop after N successful chunks
our $inspect_tree = 0; # >0 - try to interpret parse tree
our $addfunc      = 0; # >0 - add a function to the top of the header for devel

Readonly our $NEW  => -1;
Readonly our $SAME =>  0;
Readonly our $DIFF =>  1;

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

