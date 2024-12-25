#pragma once
#include "platform.hpp"

#include <llarp/util/fs.hpp>

#include <stdlib.h>

#include <unistd.h>
#include <pwd.h>

namespace llarp
{
  constexpr auto our_rc_filename = "self.signed";
  constexpr auto our_identity_filename = "identity.key";
  constexpr auto our_enc_key_filename = "encryption.key";
  constexpr auto our_transport_key_filename = "transport.key";

  constexpr auto nodedb_dirname = "nodedb";

  inline fs::path
  GetDefaultDataDir()
  {
    fs::path datadir{"/var/lib/lokinet"};
    if (auto uid = geteuid())
    {
      if (auto* pw = getpwuid(uid))
      {
        datadir = fs::path{pw->pw_dir} / ".lokinet";
      }
    }
    return datadir;
  }

  inline fs::path
  GetDefaultConfigFilename()
  {
    return "lokinet.ini";
  }

  inline fs::path
  GetDefaultConfigPath()
  {
    return GetDefaultDataDir() / GetDefaultConfigFilename();
  }

  inline fs::path
  GetDefaultBootstrap()
  {
    return GetDefaultDataDir() / "bootstrap.signed";
  }

}  // namespace llarp
