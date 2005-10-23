# $Id$

bin_PROGRAMS = \
	anim_cascade \
	anim_fly \
	anim_hardtrack \
	anim_keyread \
	anim_lookat \
	anim_offset \
	anim_orient \
	anim_script \
	anim_sort \
	anim_time \
	anim_track \
	anim_turn \
	chan_add \
	chan_mult \
	chan_permute

anim_cascade_SOURCES = anim_cascade.c
anim_cascade_LDADD = ${BN}

anim_fly_SOURCES = anim_fly.c
anim_fly_LDADD = ${BU}

anim_hardtrack_SOURCES = anim_hardtrack.c
anim_hardtrack_LDADD = ${BN}

anim_keyread_SOURCES = anim_keyread.c
anim_keyread_LDADD = ${BN}

anim_lookat_SOURCES = anim_lookat.c
anim_lookat_LDADD = ${BN}

anim_offset_SOURCES = anim_offset.c
anim_offset_LDADD = ${BN}

anim_orient_SOURCES = anim_orient.c
anim_orient_LDADD = ${BN}

anim_script_SOURCES = anim_script.c
anim_script_LDADD = ${BN}

anim_sort_SOURCES = anim_sort.c
anim_sort_LDADD = ${BU}

anim_time_SOURCES = anim_time.c
anim_time_LDADD = ${BU}

anim_track_SOURCES = anim_track.c cattrack.c
anim_track_LDADD = ${BN}

anim_turn_SOURCES = anim_turn.c
anim_turn_LDADD = ${BN}

chan_add_SOURCES = chan_add.c

chan_mult_SOURCES = chan_mult.c

chan_permute_SOURCES = chan_permute.c

man_MANS = \
	anim_cascade.1 \
	anim_fly.1 \
	anim_hardtrack.1 \
	anim_keyread.1 \
	anim_lookat.1 \
	anim_offset.1 \
	anim_orient.1 \
	anim_script.1 \
	anim_sort.1 \
	anim_time.1 \
	anim_track.1 \
	anim_turn.1 \
	chan_mult.1 \
	chan_permute.1

EXTRA_DIST = $(man_MANS)

include $(top_srcdir)/misc/Makefile.defs

FAST_OBJECTS = \
	$(anim_cascade_OBJECTS) \
	$(anim_fly_OBJECTS) \
	$(anim_harddrack_OBJECTS) \
	$(anim_keyread_OBJECTS) \
	$(anim_lookat_OBJECTS) \
	$(anim_offset_OBJECTS) \
	$(anim_orient_OBJECTS) \
	$(anim_script_OBJECTS) \
	$(anim_sort_OBJECTS) \
	$(anim_time_OBJECTS) \
	$(anim_track_OBJECTS) \
	$(anim_turn_OBJECTS) \
	$(chan_add_OBJECTS) \
	$(chan_mult_OBJECTS) \
	$(chan_permute_OBJECTS) \
	$(bin_PROGRAMS)
