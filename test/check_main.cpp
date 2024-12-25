#define CATCH_CONFIG_RUNNER
#include <catch2/catch.hpp>

#include <llarp/util/logging.hpp>
#include <llarp/util/service_manager.hpp>

int
main(int argc, char* argv[])
{
  llarp::sys::service_manager->disable();
  llarp::log::reset_level(llarp::log::Level::off);
  int result = Catch::Session().run(argc, argv);
  return result;
}
