include_directories(
  ${TCL_INCLUDE_DIRS}
)

BRLCAD_ADDEXEC(anim_cascade anim_cascade.c "libbn libbu")

BRLCAD_ADDEXEC(anim_fly anim_fly.c "libbu ${M_LIBRARY}")

BRLCAD_ADDEXEC(anim_hardtrack anim_hardtrack.c "libbn libbu ${M_LIBRARY}")

BRLCAD_ADDEXEC(anim_keyread anim_keyread.c "libbn libbu")

BRLCAD_ADDEXEC(anim_lookat anim_lookat.c "libbn libbu ${M_LIBRARY}")

BRLCAD_ADDEXEC(anim_offset anim_offset.c "libbn libbu")

BRLCAD_ADDEXEC(anim_orient anim_orient.c "libbn libbu")

BRLCAD_ADDEXEC(anim_script anim_script.c "libbn libbu")

BRLCAD_ADDEXEC(anim_sort anim_sort.c libbu)

BRLCAD_ADDEXEC(anim_time anim_time.c "libbu ${M_LIBRARY}")

BRLCAD_ADDEXEC(anim_track "anim_track.c cattrack.c" "libbn libbu ${M_LIBRARY}")

BRLCAD_ADDEXEC(anim_turn anim_turn.c "libbn libbu ${M_LIBRARY}")

BRLCAD_ADDEXEC(chan_add chan_add.c libbu)

BRLCAD_ADDEXEC(chan_mult chan_mult.c libbu)

BRLCAD_ADDEXEC(chan_permute chan_permute.c libbu)

set(anim_MANS
  anim_script.1 
  anim_sort.1 
  anim_time.1 
  anim_track.1 
  anim_turn.1 
  chan_mult.1 
  chan_permute.1
)
ADD_MAN_PAGES(1 anim_MANS)
CMAKEFILES(cattrack.h)
