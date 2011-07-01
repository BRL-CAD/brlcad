SET(ap203_data
  ap203/203wseds.exp
  ap203/cube1.p21
  ap203/cube2.p21
  ap203/g_r_assembly1.p21
  ap203/gasket1.p21
  ap203/gasket2.p21
  ap203/gasket3.p21
  ap203/hex_prism1.p21
  ap203/rod_aspect1.p21
  ap203/star1.p21
)
INSTALL(FILES ${ap203_data} DESTINATION ${DATA_DIR}/data/ap203)

SET(ap227_data
  ap227/ap227.exp
  ap227/mitre.p21
  ap227/mitre.step.txt
)
INSTALL(FILES ${ap227_data} DESTINATION ${DATA_DIR}/data/ap227)

SET(step_example_data
  example/example.exp
)
INSTALL(FILES ${step_example_data} DESTINATION ${DATA_DIR}/data/example)



SET(step_datafiles
  pdmnet.exp
  select.exp 
)
INSTALL(FILES ${step_datafiles} DESTINATION ${DATA_DIR}/data)
