

set(debug OFF)
if(CMAKE_BUILD_TYPE MATCHES "[Dd][Ee][Bb][Uu][Gg]")
  set(debug ON)
endif()

option(WARNINGS_AS_ERRORS "treat all warnings as errors. turn off for development, on for release" OFF)
option(WARN_DEPRECATED "show deprecation warnings" ${debug})

set(warning_flags -Wall -Wextra -Wno-unknown-pragmas -Wno-unused-function -Werror=vla)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  list(APPEND warning_flags -Wno-unknown-warning-option)
endif()
if(WARN_DEPRECATED)
  list(APPEND warning_flags -Wdeprecated-declarations)
else()
  list(APPEND warning_flags -Wno-deprecated-declarations)
endif()
