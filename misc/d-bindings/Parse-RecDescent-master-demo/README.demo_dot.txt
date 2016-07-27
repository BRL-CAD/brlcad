From root@bib.adnintern.org  Thu Oct 21 05:31:59 1999
Received: from gimli.cs.monash.edu.au (gimli.cs.monash.edu.au [130.194.64.60])
	by indy05.cs.monash.edu.au (8.8.8/8.8.8) with ESMTP id FAA07658
	for <damian@indy05.cs.monash.edu.au>; Thu, 21 Oct 1999 05:31:59 +1000 (EST)
Received: from bib.adnintern.org ([194.242.172.1])
	by gimli.cs.monash.edu.au (8.8.8/8.8.8) with ESMTP id FAA08901
	for <damian@cs.monash.edu.au>; Thu, 21 Oct 1999 05:31:54 +1000
Received: (from root@localhost)
	by bib.adnintern.org (8.9.3/8.9.3) id VAA00889;
	Wed, 20 Oct 1999 21:29:05 +0200
Date: Wed, 20 Oct 1999 21:29:05 +0200
Message-Id: <199910201929.VAA00889@bib.adnintern.org>
From: "Stéphane Payrard -- stef@adnaccess.com (06 60 95 82 69)" <stef@adnaccess.com>
To: damian@cs.monash.edu.au
Subject: parsing dot file
Reply-to: stef@adnaccess.com
Status: RO


you may be interested at this rough cut at the dot grammar to enrichen
your collection.  dot is a language that describe graphs. I have
problem with embedded \n in strings that I have not investigated yet

http:/pub/web/www.research.att.com/sw/tools/graphviz

__
 stef




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

