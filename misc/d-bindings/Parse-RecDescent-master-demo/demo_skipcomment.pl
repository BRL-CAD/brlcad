#!/usr/local/bin/perl -ws

# REMOVE COMMENTS FROM C++ CODE

use strict;
use Parse::RecDescent;

use vars qw/ $Grammar /;

$RD_TRACE=1;
my $parser = new Parse::RecDescent $Grammar  or  die "invalid grammar";

undef $/;
my $text = @ARGV ? <> : <DATA>;

print join " ", @{$parser->program($text) or die "malformed C program"};

BEGIN
{ $Grammar=<<'EOF';

program	: <skip:qr/(\s+|#.*\n)*/> part(s)

part	: /\S+/
EOF
}
__DATA__

Now we find # to our inexpressible joy
That the parser # a cunningly wrought thing
Skips both whitespace # the usual behaviour
# though it be configurable
And comments
#however many there be
