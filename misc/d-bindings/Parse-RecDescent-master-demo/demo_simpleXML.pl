#! /usr/local/bin/perl -w

package XML2DS;
use Parse::RecDescent;
@ISA = qw( Parse::RecDescent );

sub allow_nested
{
	my ($parser, $tag, @nestedtags) = @_;
	my $nestedtags = join '|', map { '^'.$_.'$' } @nestedtags;
	$parser->{allow}{$tag} = qr/$nestedtags/;
}

sub new
{
	bless Parse::RecDescent->new(<<'EOGRAMMAR'), XML2DS;

	xml:  unitag(?)
	   |  tag content[$item[1]](s) endtag[$item[1]]
					{ bless $item[2], $item[1]}

	unitag:
		m{<([a-zA-Z]+)/>}	{ bless [], $1 }

	tag:
		m{<([a-zA-Z]+)>}	{ $return = $1 }

	endtag:
		m{</$arg[0]>}
	      |	m{(\S+)} <error: was expecting </$arg[0]> but found $1 instead>

	content: <rulevar: local $error>
	content: rawtext <commit> check[$arg[0], $item[1]]
	       | xml   <commit> check[$arg[0], $item[1]]
	       | <error?: $error>  <error: error in <$arg[0]> block>

	rawtext: m{[^<]+}		{ bless \$item[1], 'rawtext' }

	check: { my ($outertag, $innertag) = ($arg[0], ref $arg[1]);
		 $return = $arg[1] if !$thisparser->{allow}{$outertag}
				   || $innertag =~ $thisparser->{allow}{$outertag};
		 $error = ($innertag eq 'rawtext')
			? "Raw text not valid in <$outertag> block"
			: "<$innertag> tag not valid in <$outertag> block";
		 undef;
		}
EOGRAMMAR
}


package main;
use Data::Dumper;

my $parser = XML2DS->new();

$parser->allow_nested( Test    => qw(Example)	      );
$parser->allow_nested( Example => qw(Data    rawtext) );
$parser->allow_nested( Data    => qw(SubData rawtext) );
$parser->allow_nested( SubData => qw(Example rawtext) );

my $xml = join '', <DATA>;

if (my $tree = $parser->xml($xml))
{
	print Data::Dumper->Dump([$tree]);
}


__DATA__

<Test>
	<Example/>
	<Example>
		<Data>
			raw data
			<SubData>
				raw subdata
			</SubData>
			more raw data
		</Data>
		<Data>
			still more raw data
			<SubData>
				<Example>
					<Data>
						nested example data
					</Data>
				</Example>
			</SubData>
		</Data>
		last rawtext
	</Example>
</Test>
