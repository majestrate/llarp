if(BUILD_GUI)
  message(STATUS "Building lokinet-gui from source")

  set(default_gui_target pack)

  set(GUI_YARN_TARGET "${default_gui_target}" CACHE STRING "yarn target for building the GUI")
  set(GUI_YARN_EXTRA_OPTS "" CACHE STRING "extra options to pass into the yarn build command")

  # allow manually specifying yarn with -DYARN=
  if(NOT YARN)
    find_program(YARN NAMES yarnpkg yarn REQUIRED)
  endif()
  message(STATUS "Building lokinet-gui with yarn ${YARN}, target ${GUI_YARN_TARGET}")

  add_custom_target(lokinet-gui
    COMMAND ${YARN} install --frozen-lockfile &&
    ${YARN} ${GUI_YARN_EXTRA_OPTS} ${GUI_YARN_TARGET}
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}/gui")

  message(FATAL_ERROR "Building/bundling the GUI from this repository is not supported on this platform")
else()
  message(STATUS "not building gui")
endif()

if(NOT TARGET assemble_gui)
  add_custom_target(assemble_gui COMMAND "true")
endif()
