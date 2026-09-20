# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Debug")
  file(REMOVE_RECURSE
  "CMakeFiles\\invento_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\invento_autogen.dir\\ParseCache.txt"
  "CMakeFiles\\pos_core_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\pos_core_autogen.dir\\ParseCache.txt"
  "CMakeFiles\\pos_core_tests_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\pos_core_tests_autogen.dir\\ParseCache.txt"
  "CMakeFiles\\pos_sqlite_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\pos_sqlite_autogen.dir\\ParseCache.txt"
  "CMakeFiles\\pos_ui_smoke_tests_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\pos_ui_smoke_tests_autogen.dir\\ParseCache.txt"
  "invento_autogen"
  "pos_core_autogen"
  "pos_core_tests_autogen"
  "pos_sqlite_autogen"
  "pos_ui_smoke_tests_autogen"
  )
endif()
