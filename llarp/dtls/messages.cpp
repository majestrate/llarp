#include <llarp/util/alloc.h>
#include "messages.hpp"

#include <llarp/util/logging.hpp>

namespace llarp::dtls
{
  static auto logcat = log::Cat("dtls-msg");

  std::optional<DialbackAction>
  DialbackActionFromByte(byte_t action)
  {
    switch (action)
    {
      case static_cast<byte_t>(DialbackAction::Challenge):
        return DialbackAction::Challenge;
      case static_cast<byte_t>(DialbackAction::Reply):
        return DialbackAction::Reply;
      case static_cast<byte_t>(DialbackAction::Failure):
        return DialbackAction::Failure;
      default:
        return std::nullopt;
    }
  }

  bool
  DialbackFrame::DecodeAction(llarp_buffer_t* buf)
  {
    llarp_buffer_t strbuf;
    if (!bencode_read_string(buf, &strbuf) || strbuf.sz != 1)
      return false;
    const auto maybeAction = DialbackActionFromByte(*strbuf.cur);
    if (!maybeAction)
      return false;
    action = *maybeAction;
    m_DecodedAction = true;
    return true;
  }

  bool
  DialbackFrame::DecodeChallenge(llarp_buffer_t* buf)
  {
    llarp_buffer_t strbuf;
    if (!bencode_read_string(buf, &strbuf) || strbuf.sz != challenge.size())
      return false;
    std::copy_n(strbuf.base, challenge.size(), challenge.data());
    m_DecodedChallenge = true;
    return true;
  }

  bool
  DialbackFrame::DecodeMarker(llarp_buffer_t* buf, bool& marker)
  {
    llarp_buffer_t strbuf;
    if (!bencode_read_string(buf, &strbuf))
      return false;
    if (strbuf.sz != 0)
      return false;
    marker = true;
    return true;
  }

  bool
  DialbackFrame::DecodeError(llarp_buffer_t* buf)
  {
    llarp_buffer_t strbuf;
    if (!bencode_read_string(buf, &strbuf))
      return false;
    error.assign(reinterpret_cast<char*>(strbuf.base), strbuf.sz);
    m_DecodedError = true;
    return true;
  }

  bool
  DialbackFrame::DecodeKey(const llarp_buffer_t& key, llarp_buffer_t* buf)
  {
    if (key.startswith("a"))
      return DecodeAction(buf);
    if (key.startswith("c"))
      return DecodeChallenge(buf);
    if (key.startswith("e"))
      return DecodeError(buf);
    if (key.startswith("r"))
      return DecodeMarker(buf, relayMarker);
    if (key.startswith("t"))
    {
      m_DecodedTimestamp = bencode_read_integer(buf, &timestamp);
      return m_DecodedTimestamp;
    }
    if (key.startswith("x"))
      return DecodeMarker(buf, xMarker);
    if (key.startswith("y"))
      return DecodeMarker(buf, yMarker);
    if (key.startswith("z"))
    {
      m_DecodedSignature = signature.BDecode(buf);
      return m_DecodedSignature;
    }
    log::warning(logcat, "invalid dialback key: {}", static_cast<char>(*key.cur));
    return false;
  }

  bool
  DialbackFrame::Validate() const
  {
    if (!(m_DecodedAction and m_DecodedChallenge and m_DecodedTimestamp and m_DecodedSignature))
      return false;

    switch (action)
    {
      case DialbackAction::Challenge:
        return relayMarker and xMarker and yMarker and !m_DecodedError;
      case DialbackAction::Reply:
        return !relayMarker and !xMarker and !yMarker and !m_DecodedError;
      case DialbackAction::Failure:
        return !relayMarker and !xMarker and !yMarker and m_DecodedError;
    }

    return false;
  }

  bool
  DialbackFrame::BEncode(llarp_buffer_t* buf) const
  {
    if (!bencode_start_dict(buf))
      return false;

    if (!BEncodeWriteDictString("a", std::string_view{reinterpret_cast<const char*>(&action), 1}, buf))
      return false;

    if (!BEncodeWriteDictEntry("c", challenge, buf))
      return false;

    if (action == DialbackAction::Failure)
    {
      if (!BEncodeWriteDictString("e", error, buf))
        return false;
    }

    if (!BEncodeWriteDictInt("t", timestamp, buf))
      return false;

    if (action == DialbackAction::Challenge)
    {
      static constexpr std::string_view empty{};
      if (!BEncodeWriteDictString("r", empty, buf))
        return false;
      if (!BEncodeWriteDictString("x", empty, buf))
        return false;
      if (!BEncodeWriteDictString("y", empty, buf))
        return false;
    }

    if (!BEncodeWriteDictEntry("z", signature, buf))
      return false;

    return bencode_end(buf);
  }

  bool
  DialbackFrame::BDecode(llarp_buffer_t* buf)
  {
    m_DecodedAction = false;
    m_DecodedChallenge = false;
    m_DecodedTimestamp = false;
    m_DecodedSignature = false;
    m_DecodedError = false;
    relayMarker = false;
    xMarker = false;
    yMarker = false;
    error.clear();

    if (!bencode_decode_dict(*this, buf))
      return false;

    return Validate();
  }
}  // namespace llarp::dtls
