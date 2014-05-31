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
our $force      = 0;
our $verbose    = 0;
our $debug      = 0;
our $clean      = 0;
our $devel      = 0;
our $chunkparse = 0; # 1 - parse input in chunks
our $quitundef  = 0; # 1 - stop at first undef chunk
our $maxchunks  = 0; # >0 - stop after N successful chunks

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

