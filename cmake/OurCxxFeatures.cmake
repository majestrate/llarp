
# check for std::optional because macos is broke af sometimes

set(std_optional_code [[
#include <optional>

int main() {
   std::optional<int> maybe;
   maybe = 1;
   return *maybe == 1;
}
]])

check_cxx_source_compiles("${std_optional_code}" was_compiled)
if(was_compiled)
  message(STATUS "we have std::optional")
else()
  message(FATAL_ERROR "we dont have std::optional your compiler is broke af")
endif()


# bunch of hax for std::filesystem

set(filesystem_code [[
#include <filesystem>

int main() {
    auto cwd = std::filesystem::current_path();
    return !cwd.string().empty();
}
]])

check_cxx_source_compiles("${filesystem_code}" filesystem_compiled)
if(NOT filesystem_compiled)
  message(STATUS "trying extra link flags for std::filesystem")
  foreach(fslib stdc++fs c++fs)
    set(CMAKE_REQUIRED_LIBRARIES -l${fslib})
    check_cxx_source_compiles("${filesystem_code}" filesystem_compiled_${fslib})
    if (filesystem_compiled_${fslib})
      message(STATUS "Using -l${fslib} for std::filesystem")
      target_link_libraries(filesystem INTERFACE ${fslib})
      set(filesystem_is_good 1)
      break()
    endif()
  endforeach()
endif()

unset(CMAKE_REQUIRED_LIBRARIES)
if(filesystem_is_good EQUAL 1)
  message(STATUS "we have a sensible std::filesystem")
else()
  message(FATAL_ERROR "cannot find a sensible std::filesystem")
endif()
