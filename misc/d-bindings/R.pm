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

# for C line conversions
#   transform, e.g., '((struct wdb_metaballpt *)0)'
#   or               '(struct wdb_metaballpt *)0' to 'null'

# Note the outside set of parens may not exist, but they'll need to be
# paired and I'm not sure how to check that inside one regex, so I'll
# define two separate ones.

# two pairs of parens
our $r_null_2
  = qr{
      \( \s*      # outside open paren
      \(          # inside open paren

      [^\(\)\*]+  # collect all but parens and asterisks

      \)          # inside close paren
      \s* 0 \s*   # zero between the outer parens
      \)          # outside close paren
    }ox;

# one pair of parens
our $r_null_1
  = qr{
      \(          # inside open paren

      [^\(\)\*]+  # collect all but parens and asterisks

      \)          # inside close paren
      \s* 0       # zero after the close paren
    }ox;

# mandatory true return for a Perl module
1;
