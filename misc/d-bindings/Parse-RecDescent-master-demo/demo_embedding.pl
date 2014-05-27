#! /usr/local/bin/perl -w

use Parse::RecDescent;

#$RD_TRACE=1;

my $parser = Parse::RecDescent->new(<<'EOGRAMMAR');

	file: 		<skip:''> item(s)

	item: 		directive
	    | 		text

	directive: 	'<%' <skip:'\\s*'> command arg(s?) m|/?%>|
				{ $return = bless \%item, 'directive' }

	command: 	m|/?[a-z]\w*|i

	arg:		argname '=' string
				{ $return = \%item }
	   |		string
				{ $return = \%item }
	   |		data
				{ $return = \%item }

	argname:	/[a-z]\w*/i

	string:		'"' /[^\\"]*(\\.[^\\"]*)*/ '"'
				{ $return = $item[2] }

	data:		m|((?!/?%>)\S)+|


	text: 		/((?!<%).)+/s
				{ $return = bless \$item[1], 'text' }

EOGRAMMAR

my $data = join '', <DATA>;

my $info = $parser->file($data);

use Data::Dumper;

print Data::Dumper->Dump($info);

__DATA__

<% If expr %>
the if worked
<% Else %>
it didn't work
<% /If %>
   >
<% Include file="foo.txt" /%>
   >
<% Run Function="myFunc" Attr="x" ...%>
<% Arg Name="row" %>
<TR><TD>Name</TD><TD>{name}</TD></TR>
<% /Arg %>
<% /Run %>
