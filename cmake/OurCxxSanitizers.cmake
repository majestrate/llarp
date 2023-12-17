


option(XSAN "use sanitiser, if your system has it (requires -DCMAKE_BUILD_TYPE=Debug)" OFF)

if(XSAN)
  string(APPEND CMAKE_CXX_FLAGS_DEBUG " -fsanitize=${XSAN} -fno-omit-frame-pointer -fno-sanitize-recover")
  foreach(type EXE MODULE SHARED STATIC)
    string(APPEND CMAKE_${type}_LINKER_FLAGS_DEBUG " -fsanitize=${XSAN} -fno-omit-frame-pointer -fno-sanitize-recover")
  endforeach()
  message(STATUS "Doing a ${XSAN} sanitizer build")
endif()
