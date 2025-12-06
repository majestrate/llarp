#include <catch2/catch.hpp>
#include <llarp/util/logging.hpp>
#include <llarp/config/config.hpp>

using TestString = std::string;

struct TestParseLog
{
  TestString input;
  std::optional<llarp::log::Level> level;
};

std::vector<TestParseLog> testParseLog{// bad cases
                                       {"bogus", {}},
                                       {"BOGUS", {}},
                                       {"", {}},
                                       {" ", {}},
                                       {"infogarbage", {}},
                                       {"notcritical", {}},
                                       // good cases
                                       {"info", llarp::log::Level::lvl_info},
                                       {"infO", llarp::log::Level::lvl_info},
                                       {"iNfO", llarp::log::Level::lvl_info},
                                       {"InfO", llarp::log::Level::lvl_info},
                                       {"INFO", llarp::log::Level::lvl_info},
                                       {"trace", llarp::log::Level::lvl_trace},
                                       {"debug", llarp::log::Level::lvl_debug},
                                       {"warn", llarp::log::Level::lvl_warning},
                                       {"warning", llarp::log::Level::lvl_warning},
                                       {"error", llarp::log::Level::lvl_error},
                                       {"err", llarp::log::Level::lvl_error},
                                       {"off", llarp::log::Level::off},
                                       {"false", llarp::log::Level::off},
                                       {"none", llarp::log::Level::off}};

TEST_CASE("parseLevel")
{
  const auto& [input, expected] = GENERATE(from_range(testParseLog));

  if (not expected)
    REQUIRE_THROWS_AS(llarp::log::level_from_string(input), std::invalid_argument);
  else
  {
    llarp::log::Level level;
    REQUIRE_NOTHROW(level = llarp::log::level_from_string(input));
    CHECK(level == *expected);
  }
}

TEST_CASE("TestLogLevelToString")
{
  CHECK("trace" == llarp::log::to_string(llarp::log::Level::lvl_trace));
  CHECK("debug" == llarp::log::to_string(llarp::log::Level::lvl_debug));
  CHECK("info" == llarp::log::to_string(llarp::log::Level::lvl_info));
  CHECK("warning" == llarp::log::to_string(llarp::log::Level::lvl_warning));
  CHECK("error" == llarp::log::to_string(llarp::log::Level::lvl_error));
  CHECK("off" == llarp::log::to_string(llarp::log::Level::off));
}
