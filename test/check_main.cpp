#define CATCH_CONFIG_RUNNER
#include <catch2/catch.hpp>

#include <llarp/util/logging.hpp>
#include <llarp/util/service_manager.hpp>

int
main(int argc, char* argv[])
{
  llarp::sys::service_manager->disable();
  llarp::log::set_log_level("off");
  int result = Catch::Session().run(argc, argv);
  return result;
}
