use v5.10;
use warnings;


# THE ONLY TRUE MISTAKES ARE THE ONES YOU NEVER MAKE

#BEGIN {
#        close STDERR and open STDERR, '>./STDERR' or die $!;
#}
use Parse::RecDescent;

$grammar =
q{
        Para:     Sentence(s) /\Z/

        # Can also intercept the error messages like so: ##

           |    { use Data::Dumper 'Dumper';
                  print "$_->[0]\n" for @{$thisparser->{errors}};
                  exit;
                }

        Sentence: Noun Verb
                | Verb Noun
                | <error>

        Noun:     Fish
            |     Cat
            |     'dog'

        Verb:     'runs'

        Fish:     'fish'
            |     <error:Expected a fish!  But didn't get one>

        Cat:      'cat'
            |     <error:I wanna cat!>
};

$parse = new Parse::RecDescent ($grammar);

while (<DATA>)
{
        chomp;
        print "$_...\n";
    $parse->Para($_);
}

__DATA__
rat runs
dog runs
cat purrs
cat runs
mouse squeaks
