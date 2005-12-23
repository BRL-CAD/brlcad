#!../src/bltwish

package require BLT
# --------------------------------------------------------------------------
# Starting with Tcl 8.x, the BLT commands are stored in their own 
# namespace called "blt".  The idea is to prevent name clashes with
# Tcl commands and variables from other packages, such as a "table"
# command in two different packages.  
#
# You can access the BLT commands in a couple of ways.  You can prefix
# all the BLT commands with the namespace qualifier "blt::"
#  
#    blt::graph .g
#    blt::table . .g -resize both
# 
# or you can import all the command into the global namespace.
#
#    namespace import blt::*
#    graph .g
#    table . .g -resize both
#
# --------------------------------------------------------------------------
if { $tcl_version >= 8.0 } {
    namespace import blt::*
    namespace import -force blt::tile::*
}
source scripts/demo.tcl
#bltdebug 100

image create photo label1 -file ./images/mini-book1.gif
image create photo label2 -file ./images/mini-book2.gif
image create photo testImage -file ./images/txtrflag.gif

tabset .t \
    -textside right \
    -slant both \
    -side right \
    -samewidth yes \
    -highlightcolor yellow \
    -tiers 5 \
    -scrollcommand { .s set } \
    -scrollincrement 1 

label .t.l -image testImage

#option add *Tabset.Tab.font -*-helvetica-bold-r-*-*-12-*-*-*-*-*-*-*
#option add *Tabset.Tab.fill both

set attributes {
    graph1 "Graph \#1" pink	
    graph2 "Graph \#2" lightblue	
    graph3 "Graph \#3" orange
    graph5 "Graph \#5" yellow	
    barchart2 "Barchart \#2" green
}

foreach { name label color } $attributes {
    .t insert end $name -text $label \
	-selectbackground ${color}3  \
	-background ${color}3 \
	-activebackground ${color}2
}

.t insert end Image -selectbackground salmon2 -background salmon3 \
    -selectbackground salmon3 -activebackground salmon2 -window .t.l

