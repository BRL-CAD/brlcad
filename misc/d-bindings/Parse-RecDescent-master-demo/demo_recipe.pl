#! /usr/local/bin/perl -sw

# "Potato, Egg, Red meat & Lard Cookbook",
# T. Omnicient Rash & N. Hot Ignorant-Kant
# O'Besity & Associates

use Parse::RecDescent;

$grammar =
q{
	Recipe: Step(s)

	Step:
		Verb Object Clause(s?)
			{ print "$item[1]\n" }
	      | <resync:[ ]{2}>

	Verb:
		'boil'
	      | 'peel'
	      | 'mix'
	      | 'melt'
	      | 'fry'
	      | 'steam'
	      | 'marinate'
	      | 'sprinkle'
	      | 'is'
	      | 'are'
	      | 'has'

	Object:
		IngredientQualifier(s) Ingredient
	      | ReferenceQualifier(s) Ingredient
	      | Reference

	Clause:
	        SubordinateClause
              | CoordinateClause

	SubordinateClause:
		'until' State
	      | 'while' State
	      | 'for' Time

	CoordinateClause:
		/and( then)?/ Step
	      | /or/ Step

	State:
		Object Verb Adjective
	      | Adjective

	Time:
		Number TimeUnit

	TimeUnit:
		/hours?/
		/minutes?/
		/seconds?/

	QuantityUnit:
		/lbs?/


	Object:
		ReferenceQualifier Ingredient
	      | Reference

	Reference:
		'they'
	      | 'it'
	      | 'them'

	Ingredient:
		'potatoes'
	      | 'lard'
	      | 'olive oil'
	      | 'sugar'
	      | 'bacon fat'
	      | 'butter'
	      | 'salt'
	      | 'vinegar'

	IngredientQualifier:
		Amount
	      | Number
	      | 'a'
	      | 'some'
	      | 'large'
	      | 'small'

	Amount: Number QuantityUnit

	ReferenceQualifier:
	        'the'
	      | 'those'
	      | 'each'
	      | 'half the'

	Number:
		/[1-9][0-9]*/
	      | /one|two|three|four|five|six|seven|eight|nine/
	      | 'a dozen'

	Adjective:
		'soft'
	      | 'tender'
	      | 'done'
	      | 'charred'
	      | 'grey'

};

$parse = new Parse::RecDescent ($grammar);

$/ = "\n\n";
while (<DATA>)
{
	if($ingredients = $parse->Recipe(lc $_))
	{
		print "$ingredients\n$_";
	}
}

__DATA__
Boil six large potatoes until they are grey and then marinate them
for at least two hours in a mixture of lard, olive oil, raw
sugar, and sea-salt.  In a deep-fryer melt 2 lbs of bacon fat and
bring to the boil.  Fry the marinated potatoes for 7 minutes, or
until they are nicely charred.  Serve with lashings of butter.
Sprinkle with salt and vinegar to taste.
