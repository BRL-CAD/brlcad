package R;

use v5.14;

# holds varous regexes

# regexes for HObj use:
our $r_first_line
  = qr{\A \s*
       (typedef)?            # capture $1
       \s*
       (struct|enum|union)?  # capture $2

       ([\s\S]*)             # capture $3

       (\{)                  # capture $4

       \s* \z
    }ox;

our $r_only_line
  = qr{\A \s*
       (typedef)            # capture $1

       ([\s\S]*)            # capture $2

       (;)                  # capture $3

       \s* \z
    }ox;

our $r_last_line
  = qr{\A \s*
       (\})                     # capture $1

       ([\S\s]*)                # capture $2

       (\;)                     # capture $3

       \s* \z
    }ox;

# mandatory true return for a Perl module
1;