set tabLabels { 
    Aarhus Aaron Ababa aback abaft abandon abandoned abandoning
    abandonment abandons abase abased abasement abasements abases
    abash abashed abashes abashing abasing abate abated abatement
    abatements abater abates abating Abba abbe abbey abbeys abbot
    abbots Abbott abbreviate abbreviated abbreviates abbreviating
    abbreviation abbreviations Abby abdomen abdomens abdominal
    abduct abducted abduction abductions abductor abductors abducts
    Abe abed Abel Abelian Abelson Aberdeen Abernathy aberrant
    aberration aberrations abet abets abetted abetter abetting
    abeyance abhor abhorred abhorrent abhorrer abhorring abhors
    abide abided abides abiding Abidjan Abigail Abilene abilities
    ability abject abjection abjections abjectly abjectness abjure
    abjured abjures abjuring ablate ablated ablates ablating
    ablation ablative ablaze able abler ablest ably Abner abnormal
    abnormalities abnormality abnormally Abo aboard abode abodes
    abolish abolished abolisher abolishers abolishes abolishing
    abolishment abolishments abolition abolitionist abolitionists
    abominable abominate aboriginal aborigine aborigines abort
    aborted aborting abortion abortions abortive abortively aborts
    Abos abound abounded abounding abounds about above aboveboard
    aboveground abovementioned abrade abraded abrades abrading
    Abraham Abram Abrams Abramson abrasion abrasions abrasive
    abreaction abreactions abreast abridge abridged abridges
    abridging abridgment abroad abrogate abrogated abrogates
    abrogating abrupt abruptly abruptness abscess abscessed
    abscesses abscissa abscissas abscond absconded absconding
    absconds absence absences absent absented absentee
    absenteeism absentees absentia absenting absently absentminded
    absents absinthe absolute absolutely absoluteness absolutes
    absolution absolve absolved absolves absolving absorb
    absorbed absorbency absorbent absorber absorbing absorbs
    absorption absorptions absorptive abstain abstained abstainer
    abstaining abstains abstention abstentions abstinence
    abstract abstracted abstracting abstraction abstractionism
    abstractionist abstractions abstractly abstractness
    abstractor abstractors abstracts abstruse abstruseness
    absurd absurdities absurdity absurdly Abu abundance abundant
    abundantly abuse abused abuses abusing abusive abut abutment
    abuts abutted abutter abutters abutting abysmal abysmally
    abyss abysses Abyssinia Abyssinian Abyssinians acacia
    academia academic academically academics academies academy
    Acadia Acapulco accede acceded accedes accelerate accelerated
    accelerates accelerating acceleration accelerations
    accelerator accelerators accelerometer accelerometers accent
    accented accenting accents accentual accentuate accentuated
    accentuates accentuating accentuation accept acceptability
    acceptable acceptably acceptance acceptances accepted
    accepter accepters accepting acceptor acceptors accepts
    access accessed accesses accessibility accessible accessibly
    accessing accession accessions accessories accessors
    accessory accident accidental accidentally accidently
    accidents acclaim acclaimed acclaiming acclaims acclamation
    acclimate acclimated acclimates acclimating acclimatization
    acclimatized accolade accolades accommodate accommodated
    accommodates accommodating accommodation accommodations
    accompanied accompanies accompaniment accompaniments
    accompanist accompanists accompany accompanying accomplice
    accomplices accomplish accomplished accomplisher accomplishers
    accomplishes accomplishing accomplishment accomplishments
    accord accordance accorded accorder accorders according
    accordingly accordion accordions accords accost accosted
    accosting accosts account accountability accountable accountably
    accountancy accountant accountants accounted accounting
    accounts Accra accredit accreditation accreditations
    accredited accretion accretions accrue accrued accrues
    accruing acculturate acculturated acculturates acculturating
    acculturation accumulate accumulated accumulates accumulating
    accumulation accumulations accumulator accumulators
    accuracies accuracy accurate accurately accurateness accursed
    accusal accusation accusations accusative accuse accused
    accuser accuses accusing accusingly accustom accustomed
    accustoming accustoms ace aces acetate acetone acetylene
    Achaean Achaeans ache ached aches achievable achieve achieved
    achievement achievements achiever achievers achieves achieving
    Achilles aching acid acidic acidities acidity acidly acids
    acidulous Ackerman Ackley acknowledge acknowledgeable
    acknowledged acknowledgement acknowledgements acknowledger
    acknowledgers acknowledges acknowledging acknowledgment
    acknowledgments acme acne acolyte acolytes acorn acorns
    acoustic acoustical acoustically acoustician acoustics
    acquaint acquaintance acquaintances acquainted acquainting
    acquaints acquiesce acquiesced acquiescence acquiescent
    acquiesces acquiescing acquirable acquire acquired acquires
    acquiring acquisition acquisitions
}

for { set i 0 } { $i < 500 } { incr i } {
    .t insert end [lindex $tabLabels $i] -state normal
}

scrollbar .s -command { .t view } -orient horizontal
radiobutton .left -text "Left" -variable side -value "left" \
    -command { .t configure -side $side -rotate 90 }
radiobutton .right -text "Right" -variable side -value "right" \
    -command { .t configure -side $side -rotate 270 }
radiobutton .top -text "Top" -variable side -value "top" \
    -command { .t configure -side $side -rotate 0 }
radiobutton .bottom -text "Bottom" -variable side -value "bottom" \
    -command { .t configure -side $side -rotate 0 }

table . \
    .t 0,0 -fill both -cspan 2 \
    .s 1,0 -fill x -cspan 2 \
    .top 2,0 -cspan 2 \
    .left 3,0 \
    .right 3,1 \
    .bottom 4,0 -cspan 2 

table configure . r1 r3 r4 r2 -resize none
focus .t

.t focus 0

after 3000 {
	.t move 0 after 3
	.t tab configure [.t get 3] -state disabled 
}

foreach file { graph1 graph2 graph3 graph5 barchart2 } {
    namespace eval $file {
	if { [string match graph* $file] } {
	    set graph [graph .t.$file]
	} else {
	    set graph [barchart .t.$file]
	}
	source scripts/$file.tcl
	.t tab configure $file -window $graph -fill both 
    }
}

.top invoke

