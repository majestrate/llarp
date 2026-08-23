#pragma once

#include <llarp/crypto/types.hpp>
#include <llarp/util/aligned.hpp>
#include <llarp/util/bencode.hpp>

#include <optional>
#include <string>

namespace llarp::dtls
{
  enum class DialbackAction : char
  {
    Challenge = '@',
    Reply = '#',
    Failure = '!'
  };

  struct DialbackFrame
  {
    static constexpr size_t ChallengeSize = 32;

    DialbackAction action = DialbackAction::Challenge;
    AlignedBuffer<ChallengeSize> challenge{};
    uint64_t timestamp = 0;
    Signature signature{};
    std::string error;

    // challenge-only markers from issue spec
    bool relayMarker = true;
    bool xMarker = true;
    bool yMarker = true;

    bool
    BEncode(llarp_buffer_t* buf) const;

    bool
    BDecode(llarp_buffer_t* buf);

    bool
    DecodeKey(const llarp_buffer_t& key, llarp_buffer_t* buf);

   private:
    bool m_DecodedAction = false;
    bool m_DecodedChallenge = false;
    bool m_DecodedTimestamp = false;
    bool m_DecodedSignature = false;
    bool m_DecodedError = false;
    bool m_DecodedRelayMarker = false;
    bool m_DecodedXMarker = false;
    bool m_DecodedYMarker = false;

    bool
    DecodeAction(llarp_buffer_t* buf);

    bool
    DecodeChallenge(llarp_buffer_t* buf);

    bool
    DecodeMarker(llarp_buffer_t* buf, bool& marker, bool& decoded);

    bool
    DecodeError(llarp_buffer_t* buf);

    bool
    Validate() const;
  };

  std::optional<DialbackAction>
  DialbackActionFromByte(byte_t action);
}  // namespace llarp::dtls
