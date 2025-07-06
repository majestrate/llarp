#include <llarp/util/alloc.h>
#include "bootstrap.hpp"
#include <llarp/util/fs.hpp>
#include <llarp/util/bencode.hpp>
#include <llarp/util/logging.hpp>
#include <llarp/util/logging/buffer.hpp>

namespace llarp
{
  void
  BootstrapList::Clear()
  {
    clear();
  }

  static auto logcat = log::Cat("bootstrap-list");

  bool
  BootstrapList::BDecode(llarp_buffer_t* buf)
  {
    std::unique_ptr<RouterContact> rc;
    switch (static_cast<char>(*buf->cur))
    {
      case 'l':
        return bencode_read_list(
            [&](llarp_buffer_t* b, bool more) -> bool {
              if (more)
              {
                rc = std::make_unique<RouterContact>();
                if (not rc->BDecode(b))
                {
                  log::error(logcat, "invalid rc in bootstrap list: {}", llarp::buffer_printer{*b});
                  return false;
                }
                emplace(std::move(*rc));
                rc.reset();
              }
              return true;
            },
            buf);
      case 'd':
        rc = std::make_unique<RouterContact>();
        if (not rc->BDecode(buf))
        {
          log::error(logcat, "invalid rc: {}", llarp::buffer_printer{*buf});
          return false;
        }
        emplace(std::move(*rc));
        rc.reset();
        return true;
      default:
        log::error(logcat, "invalid data: {}", llarp::buffer_printer{*buf});
        return false;
    }
  }

  bool
  BootstrapList::BEncode(llarp_buffer_t* buf) const
  {
    return BEncodeWriteList(begin(), end(), buf);
  }

  template <>
  void
  BootstrapList::AddFromFile(const fs::path& fpath)
  {
    bool isListFile = false;
    {
      std::ifstream inf(fpath.c_str(), std::ios::binary);
      if (inf.is_open())
      {
        const char ch = inf.get();
        isListFile = ch == 'l';
      }
    }
    if (isListFile)
    {
      if (not BDecodeReadFile(fpath, *this))
      {
        throw std::runtime_error{fmt::format("failed to read bootstrap list file '{}'", fpath)};
      }
    }
    else
    {
      RouterContact rc;
      if (not rc.Read(fpath))
      {
        throw std::runtime_error{
            fmt::format("failed to decode bootstrap RC, file='{}', rc={}", fpath, rc)};
      }
      this->insert(rc);
    }
  }
}  // namespace llarp
