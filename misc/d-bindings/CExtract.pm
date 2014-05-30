package CExtract;

use strict;
use warnings;

use ParsePPCHeader;

my $_WARNED = 0;

our @keys
  = (
     'typedef',
     'struct',
     'enum',
     'union',
     'extern',
     'void',
     '__extension__',
     '__attribute__((deprecated))',
     'int',
     'long',
    );
our %key;
@key{@keys} = ();

our @keys2
  = (
     'BU_RB_WALK_ORDER',
     'ClientData',
     'FILE',
     'TclPlatStubs',
     'TclStubs',
     'Tcl_AsyncHandler',
     'Tcl_Channel',
     'Tcl_ChannelType',
     'Tcl_ChannelTypeVersion',
     'Tcl_Command',
     'Tcl_DecrRefCount',
     'Tcl_DriverBlockModeProc',
     'Tcl_DriverClose2Proc',
     'Tcl_DriverCloseProc',
     'Tcl_DriverFlushProc',
     'Tcl_DriverGetHandleProc',
     'Tcl_DriverGetOptionProc',
     'Tcl_DriverHandlerProc',
     'Tcl_DriverInputProc',
     'Tcl_DriverOutputProc',
     'Tcl_DriverSeekProc',
     'Tcl_DriverSetOptionProc',
     'Tcl_DriverThreadActionProc',
     'Tcl_DriverTruncateProc',
     'Tcl_DriverWatchProc',
     'Tcl_DriverWideSeekProc',
     'Tcl_Encoding',
     'Tcl_Event',
     'Tcl_ExitProc',
     'Tcl_Filesystem',
     'Tcl_HashEntry',
     'Tcl_HashKeyType',
     'Tcl_HashTable',
     'Tcl_IncrRefCount',
     'Tcl_Interp',
     'Tcl_InterpState',
     'Tcl_IsShared',
     'Tcl_Mutex',
     'Tcl_Namespace',
     'Tcl_Obj',
     'Tcl_ObjType',
     'Tcl_PathType',
     'Tcl_PathType',
     'Tcl_Pid',
     'Tcl_RegExp',
     'Tcl_StatBuf',
     'Tcl_ThreadId',
     'Tcl_TimerToken',
     'Tcl_Trace',
     'Tcl_UniChar',
     'Tcl_WideInt',
     '_G_fpos_t',
     '_IO_FILE',
     '_IO_FILE_plus;',
     '_IO_jump_t;',
     '_IO_marker',
     '__blkcnt_t',
     '__blksize_t',
     '__builtin_va_list',
     '__caddr_t',
     '__clock_t',
     '__clockid_t',
     '__codecvt_result',
     '__const',
     '__daddr_t',
     '__dev_t',
     '__fd_mask',
     '__fsblkcnt_t',
     '__fsfilcnt_t',
     '__fsid_t',
     '__gid_t',
     '__gnuc_va_list',
     '__id_t',
     '__ino_t',
     '__jmp_buf_tag',
     '__key_t',
     '__loff_t',
     '__mode_t',
     '__nlink_t',
     '__off64_t',
     '__off_t',
     '__pid_t',
     '__quad_t',
     '__sigset_t',
     '__ssize_t',
     '__suseconds_t',
     '__time_t',
     '__timer_t',
     '__u_char',
     '__u_int',
     '__u_long',
     '__u_quad_t',
     '__u_short',
     '__uid_t',
     'bu_attribute_value_pair',
     'bu_attribute_value_set',
     'bu_bitv',
     'bu_color',
     'bu_endian_t',
     'bu_external',
     'bu_hash_entry',
     'bu_hash_record',
     'bu_hash_tbl',
     'bu_heap_func_t',
     'bu_hist',
     'bu_hook_list',
     'bu_lex_key',
     'bu_lex_t_dbl',
     'bu_lex_t_id',
     'bu_lex_t_int',
     'bu_lex_t_key',
     'bu_lex_token',
     'bu_list',
     'bu_mapped_file',
     'bu_observer',
     'bu_ptbl',
     'bu_rb_list',
     'bu_rb_node',
     'bu_rb_package',
     'bu_rb_tree',
     'bu_structparse',
     'bu_utctime(struct',
     'bu_vlb',
     'bu_vls',
     'char',
     'const',
     'div_t',
     'double',
     'drand48_data',
     'enum',
     'extern',
     'float',
     'genptr_t',
     'int',
     'int64_t',
     'jmp_buf',
     'ldiv_t',
     'long',
     'off_t',
     'random_data',
     'short',
     'signed',
     'size_t',
     'struct',
     'timespec',
     'timeval',
     'typedef',
     'union',
     'unsigned',
     'utimbuf;',
     'void',
     'volatile',
     'wait',
    );
our %key2;
@key2{@keys2} = ();

