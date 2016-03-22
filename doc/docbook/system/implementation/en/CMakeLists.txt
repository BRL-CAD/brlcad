set(implementation_EN
  bool_eval_development.xml
  )

ADD_DOC(implementation_EN html/implementation/en)
ADD_DOCBOOK("HTML;PHP;PDF" implementation_EN implementation/en "")

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
