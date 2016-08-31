#! /usr/local/bin/perl -ws

#SHARED SYMBOL_TABLE

my %symbol_table = ();

package Operation;

sub new
{
	my ($class, %args) = @_;
	bless \%args, $class;
}


package Assignment_Op; @ISA = qw( Operation );

sub eval
{
	my ($self) = @_;
	$symbol_table{$self->{var}->{name}}
		= $self->{value}->eval();
}


package Addition_Op; @ISA = qw ( Operation );

sub eval
{
	my ($self) = @_;
	return $self->{left}->eval() + $self->{right}->eval();
}


package Multiplication_Op; @ISA = qw ( Operation );

sub eval
{
	my ($self) = @_;
	return $self->{left}->eval() * $self->{right}->eval();
}


package IfThenElse_Op; @ISA = qw ( Operation );

sub eval
{
	my ($self) = @_;
	if ($self->{condition}->eval() )
	{
		return $self->{true_expr}->eval();
	}
	else
	{
		return $self->{false_expr}->eval();
	}
}


package LessThan_Op; @ISA = qw ( Operation );

sub eval
{
	my ($self) = @_;
	return $self->{left}->eval() < $self->{right}->eval();
}


package Value_Op; @ISA = qw( Operation );

sub eval
{
	my ($self) = @_;
	return $self->{value};
}


package Variable_Op; @ISA = qw( Operation );

sub eval
{
	my ($self) = @_;
	return $symbol_table{$self->{name}};
}

package Sequence_Op;

sub new
{
	my ($class, $list_ref) = @_;
	bless $list_ref, $class;
}

sub eval
{
	my ($self) = @_;
	my $last_val;
	foreach my $statement ( @$self )
	{
		$last_val = $statement->eval();
	}
	return $last_val;
}


package main;

use Parse::RecDescent;

my $grammar = q
{
	Script:		Statement(s) /^$/
				{ Sequence_Op->new( $item[1] ) }

	Statement:	Assignment
		 |	IfThenElse
		 |	Expression
		 |	<error>

	Assignment:	Variable '<-' Expression
				{ Assignment_Op->new( var   => $item[1],
						   value => $item[3]) }

	Expression:	Product "+" Expression
				{ Addition_Op->new( left  => $item[1],
						    right => $item[3] ) }
		  |	Product

	Product:	Value "*" Product
				{ Multiplication_Op->new( left  => $item[1],
							  right => $item[3] ) }
	       |	Value

	Value:		/\d+/
				{ Value_Op->new( value => $item[1] ) }
	     |		Variable

	Variable:	/(?!if)[a-z]/
				{ Variable_Op->new( name => $item[1] ); }

	IfThenElse:	'if' Condition 'then' Statement 'else' Statement
				{ IfThenElse_Op->new( condition  => $item[2],
						      true_expr  => $item[4],
						      false_expr => $item[6]) }

	Condition:	Expression '<' Expression
				{ LessThan_Op->new( left  => $item[1],
						    right => $item[3] ) }



};

my $parser = Parse::RecDescent->new($grammar)
	or die "Bad grammar";

local $/;
my $script = <DATA>;

my $tree = $parser->Script($script)
	or die "Bad script";

print $tree->eval();

__DATA__

a <- 1

b <- 2

if a<b then
	c <- 3
else
	c <- 99

b*c+a
