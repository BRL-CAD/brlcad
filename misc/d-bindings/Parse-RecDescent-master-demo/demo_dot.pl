#! /usr/bin/perl
use Parse::RecDescent;
my $DOTSRC="/var/src/gv1.5";

my $graph = "$DOTSRC/graphs";

# $::RD_HINT=1;
# $::RD_AUTOSTUB=1;
# $::RD_TRACE=1;

#  'strict'(?) pas accepté
$gram = <<'EOF';

graph:        comment(?) strict(?)  ( 'digraph' | 'graph' ) id  '{' stmt_list '}'
attr_stmt:    m/(graph|node|edge)\s+/ attrs(?)
subgraph:     ( 'subgraph' id  )(?)  '{' stmt_list '}' | 'subgraph' id
stmt_list:    ( stmt semi(?) )(s?)
stmt:         id '=' id | attr_stmt |  edge_stmt | subgraph | node_stmt | comment
node_stmt:    node_id  attrs(?)
node_id:      id ( ':' id )(?)
attrs:        '[' ( id '=' value comma(?) )(s) ']'
value:        id | CONSTANT
edge_stmt:    ( node_id | subgraph ) edgeRHS(s)  attrs(?)
edgeRHS:      edgeop ( node_id | subgraph )
edgeop:       m|-[>-]|
keyword:      m/(subgraph|graph|node|edge)\s+/
id:          ...!keyword /([\w\d][\w\d-]*)/ | STRING_LITERAL
STRING_LITERAL:	{ extract_delimited($text,'"') }
CONSTANT:	/[+-]?(?=\d|\.\d)\d*(\.\d*)?([Ee]([+-]?\d+))?/
strict:       'strict'
semi:         ';'
comma:        ','
comment	: m{\s*			# optional whitespace
	    //			# comment delimiter
	    [^\n]*		# anything except a newline
	    \n			# then a newline
	   }x

	| m{\s*			# optional whitespace
	    /\*			# comment opener
	    (?:[^*]+|\*(?!/))*	# anything except */
	    \*/		        # comment closer
            ([ \t]*)?           # trailing blanks or tabs
	   }x



EOF

$parser = new Parse::RecDescent($gram);

 $_='hashtable.dot';
 for ( <$graph/directed/*.dot>, <$graph/undirected/*.dot> ) {
  undef $/;
  open F, $_;
  $txt =   <F>;
  $ok = $parser->graph($txt);
  print $ok ? '': "not ", "OK $_\n";
}

__END__

here is the score so far:


OK /var/src/gv1.5/graphs/directed/KW91.dot
OK /var/src/gv1.5/graphs/directed/NaN.dot
OK /var/src/gv1.5/graphs/directed/abstract.dot
OK /var/src/gv1.5/graphs/directed/alf.dot
OK /var/src/gv1.5/graphs/directed/awilliams.dot
OK /var/src/gv1.5/graphs/directed/clust.dot
OK /var/src/gv1.5/graphs/directed/clust1.dot
OK /var/src/gv1.5/graphs/directed/clust2.dot
OK /var/src/gv1.5/graphs/directed/clust3.dot
OK /var/src/gv1.5/graphs/directed/clust4.dot
OK /var/src/gv1.5/graphs/directed/clust5.dot
OK /var/src/gv1.5/graphs/directed/crazy.dot
OK /var/src/gv1.5/graphs/directed/ctext.dot
OK /var/src/gv1.5/graphs/directed/dfa.dot
OK /var/src/gv1.5/graphs/directed/fig6.dot
OK /var/src/gv1.5/graphs/directed/fsm.dot
OK /var/src/gv1.5/graphs/directed/grammar.dot
not OK /var/src/gv1.5/graphs/directed/hashtable.dot
OK /var/src/gv1.5/graphs/directed/jcctree.dot
OK /var/src/gv1.5/graphs/directed/jsort.dot
OK /var/src/gv1.5/graphs/directed/ldbxtried.dot
OK /var/src/gv1.5/graphs/directed/mike.dot
OK /var/src/gv1.5/graphs/directed/newarrows.dot
OK /var/src/gv1.5/graphs/directed/nhg.dot
OK /var/src/gv1.5/graphs/directed/pgram.dot
not OK /var/src/gv1.5/graphs/directed/pm2way.dot
not OK /var/src/gv1.5/graphs/directed/pmpipe.dot
not OK /var/src/gv1.5/graphs/directed/polypoly.dot
not OK /var/src/gv1.5/graphs/directed/proc3d.dot
OK /var/src/gv1.5/graphs/directed/records.dot
OK /var/src/gv1.5/graphs/directed/rowe.dot
OK /var/src/gv1.5/graphs/directed/shells.dot
OK /var/src/gv1.5/graphs/directed/states.dot
OK /var/src/gv1.5/graphs/directed/structs.dot
OK /var/src/gv1.5/graphs/directed/train11.dot
OK /var/src/gv1.5/graphs/directed/trapeziumlr.dot
OK /var/src/gv1.5/graphs/directed/tree.dot
not OK /var/src/gv1.5/graphs/directed/triedds.dot
OK /var/src/gv1.5/graphs/directed/try.dot
OK /var/src/gv1.5/graphs/directed/unix.dot
OK /var/src/gv1.5/graphs/directed/unix2.dot
OK /var/src/gv1.5/graphs/directed/viewfile.dot
OK /var/src/gv1.5/graphs/directed/world.dot
not OK /var/src/gv1.5/graphs/undirected/ER.dot
OK /var/src/gv1.5/graphs/undirected/ngk10_4.dot
OK /var/src/gv1.5/graphs/undirected/process.dot

