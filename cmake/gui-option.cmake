set(default_build_gui OFF)

option(BUILD_GUI "build electron gui from 'gui' submodule source" ${default_build_gui})

if(BUILD_GUI AND GUI_EXE)
  message(FATAL_ERROR "-DGUI_EXE=... and -DBUILD_GUI=ON are mutually exclusive")
endif()