sub extract_object {

  my $lines_aref = shift @_; # \@lines
  my $i          = shift @_; # $i - current @lines index
  my $fp         = shift @_; # ouput file pointer

  my $nl = scalar @{$lines_aref};

  # we're at the first line of the unknown object
  # get all lines til the last
  my @olines = ();

  # track '{}', '()', '[]' levels
  my $clevel = 0; # curly braces
  my $plevel = 0; # parentheses
  my $slevel = 0; # square brackets

  my $first_index = $i;
  my $last_index;

  my $nactual_lines = 0;

 LINE:
  for (; $i < $nl; ++$i) {
    my $lnum = $i + 1;
    my $line = $lines_aref->[$i];
    chomp $line;

    # skip blank lines
    my @d = split(' ', $line);
    next LINE if !defined $d[0];

    ++$nactual_lines;

    my $len = length $line;
    for (my $j = 0; $j < $len; ++$j) {
      my $c = substr $line, $j, 1;
      if ($c eq '{') {
	++$clevel;
      }
      elsif ($c eq '}') {
	--$clevel;
      }

      elsif ($c eq '(') {
	++$plevel;
      }
      elsif ($c eq ')') {
	--$plevel;
      }

      elsif ($c eq '[') {
	++$slevel;
      }
      elsif ($c eq ']') {
	--$slevel;
      }

      elsif ($c eq ';'
	     && $clevel == 0
	     && $plevel == 0
	     && $slevel == 0
	    ) {
	push @olines, $line;
	$last_index = $i;
	last LINE;
      }
    }
    push @olines, $line;
  }

  if (1) {
    # try Parse::RecDescent instead of my kludges below
    ParseCChunk::parse_chunk(\@olines);
    return $last_index;
  }
  elsif (!$_WARNED) {
    printf "WARNING: Parse::RecDescent not being used (file: '%s', line: %d).\n",
      __FILE__, __LINE__;
    $_WARNED = 1;
  }

  my $norig_lines = $last_index - $first_index + 1;

  # do some rudimentary analysis
  # make lines one string
  my $s = join ' ', @olines;
  # add spaces around some items
  my @ch = ('{', '(', '[', ']', ')', '}', ';', '*');
  my %ch;
  @ch{@ch} = ();

  my $slen = length $s;

  my $ss = '';
  for (my $i = 0; $i < $slen; ++$i) {
    my $c = substr $s, $i, 1;
    if (!exists $ch{$c}) {
      $ss .= $c;
      next;
    }

    $ss .= ' ';
    $ss .= $c;
    $ss .= ' ';
  }

  # now close up '*'
  while ($ss =~ s{\*[\s]+\*}{\*\*}) {
    ; # noopt
  }

  # get new length
  $s = $ss;
  $slen = length $s;


  # track '{}', '()', '[]' first and last positions
  my $cfirst   = -1; # curly braces
  my $pfirst   = -1; # parentheses
  my $sfirst   = -1; # square brackets
  my $clast    = -1; # curly braces
  my $plast    = -1; # parentheses
  my $slast    = -1; # square brackets
  my $semilast = -1; # semicolon

  for (my $i = 0; $i < $slen; ++$i) {
    my $c = substr $s, $i, 1;
    if ($c eq '{') {
      $cfirst = _update_index($cfirst, $i);
    }
    elsif ($c eq '}') {
      $clast = _update_index($clast, $i);
    }

    elsif ($c eq '(') {
      $pfirst = _update_index($pfirst, $i);
    }
    elsif ($c eq ')') {
      $plast = _update_index($plast, $i);
    }

    elsif ($c eq '[') {
      $sfirst = _update_index($sfirst, $i);
    }
    elsif ($c eq ']') {
      $slast = _update_index($slast, $i);
    }

    elsif ($c eq ';') {
      $semilast = _update_index($semilast, $i);
    }
  }

  my $typ = 'unknown';
  my $enum   = index $s, 'enum';
  my $struct = index $s, 'struct';
  my $union  = index $s, 'union';
  my $typdef = index $s, 'typedef';

  my @d = split(' ', $s);
  my $ftok = shift @d;
  if ($ftok eq 'extern') {
    $typ = 'var decl';
  }
  elsif ($ftok eq 'struct') {
    $typ = 'struct';
  }
  elsif ($ftok eq 'union') {
    $typ = 'union';
  }
  elsif ($cfirst >= 0
      && ($pfirst < 0 || $cfirst < $pfirst)
      && ($sfirst < 0 || $cfirst < $sfirst)) {
    if ($enum >=0 && $enum < $cfirst) {
      $typ = 'enum';
    }
    elsif ($struct >=0 && $struct < $cfirst) {
      $typ = 'struct';
    }
    elsif ($union >=0 && $union < $cfirst) {
      $typ = 'union';
    }
  }
  elsif ($pfirst >= 0
      && ($cfirst < 0 || $pfirst < $cfirst)
      && ($sfirst < 0 || $pfirst < $sfirst)) {
    $typ = 'func decl';
  }

  my $typd = '';
  $typd = 'typedef' if ($s =~/typedef/);
  my $t = '';
  if ($typd) {
    $t = $typd;
  }

  if ($t && $typ ne 'unknown') {
    $t .= " $typ";
  }
  elsif (!$t) {
    $t = $typ;
  }

  # print good lines to output file
  print  $fp "\n";
  printf $fp "//=== starting extracted code at input line %d:\n", $first_index + 1;
  printf $fp "//  object type '$t'\n";
  #printf $fp "// N original lines: $norig_lines\n";
  #printf $fp "// N actual lines:   $nactual_lines\n";
  print  $fp "$_\n" for @olines;
  printf $fp "//=== ending extracted code at input line %d:\n", $last_index + 1;
  print  $fp "\n";

  return $last_index;

} # extract_object

sub _update_index {
  my $max = shift @_;
  my $i   = shift @_;

  return $i
    if ($i > $max);

  return $max;

} # _update_index

# mandatory true return for a Perl module
1;
