

use Parse::RecDescent;

#$RD_TRACE = 1;
#$RD_HINT = 1;

my $parser = Parse::RecDescent->new(<<'EOGRAMMAR');

    <autoaction: { bless \%item, $item{__RULE__} } >

	file:		element(s)

	element:	command | literal

	command:	'\\' literal options(?) args(?)

	options:	'[' option(s? /,/) ']'

	args:		'{' element(s?) '}'

	option:		/[^][\\$&%#_{}~^ \t\n,]+/

	literal:	/[^][\\$&%#_{}~^ \t\n]+/

EOGRAMMAR

local $/;
my $tree = $parser->file(<DATA>);

use Data::Dumper 'Dumper';
warn Dumper [ $tree ];

$tree->explain(0);

sub file::explain
{
	my ($self, $level) = @_;
	for (@{$self->{'element(s)'}})
	{
		$_->explain($level);
		print "\n";
	}
}

sub element::explain
{
	my ($self, $level) = @_;
	($self->{command}||$self->{literal})->explain($level)
}

sub command::explain
{
	my ($self, $level) = @_;
	print "\t"x$level, "Command: $self->{literal}{__PATTERN1__}\n";
	print "\t"x$level, "\tOptions:\n";
	$self->{'options(?)'}[0]->explain($level+2) if @{$self->{'options(?)'}};
	print "\t"x$level, "\tArgs:\n";
	$self->{'args(?)'}[0]->explain($level+2) if @{$self->{'args(?)'}};
}

sub options::explain
{
	my ($self, $level) = @_;
	$_->explain($level) foreach @{$self->{'option(s?)'}};
}

sub args::explain
{
	my ($self, $level) = @_;
	$_->explain($level) foreach @{$self->{'element(s?)'}};
}


sub option::explain
{
	my ($self, $level) = @_;
	print "\t"x$level, "Option: $self->{__PATTERN1__}\n";
}

sub literal::explain
{
	my ($self, $level) = @_;
	print "\t"x$level, "Literal: $self->{__PATTERN1__}\n";
}


__DATA__

\documentclass[a4paper,11pt]{article}
\usepackage{latexsym}
\author{D. Conway}
\title{Parsing \LaTeX{}}
\begin{document}
\maketitle
\tableofcontents
\section{Description}
...is easy \footnote{But not \emph{necessarily} simple}.
\end{document}
