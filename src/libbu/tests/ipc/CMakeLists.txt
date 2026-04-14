if(BUILD_TESTING)

  # Worker process spawned by the orchestrator
  brlcad_addexec(ipc_worker ipc_worker.c libbu TEST)
  set_target_properties(ipc_worker PROPERTIES FOLDER "BRL-CAD Executables/Test Programs/ipc")

  # Orchestrator
  brlcad_addexec(bu_ipc_test run.c libbu TEST)
  set_target_properties(bu_ipc_test PROPERTIES FOLDER "BRL-CAD Executables/Test Programs/ipc")

  # bu_ipc_test must be built after ipc_worker (it needs the path at run time)
  add_dependencies(bu_ipc_test ipc_worker)

  # Register with CTest.  The worker path is passed as a generator expression
  # so it resolves to the correct binary for the current build configuration.
  brlcad_add_test(NAME bu_ipc_test COMMAND bu_ipc_test "$<TARGET_FILE:ipc_worker>")

endif(BUILD_TESTING)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
