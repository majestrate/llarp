if(STATIC_LINK)
  if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    link_libraries( -static-libstdc++ )
  else()
    link_libraries( -static-libstdc++ -static-libgcc )
  endif()

  # For a fully static executable, add -static to the final link
  string(APPEND CMAKE_EXE_LINKER_FLAGS " -static")
endif()
