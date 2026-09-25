#include <llarp/link/factory.hpp>

#include <catch2/catch.hpp>

TEST_CASE("LinkFactory maps dtls dialect")
{
  REQUIRE(llarp::LinkFactory::TypeFromName("dtls") == llarp::LinkFactory::LinkType::eLinkDTLS);
  REQUIRE(llarp::LinkFactory::NameFromType(llarp::LinkFactory::LinkType::eLinkDTLS) == "dtls");
}
