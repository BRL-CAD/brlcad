
###########################################################################
# Some handy utilities used by the rest of the hv3 app. The public
# interface to this file consists of the commands:
#
#      swproc
#

# swproc --
# 
#         swproc NAME ARGS BODY
#
#     [swproc] is very similar to the proc command, except any procedure
#     arguments with default values must be specified with switches instead
#     of on the command line. For example, the following are equivalent:
#
#         proc   abc {a {b hello} {c goodbye}} {...}
#         abc one two
#
#         swproc swabc {a {b hello} {c goodbye}} {...}
#         swabc one -b two
#
#     This means, in the above example, that it is possible to call [swabc]
#     and supply a value for parameter "c" but not "b". This is not
#     possible with commands created by regular Tcl [proc].
#
#     Commands created with [swproc] may also accept switches that do not
#     take arguments. These should be specified as a list of three elements.
#     The first is the name of the switch (and variable). The second is the
#     default value of the variable (if the switch is not present), and the
#     third is the value if the switch is present. For example, the
#     following two blocks are equivalent:
#
#         proc abc {a} {...}
#         abc b
#         abc c
#
#         swproc abc {{a b c}} {...}
#         abc
#         abc -a
#
proc swproc {procname arguments script} {
  uplevel [subst {
    proc $procname {args} {
      ::tkhtml::swproc_rt [list $arguments] \$args
      $script
    }
  }]
}
