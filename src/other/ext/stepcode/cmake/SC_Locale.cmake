# Save the current LC_ALL, LC_MESSAGES, and LANG environment variables and set them
# to "C" so things like date output are as expected
set(_orig_lc_all      $ENV{LC_ALL})
set(_orig_lc_messages $ENV{LC_MESSAGES})
set(_orig_lang        $ENV{LANG})
if(_orig_lc_all)
  set(ENV{LC_ALL}      C)
endif(_orig_lc_all)
if(_orig_lc_messages)
  set(ENV{LC_MESSAGES} C)
endif(_orig_lc_messages)
if(_orig_lang)
  set(ENV{LANG}        C)
endif(_orig_lang)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

