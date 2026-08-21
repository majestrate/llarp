#include <llarp/dtls/messages.hpp>

#include <catch2/catch.hpp>
#include <algorithm>

namespace
{
  llarp::dtls::DialbackFrame
  RoundTrip(const llarp::dtls::DialbackFrame& input)
  {
    std::array<byte_t, 512> storage{};
    llarp_buffer_t writeBuf{storage};
    REQUIRE(input.BEncode(&writeBuf));

    const auto written = writeBuf.cur - writeBuf.base;
    std::vector<byte_t> encoded{storage.begin(), storage.begin() + written};

    llarp_buffer_t readBuf{encoded};
    llarp::dtls::DialbackFrame decoded;
    REQUIRE(decoded.BDecode(&readBuf));
    REQUIRE(readBuf.cur == readBuf.base + readBuf.sz);
    return decoded;
  }
}  // namespace

TEST_CASE("DTLS dialback challenge roundtrip")
{
  llarp::dtls::DialbackFrame frame;
  frame.action = llarp::dtls::DialbackAction::Challenge;
  frame.challenge.fill(0x42);
  frame.timestamp = 123456;
  frame.signature.Fill(0x24);

  const auto decoded = RoundTrip(frame);
  REQUIRE(decoded.action == llarp::dtls::DialbackAction::Challenge);
  REQUIRE(decoded.challenge == frame.challenge);
  REQUIRE(decoded.timestamp == frame.timestamp);
  REQUIRE(decoded.signature.as_array() == frame.signature.as_array());
  REQUIRE(decoded.relayMarker);
  REQUIRE(decoded.xMarker);
  REQUIRE(decoded.yMarker);
}

TEST_CASE("DTLS dialback reply roundtrip")
{
  llarp::dtls::DialbackFrame frame;
  frame.action = llarp::dtls::DialbackAction::Reply;
  frame.challenge.fill(0x09);
  frame.timestamp = 98765;
  frame.signature.Fill(0x99);

  const auto decoded = RoundTrip(frame);
  REQUIRE(decoded.action == llarp::dtls::DialbackAction::Reply);
  REQUIRE(decoded.challenge == frame.challenge);
  REQUIRE(decoded.timestamp == frame.timestamp);
  REQUIRE(decoded.signature.as_array() == frame.signature.as_array());
  REQUIRE_FALSE(decoded.relayMarker);
  REQUIRE_FALSE(decoded.xMarker);
  REQUIRE_FALSE(decoded.yMarker);
}

TEST_CASE("DTLS dialback failure roundtrip")
{
  llarp::dtls::DialbackFrame frame;
  frame.action = llarp::dtls::DialbackAction::Failure;
  frame.challenge.fill(0x7f);
  frame.timestamp = 22222;
  frame.signature.Fill(0x11);
  frame.error = "dialback failed";

  const auto decoded = RoundTrip(frame);
  REQUIRE(decoded.action == llarp::dtls::DialbackAction::Failure);
  REQUIRE(decoded.challenge == frame.challenge);
  REQUIRE(decoded.timestamp == frame.timestamp);
  REQUIRE(decoded.signature.as_array() == frame.signature.as_array());
  REQUIRE(decoded.error == frame.error);
}

TEST_CASE("DTLS dialback rejects invalid action")
{
  std::array<byte_t, 512> storage{};
  llarp::dtls::DialbackFrame frame;
  frame.action = llarp::dtls::DialbackAction::Reply;
  frame.challenge.fill(0x01);
  frame.timestamp = 1;
  frame.signature.Fill(0x01);

  llarp_buffer_t writeBuf{storage};
  REQUIRE(frame.BEncode(&writeBuf));

  const auto written = writeBuf.cur - writeBuf.base;
  std::vector<byte_t> encoded{storage.begin(), storage.begin() + written};

  auto itr = std::find(encoded.begin(), encoded.end(), static_cast<byte_t>('#'));
  REQUIRE(itr != encoded.end());
  *itr = static_cast<byte_t>('X');

  llarp_buffer_t readBuf{encoded};
  llarp::dtls::DialbackFrame decoded;
  REQUIRE_FALSE(decoded.BDecode(&readBuf));
}
