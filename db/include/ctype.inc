<?php
/* character functions */
function isalnum ($c){ global $ctype__; return ((($ctype__[( ord($c) )]&(01 | 02 | 04 )) != 0)?1:0);}
function isalpha ($c){ global $ctype__; return ((($ctype__[( ord($c) )]&(01 | 02 )) != 0)?1:0);}
function isascii ($c){ global $ctype__; return (((( ord($c) )<=0177) != 0)?1:0);}
function iscntrl ($c){ global $ctype__; return ((($ctype__[( ord($c) )]& 040 ) != 0)?1:0);}
function isdigit ($c){ global $ctype__; return ((($ctype__[( ord($c) )]& 04 ) != 0)?1:0);}
function isgraph ($c){ global $ctype__; return ((($ctype__[( ord($c) )]&(020 | 01 | 02 | 04 )) != 0)?1:0);}
function islower ($c){ global $ctype__; return ((($ctype__[( ord($c) )]& 02 ) != 0)?1:0);}
function isprint ($c){ global $ctype__; return ((($ctype__[( ord($c) )]&(020 | 01 | 02 | 04 | 0200 )) != 0)?1:0);}
function ispunct ($c){ global $ctype__; return ((($ctype__[( ord($c) )]& 020 ) != 0)?1:0);}
function isspace ($c){ global $ctype__; return ((($ctype__[( ord($c) )]& 010 ) != 0)?1:0);}
function isupper ($c){ global $ctype__; return ((($ctype__[( ord($c) )]& 01 ) != 0)?1:0);}
function isxdigit ($c){ global $ctype__; return ((($ctype__[( ord($c) )]&(0100 | 04 )) != 0)?1:0);}
$ctype__ = array(32,32,32,32,32,32,32,32,32,40,40,40,40,40,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,
		 -120,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,4,4,4,4,4,4,4,4,4,4,16,16,16,16,16,16,
		 16,65,65,65,65,65,65,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,16,16,16,16,16,
		 16,66,66,66,66,66,66,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,16,16,16,16,32,
		 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0);

# Local Variables: ***
# mode: C ***
# tab-width: 8 ***
# c-basic-offset: 2 ***
# indent-tabs-mode: t ***
# End: ***
# ex: shiftwidth=2 tabstop=8
?>
